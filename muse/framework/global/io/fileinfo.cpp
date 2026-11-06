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
#include "fileinfo.h"

using namespace muse;
using namespace muse::io;

FileInfo::FileInfo(const PathT &file_path) : file_path_(file_path.to_string()) {}

String FileInfo::path() const {
    size_t last_sep = file_path_.last_index_of(u'/');
    if (last_sep == muse::nidx) {
#if defined(_WIN32)
        if (file_path_.size() >= 2 && file_path_.at(1) == u':') {
            return file_path_.left(2);
        }
#endif
        return String(u'.');
    }
    if (last_sep == 0) {
        return String(u'/');
    }
#if defined(_WIN32)
    if (last_sep == 2 && file_path_.at(1) == u':') {
        return file_path_.left(last_sep + 1);
    }
#endif
    return file_path_.left(last_sep);
}

String FileInfo::file_path() const { return file_path_; }

String FileInfo::canonical_file_path() const { return file_system()->canonical_file_path(file_path_).to_string(); }

String FileInfo::absolute_path() const { return file_system()->absolute_path(file_path_).to_string(); }

String FileInfo::absolute_file_path() const { return file_system()->absolute_file_path(file_path_).to_string(); }

String FileInfo::file_name() const {
    size_t last_sep = file_path_.last_index_of(u'/');
#if defined(_WIN32)
    if (last_sep == muse::nidx && file_path_.size() >= 2 && file_path_.at(1) == u':') {
        return file_path_.mid(2);
    }
#endif
    return file_path_.mid(last_sep + 1);
}

String FileInfo::base_name() const {
    size_t last_sep = file_path_.last_index_of(u'/');
    size_t from = last_sep + 1;
    size_t first_dot = file_path_.index_of(u'.', from);
    size_t to = first_dot > 0 ? first_dot : file_path_.size();
    size_t length = to - from;

#if defined(_WIN32)
    if (last_sep == muse::nidx && file_path_.size() >= 2 && file_path_.at(1) == u':') {
        return file_path_.mid(2, length - 2);
    }
#endif
    return file_path_.mid(from, length);
}

String FileInfo::complete_base_name() const {
    size_t last_sep = file_path_.last_index_of(u'/');
    size_t from = last_sep + 1;
    size_t last_dot = file_path_.last_index_of(u'.');
    size_t to = last_dot > 0 ? last_dot : file_path_.size();
    size_t length = to - from;

#if defined(_WIN32)
    if (last_sep == muse::nidx && file_path_.size() >= 2 && file_path_.at(1) == u':') {
        return file_path_.mid(2, length - 2);
    }
#endif
    return file_path_.mid(from, length);
}

String FileInfo::suffix() const { return do_suffix(file_path_); }

String FileInfo::suffix(const PathT &file_path) { return do_suffix(file_path.to_string()); }

String FileInfo::do_suffix(const String &file_path) {
    size_t last_dot = file_path.last_index_of(u'.');
    if (last_dot == muse::nidx) {
        return String();
    }

    size_t last_sep = file_path.last_index_of(u'/');
    if (last_sep == muse::nidx) {
        last_sep = 0;
    }

    if (last_dot < last_sep) {
        return String();
    }

    return file_path.mid(last_dot + 1);
}

DateTime FileInfo::birth_time() const { return file_system()->birth_time(file_path_); }

DateTime FileInfo::last_modified() const { return file_system()->last_modified(file_path_); }

EntryType FileInfo::entry_type() const { return file_system()->entry_type(file_path_); }

bool FileInfo::is_relative() const { return !is_absolute(); }

bool FileInfo::is_absolute() const {
#ifdef _WIN32
    return (file_path_.size() >= 3 && file_path_.at(0).is_letter() && file_path_.at(1) == u':' && file_path_.at(2) == u'/') ||
           (!file_path_.is_empty() && (file_path_.at(0) == u'/' || file_path_.at(0) == u':'));
#else
    return !file_path_.empty() && (file_path_.at(0) == u'/' || file_path_.at(0) == u':');
#endif
}

bool FileInfo::is_sym_link() const { return file_system()->is_sym_link(file_path_); }

PathT FileInfo::sym_link_target() const { return file_system()->sym_link_target(file_path_); }

bool FileInfo::exists() const { return file_system()->exists(file_path_); }

bool FileInfo::exists(const PathT &file_path) { return file_system()->exists(file_path); }

PathT FileInfo::dir_path() const {
    size_t last_sep = file_path_.last_index_of(u'/');
    if (last_sep == muse::nidx) {
        return ".";
    }
    return file_path_.mid(0, last_sep);
}

Dir FileInfo::dir() const { return Dir(dir_path()); }
