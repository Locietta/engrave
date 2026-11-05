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

static const muse::Char SUFFIX_DELIMITER = '-';

using namespace muse;

static std::array<int, 3> parseVersion(const muse::String &versionString, bool &ok) {
    std::array<int, 3> result{0, 0, 0};

    if (versionString.isEmpty()) {
        ok = false;
        return result;
    }

    size_t componentIdx = 0;
    int curNum = 0;

    for (size_t i = 0; i < versionString.size(); ++i) {
        char ch = versionString[i];
        if (ch == '.' || ch == '\0') {
            result.at(componentIdx++) = curNum;
            curNum = 0;
        } else if ('0' <= ch && ch <= '9') {
            curNum = curNum * 10 + (ch - '0');
        } else {
            ok = false;
            return result;
        }
    }

    if (componentIdx < result.size()) {
        result.at(componentIdx) = curNum;
    }

    ok = true;
    return result;
}

static std::pair<String, int> parseVersionSuffix(const String &suffix, bool &ok) {
    if (suffix.isEmpty()) {
        ok = false;
        return std::make_pair(String(), 0);
    }

    StringList suffixComponents = suffix.split('.');

    ok = true;
    return std::make_pair(suffixComponents.front(), (suffixComponents.size() > 1 ? suffixComponents[1].toInt() : 0));
}

Version::Version(int major, int minor, int patch, const String &suffix, int suffixVersion)
    : major_(major), minor_(minor), patch_(patch), suffix_(suffix), suffixVersion_(suffixVersion) {}

Version::Version(const String &versionStr) {
    String version = versionStr.left(versionStr.indexOf(SUFFIX_DELIMITER));

    bool ok = true;
    std::array<int, 3> versionComponents = parseVersion(version, ok);
    if (!ok) {
        return;
    }

    major_ = versionComponents[0];
    minor_ = versionComponents[1];
    patch_ = versionComponents[2];

    if (!versionStr.contains(SUFFIX_DELIMITER)) {
        return;
    }

    setSuffix(versionStr.right(versionStr.size() - versionStr.indexOf(SUFFIX_DELIMITER) - 1));
}

Version::Version(const std::string &versionStr) : Version(String::fromStdString(versionStr)) {}

int Version::major() const { return major_; }

int Version::minor() const { return minor_; }

int Version::patch() const { return patch_; }

const String &Version::suffix() const { return suffix_; }

int Version::suffixVersion() const { return suffixVersion_; }

void Version::setSuffix(const String &suffix) {
    bool ok = true;
    std::pair<String, int> versionSuffix = parseVersionSuffix(suffix, ok);
    if (!ok) {
        return;
    }

    suffix_ = versionSuffix.first;
    suffixVersion_ = versionSuffix.second;
}

bool Version::isNull() const { return major_ == 0 && minor_ == 0 && patch_ == 0; }

bool Version::preRelease() const { return !suffix().isEmpty(); }

String Version::toString() const {
    String res = String(u"%1.%2.%3").arg(major_, minor_, patch_);

    if (!suffix_.isEmpty()) {
        res.append(SUFFIX_DELIMITER);
        res.append(suffix_ + (suffixVersion_ > 0 ? u"." + String::number(suffixVersion_) : u""));
    }

    return res;
}

std::string Version::toStdString() const { return toString().toStdString(); }

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
                if (suffix_.isEmpty()) {
                    return false;
                }

                if (other.suffix().isEmpty()) {
                    return true;
                }

                static StringList suffixes{u"dev", u"alpha", u"beta", u"rc"};

                auto currentIt =
                    std::find_if(suffixes.cbegin(), suffixes.cend(), [suffix = suffix_](const String &s) { return s.startsWith(suffix); });

                auto updateIt = std::find_if(suffixes.cbegin(), suffixes.cend(),
                                             [suffix = other.suffix()](const String &s) { return s.startsWith(suffix); });

                if (currentIt == suffixes.cend() || updateIt == suffixes.cend()) {
                    LOGE() << "Invalid version suffix; current " << suffix_ << ", update " << other.suffix();
                    return true;
                }

                if (currentIt < updateIt) {
                    return true;
                }

                if (currentIt == updateIt) {
                    return suffixVersion_ < other.suffixVersion();
                }

                return false;
            }
        }
    }

    return true;
}

bool Version::operator==(const Version &other) const {
    return major_ == other.major() && minor_ == other.minor() && patch_ == other.patch() && suffix_ == other.suffix() &&
           suffixVersion_ == other.suffixVersion();
}

bool Version::operator<=(const Version &other) const {
    if (operator==(other)) {
        return true;
    }

    return operator<(other);
}
