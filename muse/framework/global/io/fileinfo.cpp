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

FileInfo::FileInfo(const path_t &filePath) : filePath_(filePath.toString()) {}

String FileInfo::path() const {
    size_t lastSep = filePath_.lastIndexOf(u'/');
    if (lastSep == muse::nidx) {
#if defined(_WIN32)
        if (filePath_.size() >= 2 && filePath_.at(1) == u':') {
            return filePath_.left(2);
        }
#endif
        return String(u'.');
    }
    if (lastSep == 0) {
        return String(u'/');
    }
#if defined(_WIN32)
    if (lastSep == 2 && filePath_.at(1) == u':') {
        return filePath_.left(lastSep + 1);
    }
#endif
    return filePath_.left(lastSep);
}

String FileInfo::filePath() const { return filePath_; }

String FileInfo::canonicalFilePath() const { return fileSystem()->canonicalFilePath(filePath_).toString(); }

String FileInfo::absolutePath() const { return fileSystem()->absolutePath(filePath_).toString(); }

String FileInfo::absoluteFilePath() const { return fileSystem()->absoluteFilePath(filePath_).toString(); }

String FileInfo::fileName() const {
    size_t lastSep = filePath_.lastIndexOf(u'/');
#if defined(_WIN32)
    if (lastSep == muse::nidx && filePath_.size() >= 2 && filePath_.at(1) == u':') {
        return filePath_.mid(2);
    }
#endif
    return filePath_.mid(lastSep + 1);
}

String FileInfo::baseName() const {
    size_t lastSep = filePath_.lastIndexOf(u'/');
    size_t from = lastSep + 1;
    size_t firstDot = filePath_.indexOf(u'.', from);
    size_t to = firstDot > 0 ? firstDot : filePath_.size();
    size_t length = to - from;

#if defined(_WIN32)
    if (lastSep == muse::nidx && filePath_.size() >= 2 && filePath_.at(1) == u':') {
        return filePath_.mid(2, length - 2);
    }
#endif
    return filePath_.mid(from, length);
}

String FileInfo::completeBaseName() const {
    size_t lastSep = filePath_.lastIndexOf(u'/');
    size_t from = lastSep + 1;
    size_t lastDot = filePath_.lastIndexOf(u'.');
    size_t to = lastDot > 0 ? lastDot : filePath_.size();
    size_t length = to - from;

#if defined(_WIN32)
    if (lastSep == muse::nidx && filePath_.size() >= 2 && filePath_.at(1) == u':') {
        return filePath_.mid(2, length - 2);
    }
#endif
    return filePath_.mid(from, length);
}

String FileInfo::suffix() const { return doSuffix(filePath_); }

String FileInfo::suffix(const path_t &filePath) { return doSuffix(filePath.toString()); }

String FileInfo::doSuffix(const String &filePath) {
    size_t lastDot = filePath.lastIndexOf(u'.');
    if (lastDot == muse::nidx) {
        return String();
    }

    size_t lastSep = filePath.lastIndexOf(u'/');
    if (lastSep == muse::nidx) {
        lastSep = 0;
    }

    if (lastDot < lastSep) {
        return String();
    }

    return filePath.mid(lastDot + 1);
}

DateTime FileInfo::birthTime() const { return fileSystem()->birthTime(filePath_); }

DateTime FileInfo::lastModified() const { return fileSystem()->lastModified(filePath_); }

EntryType FileInfo::entryType() const { return fileSystem()->entryType(filePath_); }

bool FileInfo::isRelative() const { return !isAbsolute(); }

bool FileInfo::isAbsolute() const {
#ifdef _WIN32
    return (filePath_.size() >= 3 && filePath_.at(0).isLetter() && filePath_.at(1) == u':' && filePath_.at(2) == u'/') ||
           (!filePath_.isEmpty() && (filePath_.at(0) == u'/' || filePath_.at(0) == u':'));
#else
    return !filePath_.empty() && (filePath_.at(0) == u'/' || filePath_.at(0) == u':');
#endif
}

bool FileInfo::isSymLink() const { return fileSystem()->isSymLink(filePath_); }

path_t FileInfo::symLinkTarget() const { return fileSystem()->symLinkTarget(filePath_); }

bool FileInfo::exists() const { return fileSystem()->exists(filePath_); }

bool FileInfo::exists(const path_t &filePath) { return fileSystem()->exists(filePath); }

path_t FileInfo::dirPath() const {
    size_t lastSep = filePath_.lastIndexOf(u'/');
    if (lastSep == muse::nidx) {
        return ".";
    }
    return filePath_.mid(0, lastSep);
}

Dir FileInfo::dir() const { return Dir(dirPath()); }
