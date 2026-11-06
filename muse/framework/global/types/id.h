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
#ifndef MUSE_GLOBAL_ID_H
#define MUSE_GLOBAL_ID_H

#include <cstdint>
#include <vector>
#include <algorithm>

#include "global/logstream.h"

namespace muse {
class ID {
public:
    ID();
    ID(const ID &id);
    ID(const std::string &id);

    constexpr ID(const uint64_t &id) : id_(id) {}

    bool is_valid() const;

    ID &operator=(const ID &id);
    ID &operator+=(const ID &id);

    ID operator+(const ID &id) const;
    ID operator^(const ID &id) const;

    bool operator==(const ID &id) const;
    bool operator==(uint64_t id) const;
    bool operator!=(const ID &id) const;
    bool operator<(const ID &id) const;
    bool operator>(const ID &id) const;

    uint64_t to_uint64() const;
    std::string to_std_string() const;


private:
    uint64_t id_ = 0;
};

using IDList = std::vector<ID>;

inline bool contains_id(const IDList &list, const ID &id) { return std::find(list.cbegin(), list.cend(), id) != list.cend(); }
} // namespace muse

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::ID &v) {
    s << v.to_uint64();
    return s;
}

#endif // MUSE_GLOBAL_ID_H
