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

#include "accidental.h"
#include "chord.h"
#include "durationtype.h"
#include "mscore.h"
#include "noteval.h"

#include "../types/types.h"

namespace mu::engraving {
class ChordRest;
class Drumset;
class EngravingItem;
class Note;
class Score;
class Segment;
class Selection;
class Slur;
class Staff;

// no ordinal for the visual repres. of string
// (topmost in TAB varies according to visual order and presence of bass strings)
static constexpr int visual_invalid_string_index = -100;

//---------------------------------------------------------
//   NoteEntryMethod
//---------------------------------------------------------

enum class NoteEntryMethod : char { UNKNOWN, BY_NOTE_NAME, BY_DURATION, REPITCH, RHYTHM, REALTIME_AUTO, REALTIME_MANUAL, TIMEWISE };

//---------------------------------------------------------
//   InputState
//---------------------------------------------------------

class InputState {
public:
    bool is_valid() const;

    ChordRest *cr() const;

    Fraction tick() const;

    void set_duration(const TDuration &d) { duration_ = d; }

    TDuration duration() const { return duration_; }

    void set_dots(int n);

    Fraction ticks() const { return duration_.ticks(); }

    Segment *segment() const { return segment_; }

    void set_segment(Segment *s);

    Segment *last_segment() const { return last_segment_; }

    void set_last_segment(Segment *s) { last_segment_ = s; }

    Staff *staff() const;
    staff_idx_t staff_idx() const;

    Drumset *drumset() const;

    int drum_note() const { return drum_note_; }

    void set_drum_note(int v) { drum_note_ = v; }

    // Used in the input-by-duration mode
    const NoteValList &notes() const { return notes_; }

    void set_notes(const NoteValList &v) { notes_ = v; }

    voice_idx_t voice() const { return track_ == muse::nidx ? 0 : (track_ % ::mu::engraving::voices); }

    void set_voice(voice_idx_t v);

    track_idx_t track() const { return track_; }

    void set_track(track_idx_t v) {
        prev_track_ = track_;
        track_ = v;
    }

    track_idx_t prev_track() const { return prev_track_; }

    int string() const { return string_; }

    void set_string(int val) { string_ = val; }

    StaffGroup staff_group() const;

    bool rest() const { return rest_; }

    void set_rest(bool v) { rest_ = v; }

    NoteType note_type() const { return note_type_; }

    void set_note_type(NoteType t) { note_type_ = t; }

    BeamMode beam_mode() const { return beam_mode_; }

    void set_beam_mode(BeamMode m) { beam_mode_ = m; }

    bool note_entry_mode() const { return note_entry_mode_; }

    void set_note_entry_mode(bool v) { note_entry_mode_ = v; }

    NoteEntryMethod note_entry_method() const { return note_entry_method_; }

    void set_note_entry_method(NoteEntryMethod m) { note_entry_method_ = m; }

    bool using_note_entry_method(NoteEntryMethod m) const { return m == note_entry_method(); }

    AccidentalType accidental_type() const { return accidental_type_; }

    void set_accidental_type(AccidentalType val) { accidental_type_ = val; }

    const std::set<SymId> &articulation_ids() const { return articulation_ids_; }

    void set_articulation_ids(const std::set<SymId> &ids) { articulation_ids_ = ids; }

    Slur *slur() const { return slur_; }

    void set_slur(Slur *s) { slur_ = s; }

    void update(Selection &selection);
    void move_input_pos(EngravingItem *e);
    void move_to_next_input_pos();

    bool beyond_score() const { return beyond_score_; }

    void set_beyond_score(bool val) { beyond_score_ = val; }

    // TODO: unify with Selection::cr()?
    static Note *note(EngravingItem *);
    static ChordRest *chord_rest(EngravingItem *);

private:

    Segment *next_input_pos() const;

    track_idx_t track_ = 0;
    track_idx_t prev_track_ = 0; // used for navigation

    int drum_note_ = -1;
    int string_ = ::mu::engraving::visual_invalid_string_index; // visual string selected for input (TAB staves only)

    NoteValList notes_;

    Segment *last_segment_ = nullptr;
    Segment *segment_ = nullptr; // current segment

    bool note_entry_mode_ = false;
    NoteEntryMethod note_entry_method_ = NoteEntryMethod::BY_NOTE_NAME;

    TDuration duration_ = DurationType::V_INVALID; // currently duration
    bool rest_ = false;                            // rest mode

    NoteType note_type_ = NoteType::NORMAL;
    BeamMode beam_mode_ = BeamMode::AUTO;

    AccidentalType accidental_type_ = AccidentalType::NONE;
    Slur *slur_ = nullptr;

    std::set<SymId> articulation_ids_;

    bool beyond_score_ = false;
};
} // namespace mu::engraving
#endif
