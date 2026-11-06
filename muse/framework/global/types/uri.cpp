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
#include "uri.h"

#include <iomanip>

#include "global/stringutils.h"
#include "log.h"

using namespace muse;

static const std::string uri_val_true("true");
static const std::string uri_val_false("false");

// muse://module/target/name

static std::string extract_path(const std::string &str) {
    auto scheme_pos = str.find(':');
    auto params_pos = str.find('?');
    auto path_pos = (scheme_pos != std::string::npos) ? (scheme_pos + 3) : 0;
    size_t path_n = (params_pos != std::string::npos) ? (params_pos - path_pos) : std::string::npos;

    return str.substr(path_pos, path_n);
}

Uri::Uri(const std::string &str) {
    auto scheme_pos = str.find(':');
    if (scheme_pos != std::string::npos) {
        scheme_ = str.substr(0, scheme_pos);
    }

    path_ = extract_path(str);
}

Uri::Uri(const String &str) : Uri(str.to_std_string()) {}

Uri::Uri(const std::string &scheme, const std::string &path) : scheme_(scheme) { path_ = extract_path(path); }

bool Uri::is_valid() const {
    if (scheme_.empty()) {
        return false;
    }

    if (path_.empty()) {
        return false;
    }

    return true;
}

const Uri::Scheme &Uri::scheme() const { return scheme_; }

void Uri::set_scheme(const Scheme &scheme) { scheme_ = scheme; }

const std::string &Uri::path() const { return path_; }

void Uri::set_path(const std::string &path) { path_ = path; }

void Uri::add_path(const std::string &path) {
    if (path.empty()) {
        return;
    }

    if (path_.empty()) {
        path_ = path;
        return;
    }

    if (path_.back() != '/') {
        path_ += '/';
    }

    if (path.front() == '/') {
        path_ += path.substr(1);
    } else {
        path_ += path;
    }
}

std::vector<std::string> Uri::path_segments() const {
    std::vector<std::string> segments;
    strings::split(path_, segments, "/");
    return segments;
}

std::string Uri::to_string() const { return scheme_ + "://" + path_; }

static bool should_encode(char c) {
    static const std::string safe_chars = "abcdefghijklmnopqrstuvwxyz"
                                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "0123456789"
                                          "-_.~/";

    return safe_chars.find(c) == std::string::npos;
}

Uri Uri::from_local_file(const io::PathT &path) {
    std::string absolute_path = io::absolute_file_path(path).to_std_string();

    std::replace(absolute_path.begin(), absolute_path.end(), '\\', '/');

    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex << std::uppercase;

    for (char c : absolute_path) {
        if (should_encode(c)) {
            encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        } else {
            encoded << c;
        }
    }

    return Uri("file", encoded.str());
}

io::PathT Uri::to_local_file() const {
    std::string decoded;
    for (size_t i = 0; i < path_.size(); ++i) {
        if (path_[i] == '%' && i + 2 < path_.size()) {
            std::string hex = path_.substr(i + 1, 2);
            char c = static_cast<char>(std::stoi(hex, nullptr, 16));
            decoded += c;
            i += 2;
        } else {
            decoded += path_[i];
        }
    }

    return io::PathT(decoded);
}

// muse://module/target/name?param1=value1&paramn=valuen

UriQuery::UriQuery(const std::string &str) : uri_(str) { parse_params(str, params_); }

UriQuery::UriQuery(const String &str) : UriQuery(str.to_std_string()) {}

UriQuery::UriQuery(const Uri &uri) : uri_(uri) {}

void UriQuery::parse_params(const std::string &uri, Params &out) const {
    auto params_pos = uri.find('?');
    if (params_pos == std::string::npos) {
        return;
    }

    std::string params_str = uri.substr(params_pos + 1);

    strings::trim(params_str);

    std::map<std::string, std::string> placeholders;
    std::vector<std::string> quotes_strings;
    extract_quoted_strings(params_str, quotes_strings);
    for (size_t i = 0; i < quotes_strings.size(); ++i) {
        std::string key = "s" + std::to_string(i);
        const std::string &val = quotes_strings.at(i);

        strings::replace(params_str, val, key);
        placeholders[key] = val;
    }

    std::vector<std::string> params_pairs;
    strings::split(params_str, params_pairs, "&");

    for (const std::string &pair : params_pairs) {
        size_t eq_pos = pair.find('=');
        if (eq_pos == std::string::npos) {
            LOGE() << "Invalid param: " << pair << ", in uri: " << uri;
            continue;
        }
        std::string key = pair.substr(0, eq_pos);
        strings::trim(key);

        std::string val = pair.substr(eq_pos + 1);

        //! NOTE Val is bool?
        if (::uri_val_true == val || ::uri_val_false == val) {
            out[key] = Val(val == ::uri_val_true);
            continue;
        }

        auto it = placeholders.find(val);
        if (it != placeholders.end()) {
            val = it->second;
        }

        strings::trim(val);

        if (val.size() > 2 && val.at(0) == '\'' && val.at(val.size() - 1) == '\'') {
            val = val.substr(1, val.size() - 2);
        }

        out[key] = Val(val);
    }
}

void UriQuery::extract_quoted_strings(const std::string &str, std::vector<std::string> &out) const {
    //! NOTE It is necessary to get substrings limited to single quotes from a string
    //! Example - "path='path/to/file.jpg'"

    int bi = -1;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str.at(i) == u'\'') {
            if (bi == -1) { // begin quotes string
                bi = int(i);
            } else { // end quotes string
                out.push_back(str.substr(bi, i - bi + 1));
                bi = -1;
            }
        }
    }
}

std::string UriQuery::to_string() const {
    std::string str = uri_.to_string();
    if (!params_.empty()) {
        str += "?";
        for (auto it = params_.cbegin(); it != params_.cend(); ++it) {
            str += it->first + "=" + it->second.to_string() + "&";
        }

        str.erase(str.size() - 1);
    }
    return str;
}

bool UriQuery::is_valid() const { return uri_.is_valid(); }

const Uri &UriQuery::uri() const { return uri_; }

void UriQuery::set_scheme(const Uri::Scheme &scheme) { uri_.set_scheme(scheme); }

const UriQuery::Params &UriQuery::params() const { return params_; }

void UriQuery::set_params(const Params &params) { params_ = params; }

Val UriQuery::param(const std::string &key, const Val &def) const {
    auto it = params_.find(key);
    if (it == params_.end()) {
        return def;
    }
    return it->second;
}

void UriQuery::add_param(const std::string &key, const Val &val) { params_[key] = val; }

UriQuery &UriQuery::set(const ValMap &vals) {
    params_ = vals;
    return *this;
}

UriQuery &UriQuery::set(const std::string &key, const Val &val) {
    params_[key] = val;
    return *this;
}

UriQuery UriQuery::set(const ValMap &vals) const {
    UriQuery copy(*this);
    copy.set(vals);
    return copy;
}

UriQuery UriQuery::set(const std::string &key, const Val &val) const {
    UriQuery copy(*this);
    copy.set(key, val);
    return copy;
}

UriQuery UriQuery::adding_param(const std::string &key, const Val &val) const {
    UriQuery copy(*this);
    copy.add_param(key, val);
    return copy;
}

bool UriQuery::contains(const std::string &key) const { return params_.count(key) > 0; }

bool UriQuery::operator==(const UriQuery &query) const { return uri_ == query.uri_ && params_ == query.params_; }

bool UriQuery::operator!=(const UriQuery &query) const { return !(*this == query); }
