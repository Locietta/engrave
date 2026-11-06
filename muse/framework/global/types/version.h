/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include <string>

#include "types/string.h"

namespace muse {
struct Version {
    Version() = default;
    Version(int major, int minor = 0, int patch = 0, const String &suffix = String(), int suffix_version = 0);
    Version(const String &version_str);
    Version(const std::string &version_str);

    int major() const;
    int minor() const;
    int patch() const;

    const String &suffix() const;
    int suffix_version() const;
    void set_suffix(const String &suffix);

    bool is_null() const;

    bool pre_release() const;

    String to_string() const;
    std::string to_std_string() const;

    bool operator<(const Version &other) const;
    bool operator==(const Version &other) const;
    bool operator<=(const Version &other) const;

private:
    int major_ = 0;
    int minor_ = 0;
    int patch_ = 0;
    String suffix_;
    int suffix_version_ = 0;
};
} // namespace muse
