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
#ifndef MUSE_IO_DIR_H
#define MUSE_IO_DIR_H

#include "engrave/src/runtime/resource_filesystem.h"
#include "global/types/retval.h"
#include "ioenums.h"
#include "path.h"

namespace muse::io {
struct Dir {
    Dir() = default;
    Dir(const PathT &path);

    PathT path() const;
    PathT absolute_path() const;

    bool exists() const;
    Ret remove_recursively();

    Ret mkpath();
    static Ret mkpath(const PathT &path);

    static RetVal<io::paths_t> scan_files(const io::PathT &root_dir, const std::vector<std::string> &filters,
                                          ScanMode mode = ScanMode::FILES_IN_CURRENT_DIR_AND_SUBDIRS);

    static PathT from_native_separators(const PathT &path_name);
    static PathT clean_path(const PathT &path);
    static PathT temp_path();

private:
    PathT path_;
};
} // namespace muse::io

#endif // MUSE_IO_DIR_H
