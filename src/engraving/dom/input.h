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

#ifndef MU_ENGRAVING_INPUT_H
#define MU_ENGRAVING_INPUT_H

#include "../types/types.h"
#include "accidental.h"
#include "chord.h"
#include "durationtype.h"
#include "mscore.h"
#include "noteval.h"

namespace mu::engraving {
struct ChordRest;
struct Drumset;
struct EngravingItem;
struct Note;
struct Score;
struct Segment;
struct Selection;
struct Staff;

// no ordinal for the visual repres. of string
// (topmost in TAB varies according to visual order and presence of bass strings)
static constexpr int visual_invalid_string_index = -100;

struct InputState {

    Segment *segment() const { return segment_; }

    void set_segment(Segment *s);

    track_idx_t track() const { return track_; }

    void set_track(track_idx_t track) { track_ = track; }

    void move_to_next_input_pos();


private:

    Segment *next_input_pos() const;

    track_idx_t track_ = 0;
    Segment *segment_ = nullptr;
};
} // namespace mu::engraving
#endif
