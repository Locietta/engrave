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
#include "eid.h"

#include <string>
#include <random>

#include "global/log.h"

using namespace mu::engraving;

static constexpr char separator_constant = '_';

static char *to_base64_chars(char *const first, char *const last, uint64_t n) {
    static constexpr char chars[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    IF_ASSERT_FAILED(static_cast<size_t>(last - first) >= EID::max_uin_t64_bas_e64_size) { return first; }

    char *it = first;
    do {
        *it++ = chars[n % 64];
        n = n / 64;
    } while (n != 0);

    return it;
}

static constexpr int char_to_int(char c) {
    switch (c) {
        case 'A': return 0;
        case 'B': return 1;
        case 'C': return 2;
        case 'D': return 3;
        case 'E': return 4;
        case 'F': return 5;
        case 'G': return 6;
        case 'H': return 7;
        case 'I': return 8;
        case 'J': return 9;
        case 'K': return 10;
        case 'L': return 11;
        case 'M': return 12;
        case 'N': return 13;
        case 'O': return 14;
        case 'P': return 15;
        case 'Q': return 16;
        case 'R': return 17;
        case 'S': return 18;
        case 'T': return 19;
        case 'U': return 20;
        case 'V': return 21;
        case 'W': return 22;
        case 'X': return 23;
        case 'Y': return 24;
        case 'Z': return 25;
        case 'a': return 26;
        case 'b': return 27;
        case 'c': return 28;
        case 'd': return 29;
        case 'e': return 30;
        case 'f': return 31;
        case 'g': return 32;
        case 'h': return 33;
        case 'i': return 34;
        case 'j': return 35;
        case 'k': return 36;
        case 'l': return 37;
        case 'm': return 38;
        case 'n': return 39;
        case 'o': return 40;
        case 'p': return 41;
        case 'q': return 42;
        case 'r': return 43;
        case 's': return 44;
        case 't': return 45;
        case 'u': return 46;
        case 'v': return 47;
        case 'w': return 48;
        case 'x': return 49;
        case 'y': return 50;
        case 'z': return 51;
        case '0': return 52;
        case '1': return 53;
        case '2': return 54;
        case '3': return 55;
        case '4': return 56;
        case '5': return 57;
        case '6': return 58;
        case '7': return 59;
        case '8': return 60;
        case '9': return 61;
        case '+': return 62;
        case '/': return 63;
        default: return -1;
    }
}

// The half a base64 string encodes; false when it is empty, longer than a uint64
// needs, or contains a character outside the alphabet.
static bool base64_str_to_int64(const std::string_view s, uint64_t &result) {
    if (s.empty() || s.size() > EID::max_uin_t64_bas_e64_size) {
        return false;
    }

    uint64_t value = 0;
    for (auto iter = s.rbegin(); iter != s.rend(); ++iter) {
        const int digit = char_to_int(*iter);
        if (digit < 0) {
            return false;
        }
        value = (value << 6) | static_cast<uint64_t>(digit);
    }

    result = value;
    return true;
}

char *EID::to_chars(char *const first, char *const last) const {
    IF_ASSERT_FAILED(static_cast<size_t>(last - first) >= ::mu::engraving::EID::max_str_size) { return first; }

    char *it = to_base64_chars(first, last, first_);
    *it++ = ::separator_constant;
    it = to_base64_chars(it, last, second_);

    return it;
}

std::string EID::to_std_string() const {
    std::string base64_repr(::mu::engraving::EID::max_str_size, char{});
    const char *last = to_chars(base64_repr.data(), base64_repr.data() + base64_repr.size());
    base64_repr.resize(last - base64_repr.data());

    return base64_repr;
}

EID EID::from_std_string(const std::string &s) { return from_std_string(std::string_view(s)); }

EID EID::from_std_string(const std::string_view &s) {
    const size_t separator = s.find(::separator_constant);
    if (separator == std::string_view::npos || s.find(::separator_constant, separator + 1) != std::string_view::npos) {
        return EID::invalid();
    }

    uint64_t first = 0;
    uint64_t second = 0;
    if (!base64_str_to_int64(s.substr(0, separator), first) || !base64_str_to_int64(s.substr(separator + 1), second)) {
        return EID::invalid();
    }

    return EID(first, second);
}

EID EID::new_unique() {
    static std::random_device s_device;
    thread_local static std::mt19937_64 s_engine(s_device());
    thread_local static std::uniform_int_distribution<uint64_t> s_unif_dist;

    return EID(s_unif_dist(s_engine), s_unif_dist(s_engine));
}

// FOR UNIT TESTING
// EIDs are creates sequentially instead of randomly for test repeatability

EID EID::new_unique_test_mode(uint64_t &max_val) {
    ++max_val;
    return EID(max_val, max_val);
}

void EID::update_max_val_test_mode(const EID &cur_eid, uint64_t &max_val) {
    max_val = std::max(max_val, cur_eid.first_);
    max_val = std::max(max_val, cur_eid.second_);
}
