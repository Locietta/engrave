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

#include "inserttime.h"

#include <set>

#include "../dom/repeatlist.h"
#include "../dom/score.h"
#include "../dom/spanner.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   InsertTime
//---------------------------------------------------------

void InsertTime::redo() { score->insert_time(tick, len, score->repeat_segment_info_list(/*expandRepeats*/ true)); }

void InsertTime::undo() { score->insert_time(tick, -len, score->repeat_segment_info_list(/*expandRepeats*/ true)); }

//---------------------------------------------------------
//   InsertTimeUnmanagedSpanner
//---------------------------------------------------------

void InsertTimeUnmanagedSpanner::flip() {
    for (Score *s : score->score_list()) {
        std::set<Spanner *> spanners_copy = s->unmanaged_spanners();
        for (Spanner *sp : spanners_copy) {
            sp->insert_time_unmanaged(tick, len);
        }
    }
    len = -len;
}
