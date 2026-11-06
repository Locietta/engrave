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
#include "zipreader.h"

#include "global/io/dir.h"
#include "global/io/file.h"
#include "internal/zipcontainer.h"

using namespace muse;
using namespace muse::io;

struct ZipReader::Impl {
    ZipContainer *zip = nullptr;
    IODevice *device = nullptr;
    bool is_self_device = false;
};

bool ZipReader::starts_with_entry(const ByteArray &data, std::span<const uint8_t> name) {
    return ZipContainer::starts_with_entry(data.const_data(), data.size(), name);
}

ZipReader::ZipReader(const io::PathT &file_path) : file_path_(file_path) {
    impl_ = new Impl();
    impl_->device = new File(file_path);
    impl_->is_self_device = true;
    if (impl_->device->open(IODevice::READ_ONLY)) {}
    impl_->zip = new ZipContainer(impl_->device);
}

ZipReader::ZipReader(IODevice *device) {
    impl_ = new Impl();
    impl_->device = device;
    impl_->zip = new ZipContainer(impl_->device);
}

ZipReader::~ZipReader() {
    close();
    delete impl_->zip;
    if (impl_->is_self_device) {
        delete impl_->device;
    }
    delete impl_;
}

bool ZipReader::exists() const {
    if (!file_path_.empty()) {
        return File::exists(file_path_);
    }

    return true;
}

void ZipReader::close() { impl_->zip->close(); }

bool ZipReader::has_error() const { return impl_->zip->status() != ZipContainer::NO_ERROR; }

std::vector<ZipReader::FileInfo> ZipReader::file_info_list() const {
    std::vector<FileInfo> ret;
    std::vector<ZipContainer::FileInfo> fis = impl_->zip->file_info_list();
    ret.reserve(fis.size());
    for (const ZipContainer::FileInfo &qfi : fis) {
        FileInfo fi;
        fi.file_path = qfi.file_path;
        fi.is_dir = qfi.is_dir;
        fi.is_file = qfi.is_file;
        fi.is_sym_link = qfi.is_sym_link;
        fi.size = qfi.size;

        ret.push_back(std::move(fi));
    }

    return ret;
}

bool ZipReader::file_exists(const std::string &file_name) const { return impl_->zip->file_exists(file_name); }

ByteArray ZipReader::file_data(const std::string &file_name) const { return impl_->zip->file_data(file_name); }

// ===========================
// ZipUnpack
// ===========================

Ret ZipUnpack::unpack(const io::PathT &zip_path, const io::PathT &dir_path) {
    Ret ret = io::Dir::mkpath(dir_path);
    if (!ret) {
        return ret;
    }

    ZipReader zip(zip_path);
    for (const ZipReader::FileInfo &fi : zip.file_info_list()) {
        const io::PathT file_path = dir_path + "/" + fi.file_path;

        if (fi.is_dir) {
            ret = io::Dir::mkpath(file_path);
        } else if (fi.is_file) {
            ret = io::Dir::mkpath(io::dirpath(file_path));

            if (ret) {
                const ByteArray data = zip.file_data(fi.file_path.to_std_string());
                ret = io::File::write_file(file_path, data);
            }
        }

        if (!ret) {
            break;
        }
    }

    return ret;
}
