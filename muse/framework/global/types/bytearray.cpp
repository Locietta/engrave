/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
#include "types/bytearray.h"

#include <cassert>
#include <cstring>

using namespace muse;

ByteArray::ByteArray() {
    data_ = std::make_shared<Data>();
    data_->resize(1);
    data_->operator[](0) = 0;
}

ByteArray::ByteArray(const uint8_t *data, size_t size) {
    data_ = std::make_shared<Data>();
    data_->resize(size + 1);
    data_->operator[](size) = 0;
    std::memcpy(data_->data(), data, size);
}

ByteArray::ByteArray(const char *str, size_t size) {
    size = (size == static_cast<size_t>(-1)) ? std::strlen(str) : size;
    data_ = std::make_shared<Data>();
    data_->resize(size + 1);
    data_->operator[](size) = 0;
    std::memcpy(data_->data(), str, size);
}

ByteArray::ByteArray(size_t size) {
    data_ = std::make_shared<Data>();
    data_->resize(size + 1);
    data_->operator[](size) = 0;
}

ByteArray ByteArray::from_raw_data(const uint8_t *data, size_t size) {
    ByteArray ba;
    ba.raw_.size = size;
    ba.raw_.data = data;
    return ba;
}

ByteArray ByteArray::from_raw_data(const char *data, size_t size) { return from_raw_data(reinterpret_cast<const uint8_t *>(data), size); }

uint8_t *ByteArray::data() {
    detach();
    return data_->data();
}

std::vector<uint8_t> &ByteArray::vdata() {
    detach();
    return *data_.get();
}

const uint8_t *ByteArray::const_data() const {
    if (raw_.data) {
        return raw_.data;
    }

    return data_->data();
}

const char *ByteArray::const_char() const { return reinterpret_cast<const char *>(const_data()); }

const std::vector<uint8_t> &ByteArray::const_v_data() const { return *data_.get(); }

size_t ByteArray::size() const {
    if (raw_.data) {
        return raw_.size;
    }

    return data_->empty() ? 0 : (data_->size() - 1);
}

void ByteArray::detach() {
    if (!data_) {
        return;
    }

    if (raw_.data) {
        data_->resize(raw_.size + 1);
        data_->operator[](raw_.size) = 0;
        std::memcpy(data_->data(), raw_.data, raw_.size);
        raw_.data = nullptr;
        return;
    }

    if (data_.use_count() == 1) {
        return;
    }

    data_ = std::make_shared<Data>(*data_);
}

bool ByteArray::operator==(const ByteArray &other) const {
    if (size() != other.size()) {
        return false;
    }
    return std::memcmp(const_data(), other.const_data(), size()) == 0;
}

bool ByteArray::empty() const { return size() == 0; }

void ByteArray::reserve(size_t nsize) {
    if (nsize + 1 <= data_->capacity()) {
        return;
    }

    detach();
    data_->reserve(nsize + 1);
}

void ByteArray::resize(size_t nsize) {
    if (nsize == size()) {
        return;
    }

    detach();
    data_->resize(nsize + 1);
    data_->operator[](nsize) = 0;
}

void ByteArray::truncate(size_t pos) {
    if (pos >= size()) {
        return;
    }
    resize(pos);
}

void ByteArray::clear() { resize(0); }

ByteArray &ByteArray::insert(size_t pos, uint8_t b) {
    if (pos > size()) {
        return *this;
    }

    detach();
    data_->insert(data_->begin() + pos, b);
    return *this;
}

void ByteArray::push_back(uint8_t b) { insert(size(), b); }

void ByteArray::push_back(const uint8_t *b, size_t len) {
    detach();
    size_t start = size();
    size_t nsize = start + len;
    Data &data = *data_.get();
    data.resize(nsize + 1);
    data[nsize] = 0;
    for (size_t i = 0; i < len; ++i) {
        data[start + i] = b[i];
    }
}

void ByteArray::push_back(const ByteArray &ba) { push_back(ba.const_data(), ba.size()); }

uint8_t ByteArray::at(size_t pos) const {
    assert(pos < size());
    if (pos < size()) {
        return const_data()[pos];
    }
    return 0;
}

uint8_t ByteArray::operator[](size_t pos) const { return at(pos); }

uint8_t &ByteArray::operator[](size_t pos) {
    detach();

    assert(pos < size());
    if (pos < size()) {
        return data_->operator[](pos);
    }

    static uint8_t dummy;
    return dummy;
}

ByteArray ByteArray::left(size_t len) const { return ByteArray(const_data(), len); }

ByteArray ByteArray::right(size_t len) const { return ByteArray(&(const_data()[size() - len]), len); }
