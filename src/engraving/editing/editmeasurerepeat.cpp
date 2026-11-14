/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "editmeasurerepeat.h"

#include <vector>

#include "../dom/barline.h"
#include "../dom/factory.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/staff.h"
#include "../dom/utils.h"
#include "../infrastructure/messagebox.h"
#include "translation.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   addMeasureRepeat
//---------------------------------------------------------

//---------------------------------------------------------
//   makeMeasureRepeatGroup
///   clear measures, apply noBreak, set measureRepeatCount
///   returns false if these measures won't work or user aborted
//---------------------------------------------------------

//---------------------------------------------------------
//   addMeasureRepeat
//    create one MeasureRepeat at tick of subtype numMeasures
//    create segment if necessary
//    does NOT set measureRepeatCount or do anything else with measure(s)!
//---------------------------------------------------------

MeasureRepeat *EditMeasureRepeat::add_measure_repeat(Transaction &, Score *score, const Fraction &tick, track_idx_t track,
                                                     int num_measures) {
    Measure *measure = score->tick2measure(tick);
    MeasureRepeat *mr = Factory::create_measure_repeat(score->dummy());
    mr->set_num_measures(num_measures);
    mr->set_ticks(measure->stretched_len(score->staff(track2staff(track))));
    mr->set_track(track);
    score->undo_add_cr(mr, measure, tick);
    return mr;
}
