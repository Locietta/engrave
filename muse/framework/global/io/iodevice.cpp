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
    if (is_open()) {
        return true;
    }

    bool ok = do_open(mode);
    if (ok) {
        mode_ = mode;
        pos_ = mode == APPEND ? data_size() : 0;
    }

    return ok;
}

void IODevice::close() {
    do_close();
    mode_ = UNKNOWN;
}

bool IODevice::is_open() const { return mode_ != OpenMode::UNKNOWN; }

IODevice::OpenMode IODevice::open_mode() const { return mode_; }

bool IODevice::is_open_mode_readable() const { return mode_ == OpenMode::READ_ONLY || mode_ == OpenMode::READ_WRITE; }

bool IODevice::is_readable() const { return is_open_mode_readable() && pos_ < size(); }

bool IODevice::is_open_mode_writeable() const {
    return mode_ == OpenMode::WRITE_ONLY || mode_ == OpenMode::READ_WRITE || mode_ == OpenMode::APPEND;
}

void IODevice::set_error(int error, const std::string &error_string) {
    error_ = error;
    error_string_ = error_string;
}

bool IODevice::is_writeable() const { return is_open_mode_writeable(); }

size_t IODevice::size() const {
    IF_ASSERT_FAILED(is_open()) { return 0; }

    return data_size();
}

size_t IODevice::pos() const { return pos_; }

bool IODevice::seek(size_t pos) {
    IF_ASSERT_FAILED(is_open()) { return false; }

    if (pos <= size()) {
        pos_ = pos;
        return true;
    }

    if (!is_open_mode_writeable()) {
        return false;
    }

    bool ok = resize_data(pos);
    if (ok) {
        pos_ = pos;
    }

    return ok;
}

size_t IODevice::read(uint8_t *data, size_t len) {
    IF_ASSERT_FAILED(is_open_mode_readable()) { return 0; }

    IF_ASSERT_FAILED(pos_ <= size()) { return 0; }

    if (pos_ == size()) {
        memset(data, 0, len);
        return 0;
    }

    size_t left = size() - pos_;
    if (left < len) {
        len = left;
    }

    std::memcpy(data, cdata_offsetted(), len);

    pos_ += len;
    return len;
}

ByteArray IODevice::read(size_t len) {
    IF_ASSERT_FAILED(is_open_mode_readable()) { return ByteArray(); }

    IF_ASSERT_FAILED(pos_ <= size()) { return ByteArray(); }

    if (pos_ == size()) {
        return ByteArray();
    }

    size_t left = size() - pos_;
    if (left < len) {
        len = left;
    }

    ByteArray result(cdata_offsetted(), len);

    pos_ += len;

    return result;
}

const uint8_t *IODevice::cdata_offsetted() const {
    const uint8_t *d = raw_data();
    IF_ASSERT_FAILED(d) { return nullptr; }
    return d + pos_;
}

ByteArray IODevice::read_all() { return do_read_all(); }

ByteArray IODevice::do_read_all() { return read(size()); }

const uint8_t *IODevice::read_data() {
    IF_ASSERT_FAILED(is_open()) { return nullptr; }
    return raw_data();
}

size_t IODevice::write(const uint8_t *data, size_t len) {
    IF_ASSERT_FAILED(is_open_mode_writeable()) { return 0; }

    size_t left = size() - pos_;
    if (left < len) {
        bool ok = resize_data(pos_ + len);
        if (!ok) {
            LOGE() << "failed resize data";
            return 0;
        }
    }

    len = write_data(data, len);
    pos_ += len;

    return len;
}

size_t IODevice::write(const ByteArray &ba) { return write(ba.const_data(), ba.size()); }

const std::string &IODevice::meta(const std::string &key) const {
    auto it = meta_.find(key);
    if (it != meta_.end()) {
        return it->second;
    }

    static const std::string dummy_val;
    return dummy_val;
}

void IODevice::set_meta(const std::string &key, const std::string &val) { meta_[key] = val; }

bool IODevice::has_error() const { return error_ != 0; }

int IODevice::error() const { return error_; }

const std::string &IODevice::error_string() const { return error_string_; }
