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

#pragma once

#include "../infrastructure/mscreader.h"
#include "global/types/ret.h"

namespace mu::engraving::compat {
struct ReadStyleHook;
} // namespace mu::engraving::compat

namespace mu::engraving::rw {
struct ReadInOutData;
} // namespace mu::engraving::rw

namespace mu::engraving {
struct MasterScore;
struct XmlReader;

struct MscLoader {
    MscLoader() = default;

    muse::Ret load_mscz(MasterScore *score, const MscReader &msc_reader, rw::ReadInOutData *out, bool ignore_version_error);

private:
    friend struct MasterScore;
    muse::Ret read_master_score(MasterScore *score, XmlReader &, bool ignore_version_error, rw::ReadInOutData *out = nullptr,
                                compat::ReadStyleHook *style_hook = nullptr);
};
} // namespace mu::engraving
