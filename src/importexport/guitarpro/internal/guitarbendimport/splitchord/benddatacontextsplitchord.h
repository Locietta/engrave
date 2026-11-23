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

#include "../guitarbendimporttypes.h"
#include "global/btree.h"
#include <engraving/types/types.h>

namespace mu::iex::guitarpro {
struct BendDataContextSplitChord {
    muse::BTreeMap<mu::engraving::track_idx_t, muse::BTreeMap<int, BendChordData>> bend_data_by_end_tick;
    muse::BTreeMap<mu::engraving::track_idx_t, muse::BTreeMap<int, std::vector<mu::engraving::Fraction>>> bend_chord_durations;
    muse::BTreeMap<mu::engraving::track_idx_t, muse::BTreeSet<mu::engraving::Fraction>> reduntant_chord_ticks;
    muse::BTreeMap<mu::engraving::track_idx_t, muse::BTreeSet<mu::engraving::Fraction>> chord_ticks_for_tie_back;
};
} // namespace mu::iex::guitarpro
