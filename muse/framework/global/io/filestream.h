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
#pragma once

#include "ifilesystem.h"

#include "iodevice.h"
#include "path.h"

namespace muse::io {
class FileStream : public IODevice {
public:
    FileStream() = default;
    explicit FileStream(const PathT &file_path);
    ~FileStream() override;

    FileStream(const FileStream &) = delete;
    FileStream &operator=(const FileStream &) = delete;

    const PathT &file_path() const;

protected:
    bool do_open(OpenMode m) override;
    void do_close() override;
    size_t data_size() const override;
    const uint8_t *raw_data() const override;
    bool resize_data(size_t size) override;
    size_t write_data(const uint8_t *data, size_t len) override;

private:
    PathT file_path_;
    size_t size_ = 0;

    StreamId stream_id_ = ::muse::io::invalid_stream_id;
    size_t expected_pos_ = 0;
};
} // namespace muse::io
