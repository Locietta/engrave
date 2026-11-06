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
#ifndef MU_ENGRAVING_MSCIO_H
#define MU_ENGRAVING_MSCIO_H

#include <string>

#include "io/path.h"

namespace mu::engraving {
//! NOTE The main format is MuseScore, is a zip archive with a specific structure
static const std::string mscz = "mscz";

//! NOTE This is the automatically generated backup file.
static const std::string mscz_backup = "mscz~";

//! NOTE Before MuseScore 4, MuseScore could save data in one xml file (excluding binary) with `.mscx` extension.
//! Starting from MuseScore 4, only the score domain model store to the `.mscx` file,
//! and other data, such as styles, chordlist, synthesizer settings, etc. are stored in separate files.
//! We can choose to save a `.mscx` file, in this case a folder with the same structure
//! as a zip container will be created (i.e. an unpacked zip container)
//! We can select `.mscx` to open, in this case it is now expected that it is in a folder
//! in which there are also other data, structured as in a zip container.
static const std::string mscx = "mscx";

//! NOTE For testing purposes and some workflows of work with score, there is a need to store all data in one text file.
//! Therefore, a new file type has been introduced for this.
//! This is a XML file that stores all files from the ZIP container by concatenation (excluding binaries)
static const std::string mscs = "mscs";

inline bool is_muse_score_file(const std::string &suffix) {
    return suffix == ::mu::engraving::mscz || suffix == ::mu::engraving::mscx || suffix == ::mu::engraving::mscs ||
           suffix == ::mu::engraving::mscz_backup;
}

enum class MscIoMode : unsigned char { UNKNOWN = 0, ZIP, DIR, XML_FILE };

inline MscIoMode msc_io_mode_by_suffix(const std::string &suffix) {
    if (suffix == ::mu::engraving::mscz || suffix == ::mu::engraving::mscz_backup) {
        return MscIoMode::ZIP;
    } else if (suffix == ::mu::engraving::mscx) {
        return MscIoMode::DIR;
    } else if (suffix == ::mu::engraving::mscs) {
        return MscIoMode::XML_FILE;
    }
    return MscIoMode::UNKNOWN;
}

inline muse::io::PathT container_path(const muse::io::PathT &path) {
    if (muse::io::suffix(path) == ::mu::engraving::mscx) {
        return muse::io::absolute_dirpath(path);
    }

    return path;
}

inline muse::io::PathT main_file_path(const muse::io::PathT &path) {
    if (is_muse_score_file(muse::io::suffix(path))) {
        return path;
    }

    return path.appending_component(muse::io::filename(path)).appending_suffix(::mu::engraving::mscx);
}

inline muse::io::PathT main_file_name(const muse::io::PathT &path) {
    return muse::io::filename(path, !is_muse_score_file(muse::io::suffix(path))).appending_suffix(::mu::engraving::mscx);
}
} // namespace mu::engraving

#endif // MU_ENGRAVING_MSCIO_H
