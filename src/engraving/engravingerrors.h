/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#ifndef MU_ENGRAVING_ENGRAVINGERRORS_H
#define MU_ENGRAVING_ENGRAVINGERRORS_H

#include "io/path.h"
#include "types/ret.h"

#include "translation.h"

namespace mu::engraving {
enum class Err {
    UNDEFINED = -1,
    NO_ERROR = 0,
    UNKNOWN_ERROR = 2000,

    // file
    FILE_UNKNOWN_ERROR = 2001,
    FILE_NOT_FOUND = 2002,
    FILE_OPEN_ERROR = 2003,
    FILE_BAD_FORMAT = 2004,
    FILE_UNKNOWN_TYPE = 2005,
    FILE_TOO_OLD = 2006,
    FILE_TOO_NEW = 2007,
    FILE_OLD300_FORMAT = 2008,
    FILE_CORRUPTED = 2009,
    FILE_CRITICALLY_CORRUPTED = 2010,

    USER_ABORT = 2011,
    IGNORE_ERROR = 2012
};

inline muse::Ret make_ret(Err err, const muse::String &text) { return muse::Ret(static_cast<int>(err), text.to_std_string()); }

inline muse::Ret make_ret(Err err, const muse::io::PathT &file_path = {}) {
    muse::String text;

    switch (err) {
        case Err::NO_ERROR: return muse::make_ok();
        case Err::FILE_UNKNOWN_ERROR: text = muse::mtrc("engraving", "Unknown error"); break;
        case Err::FILE_NOT_FOUND: text = muse::mtrc("engraving", "File “%1” not found").arg(file_path.to_string()); break;
        case Err::FILE_OPEN_ERROR: text = muse::mtrc("engraving", "File open error"); break;
        case Err::FILE_BAD_FORMAT: text = muse::mtrc("engraving", "Bad format"); break;
        case Err::FILE_UNKNOWN_TYPE: text = muse::mtrc("engraving", "Unknown filetype"); break;
        case Err::FILE_TOO_OLD: text = muse::mtrc("engraving", "Scores from before MuseScore 4.6 are not supported"); break;
        case Err::FILE_TOO_NEW:
            text = muse::mtrc("engraving", "This file was saved using a newer version of MuseScore Studio. "
                                           "Please visit <a href=\"%1\">MuseScore.org</a> to obtain the latest version.")
                       .arg(u"https://musescore.org");
            break;
        case Err::FILE_OLD300_FORMAT: text = muse::mtrc("engraving", "This file was last saved in a development version of 3.0."); break;
        case Err::FILE_CORRUPTED: text = muse::mtrc("engraving", "File “%1” is corrupted.").arg(file_path.to_string()); break;
        case Err::FILE_CRITICALLY_CORRUPTED:
            text = muse::mtrc("engraving", "File “%1” is critically corrupted and cannot be processed.").arg(file_path.to_string());
            break;
        case Err::UNDEFINED:
        case Err::UNKNOWN_ERROR:
        case Err::IGNORE_ERROR:
        case Err::USER_ABORT: break;
    }

    return muse::Ret(static_cast<int>(err), text.to_std_string());
}
} // namespace mu::engraving

#endif // MU_ENGRAVING_ENGRAVINGERRORS_H
