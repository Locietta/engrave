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

Dir::Dir(const path_t &path) : path_(path) {}

path_t Dir::path() const { return path_; }

path_t Dir::absolutePath() const { return fileSystem()->absoluteFilePath(path_); }

bool Dir::exists() const { return fileSystem()->exists(path_); }

Ret Dir::removeRecursively() { return fileSystem()->remove(path_); }

Ret Dir::mkpath() { return mkpath(path_); }

Ret Dir::mkpath(const path_t &path) { return fileSystem()->makePath(path); }

RetVal<io::paths_t> Dir::scanFiles(const io::path_t &rootDir, const std::vector<std::string> &filters, ScanMode mode) {
    return fileSystem()->scanFiles(rootDir, filters, mode);
}

path_t Dir::fromNativeSeparators(const path_t &pathName) {
#if defined(_WIN32)
    String path = pathName.toString();
    size_t i = path.indexOf(u'\\');
    if (i != muse::nidx) {
        String n(path);
        if (n.startsWith(u"\\\\?\\")) {
            n.remove(0, 4);
            i = n.indexOf(u'\\');
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
    return pathName;
}

path_t Dir::cleanPath(const path_t &path) { return fileSystem()->cleanPath(path); }

path_t Dir::tempPath() { return fileSystem()->temporaryDirectoryPath(); }
