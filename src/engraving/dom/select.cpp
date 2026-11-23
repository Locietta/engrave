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

/**
 \file
 Implementation of class Selection plus other selection related functions.
*/

#include "select.h"

#include <algorithm>

#include <ankerl/unordered_dense.h>

#include "../types/types.h"
#include "accidental.h"
#include "arpeggio.h"
#include "articulation.h"
#include "beam.h"
#include "box.h"
#include "breath.h"
#include "chord.h"
#include "dynamic.h"
#include "engravingitem.h"
#include "expression.h"
#include "figuredbass.h"
#include "fingering.h"
#include "fret.h"
#include "global/containers.h"
#include "guitarbend.h"
#include "harmony.h"
#include "harppedaldiagram.h"
#include "hook.h"
#include "laissezvib.h"
#include "linkedobjects.h"
#include "log.h"
#include "lyrics.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "notedot.h"
#include "pagelockindicator.h"
#include "part.h"
#include "partialtie.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "stafftextbase.h"
#include "stem.h"
#include "stemslash.h"
#include "sticking.h"
#include "system.h"
#include "tie.h"
#include "tremolosinglechord.h"
#include "tremolotwochord.h"
#include "tuplet.h"
#include "utils.h"

using namespace mu;
using namespace mu::engraving;

// ====================================================
// ====================================================

Selection::Selection(Score *s) {
    score_ = s;
    state_ = SelState::NONE;
    start_segment_ = 0;
    end_segment_ = 0;
    start_box_ = 0;
    end_box_ = 0;
    active_segment_ = 0;
    staff_start_ = 0;
    staff_end_ = 0;
    active_track_ = 0;
}

Fraction Selection::tick_start() const {
    switch (state_) {
        case SelState::RANGE: return start_segment_ ? start_segment_->tick() : Fraction(-1, 1);
        case SelState::LIST: {
            ChordRest *cr = first_chord_rest();
            return (cr) ? cr->tick() : Fraction(-1, 1);
        }
        default: return Fraction(-1, 1);
    }
}

Fraction Selection::tick_end() const {
    switch (state_) {
        case SelState::RANGE: {
            if (end_segment_) {
                return end_segment_->tick();
            } else { // endsegment == 0 if end of score
                Measure *m = score_->last_measure();
                return m->end_tick();
            }
            break;
        }
        case SelState::LIST: {
            ChordRest *cr = last_chord_rest();
            return (cr) ? cr->segment()->tick() : Fraction(-1, 1);
            break;
        }
        default: return Fraction(-1, 1);
    }
}

EngravingItem *Selection::element() const { return ((state() != SelState::RANGE) && (el_.size() == 1)) ? el_[0] : 0; }

ChordRest *Selection::active_cr() const {
    if (state_ != SelState::RANGE || !active_segment_) {
        return nullptr;
    }
    if (active_segment_ == start_segment_) {
        return first_chord_rest(active_track_);
    } else {
        return last_chord_rest(active_track_);
    }
}

ChordRest *Selection::first_chord_rest_in_range(track_idx_t preferred_track) const {
    IF_ASSERT_FAILED(is_range()) { return nullptr; }

    Segment *first_cr_segment = nullptr;

    for (Segment *curr_seg = start_segment_; curr_seg && (curr_seg != end_segment_); curr_seg = curr_seg->next1_mm()) {
        if (!curr_seg->enabled() || !curr_seg->is_chord_rest_type()) {
            continue;
        }
        if (preferred_track == muse::nidx) {
            // no track specified, use the first CR segment we find...
            first_cr_segment = curr_seg;
            break;
        }
        EngravingItem *e = curr_seg->element(preferred_track);
        if (e && e->is_chord_rest()) {
            return to_chord_rest(e);
        }
    }

    if (!first_cr_segment) {
        return nullptr;
    }

    for (track_idx_t track = staff2track(staff_start()); track < staff2track(staff_end()); ++track) {
        EngravingItem *e = first_cr_segment->element(track);
        if (e && e->is_chord_rest()) {
            return to_chord_rest(e);
        }
    }
    return nullptr;
}

ChordRest *Selection::last_chord_rest_in_range(track_idx_t preferred_track) const {
    IF_ASSERT_FAILED(is_range()) { return nullptr; }

    Segment *last_cr_segment = nullptr;

    Segment *last_seg_in_score = score_->last_measure_mm() ? score_->last_measure_mm()->last() : nullptr;
    Segment *curr_seg = end_segment_ ? end_segment_->prev1_mm() : last_seg_in_score;
    while (curr_seg) {
        if (!curr_seg->enabled() || !curr_seg->is_chord_rest_type()) {
            curr_seg = curr_seg->prev1_mm();
            continue;
        }
        if (preferred_track == muse::nidx) {
            // no track specified, use the first CR segment we find...
            last_cr_segment = curr_seg;
            break;
        }
        EngravingItem *e = curr_seg->element(preferred_track);
        if (e && e->is_chord_rest()) {
            return to_chord_rest(e);
        }
        curr_seg = curr_seg->prev1_mm();
        if (curr_seg == start_segment_->prev1_mm()) {
            break;
        }
    }

    if (!last_cr_segment) {
        return nullptr;
    }

    for (track_idx_t track = staff2track(staff_start()); track < staff2track(staff_end()); ++track) {
        EngravingItem *e = last_cr_segment->element(track);
        if (e && e->is_chord_rest()) {
            return to_chord_rest(e);
        }
    }

    return nullptr;
}

ChordRest *Selection::first_chord_rest(track_idx_t track) const {
    if (is_range()) {
        return first_chord_rest_in_range(track);
    }
    if (el_.size() == 1) {
        EngravingItem *el = el_[0];
        if (el->is_note()) {
            return to_chord_rest(el->ownership_parent());
        } else if (el->is_chord_rest()) {
            return to_chord_rest(el);
        }
        return nullptr;
    }
    ChordRest *cr = nullptr;
    for (EngravingItem *el : el_) {
        if (el->is_note()) {
            el = to_note(el)->chord();
        }
        if (el->is_chord_rest()) {
            if (track != muse::nidx && el->track() != track) {
                continue;
            }
            if (cr) {
                if (to_chord_rest(el)->tick() < cr->tick()) {
                    cr = to_chord_rest(el);
                }
            } else {
                cr = to_chord_rest(el);
            }
        }
    }
    return cr;
}

ChordRest *Selection::last_chord_rest(track_idx_t track) const {
    if (is_range()) {
        return last_chord_rest_in_range(track);
    }
    if (el_.size() == 1) {
        EngravingItem *el = el_[0];
        if (el) {
            if (el->is_note()) {
                return to_chord_rest(el->ownership_parent());
            } else if (el->is_chord_rest()) {
                return to_chord_rest(el);
            }
        }
        return nullptr;
    }
    ChordRest *cr = nullptr;
    for (EngravingItem *el : el_) {
        if (el->is_note()) {
            el = to_note(el)->chord();
        }
        if (el->is_chord_rest() && to_chord_rest(el)->segment()->is_chord_rest_type()) {
            if (track != muse::nidx && el->track() != track) {
                continue;
            }
            if (cr) {
                if (to_chord_rest(el)->tick() >= cr->tick()) {
                    cr = to_chord_rest(el);
                }
            } else {
                cr = to_chord_rest(el);
            }
        }
    }
    return cr;
}

MeasureBase *Selection::start_measure_base() const {
    EngravingItem *selection_element = element();
    if (selection_element) {
        if (selection_element->is_box()) {
            return to_measure_base(selection_element);
        }
        MeasureBase *mb = selection_element->find_measure_base();
        if (mb) {
            return mb;
        }
    }

    if (start_box_) {
        return start_box_;
    }

    if (tick_start().negative()) { // Tick is not set
        return nullptr;
    }

    MeasureBase *mb = score_->tick2measure_base(tick_start());
    if (mb && mb->is_measure()) {
        mb = to_measure(mb)->covering_mm_rest_or_this();
    }
    return mb;
}

void Selection::deselect_all() {
    if (state_ == SelState::RANGE) {
        score_->set_update_all();
    }
    clear();
}

static RectF change_selection(EngravingItem *e, bool b) {
    RectF r = e->canvas_bounding_rect();
    e->set_selected(b);
    r.unite(e->canvas_bounding_rect());
    return r;
}

void Selection::clear() {
    for (EngravingItem *e : el_) {
        e->score()->add_refresh(change_selection(e, false));
        if (e->is_spanner()) { // TODO: only visible elements should be selectable?
            Spanner *sp = to_spanner(e);
            for (auto s : sp->spanner_segments()) {
                e->score()->add_refresh(change_selection(s, false));
            }
        }
    }
    el_.clear();
    start_segment_ = 0;
    end_segment_ = 0;
    start_box_ = 0;
    end_box_ = 0;
    active_segment_ = 0;
    staff_start_ = 0;
    staff_end_ = 0;
    active_track_ = 0;
    set_state(SelState::NONE);
}

void Selection::remove(EngravingItem *el) {
    const bool removed = muse::remove(el_, el);
    el->set_selected(false);
    if (removed) {
        refresh_state();
    }
}

void Selection::add(EngravingItem *el) {
    el_.push_back(el);
    update();
}

void Selection::append_anchored_elements(const ankerl::unordered_dense::set<EngravingItem *> &elems) {
    for (EngravingItem *elem : elems) {
        IF_ASSERT_FAILED(!elem->is_spanner_segment()) {
            LOGE() << "Append whole spanners instead of spanner segments";
            continue;
        }

        // Special handling for bends...
        if (elem->is_guitar_bend()) {
            GuitarBend *bend = to_guitar_bend(elem);
            append_guitar_bend(bend);
            continue;
        }

        // Special handling for spanners...
        if (elem->is_spanner() && !elem->is_partial_tie() && !elem->is_laissez_vib()) {
            const Spanner *spanner = to_spanner(elem);
            if (!note_anchored_spanner_is_in_range(spanner, tick_start(), tick_end())) {
                continue;
            }
            for (SpannerSegment *spanner_seg : spanner->spanner_segments()) {
                el_.push_back(spanner_seg);
            }
            continue;
        }

        // Special handling for grace notes...
        if (elem->is_chord() && to_chord(elem)->is_grace()) {
            append_chord_rest(to_chord_rest(elem));
            continue;
        }

        el_.push_back(elem);
    }
}

void Selection::append_chord_rest(ChordRest *cr) {
    const ankerl::unordered_dense::set<EngravingItem *> cr_anchored = collect_elements_anchored_to_chord_rest(cr);
    append_anchored_elements(cr_anchored);

    if (cr->is_rest_family()) {
        el_.push_back(cr);
        return;
    }

    IF_ASSERT_FAILED(cr->is_chord()) { return; }

    Chord *chord = to_chord(cr);

    for (Note *note : chord->notes()) {
        const ankerl::unordered_dense::set<EngravingItem *> note_anchored = collect_elements_anchored_to_note(note, true, false);
        append_anchored_elements(note_anchored);

        el_.push_back(note);

        if (note->accidental()) {
            el_.push_back(note->accidental());
        }
        for (EngravingItem *el : note->el()) {
            if (el->is_fingering()) {
                // Slight hack (already handled, see collectElementsAnchoredToNote)...
                continue;
            }
            el_.push_back(el);
        }
    }

    if (chord->beam() && !muse::contains(el_, static_cast<EngravingItem *>(chord->beam()))) {
        el_.push_back(chord->beam());
    }
}

static bool tuplet_is_in_range(const Tuplet &tuplet, const Fraction &start, const Fraction &end) {
    return std::ranges::all_of(tuplet.elements(), [&](const DurationElement *element) {
        return element->tick() >= start && element->tick() < end &&
               (!element->is_tuplet() || tuplet_is_in_range(*to_tuplet(element), start, end));
    });
}

void Selection::append_tuplet_hierarchy(Tuplet *innermost_tuplet) {
    if (muse::contains(el_, static_cast<EngravingItem *>(innermost_tuplet))) {
        return;
    }

    if (!tuplet_is_in_range(*innermost_tuplet, tick_start(), tick_end())) {
        return;
    }

    el_.push_back(innermost_tuplet);

    // Recursively append upwards/outwards
    Tuplet *outer_tuplet = innermost_tuplet->tuplet();
    if (outer_tuplet) {
        append_tuplet_hierarchy(outer_tuplet);
    }
}

void Selection::append_guitar_bend(GuitarBend *guitar_bend) {
    if (!guitar_bend) {
        return;
    }

    el_.push_back(guitar_bend);

    if (GuitarBendHold *hold = guitar_bend->hold_line()) {
        if (hold->tick2() < tick_end()) {
            el_.push_back(hold);
        }
    }

    if (GuitarBendSegment *bend_seg = to_guitar_bend_segment(guitar_bend->front_segment())) {
        if (GuitarBendText *bend_text = bend_seg->bend_text()) {
            el_.push_back(bend_text);
        }
    }
}

void Selection::update_selected_elements() {
    if (state_ != SelState::RANGE) {
        update();
        return;
    }

    // TODO: Quite a lot of nesting - should clean this up...
    if (state_ == SelState::RANGE && planned_tick1_ != Fraction(-1, 1) && planned_tick2_ != Fraction(-1, 1)) {
        const staff_idx_t staff_start = staff_start_;
        const staff_idx_t staff_end = staff_end_;
        Box *start_box = start_box_;
        Box *end_box = end_box_;

        deselect_all();

        Segment *s1 = nullptr;
        Segment *s2 = nullptr;

        if (start_box && end_box && planned_tick1_ == planned_tick2_) {
            // Box-only range...
            if (planned_tick1_ == score_->end_tick()) {
                s1 = score_->last_segment_mm();
            } else {
                s1 = score_->tick2segment_mm(planned_tick1_, true, SegmentType::DURATION);
                s2 = s1;
            }
        } else {
            s1 = score_->tick2segment_mm(planned_tick1_);
            s2 = score_->tick2segment_mm(planned_tick2_, /* first */ true /* HACK */);
            if (s2 && s2->measure()->is_mm_rest()) {
                s2 = s2->prev1_mm(); // HACK
            }
            // These hacks are needed to prevent https://musescore.org/node/173381.
            // This should exclude any segments belonging to MM-rest range from the selection.
            if (s1 && s2 && s1->tick() + s1->ticks() > s2->tick()) {
                // can happen with MM rests as tick2measure returns only the first segment for them.

                planned_tick1_ = Fraction(-1, 1);
                planned_tick2_ = Fraction(-1, 1);
                return;
            }

            if (s2 && s2 == s2->measure()->first()) {
                // we want the last segment of the previous measure (unless it's part of a MMrest)
                Measure *prev_measure = s2->measure()->prev_measure();
                if (!(prev_measure && prev_measure != prev_measure->covering_mm_rest_or_this())) {
                    s2 = s2->prev1();
                }
            }
        }

        set_range(s1, s2, staff_start, staff_end, start_box, end_box);

        planned_tick1_ = Fraction(-1, 1);
        planned_tick2_ = Fraction(-1, 1);
    }

    for (EngravingItem *e : el_) {
        e->set_selected(false);
    }
    el_.clear();

    // assert:
    size_t staves = score_->nstaves();
    if (staff_start_ == muse::nidx || staff_start_ >= staves || staff_end_ == muse::nidx || staff_end_ > staves ||
        staff_start_ >= staff_end_) {
        LOGD("updateSelectedElements: bad staff selection %zu - %zu, staves %zu", staff_start_, staff_end_, staves);
        staff_start_ = 0;
        staff_end_ = 0;
    }
    track_idx_t start_track = staff_start_ * ::mu::engraving::voices;
    track_idx_t end_track = staff_end_ * ::mu::engraving::voices;

    // A nested tuplet is selected only when the range contains all its elements.
    ankerl::unordered_dense::set<Tuplet *> inner_tuplets;

    for (track_idx_t st = start_track; st < end_track; ++st) {
        for (Segment *s = start_segment_; s && (s != end_segment_); s = s->next1_mm()) {
            if (!s->enabled() || s->is_end_bar_line_type()) { // do not select end bar line
                continue;
            }
            for (EngravingItem *e : s->annotations()) {
                if (e->track() != st) {
                    continue;
                }
                if (e->is_fret_diagram()) {
                    FretDiagram *fd = to_fret_diagram(e);
                    if (Harmony *harm = fd->harmony()) {
                        el_.push_back(harm);
                    }
                }
                el_.push_back(e);
            }
            EngravingItem *e = s->element(st);
            if (!e || e->generated() || e->is_time_sig() || e->is_key_sig()) {
                continue;
            }

            if (!e->is_chord_rest()) {
                el_.push_back(e);
                continue;
            }

            ChordRest *cr = to_chord_rest(e);
            if (Tuplet *tuplet = cr->tuplet()) {
                inner_tuplets.emplace(tuplet);
            }

            append_chord_rest(cr);
        }
    }

    for (Tuplet *tuplet : inner_tuplets) {
        append_tuplet_hierarchy(tuplet);
    }

    const Fraction range_start = tick_start();
    const Fraction range_end = tick_end();

    //! NOTE: Ties are NOT handled in here - these are note anchored and handled in appendChordRest...
    for (auto i = score_->spanner().begin(); i != score_->spanner().end(); ++i) {
        Spanner *sp = (*i).second;
        // ignore spanners belonging to other tracks
        if (sp->track() < start_track || sp->track() >= end_track) {
            continue;
        }

        const bool start_and_end_calculated = sp->start_element() && sp->end_element();
        if (!start_and_end_calculated || sp->is_volta()) {
            continue;
        }

        //! NOTE: isZeroLengthAtStart (for lack of a better name) covers an edge case that may occur if sp spans from
        //! a grace note to its parent note or vice versa. In this scenario sp->tick == sp->tick2 == rangeStart and
        //! we should include the spanner in our selection...
        const bool overlaps_selection_range = range_start < sp->tick2() && range_end > sp->tick();
        const bool is_zero_length_at_start = sp->tick() == sp->tick2() && sp->tick() == range_start;
        if (!overlaps_selection_range && !is_zero_length_at_start) {
            continue;
        }

        for (auto seg : sp->spanner_segments()) {
            el_.push_back(seg);
        }
    }
    update();
    score_->set_selection_changed(true);
}

void Selection::set_start_segment(Segment *s) {
    start_segment_ = s;
    validate_boxes_and_segments();
}

void Selection::set_end_segment(Segment *s) {
    end_segment_ = s;
    validate_boxes_and_segments();
}

void Selection::validate_boxes_and_segments() {
    if (start_box_) {
        IF_ASSERT_FAILED(start_segment_ && start_segment_->tick() == start_box_->tick()) { start_box_ = nullptr; }
    }

    if (end_box_) {
        const Fraction end_tick = end_segment_ ? end_segment_->tick() : score_->end_tick();
        IF_ASSERT_FAILED(end_tick == end_box_->tick()) { end_box_ = nullptr; }
    }
}

void Selection::set_range(Segment *start_segment, Segment *end_segment, staff_idx_t staff_start, staff_idx_t staff_end, Box *start_box,
                          Box *end_box) {
    assert(staff_end > staff_start && staff_end <= score_->nstaves());
    assert(!(end_segment && !start_segment));

    start_segment_ = start_segment;
    end_segment_ = end_segment;
    start_box_ = start_box;
    end_box_ = end_box;
    validate_boxes_and_segments();

    active_segment_ = end_segment;
    staff_start_ = staff_start;
    staff_end_ = staff_end;
    active_track_ = staff2track(staff_start);

    if (state_ == SelState::RANGE) {
        score_->set_selection_changed(true);
    } else {
        set_state(SelState::RANGE);
    }
}

void Selection::update() {
    for (EngravingItem *e : el_) {
        e->set_selected(true);
    }

    refresh_state();
}

bool Selection::range_is_valid() const {
    if (!is_range()) {
        return false;
    }

    //! NOTE: startSegment_ being non-null and endSegment_ being null is a
    //! valid case. It means we've selected the last segment of the final measure...
    if (!start_segment_) {
        return false;
    }

    if (end_segment_ && end_segment_->tick() <= start_segment_->tick()) {
        const bool is_box_only_range = start_box_ && end_box_ && tick_start() == tick_end() && start_box_->is_before_or_equal(end_box_);
        if (!is_box_only_range) {
            return false;
        }
    }

    const size_t total_staves = score_->nstaves();
    if (staff_start_ == muse::nidx || staff_start_ >= total_staves) {
        return false;
    }
    if (staff_end_ == muse::nidx || staff_end_ > total_staves) {
        return false;
    }

    return staff_start_ < staff_end_;
}

void Selection::refresh_state() {
    if (el_.empty() && !range_is_valid()) {
        set_state(SelState::NONE);
    } else if (state_ == SelState::NONE) {
        set_state(SelState::LIST);
    }
}

void Selection::set_state(SelState s) {
    if (state_ == s) {
        return;
    }

    state_ = s;
    score_->set_selection_changed(true);
}

std::vector<EngravingItem *> Selection::elements(ElementType type) const {
    std::vector<EngravingItem *> result;

    for (EngravingItem *element : el_) {
        if (element->type() == type) {
            result.push_back(element);
        }
    }

    return result;
}

//    Extends the range selection to contain the given
//    segment. SegAfter should represent the segment
//    that is after seg. Tick and etick represent
//    the start and end tick of an element. Useful when
//    extending by a chord rest.

void Selection::extend_range_selection(Segment *seg, Segment *seg_after, staff_idx_t staff_idx, const Fraction &tick, const Fraction &etick,
                                       Box *box) {
    bool active_segment_is_start = false;
    staff_idx_t active_staff = active_track_ / ::mu::engraving::voices;

    // TODO: Quite a lot of nesting - should clean this up...
    if (!box) {
        if (staff_idx < staff_start_) {
            staff_start_ = staff_idx;
        } else if (staff_idx >= staff_end_) {
            staff_end_ = staff_idx + 1;
        } else if (staff_end_ - staff_start_ > 1) {                              // at least 2 staff selected
            if (staff_idx == staff_start_ + 1 && active_staff == staff_start_) { // going down
                staff_start_ = staff_idx;
            } else if (staff_idx == staff_end_ - 2 && active_staff == staff_end_ - 1) { // going up
                staff_end_ = staff_idx + 1;
            }
        }
    }

    // start/end tick may not have changed, but a Box at the same tick may still lie outside the range...
    bool extends_backwards = tick < tick_start();
    bool extends_forwards = etick > tick_end();
    if (box) {
        if (box->tick() == tick_start()) {
            extends_backwards = !start_box_ || box->is_before(start_box_);
        }
        if (box->tick() == tick_end()) {
            extends_forwards = !end_box_ || end_box_->is_before(box);
        }
    }

    if (extends_backwards) {
        // Range extends backwards...
        start_segment_ = seg;
        start_box_ = box;
        active_segment_is_start = true;
    } else if (extends_forwards) {
        // Range extends forwards...
        end_segment_ = seg_after;
        end_box_ = box;
    } else {
        if (active_segment_ == start_segment_) {
            start_segment_ = seg;
            start_box_ = box;
            active_segment_is_start = true;
        } else {
            end_segment_ = seg_after;
            end_box_ = box;
        }
    }
    validate_boxes_and_segments();

    active_segment_ = active_segment_is_start ? start_segment_ : end_segment_;
    score_->set_selection_changed(true);
    assert(!(end_segment_ && !start_segment_));
}
