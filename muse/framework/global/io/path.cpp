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
#include "path.h"

#include "fileinfo.h"
#include "global/stringutils.h"

using namespace muse;
using namespace muse::io;

PathT::PathT(const String &s) : path_(s.to_std_string()) {}

PathT::PathT(const std::string &s) : path_(s) {}

PathT::PathT(std::string &&s) : path_(std::move(s)) {}

PathT::PathT(const char *s) : path_(s ? s : "") {}

bool PathT::empty() const { return path_.empty(); }

size_t PathT::size() const { return path_.size(); }

bool PathT::has_suffix(const char *str) const { return io::FileInfo::suffix(path_).to_lower() == str; }

PathT PathT::appending_component(const PathT &other) const {
    if ((!path_.empty() && path_.back() == '/') || (!other.path_.empty() && other.path_.front() == '/')) {
        return path_ + other.path_;
    }

    return path_ + '/' + other.path_;
}

PathT PathT::appending_suffix(const PathT &suffix) const {
    if (!suffix.path_.empty() && suffix.path_.front() == '.') {
        return path_ + suffix.path_;
    }

    return path_ + '.' + suffix.path_;
}

String PathT::to_string() const { return String::from_std_string(path_); }

const std::string &PathT::to_std_string() const { return path_; }

const char *PathT::c_str() const { return path_.c_str(); }

std::string muse::io::suffix(const io::PathT &path) { return FileInfo::suffix(path).to_lower().to_std_string(); }

io::PathT muse::io::filename(const io::PathT &path, bool including_extension) {
    FileInfo fi(path);
    return including_extension ? fi.file_name() : fi.complete_base_name();
}

io::PathT muse::io::basename(const io::PathT &path) {
    FileInfo fi(path);
    return fi.base_name();
}

io::PathT muse::io::complete_basename(const io::PathT &path) {
    FileInfo fi(path);
    return fi.complete_base_name();
}

io::PathT muse::io::absolute_path(const PathT &path) { return FileInfo(path).absolute_path(); }

io::PathT muse::io::absolute_file_path(const PathT &path) { return FileInfo(path).absolute_file_path(); }

io::PathT muse::io::dirpath(const io::PathT &path) { return FileInfo(path).dir().path(); }

io::PathT muse::io::absolute_dirpath(const io::PathT &path) { return FileInfo(path).dir().absolute_path(); }

bool muse::io::is_absolute(const PathT &path) { return FileInfo(path).is_absolute(); }

bool muse::io::is_allowed_file_name(const PathT &input_fn) {
    std::string fn = basename(input_fn).to_std_string();
    if (fn.empty()) {
        return false;
    }

    // Windows filenames are not case sensitive.
    fn = String::from_std_string(fn).to_upper().to_std_string();

    static const std::string illegal = "<>:\"|?*";

    for (const char &c : fn) {
        // Check for control characters
        if (c > 0 && c < 32) {
            return false;
        }

        // Check for illegal characters
        for (const char &ilc : illegal) {
            if (c == ilc) {
                return false;
            }
        }
    }

    // Check for device names in filenames
    static const std::vector<std::string> devices = {"CON",  "PRN",  "AUX",  "NUL",  "COM0", "COM1", "COM2", "COM3",
                                                     "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT0", "LPT1",
                                                     "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

    for (const std::string &s : devices) {
        if (fn == s) {
            return false;
        }
    }

    // Check for trailing periods or spaces
    if (fn.back() == '.' || fn.back() == ' ') {
        return false;
    }

    // Check for pathnames that are too long
    if (fn.size() > 96) {
        return false;
    }

    // Since we are checking for a filename, it mustn't be a directory
    if (fn.back() == '\\') {
        return false;
    }

    return true;
}

io::PathT muse::io::escape_file_name(const io::PathT &input_fn) {
    //
    // special characters in filenames are a constant source
    // of trouble, this replaces some of them common in german:
    //
    String fn = input_fn.to_string();
    fn = fn.simplified();
    fn = fn.replace(' ', '_');
    fn = fn.replace('\n', '_');
    fn = fn.replace(Char(0xe4), u"ae");  // &auml;
    fn = fn.replace(Char(0xf6), u"oe");  // &ouml;
    fn = fn.replace(Char(0xfc), u"ue");  // &uuml;
    fn = fn.replace(Char(0xdf), u"ss");  // &szlig;
    fn = fn.replace(Char(0xc4), u"Ae");  // &Auml;
    fn = fn.replace(Char(0xd6), u"Oe");  // &Ouml;
    fn = fn.replace(Char(0xdc), u"Ue");  // &Uuml;
    fn = fn.replace(Char(0x266d), u"b"); // musical flat sign, happen in instrument names, so can happen in part (file) names
    fn = fn.replace(Char(0x266f), u"#"); // musical sharp sign, can happen in titles, so can happen in score (file) names
    //FAT/NTFS special chars
    fn = fn.replace('<', '_')
             .replace('>', '_')
             .replace(':', '_')
             .replace('"', '_')
             .replace('/', '_')
             .replace('\\', '_')
             .replace('|', '_')
             .replace('?', '_')
             .replace('*', '_');

    return fn;
}

paths_t muse::io::paths_from_string(const std::string &str, const std::string &delim) {
    if (str.empty()) {
        return {};
    }

    std::vector<std::string> strs;
    strings::split(str, strs, delim);

    paths_t ps;
    ps.reserve(strs.size());
    for (const std::string &s : strs) {
        ps.push_back(PathT(s));
    }
    return ps;
}

std::string muse::io::paths_to_string(const paths_t &ps, const std::string &delim) {
    std::string result;
    bool first = true;
    for (const PathT &path_value : ps) {
        if (!first) {
            result += delim;
        }
        first = false;
        result += path_value.to_std_string();
    }

    return result;
}

PathT muse::io::to_native_separators(const PathT &path) { return path; }
