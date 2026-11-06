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
struct Chord;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
struct DiveDataCollector {
    DiveDataCollector() = default;

    void collect_dive_data(const mu::engraving::Chord *chord, const mu::engraving::PitchValues &pitch_values);

    DiveDataContext &context() { return ctx_; }

private:
    struct PrevWhammy {
        int dest_pitch = 0;
        mu::engraving::Fraction end_tick;
    };

    DiveDataContext ctx_;
    std::unordered_map<mu::engraving::track_idx_t, PrevWhammy> last_whammy_by_track_;
};
} // namespace mu::iex::guitarpro
