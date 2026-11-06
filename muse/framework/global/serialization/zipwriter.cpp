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
#include "zipwriter.h"

#include "global/io/iodevice.h"
#include "global/types/bytearray.h"

#include "internal/zipcontainer.h"

using namespace muse;

struct ZipWriter::Impl {
    ZipContainer *zip = nullptr;
    bool is_closed = false;
};

ZipWriter::ZipWriter(io::IODevice *device) {
    device_ = device;
    impl_ = new Impl();
    impl_->zip = new ZipContainer(device_);
}

ZipWriter::~ZipWriter() {
    close();
    delete impl_->zip;
    delete impl_;
}

void ZipWriter::flush() {}

void ZipWriter::close() {
    if (impl_->is_closed) {
        return;
    }

    impl_->zip->close();
    if (device_) {
        flush();
        device_->close();
    }

    impl_->is_closed = true;
}

bool ZipWriter::has_error() const { return impl_->zip->status() != ZipContainer::NO_ERROR; }

void ZipWriter::add_file(const std::string &file_name, const ByteArray &data) {
    impl_->zip->add_file(file_name, data);
    flush();
}
