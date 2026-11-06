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

#include "swing.h"

#include "realfn.h"

#include "../editing/navigation.h"

#include "chord.h"
#include "measure.h"
#include "staff.h"

using namespace mu::engraving;

bool Swing::ChordDurationAdjustment::is_null() const {
    return muse::real_is_null(remaining_duration_multiplier) && muse::real_is_null(duration_multiplier);
}

//---------------------------------------------------------
//   isSubdivided
//   Check for subdivided beat
//---------------------------------------------------------

static bool is_subdivided(const ChordRest *chord, int swing_unit) {
    if (!chord) {
        return false;
    }

    const ChordRest *prev = Navigation::prev_chord_rest(chord);
    if (chord->actual_ticks().ticks() < swing_unit || (prev && prev->actual_ticks().ticks() < swing_unit)) {
        return true;
    }

    return false;
}

void Swing::swing_adjust_params(const Chord *chord, const SwingParameters &params, int &on_time, int &gate_time) {
    Fraction tick = chord->rtick() + chord->measure()->anacrusis_offset();

    int swing_beat = params.swing_unit * 2;
    double ticks_duration = (double) chord->actual_ticks().ticks();
    double swing_tick_adjust = ((double) swing_beat) * (((double) (params.swing_ratio - 50)) / 100.0);
    double swing_actual_adjust = (swing_tick_adjust / ticks_duration) * 1000.0;
    int start_tick = tick.ticks();

    //Check the position of the chord to apply changes accordingly
    if (start_tick % swing_beat == params.swing_unit) {
        if (!is_subdivided(chord, params.swing_unit)) {
            on_time = on_time + swing_actual_adjust;
            gate_time = (200 - (gate_time + (swing_actual_adjust / 10)));
        }
    }

    int end_tick = start_tick + ticks_duration;
    if (end_tick % swing_beat == params.swing_unit) {
        const ChordRest *ncr = Navigation::next_chord_rest(chord);

        if (!is_subdivided(ncr, params.swing_unit)) {
            gate_time = gate_time + (swing_actual_adjust / 10);
        }
    }
}

Swing::ChordDurationAdjustment Swing::apply_swing(const Chord *chord, const SwingParameters &params) {
    int on_time = 0;
    int gate_time = 100;

    swing_adjust_params(chord, params, on_time, gate_time);

    if (gate_time <= 0) {
        gate_time = 100;
    }

    ChordDurationAdjustment result;
    result.remaining_duration_multiplier = static_cast<double>(on_time) / 1000.;
    result.duration_multiplier = static_cast<double>(gate_time) / 100.;

    return result;
}
