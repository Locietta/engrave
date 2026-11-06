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
#ifndef MUSE_IO_IOENUMS_H
#define MUSE_IO_IOENUMS_H

#include <cstdint>

namespace muse::io {
using StreamId = int;
static constexpr StreamId invalid_stream_id = -1;
static constexpr uint64_t stream_pos_current = ~uint64_t(0);

enum class OpenMode { WRITE_ONLY, APPEND };

enum class ScanMode { FILES_IN_CURRENT_DIR, FOLDERS_IN_CURRENT_DIR, FILES_AND_FOLDERS_IN_CURRENT_DIR, FILES_IN_CURRENT_DIR_AND_SUBDIRS };

enum class EntryType { UNDEFINED = 0, FILE, DIR };
} // namespace muse::io

#endif // MUSE_IO_IOENUMS_H
