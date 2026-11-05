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

#include "global/io/file.h"
#include "global/io/dir.h"
#include "internal/zipcontainer.h"

using namespace muse;
using namespace muse::io;

struct ZipReader::Impl {
    ZipContainer *zip = nullptr;
    IODevice *device = nullptr;
    bool isSelfDevice = false;
};

bool ZipReader::startsWithEntry(const ByteArray &data, std::span<const uint8_t> name) {
    return ZipContainer::startsWithEntry(data.constData(), data.size(), name);
}

ZipReader::ZipReader(const io::path_t &filePath) : filePath_(filePath) {
    impl_ = new Impl();
    impl_->device = new File(filePath);
    impl_->isSelfDevice = true;
    if (impl_->device->open(IODevice::ReadOnly)) {}
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
    if (impl_->isSelfDevice) {
        delete impl_->device;
    }
    delete impl_;
}

bool ZipReader::exists() const {
    if (!filePath_.empty()) {
        return File::exists(filePath_);
    }

    return true;
}

void ZipReader::close() { impl_->zip->close(); }

bool ZipReader::hasError() const { return impl_->zip->status() != ZipContainer::NoError; }

std::vector<ZipReader::FileInfo> ZipReader::fileInfoList() const {
    std::vector<FileInfo> ret;
    std::vector<ZipContainer::FileInfo> fis = impl_->zip->fileInfoList();
    ret.reserve(fis.size());
    for (const ZipContainer::FileInfo &qfi : fis) {
        FileInfo fi;
        fi.filePath = qfi.filePath;
        fi.isDir = qfi.isDir;
        fi.isFile = qfi.isFile;
        fi.isSymLink = qfi.isSymLink;
        fi.size = qfi.size;

        ret.push_back(std::move(fi));
    }

    return ret;
}

bool ZipReader::fileExists(const std::string &fileName) const { return impl_->zip->fileExists(fileName); }

ByteArray ZipReader::fileData(const std::string &fileName) const { return impl_->zip->fileData(fileName); }

// ===========================
// ZipUnpack
// ===========================

Ret ZipUnpack::unpack(const io::path_t &zipPath, const io::path_t &dirPath) {
    Ret ret = io::Dir::mkpath(dirPath);
    if (!ret) {
        return ret;
    }

    ZipReader zip(zipPath);
    for (const ZipReader::FileInfo &fi : zip.fileInfoList()) {
        const io::path_t filePath = dirPath + "/" + fi.filePath;

        if (fi.isDir) {
            ret = io::Dir::mkpath(filePath);
        } else if (fi.isFile) {
            ret = io::Dir::mkpath(io::dirpath(filePath));

            if (ret) {
                const ByteArray data = zip.fileData(fi.filePath.toStdString());
                ret = io::File::writeFile(filePath, data);
            }
        }

        if (!ret) {
            break;
        }
    }

    return ret;
}
