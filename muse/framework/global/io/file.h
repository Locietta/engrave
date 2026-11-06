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
#pragma once

#include "ifilesystem.h"
#include "iodevice.h"
#include "path.h"

namespace muse::io {
struct File : IODevice {

    File() = default;
    File(const PathT &file_path);
    ~File() override;

    const PathT &file_path() const;

    bool exists() const;
    bool remove();

    static bool exists(const PathT &file_path);
    static bool remove(const PathT &file_path);
    static bool copy(const PathT &src, const PathT &dst, bool replace = false);
    static Ret read_file(const io::PathT &file_path, ByteArray &out);
    static Ret write_file(const io::PathT &file_path, const ByteArray &data);
    static bool set_permissions_allowed_for_all(const PathT &file_path);

protected:

    bool do_open(OpenMode m) override;
    size_t data_size() const override;
    const uint8_t *raw_data() const override;
    bool resize_data(size_t size) override;
    size_t write_data(const uint8_t *data, size_t len) override;

private:

    PathT file_path_;
    ByteArray data_;
};
} // namespace muse::io
