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

#include <set>

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
struct Slur;
struct Staff;

// no ordinal for the visual repres. of string
// (topmost in TAB varies according to visual order and presence of bass strings)
static constexpr int visual_invalid_string_index = -100;

enum class NoteEntryMethod : char { UNKNOWN, BY_NOTE_NAME, BY_DURATION, REPITCH, RHYTHM, REALTIME_AUTO, REALTIME_MANUAL, TIMEWISE };

struct InputState {

    Fraction tick() const;

    Segment *segment() const { return segment_; }

    void set_segment(Segment *s);

    void set_last_segment(Segment *s) { last_segment_ = s; }

    track_idx_t track() const { return track_; }

    void set_track(track_idx_t v) {
        prev_track_ = track_;
        track_ = v;
    }

    bool note_entry_mode() const { return note_entry_mode_; }

    NoteEntryMethod note_entry_method() const { return note_entry_method_; }

    Slur *slur() const { return slur_; }

    void move_to_next_input_pos();


private:

    Segment *next_input_pos() const;

    track_idx_t track_ = 0;
    track_idx_t prev_track_ = 0; // used for navigation

    Segment *last_segment_ = nullptr;
    Segment *segment_ = nullptr; // current segment

    bool note_entry_mode_ = false;
    NoteEntryMethod note_entry_method_ = NoteEntryMethod::BY_NOTE_NAME;

    Slur *slur_ = nullptr;

    bool beyond_score_ = false;
};
} // namespace mu::engraving
#endif
