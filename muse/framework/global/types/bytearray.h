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
#ifndef MUSE_GLOBAL_BYTEARRAY_H
#define MUSE_GLOBAL_BYTEARRAY_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace muse {
struct ByteArray {
    typedef uint8_t value_type;

    ByteArray();
    ByteArray(const uint8_t *data, size_t size);
    ByteArray(const char *str, size_t size = static_cast<size_t>(-1));
    ByteArray(size_t size);

    // Always owns its bytes; pointers and views borrow until the next mutation.

    bool operator==(const ByteArray &other) const;

    bool operator!=(const ByteArray &other) const { return !operator==(other); }

    uint8_t *data();
    std::vector<uint8_t> &vdata();
    const uint8_t *const_data() const;
    const char *const_char() const; // data as char*
    const std::vector<uint8_t> &const_v_data() const;
    size_t size() const;
    bool empty() const;

    ByteArray &insert(size_t pos, uint8_t b);
    void push_back(uint8_t b);
    void push_back(const uint8_t *b, size_t len);
    void push_back(const ByteArray &ba);

    uint8_t at(size_t pos) const;
    uint8_t operator[](size_t pos) const;
    uint8_t &operator[](size_t pos);

    void reserve(size_t nsize);
    void resize(size_t nsize);
    void truncate(size_t pos);
    void clear();

    ByteArray left(size_t len) const;
    ByteArray right(size_t len) const;


private:
    using Data = std::vector<uint8_t>;

    // Nonempty storage includes a trailing zero outside the logical size.
    Data data_;
};
} // namespace muse

#endif // MUSE_GLOBAL_BYTEARRAY_H
