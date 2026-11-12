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

#include <algorithm>
#include <cstring>

using namespace muse;

ByteArray::ByteArray() = default;

ByteArray::ByteArray(const uint8_t *data, size_t size) : data_(size + 1, 0) {
    if (size) std::memcpy(data_.data(), data, size);
}

ByteArray::ByteArray(const char *str, size_t size)
    : ByteArray(reinterpret_cast<const uint8_t *>(str), size == static_cast<size_t>(-1) ? std::strlen(str) : size) {}

ByteArray::ByteArray(size_t size) : data_(size + 1, 0) {}

uint8_t *ByteArray::data() {
    if (data_.empty()) data_.push_back(0);
    return data_.data();
}

std::vector<uint8_t> &ByteArray::vdata() {
    data();
    return data_;
}

const uint8_t *ByteArray::const_data() const {
    static constexpr uint8_t zero = 0;
    return data_.empty() ? &zero : data_.data();
}

const char *ByteArray::const_char() const { return reinterpret_cast<const char *>(const_data()); }

const std::vector<uint8_t> &ByteArray::const_v_data() const { return data_; }

size_t ByteArray::size() const { return data_.empty() ? 0 : data_.size() - 1; }

bool ByteArray::empty() const { return size() == 0; }

bool ByteArray::operator==(const ByteArray &other) const {
    return size() == other.size() && std::memcmp(const_data(), other.const_data(), size()) == 0;
}

void ByteArray::reserve(size_t nsize) { data_.reserve(nsize + 1); }

void ByteArray::resize(size_t nsize) {
    data_.resize(nsize + 1);
    data_[nsize] = 0;
}

void ByteArray::truncate(size_t pos) {
    if (pos < size()) resize(pos);
}

void ByteArray::clear() { data_.clear(); }

ByteArray &ByteArray::insert(size_t pos, uint8_t b) {
    if (pos > size()) return *this;
    data();
    data_.insert(data_.begin() + pos, b);
    return *this;
}

void ByteArray::push_back(uint8_t b) { insert(size(), b); }

void ByteArray::push_back(const uint8_t *b, size_t len) {
    if (!len) return;
    // Preserve a borrowed slice of ourselves across vector reallocation.
    const auto address = reinterpret_cast<uintptr_t>(b);
    const auto base = reinterpret_cast<uintptr_t>(const_data());
    const bool aliases = address >= base && address - base <= size();
    const size_t offset = aliases ? address - base : 0;
    const size_t start = size();
    resize(start + len);
    if (aliases) b = data_.data() + offset;
    std::memmove(data_.data() + start, b, len);
}

void ByteArray::push_back(const ByteArray &ba) { push_back(ba.const_data(), ba.size()); }

uint8_t ByteArray::at(size_t pos) const { return data_.at(pos < size() ? pos : data_.size()); }

uint8_t ByteArray::operator[](size_t pos) const { return at(pos); }

uint8_t &ByteArray::operator[](size_t pos) { return data_.at(pos < size() ? pos : data_.size()); }

ByteArray ByteArray::left(size_t len) const { return ByteArray(const_data(), std::min(len, size())); }

ByteArray ByteArray::right(size_t len) const {
    len = std::min(len, size());
    return ByteArray(const_data() + size() - len, len);
}
