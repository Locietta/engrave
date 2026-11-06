/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "guitarbendimporttypes.h"
#include <engraving/types/pitchvalue.h>

namespace mu::engraving {
struct Note;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
int gp_pitch_to_quarter_tones(int gp_pitch);

struct DiveInfoConverter {
    static ImportedDiveInfo fill_dive_info(const mu::engraving::Note *note, const mu::engraving::PitchValues &pitch_values,
                                           bool is_continued_whammy = false);

    static ImportedDiveInfo from_pitch_values(const mu::engraving::PitchValues &pitch_values, bool is_continued_whammy = false);
};
} // namespace mu::iex::guitarpro
