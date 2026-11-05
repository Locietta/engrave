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

FileStream::FileStream(const path_t &filePath) : filePath_(filePath) {}

FileStream::~FileStream() { doClose(); }

void FileStream::doClose() {
    if (streamId_ != INVALID_STREAM_ID) {
        fileSystem()->closeStream(streamId_);
        streamId_ = INVALID_STREAM_ID;
    }
}

const path_t &FileStream::filePath() const { return filePath_; }

bool FileStream::doOpen(OpenMode m) {
    if (streamId_ != INVALID_STREAM_ID) {
        fileSystem()->closeStream(streamId_);
        streamId_ = INVALID_STREAM_ID;
    }

    io::OpenMode fsMode;
    switch (m) {
        case OpenMode::WriteOnly:
            fsMode = io::OpenMode::WriteOnly;
            size_ = 0;
            break;
        case OpenMode::Append: {
            fsMode = io::OpenMode::Append;
            RetVal<uint64_t> sz = fileSystem()->fileSize(filePath_);
            size_ = sz.ret ? static_cast<size_t>(sz.val) : 0;
            break;
        }
        default: setError(int(Err::FSWriteError), "FileStream supports only WriteOnly and Append modes"); return false;
    }

    RetVal<StreamId> rv = fileSystem()->openStream(filePath_, fsMode);
    if (!rv.ret) {
        setError(rv.ret.code(), rv.ret.text());
        return false;
    }

    streamId_ = rv.val;
    expectedPos_ = size_;
    return true;
}

size_t FileStream::dataSize() const { return size_; }

const uint8_t *FileStream::rawData() const { return nullptr; }

bool FileStream::resizeData(size_t size) {
    size_ = size;
    return true;
}

size_t FileStream::writeData(const uint8_t *data, size_t len) {
    IF_ASSERT_FAILED(streamId_ != INVALID_STREAM_ID) { return 0; }

    uint64_t offset = (pos() == expectedPos_) ? STREAM_POS_CURRENT : static_cast<uint64_t>(pos());

    Ret ret = fileSystem()->writeToStream(streamId_, ByteArray::fromRawData(data, len), offset);
    if (!ret) {
        setError(ret.code(), ret.text());
        return 0;
    }

    expectedPos_ = pos() + len;
    return len;
}
