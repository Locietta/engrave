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

#include <sstream>
#include <string>
#include <thread>

namespace muse::runtime {
inline constexpr bool is_debug() {
#ifndef NDEBUG
    return true;
#else
    return false;
#endif
}

inline std::thread::id main_thread_id() {
    static std::thread::id main_id = std::this_thread::get_id();
    return main_id;
}

inline std::string to_string(const std::thread::id &id) {
    std::ostringstream ss;
    ss << id;
    return ss.str();
}

void set_thread_name(const std::string &name);
const std::string &thread_name();
} // namespace muse::runtime
