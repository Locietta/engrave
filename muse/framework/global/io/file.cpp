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
#include "file.h"

#include <cstring>

#include "ioretcodes.h"

#include "log.h"

using namespace muse;
using namespace muse::io;

namespace {
IFileSystem *s_fileSystem = nullptr;
} // namespace

IFileSystem *muse::io::fileSystem() { return s_fileSystem; }

void muse::io::setFileSystem(IFileSystem *fileSystem) { s_fileSystem = fileSystem; }

File::File(const path_t &filePath) : filePath_(filePath) { setMeta("file_path", filePath_.toStdString()); }

File::~File() { close(); }

const path_t &File::filePath() const { return filePath_; }

bool File::exists() const { return fileSystem()->exists(filePath_); }

bool File::exists(const path_t &filePath) { return fileSystem()->exists(filePath); }

bool File::remove(const path_t &filePath) { return fileSystem()->remove(filePath); }

bool File::copy(const path_t &src, const path_t &dst, bool replace) { return fileSystem()->copy(src, dst, replace); }

Ret File::readFile(const io::path_t &filePath, ByteArray &out) {
    bool ok = fileSystem()->readFile(filePath, out);
    return make_ret(ok ? Err::NoError : Err::FSReadError);
}

Ret File::writeFile(const io::path_t &filePath, const ByteArray &data) { return fileSystem()->writeFile(filePath, data); }

bool File::setPermissionsAllowedForAll(const path_t &filePath) { return fileSystem()->setPermissionsAllowedForAll(filePath); }

bool File::remove() { return fileSystem()->remove(filePath_); }

bool File::doOpen(OpenMode m) {
    if (m == IODevice::WriteOnly) {
        Ret ret = fileSystem()->isWritable(filePath_);
        if (!ret) {
            setError(ret.code(), ret.text());
        }

        return ret;
    }

    if (!exists()) {
        if (m == OpenMode::ReadOnly) {
            setError(int(Err::FSReadError), "Opening non-existent file called on a read-only mode");
            return false;
        } else {
            return true;
        }
    }

    data_ = ByteArray();
    Ret ret = fileSystem()->readFile(filePath_, data_);
    if (!ret) {
        setError(ret.code(), ret.text());
        return false;
    }

    return true;
}

size_t File::dataSize() const { return data_.size(); }

const uint8_t *File::rawData() const { return data_.constData(); }

bool File::resizeData(size_t size) {
    data_.resize(size);
    return true;
}

size_t File::writeData(const uint8_t *data, size_t len) {
    std::memcpy(data_.data() + pos(), data, len);
    Ret ret = fileSystem()->writeFile(filePath_, data_);
    if (!ret) {
        setError(ret.code(), ret.text());
        return 0;
    }
    return len;
}
