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
#ifndef MUSE_IO_IORETCODES_H
#define MUSE_IO_IORETCODES_H

#include "global/translation.h"
#include "global/types/ret.h"

namespace muse::io {
enum class Err {
    UNDEFINED = int(Ret::Code::UNDEFINED),
    NO_ERROR = int(Ret::Code::OK),
    UNKNOWN_ERROR = int(Ret::Code::SYSTEM_FIRST),

    FS_NOT_EXIST,
    FS_ALREADY_EXISTS,
    FS_REMOVE_ERROR,
    FS_DIR_NOT_EMPTY_ERROR,
    FS_CLEAR_ERROR,
    FS_READ_ERROR,
    FS_WRITE_ERROR,
    FS_MAKING_ERROR,
    FS_COPY_ERROR,
    FS_MOVE_ERRORS
};

inline Ret make_ret(Err e) {
    int ret_code = static_cast<int>(e);

    switch (e) {
        case Err::UNDEFINED: return Ret(ret_code);
        case Err::NO_ERROR: return Ret(ret_code);
        case Err::UNKNOWN_ERROR: return Ret(ret_code);
        case Err::FS_NOT_EXIST: return Ret(ret_code, muse::trc("system", "The file does not exist"));
        case Err::FS_ALREADY_EXISTS: return Ret(ret_code, muse::trc("system", "The file already exists"));
        case Err::FS_REMOVE_ERROR: return Ret(ret_code, muse::trc("system", "The file could not be removed"));
        case Err::FS_DIR_NOT_EMPTY_ERROR: return Ret(ret_code, muse::trc("system", "The directory is not empty"));
        case Err::FS_CLEAR_ERROR: return Ret(ret_code, muse::trc("system", "The directory could not be cleared"));
        case Err::FS_READ_ERROR: return Ret(ret_code, muse::trc("system", "An error occurred when reading from the file"));
        case Err::FS_WRITE_ERROR: return Ret(ret_code, muse::trc("system", "An error occurred when writing to the file"));
        case Err::FS_MAKING_ERROR: return Ret(ret_code, muse::trc("system", "An error occurred when making a path"));
        case Err::FS_COPY_ERROR: return Ret(ret_code, muse::trc("system", "An error occurred when copying the file"));
        case Err::FS_MOVE_ERRORS: return Ret(ret_code, muse::trc("system", "An error occurred when moving the file"));
    }

    return Ret(static_cast<int>(e));
}
} // namespace muse::io

#endif // MUSE_IO_IORETCODES_H
