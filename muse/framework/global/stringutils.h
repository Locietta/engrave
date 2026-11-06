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
#ifndef MUSE_GLOBAL_STRINGUTILS_H
#define MUSE_GLOBAL_STRINGUTILS_H

#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "types/string.h"

namespace muse::strings {
bool replace(std::string &source, const std::string &what, const std::string &to);
void split(const std::string &str, std::vector<std::string> &out, const std::string &delim, const std::string &esc = "");
std::string join(const std::vector<std::string> &strs, const std::string &sep = ",", const std::string &esc = "");

void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);

std::string to_lower(const std::string &source);
std::string to_upper(const std::string &source);
bool starts_with(const std::string &str, const std::string &start);
bool starts_with(const std::string &str, const std::string_view &start);
bool starts_with(const std::string &str, const char *start);
bool ends_with(const std::string &str, const std::string &end);
bool ends_with(const std::string &str, const std::string_view &end);
bool ends_with(const std::string &str, const char *end);
std::string left_justified(const std::string &val, size_t width);

// Locale-independent version of std::to_string
template <typename T>
std::string to_string(const T &t) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << t;
    return oss.str();
}

bool less_than_case_insensitive(const std::string &lhs, const std::string &rhs);
bool less_than_case_insensitive(const String &lhs, const String &rhs);

size_t levenshtein_distance(const std::string &s1, const std::string &s2);
} // namespace muse::strings

#endif // MUSE_GLOBAL_STRINGUTILS_H
