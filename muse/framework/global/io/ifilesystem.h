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
#ifndef MUSE_IO_IFILESYSTEM_H
#define MUSE_IO_IFILESYSTEM_H

#include "global/types/bytearray.h"
#include "global/types/datetime.h"
#include "global/types/retval.h"

#include "path.h"
#include "ioenums.h"

namespace muse::io {
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual Ret exists(const io::PathT &path) const = 0;
    virtual Ret remove(const io::PathT &path, bool only_if_empty = false) = 0; // remove file or dir
    virtual Ret clear(const io::PathT &path) = 0;                              // clear dir
    virtual Ret copy(const io::PathT &src, const io::PathT &dst, bool replace = false) = 0;
    virtual Ret move(const io::PathT &src, const io::PathT &dst, bool replace = false) = 0;

    virtual Ret make_path(const io::PathT &path) const = 0;
    virtual Ret make_link(const io::PathT &target_path, const io::PathT &link_path) const = 0;

    virtual EntryType entry_type(const io::PathT &path) const = 0;

    //! NOTE In bytes
    virtual RetVal<uint64_t> file_size(const io::PathT &path) const = 0;
    virtual RetVal<uint64_t> available_space(const io::PathT &path) const = 0;

    virtual RetVal<io::paths_t> scan_files(const io::PathT &root_dir, const std::vector<std::string> &filters,
                                           ScanMode mode = ScanMode::FILES_IN_CURRENT_DIR_AND_SUBDIRS) const = 0;

    enum class Attribute { HIDDEN };

    virtual void set_attribute(const io::PathT &path, Attribute attribute) const = 0;
    virtual bool set_permissions_allowed_for_all(const io::PathT &path) const = 0;

    virtual RetVal<ByteArray> read_file(const io::PathT &file_path) const = 0;
    virtual Ret read_file(const io::PathT &file_path, ByteArray &data) const = 0;
    virtual Ret write_file(const io::PathT &file_path, const ByteArray &data) = 0;

    //! Streaming write
    virtual RetVal<StreamId> open_stream(const io::PathT &file_path, OpenMode mode) = 0;
    virtual Ret write_to_stream(StreamId file_id, const ByteArray &data, uint64_t offset = ::muse::io::stream_pos_current) = 0;
    virtual Ret close_stream(StreamId file_id) = 0;

    //! NOTE File info
    virtual io::PathT temporary_directory_path() const = 0;
    virtual io::PathT canonical_file_path(const io::PathT &file_path) const = 0;
    virtual io::PathT absolute_path(const io::PathT &file_path) const = 0;
    virtual io::PathT absolute_file_path(const io::PathT &file_path) const = 0;
    virtual io::PathT clean_path(const io::PathT &file_path) const = 0;
    virtual bool is_sym_link(const io::PathT &file_path) const = 0;
    virtual io::PathT sym_link_target(const io::PathT &file_path) const = 0;
    virtual DateTime birth_time(const io::PathT &file_path) const = 0;
    virtual DateTime last_modified(const io::PathT &file_path) const = 0;
    virtual Ret is_writable(const io::PathT &file_path) const = 0;
};

// The engine's file system behind File, Dir, FileInfo and FileStream. These are
// value types used throughout the engine without an owner to carry a reference,
// so EngineContext binds its file system here for its lifetime; null outside it.
IFileSystem *file_system();
void set_file_system(IFileSystem *file_system);
} // namespace muse::io

#endif // MUSE_IO_IFILESYSTEM_H
