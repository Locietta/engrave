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
        io::PathT file_path;
        bool is_dir = false;
        bool is_file = false;
        bool is_sym_link = false;
        uint64_t size = 0;

        bool is_valid() const { return is_dir || is_file || is_sym_link; }
    };

    static bool starts_with_entry(const ByteArray &data, std::span<const uint8_t> name);

    explicit ZipReader(const io::PathT &file_path);
    explicit ZipReader(io::IODevice *device);
    ~ZipReader();

    bool exists() const;
    void close();
    bool has_error() const;

    std::vector<FileInfo> file_info_list() const;
    bool file_exists(const std::string &file_name) const;
    ByteArray file_data(const std::string &file_name) const;

private:
    struct Impl;
    Impl *impl_ = nullptr;
    io::PathT file_path_;
};

class ZipUnpack {
public:

    Ret unpack(const io::PathT &zip_path, const io::PathT &dir_path);
};
} // namespace muse

#endif // MUSE_GLOBAL_ZIPREADER_H
