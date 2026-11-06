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
IFileSystem *s_file_system = nullptr;
} // namespace

IFileSystem *muse::io::file_system() { return s_file_system; }

void muse::io::set_file_system(IFileSystem *file_system) { s_file_system = file_system; }

File::File(const PathT &file_path) : file_path_(file_path) { set_meta("file_path", file_path_.to_std_string()); }

File::~File() { close(); }

const PathT &File::file_path() const { return file_path_; }

bool File::exists() const { return file_system()->exists(file_path_); }

bool File::exists(const PathT &file_path) { return file_system()->exists(file_path); }

bool File::remove(const PathT &file_path) { return file_system()->remove(file_path); }

bool File::copy(const PathT &src, const PathT &dst, bool replace) { return file_system()->copy(src, dst, replace); }

Ret File::read_file(const io::PathT &file_path, ByteArray &out) {
    bool ok = file_system()->read_file(file_path, out);
    return make_ret(ok ? Err::NO_ERROR : Err::FS_READ_ERROR);
}

Ret File::write_file(const io::PathT &file_path, const ByteArray &data) { return file_system()->write_file(file_path, data); }

bool File::set_permissions_allowed_for_all(const PathT &file_path) { return file_system()->set_permissions_allowed_for_all(file_path); }

bool File::remove() { return file_system()->remove(file_path_); }

bool File::do_open(OpenMode m) {
    if (m == IODevice::WRITE_ONLY) {
        Ret ret = file_system()->is_writable(file_path_);
        if (!ret) {
            set_error(ret.code(), ret.text());
        }

        return ret;
    }

    if (!exists()) {
        if (m == OpenMode::READ_ONLY) {
            set_error(int(Err::FS_READ_ERROR), "Opening non-existent file called on a read-only mode");
            return false;
        } else {
            return true;
        }
    }

    data_ = ByteArray();
    Ret ret = file_system()->read_file(file_path_, data_);
    if (!ret) {
        set_error(ret.code(), ret.text());
        return false;
    }

    return true;
}

size_t File::data_size() const { return data_.size(); }

const uint8_t *File::raw_data() const { return data_.const_data(); }

bool File::resize_data(size_t size) {
    data_.resize(size);
    return true;
}

size_t File::write_data(const uint8_t *data, size_t len) {
    std::memcpy(data_.data() + pos(), data, len);
    Ret ret = file_system()->write_file(file_path_, data_);
    if (!ret) {
        set_error(ret.code(), ret.text());
        return 0;
    }
    return len;
}
