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
#ifndef MUSE_IO_FILEINFO_H
#define MUSE_IO_FILEINFO_H

#include "dir.h"
#include "global/types/string.h"
#include "ifilesystem.h"

namespace muse::io {
struct FileInfo {
    FileInfo() = default;
    FileInfo(const PathT &file_path);

    String path() const;
    String file_path() const;
    String canonical_file_path() const;
    String absolute_path() const;
    String absolute_file_path() const;

    String file_name() const;
    String base_name() const;
    String complete_base_name() const;
    String suffix() const;
    static String suffix(const PathT &file_path);

    EntryType entry_type() const;

    bool is_relative() const;
    bool is_absolute() const;

    bool is_sym_link() const;
    PathT sym_link_target() const;

    bool exists() const;
    static bool exists(const PathT &file_path);

    DateTime birth_time() const;
    DateTime last_modified() const;

    PathT dir_path() const;
    Dir dir() const;

private:
    static String do_suffix(const String &file_path);

    String file_path_;
};
} // namespace muse::io

#endif // MUSE_IO_FILEINFO_H
