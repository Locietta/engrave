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
    static constexpr size_t max_uin_t64_bas_e64_size = 11;
    // 2 * uint64_t as base64 + separator
    static constexpr size_t max_str_size = 2 * ::mu::engraving::EID::max_uin_t64_bas_e64_size + 1;

    bool is_valid() const { return first_ != ::mu::engraving::EID::invalid_value || second_ != ::mu::engraving::EID::invalid_value; }

    inline bool operator==(const EID &other) const { return first_ == other.first_ && second_ == other.second_; }

    inline bool operator!=(const EID &other) const { return first_ != other.first_ || second_ != other.second_; }

    // The two values the text form encodes; 0xFFFFFFFFFFFFFFFF is INVALID.
    uint64_t first() const { return first_; }

    uint64_t second() const { return second_; }

    char *to_chars(char *first, char *last) const;
    std::string to_std_string() const;
    // Strict: a value that is not two non-empty halves of at most
    // MAX_UINT64_BASE64_SIZE base64 characters gives EID::invalid(), where upstream
    // ignored a wrong part count, accepted any character (OR-ing 0xFFFFFFFFFFFFFFFF
    // into the half) and shifted the excess bits of a longer half out.
    static EID from_std_string(const std::string &v);
    static EID from_std_string(const std::string_view &v);

    static EID from_parts(uint64_t first, uint64_t second) { return EID(first, second); }

    static EID new_unique();

    static EID invalid() { return EID(::mu::engraving::EID::invalid_value, ::mu::engraving::EID::invalid_value); }

    // FOR UNIT TESTING
    static EID new_unique_test_mode(uint64_t &max_val);
    static void update_max_val_test_mode(const EID &cur_eid, uint64_t &max_val);

private:
    EID() = delete;

    EID(uint64_t f, uint64_t s) : first_(f), second_(s) {}

    static constexpr uint64_t invalid_value = uint64_t(-1);

    uint64_t first_ = ::mu::engraving::EID::invalid_value;
    uint64_t second_ = ::mu::engraving::EID::invalid_value;

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
    s << v.to_std_string();
    return s;
}
