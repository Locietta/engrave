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
#include "durationtype.h"
#include "mscore.h"
#include "pitchspelling.h"
#include "selectionfilter.h"

namespace mu::engraving {
struct Score;
struct Page;
struct System;
struct ChordRest;
struct EngravingItem;
struct Segment;
struct Note;
struct Measure;
struct MeasureBase;
struct Chord;
struct Tuplet;
struct GuitarBend;
struct Box;

//---------------------------------------------------------
//   ElementPattern
//---------------------------------------------------------

struct ElementPattern {
    std::vector<EngravingItem *> el;
    int type = 0;
    int subtype = 0;
    staff_idx_t staff_start = 0;
    staff_idx_t staff_end = 0; // exclusive
    voice_idx_t voice = 0;
    bool subtype_valid = false;
    Fraction duration_ticks{-1, 1};
    Fraction beat{0, 0};
    const Measure *measure = nullptr;
    const System *system = nullptr;
};

//---------------------------------------------------------
//   NotePattern
//---------------------------------------------------------

struct NotePattern : ElementPattern {
    std::vector<Note *> el;
    int pitch = -1;
    int string = ::mu::engraving::invalid_string_index;
    int tpc = Tpc::TPC_INVALID;
    NoteHeadGroup notehead = NoteHeadGroup::HEAD_INVALID;
    TDuration duration_type = TDuration();
    NoteType type = NoteType::INVALID;
};

//---------------------------------------------------------
//   SelState
//---------------------------------------------------------

enum class SelState : char {
    NONE,  // nothing is selected
    LIST,  // disjoint selection
    RANGE, // adjacent selection, a range in one or more staves
    // is selected
};

//-------------------------------------------------------------------
//   Selection
//    For SelState::LIST state only visible elements can be selected
//    (no Chord element etc.).
//-------------------------------------------------------------------

struct Selection {
    Selection() {
        score_ = 0;
        state_ = SelState::NONE;
    }

    Selection(Score *);

    Score *score() const { return score_; }

    SelState state() const { return state_; }

    bool is_none() const { return state_ == SelState::NONE; }

    bool is_range() const { return state_ == SelState::RANGE; }

    bool is_list() const { return state_ == SelState::LIST; }

    void set_state(SelState s);

    //! NOTE If locked, the selected items should not be changed.
    void lock(const String &reason) { lock_reason_ = reason; }

    void unlock(const String & /*reason*/) { lock_reason_.clear(); } // reason for clarity

    bool is_locked() const { return !lock_reason_.is_empty(); }

    const String &lock_reason() const { return lock_reason_; }

    const std::vector<EngravingItem *> &elements() const { return el_; }

    std::vector<EngravingItem *> elements(ElementType type) const;
    std::vector<Note *> note_list(track_idx_t track = muse::nidx) const;

    const std::list<EngravingItem *> unique_elements() const;
    std::vector<Note *> unique_notes(track_idx_t track = muse::nidx, bool tied = true) const;

    bool is_single() const { return (state_ == SelState::LIST) && (el_.size() == 1); }

    void add(EngravingItem *);
    void deselect_all();
    void remove(EngravingItem *);
    void clear();
    EngravingItem *element() const;
    ChordRest *cr() const;
    ChordRest *first_chord_rest(track_idx_t track = muse::nidx) const;
    ChordRest *last_chord_rest(track_idx_t track = muse::nidx) const;
    Measure *find_measure() const;
    MeasureBase *start_measure_base() const;
    MeasureBase *end_measure_base() const;
    void update();
    void update_state();
    void dump();

    Segment *start_segment() const { return start_segment_; }

    Segment *end_segment() const { return end_segment_; }

    void set_start_segment(Segment *s);
    void set_end_segment(Segment *s);

    Box *start_box() const { return start_box_; }

    Box *end_box() const { return end_box_; }

    void set_range(Segment *start_segment, Segment *end_segment, staff_idx_t staff_start, staff_idx_t staff_end, Box *start_box = nullptr,
                   Box *end_box = nullptr);
    void set_range_ticks(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff_start, staff_idx_t staff_end,
                         Box *start_box = nullptr, Box *end_box = nullptr);

    Segment *active_segment() const { return active_segment_; }

    void set_active_segment(Segment *s) { active_segment_ = s; }

    ChordRest *active_cr() const;
    bool is_end_active() const;
    ChordRest *current_cr() const;
    Fraction tick_start() const;
    Fraction tick_end() const;

    staff_idx_t staff_start() const { return staff_start_; }

    staff_idx_t staff_end() const { return staff_end_; }

    track_idx_t active_track() const { return active_track_; }

    void set_staff_start(int v) { staff_start_ = v; }

    void set_staff_end(int v) { staff_end_ = v; }

    void set_active_track(track_idx_t v) { active_track_ = v; }

    void update_selected_elements();
    bool measure_base_range(MeasureBase **mb1, MeasureBase **mb2) const;
    void extend_range_selection(ChordRest *cr);
    void extend_range_selection(Segment *seg, Segment *seg_after, staff_idx_t staff_idx, const Fraction &tick, const Fraction &etick,
                                Box *box = nullptr);
    bool range_contains_multi_note_chords() const;

private:
    SelectionFilter selection_filter() const;

    bool can_select(const EngravingItem *e) const { return selection_filter().can_select(e); }

    bool can_select_note_idx(size_t note_idx, size_t total_notes_in_chord, bool range_contains_multi_note_chords) const;

    bool can_select_voice(track_idx_t track) const { return selection_filter().can_select_voice(track); }

    void append_filtered(EngravingItem *e);
    void append_filtered(const std::unordered_set<EngravingItem *> &elems);
    void append_chord_rest(ChordRest *cr);
    void append_tuplet_hierarchy(Tuplet *innermost_tuplet);
    void append_guitar_bend(GuitarBend *guitar_bend);

    ChordRest *first_chord_rest_in_range(track_idx_t preferred_track = muse::nidx) const;
    ChordRest *last_chord_rest_in_range(track_idx_t preferred_track = muse::nidx) const;

    void validate_boxes_and_segments(); // Checks that the Boxes/segments match (asserts and clears the Box if not)

    bool range_is_valid() const;

    Score *score_ = nullptr;
    SelState state_ = SelState::NONE;
    std::vector<EngravingItem *> el_; // valid in mode SelState::LIST

    staff_idx_t staff_start_ = 0; // valid if selState is SelState::RANGE
    staff_idx_t staff_end_ = 0;

    Segment *start_segment_ = nullptr;
    Segment *end_segment_ = nullptr; // next segment after selection

    Box *start_box_ = nullptr;
    Box *end_box_ = nullptr; // the last Box included in the selection

    Fraction planned_tick1_{-1, 1}; // Will be actually selected on updateSelectedElements() call.
    Fraction planned_tick2_{-1, 1}; // Used by setRangeTicks() to restore proper selection after
    // command end in case some changes are expected to segments'
    // structure (e.g. MMRests reconstruction).

    Segment *active_segment_ = nullptr;
    track_idx_t active_track_ = 0;

    Fraction current_tick_; // tracks the most recent selection
    track_idx_t current_track_ = 0;

    bool range_contains_multi_note_chords_ = false; // cached - calculating this isn't free

    String lock_reason_;
};
} // namespace mu::engraving
