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
#ifndef MUSE_GLOBAL_ZIPREADER_H
#define MUSE_GLOBAL_ZIPREADER_H

#include <span>
#include <vector>

#include "global/types/ret.h"
#include "global/io/path.h"
#include "global/io/iodevice.h"

namespace muse {
class ZipReader {
public:

    struct FileInfo {
        io::path_t filePath;
        bool isDir = false;
        bool isFile = false;
        bool isSymLink = false;
        uint64_t size = 0;

        bool isValid() const { return isDir || isFile || isSymLink; }
    };

    static bool startsWithEntry(const ByteArray &data, std::span<const uint8_t> name);

    explicit ZipReader(const io::path_t &filePath);
    explicit ZipReader(io::IODevice *device);
    ~ZipReader();

    bool exists() const;
    void close();
    bool hasError() const;

    std::vector<FileInfo> fileInfoList() const;
    bool fileExists(const std::string &fileName) const;
    ByteArray fileData(const std::string &fileName) const;

private:
    struct Impl;
    Impl *impl_ = nullptr;
    io::path_t filePath_;
};

class ZipUnpack {
public:

    Ret unpack(const io::path_t &zipPath, const io::path_t &dirPath);
};
} // namespace muse

#endif // MUSE_GLOBAL_ZIPREADER_H
