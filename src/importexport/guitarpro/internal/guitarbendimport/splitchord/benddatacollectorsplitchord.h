/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include <engraving/types/pitchvalue.h>
#include <engraving/types/types.h>

#include "benddatacontextsplitchord.h"

namespace mu::engraving {
class Note;
class Chord;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
class BendDataCollectorSplitChord {
public:

    void store_bend_data(const mu::engraving::Note *note, const mu::engraving::PitchValues &pitch_values);
    BendDataContextSplitChord collect_bend_data_context();

private:
    std::unordered_map<mu::engraving::track_idx_t,
                       std::map<mu::engraving::Fraction, std::unordered_map<const mu::engraving::Note *, ImportedBendInfo>>>
        bend_info_for_note_;

    void fill_bends_durations(BendDataContextSplitChord &bend_data_ctx);
    void fill_bend_data(BendDataContextSplitChord &bend_data_ctx);
};
} // namespace mu::iex::guitarpro
