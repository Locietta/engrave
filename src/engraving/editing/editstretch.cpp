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

#include "editstretch.h"

#include "../dom/measure.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"

using namespace mu::engraving;

void EditStretch::add_stretch(Transaction &, Score *score, double val) {
    if (!score->selection().is_range()) {
        return;
    }

    Fraction start_tick = score->selection().tick_start();
    Fraction end_tick = score->selection().tick_end();
    for (Measure *m = score->first_measure_mm(); m; m = m->next_measure_mm()) {
        if (m->tick() < start_tick) {
            continue;
        }
        if (m->tick() >= end_tick) {
            break;
        }
        double stretch = m->user_stretch();
        stretch += val;
        if (stretch < 0) {
            stretch = 0;
        }
        m->undo_change_property(Pid::USER_STRETCH, stretch);
    }
}

void EditStretch::reset_user_stretch(Transaction &, Score *score) {
    Measure *m1 = nullptr;
    Measure *m2 = nullptr;
    // retrieve span of selection
    Segment *s1 = score->selection().start_segment();
    Segment *s2 = score->selection().end_segment();
    // if either segment is not returned by the selection
    // (for instance, no selection) fall back to first/last measure
    if (!s1) {
        m1 = score->first_measure_mm();
    } else {
        m1 = s1->measure();
    }
    if (!s2) {
        m2 = score->last_measure_mm();
    } else {
        m2 = s2->measure();
    }
    if (!m1 || !m2) { // should not happen!
        return;
    }

    for (Measure *m = m1; m; m = m->next_measure_mm()) {
        m->undo_change_property(Pid::USER_STRETCH, 1.0);
        if (m == m2) {
            break;
        }
    }
}
