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

#include <map>
#include <string>
#include <vector>

#include "global/io/path.h"
#include "global/logstream.h"
#include "val.h"

namespace muse {
struct Uri {
    Uri() = default;
    explicit Uri(const std::string &str);
    explicit Uri(const muse::String &str);
    explicit Uri(const std::string &scheme, const std::string &path);

    using Scheme = std::string;

    bool is_valid() const;

    const Scheme &scheme() const;
    void set_scheme(const Scheme &scheme);

    const std::string &path() const;
    void set_path(const std::string &path);
    void add_path(const std::string &path);
    std::vector<std::string> path_segments() const;

    inline bool operator==(const Uri &uri) const { return path_ == uri.path_ && scheme_ == uri.scheme_; }

    inline bool operator!=(const Uri &uri) const { return !(*this == uri); }

    inline bool operator<(const Uri &uri) const {
        if (scheme_ != uri.scheme_) {
            return scheme_ < uri.scheme_;
        }
        return path_ < uri.path_;
    }

    std::string to_string() const;

    io::PathT to_local_file() const;
    static Uri from_local_file(const io::PathT &path);

private:

    Scheme scheme_;
    std::string path_;
};

struct UriQuery {

    using Params = std::map<std::string /*key*/, Val>;

    UriQuery() = default;
    explicit UriQuery(const std::string &str);
    explicit UriQuery(const String &str);
    explicit UriQuery(const Uri &uri);

    bool is_valid() const;
    const Uri &uri() const;
    void set_scheme(const Uri::Scheme &scheme);

    const Params &params() const;
    Val param(const std::string &key, const Val &def = Val()) const;

    void set_params(const Params &params);

    UriQuery &set(const ValMap &vals);
    UriQuery &set(const std::string &key, const Val &val);

    UriQuery &set(const std::string &key, const ValList &vals) { return set(key, Val(vals)); }

    UriQuery &set(const std::string &key, const std::string &val) { return set(key, Val(val)); }

    UriQuery &set(const std::string &key, const char *val) { return set(key, Val(val)); }

    UriQuery &set(const std::string &key, int val) { return set(key, Val(val)); }

    UriQuery &set(const std::string &key, bool val) { return set(key, Val(val)); }

    UriQuery set(const ValMap &vals) const;
    UriQuery set(const std::string &key, const Val &val) const;

    UriQuery set(const std::string &key, const ValList &vals) const { return set(key, Val(vals)); }

    UriQuery set(const std::string &key, const std::string &val) const { return set(key, Val(val)); }

    UriQuery set(const std::string &key, const char *val) const { return set(key, Val(val)); }

    UriQuery set(const std::string &key, int val) const { return set(key, Val(val)); }

    UriQuery set(const std::string &key, bool val) const { return set(key, Val(val)); }

    void add_param(const std::string &key, const Val &val);
    UriQuery adding_param(const std::string &key, const Val &val) const;
    bool contains(const std::string &key) const;

    std::string to_string() const;

    bool operator==(const UriQuery &query) const;
    bool operator!=(const UriQuery &query) const;

private:

    void parse_params(const std::string &str, Params &out) const;
    void extract_quoted_strings(const std::string &str, std::vector<std::string> &out) const;

    Uri uri_;
    Params params_;
};
} // namespace muse

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::Uri &uri) {
    s << uri.to_string();
    return s;
}

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::UriQuery &q) {
    s << q.to_string();
    return s;
}

template <>
struct std::hash<muse::Uri> {
    std::size_t operator()(const muse::Uri &uri) const noexcept { return std::hash<std::string>{}(uri.to_string()); }
};
