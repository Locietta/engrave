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

#include <map>

#include "../types/fraction.h"

namespace mu::engraving {
struct Harmony;

//voicing modes to use
enum class Voicing : signed char { INVALID = -1, AUTO = 0, ROOT_ONLY, CLOSE, DROP_2, SIX_NOTE, FOUR_NOTE, THREE_NOTE };

//duration to realize notes for
enum class HDuration : signed char {
    INVALID = -1,
    UNTIL_NEXT_CHORD_SYMBOL = 0, //lasts until the next chord symbol or end of the schore
    STOP_AT_MEASURE_END,         //lasts until next chord symbol or measure end
    SEGMENT_DURATION             //lasts for the duration of the segment
};

//-----------------------------------------
//    Realized Harmony
///     holds information and functions
///     to assist in the realization of chord
///     symbols. This is what is used to
///     allow for chord symbol playback
//-----------------------------------------
struct RealizedHarmony {
    using PitchMap = std::multimap<int, int>; //map from pitch to tpc

    RealizedHarmony() : harmony_(0), notes_(PitchMap()), dirty_(1) {}

    RealizedHarmony(Harmony *h) : harmony_(h), notes_(PitchMap()), dirty_(1) {}

    void set_voicing(Voicing);
    void set_duration(HDuration);
    void set_literal(bool);

    void set_dirty(bool dirty) { cascade_dirty(dirty); } //set dirty flag and cascade

    void set_harmony(Harmony *h) { harmony_ = h; }

    Voicing voicing() const { return voicing_; }

    HDuration duration() const { return duration_; }

    bool literal() const { return literal_; }

    Harmony *harmony() { return harmony_; }

    bool valid() const { return !dirty_ && harmony_; }

    const PitchMap &notes() const;
    const PitchMap generate_notes(int root_tpc, int bass_tpc, bool literal, Voicing voicing, int transpose_offset) const;

    void update(int root_tpc, int bass_tpc, int transpose_offset = 0); //updates the notes map

    Fraction get_actual_duration(int utick, HDuration duration_type = HDuration::INVALID) const;

private:
    PitchMap get_intervals(int root_tpc, bool literal = true) const;
    PitchMap normalize_note_map(const PitchMap &intervals, int root_tpc, int root_pitch, size_t max = 128,
                                bool enforce_max_as_goal = false) const;
    void cascade_dirty(bool dirty);

    Harmony *harmony_ = nullptr;

    PitchMap notes_;

    Voicing voicing_ = Voicing::AUTO;
    HDuration duration_ = HDuration::INVALID;

    //whether or not the current notes QMap is up to date
    bool dirty_ = false;
    bool literal_ = false; //use all notes when possible and do not add any notes
};
} // namespace mu::engraving
