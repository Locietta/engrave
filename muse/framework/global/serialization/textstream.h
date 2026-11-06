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
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace muse {
struct ByteArray;
struct String;

namespace io {
struct IODevice;
} // namespace io

struct TextStream {
    TextStream() = default;
    explicit TextStream(io::IODevice *device);
    virtual ~TextStream();

    void set_device(io::IODevice *device);

    void flush();

    TextStream &operator<<(char ch);
    TextStream &operator<<(int32_t);
    TextStream &operator<<(uint32_t);
    TextStream &operator<<(double);
    TextStream &operator<<(int64_t);
    TextStream &operator<<(uint64_t);
    TextStream &operator<<(const char *s);
    TextStream &operator<<(std::string_view);
    TextStream &operator<<(const ByteArray &b);
    TextStream &operator<<(const String &s);


private:
    void write(const char *ch, size_t len);
    io::IODevice *device_ = nullptr;
    std::vector<uint8_t> buf_;
};
} // namespace muse
