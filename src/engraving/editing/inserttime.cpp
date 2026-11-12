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

#include <amc/vector.hpp>

#include "../dom/masterscore.h"
#include "../dom/repeatlist.h"
#include "../dom/spanner.h"

void mu::engraving::apply_time_insertion(Score &score, Fraction tick, Fraction length, bool unmanaged) {
    if (unmanaged) {
        for (Score *linked : score.score_list()) {
            const auto &source = linked->unmanaged_spanners();
            const amc::vector<Spanner *> spanners(source.begin(), source.end());
            for (Spanner *spanner : spanners) spanner->insert_time_unmanaged(tick, length);
        }
        return;
    }
    score.master_score()->update_repeat_list(false);
    score.insert_time(tick, length, score.repeat_segment_info_list(/*expandRepeats*/ true));
}
