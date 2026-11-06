/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include "filestream.h"

#include "ioretcodes.h"
#include "log.h"

using namespace muse;
using namespace muse::io;

FileStream::FileStream(const PathT &file_path) : file_path_(file_path) {}

FileStream::~FileStream() { do_close(); }

void FileStream::do_close() {
    if (stream_id_ != ::muse::io::invalid_stream_id) {
        file_system()->close_stream(stream_id_);
        stream_id_ = ::muse::io::invalid_stream_id;
    }
}

const PathT &FileStream::file_path() const { return file_path_; }

bool FileStream::do_open(OpenMode m) {
    if (stream_id_ != ::muse::io::invalid_stream_id) {
        file_system()->close_stream(stream_id_);
        stream_id_ = ::muse::io::invalid_stream_id;
    }

    io::OpenMode fs_mode;
    switch (m) {
        case OpenMode::WRITE_ONLY:
            fs_mode = io::OpenMode::WRITE_ONLY;
            size_ = 0;
            break;
        case OpenMode::APPEND: {
            fs_mode = io::OpenMode::APPEND;
            RetVal<uint64_t> sz = file_system()->file_size(file_path_);
            size_ = sz.ret ? static_cast<size_t>(sz.val) : 0;
            break;
        }
        default: set_error(int(Err::FS_WRITE_ERROR), "FileStream supports only WriteOnly and Append modes"); return false;
    }

    RetVal<StreamId> rv = file_system()->open_stream(file_path_, fs_mode);
    if (!rv.ret) {
        set_error(rv.ret.code(), rv.ret.text());
        return false;
    }

    stream_id_ = rv.val;
    expected_pos_ = size_;
    return true;
}

size_t FileStream::data_size() const { return size_; }

const uint8_t *FileStream::raw_data() const { return nullptr; }

bool FileStream::resize_data(size_t size) {
    size_ = size;
    return true;
}

size_t FileStream::write_data(const uint8_t *data, size_t len) {
    IF_ASSERT_FAILED(stream_id_ != ::muse::io::invalid_stream_id) { return 0; }

    uint64_t offset = (pos() == expected_pos_) ? ::muse::io::stream_pos_current : static_cast<uint64_t>(pos());

    Ret ret = file_system()->write_to_stream(stream_id_, ByteArray::from_raw_data(data, len), offset);
    if (!ret) {
        set_error(ret.code(), ret.text());
        return 0;
    }

    expected_pos_ = pos() + len;
    return len;
}
