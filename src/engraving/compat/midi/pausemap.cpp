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

/**
 \file
 Implementation of class PauseMap.
*/

#include "pausemap.h"

#include "dom/repeatlist.h"
#include "dom/score.h"
#include "dom/sig.h"
#include "dom/tempotimeline.h"
#include "log.h"

using namespace mu;
using namespace muse;

namespace mu::engraving {
//---------------------------------------------------------
//   calculate
//    MIDI files cannot contain pauses so insert extra ticks and tempo changes instead.
//    The PauseMap and tempo events are fully unwound to account for pauses on repeats.
//---------------------------------------------------------

void PauseMap::calculate(const Score *s) {
    IF_ASSERT_FAILED(s) { LOGE() << "failed to calculate pause map"; }

    insert(std::pair<const int, int>(0, 0)); // can't start with a pause

    const TimeSigMap *sigmap = s->sigmap();
    const RepeatList &repeat_list = s->repeat_list();

    for (const TempoTimePoint &point : s->tempo_timeline().points()) {
        const int utick = point.utick;

        if (point.pause <= 0.0) {
            // We have a regular tempo change.
            tempo_events_[tick_with_pauses(utick)] = point.bps;
            continue;
        }

        // We have a pause event.
        const int tick = repeat_list.utick2tick(utick);
        Fraction time_sig(sigmap->timesig(tick).timesig());
        double quarter_notes_per_measure = (4.0 * time_sig.numerator()) / time_sig.denominator();
        int ticks_per_measure =
            quarter_notes_per_measure * Constants::division; // store a full measure of ticks to keep barlines in same places
        tempo_events_[tick_with_pauses(utick)] = quarter_notes_per_measure / point.pause;      // new tempo for pause
        insert(std::pair<const int, int>(utick, ticks_per_measure + offset_at_u_tick(utick))); // store running total of extra ticks
        tempo_events_[tick_with_pauses(utick)] = point.bps;                                    // restore previous tempo
    }
}

//---------------------------------------------------------
//   offsetAtUTick
//    In total, how many extra ticks have been inserted prior to this utick.
//---------------------------------------------------------

int PauseMap::offset_at_u_tick(int utick) const {
    // make sure calculate was called
    IF_ASSERT_FAILED(!empty()) { LOGE() << "accessing empty container"; }

    auto i = upper_bound(utick);
    if (i != begin()) {
        --i;
    }
    return i->second;
}
} // namespace mu::engraving
