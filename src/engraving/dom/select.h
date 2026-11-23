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

#include <ankerl/unordered_dense.h>

#include "../types/types.h"
#include "chord.h"
#include "durationtype.h"
#include "mscore.h"
#include "pitchspelling.h"

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

struct NotePattern : ElementPattern {
    std::vector<Note *> el;
    int pitch = -1;
    int string = ::mu::engraving::invalid_string_index;
    int tpc = Tpc::TPC_INVALID;
    NoteHeadGroup notehead = NoteHeadGroup::HEAD_INVALID;
    TDuration duration_type = TDuration();
    NoteType type = NoteType::INVALID;
};

enum class SelState : char {
    NONE,  // nothing is selected
    LIST,  // disjoint selection
    RANGE, // adjacent selection, a range in one or more staves
    // is selected
};

//    For SelState::LIST state only visible elements can be selected
//    (no Chord element etc.).

struct Selection {
    Selection() {
        score_ = 0;
        state_ = SelState::NONE;
    }

    Selection(Score *);

    SelState state() const { return state_; }

    bool is_none() const { return state_ == SelState::NONE; }

    bool is_range() const { return state_ == SelState::RANGE; }

    bool is_list() const { return state_ == SelState::LIST; }

    void set_state(SelState s);

    const std::vector<EngravingItem *> &elements() const { return el_; }

    std::vector<EngravingItem *> elements(ElementType type) const;

    bool is_single() const { return (state_ == SelState::LIST) && (el_.size() == 1); }

    void add(EngravingItem *);
    void deselect_all();
    void remove(EngravingItem *);
    void clear();
    EngravingItem *element() const;

    ChordRest *first_chord_rest(track_idx_t track = muse::nidx) const;
    ChordRest *last_chord_rest(track_idx_t track = muse::nidx) const;

    MeasureBase *start_measure_base() const;

    void update();

    Segment *start_segment() const { return start_segment_; }

    Segment *end_segment() const { return end_segment_; }

    void set_start_segment(Segment *s);
    void set_end_segment(Segment *s);

    void set_range(Segment *start_segment, Segment *end_segment, staff_idx_t staff_start, staff_idx_t staff_end, Box *start_box = nullptr,
                   Box *end_box = nullptr);

    void set_active_segment(Segment *s) { active_segment_ = s; }

    ChordRest *active_cr() const;

    Fraction tick_start() const;
    Fraction tick_end() const;

    staff_idx_t staff_start() const { return staff_start_; }

    staff_idx_t staff_end() const { return staff_end_; }

    void set_staff_start(int v) { staff_start_ = v; }

    void set_staff_end(int v) { staff_end_ = v; }

    void set_active_track(track_idx_t v) { active_track_ = v; }

    void update_selected_elements();

    void extend_range_selection(Segment *seg, Segment *seg_after, staff_idx_t staff_idx, const Fraction &tick, const Fraction &etick,
                                Box *box = nullptr);

private:
    void append_anchored_elements(const ankerl::unordered_dense::set<EngravingItem *> &elems);
    void append_chord_rest(ChordRest *cr);
    void append_tuplet_hierarchy(Tuplet *innermost_tuplet);
    void append_guitar_bend(GuitarBend *guitar_bend);

    ChordRest *first_chord_rest_in_range(track_idx_t preferred_track = muse::nidx) const;
    ChordRest *last_chord_rest_in_range(track_idx_t preferred_track = muse::nidx) const;

    void validate_boxes_and_segments(); // Checks that the Boxes/segments match (asserts and clears the Box if not)

    bool range_is_valid() const;
    void refresh_state();

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
};
} // namespace mu::engraving
