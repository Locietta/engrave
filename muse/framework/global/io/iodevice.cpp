/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "iodevice.h"

#include <cstring>

#include "log.h"

using namespace muse;
using namespace muse::io;

bool IODevice::open(IODevice::OpenMode mode) {
    if (isOpen()) {
        return true;
    }

    bool ok = doOpen(mode);
    if (ok) {
        mode_ = mode;
        pos_ = mode == Append ? dataSize() : 0;
    }

    return ok;
}

void IODevice::close() {
    doClose();
    mode_ = Unknown;
}

bool IODevice::isOpen() const { return mode_ != OpenMode::Unknown; }

IODevice::OpenMode IODevice::openMode() const { return mode_; }

bool IODevice::isOpenModeReadable() const { return mode_ == OpenMode::ReadOnly || mode_ == OpenMode::ReadWrite; }

bool IODevice::isReadable() const { return isOpenModeReadable() && pos_ < size(); }

bool IODevice::isOpenModeWriteable() const {
    return mode_ == OpenMode::WriteOnly || mode_ == OpenMode::ReadWrite || mode_ == OpenMode::Append;
}

void IODevice::setError(int error, const std::string &errorString) {
    error_ = error;
    errorString_ = errorString;
}

bool IODevice::isWriteable() const { return isOpenModeWriteable(); }

size_t IODevice::size() const {
    IF_ASSERT_FAILED(isOpen()) { return 0; }

    return dataSize();
}

size_t IODevice::pos() const { return pos_; }

bool IODevice::seek(size_t pos) {
    IF_ASSERT_FAILED(isOpen()) { return false; }

    if (pos <= size()) {
        pos_ = pos;
        return true;
    }

    if (!isOpenModeWriteable()) {
        return false;
    }

    bool ok = resizeData(pos);
    if (ok) {
        pos_ = pos;
    }

    return ok;
}

size_t IODevice::read(uint8_t *data, size_t len) {
    IF_ASSERT_FAILED(isOpenModeReadable()) { return 0; }

    IF_ASSERT_FAILED(pos_ <= size()) { return 0; }

    if (pos_ == size()) {
        memset(data, 0, len);
        return 0;
    }

    size_t left = size() - pos_;
    if (left < len) {
        len = left;
    }

    std::memcpy(data, cdataOffsetted(), len);

    pos_ += len;
    return len;
}

ByteArray IODevice::read(size_t len) {
    IF_ASSERT_FAILED(isOpenModeReadable()) { return ByteArray(); }

    IF_ASSERT_FAILED(pos_ <= size()) { return ByteArray(); }

    if (pos_ == size()) {
        return ByteArray();
    }

    size_t left = size() - pos_;
    if (left < len) {
        len = left;
    }

    ByteArray result(cdataOffsetted(), len);

    pos_ += len;

    return result;
}

const uint8_t *IODevice::cdataOffsetted() const {
    const uint8_t *d = rawData();
    IF_ASSERT_FAILED(d) { return nullptr; }
    return d + pos_;
}

ByteArray IODevice::readAll() { return doReadAll(); }

ByteArray IODevice::doReadAll() { return read(size()); }

const uint8_t *IODevice::readData() {
    IF_ASSERT_FAILED(isOpen()) { return nullptr; }
    return rawData();
}

size_t IODevice::write(const uint8_t *data, size_t len) {
    IF_ASSERT_FAILED(isOpenModeWriteable()) { return 0; }

    size_t left = size() - pos_;
    if (left < len) {
        bool ok = resizeData(pos_ + len);
        if (!ok) {
            LOGE() << "failed resize data";
            return 0;
        }
    }

    len = writeData(data, len);
    pos_ += len;

    return len;
}

size_t IODevice::write(const ByteArray &ba) { return write(ba.constData(), ba.size()); }

const std::string &IODevice::meta(const std::string &key) const {
    auto it = meta_.find(key);
    if (it != meta_.end()) {
        return it->second;
    }

    static const std::string dummyVal;
    return dummyVal;
}

void IODevice::setMeta(const std::string &key, const std::string &val) { meta_[key] = val; }

bool IODevice::hasError() const { return error_ != 0; }

int IODevice::error() const { return error_; }

const std::string &IODevice::errorString() const { return errorString_; }
