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
#ifndef MUSE_IO_PATH_H
#define MUSE_IO_PATH_H

#include <string>

#include "global/types/string.h"
#include "global/logstream.h"

namespace muse::io {
struct PathT;
using paths_t = std::vector<PathT>;

struct PathT {
    PathT() = default;
    PathT(const String &s);
    PathT(const std::string &s);
    PathT(std::string &&s);
    PathT(const char *s);

    bool empty() const;
    size_t size() const;
    bool has_suffix(const char *str) const;

    PathT appending_component(const PathT &other) const;
    PathT appending_suffix(const PathT &suffix) const;

    inline PathT &operator=(const String &other) {
        path_ = other.to_std_string();
        return *this;
    }

    inline PathT &operator=(const char *other) {
        path_ = other;
        return *this;
    }

    inline bool operator==(const PathT &other) const { return path_ == other.path_; }

    inline bool operator!=(const PathT &other) const { return !(path_ == other.path_); }

    inline PathT operator+(const PathT &other) const {
        PathT p = *this;
        p += other;
        return p;
    }

    inline PathT operator+(const String &other) const {
        PathT p = *this;
        p += other;
        return p;
    }

    inline PathT operator+(const char *other) const {
        PathT p = *this;
        p += other;
        return p;
    }

    inline PathT operator+(const char other) const {
        PathT p = *this;
        p += other;
        return p;
    }

    inline PathT &operator+=(const PathT &other) {
        path_ += other.path_;
        return *this;
    }

    inline PathT &operator+=(const String &other) {
        path_ += other.to_std_string();
        return *this;
    }

    inline PathT &operator+=(const char *other) {
        path_ += other;
        return *this;
    }

    inline PathT &operator+=(const char other) {
        path_ += other;
        return *this;
    }

    inline bool operator<(const PathT &other) const { return path_ < other.path_; }

    String to_string() const;
    const std::string &to_std_string() const;
    const char *c_str() const;


private:
    std::string path_;
};

inline PathT operator+(const String &one, const PathT &other) { return PathT(one) + other; }

inline PathT operator+(const char *one, const PathT &other) { return PathT(one) + other; }

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::io::PathT &p) {
    s << p.to_std_string();
    return s;
}

std::string suffix(const PathT &path);
PathT filename(const PathT &path, bool including_extension = true);
PathT basename(const PathT &path);
PathT complete_basename(const PathT &path);
PathT absolute_path(const PathT &path);
PathT absolute_file_path(const PathT &path);
//path_t dirname(const path_t& path);
PathT dirpath(const PathT &path);
PathT absolute_dirpath(const PathT &path);

bool is_absolute(const PathT &path);

bool is_allowed_file_name(const PathT &fn);
PathT escape_file_name(const PathT &fn);

PathT to_native_separators(const PathT &path);

paths_t paths_from_string(const std::string &str, const std::string &delim = ";");
std::string paths_to_string(const paths_t &ps, const std::string &delim = ";");
} // namespace muse::io

#endif // MUSE_IO_PATH_H
