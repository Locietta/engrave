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

#include "log.h"
#include "tiejumppointlist.h"

#include "../editing/edittie.h"

#include "barline.h"
#include "chord.h"
#include "factory.h"
#include "marker.h"
#include "measure.h"
#include "note.h"
#include "partialtie.h"
#include "score.h"
#include "segment.h"
#include "tie.h"
#include "volta.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   TieJumpPoint
//---------------------------------------------------------

TieJumpPoint::TieJumpPoint(Note *note, bool active, int idx, bool following_note)
    : note_(note), active_(active), following_note_(following_note) {
    id_ = u"jumpPoint" + String::from_std_string(std::to_string(idx));
    if (active && end_tie()) {
        end_tie()->set_jump_point(this);
    }
}

Tie *TieJumpPoint::end_tie() const { return note_ ? note_->tie_back() : nullptr; }

void TieJumpPoint::undo_set_active(bool v) {
    Score *score = note_ ? note_->score() : nullptr;
    if (!score || active_ == v) {
        return;
    }
    score->undo(new ChangeTieJumpPointActive(jump_point_list_, id_, v));
}

TranslatableString TieJumpPoint::menu_title() const {
    const Measure *measure = note_->find_measure();
    const int measure_no = measure ? measure->measure_number() + 1 : 0;

    //: %1 represents the preceding jump item eg. coda. %2 represents the measure number
    return TranslatableString("engraving", "Tie to %1 (m. %2)").arg(preceding_jump_item_name(), String::number(measure_no));
}

String TieJumpPoint::preceding_jump_item_name() const {
    const Chord *start_chord = note_->chord();
    const Segment *seg = start_chord->segment();
    const Measure *measure = seg->measure();

    if (seg->score()->first_segment(SegmentType::CHORD_REST) == seg) {
        //: Used at %1 in the string "Tie to %1 (m. %2)"
        return muse::mtrc("engraving", "start of score", "partial tie menu");
    }

    // Markers
    for (const EngravingItem *e : measure->el()) {
        if (!e->is_marker()) {
            continue;
        }

        const Marker *marker = to_marker(e);
        if (marker->is_right_marker()) {
            continue;
        }

        if (marker->marker_type() == MarkerType::CODA || marker->marker_type() == MarkerType::VARCODA) {
            //: Used at %1 in the string "Tie to %1 (m. %2)"
            return muse::mtrc("engraving", "coda", "partial tie menu");
        } else {
            //: Used at %1 in the string "Tie to %1 (m. %2)"
            return muse::mtrc("engraving", "segno", "partial tie menu");
        }
    }

    // Voltas
    auto spanners = note_->score()->spanner_map().find_overlapping(measure->tick().ticks(), measure->tick().ticks());
    for (auto &spanner : spanners) {
        if (!spanner.value->is_volta() || Fraction::from_ticks(spanner.start) != start_chord->tick()) {
            continue;
        }

        Volta *volta = to_volta(spanner.value);

        //: Used at %1 in the string "Tie to %1 (m. %2)". %1 in this string represents the volta's text set by the user
        return muse::mtrc("engraving", "“%1” volta", "partial tie menu").arg(volta->begin_text());
    }

    // Repeat barlines
    if (measure->repeat_start()) {
        //: Used at %1 in the string "Tie to %1 (m. %2)"
        return muse::mtrc("engraving", "start repeat", "partial tie menu");
    }

    for (Segment *prev_seg = seg->prev(SegmentType::BAR_LINE_TYPES); prev_seg && prev_seg->tick() == seg->tick();
         prev_seg = prev_seg->prev(SegmentType::BAR_LINE_TYPES)) {
        EngravingItem *el = prev_seg->element(start_chord->track());
        if (!el || !el->is_bar_line()) {
            continue;
        }

        BarLine *bl = to_bar_line(el);
        if (bl->bar_line_type() & (BarLineType::START_REPEAT | BarLineType::END_START_REPEAT)) {
            //: Used at %1 in the string "Tie to %1 (m. %2)"
            return muse::mtrc("engraving", "start repeat", "partial tie menu");
        }
    }

    if (following_note_) {
        //: Used at %1 in the string "Tie to %1 (m. %2)"
        return muse::mtrc("engraving", "next note", "partial tie menu");
    }

    //: Used at %1 in the string "Tie to %1 (m. %2)"
    return muse::mtrc("engraving", "invalid", "partial tie menu");
}

//---------------------------------------------------------
//   TieJumpPointList
//---------------------------------------------------------

TieJumpPointList::~TieJumpPointList() {
    muse::delete_all(jump_points_);
    jump_points_.clear();
}

void TieJumpPointList::add(TieJumpPoint *item) {
    item->set_jump_point_list(this);
    jump_points_.push_back(item);
}

void TieJumpPointList::clear() {
    for (const TieJumpPoint *jump_point : jump_points_) {
        Tie *end_tie = jump_point->end_tie();
        if (!end_tie) {
            continue;
        }
        end_tie->set_jump_point(nullptr);
    }
    muse::delete_all(jump_points_);
    jump_points_.clear();
}

Tie *TieJumpPointList::start_tie() const { return note_ ? note_->tie_for() : nullptr; }

TieJumpPoint *TieJumpPointList::find_jump_point(const String &id) {
    for (TieJumpPoint *jump_point : jump_points_) {
        if (jump_point->id() != id) {
            continue;
        }

        return jump_point;
    }
    return nullptr;
}

void TieJumpPointList::toggle_jump_point(const String &id) {
    TieJumpPoint *end = find_jump_point(id);

    if (!end) {
        LOGE() << "No partial tie end point found with id: " << id;
        return;
    }

    Score *score = end->note() ? end->note()->score() : nullptr;
    if (!score) {
        return;
    }

    score->start_cmd(TranslatableString("engraving", "Toggle partial tie"));
    const bool checked = end->active();
    if (checked) {
        undo_remove_tie_from_score(end);
    } else {
        undo_add_tie_to_score(end);
    }
    score->end_cmd();
}

void TieJumpPointList::undo_add_tie_to_score(TieJumpPoint *jump_point) {
    Note *note = jump_point->note();
    Score *score = note ? note->score() : nullptr;
    Tie *tie = start_tie();
    if (!tie || !score) {
        return;
    }

    if (jump_point->following_note()) {
        // Remove partial tie and add full tie
        if (!tie->is_partial_tie() || !to_partial_tie(tie)->is_outgoing()) {
            return;
        }
        jump_point->undo_set_active(true);
        Tie::change_tie_type(tie, note);
        return;
    }

    jump_point->undo_set_active(true);

    // Check if there is already a tie.  If so, add partial tie info to it
    Tie *tie_back = note->tie_back();
    if (tie_back && !tie_back->is_partial_tie()) {
        tie_back->set_jump_point(jump_point);
        return;
    }
    // Otherwise create incoming partial tie on note
    PartialTie *pt = Factory::create_partial_tie(note);
    pt->set_end_note(note);
    pt->set_jump_point(jump_point);
    score->undo_add_element(pt);
}

void TieJumpPointList::undo_remove_tie_from_score(TieJumpPoint *jump_point) {
    Note *note = jump_point->note();
    Score *score = note ? note->score() : nullptr;
    Tie *tie = start_tie();
    if (!tie || !score) {
        return;
    }

    if (jump_point->following_note()) {
        // Remove full tie and add partial tie
        if (tie->is_partial_tie()) {
            return;
        }
        jump_point->undo_set_active(false);

        Tie::change_tie_type(tie);
        return;
    }

    jump_point->undo_set_active(false);

    // Check if there is a full tie. If so, remove partial tie info from it
    Tie *tie_back = note->tie_back();
    if (tie_back && !tie_back->is_partial_tie()) {
        tie_back->set_jump_point(nullptr);
        return;
    }
    // Otherwise remove incoming partial tie on note
    PartialTie *pt = note->incoming_partial_tie();
    if (!pt) {
        return;
    }
    score->undo_remove_element(pt);
}
