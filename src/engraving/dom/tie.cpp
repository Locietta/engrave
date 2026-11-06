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
#include "tie.h"

#include "../rendering/iscorerenderer.h"
#include "accidental.h"
#include "barline.h"
#include "chord.h"
#include "factory.h"
#include "hook.h"
#include "ledgerline.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "notedot.h"
#include "part.h"
#include "partialtie.h"
#include "repeatlist.h"
#include "score.h"
#include "staff.h"
#include "stafftype.h"
#include "stem.h"
#include "system.h"
#include "tiejumppointlist.h"
#include "utils.h"
#include "volta.h"

using namespace mu;
using namespace muse::draw;

namespace mu::engraving {
//---------------------------------------------------------
//   TieSegment
//---------------------------------------------------------

TieSegment::TieSegment(Tie *sp) : SlurTieSegment(ElementType::TIE_SEGMENT, sp) {}

TieSegment::TieSegment(const ElementType &type, Tie *sp) : SlurTieSegment(type, sp) {}

TieSegment::TieSegment(const TieSegment &s) : SlurTieSegment(s) {}

void TieSegment::consolidate_adjustment_offset_into_user_offset() {
    for (size_t i = 0; i < adjustment_offsets_.size(); ++i) {
        Grip grip = static_cast<Grip>(i);
        PointF adjust_offset = adjustment_offsets_[i];
        if (!adjust_offset.is_null()) {
            ups(grip).p -= adjust_offset;
            ups(grip).off = adjust_offset;
        }
    }
    reset_adjustment_offset();
}

bool TieSegment::is_edited() const {
    for (int i = 0; i < int(Grip::GRIPS); ++i) {
        if (!ups_[i].off.is_null()) {
            return true;
        }
    }
    return false;
}

double TieSegment::min_shoulder_height() const { return style().style_absolute(Sid::TIE_MIN_SHOULDER_HEIGHT); }

double TieSegment::max_shoulder_height() const { return style().style_absolute(Sid::TIE_MAX_SHOULDER_HEIGHT); }

double TieSegment::end_width() const { return style().style_absolute(Sid::TIE_END_WIDTH); }

double TieSegment::mid_width() const { return style().style_absolute(Sid::TIE_MID_WIDTH); }

double TieSegment::dotted_width() const { return style().style_absolute(Sid::TIE_DOTTED_WIDTH); }

//---------------------------------------------------------
//   Tie
//---------------------------------------------------------

Tie::Tie(const ElementType &type, EngravingObject *parent) : SlurTie(type, parent) {}

TieJumpPointList *Tie::start_tie_jump_points() const { return jump_point_ ? jump_point_->jump_point_list() : nullptr; }

void Tie::update_possible_jump_points() {
    if (!tie_jump_points()) {
        return;
    }

    tie_jump_points()->clear();

    const Note *note = to_note(ownership_parent());
    const Chord *chord = note ? note->chord() : nullptr;
    const Measure *measure = chord ? chord->measure() : nullptr;
    if (!measure) {
        return;
    }

    const Segment *segment = chord ? chord->segment() : nullptr;

    // Check ties starting in this measure and ending in another
    // If they cross a repeat, add jump points
    const bool has_following_jump_item = chord->has_following_jump_item();

    if (!has_following_jump_item) {
        const Note *tie_end_note = end_note();
        const Chord *end_chord = tie_end_note ? tie_end_note->chord() : nullptr;
        if (!end_chord) {
            return;
        }
        const Segment *end_note_segment = end_chord ? end_chord->segment() : nullptr;
        const ChordRest *final_cr_of_measure = measure->last_chord_rest(track());
        const bool final_cr_has_following_jump = final_cr_of_measure ? final_cr_of_measure->has_following_jump_item() : false;
        const bool segs_are_adjacent = segments_are_adjacent(segment, end_note_segment);
        const bool segs_are_in_different_repeat_segments = segments_are_in_different_repeat_segments(segment, end_note_segment);

        if (!(final_cr_has_following_jump && segs_are_adjacent) || !segs_are_in_different_repeat_segments) {
            return;
        }
    }

    int jump_point_idx = 0;

    Note *next_note = search_tie_note(note);
    next_note = next_note ? next_note : end_note();

    if (next_note) {
        const bool has_tie = next_note->tie_back();
        TieJumpPoint *jump_point = new TieJumpPoint(next_note, has_tie, jump_point_idx, true);
        tie_jump_points()->add(jump_point);
        jump_point_idx++;
    }

    for (Measure *jump_measure : find_following_repeat_measures(measure)) {
        const Segment *first_cr_seg = jump_measure ? jump_measure->first(SegmentType::CHORD_REST) : nullptr;
        if (!first_cr_seg) {
            continue;
        }

        next_note = search_tie_note(note, first_cr_seg, false);

        if (next_note) {
            bool has_incoming_tie = next_note->tie_back();
            TieJumpPoint *jump_point = new TieJumpPoint(next_note, has_incoming_tie, jump_point_idx, false);
            tie_jump_points()->add(jump_point);
            jump_point_idx++;
        }
    }

    if (jump_point_idx < 2 && !is_partial_tie()) {
        tie_jump_points()->clear();
    }
}

void Tie::add_ties_to_jump_points() {
    update_possible_jump_points();
    TieJumpPointList *jump_points = tie_jump_points();
    if (!jump_points) {
        return;
    }

    for (TieJumpPoint *jump_point : *jump_points) {
        if (jump_point->following_note()) {
            jump_point->undo_set_active(true);
            continue;
        }
        jump_points->undo_add_tie_to_score(jump_point);
    }

    // Update jump points for linked ties
    for (EngravingObject *linked_tie : link_list()) {
        if (!linked_tie || !linked_tie->is_tie() || linked_tie == this) {
            continue;
        }
        to_tie(linked_tie)->update_possible_jump_points();
    }
}

void Tie::undo_remove_ties_from_jump_points() {
    TieJumpPointList *jump_points = tie_jump_points();
    if (!jump_points) {
        return;
    }
    for (TieJumpPoint *jump_point : *jump_points) {
        if (jump_point->following_note() || !jump_point->active()) {
            jump_point->undo_set_active(false);
            continue;
        }

        jump_points->undo_remove_tie_from_score(jump_point);
    }
}

bool Tie::all_jump_points_inactive() const {
    if (end_note()) {
        return false;
    }
    if (!tie_jump_points()) {
        return true;
    }

    for (const TieJumpPoint *jump_point : *tie_jump_points()) {
        if (jump_point->active()) {
            return false;
        }
    }

    return true;
}

TieJumpPointList *Tie::tie_jump_points() { return start_note() ? start_note()->tie_jump_points() : nullptr; }

const TieJumpPointList *Tie::tie_jump_points() const { return start_note() ? start_note()->tie_jump_points() : nullptr; }

Tie::Tie(EngravingObject *parent) : SlurTie(ElementType::TIE, parent) {}

Tie::Tie(const Tie &t) : SlurTie(t) {
    is_inside_ = t.is_inside_;
    tie_placement_ = t.tie_placement_;
    // Jump points must be recalculated for this tie
    jump_point_ = nullptr;
}

PropertyValue Tie::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TIE_PLACEMENT: return tie_placement();
        default: return SlurTie::get_property(property_id);
    }
}

PropertyValue Tie::property_default(Pid id) const {
    switch (id) {
        case Pid::TIE_PLACEMENT: return TiePlacement::AUTO;
        default: return SlurTie::property_default(id);
    }
}

bool Tie::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::TIE_PLACEMENT: set_tie_placement(v.value<TiePlacement>()); break;
        default: return SlurTie::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

double Tie::scaling_factor() const {
    const bool has_both_notes = start_note() && end_note();

    const Note *primary_note = start_note() ? start_note() : end_note();
    const Note *secondary_note = has_both_notes ? end_note() : nullptr;

    if (!primary_note) {
        return 1.0;
    }

    if (primary_note->is_grace()) {
        return style().style_d(Sid::GRACE_NOTE_MAG);
    }

    if (has_both_notes) {
        return 0.5 * (primary_note->chord()->intrinsic_mag() + secondary_note->chord()->intrinsic_mag());
    }

    return primary_note->chord()->intrinsic_mag();
}

void Tie::set_start_note(Note *note) {
    set_start_element(note);
    set_ownership_parent(note);
}

Note *Tie::start_note() const {
    assert(!start_element() || start_element()->is_note());
    return to_note(start_element());
}

Note *Tie::end_note() const { return to_note(end_element()); }

bool Tie::is_outer_tie_of_chord(Grip start_or_end) const {
    if (is_inside_) {
        return false;
    }

    const bool start = start_or_end == Grip::START;
    const Note *note = start ? start_note() : end_note();
    if (!note) {
        return false;
    }

    const Chord *chord = note->chord();

    return (note == chord->up_note() && up()) || (note == chord->down_note() && !up());
}

bool Tie::has_tied_second_inside() const {
    const Note *note = start_note();
    if (!note) {
        return false;
    }

    const Chord *chord = note->chord();
    const int line = note->line();
    const int second_inside_line = up() ? line + 1 : line - 1;

    for (const Note *other_note : chord->notes()) {
        if (other_note->line() == second_inside_line && other_note->tie_for() && other_note->tie_for()->up() == up()) {
            return true;
        }
    }

    return false;
}

bool Tie::is_cross_staff() const {
    const Note *start_n = start_note();
    const Note *end_n = end_note();
    const Chord *start_chord = start_n ? start_n->chord() : nullptr;
    const Chord *end_chord = end_n ? end_n->chord() : nullptr;
    const staff_idx_t staff = staff_idx();

    return (start_chord && (start_chord->staff_move() != 0 || start_chord->v_staff_idx() != staff)) ||
           (end_chord && (end_chord->staff_move() != 0 || end_chord->v_staff_idx() != staff));
}

void Tie::change_tie_type(Tie *old_tie, Note *end_note) {
    // Replaces oldTie with an outgoing partial tie if no endNote is specified.  Otherwise replaces oldTie with a regular tie
    Note *start_note = old_tie->start_note();
    bool add_partial_tie = !end_note;
    Score *score = start_note ? start_note->score() : nullptr;
    if (!score) {
        return;
    }

    Tie *new_tie = add_partial_tie ? Factory::create_partial_tie(start_note) : Factory::create_tie(start_note);

    score->undo_remove_element(old_tie);

    new_tie->set_start_note(start_note);
    new_tie->set_tick(start_note->tick());
    new_tie->set_track(start_note->track());
    start_note->set_tie_for(new_tie);
    if (!add_partial_tie) {
        new_tie->set_end_note(end_note);
        end_note->set_tie_back(new_tie);
    }

    new_tie->set_style_type(old_tie->style_type());
    new_tie->set_tie_placement(old_tie->tie_placement());
    new_tie->set_slur_direction(old_tie->slur_direction());

    new_tie->set_visible(old_tie->visible());
    new_tie->set_offset(old_tie->offset());

    score->undo_add_element(new_tie);
}

void Tie::update_start_tie_on_removal() {
    if (!jump_point() || !start_tie() || !start_tie_jump_points()) {
        return;
    }
    jump_point()->undo_set_active(false);
    Tie *start_tie = this->start_tie();
    if (start_tie_jump_points()->size() <= 1 || start_tie->all_jump_points_inactive()) {
        score()->undo_remove_element(start_tie);
    }
}

Tie *Tie::start_tie() const { return start_tie_jump_points() ? start_tie_jump_points()->start_tie() : nullptr; }
} // namespace mu::engraving
