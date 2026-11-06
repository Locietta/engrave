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
#ifndef MUSE_GLOBAL_ZIPCONTAINER_H
#define MUSE_GLOBAL_ZIPCONTAINER_H

#include <ctime>
#include <span>
#include <string>

#include "io/iodevice.h"

namespace muse {
struct ZipContainer {
    static bool starts_with_entry(const uint8_t *data, size_t size, std::span<const uint8_t> name);

    explicit ZipContainer(io::IODevice *device);
    ~ZipContainer();

    enum Status { NO_ERROR, FILE_OPEN_ERROR, FILE_READ_ERROR, FILE_WRITE_ERROR, FILE_ERROR };

    struct FileInfo {
        std::string file_path;
        bool is_dir = false;
        bool is_file = false;
        bool is_sym_link = false;
        unsigned int crc = 0;
        int64_t size = 0;
        std::tm last_modified;

        bool is_valid() const { return is_dir || is_file || is_sym_link; }
    };

    Status status() const;

    void close();

    // Read
    std::vector<FileInfo> file_info_list() const;
    int count() const;

    bool file_exists(const std::string &file_name) const;
    ByteArray file_data(const std::string &file_name) const;

    // Write
    enum CompressionPolicy { ALWAYS_COMPRESS, NEVER_COMPRESS, AUTO_COMPRESS };

    void set_compression_policy(CompressionPolicy policy);
    CompressionPolicy compression_policy() const;

    void add_file(const std::string &file_name, const ByteArray &data);
    void add_directory(const std::string &dir_name);

private:

    struct Impl;
    Impl *p = nullptr;
};
} // namespace muse

#endif // MUSE_GLOBAL_ZIPCONTAINER_H
