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
#include "dir.h"

using namespace muse;
using namespace muse::io;

Dir::Dir(const PathT &path) : path_(path) {}

PathT Dir::path() const { return path_; }

PathT Dir::absolute_path() const { return file_system()->absolute_file_path(path_); }

bool Dir::exists() const { return file_system()->exists(path_); }

Ret Dir::remove_recursively() { return file_system()->remove(path_); }

Ret Dir::mkpath() { return mkpath(path_); }

Ret Dir::mkpath(const PathT &path) { return file_system()->make_path(path); }

RetVal<io::paths_t> Dir::scan_files(const io::PathT &root_dir, const std::vector<std::string> &filters, ScanMode mode) {
    return file_system()->scan_files(root_dir, filters, mode);
}

PathT Dir::from_native_separators(const PathT &path_name) {
#if defined(_WIN32)
    String path = path_name.to_string();
    size_t i = path.index_of(u'\\');
    if (i != muse::nidx) {
        String n(path);
        if (n.starts_with(u"\\\\?\\")) {
            n.remove(0, 4);
            i = n.index_of(u'\\');
            if (i == muse::nidx) {
                return n;
            }
        }

        for (i = 0; i < n.size(); ++i) {
            if (n.at(i) == u'\\') {
                n[i] = u'/';
            }
        }

        return n;
    }
#endif
    return path_name;
}

PathT Dir::clean_path(const PathT &path) { return file_system()->clean_path(path); }

PathT Dir::temp_path() { return file_system()->temporary_directory_path(); }
