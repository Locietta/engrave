/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include "mmrestlayout.h"

#include "dom/barline.h"
#include "dom/factory.h"
#include "dom/layoutbreak.h"
#include "dom/marker.h"
#include "dom/measure.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"

#include "editing/editmeasures.h"

#include "measurelayout.h"
#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

static const std::unordered_set<ElementType> break_types{
    ElementType::TEMPO_TEXT,
    ElementType::REHEARSAL_MARK,
    ElementType::HARMONY,
    ElementType::STAFF_TEXT,
    ElementType::EXPRESSION,
    ElementType::DYNAMIC,
    ElementType::SYSTEM_TEXT,
    ElementType::TRIPLET_FEEL,
    ElementType::PLAYTECH_ANNOTATION,
    ElementType::CAPO,
    ElementType::INSTRUMENT_CHANGE,
    ElementType::STRING_TUNINGS,
    ElementType::SYMBOL,
    ElementType::FRET_DIAGRAM,
    ElementType::HARP_DIAGRAM,
    ElementType::PLAY_COUNT_TEXT,
    ElementType::FERMATA,
};

static const std::unordered_set<ElementType> always_break_types{ElementType::TEMPO_TEXT, ElementType::REHEARSAL_MARK};

static const std::unordered_set<ElementType> conditional_break_types{
    ElementType::HARMONY,
    ElementType::STAFF_TEXT,
    ElementType::EXPRESSION,
    ElementType::DYNAMIC,
    ElementType::SYSTEM_TEXT,
    ElementType::TRIPLET_FEEL,
    ElementType::PLAYTECH_ANNOTATION,
    ElementType::CAPO,
    ElementType::INSTRUMENT_CHANGE,
    ElementType::STRING_TUNINGS,
    ElementType::SYMBOL,
    ElementType::FRET_DIAGRAM,
    ElementType::HARP_DIAGRAM,
};

/*
    MMRests are measures which overlay existing notation
    When they are created, any item which should be visible is moved from the underlying measure to the MMRest measure.
    Items which are invisible remain in the underlying measure.
    This means that when deciding where in the score to break MMRests, we must check both the underlying measures and any MMRests connected
    to them
*/

void MMRestLayout::reuse_existing_mm_rest(LayoutContext &ctx, Measure *mmr_measure, Measure *last_measure, Fraction len) {
    if (mmr_measure->ticks() == len) {
        MeasureLayout::remove_system_trailer(mmr_measure, ctx);
        return;
    }

    Fraction remaining_mmr_duration = std::min(mmr_measure->ticks() - len, ctx.dom().last_measure()->end_tick() - len);

    Segment *bar_line_seg = mmr_measure->find_segment_r(SegmentType::END_BAR_LINE, mmr_measure->ticks());
    Segment *clef_seg = mmr_measure->find_segment_r(SegmentType::CLEF, mmr_measure->ticks());
    Segment *breath_seg = mmr_measure->find_segment_r(SegmentType::BREATH, mmr_measure->ticks());

    if (!remaining_mmr_duration.negative() && remaining_mmr_duration.is_not_zero()) {
        // Time remaining. Create another mmrest to fill the time
        Measure *next_first_measure = ctx.mut_dom().tick2measure(last_measure->end_tick());
        if (!next_first_measure->mm_rest()) {
            Measure *next_last_measure = ctx.mut_dom().tick2measure(last_measure->end_tick() + remaining_mmr_duration - Fraction::eps());
            next_last_measure = next_last_measure ? next_last_measure : ctx.mut_dom().last_measure();
            const int num_measures_in_new_mm_rest = next_last_measure->measure_index() - next_first_measure->measure_index() + 1;

            Measure *next_mmr_measure = Factory::create_measure(ctx.mut_dom().score());
            next_mmr_measure->set_ticks(remaining_mmr_duration);
            next_mmr_measure->set_tick(next_first_measure->tick());
            ctx.mut_dom().undo(new ChangeMMRest(next_first_measure, next_mmr_measure));

            next_mmr_measure->set_timesig(next_first_measure->timesig());
            next_mmr_measure->set_repeat_count(next_last_measure->repeat_count());
            next_mmr_measure->set_mm_rest_count(num_measures_in_new_mm_rest);
            next_mmr_measure->set_measure_number(next_first_measure->measure_number());

            ctx.mut_dom().update_locks_on_create_mm_rest(next_first_measure, next_last_measure);

            next_mmr_measure->set_repeat_start(next_first_measure->repeat_start() || next_last_measure->repeat_start());
            next_mmr_measure->set_repeat_end(next_first_measure->repeat_end() || next_last_measure->repeat_end());

            if (bar_line_seg) {
                change_elements_parent(bar_line_seg, next_mmr_measure, remaining_mmr_duration, ctx, false);
            }
            if (clef_seg) {
                change_elements_parent(clef_seg, next_mmr_measure, remaining_mmr_duration, ctx, false);
            }
            if (breath_seg) {
                change_elements_parent(breath_seg, next_mmr_measure, remaining_mmr_duration, ctx, false);
            }
        }
    }

    // adjust length
    mmr_measure->set_ticks(len);
    // move existing end barline, clef and breath
    if (bar_line_seg) {
        bar_line_seg->set_rtick(len);
    }
    if (clef_seg) {
        clef_seg->set_rtick(len);
    }
    if (breath_seg) {
        breath_seg->set_rtick(len);
    }
    MeasureLayout::remove_system_trailer(mmr_measure, ctx);
}

//---------------------------------------------------------
//   createMMRest
//    create a multimeasure rest
//    from firstMeasure to lastMeasure (inclusive)
//---------------------------------------------------------

void MMRestLayout::create_mm_rest(LayoutContext &ctx, Measure *first_measure, Measure *last_measure, const Fraction &len) {
    int num_measures_in_mm_rest = 1;
    if (first_measure != last_measure) {
        for (Measure *m = first_measure->next_measure(); m; m = m->next_measure()) {
            ++num_measures_in_mm_rest;
            m->set_mm_rest_count(0);
            if (Measure *old_mm_rest = m->mm_rest()) {
                ctx.mut_dom().undo(new ChangeMMRest(m, nullptr));
                // These MMRests are being removed. Move all elements back to underlying measures.
                // Any elements that need moving back to the MMRest (in lastUnderlyingMeasure) will be handled by changeMeasureElParents below
                Measure *last_underlying_measure = old_mm_rest->mm_rest_last();
                restore_measure_el_parents(m, last_underlying_measure, old_mm_rest, ctx);
            }
            if (m == last_measure) {
                break;
            }
        }
    }

    // mmrMeasure coexists with n undisplayed measures of rests
    Measure *mmr_measure = first_measure->mm_rest();
    if (mmr_measure) {
        reuse_existing_mm_rest(ctx, mmr_measure, last_measure, len);
    } else {
        mmr_measure = Factory::create_measure(ctx.mut_dom().score());
        mmr_measure->set_ticks(len);
        mmr_measure->set_tick(first_measure->tick());
        ctx.mut_dom().undo(new ChangeMMRest(first_measure, mmr_measure));
    }
    mmr_measure->set_timesig(first_measure->timesig());
    mmr_measure->set_repeat_count(last_measure->repeat_count());
    mmr_measure->set_mm_rest_count(num_measures_in_mm_rest);
    mmr_measure->set_measure_number(first_measure->measure_number());

    ctx.mut_dom().update_locks_on_create_mm_rest(first_measure, last_measure);

    mmr_measure->set_repeat_start(first_measure->repeat_start() || last_measure->repeat_start());
    mmr_measure->set_repeat_end(first_measure->repeat_end() || last_measure->repeat_end());

    Segment *chord_rest_seg = mmr_measure->undo_get_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
    for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        track_idx_t track = staff_idx * ::mu::engraving::voices;
        EngravingItem *existing = chord_rest_seg->element(track);
        if (!existing) {
            MMRest *mmr = Factory::create_mm_rest(chord_rest_seg);
            mmr->set_duration_type(DurationType::V_MEASURE);
            mmr->set_ticks(mmr_measure->ticks());
            mmr->set_track(track);
            ctx.mut_dom().do_undo_add_element(mmr);
        } else if (existing->is_mm_rest()) {
            // a reused mmrMeasure may have changed length, so keep its MM rests in sync
            to_mm_rest(existing)->set_ticks(mmr_measure->ticks());
        }
    }

    change_measure_el_parents(first_measure, last_measure, mmr_measure, ctx);

    MeasureBase *nm = ctx.conf().is_show_v_box() ? last_measure->next() : last_measure->next_measure();
    ctx.mut_dom().undo(new ChangeMMRestNext(mmr_measure, nm));
    ctx.mut_dom().undo(new ChangeMMRestPrev(mmr_measure, first_measure->prev()));
}

void MMRestLayout::change_measure_el_parents(Measure *first_measure, Measure *last_measure, Measure *mmr_measure, LayoutContext &ctx) {
    // copy markers & jumps to mmrMeasure
    // jumps come from lastMeasure, markers come from firstMeasure
    ElementList new_list = last_measure->el();
    for (EngravingItem *e : first_measure->el()) {
        if (e->is_marker() && first_measure != last_measure) {
            new_list.push_back(e);
        }
    }
    for (EngravingItem *e : new_list) {
        ctx.mut_dom().undo_change_parent(e, mmr_measure, e->staff_idx(), false);
    }

    // set mmrMeasure with same barline as last underlying measure
    Segment *last_measure_end_barline_seg = last_measure->find_segment_r(SegmentType::END_BAR_LINE, last_measure->ticks());
    change_elements_parent(last_measure_end_barline_seg, mmr_measure, mmr_measure->ticks(), ctx, false);

    // if last underlying measure ends with clef change, show same at end of mmrest
    Segment *last_measure_clef_seg = last_measure->find_segment_r(SegmentType::CLEF | SegmentType::HEADER_CLEF, last_measure->ticks());
    change_elements_parent(last_measure_clef_seg, mmr_measure, mmr_measure->ticks(), ctx, false);

    // further check for clefs
    Segment *underlying_clef_seg = last_measure->find_segment_r(SegmentType::CLEF, last_measure->ticks());
    Segment *mmr_clef_seg = change_elements_parent(underlying_clef_seg, mmr_measure, mmr_measure->ticks(), ctx, false);

    if (underlying_clef_seg && mmr_clef_seg) {
        mmr_clef_seg->set_enabled(underlying_clef_seg->enabled());
        mmr_clef_seg->set_trailer(underlying_clef_seg->trailer());
    }

    // check for time signature
    Segment *underlying_time_sig_seg = first_measure->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
    Segment *mmr_time_sig_seg = change_elements_parent(underlying_time_sig_seg, mmr_measure, Fraction(0, 1), ctx, false);

    if (underlying_time_sig_seg && mmr_time_sig_seg) {
        mmr_time_sig_seg->set_enabled(underlying_time_sig_seg->enabled());
        mmr_time_sig_seg->set_header(underlying_time_sig_seg->header());
    }

    // check for end of measure time signature
    Segment *underlying_end_time_sig_seg = last_measure->find_segment_r(SegmentType::TIME_SIG, last_measure->ticks());
    Segment *mmr_end_time_sig_seg = change_elements_parent(underlying_end_time_sig_seg, mmr_measure, mmr_measure->ticks(), ctx, false);

    if (underlying_end_time_sig_seg && mmr_end_time_sig_seg) {
        mmr_end_time_sig_seg->set_enabled(underlying_end_time_sig_seg->enabled());
        mmr_end_time_sig_seg->set_header(underlying_end_time_sig_seg->header());
        mmr_end_time_sig_seg->set_end_of_measure_change(underlying_end_time_sig_seg->end_of_measure_change());
    }

    // check for end of measure breaths
    Segment *underlying_breath_seg = last_measure->find_segment_r(SegmentType::BREATH, last_measure->ticks());
    change_elements_parent(underlying_breath_seg, mmr_measure, mmr_measure->ticks(), ctx, false);

    // check for ambitus
    Segment *underlying_ambitus_seg = first_measure->find_segment_r(SegmentType::AMBITUS, Fraction(0, 1));
    change_elements_parent(underlying_ambitus_seg, mmr_measure, Fraction(0, 1), ctx, false);

    // check for key signature
    Segment *underlying_key_sig_seg = first_measure->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    Segment *mmr_key_sig_seg = change_elements_parent(underlying_key_sig_seg, mmr_measure, Fraction(0, 1), ctx, false);

    if (underlying_key_sig_seg && mmr_key_sig_seg) {
        mmr_key_sig_seg->set_enabled(underlying_key_sig_seg->enabled());
        mmr_key_sig_seg->set_header(underlying_key_sig_seg->header());
    }

    mmr_measure->check_header();
    mmr_measure->check_trailer();

    // check for rehearsal mark etc.
    Segment *mmr_chord_rest_seg = mmr_measure->undo_get_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
    Segment *underlying_cr_seg = first_measure->find_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
    change_annotations_parent(underlying_cr_seg, mmr_chord_rest_seg, ctx, false);
}

void MMRestLayout::restore_measure_el_parents(Measure *first_measure, Measure *last_measure, Measure *mmr_measure, LayoutContext &ctx) {
    // copy markers & jumps back to underlying measures
    ElementList mmr_measure_els = mmr_measure->el();
    for (EngravingItem *e : mmr_measure_els) {
        Measure *new_measure = e->is_marker() ? first_measure : last_measure;
        ctx.mut_dom().undo_change_parent(e, new_measure, e->staff_idx(), false);
    }

    // restore the barline from mmrMeasure to the last underlying measure
    Segment *mmr_end_barline_seg = mmr_measure->find_segment_r(SegmentType::END_BAR_LINE, mmr_measure->ticks());
    change_elements_parent(mmr_end_barline_seg, last_measure, last_measure->ticks(), ctx, false);

    // restore clef segments from mmrMeasure to the last underlying measure
    Segment *mmr_clef_or_header_seg = mmr_measure->find_segment_r(SegmentType::CLEF | SegmentType::HEADER_CLEF, mmr_measure->ticks());
    change_elements_parent(mmr_clef_or_header_seg, last_measure, last_measure->ticks(), ctx, false);

    Segment *mmr_clef_seg = mmr_measure->find_segment_r(SegmentType::CLEF, mmr_measure->ticks());
    Segment *last_measure_clef_seg = change_elements_parent(mmr_clef_seg, last_measure, last_measure->ticks(), ctx, false);

    if (mmr_clef_seg && last_measure_clef_seg) {
        last_measure_clef_seg->set_enabled(mmr_clef_seg->enabled());
        last_measure_clef_seg->set_trailer(mmr_clef_seg->trailer());
    }

    // restore time signature segments from mmrMeasure to the first underlying measure
    Segment *mmr_time_sig_seg = mmr_measure->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
    Segment *first_measure_time_sig_seg = change_elements_parent(mmr_time_sig_seg, first_measure, Fraction(0, 1), ctx, false);

    if (mmr_time_sig_seg && first_measure_time_sig_seg) {
        first_measure_time_sig_seg->set_enabled(mmr_time_sig_seg->enabled());
        first_measure_time_sig_seg->set_header(mmr_time_sig_seg->header());
    }

    // restore end-of-measure time signature segment from mmrMeasure to the last underlying measure
    Segment *mmr_end_time_sig_seg = mmr_measure->find_segment_r(SegmentType::TIME_SIG, mmr_measure->ticks());
    Segment *last_measure_end_time_sig_seg = change_elements_parent(mmr_end_time_sig_seg, last_measure, last_measure->ticks(), ctx, false);

    if (mmr_end_time_sig_seg && last_measure_end_time_sig_seg) {
        last_measure_end_time_sig_seg->set_enabled(mmr_end_time_sig_seg->enabled());
        last_measure_end_time_sig_seg->set_header(mmr_end_time_sig_seg->header());
        last_measure_end_time_sig_seg->set_end_of_measure_change(mmr_end_time_sig_seg->end_of_measure_change());
    }

    // restore breath segment from mmrMeasure to the last underlying measure
    Segment *mmr_breath_seg = mmr_measure->find_segment_r(SegmentType::BREATH, mmr_measure->ticks());
    change_elements_parent(mmr_breath_seg, last_measure, last_measure->ticks(), ctx, false);

    // restore ambitus segment from mmrMeasure to the first underlying measure
    Segment *mmr_ambitus_seg = mmr_measure->find_segment_r(SegmentType::AMBITUS, Fraction(0, 1));
    change_elements_parent(mmr_ambitus_seg, first_measure, Fraction(0, 1), ctx, false);

    // restore key signature segment from mmrMeasure to the first underlying measure
    Segment *mmr_key_sig_seg = mmr_measure->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    Segment *first_measure_key_sig_seg = change_elements_parent(mmr_key_sig_seg, first_measure, Fraction(0, 1), ctx, false);

    if (mmr_key_sig_seg && first_measure_key_sig_seg) {
        first_measure_key_sig_seg->set_enabled(mmr_key_sig_seg->enabled());
        first_measure_key_sig_seg->set_header(mmr_key_sig_seg->header());
    }

    first_measure->check_header();
    last_measure->check_trailer();

    // restore annotations from the mmr chord/rest segment back to the first underlying measure
    Segment *mmr_chord_rest_seg = mmr_measure->undo_get_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
    Segment *underlying_cr_seg = first_measure->find_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
    change_annotations_parent(mmr_chord_rest_seg, underlying_cr_seg, ctx, false);
}

void MMRestLayout::change_annotations_parent(Segment *old_parent, Segment *new_parent, const LayoutContext &ctx, bool check_visibility) {
    if (!old_parent || !new_parent) {
        return;
    }
    std::vector<EngravingItem *> annotations = old_parent->annotations();
    for (EngravingItem *e : annotations) {
        // look at elements in underlying measure
        if (!muse::contains(::break_types, e->type())) {
            continue;
        }
        if (check_visibility && (!e->visible() || !ctx.dom().staff(e->staff_idx())->show())) {
            continue;
        }

        if (e->is_instrument_change()) {
            /* InstrumentChange items register their Instrument in Part::instruments(), keyed by the tick of
             * the segment they belong to (see Segment::add/remove). Re-parenting one to a segment at a different
             * tick would re-key that entry, making the instrument change take effect at the wrong time: */
            DO_ASSERT_X(old_parent->tick() == new_parent->tick(), "Changing InstrumentChange's parent to a different tick");
        }

        e->score()->undo_change_parent(e, new_parent, e->staff_idx(), false);
    }
}

Segment *MMRestLayout::change_elements_parent(Segment *old_seg, Measure *new_measure, const Fraction &new_seg_tick, LayoutContext &ctx,
                                              bool check_visibility) {
    // Moves elements in oldSeg to newSeg at newSegTick in newMeasure. Creates a new segment if no new seg exists
    if (!old_seg) {
        return nullptr;
    }
    Segment *new_seg = new_measure->undo_get_segment_r(old_seg->segment_type(), new_seg_tick);
    for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        const track_idx_t track = staff2track(staff_idx);
        EngravingItem *el = old_seg->element(track);
        if (!el || !el->visible()) {
            continue;
        }
        if (check_visibility && (!el->visible() || !ctx.dom().staff(staff_idx)->show())) {
            continue;
        }
        ctx.mut_dom().undo_change_parent(el, new_seg, el->staff_idx(), false);
    }

    change_annotations_parent(old_seg, new_seg, ctx, check_visibility);

    return new_seg;
}

//---------------------------------------------------------
// validMMRestMeasure
//    return true if this might be a measure in a
//    multi measure rest
//---------------------------------------------------------

bool MMRestLayout::valid_mm_rest_measure(const LayoutContext &ctx, const Measure *m) {
    if (m->exclude_from_numbering()) {
        return false;
    }

    size_t nstaves = ctx.dom().nstaves();
    for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        if (!ctx.dom().staff(staff_idx)->show()) {
            continue;
        }

        if (m->is_measure_repeat_group(staff_idx)) {
            return false;
        }
    }

    int n = 0;
    for (const Segment *s = m->first(); s; s = s->next()) {
        for (const EngravingItem *e : s->annotations()) {
            if (e->staff_idx() >= nstaves || !e->staff()->show() || !e->visible()) {
                continue;
            }
            if (muse::contains(::break_types, e->type()) && !s->rtick().is_zero()) {
                // play count text and fermatas are permitted at the end of a measure
                if (!e->is_play_count_text() && !e->is_fermata()) {
                    return false;
                }
            }
        }
        size_t tracks = ctx.dom().ntracks();
        if (!s->is_chord_rest_type()) {
            continue;
        }
        bool rest_found = false;
        for (track_idx_t track = 0; track < tracks; ++track) {
            if ((track % ::mu::engraving::voices) == 0 && !ctx.dom().staff(track / ::mu::engraving::voices)->show()) {
                track += ::mu::engraving::voices - 1;
                continue;
            }
            if (!s->element(track)) {
                continue;
            }
            if (!s->element(track)->is_rest()) {
                return false;
            } else {
                bool is_non_empty_irregular = m->is_irregular() && !to_rest(s->element(track))->is_full_measure_rest();
                if (is_non_empty_irregular) {
                    return false;
                }
            }
            rest_found = true;
        }
        for (const EngravingItem *e : s->annotations()) {
            if (e->is_fermata()) {
                return false;
            }
        }
        if (rest_found) {
            ++n;
        }
        // measure is not empty if there is more than one rest
        if (n > 1) {
            return false;
        }
    }
    return true;
}

bool MMRestLayout::break_mmr_for_element(Measure *measure, Measure *prev_measure, const LayoutContext &ctx) {
    auto break_for_annotation = [&](EngravingItem *e) {
        if (muse::contains(::always_break_types, e->type())) {
            return true;
        }
        bool break_for_element = e->system_flag() || e->staff()->show();
        if (muse::contains(::conditional_break_types, e->type()) && break_for_element) {
            return true;
        }
        return false;
    };

    // break for marker in this measure
    for (EngravingItem *e : measure->el()) {
        if (e->is_marker()) {
            Marker *mark = to_marker(e);
            if (!(mark->align() == AlignH::RIGHT)) {
                return true;
            }
        }
    }

    for (Segment *s = measure->first(); s; s = s->next()) {
        for (EngravingItem *e : s->annotations()) {
            if (!e->visible()) {
                continue;
            }
            if (break_for_annotation(e)) {
                return true;
            }
        }
        for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
            if (!ctx.dom().staff(staff_idx)->show()) {
                continue;
            }
            EngravingItem *e = s->element(staff_idx * ::mu::engraving::voices);
            if (!e || e->generated()) {
                continue;
            }
            if (s->is_start_repeat_bar_line_type()) {
                return true;
            }
            if (s->is_type(SegmentType::KEY_SIG | SegmentType::TIME_SIG) && measure->tick().is_not_zero()) {
                return true;
            }
            if (s->is_clef_type()) {
                if (s->tick() != measure->end_tick() && measure->tick().is_not_zero()) {
                    return true;
                }
            }
        }
    }

    if (!prev_measure) {
        return false;
    }

    // break for marker & jump in previous measure
    for (EngravingItem *e : prev_measure->el()) {
        if (e->is_jump()) {
            return true;
        } else if (e->is_marker()) {
            Marker *mark = to_marker(e);
            if (mark->align() == AlignH::RIGHT) {
                return true;
            }
        }
    }

    // Break for breaths and annotations found mid-way into the previous measure
    for (Segment *s = prev_measure->first(); s; s = s->next()) {
        for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
            if (!ctx.dom().staff(staff_idx)->show()) {
                continue;
            }
            EngravingItem *e = s->element(staff_idx * ::mu::engraving::voices);
            if (!e || e->generated()) {
                continue;
            }
            if (s->is_breath_type()) {
                return true;
            }
        }

        for (EngravingItem *e : s->annotations()) {
            if (!e->visible()) {
                continue;
            }
            if (e->rtick() == Fraction(0, 1)) {
                continue;
            }
            if (break_for_annotation(e) || e->is_fermata()) {
                return true;
            }
        }
    }

    // Break for non-normal end barline in previous measure
    Segment *end_bar_seg = prev_measure->find_segment_r(SegmentType::END_BAR_LINE, prev_measure->ticks());
    if (end_bar_seg) {
        for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
            BarLine *bl = to_bar_line(end_bar_seg->element(staff_idx * ::mu::engraving::voices));
            if (!bl) {
                continue;
            }
            BarLineType t = bl->bar_line_type();
            if (t != BarLineType::NORMAL && t != BarLineType::BROKEN && t != BarLineType::DOTTED && !bl->generated()) {
                return true;
            } else {
                break;
            }
        }
    }

    // Break for courtesy clefs/sigs at the end of the previous measure,
    // only if the element is on a visible staff
    auto has_visible_element = [&ctx](Segment *seg) -> bool {
        for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
            if (!ctx.dom().staff(staff_idx)->show()) {
                continue;
            }
            EngravingItem *e = seg->element(staff_idx * ::mu::engraving::voices);
            if (e && !e->generated()) {
                return true;
            }
        }
        return false;
    };

    if (Segment *clef_seg = prev_measure->find_segment(SegmentType::CLEF, measure->tick())) {
        if (has_visible_element(clef_seg)) {
            return true;
        }
    }
    if (Segment *ts_seg = prev_measure->find_segment(SegmentType::TIME_SIG_TYPES, measure->tick())) {
        if (has_visible_element(ts_seg)) {
            return true;
        }
    }
    if (Segment *ks_seg = prev_measure->find_segment(SegmentType::KEY_SIG_TYPES, measure->tick())) {
        if (has_visible_element(ks_seg)) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//  breakMultiMeasureRest
//    return true if this measure should start a new
//    multi measure rest
//---------------------------------------------------------

bool MMRestLayout::break_multi_measure_rest(const LayoutContext &ctx, Measure *m) {
    if (m->break_multi_measure_rest()) {
        return true;
    }

    if (m->repeat_start() || (m->prev_measure() && m->prev_measure()->repeat_end()) || (m->is_irregular()) ||
        (m->prev_measure() && m->prev_measure()->is_irregular()) || (m->prev_measure() && (m->prev_measure()->section_break()))) {
        return true;
    }

    static const std::set<ElementType> break_spanner_types{
        ElementType::VOLTA,
        ElementType::GRADUAL_TEMPO_CHANGE,
        ElementType::TEXTLINE,
    };
    // Break for spanners/textLines in this measure
    auto sl = ctx.dom().spanner_map().find_overlapping(m->tick().ticks(), m->end_tick().ticks());
    for (auto i : sl) {
        Spanner *s = i.value;
        if (!s->visible()) {
            continue;
        }
        Fraction spanner_start = s->tick();
        Fraction spanner_end = s->tick2();
        Fraction measure_start = m->tick();
        Fraction measure_end = m->end_tick();
        bool spanner_starts_inside = spanner_start >= measure_start && spanner_start < measure_end;
        bool spanner_ends_inside = spanner_end >= measure_start && spanner_end < measure_end;
        if (muse::contains(break_spanner_types, s->type()) && (spanner_starts_inside || spanner_ends_inside)) {
            return true;
        }
    }
    // Break for spanners/textLines starting or ending mid-way inside the *previous* measure
    Measure *prev_meas = m->prev_measure();
    if (prev_meas) {
        auto prev_meas_spanners = ctx.dom().spanner_map().find_overlapping(prev_meas->tick().ticks(), prev_meas->end_tick().ticks());
        for (auto i : prev_meas_spanners) {
            Spanner *s = i.value;
            if (!s->visible()) {
                continue;
            }
            Fraction spanner_start = s->tick();
            Fraction spanner_end = s->tick2();
            Fraction measure_start = prev_meas->tick();
            Fraction measure_end = prev_meas->end_tick();
            bool spanner_starts_inside = spanner_start > measure_start && spanner_start < measure_end;
            bool spanner_ends_inside = spanner_end > measure_start && spanner_end < measure_end;
            if (muse::contains(break_spanner_types, s->type()) && (spanner_starts_inside || spanner_ends_inside)) {
                return true;
            }
        }
    }

    bool break_rest = break_mmr_for_element(m, prev_meas, ctx);

    Measure *mmrest = m->mm_rest();
    Measure *mmrest_prev = mmrest ? m->mm_rest()->prev_measure_mm() : nullptr;

    // If we are creating MMRests from expanded notation, checking underlying measures is sufficient.
    // If we are updating MMRests which are already being shown (any layout update) we must also check that the elements dictating their boundaries are still present
    // This is because elements are moved between underlying and covering measures
    if (mmrest && (mmrest->mm_rest_first() == m || mmrest->mm_rest_last() == m)) {
        break_rest |= break_mmr_for_element(mmrest, mmrest_prev, ctx);
    }

    return break_rest;
}

void MMRestLayout::create_multi_measure_rests_if_need(Measure *first_measure, LayoutContext &ctx) {
    LAYOUT_CALL() << LAYOUT_ITEM_INFO(first_measure);

    if (ctx.dom().nstaves() == 0) {
        return;
    }

    int mn = ctx.state().measure_number();

    if (ctx.conf().style_b(Sid::CREATE_MULTI_MEASURE_RESTS)) {
        Measure *measure_to_be_checked = first_measure;
        Measure *last_measure = measure_to_be_checked;
        int n = 0;
        Fraction len;

        while (valid_mm_rest_measure(ctx, measure_to_be_checked)) {
            if (n && break_multi_measure_rest(ctx, measure_to_be_checked)) {
                break;
            }
            if (measure_to_be_checked != first_measure) {
                int measure_number = MeasureLayout::adjust_measure_number(measure_to_be_checked, ctx.state().measure_number());
                ctx.mut_state().set_measure_number(measure_number);
            }
            ++n;
            len += measure_to_be_checked->ticks();
            last_measure = measure_to_be_checked;
            MeasureBase *next_measure_base =
                ctx.conf().is_show_v_box() ? measure_to_be_checked->next() : measure_to_be_checked->next_measure();
            if (!(next_measure_base && next_measure_base->is_measure())) {
                break;
            }
            measure_to_be_checked = to_measure(next_measure_base);
        }

        if (n >= ctx.conf().style_i(Sid::MIN_EMPTY_MEASURES)) {
            create_mm_rest(ctx, first_measure, last_measure, len);
            ctx.mut_state().set_cur_measure(first_measure->mm_rest());
            ctx.mut_state().set_next_measure(ctx.conf().is_show_v_box() ? last_measure->next() : last_measure->next_measure());
        } else {
            if (first_measure->mm_rest()) {
                remove_mm_rest_elements(first_measure->mm_rest(), ctx);
                ctx.mut_dom().undo(new ChangeMMRest(first_measure, nullptr));
            }
            ctx.mut_state().set_measure_number(mn);
        }
    } else if (first_measure->mm_rest()) {
        remove_mm_rest_elements(first_measure->mm_rest(), ctx);

        if (first_measure->mm_rest_count() > 0) {
            LOGD("mmrest: measureNumber %d += %d", ctx.state().measure_number(), first_measure->mm_rest_count());
            int measure_number = ctx.state().measure_number() + first_measure->mm_rest_count() - 1;
            ctx.mut_state().set_measure_number(measure_number);
        }
    }
}

void MMRestLayout::remove_mm_rest_elements(Measure *mmr_measure, LayoutContext &ctx) {
    // Move elements from MMRest measure back to underlying
    Measure *first_measure = mmr_measure->mm_rest_first();
    Measure *last_measure = mmr_measure->mm_rest_last();
    IF_ASSERT_FAILED(first_measure && last_measure) { return; }

    restore_measure_el_parents(first_measure, last_measure, mmr_measure, ctx);
}

void MMRestLayout::layout_mm_rest_range(Measure *m, LayoutContext &ctx) {
    if (!m->is_mm_rest() || !ctx.conf().style_b(Sid::MM_REST_SHOW_MEASURE_NUMBER_RANGE)) {
        // Remove existing
        for (unsigned staff_idx = 0; staff_idx < m->mstaves().size(); ++staff_idx) {
            const MStaff *ms = m->mstaves().at(staff_idx);
            MMRestRange *rr = ms->mm_range_text();
            if (rr) {
                ctx.mut_dom().do_undo_remove_element(rr);
            }
        }

        return;
    }

    String s;
    if (m->mm_rest_count() > 1) {
        // middle char is an en dash (not em)
        s = String(u"%1–%2").arg(m->measure_number() + 1).arg(m->measure_number() + m->mm_rest_count());
    } else {
        // If the minimum range to create a mmrest is set to 1,
        // then simply show the measure number as there is no range
        s = String::number(m->measure_number() + 1);
    }

    for (unsigned staff_idx = 0; staff_idx < m->mstaves().size(); ++staff_idx) {
        const MStaff *ms = m->mstaves().at(staff_idx);
        MMRestRange *rr = ms->mm_range_text();
        if (!rr) {
            rr = new MMRestRange(m);
            rr->set_track(staff_idx * ::mu::engraving::voices);
            rr->set_generated(true);
            m->add(rr);
        }
        // setXmlText is reimplemented to take care of brackets
        rr->set_xml_text(s);
        TLayout::layout_mm_rest_range(rr, rr->mutldata(), ctx);
    }
}
