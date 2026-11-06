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
#include "mscxcompat.h"

#include "io/file.h"
#include "io/buffer.h"

#include "engraving/engravingerrors.h"
#include "../rw/mscloader.h"

#include "infrastructure/localfileinfoprovider.h"

#include "log.h"

using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

Ret mu::engraving::compat::load_mscz_or_mscx(MasterScore *score, const io::PathT &path, bool ignore_version_error) {
    std::string suffix = io::suffix(path);

    MscReader::Params params;
    params.file_path = path;
    params.mode = msc_io_mode_by_suffix(suffix);
    IF_ASSERT_FAILED(params.mode != MscIoMode::UNKNOWN) { return make_ret(Err::FILE_UNKNOWN_TYPE, path); }

    MscReader reader(params);
    Ret ret = reader.open();
    if (!ret) {
        return ret;
    }

    score->set_file_info_provider(std::make_shared<LocalFileInfoProvider>(path));

    MscLoader score_reader;
    return score_reader.load_mscz(score, reader, nullptr, ignore_version_error);
}

Ret mu::engraving::compat::load_mscz_or_mscx(EngravingProjectPtr project, const io::PathT &path, bool ignore_version_error) {
    std::string suffix = io::suffix(path);

    MscReader::Params params;
    params.file_path = path;
    params.mode = msc_io_mode_by_suffix(suffix);
    IF_ASSERT_FAILED(params.mode != MscIoMode::UNKNOWN) { return make_ret(Err::FILE_UNKNOWN_TYPE, path); }

    MscReader reader(params);
    Ret ret = reader.open();
    if (!ret) {
        return ret;
    }

    project->set_file_info_provider(std::make_shared<LocalFileInfoProvider>(path));

    return project->load_mscz(reader, nullptr, ignore_version_error);
}
