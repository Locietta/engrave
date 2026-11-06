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
#include "version.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "log.h"

static const muse::Char suffix_delimiter = '-';

using namespace muse;

static std::array<int, 3> parse_version(const muse::String &version_string, bool &ok) {
    std::array<int, 3> result{0, 0, 0};

    if (version_string.is_empty()) {
        ok = false;
        return result;
    }

    size_t component_idx = 0;
    int cur_num = 0;

    for (size_t i = 0; i < version_string.size(); ++i) {
        char ch = version_string[i];
        if (ch == '.' || ch == '\0') {
            result.at(component_idx++) = cur_num;
            cur_num = 0;
        } else if ('0' <= ch && ch <= '9') {
            cur_num = cur_num * 10 + (ch - '0');
        } else {
            ok = false;
            return result;
        }
    }

    if (component_idx < result.size()) {
        result.at(component_idx) = cur_num;
    }

    ok = true;
    return result;
}

static std::pair<String, int> parse_version_suffix(const String &suffix, bool &ok) {
    if (suffix.is_empty()) {
        ok = false;
        return std::make_pair(String(), 0);
    }

    StringList suffix_components = suffix.split('.');

    ok = true;
    return std::make_pair(suffix_components.front(), (suffix_components.size() > 1 ? suffix_components[1].to_int() : 0));
}

Version::Version(int major, int minor, int patch, const String &suffix, int suffix_version)
    : major_(major), minor_(minor), patch_(patch), suffix_(suffix), suffix_version_(suffix_version) {}

Version::Version(const String &version_str) {
    String version = version_str.left(version_str.index_of(::suffix_delimiter));

    bool ok = true;
    std::array<int, 3> version_components = parse_version(version, ok);
    if (!ok) {
        return;
    }

    major_ = version_components[0];
    minor_ = version_components[1];
    patch_ = version_components[2];

    if (!version_str.contains(::suffix_delimiter)) {
        return;
    }

    set_suffix(version_str.right(version_str.size() - version_str.index_of(::suffix_delimiter) - 1));
}

Version::Version(const std::string &version_str) : Version(String::from_std_string(version_str)) {}

int Version::major() const { return major_; }

int Version::minor() const { return minor_; }

int Version::patch() const { return patch_; }

const String &Version::suffix() const { return suffix_; }

int Version::suffix_version() const { return suffix_version_; }

void Version::set_suffix(const String &suffix) {
    bool ok = true;
    std::pair<String, int> version_suffix = parse_version_suffix(suffix, ok);
    if (!ok) {
        return;
    }

    suffix_ = version_suffix.first;
    suffix_version_ = version_suffix.second;
}

bool Version::is_null() const { return major_ == 0 && minor_ == 0 && patch_ == 0; }

bool Version::pre_release() const { return !suffix().is_empty(); }

String Version::to_string() const {
    String res = String(u"%1.%2.%3").arg(major_, minor_, patch_);

    if (!suffix_.is_empty()) {
        res.append(::suffix_delimiter);
        res.append(suffix_ + (suffix_version_ > 0 ? u"." + String::number(suffix_version_) : u""));
    }

    return res;
}

std::string Version::to_std_string() const { return to_string().to_std_string(); }

bool Version::operator<(const Version &other) const {
    if (major_ > other.major()) {
        return false;
    } else if (major_ == other.major()) {
        if (minor_ > other.minor()) {
            return false;
        } else if (minor_ == other.minor()) {
            if (patch_ > other.patch()) {
                return false;
            } else if (patch_ == other.patch()) {
                if (suffix_.is_empty()) {
                    return false;
                }

                if (other.suffix().is_empty()) {
                    return true;
                }

                static StringList suffixes{u"dev", u"alpha", u"beta", u"rc"};

                auto current_it =
                    std::find_if(suffixes.cbegin(), suffixes.cend(), [suffix = suffix_](const String &s) { return s.starts_with(suffix); });

                auto update_it = std::find_if(suffixes.cbegin(), suffixes.cend(),
                                              [suffix = other.suffix()](const String &s) { return s.starts_with(suffix); });

                if (current_it == suffixes.cend() || update_it == suffixes.cend()) {
                    LOGE() << "Invalid version suffix; current " << suffix_ << ", update " << other.suffix();
                    return true;
                }

                if (current_it < update_it) {
                    return true;
                }

                if (current_it == update_it) {
                    return suffix_version_ < other.suffix_version();
                }

                return false;
            }
        }
    }

    return true;
}

bool Version::operator==(const Version &other) const {
    return major_ == other.major() && minor_ == other.minor() && patch_ == other.patch() && suffix_ == other.suffix() &&
           suffix_version_ == other.suffix_version();
}

bool Version::operator<=(const Version &other) const {
    if (operator==(other)) {
        return true;
    }

    return operator<(other);
}
