/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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
#include <string>
#include <string_view>

#include "global/logstream.h"

namespace mu::engraving {
class EID {
public:
    // max size of base64 string of 64-bit int is ceil(log(2^64) / log(64)) = 11
    static constexpr size_t MAX_UINT64_BASE64_SIZE = 11;
    // 2 * uint64_t as base64 + separator
    static constexpr size_t MAX_STR_SIZE = 2 * MAX_UINT64_BASE64_SIZE + 1;

    bool isValid() const { return first_ != INVALID || second_ != INVALID; }

    inline bool operator==(const EID &other) const { return first_ == other.first_ && second_ == other.second_; }

    inline bool operator!=(const EID &other) const { return first_ != other.first_ || second_ != other.second_; }

    // The two values the text form encodes; 0xFFFFFFFFFFFFFFFF is INVALID.
    uint64_t first() const { return first_; }

    uint64_t second() const { return second_; }

    char *toChars(char *first, char *last) const;
    std::string toStdString() const;
    // Strict: a value that is not two non-empty halves of at most
    // MAX_UINT64_BASE64_SIZE base64 characters gives EID::invalid(), where upstream
    // ignored a wrong part count, accepted any character (OR-ing 0xFFFFFFFFFFFFFFFF
    // into the half) and shifted the excess bits of a longer half out.
    static EID fromStdString(const std::string &v);
    static EID fromStdString(const std::string_view &v);

    static EID fromParts(uint64_t first, uint64_t second) { return EID(first, second); }

    static EID newUnique();

    static EID invalid() { return EID(INVALID, INVALID); }

    // FOR UNIT TESTING
    static EID newUniqueTestMode(uint64_t &maxVal);
    static void updateMaxValTestMode(const EID &curEID, uint64_t &maxVal);

private:
    EID() = delete;

    EID(uint64_t f, uint64_t s) : first_(f), second_(s) {}

    static constexpr uint64_t INVALID = uint64_t(-1);

    uint64_t first_ = INVALID;
    uint64_t second_ = INVALID;

    friend struct std::hash<EID>;
};
} // namespace mu::engraving

template <>
struct std::hash<mu::engraving::EID> {
    size_t operator()(const mu::engraving::EID &eid) const {
        return std::hash<uint64_t>()(eid.first_) ^ (std::hash<uint64_t>()(eid.second_) << 1);
    }
};

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const mu::engraving::EID &v) {
    s << v.toStdString();
    return s;
}
