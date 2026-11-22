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

#include "input.h"

#include "measure.h"
#include "segment.h"

using namespace mu;

namespace mu::engraving {

void InputState::set_segment(Segment *s) {
    if (s && s->measure()->is_mm_rest()) {
        Measure *m = s->measure()->mm_rest_first();
        s = m->find_segment(SegmentType::CHORD_REST, m->tick());
    }
    segment_ = s;
}

Segment *InputState::next_input_pos() const {
    Measure *m = segment_->measure();
    Segment *s = segment_->next1(SegmentType::CHORD_REST);
    while (s) {
        if (s->element(track_) || s->measure() != m) {
            return s;
        }
        s = s->next1(SegmentType::CHORD_REST);
    }
    return 0;
}

//   TODO: special case: note is first note of tie: goto to last note of tie

void InputState::move_to_next_input_pos() {
    if (auto *next = next_input_pos()) segment_ = next;
}
} // namespace mu::engraving
