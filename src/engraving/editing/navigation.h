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

#pragma once

#include "../types/types.h"

namespace mu::engraving {
class ChordRest;
class EngravingItem;
class Lyrics;
class Note;
class Score;
class Segment;

struct ChordRestNavigateOptions {
    bool skip_grace = false;
    bool skip_measure_repeat_rests = true;
    bool disable_over_repeats = false;
};

class Navigation {
public:
    static ChordRest *next_chord_rest(const ChordRest *cr, const ChordRestNavigateOptions &options = {});
    static ChordRest *prev_chord_rest(const ChordRest *cr, const ChordRestNavigateOptions &options = {});
    static Lyrics *last_lyrics_in_measure(const Segment *seg, const staff_idx_t staff_idx, const int no, const PlacementV &placement);
    static Lyrics *prev_lyrics(const Lyrics *lyrics);
    static Lyrics *next_lyrics(const Lyrics *lyrics);

    static EngravingItem *chord_note_above(Score *score, EngravingItem *element);
    static EngravingItem *chord_note_below(Score *score, EngravingItem *element);
    static Note *top_note_in_chord(Note *note);
    static Note *bottom_note_in_chord(Note *note);
    static EngravingItem *first_element(Score *score, bool frame = true);
    static EngravingItem *last_element(Score *score, bool frame = true);
    static ChordRest *next_measure(Score *score, ChordRest *element, bool select_behavior = false, bool mm_rest = false);
    static ChordRest *prev_measure(Score *score, ChordRest *element, bool mm_rest = false);
    static ChordRest *up_staff(ChordRest *cr);
    static ChordRest *down_staff(Score *score, ChordRest *cr);
    static ChordRest *top_staff(Score *score, ChordRest *cr = nullptr);
};
} // namespace mu::engraving
