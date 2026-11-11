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
// Selection
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
    current_tick_ = Fraction(-1, 1);
    current_track_ = 0;
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

bool Selection::is_start_active() const { return active_segment() && active_segment()->tick() == tick_start(); }

bool Selection::is_end_active() const { return active_segment() && active_segment()->tick() == tick_end(); }

EngravingItem *Selection::element() const { return ((state() != SelState::RANGE) && (el_.size() == 1)) ? el_[0] : 0; }

ChordRest *Selection::cr() const {
    EngravingItem *e = element();
    if (!e) {
        return 0;
    }
    if (e->is_note()) {
        e = to_note(e)->chord();
    }
    if (e->is_chord_rest()) {
        return to_chord_rest(e);
    }
    return 0;
}

ChordRest *Selection::current_cr() const {
    // no selection yet - start at very beginning, not first cr
    if (current_tick_ == Fraction(-1, 1)) {
        return nullptr;
    }
    Segment *s = score()->tick2right_segment(current_tick_, true);
    if (!s) {
        return nullptr;
    }
    track_idx_t track = current_track_;
    // staff may have been removed - start at top
    if (track >= score()->ntracks()) {
        track = 0;
    }
    EngravingItem *e = s->element(track);
    if (e && e->is_chord_rest()) {
        return to_chord_rest(e);
    } else {
        return nullptr;
    }
}

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

Measure *Selection::find_measure() const {
    Measure *m = 0;
    if (el_.size() > 0) {
        EngravingItem *el = el_[0];
        m = to_measure(el->find_measure());
    }
    return m;
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

MeasureBase *Selection::end_measure_base() const {
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

    if (end_box_) {
        return end_box_;
    }

    if (tick_end().negative()) { // Tick is not set
        return nullptr;
    }

    MeasureBase *mb = score_->tick2measure_base(tick_end() - Fraction::eps());
    if (mb && mb->is_measure()) {
        mb = to_measure(mb)->covering_mm_rest_or_this();
    }
    return mb;
}

std::vector<System *> Selection::selected_systems() const {
    EngravingItem *el = element();
    if (el && (el->is_system_lock_indicator() || el->is_page_lock_indicator() /*TODO: || el->isStaffVisibilityIndicator*/)) {
        return {const_cast<System *>(to_indicator_icon(el)->system())};
    }

    const MeasureBase *start_mb = start_measure_base();
    const MeasureBase *end_mb = end_measure_base();
    if (!start_mb || !end_mb) {
        return {};
    }

    std::vector<System *> systems;
    for (const MeasureBase *mb = start_mb; mb && mb->is_before_or_equal(end_mb); mb = mb->next_mm()) {
        if (!mb->is_measure() && !mb->is_h_box()) {
            continue;
        }
        System *sys = mb->system();
        IF_ASSERT_FAILED(sys) { continue; }
        if (systems.empty() || sys != systems.back()) {
            systems.push_back(sys);
        }
    }

    return systems;
}

std::vector<Page *> mu::engraving::Selection::pages_containing_selection() const {
    EngravingItem *el = element();
    if (el && (el->is_page_lock_indicator())) {
        return {const_cast<Page *>(to_page_lock_indicator(el)->page())};
    }

    std::vector<System *> systems = selected_systems();
    std::vector<Page *> pages;
    for (System *system : systems) {
        Page *page = system->page();
        if (page && std::find(pages.begin(), pages.end(), page) == pages.end()) {
            pages.push_back(page);
        }
    }

    return pages;
}

void Selection::deselect_all() {
    if (state_ == SelState::RANGE) {
        score_->set_update_all();
    }
    clear();
    update_state();
}

static RectF change_selection(EngravingItem *e, bool b) {
    RectF r = e->canvas_bounding_rect();
    e->set_selected(b);
    r.unite(e->canvas_bounding_rect());
    return r;
}

void Selection::clear() {
    IF_ASSERT_FAILED(!is_locked()) {
        LOGE() << "selection locked, reason: " << lock_reason();
        return;
    }

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
        update_state();
    }
}

void Selection::add(EngravingItem *el) {
    IF_ASSERT_FAILED(!is_locked()) {
        LOGE() << "selection locked, reason: " << lock_reason();
        return;
    }
    el_.push_back(el);
    update();
}

void Selection::append_filtered(EngravingItem *e) {
    IF_ASSERT_FAILED(!is_locked()) {
        LOGE() << "selection locked, reason: " << lock_reason();
        return;
    }
    if (can_select_voice(e->voice()) && can_select(e)) {
        el_.push_back(e);
    }
}

void Selection::append_filtered(const std::unordered_set<EngravingItem *> &elems) {
    IF_ASSERT_FAILED(!is_locked()) {
        LOGE() << "selection locked, reason: " << lock_reason();
        return;
    }

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
                append_filtered(spanner_seg);
            }
            continue;
        }

        // Special handling for grace notes...
        if (elem->is_chord() && to_chord(elem)->is_grace()) {
            append_chord_rest(to_chord_rest(elem));
            continue;
        }

        append_filtered(elem);
    }
}

void Selection::append_chord_rest(ChordRest *cr) {
    IF_ASSERT_FAILED(!is_locked()) {
        LOGE() << "selection locked, reason: " << lock_reason();
        return;
    }

    if (!can_select_voice(cr->voice())) {
        return;
    }

    const std::unordered_set<EngravingItem *> cr_anchored = collect_elements_anchored_to_chord_rest(cr);
    append_filtered(cr_anchored);

    if (cr->is_rest_family()) {
        append_filtered(cr);
        Rest *r = to_rest(cr);
        for (int i = 0; i < r->dots(); ++i) {
            append_filtered(r->dot(i));
        }
        return;
    }

    IF_ASSERT_FAILED(cr->is_chord()) { return; }

    Chord *chord = to_chord(cr);

    const size_t total_notes_in_chord = chord->notes().size();
    const bool is_single_note = total_notes_in_chord == 1;

    size_t total_appended_notes = 0;
    for (size_t note_idx = 0; note_idx < total_notes_in_chord; ++note_idx) {
        Note *note = chord->notes().at(note_idx);

        const std::unordered_set<EngravingItem *> note_anchored = collect_elements_anchored_to_note(note, true, false);
        append_filtered(note_anchored);

        if (chord->is_grace() && !can_select(chord)) {
            continue;
        }

        //! Hack Explainer: Due to the fact that this method is called while we're still in the process of "building" our
        //! selection, we can't know for certain whether the selection as a whole will contain multi-note Chords (and thus
        //! whether the "includeSingleNotes" flag should apply - see includeSingleNotes in select.h). For this reason, this
        //! method ALWAYS appends single note chords. If single note chords should be ommitted from a selection, we simply
        //! don't call this method (see usage of appendChordRest in Selection::updateSelectedElements).
        if (/*hack*/ !is_single_note && !can_select_note_idx(note_idx, total_notes_in_chord, /*hack*/ true)) {
            continue;
        }

        el_.push_back(note);
        ++total_appended_notes;

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
        for (NoteDot *dot : note->dots()) {
            el_.push_back(dot);
        }
    }

    //! NOTE: Beams, stems, etc should only be added if all notes in chord are selected...
    if (total_appended_notes < total_notes_in_chord) {
        return;
    }
    if (chord->beam() && !muse::contains(el_, static_cast<EngravingItem *>(chord->beam()))) {
        el_.push_back(chord->beam());
    }
    if (chord->stem()) {
        el_.push_back(chord->stem());
    }
    if (chord->hook()) {
        el_.push_back(chord->hook());
    }
    if (chord->stem_slash()) {
        el_.push_back(chord->stem_slash());
    }
}

void Selection::append_tuplet_hierarchy(Tuplet *innermost_tuplet) {
    if (!can_select_voice(innermost_tuplet->voice()) || muse::contains(el_, static_cast<EngravingItem *>(innermost_tuplet))) {
        return;
    }

    //! NOTE: Not hugely efficient since canSelectTuplet will recursively check all contained tuplets...
    if (!selection_filter().can_select_tuplet(innermost_tuplet, tick_start(), tick_end(), range_contains_multi_note_chords())) {
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
    if (!guitar_bend || !can_select_voice(guitar_bend->voice())) {
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
    IF_ASSERT_FAILED(!is_locked()) {
        LOGE() << "selection locked, reason: " << lock_reason();
        return;
    }
    if (state_ != SelState::RANGE) {
        range_contains_multi_note_chords_ = false;
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

    //! NOTE: See appendChord/appendChordRest - we should include single notes if the selection consists solely of
    //! single notes, even if the "include single notes" filter flag is false...
    std::vector<Chord *> single_note_chords;
    size_t total_chords_found = 0;

    //! NOTE: We also need to delay the appending of tuplets. We should only display tuplets as selected
    //! if all of their contained elements are selected...
    std::unordered_set<Tuplet *> inner_tuplets;

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
                        append_filtered(harm);
                    }
                }
                append_filtered(e);
            }
            EngravingItem *e = s->element(st);
            if (!e || e->generated() || e->is_time_sig() || e->is_key_sig()) {
                continue;
            }

            if (!e->is_chord_rest()) {
                append_filtered(e);
                continue;
            }

            ChordRest *cr = to_chord_rest(e);
            if (Tuplet *tuplet = cr->tuplet()) {
                inner_tuplets.emplace(tuplet);
            }

            if (!cr->is_chord()) {
                append_chord_rest(cr);
                continue;
            }

            Chord *chord = to_chord(cr);
            const std::vector<Note *> notes = chord->notes();
            if (notes.size() == 1) {
                single_note_chords.emplace_back(chord);
            } else {
                append_chord_rest(chord);
            }
            ++total_chords_found;
        }
    }

    range_contains_multi_note_chords_ = total_chords_found > single_note_chords.size();

    for (Chord *single_note_chord : single_note_chords) {
        if (!range_contains_multi_note_chords_ || selection_filter().include_single_notes()) {
            append_chord_rest(single_note_chord);
        } else {
            // Include elements anchored to the note even if the note itself isn't included...
            const Note *note = single_note_chord->notes().front();
            const std::unordered_set<EngravingItem *> note_anchored = collect_elements_anchored_to_note(note, true, false);
            append_filtered(note_anchored);
            const std::unordered_set<EngravingItem *> cr_anchored = collect_elements_anchored_to_chord_rest(single_note_chord);
            append_filtered(cr_anchored);
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
        if (!start_and_end_calculated || !can_select_voice(sp->track()) || sp->is_volta()) {
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
            append_filtered(seg);
        }
    }
    update();
    score_->set_selection_changed(true);
}

bool Selection::range_contains_multi_note_chords() const {
    IF_ASSERT_FAILED(state_ == SelState::RANGE) { return false; }
    return range_contains_multi_note_chords_;
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

//---------------------------------------------------------
//   setRangeTicks
//    sets the range to be selected on next
//    updateSelectedElements() call. Can be used if some
//    segment structure changes are expected (e.g. if
//    creating MM rests is pending).
//---------------------------------------------------------

void Selection::set_range_ticks(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff_start, staff_idx_t staff_end,
                                Box *start_box, Box *end_box) {
    assert(staff_end > staff_start && staff_end <= score_->nstaves());

    planned_tick1_ = tick1;
    planned_tick2_ = tick2;
    start_segment_ = end_segment_ = active_segment_ = nullptr;
    start_box_ = start_box;
    end_box_ = end_box;
    staff_start_ = staff_start;
    staff_end_ = staff_end;
    active_track_ = staff2track(staff_start);

    if (state_ == SelState::RANGE) {
        score_->set_selection_changed(true);
    } else {
        set_state(SelState::RANGE);
    }
}

//---------------------------------------------------------
//   update
///   Set select flag for all Elements in select list.
//---------------------------------------------------------

void Selection::update() {
    for (EngravingItem *e : el_) {
        e->set_selected(true);
    }

    ChordRest *cr = active_cr();
    if (!cr) {
        update_state();
        return;
    }

    // Only one element can have focus at a time, and currently the final element in el_ has
    // focus. That's ok for a LIST selection (because it corresponds to the last element the
    // user clicked on) but for range selections, we should focus something in activeCR...
    EngravingItem *to_select_again = nullptr;
    if (cr->is_chord()) {
        // Use the top selected note in the chord...
        const std::vector<Note *> notes = to_chord(cr)->notes();
        const size_t note_count = notes.size();
        for (size_t note_idx = note_count - 1; note_idx < note_count; --note_idx) {
            Note *note = notes.at(note_idx);
            if (selection_filter().can_select_note_idx(note_idx, notes.size(), range_contains_multi_note_chords())) {
                to_select_again = note;
                break;
            }
        }
    } else {
        to_select_again = cr;
    }

    if (!to_select_again || !to_select_again->selected()) {
        // Means we have a range with no selected elements...
        update_state();
        return;
    }

    to_select_again->set_selected(true);

    update_state();
}

void Selection::dump() {
    LOGD("Selection dump: ");
    switch (state_) {
        case SelState::NONE: LOGD("NONE"); return;
        case SelState::RANGE: LOGD("RANGE"); break;
        case SelState::LIST: LOGD("LIST"); break;
    }
    for (const EngravingItem *e : el_) {
        LOGD("  %p %s", e, e->type_name());
    }
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

//---------------------------------------------------------
//   updateState
///   update selection state and current position
//---------------------------------------------------------

void Selection::update_state() {
    //! NOTE: Range can be valid even if no elements are selectable in that range...
    if (el_.size() == 0 && !range_is_valid()) {
        set_state(SelState::NONE);
    } else if (state_ == SelState::NONE) {
        set_state(SelState::LIST);
    }

    const EngravingItem *e = element();
    if (!e) {
        return;
    }

    if (e->is_spanner_segment()) {
        current_tick_ = to_spanner_segment(e)->spanner()->tick();
    } else {
        current_tick_ = e->tick();
    }
    // ignore system elements (e.g., frames)
    if (e->track() != muse::nidx) {
        current_track_ = e->track();
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

std::vector<Note *> Selection::note_list(track_idx_t sel_track) const {
    std::vector<Note *> nl;

    if (state_ == SelState::LIST) {
        for (EngravingItem *e : el_) {
            if (e->is_note()) {
                nl.push_back(to_note(e));
            }
        }
    } else if (state_ == SelState::RANGE) {
        for (staff_idx_t staff_idx = staff_start(); staff_idx < staff_end(); ++staff_idx) {
            track_idx_t start_track = staff_idx * ::mu::engraving::voices;
            track_idx_t end_track = start_track + ::mu::engraving::voices;
            for (Segment *seg = start_segment_; seg && seg != end_segment_; seg = seg->next1()) {
                if (!(seg->segment_type() & (SegmentType::CHORD_REST))) {
                    continue;
                }
                for (track_idx_t track = start_track; track < end_track; ++track) {
                    if (!can_select_voice(track)) {
                        continue;
                    }
                    EngravingItem *e = seg->element(track);
                    if (e == 0 || !e->is_chord() || (sel_track != muse::nidx && sel_track != track)) {
                        continue;
                    }
                    Chord *c = to_chord(e);
                    const std::vector<Note *> notes = c->notes();
                    for (size_t note_idx = 0; note_idx < notes.size(); ++note_idx) {
                        Note *note = notes.at(note_idx);
                        if (selection_filter().can_select_note_idx(note_idx, notes.size(), range_contains_multi_note_chords())) {
                            nl.push_back(note);
                        }
                    }
                    for (Chord *g : c->grace_notes()) {
                        nl.insert(nl.end(), g->notes().begin(), g->notes().end());
                    }
                }
            }
        }
    }
    return nl;
}

//---------------------------------------------------------
//   checkStartForPartialCopy
//     return no error if element is NOT a tuplet or is start of a tuplet/tremolo
//     return error if element is part of a tuplet/tremolo, but not the start
//---------------------------------------------------------

static MsError check_start_for_partial_copy(EngravingItem *e) {
    if (!e || !e->is_chord_rest()) {
        return MsError::MS_NO_ERROR;
    }

    const ChordRest *cr = to_chord_rest(e);
    if (cr->tuplet()) {
        // check that complete tuplet is selected, all the way up to top level
        Tuplet *tuplet = cr->tuplet();
        while (tuplet) {
            if (tuplet->elements().front() != e) {
                return MsError::SOURCE_PARTIAL_TUPLET;
            }
            e = tuplet;
            tuplet = tuplet->tuplet();
        }
    }

    if (cr->is_chord()) {
        const Chord *chord = to_chord(cr);
        const TremoloTwoChord *ttc = chord ? chord->tremolo_two_chord() : nullptr;
        if (ttc && ttc->chord2() == chord) {
            return MsError::SOURCE_PARTIAL_TREMOLO;
        }
    }

    return MsError::MS_NO_ERROR;
}

//---------------------------------------------------------
//   checkEndForPartialCopy
//     return no error if element is NOT a tuplet or is end of a tuplet/tremolo
//     return error if element is part of a tuplet/tremolo, but not the end
//---------------------------------------------------------

static MsError check_end_for_partial_copy(EngravingItem *e, const Fraction &end_tick) {
    if (!e || !e->is_chord_rest()) {
        return MsError::MS_NO_ERROR;
    }

    const ChordRest *cr = to_chord_rest(e);
    if (cr->tuplet()) {
        // check that complete tuplet is selected, all the way up to top level
        Tuplet *tuplet = cr->tuplet();
        while (tuplet) {
            if (tuplet->elements().back() != e) {
                return MsError::SOURCE_PARTIAL_TUPLET;
            }
            e = tuplet;
            tuplet = tuplet->tuplet();
        }
        // also check that the selection extends to the end of the top-level tuplet
        tuplet = to_tuplet(e);
        if (tuplet->end_tick() > end_tick) {
            return MsError::SOURCE_PARTIAL_TUPLET;
        }
    }

    if (cr->is_chord()) {
        const Chord *chord = to_chord(cr);
        const TremoloTwoChord *ttc = chord ? chord->tremolo_two_chord() : nullptr;
        if (ttc && ttc->chord1() == chord) {
            return MsError::SOURCE_PARTIAL_TREMOLO;
        }
    }

    return MsError::MS_NO_ERROR;
}

//---------------------------------------------------------
//   canCopy
//    return false if range selection intersects a tuplet
//    or a tremolo, or a local time signature, or only part
//    of a measure repeat group
//---------------------------------------------------------

bool Selection::can_copy() const {
    if (state_ != SelState::RANGE) {
        return true;
    }

    Fraction end_tick = end_segment_ ? end_segment_->tick() : score_->last_segment()->tick();
    Fraction time_stretch(1, 0);

    for (staff_idx_t staff_idx = staff_start_; staff_idx != staff_end_; ++staff_idx) {
        for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
            track_idx_t track = staff_idx * ::mu::engraving::voices + voice;
            if (!can_select_voice(track)) {
                continue;
            }

            // check first cr in track within selection
            ChordRest *check = start_segment_->next_chord_rest(track);
            if (check && check->tick() < end_tick) {
                const MsError err = check_start_for_partial_copy(check);
                if (err != MsError::MS_NO_ERROR) {
                    MScore::set_error(err);
                    return false;
                }
            }

            if (!end_segment_) {
                continue;
            }

            // find last segment in the selection.
            // Note that _endSegment is the first segment after the selection

            Segment *end_segment_selection = start_segment_;
            while (end_segment_selection->next_cr(track) && (end_segment_selection->next_cr(track)->tick() < end_segment_->tick())) {
                end_segment_selection = end_segment_selection->next_cr(track);
            }

            const MsError err = check_end_for_partial_copy(end_segment_selection->element(track), end_tick);
            if (err != MsError::MS_NO_ERROR) {
                MScore::set_error(err);
                return false;
            }
        }

        // Check that all selected measures have the same time stretch - allows copy/paste within a local time signature,
        // but don't yet support it between differing local time signatures.
        for (Measure *m = start_segment_->measure(); m && m->tick() < end_tick; m = m->next_measure()) {
            Fraction m_time_stretch = score_->staff(staff_idx)->time_stretch(m->tick());
            if (!time_stretch.is_valid()) {
                time_stretch = m_time_stretch;
            } else if (time_stretch != m_time_stretch) {
                return false;
            }
        }

        // check if selection starts or ends partway through measure repeat group
        const ChordRest *first_cr = first_chord_rest();
        const ChordRest *last_cr = last_chord_rest();
        if ((first_cr && first_cr->measure()->is_measure_repeat_group_with_prev_m(staff_idx)) ||
            (last_cr && last_cr->measure()->is_measure_repeat_group_with_next_m(staff_idx))) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   measureBaseRange
//    return false if no measure range selected
//---------------------------------------------------------

bool Selection::measure_base_range(MeasureBase **mb1, MeasureBase **mb2) const {
    if (!is_range()) {
        return false;
    }

    if (start_box_) {
        *mb1 = start_box_;
    } else {
        *mb1 = start_segment()->measure();
    }

    Segment *s2 = end_segment();
    if (end_box_) {
        *mb2 = end_box_;
    } else {
        *mb2 = s2 ? s2->measure() : score_->last_measure();
    }

    if (*mb1 == *mb2) {
        return true;
    }

    // if selection extends to last segment of a measure,
    // then endSegment() will point to next measure
    // this won't normally happen because end barlines are excluded from range selection
    // but just in case, detect this and back up one measure
    if (*mb2 && s2) {
        if ((*mb2)->is_measure() && (*mb2)->tick() == s2->tick()) {
            *mb2 = (*mb2)->prev_measure();
        }
    }
    return true;
}

//---------------------------------------------------------
//   uniqueElements
//    Return list of selected elements.
//    If some elements are linked, only one of the linked
//    elements show up in the list.
//---------------------------------------------------------

const std::list<EngravingItem *> Selection::unique_elements() const {
    std::list<EngravingItem *> l;

    for (EngravingItem *e : elements()) {
        bool already_there = false;
        for (EngravingItem *ee : l) {
            if ((ee->links() && ee->links()->contains(e)) || e == ee) {
                already_there = true;
                break;
            }
        }
        if (!already_there) {
            l.push_back(e);
        }
    }
    return l;
}

//---------------------------------------------------------
//   uniqueNotes
//    Return list of selected notes.
//    If some notes are linked, only one of the linked
//    elements show up in the list.
//---------------------------------------------------------

std::vector<Note *> Selection::unique_notes(track_idx_t track, bool tied) const {
    std::vector<Note *> l;

    auto add_note = [&l](Note *note) {
        bool already_there = false;
        for (Note *n : l) {
            if ((n->links() && n->links()->contains(note)) || n == note) {
                already_there = true;
                break;
            }
        }
        if (!already_there) {
            l.push_back(note);
        }
    };

    for (Note *nn : note_list(track)) {
        if (!tied) {
            add_note(nn);
            continue;
        }

        for (Note *note : nn->tied_notes()) {
            add_note(note);
        }
    }
    return l;
}

bool Selection::elements_selected(const ElementTypeSet &types) const {
    for (const EngravingItem *element : el_) {
        if (muse::contains(types, element->type())) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   extendRangeSelection
//    Extends the range selection to contain the given
//    chord rest.
//---------------------------------------------------------

void Selection::extend_range_selection(ChordRest *cr) {
    extend_range_selection(cr->segment(),
                           cr->next_segment_after_cr(SegmentType::CHORD_REST | SegmentType::END_BAR_LINE | SegmentType::CLEF),
                           cr->staff_idx(), cr->tick(), cr->tick());
}

//---------------------------------------------------------
//   extendRangeSelection
//    Extends the range selection to contain the given
//    segment. SegAfter should represent the segment
//    that is after seg. Tick and etick represent
//    the start and end tick of an element. Useful when
//    extending by a chord rest.
//---------------------------------------------------------

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

SelectionFilter Selection::selection_filter() const { return score_->selection_filter(); }

bool Selection::can_select_note_idx(size_t note_idx, size_t total_notes_in_chord, bool selection_contains_multi_note_chords) const {
    return selection_filter().can_select_note_idx(note_idx, total_notes_in_chord, selection_contains_multi_note_chords);
}
