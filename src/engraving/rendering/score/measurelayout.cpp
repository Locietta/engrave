/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#include "measurelayout.h"

#include <cfloat>

#include "arpeggiolayout.h"
#include "beamlayout.h"
#include "chordbracketlayout.h"
#include "chordlayout.h"
#include "dom/ambitus.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/factory.h"
#include "dom/keysig.h"
#include "dom/layoutbreak.h"
#include "dom/lyrics.h"
#include "dom/marker.h"
#include "dom/measure.h"
#include "dom/measurenumber.h"
#include "dom/measurerepeat.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/parenthesis.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/stafflines.h"
#include "dom/system.h"
#include "dom/tie.h"
#include "dom/timesig.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/utils.h"
#include "editing/editkeysig.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "horizontalspacing.h"
#include "layoutcontext.h"
#include "log.h"
#include "mmrestlayout.h"
#include "modifydom.h"
#include "normalization/normalize.h"
#include "parenthesislayout.h"
#include "restlayout.h"
#include "segmentlayout.h"
#include "slurtielayout.h"
#include "tlayout.h"
#include "tremololayout.h"
#include "types/typesconv.h"

using namespace mu::engraving;
using namespace mu::engraving::rtti;
using namespace mu::engraving::rendering::score;

//---------------------------------------------------------
//   layout2
//    called after layout of page
//---------------------------------------------------------

void MeasureLayout::layout2(Measure *item, LayoutContext &ctx) {
    assert(item->system());
    assert(ctx.dom().nstaves() == item->mstaves().size());

    for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        if (Spacer *sp = item->vspacer_down(staff_idx)) {
            TLayout::layout_spacer(sp, ctx);
            double y = item->system()->staff(staff_idx)->y() + sp->staff_offset_y();
            sp->set_pos(item->spatium() * .5, y + sp->staff()->staff_height(item->tick()));
        }
        if (Spacer *sp = item->vspacer_up(staff_idx)) {
            TLayout::layout_spacer(sp, ctx);
            double y = item->system()->staff(staff_idx)->y() + sp->staff_offset_y();
            sp->set_pos(item->spatium() * .5, y - sp->absolute_gap());
        }
    }

    // layout LAYOUT_BREAK elements
    TLayout::layout_base_measure_base(item, item->mutldata(), ctx);

    //---------------------------------------------------
    //    layout cross-staff ties
    //---------------------------------------------------

    auto layout_cross_staff_ties = [](Chord *chord, Measure *item, const Fraction &stick, LayoutContext &ctx) -> void {
        for (Note *note : chord->notes()) {
            Tie *tie_for = note->tie_for();
            Tie *tie_back = note->tie_back();
            LaissezVib *lv = note->laissez_vib();
            if (lv && lv->is_cross_staff()) {
                SlurTieLayout::layout_laissez_vib_chord(chord, ctx);
            }
            if (tie_for && !lv && tie_for->is_cross_staff()) {
                SlurTieLayout::layout_tie_for(tie_for, item->system());
            }
            if (tie_back && tie_back->tick() < stick && tie_back->is_cross_staff()) {
                SlurTieLayout::layout_tie_back(tie_back, item->system(), ctx);
            }
        }
    };

    const Fraction stick = item->system()->measures().front()->tick();
    const size_t tracks = ctx.dom().ntracks();
    static const SegmentType st{SegmentType::CHORD_REST};
    for (track_idx_t track = 0; track < tracks; ++track) {
        if (!ctx.dom().staff(track2staff(track))->show()) {
            track += ::mu::engraving::voices - 1;
            continue;
        }
        for (Segment *seg = item->first(st); seg; seg = seg->next(st)) {
            EngravingItem *element = seg->element(track);
            if (!element || !element->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(element);
            layout_cross_staff_ties(chord, item, stick, ctx);

            for (Chord *grace_chord : chord->grace_notes()) {
                layout_cross_staff_ties(grace_chord, item, stick, ctx);
            }
        }
    }
}

void MeasureLayout::move_to_next_measure(LayoutContext &ctx) {
    LAYOUT_CALL();
    LayoutState &state = ctx.mut_state();

    state.set_prev_measure(state.cur_measure());
    state.set_cur_measure(state.next_measure());
    if (!state.cur_measure()) {
        state.set_next_measure(ctx.conf().is_show_v_box() ? ctx.mut_dom().first() : ctx.mut_dom().first_measure());
    } else {
        MeasureBase *m = ctx.conf().is_show_v_box() ? state.cur_measure()->next() : state.cur_measure()->next_measure();
        state.set_next_measure(m);
    }
}

void MeasureLayout::layout_measure(MeasureBase *current_mb, LayoutContext &ctx) {
    if (!current_mb || !current_mb->is_measure()) {
        return;
    }

    Measure *measure = to_measure(current_mb);

    if (!measure->ldata()->need_layout()) {
        return;
    }

    if (ctx.conf().is_linear_mode() && (measure->tick() < ctx.state().start_tick() || measure->tick() > ctx.state().end_tick())) {
        return;
    }

    if (ctx.dom().all_staves_invisible()) {
        return;
    }

    measure->mutldata()->set_need_layout(false);

    // Semantic repairs have run at the score layout boundary.
    for (const Segment &segment : measure->segments()) {
        for (EngravingItem *item : segment.elist()) {
            if (item && item->is_chord() && item->staff()->show()) ChordLayout::layout_note_lines(to_chord(item));
        }
    }
    ModifyDom::create_stems(measure, ctx);
    if (measure->is_mm_rest()) ModifyDom::sort_measure_segments(measure, ctx);
    // --------------------
    //
    // calculate accidentals and note lines,
    // create stem and set stem direction
    //

    const DomAccessor &dom = ctx.dom();
    const LayoutConfiguration &conf = ctx.conf();

    for (size_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        const Staff *staff = dom.staff(staff_idx);

        track_idx_t start_track = staff_idx * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (!staff->show() && !segment.is_type(SegmentType::TIME_SIG_TYPES)) {
                continue;
            }

            SegmentLayout::layout_measure_independent_elements(segment, start_track, ctx);

            if (!segment.is_just_type(SegmentType::CHORD_REST)) {
                continue;
            }

            //! NOTE Maybe it makes sense to group these methods by chord

            SegmentLayout::set_chord_mag(staff, segment, start_track, end_track, conf);

            SegmentLayout::layout_chord_drumset(staff, segment, start_track, end_track, conf);

            SegmentLayout::compute_chords_up(segment, start_track, end_track, ctx);

            SegmentLayout::layout_chords_stem(segment, start_track, end_track, ctx);
        }
    }

    BeamLayout::create_beams(ctx, measure);

    /* HACK: The real beam layout is computed at much later stage (you can't do the beams until you know
     * horizontal spacing). However, horizontal spacing needs to know stems extensions to avoid collision
     * with stems, and stems extensions depend on beams. Solution: we compute dummy beams here, *before*
     * horizontal spacing. It is pointless for the beams themselves, but it *does* correctly extend the
     * stems, thus allowing to compute horizontal spacing correctly. (M.S.) */
    for (Segment &s : measure->segments()) {
        if (!s.is_chord_rest_type()) {
            continue;
        }
        for (EngravingItem *item : s.elist()) {
            if (!item || !item->is_chord_rest()) {
                continue;
            }
            if (const Staff *staff = ctx.dom().staff(item->v_staff_idx()); staff && staff->show()) {
                BeamLayout::layout_non_cross_beams(to_chord_rest(item), ctx);
            }
        }
    }

    for (staff_idx_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        const Staff *staff = ctx.dom().staff(staff_idx);
        if (!staff->show()) {
            continue;
        }

        for (Segment &segment : measure->segments()) {
            if (segment.is_chord_rest_type()) {
                ChordLayout::layout_chords1(ctx, &segment, staff_idx);
                RestLayout::resolve_vertical_rest_conflicts(ctx, &segment, staff_idx);
                for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
                    ChordRest *cr = segment.cr(staff_idx * ::mu::engraving::voices + voice);
                    if (cr) {
                        for (Lyrics *l : cr->lyrics()) {
                            if (l) {
                                TLayout::layout_lyrics(l, ctx);
                            }
                        }
                    }
                }
            }
        }
    }

    for (Segment &segment : measure->segments()) {
        if (segment.is_breath_type()) {
            for (EngravingItem *e : segment.elist()) {
                if (e && e->is_breath()) {
                    TLayout::layout_item(e, ctx);
                }
            }
        } else if (segment.is_chord_rest_type()) {
            ChordBracketLayout::layout_segment(&segment, ctx);
            for (EngravingItem *e : segment.annotations()) {
                if (e->is_symbol() || e->is_harmony() || e->is_fret_diagram()) {
                    TLayout::layout_item(e, ctx);
                }
            }
        }
    }

    Segment *seg = measure->find_segment_r(SegmentType::START_REPEAT_BAR_LINE, Fraction(0, 1));
    if (measure->repeat_start()) {
        if (!seg) {
            seg = measure->get_segment_r(SegmentType::START_REPEAT_BAR_LINE, Fraction(0, 1));
        }
        bar_lines_set_span(seg, ctx); // this also creates necessary barlines
        for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
            BarLine *b = to_bar_line(seg->element(staff_idx * ::mu::engraving::voices));
            if (b) {
                b->set_bar_line_type(BarLineType::START_REPEAT);
                TLayout::layout_bar_line(b, b->mutldata(), ctx);
            }
        }
    } else if (seg) {
        ctx.mut_dom().undo_remove_element(seg);
    }

    for (Segment &s : measure->segments()) {
        if (s.is_end_bar_line_type()) {
            continue;
        }
        s.create_shapes();
    }

    measure->compute_ticks(); // Must be called *after* Segment::createShapes() because it relies on the
    // Segment::visible() property, which is determined by Segment::createShapes().
}

void MeasureLayout::update_grace_notes(Measure *measure, LayoutContext &ctx) {
    // Attachments are pass-local, but staff shapes survive the context. Rebuild
    // base shapes even when a previous pass has already cleared its borrows.
    for (Segment &segment : measure->segments()) {
        for (track_idx_t track = 0; track < ctx.dom().ntracks(); ++track) {
            EngravingItem *item = segment.pre_appended_item(track);
            if (item && item->is_grace_notes_group()) {
                segment.clear_pre_appended(track);
            }
        }
        segment.create_shapes();
    }

    // Append grace notes to appropriate segment
    for (Segment &s : measure->segments()) {
        if (!s.is_chord_rest_type()) {
            continue;
        }
        for (auto el : s.elist()) {
            if (el && el->is_chord() && !to_chord(el)->grace_notes().empty()) {
                ChordLayout::append_grace_notes(to_chord(el), ctx);
            }
        }
    }

    // Layout grace note groups
    for (Segment &s : measure->segments()) {
        for (track_idx_t track = 0; track < ctx.dom().ntracks(); ++track) {
            EngravingItem *e = s.pre_appended_item(track);
            if (e && e->is_grace_notes_group()) {
                GraceNotesGroup *gng = to_grace_notes_group(e);
                TLayout::layout_grace_notes_group(gng, ctx);
                gng->add_to_shape();
            }
        }
    }
}

void MeasureLayout::get_next_measure(LayoutContext &ctx) {
    LAYOUT_CALL();

    move_to_next_measure(ctx);

    update_ticks_meas_numbers_and_mm_rests(ctx.mut_state().cur_measure(), ctx);

    MeasureBase *mb = ctx.mut_state().cur_measure();
    if (mb && mb->is_measure()) {
        to_measure(mb)->mutldata()->set_need_layout(true);
    }
}

void MeasureLayout::update_ticks_meas_numbers_and_mm_rests(MeasureBase *current_mb, LayoutContext &ctx) {
    IF_ASSERT_FAILED(current_mb == ctx.state().cur_measure()) { return; }

    if (!current_mb) {
        return;
    }

    if (!current_mb->is_measure()) {
        assert(current_mb->tick() == ctx.state().tick());

        const LayoutBreak *layout_break = current_mb->section_break_element();
        if (layout_break && layout_break->start_with_measure_one()) {
            ctx.mut_state().set_measure_number(0);
        }

        return;
    }

    Measure *measure = to_measure(current_mb);

    int measure_number = adjust_measure_number(measure, ctx.state().measure_number());

    ctx.mut_state().set_measure_number(measure_number);

    MMRestLayout::create_multi_measure_rests_if_need(measure, ctx);

    current_mb = ctx.mut_state().cur_measure();
    measure = to_measure(current_mb);

    measure->move_ticks(ctx.state().tick() - measure->tick());

    ctx.mut_state().set_tick(ctx.state().tick() + measure->ticks());
}

//---------------------------------------------------------
//   adjustMeasureNumber
//---------------------------------------------------------

int MeasureLayout::adjust_measure_number(Measure *m, int measure_number) { return normalize_measure_number(*m, measure_number); }

/****************************************************************
 * computePreSpacingItems
 * Computes information that is needed before horizontal spacing.
 * Caution: assumes that the system is known! (which is why we
 * cannot compute this stuff in LayoutMeasure::getNextMeasure().)
 * **************************************************************/
void MeasureLayout::compute_pre_spacing_items(Measure *m, LayoutContext &ctx) {
    // Compute chord properties
    bool is_first_chord_in_measure = true;
    ChordLayout::clear_line_attach_points(m);
    for (Segment &seg : m->segments()) {
        if (!seg.is_chord_rest_type()) {
            continue;
        }
        for (EngravingItem *e : seg.elist()) {
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(e);
            Staff *staff = chord->staff();
            if (staff && !staff->show()) {
                continue;
            }

            ChordLayout::update_line_attach_points(chord, is_first_chord_in_measure, ctx);
            for (Chord *gn : chord->grace_notes()) {
                ChordLayout::update_line_attach_points(gn, false, ctx);
            }
            if (chord->arpeggio()) {
                ArpeggioLayout::clear_accidentals(chord->arpeggio(), ctx);
            }

            ChordLayout::layout_articulations(chord, ctx);
            ChordLayout::check_start_end_slurs(chord, ctx);
            ChordLayout::layout_articulations2(chord, ctx);
            chord->compute_kerning_exceptions();
        }

        for (EngravingItem *annotation : seg.annotations()) {
            if (annotation->is_fermata()) {
                TLayout::layout_item(annotation, ctx);
            }
        }

        ChordBracketLayout::layout_segment(&seg, ctx);
        seg.create_shapes();
        is_first_chord_in_measure = false;
    }
}

void MeasureLayout::layout_staff_lines(Measure *m, LayoutContext &ctx) {
    int staff_idx = 0;
    for (const MStaff *ms : m->mstaves()) {
        if (m->is_cutaway_clef(staff_idx) && (ctx.dom().staff(staff_idx)->cutaway() || !m->visible(staff_idx))) {
            // draw short staff lines for a courtesy clef on a hidden measure
            Segment *clef_seg = m->find_segment_r(SegmentType::CLEF, m->ticks());
            double staff_mag = ctx.dom().staff(staff_idx)->staff_mag(m->tick());
            double partial_width = clef_seg ? m->width() - clef_seg->x() + clef_seg->min_left() +
                                                  ctx.conf().style_absolute(Sid::CLEF_LEFT_MARGIN) * staff_mag :
                                              0.0;
            layout_partial_width(ms->lines(), ctx, m->width(), partial_width / (m->spatium() * staff_mag), true);
        } else {
            // normal staff lines
            TLayout::layout_staff_lines(ms->lines(), ctx);
        }
        staff_idx += 1;
    }
}

void MeasureLayout::layout_play_count_text(Measure *m, LayoutContext &ctx) {
    if (!m->repeat_end()) {
        return;
    }

    Score *score = m->score();
    const std::vector<MStaff *> &measure_staves = m->mstaves();

    for (staff_idx_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
        if (staff_idx >= measure_staves.size()) {
            break;
        }
        track_idx_t track = staff2track(staff_idx);

        Segment *end_bar_seg = m->last(SegmentType::BAR_LINE_TYPES);
        PlayCountText *play_count =
            end_bar_seg ? to_play_count_text(end_bar_seg->find_annotation(ElementType::PLAY_COUNT_TEXT, track, track)) : nullptr;
        if (!play_count) {
            continue;
        }

        String text;
        const int repeat_count = m->repeat_count();
        String default_text =
            TConv::translated_user_name(ctx.conf().style_v(Sid::REPEAT_PLAY_COUNT_PRESET).value<RepeatPlayCountPreset>()).arg(repeat_count);

        switch (play_count->play_count_text_setting()) {
            case AutoCustomHide::AUTO: text = default_text; break;
            case AutoCustomHide::CUSTOM:
                text = play_count->play_count_custom_text();
                if (text.empty()) {
                    text = default_text;
                }
                break;
            case AutoCustomHide::HIDE: play_count->mutldata()->set_is_skip_draw(true); break;
        }

        if (!play_count->cursor()->editing()) {
            play_count->set_xml_text(text);
        }
    }
}

void MeasureLayout::layout_measure_number(Measure *m, LayoutContext &ctx) {
    MeasureNumberPlacement placement_mode = ctx.conf().style_v(Sid::MEASURE_NUMBER_PLACEMENT_MODE).value<MeasureNumberPlacement>();

    String string_num = String::number(m->measure_number() + 1);

    Score *score = m->score();

    const std::vector<MStaff *> &measure_staves = m->mstaves();

    for (staff_idx_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
        if (staff_idx >= measure_staves.size()) {
            break;
        }

        if (m->show_measure_number_on_staff(staff_idx)) {
            MeasureNumber *measure_number = measure_staves[staff_idx]->measure_number();
            if (!measure_number) {
                measure_number = new MeasureNumber(m);
                measure_number->set_track(staff2track(staff_idx));
                measure_number->set_generated(true);
                m->add(measure_number);
            }

            measure_number->set_xml_text(string_num);
            measure_number->set_system_flag(placement_mode != MeasureNumberPlacement::ON_ALL_STAVES);
            TLayout::layout_measure_number(measure_number, measure_number->mutldata(), ctx);
        }
    }
}

MeasureLayout::MeasureStartEndPos MeasureLayout::get_measure_start_end_pos(const Measure *measure, const Segment *first_cr_seg,
                                                                           const staff_idx_t staff_idx, const bool needs_header_exception,
                                                                           const bool modern_mm_rest, const LayoutContext &ctx) {
    if (!measure || !first_cr_seg) {
        return MeasureStartEndPos(0.0, 0.0);
    }

    Segment *s1;
    for (s1 = first_cr_seg->prev_active(); s1 && s1->all_elements_invisible(); s1 = s1->prev_active()) {}
    Segment *s2;

    for (s2 = first_cr_seg->next_active(); s2; s2 = s2->next_active()) {
        if (modern_mm_rest && !s2->is_chord_rest_type() && !s2->is_breath_type() && s2->element(staff_idx * ::mu::engraving::voices)) {
            break;
        }

        if (!modern_mm_rest &&
            (s2->is_clef_repeat_announce_type() || (s2->is_time_sig_repeat_announce_type() && !s2->has_time_sig_above_staves()) ||
             s2->is_key_sig_repeat_announce_type() || s2->is_end_bar_line_type()) &&
            s2->element(staff_idx * ::mu::engraving::voices)) {
            break;
        }
    }

    double x1 = 0.0;
    while (s1) {
        if (!s1->has_time_sig_above_staves() && !s1->all_elements_invisible()) {
            x1 = std::max(x1, s1->x() + s1->min_right());
            if (s1->is_courtesy_segment()) {
                break;
            }
        }
        s1 = s1->prev_active();
    }

    double x2 = s2 ? s2->x() - s2->min_left() : measure->width();

    bool header_exception =
        measure->header() && first_cr_seg->prev() && !first_cr_seg->prev()->is_start_repeat_bar_line_type() && needs_header_exception;
    if (header_exception) { //needs this exception on header bar
        // Set x1 to the imaginary barline located the minimum barline->note distance to the left of the rest's segment
        x1 = first_cr_seg->x() - ctx.conf().style_absolute(Sid::BAR_NOTE_DISTANCE);
    }

    return MeasureStartEndPos(x1, x2);
}

//-----------------------------------------------------------------------------
//  layoutMeasureElements()
//  lays out all the element of a measure
//  LEGACY: this method used to be called stretchMeasure() and was used to
//  distribute the remaining space at the end of a system. That task is now
//  performed elsewhere and only the layout tasks are kept.
//-----------------------------------------------------------------------------

void MeasureLayout::layout_measure_elements(Measure *m, LayoutContext &ctx) {
    //---------------------------------------------------
    //    layout individual elements
    //---------------------------------------------------

    for (Segment &s : m->segments()) {
        if (!s.enabled()) {
            continue;
        }

        // After the rest of the spacing is calculated we position grace-notes-after.
        ChordLayout::reposition_grace_notes_after(&s, ctx.dom().ntracks());

        for (EngravingItem *e : s.elist()) {
            if (!e) {
                continue;
            }
            staff_idx_t staff_idx = e->staff_idx();
            bool modern_mm_rest = e->is_mm_rest() && !to_mm_rest(e)->is_old_style();
            if ((e->is_rest() && to_rest(e)->is_full_measure_rest()) || e->is_mm_rest() || e->is_measure_repeat()) {
                //
                // element has to be centered in free space
                //    x1 - left measure position of free space
                //    x2 - right measure position of free space

                const MeasureStartEndPos measure_start_end =
                    get_measure_start_end_pos(m, &s, staff_idx, e->is_mm_rest(), modern_mm_rest, ctx);

                const double x1 = measure_start_end.x1;
                const double x2 = measure_start_end.x2;

                if (e->is_mm_rest()) {
                    MMRest *mmrest = to_mm_rest(e);
                    // center multimeasure rest
                    double d = ctx.conf().style_absolute(Sid::MULTI_MEASURE_REST_MARGIN);
                    double w = x2 - x1 - 2 * d;
                    MMRest::LayoutData *mmrest_ld = mmrest->mutldata();
                    mmrest_ld->rest_width = w;
                    TLayout::layout_mm_rest(mmrest, mmrest->mutldata(), ctx);
                    mmrest_ld->set_pos_x(x1 - s.x() + d);
                } else if (e->is_measure_repeat() && !(to_measure_repeat(e)->num_measures() % 2)) {
                    // two- or four-measure repeat, center on following barline
                    double measure_width = x2 - s.x() + .5 * (m->style_p(Sid::BAR_WIDTH));
                    e->mutldata()->set_pos_x(measure_width - .5 * e->width());
                    if (to_measure_repeat(e)->num_measures() == 4 && ctx.conf().style_b(Sid::FOUR_MEASURE_REPEAT_SHOW_EXTENDERS)) {
                        TLayout::layout_measure_repeat_extender(to_measure_repeat(e), to_measure_repeat(e)->mutldata(), ctx);
                    }
                } else {
                    // full measure rest or one-measure repeat, center within this measure
                    TLayout::layout_item(e, ctx);
                    Shape sh = e->ldata()->shape();
                    auto sh_el = sh.find_first(ElementType::REST);
                    //! HACK Previously, bbox of rest was used here
                    //! Now we are using a shape, and in the shape we need to find the part related to rest
                    //! But in some cases, the information about what the ShapeElement belongs to will disappear at the moment (item is null),
                    //! in this case, let's just take the first element, it currently corresponds to the rest
                    if (!sh_el) {
                        sh_el = sh.get_first();
                    }
                    if (sh_el) {
                        e->mutldata()->set_pos_x((x2 - x1 - sh_el->width()) * .5 + x1 - s.x() - sh_el->x());
                    }
                }
                s.create_shape(staff_idx); // DEBUG
            } else if (e->is_rest()) {
                e->mutldata()->set_pos_x(0);
            } else if (e->is_chord()) {
                Chord *c = to_chord(e);
                if (c->tremolo_single_chord()) {
                    TremoloLayout::layout(c->tremolo_single_chord(), ctx);
                }

                if (c->tremolo_two_chord()) {
                    TremoloTwoChord *tr = c->tremolo_two_chord();
                    Chord *c1 = tr->chord1();
                    Chord *c2 = tr->chord2();
                    if (c1 && !c1->staff_move() && c2 && !c2->staff_move()) {
                        TremoloLayout::layout(tr, ctx);
                    }
                }

                for (Chord *g : c->grace_notes()) {
                    if (g->tremolo_single_chord()) {
                        TremoloLayout::layout(g->tremolo_single_chord(), ctx);
                    }

                    if (g->tremolo_two_chord()) {
                        TremoloTwoChord *tr = g->tremolo_two_chord();
                        Chord *gc1 = tr->chord1();
                        Chord *gc2 = tr->chord2();
                        if (gc1 && !gc1->staff_move() && gc2 && !gc2->staff_move()) {
                            TremoloLayout::layout(tr, ctx);
                        }
                    }
                }
            } else if (e->is_bar_line()) {
                e->mutldata()->set_pos_y(0.0);
                // for end barlines, x position was set in createEndBarLines
                if (s.segment_type() != SegmentType::END_BAR_LINE) {
                    e->mutldata()->set_pos_x(0.0);
                }
            }
        }
    }
}

void MeasureLayout::bar_lines_set_span(Segment *seg, LayoutContext &ctx) {
    int track = 0;
    for (Staff *staff : ctx.dom().staves()) {
        BarLine *bl = to_bar_line(seg->element(track)); // get existing bar line for this staff, if any
        if (bl) {
            if (bl->generated()) {
                bl->set_span_staff(staff->bar_line_span());
                bl->set_span_from(staff->bar_line_from());
                bl->set_span_to(staff->bar_line_to());
            }
        } else {
            bl = Factory::create_bar_line(seg);
            bl->set_track(track);
            bl->set_generated(true);
            bl->set_span_staff(staff->bar_line_span());
            bl->set_span_from(staff->bar_line_from());
            bl->set_span_to(staff->bar_line_to());
            TLayout::layout_bar_line(bl, bl->mutldata(), ctx);
            ctx.mut_dom().add_element(bl);
        }
        track += ::mu::engraving::voices;
    }
}

//---------------------------------------------------------
//   createEndBarLines
//    actually creates or modifies barlines
//    return the width change for measure
//---------------------------------------------------------

void MeasureLayout::create_end_bar_lines(Measure *m, bool is_last_measure_in_system, LayoutContext &ctx) {
    const size_t nstaves = ctx.dom().nstaves();
    Segment *barline_seg = m->find_segment_r(SegmentType::END_BAR_LINE, m->ticks());
    const Measure *next_measure = m->next_measure();
    double barline_width = 0.0;
    bool section_break_hide_courtesy = false;
    if (LayoutBreak *section_break_element = m->section_break_element()) {
        section_break_hide_courtesy = !section_break_element->show_courtesy();
    }

    m->set_has_courtesy_key_sig(false);
    //  Set flag "hasCourtesyKeySig" if this measure needs a courtesy key sig.
    //  This flag is later used to set a double end bar line and to actually
    //  create the courtesy key sig.

    bool has_key_sig = false;
    bool has_time_sig = false;
    bool has_courtesy_time_sig = false;
    if (next_measure && !section_break_hide_courtesy) {
        //  Don't change barlines at the end of a section break,
        //  and don't create courtesy key/time signatures.
        const bool show_courtesy_key_sig = (is_last_measure_in_system && ctx.conf().style_b(Sid::GEN_COURTESY_KEYSIG)) ||
                                           (m->repeat_jump() && ctx.conf().style_b(Sid::SHOW_COURTESIES_OTHER_JUMPS));

        Fraction tick = m->end_tick();
        for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
            const Staff *staff = ctx.dom().staff(staff_idx);
            const KeySigEvent key1 = staff->key_sig_event(tick - Fraction::eps());
            const KeySigEvent key2 = staff->key_sig_event(tick);
            if (key1 == key2) {
                continue;
            }
            // locate a key sig. in next measure and, if found,
            // check if it has court. sig turned off
            Segment *key_sig_seg = next_measure->find_segment(SegmentType::KEY_SIG, tick);
            if (key_sig_seg) {
                has_key_sig = true;
                KeySig *key_sig = to_key_sig(key_sig_seg->element(staff_idx * ::mu::engraving::voices));
                if (key_sig && !key_sig->show_courtesy()) {
                    continue;
                }
            }
            if (show_courtesy_key_sig) {
                m->set_has_courtesy_key_sig(true);
            }
            break;
        }

        bool show_courtesy_time_sig = (is_last_measure_in_system && ctx.conf().style_b(Sid::GEN_COURTESY_TIMESIG)) ||
                                      (m->repeat_jump() && ctx.conf().style_b(Sid::SHOW_COURTESIES_OTHER_JUMPS));

        Segment *time_sig_seg = next_measure->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
        if (time_sig_seg) {
            for (track_idx_t track = 0; track < nstaves * ::mu::engraving::voices; track += ::mu::engraving::voices) {
                TimeSig *time_sig = to_time_sig(time_sig_seg->element(track));
                if (!time_sig) {
                    continue;
                }
                has_time_sig = true;
                if (time_sig->show_courtesy_sig() && show_courtesy_time_sig) {
                    has_courtesy_time_sig = true;
                }
                break;
            }
        }
    }

    bool next_meas_repeat =
        next_measure && next_measure->repeat_start() && !m->repeat_end() && !is_last_measure_in_system && m->next() == next_measure;

    bool barline_before_repeat_sigs = ctx.conf().style_b(Sid::BARLINE_BEFORE_SIG_CHANGE) && (has_time_sig || has_key_sig);

    if (next_meas_repeat && !barline_before_repeat_sigs) {
        // we may skip barline at end of a measure immediately before a start repeat:
        // next measure is repeat start, this measure is not a repeat end,
        // this is not last measure of system, no intervening frame
        if (!barline_seg) {
            return;
        }
        barline_seg->set_enabled(false);
    } else {
        BarLineType bl_type = next_measure ? BarLineType::NORMAL : BarLineType::END;
        if (!barline_seg) {
            barline_seg = m->get_segment_r(SegmentType::END_BAR_LINE, m->ticks());
        }
        barline_seg->set_enabled(true);

        const bool is_repeat_sig_barline = ctx.conf().style_b(Sid::BARLINE_BEFORE_SIG_CHANGE) && next_meas_repeat;

        const int key_sig_barline_mode = ctx.conf().style_i(Sid::KEY_SIG_COURTESY_BARLINE_MODE);
        const bool before_ks_courtesy =
            key_sig_barline_mode == int(CourtesyBarlineMode::DOUBLE_BEFORE_COURTESY) && m->has_courtesy_key_sig();
        const bool always_double_ks = key_sig_barline_mode == int(CourtesyBarlineMode::ALWAYS_DOUBLE) && has_key_sig;
        const bool before_repeat_ks = is_repeat_sig_barline && has_key_sig && ctx.conf().style_b(Sid::DOUBLE_BARLINE_BEFORE_KEY_SIG);
        if (before_ks_courtesy || always_double_ks || before_repeat_ks) {
            bl_type = BarLineType::DOUBLE;
        }

        const int time_sig_barline_mode = ctx.conf().style_i(Sid::TIME_SIG_COURTESY_BARLINE_MODE);
        const bool before_ts_courtesy = time_sig_barline_mode == int(CourtesyBarlineMode::DOUBLE_BEFORE_COURTESY) && has_courtesy_time_sig;
        const bool always_double_ts = time_sig_barline_mode == int(CourtesyBarlineMode::ALWAYS_DOUBLE) && has_time_sig;
        const bool before_repeat_ts = is_repeat_sig_barline && has_time_sig && ctx.conf().style_b(Sid::DOUBLE_BARLINE_BEFORE_TIME_SIG);

        if (before_ts_courtesy || always_double_ts || before_repeat_ts) {
            bl_type = BarLineType::DOUBLE;
        }

        bool force = false;
        if (m->repeat_end()) {
            bl_type = BarLineType::END_REPEAT;
            force = true;
        }

        for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
            track_idx_t track = staff_idx * ::mu::engraving::voices;
            BarLine *bar_line = to_bar_line(barline_seg->element(track));
            const Staff *staff = ctx.dom().staff(staff_idx);
            if (!bar_line) {
                bar_line = Factory::create_bar_line(barline_seg);
                bar_line->set_track(track);
                bar_line->set_generated(true);
                bar_line->set_span_staff(staff->bar_line_span());
                bar_line->set_span_from(staff->bar_line_from());
                bar_line->set_span_to(staff->bar_line_to());
                bar_line->set_bar_line_type(bl_type);
                ctx.mut_dom().add_element(bar_line);
            } else {
                // do not change bar line type if bar line is user modified
                // and its not a repeat start/end barline (forced)

                if (bar_line->generated()) {
                    bar_line->set_span_staff(staff->bar_line_span());
                    bar_line->set_span_from(staff->bar_line_from());
                    bar_line->set_span_to(staff->bar_line_to());
                    bar_line->set_bar_line_type(bl_type);
                } else if (bar_line->bar_line_type() != bl_type && force) {
                    bar_line->set_bar_line_type(bl_type);
                    bar_line->set_generated(true);
                }
            }

            TLayout::layout_bar_line(bar_line, bar_line->mutldata(), ctx);
            barline_width = std::max(barline_width, bar_line->width());
        }
        // right align within segment
        for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
            track_idx_t track = staff_idx * ::mu::engraving::voices;
            BarLine *bar_line = to_bar_line(barline_seg->element(track));
            if (!bar_line) {
                continue;
            }
            bar_line->mutldata()->move_x(barline_width - bar_line->width());
        }
        barline_seg->create_shapes();
    }

    set_clef_seg_visibility(m, is_last_measure_in_system, ctx);
}

void MeasureLayout::set_clef_seg_visibility(Measure *m, bool is_last_measure_in_system, LayoutContext &ctx) {
    const size_t nstaves = ctx.dom().nstaves();
    bool section_break_hide_courtesy = false;
    if (LayoutBreak *section_break_element = m->section_break_element()) {
        section_break_hide_courtesy = !section_break_element->show_courtesy();
    }
    const Measure *next_measure = m->next_measure();

    Segment *clef_seg = m->find_segment_r(SegmentType::CLEF, m->ticks());
    if (!clef_seg && next_measure) {
        clef_seg = next_measure->find_segment_r(SegmentType::CLEF, Fraction(0, 1));
    }

    if (!clef_seg) {
        return;
    }

    bool visible_clef = false;
    bool clef_found = false;
    for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        if (!ctx.dom().staff(staff_idx)->show()) {
            continue;
        }

        const track_idx_t track = staff_idx * ::mu::engraving::voices;
        Clef *clef = to_clef(clef_seg->element(track));
        if (!clef) {
            continue;
        }

        clef_found = true;

        const bool show_courtesy = ctx.conf().style_b(Sid::GEN_COURTESY_CLEF) && clef->show_courtesy(); // normally show a courtesy clef
        // check if the measure is the last measure of the system or the last measure before a frame
        const bool last_measure = is_last_measure_in_system || (next_measure ? !(m->next() == next_measure) : true);
        if (!next_measure || section_break_hide_courtesy || (last_measure && !show_courtesy)) {
            // hide the courtesy clef in the final measure of a section, or if the measure is the final measure of a system
            // and the score style or the clef style is set to "not show courtesy clef",
            // or if the clef is at the end of the very last measure of the score
            clef->clear();
            clef_seg->create_shape(staff_idx);
        } else {
            TLayout::layout_clef(clef, clef->mutldata(), ctx.conf());
            clef_seg->create_shape(staff_idx);
            visible_clef = true;
        }
    }

    if (visible_clef) { // there is at least one visible clef in the clef segment
        clef_seg->set_visible(true);
    } else if (!visible_clef && clef_found) { // all (courtesy) clefs in the clef segment are not visible
        clef_seg->set_visible(false);
    } else { // should never happen
        LOGD("Clef Segment without Clef elements at tick %d/%d", clef_seg->tick().numerator(), clef_seg->tick().denominator());
        clef_seg->set_visible(false);
        return;
    }
}

void MeasureLayout::set_courtesy_time_sig(Measure *m, const Fraction &ref_sig_tick, const Fraction &courtesy_sig_tick,
                                          const SegmentType courtesy_seg_type, LayoutContext &ctx) {
    // Find original element
    const size_t nstaves = ctx.dom().nstaves();
    const Fraction courtesy_sig_r_tick = courtesy_sig_tick - m->tick();
    const Measure *prev_measure = m->prev_measure_mm();

    const bool is_trailer = courtesy_seg_type == SegmentType::TIME_SIG_ANNOUNCE;
    const bool is_continuation_courtesy = courtesy_seg_type == SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE;

    const Segment *prev_courtesy_segment =
        prev_measure ? prev_measure->find_segment_r(SegmentType::TIME_SIG_REPEAT_ANNOUNCE, prev_measure->ticks()) : nullptr;

    // Cont. courtesies - check for sig at the start of the measure OR end of previous measure if changesBeforeBarline... is on
    const bool check_prev_measure = is_continuation_courtesy && prev_measure &&
                                    ((prev_measure->repeat_end() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_REPEATS)) ||
                                     (prev_measure->repeat_jump() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_OTHER_JUMPS)));
    const Segment *ts_seg_at_courtesy_tick = m->find_segment_r(SegmentType::TIME_SIG, courtesy_sig_r_tick);
    ts_seg_at_courtesy_tick = !ts_seg_at_courtesy_tick && check_prev_measure ?
                                  prev_measure->find_segment_r(SegmentType::TIME_SIG, courtesy_sig_tick - prev_measure->tick()) :
                                  ts_seg_at_courtesy_tick;
    const bool should_show_cont_courtesy = prev_measure && prev_measure->has_courtesy_time_sig();

    Segment *courtesy_sig_seg = m->find_segment_r(courtesy_seg_type, courtesy_sig_r_tick);
    for (track_idx_t track = 0; track < nstaves * ::mu::engraving::voices; track += ::mu::engraving::voices) {
        const Staff *staff = ctx.dom().staff(track2staff(track));
        const TimeSig *actual_time_sig = staff->time_sig(ref_sig_tick);
        const TimeSig *cur_time_sig = staff->time_sig(courtesy_sig_tick - Fraction::eps());
        if (!actual_time_sig || !cur_time_sig) {
            continue;
        }

        // Show repeat courtesy if the sigs are different
        // Show continuation courtesy if there's a repeat courtesy before it
        // Show trailer courtesy only when there's a time sig element in the next bar
        const bool sigs_different = actual_time_sig->sig() != cur_time_sig->sig();
        const bool needs_courtesy = is_continuation_courtesy ?
                                        should_show_cont_courtesy && prev_courtesy_segment && prev_courtesy_segment->element(track) :
                                    is_trailer ? actual_time_sig->tick() == m->end_tick() :
                                                 sigs_different;
        // If there is a real key sig at this tick (in this bar or the previous), don't create a courtesy
        const bool has_sig_at_tick =
            ts_seg_at_courtesy_tick && ts_seg_at_courtesy_tick->enabled() && ts_seg_at_courtesy_tick->element(track);
        // Only show courtesy if its real signature has courtesies enabled
        const bool actual_show_courtesy = actual_time_sig && actual_time_sig->show_courtesy_sig();
        const bool show = actual_show_courtesy && needs_courtesy && !has_sig_at_tick && ctx.conf().style_b(Sid::GEN_COURTESY_TIMESIG);

        if (!courtesy_sig_seg) {
            if (!show) {
                continue;
            }

            // Create a segment if one doesn't already exist
            courtesy_sig_seg = Factory::create_segment(m, courtesy_seg_type, courtesy_sig_r_tick);
            m->add(courtesy_sig_seg);
            m->set_trailer(m->trailer() || is_trailer);
        }
        if (!is_continuation_courtesy) {
            m->set_has_courtesy_time_sig(true);
        }
        courtesy_sig_seg->set_trailer(is_trailer);
        courtesy_sig_seg->set_enabled(true);

        // Find courtesy element or create if it doesn't exist
        TimeSig *courtesy_time_sig = nullptr;
        EngravingItem *time_sig_elem = courtesy_sig_seg->element(track);
        if (time_sig_elem && time_sig_elem->is_time_sig()) {
            courtesy_time_sig = to_time_sig(time_sig_elem);
        }

        // this time sig shouldn't be shown, remove from segment
        if (!show) {
            if (courtesy_time_sig) {
                courtesy_sig_seg->remove(courtesy_time_sig);
            }
            continue;
        }

        if (!courtesy_time_sig) {
            courtesy_time_sig = Factory::create_time_sig(courtesy_sig_seg);
            courtesy_time_sig->set_track(track);
            courtesy_time_sig->set_generated(true);
            courtesy_time_sig->set_is_courtesy(true);
            courtesy_sig_seg->add(courtesy_time_sig);
        }

        // Layout & create shapes
        courtesy_time_sig->set_from(actual_time_sig);
        if (courtesy_time_sig->is_styled(Pid::SCALE)) {
            // If this courtesyTimeSig was previously disabled its scale style may have not been updated
            courtesy_time_sig->set_scale(courtesy_time_sig->property_default(Pid::SCALE).value<ScaleF>());
        }

        TLayout::layout_time_sig(courtesy_time_sig, courtesy_time_sig->mutldata(), ctx);
    }

    if (courtesy_sig_seg &&
        ((!is_continuation_courtesy && !m->has_courtesy_time_sig()) || !ctx.conf().style_b(Sid::GEN_COURTESY_TIMESIG))) {
        // Whole segment shouldn't be shown, remove any existing courtesy signatures
        if (!is_continuation_courtesy) {
            m->set_has_courtesy_time_sig(false);
        }
        if (courtesy_sig_seg) {
            courtesy_sig_seg->set_enabled(false);
        }
    }

    if (courtesy_sig_seg && courtesy_sig_seg->enabled()) {
        if (courtesy_sig_seg->has_elements()) {
            courtesy_sig_seg->create_shapes();
        } else {
            courtesy_sig_seg->set_enabled(false);
            if (!is_continuation_courtesy) {
                m->set_has_courtesy_time_sig(false);
            }
        }
    }
}

void MeasureLayout::set_courtesy_key_sig(Measure *m, const Fraction &ref_sig_tick, const Fraction &courtesy_sig_tick,
                                         const SegmentType courtesy_seg_type, LayoutContext &ctx) {
    // Find original element
    const size_t nstaves = ctx.dom().nstaves();
    const Fraction courtesy_sig_r_tick = courtesy_sig_tick - m->tick();
    const Measure *prev_measure = m->prev_measure_mm();

    const bool is_trailer = courtesy_seg_type == SegmentType::KEY_SIG_ANNOUNCE;
    const bool is_continuation_courtesy = courtesy_seg_type == SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE;

    const Segment *prev_courtesy_segment =
        prev_measure ? prev_measure->find_segment_r(SegmentType::KEY_SIG_REPEAT_ANNOUNCE, prev_measure->ticks()) : nullptr;

    // Cont. courtesies - check for sig at the start of the measure OR end of previous measure if changesBeforeBarline... is on
    const bool check_prev_measure = is_continuation_courtesy && prev_measure &&
                                    ((prev_measure->repeat_end() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_REPEATS)) ||
                                     (prev_measure->repeat_jump() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_OTHER_JUMPS)));
    const Segment *ks_seg_at_courtesy_tick = m->find_segment_r(SegmentType::KEY_SIG, courtesy_sig_r_tick);
    ks_seg_at_courtesy_tick = !ks_seg_at_courtesy_tick && check_prev_measure ?
                                  prev_measure->find_segment_r(SegmentType::KEY_SIG, courtesy_sig_tick - prev_measure->tick()) :
                                  ks_seg_at_courtesy_tick;

    const bool should_show_cont_courtesy = prev_measure && prev_measure->has_courtesy_key_sig();

    Segment *courtesy_sig_seg = m->find_segment_r(courtesy_seg_type, courtesy_sig_r_tick);
    for (track_idx_t track = 0; track < nstaves * ::mu::engraving::voices; track += ::mu::engraving::voices) {
        // Find reference signature
        const Staff *staff = ctx.dom().staff(track2staff(track));
        const Fraction ref_sig_element_tick = staff->current_key_tick(ref_sig_tick);
        const Measure *ref_measure = ctx.dom().tick2measure_mm(ref_sig_element_tick);
        const Segment *actual_key_sig_seg =
            ref_measure ? ref_measure->find_segment_r(SegmentType::KEY_SIG, ref_sig_element_tick - ref_measure->tick()) : nullptr;
        const EngravingItem *el = actual_key_sig_seg ? actual_key_sig_seg->element(track) : nullptr;
        const KeySig *actual_key_sig = el ? to_key_sig(el) : nullptr;

        const KeySigEvent ref_key = staff->key_sig_event(ref_sig_tick);
        // Get info from correct tick for repeats
        // For trailers and pre-repeat courtesies, signatures should be different
        const bool sigs_different = staff->key(m->end_tick() - Fraction::eps()) != ref_key.key();
        const bool needs_courtesy = is_continuation_courtesy ?
                                        should_show_cont_courtesy && prev_courtesy_segment && prev_courtesy_segment->element(track) :
                                        sigs_different;
        // Only show key sig changes on pitched staves
        const bool staff_is_pitched_at_next_meas =
            ctx.dom().last_measure_mm() == m || (m->next_measure_mm() && staff->is_pitched_staff(m->next_measure_mm()->tick()));
        // If there is a real key sig at this tick (in this bar or the previous), don't create a courtesy
        const bool has_sig_at_tick =
            ks_seg_at_courtesy_tick && ks_seg_at_courtesy_tick->enabled() && ks_seg_at_courtesy_tick->element(track);
        // Only show courtesy if its real signature has courtesies enabled
        const bool actual_show_courtesy = actual_key_sig ? actual_key_sig->show_courtesy() : true;
        const bool show = actual_show_courtesy && staff_is_pitched_at_next_meas && needs_courtesy && !has_sig_at_tick &&
                          ctx.conf().style_b(Sid::GEN_COURTESY_KEYSIG);

        if (!courtesy_sig_seg) {
            if (!show) {
                continue;
            }

            // Create a segment if one doesn't already exist
            courtesy_sig_seg = Factory::create_segment(m, courtesy_seg_type, courtesy_sig_r_tick);
            m->add(courtesy_sig_seg);
            m->set_trailer(m->trailer() || is_trailer);
        }
        if (!is_continuation_courtesy) {
            m->set_has_courtesy_key_sig(true);
        }
        courtesy_sig_seg->set_trailer(is_trailer);
        courtesy_sig_seg->set_enabled(true);

        // Find courtesy element or create if it doesn't exist
        KeySig *courtesy_key_sig = nullptr;
        EngravingItem *key_sig_elem = courtesy_sig_seg->element(track);
        if (key_sig_elem && key_sig_elem->is_key_sig()) {
            courtesy_key_sig = to_key_sig(key_sig_elem);
        }

        // this key sig shouldn't be shown, remove from segment
        if (!show) {
            if (courtesy_key_sig) {
                courtesy_sig_seg->remove(courtesy_key_sig);
            }
            continue;
        }

        if (!courtesy_key_sig) {
            courtesy_key_sig = Factory::create_key_sig(courtesy_sig_seg);
            courtesy_key_sig->set_track(track);
            courtesy_key_sig->set_generated(true);
            courtesy_key_sig->set_is_courtesy(true);
            courtesy_sig_seg->add(courtesy_key_sig);
        }
        courtesy_key_sig->set_key_sig_event(ref_key);

        // Layout & create shapes
        TLayout::layout_key_sig(courtesy_key_sig, courtesy_key_sig->mutldata(), ctx.conf());
    }

    if (courtesy_sig_seg && ((!is_continuation_courtesy && !m->has_courtesy_key_sig()) || !ctx.conf().style_b(Sid::GEN_COURTESY_KEYSIG))) {
        // Whole segment shouldn't be shown, remove any existing courtesy signatures
        if (!is_continuation_courtesy) {
            m->set_has_courtesy_key_sig(false);
        }
        if (courtesy_sig_seg) {
            courtesy_sig_seg->set_enabled(false);
        }
    }

    if (courtesy_sig_seg && courtesy_sig_seg->enabled()) {
        if (courtesy_sig_seg->has_elements()) {
            courtesy_sig_seg->create_shapes();
        } else {
            courtesy_sig_seg->set_enabled(false);
            if (!is_continuation_courtesy) {
                m->set_has_courtesy_key_sig(false);
            }
        }
    }
}

void MeasureLayout::set_courtesy_clef(Measure *m, const Fraction &ref_clef_tick, const Fraction &courtesy_clef_tick,
                                      const SegmentType courtesy_seg_type, LayoutContext &ctx) {
    // Find original element
    const size_t nstaves = ctx.dom().nstaves();
    const Fraction courtesy_clef_r_tick = courtesy_clef_tick - m->tick();
    const Measure *prev_measure = m->prev_measure_mm();

    const bool is_continuation_courtesy = courtesy_seg_type == SegmentType::CLEF_START_REPEAT_ANNOUNCE;

    const Segment *prev_courtesy_segment =
        prev_measure ? prev_measure->find_segment_r(SegmentType::CLEF_REPEAT_ANNOUNCE, prev_measure->ticks()) : nullptr;
    const Segment *clef_seg_at_courtesy_tick = m->find_segment_r(SegmentType::CLEF | SegmentType::HEADER_CLEF, courtesy_clef_r_tick);
    clef_seg_at_courtesy_tick = !clef_seg_at_courtesy_tick && prev_measure ?
                                    prev_measure->find_segment_r(SegmentType::KEY_SIG, courtesy_clef_tick - prev_measure->tick()) :
                                    clef_seg_at_courtesy_tick;

    bool should_show_cont_courtesy = prev_measure && prev_measure->has_courtesy_clef();

    Segment *courtesy_clef_seg = m->find_segment_r(courtesy_seg_type, courtesy_clef_r_tick);
    for (track_idx_t track = 0; track < nstaves * ::mu::engraving::voices; track += ::mu::engraving::voices) {
        const Staff *staff = ctx.dom().staff(track2staff(track));
        const Fraction ref_clef_element_tick = staff->current_clef_tick(ref_clef_tick);
        const Measure *ref_measure = ctx.dom().tick2measure_mm(ref_clef_element_tick);
        const Segment *actual_clef_seg = ref_measure ? ref_measure->find_segment_r(SegmentType::CLEF | SegmentType::HEADER_CLEF,
                                                                                   ref_clef_element_tick - ref_measure->tick()) :
                                                       nullptr;
        if (!actual_clef_seg && ref_measure && ref_measure->prev_measure_mm()) {
            // Check previous measure
            Measure *ref_prev_measure = ref_measure->prev_measure_mm();
            actual_clef_seg = ref_prev_measure->find_segment_r(SegmentType::CLEF | SegmentType::HEADER_CLEF,
                                                               ref_clef_element_tick - ref_prev_measure->tick());
        }
        const EngravingItem *el = actual_clef_seg ? actual_clef_seg->element(track) : nullptr;
        const Clef *actual_clef = el ? to_clef(el) : nullptr;

        const ClefType ref_clef = staff->clef(ref_clef_tick);
        // For trailers and pre-repeat courtesies, clefs should be different
        const bool clefs_different = staff->clef(m->end_tick() - Fraction::eps()) != ref_clef;
        const bool needs_courtesy = is_continuation_courtesy ?
                                        should_show_cont_courtesy && prev_courtesy_segment && prev_courtesy_segment->element(track) :
                                        clefs_different;
        // If there is a real clef at this tick (in this bar or the previous), don't create a courtesy
        const bool has_clef_at_tick =
            clef_seg_at_courtesy_tick && clef_seg_at_courtesy_tick->enabled() && clef_seg_at_courtesy_tick->element(track);
        // Only show courtesy if its real clef has courtesies enabled
        const bool actual_show_courtesy = actual_clef && actual_clef->show_courtesy();
        const bool show = actual_show_courtesy && needs_courtesy && !has_clef_at_tick && ctx.conf().style_b(Sid::GEN_COURTESY_CLEF);

        if (!courtesy_clef_seg) {
            if (!show) {
                continue;
            }

            // Create a segment if one doesn't already exist
            courtesy_clef_seg = Factory::create_segment(m, courtesy_seg_type, courtesy_clef_r_tick);
            m->add(courtesy_clef_seg);
            courtesy_clef_seg->set_trailer(false);
        }
        if (!is_continuation_courtesy) {
            m->set_has_courtesy_clef(true);
        }
        courtesy_clef_seg->set_enabled(true);

        // Find courtesy element or create if it doesn't exist
        Clef *courtesy_clef = nullptr;
        EngravingItem *clef_elem = courtesy_clef_seg->element(track);
        if (clef_elem && clef_elem->is_clef()) {
            courtesy_clef = to_clef(clef_elem);
        }

        // this clef shouldn't be shown, remove from segment
        if (!show) {
            if (courtesy_clef) {
                courtesy_clef_seg->remove(courtesy_clef);
            }
            continue;
        }

        if (!courtesy_clef) {
            courtesy_clef = Factory::create_clef(courtesy_clef_seg);
            courtesy_clef->set_track(track);
            courtesy_clef->set_generated(true);
            courtesy_clef->set_small(true);
            courtesy_clef->set_clef_type(actual_clef->clef_type());
            courtesy_clef->set_is_courtesy(true);
            courtesy_clef_seg->add(courtesy_clef);
        }

        // Layout & create shapes
        TLayout::layout_clef(courtesy_clef, courtesy_clef->mutldata(), ctx.conf());
    }

    if (courtesy_clef_seg && (!ctx.conf().style_b(Sid::GEN_COURTESY_CLEF) || (!is_continuation_courtesy && !m->has_courtesy_clef()))) {
        // Whole segment shouldn't be shown, remove any existing courtesy signatures
        if (!is_continuation_courtesy) {
            m->set_has_courtesy_clef(false);
        }
        if (courtesy_clef_seg) {
            courtesy_clef_seg->set_enabled(false);
        }
    }

    if (courtesy_clef_seg && courtesy_clef_seg->enabled()) {
        if (courtesy_clef_seg->has_elements()) {
            courtesy_clef_seg->create_shapes();
        } else {
            courtesy_clef_seg->set_enabled(false);
            if (is_continuation_courtesy) {
                m->set_has_courtesy_clef(false);
            }
        }
    }
}

static void calc_paren_top_bottom(Parenthesis *item, double &top, double &bottom, LayoutContext &ctx) {
    EngravingItem *parent = item->layout_parent();
    const double spatium = item->spatium();
    if (!parent) {
        return;
    }

    if (!parent->ldata()->is_valid()) {
        TLayout::layout_item(parent, ctx);
    }

    RectF bbox = parent->ldata()->bbox();

    top = std::min(top, bbox.top() + parent->pos().y() + spatium / 4);
    bottom = std::max(bottom, bbox.bottom() + parent->pos().y() - spatium / 4);
}

void MeasureLayout::add_repeat_courtesy_parentheses(Measure *m, const bool continuation, LayoutContext &ctx) {
    const SegmentType ts_seg_type = continuation ? SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE : SegmentType::TIME_SIG_REPEAT_ANNOUNCE;
    const SegmentType ks_seg_type = continuation ? SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE : SegmentType::KEY_SIG_REPEAT_ANNOUNCE;
    const SegmentType clef_seg_type = continuation ? SegmentType::CLEF_START_REPEAT_ANNOUNCE : SegmentType::CLEF_REPEAT_ANNOUNCE;

    const Fraction sig_tick = continuation ? Fraction(0, 1) : m->ticks();

    auto el_should_have_parenthesis = [&](const Segment *seg, const track_idx_t track) -> bool {
        if (!seg || !seg->enabled() || track > ctx.dom().ntracks()) {
            return false;
        }

        const Staff *staff = ctx.dom().staff(track2staff(track));
        if (!staff) {
            return false;
        }
        const StaffType *st = staff->staff_type(seg->tick());
        const bool no_timesig = st && seg->is_type(SegmentType::TIME_SIG_TYPES) && !st->gen_timesig();
        const bool no_keysig = st && seg->is_type(SegmentType::KEY_SIG_TYPES) && !st->gen_keysig();
        const bool no_clef = st && seg->is_type(SegmentType::CLEF_TYPES) && !st->gen_clef();
        if (no_timesig || no_keysig || no_clef) {
            return false;
        }

        const EngravingItem *el = seg->element(track);
        return el && el->visible();
    };

    auto time_sig_should_have_own_parentheses = [&](const Segment *seg, const track_idx_t track) -> bool {
        if (!el_should_have_parenthesis(seg, track)) {
            return false;
        }

        const EngravingItem *el = seg ? seg->element(track) : nullptr;
        if (!el || !el->is_time_sig()) {
            return false;
        }

        return ctx.conf().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() != TimeSigPlacement::NORMAL;
    };

    auto create_parentheses_for_segments = [&](Segment *left_seg, Segment *right_seg, track_idx_t track) -> void {
        double top = DBL_MAX;
        double bottom = -DBL_MAX;
        EngravingItem *left_item = left_seg ? left_seg->element(track) : nullptr;
        EngravingItem *right_item = right_seg ? right_seg->element(track) : nullptr;
        Parenthesis *left_paren = nullptr;
        if (left_item) {
            left_item->set_parentheses_mode(left_item->right_paren() ? ParenthesesMode::BOTH : ParenthesesMode::LEFT, true, true);
            left_paren = left_item->left_paren();
        }
        Parenthesis *right_paren = nullptr;
        if (right_item) {
            right_item->set_parentheses_mode(right_item->left_paren() ? ParenthesesMode::BOTH : ParenthesesMode::RIGHT, true, true);
            right_paren = right_item->right_paren();
        }
        bool needs_big_time_sig_adjust = left_seg && right_seg && left_seg == right_seg && left_seg->is_type(SegmentType::TIME_SIG_TYPES) &&
                                         ctx.conf().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() != TimeSigPlacement::NORMAL;
        if ((ctx.conf().style_b(Sid::SMALL_PARENS) || needs_big_time_sig_adjust) && left_paren && right_paren) {
            const double spatium = left_paren->spatium();
            calc_paren_top_bottom(left_paren, top, bottom, ctx);
            calc_paren_top_bottom(right_paren, top, bottom, ctx);

            double height = bottom - top;

            if (!ctx.conf().style_b(Sid::SMALL_PARENS)) {
                top -= 1.25 * spatium;
                height += 2.5 * spatium;
            }

            right_paren->mutldata()->start_y.set_value(top);
            right_paren->mutldata()->height.set_value(height);
            right_paren->mutldata()->mid_point_thickness.set_value(height / 60 * right_paren->ldata()->mag()); // 0.1sp for a height of 6sp
            left_paren->mutldata()->start_y.set_value(top);
            left_paren->mutldata()->height.set_value(height);
            left_paren->mutldata()->mid_point_thickness.set_value(height / 60 * left_paren->ldata()->mag()); // 0.1sp for a height of 6sp

            double mag = std::max(left_paren->ldata()->mag(), right_paren->ldata()->mag());
            left_paren->mutldata()->set_mag(mag);
            right_paren->mutldata()->set_mag(mag);
        } else if (left_paren || right_paren) {
            const Staff *staff = left_paren ? left_paren->staff() : right_paren->staff();
            const double spatium = left_paren ? left_paren->spatium() : right_paren->spatium();
            const Fraction tick = left_paren ? left_paren->tick() : right_paren->tick();
            if (left_paren) {
                left_paren->mutldata()->start_y.set_value(-spatium);
                left_paren->mutldata()->height.set_value(staff->staff_height(tick) + 2 * spatium * left_paren->mag());
                left_paren->mutldata()->mid_point_thickness.set_value(left_paren->ldata()->height / 60 *
                                                                      left_paren->ldata()->mag()); // 0.1sp for a height of 6sp
            }
            if (right_paren) {
                right_paren->mutldata()->start_y.set_value(-spatium);
                right_paren->mutldata()->height.set_value(staff->staff_height(tick) + 2 * spatium * right_paren->mag());
                right_paren->mutldata()->mid_point_thickness.set_value(right_paren->ldata()->height / 60 *
                                                                       right_paren->ldata()->mag()); // 0.1sp for a height of 6sp
            }
        }

        if (left_paren) {
            ParenthesisLayout::layout_parentheses(left_item, ctx);
        }

        if (right_paren && right_item != left_item) {
            ParenthesisLayout::layout_parentheses(right_item, ctx);
        }
    };

    for (track_idx_t track = 0; track <= ctx.dom().nstaves() * ::mu::engraving::voices; track += ::mu::engraving::voices) {
        Segment *clef_seg = m->find_segment_r(clef_seg_type, sig_tick);
        Segment *ks_seg = m->find_segment_r(ks_seg_type, sig_tick);
        Segment *ts_seg = m->find_segment_r(ts_seg_type, sig_tick);
        bool separate_ts_parens = time_sig_should_have_own_parentheses(ts_seg, track);

        Segment *left_most_seg = clef_seg;

        if (!el_should_have_parenthesis(left_most_seg, track)) {
            left_most_seg = ks_seg;
        }

        if (!separate_ts_parens && !el_should_have_parenthesis(left_most_seg, track)) {
            left_most_seg = ts_seg;
        }

        if (!el_should_have_parenthesis(left_most_seg, track)) {
            left_most_seg = nullptr;
        }

        // Remove stale parentheses
        for (Segment *seg : {clef_seg, ks_seg, ts_seg}) {
            if (seg == left_most_seg || !seg || (seg == ts_seg && separate_ts_parens) || !seg->element(track)) {
                continue;
            }
            EngravingItem *item = seg->element(track);
            remove_repeat_courtesy_parenthesis(item, DirectionH::LEFT);
        }

        Segment *right_most_seg = separate_ts_parens ? nullptr : ts_seg;

        if (!el_should_have_parenthesis(right_most_seg, track)) {
            right_most_seg = ks_seg;
        }

        if (!el_should_have_parenthesis(right_most_seg, track)) {
            right_most_seg = clef_seg;
        }

        if (!el_should_have_parenthesis(right_most_seg, track)) {
            right_most_seg = nullptr;
        }

        // Remove stale parentheses
        for (Segment *seg : {clef_seg, ks_seg, ts_seg}) {
            if (seg == right_most_seg || !seg || (seg == ts_seg && separate_ts_parens) || !seg->element(track)) {
                continue;
            }
            EngravingItem *item = seg->element(track);
            remove_repeat_courtesy_parenthesis(item, DirectionH::RIGHT);
        }

        create_parentheses_for_segments(left_most_seg, right_most_seg, track);

        if (separate_ts_parens) {
            create_parentheses_for_segments(ts_seg, ts_seg, track);
        }
    }
}

void MeasureLayout::remove_repeat_courtesy_parentheses_measure(Measure *m, const bool continuation, LayoutContext &ctx) {
    const SegmentType ts_seg_type = continuation ? SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE : SegmentType::TIME_SIG_REPEAT_ANNOUNCE;
    const SegmentType ks_seg_type = continuation ? SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE : SegmentType::KEY_SIG_REPEAT_ANNOUNCE;
    const SegmentType clef_seg_type = continuation ? SegmentType::CLEF_START_REPEAT_ANNOUNCE : SegmentType::CLEF_REPEAT_ANNOUNCE;

    const Fraction sig_tick = continuation ? Fraction(0, 1) : m->ticks();

    Segment *clef_seg = m->find_segment_r(clef_seg_type, sig_tick);
    Segment *ks_seg = m->find_segment_r(ks_seg_type, sig_tick);
    Segment *ts_seg = m->find_segment_r(ts_seg_type, sig_tick);

    for (Segment *seg : {clef_seg, ks_seg, ts_seg}) {
        if (!seg) {
            continue;
        }
        for (track_idx_t track = 0; track <= ctx.dom().nstaves() * ::mu::engraving::voices; track += ::mu::engraving::voices) {
            EngravingItem *item = seg->element(track);
            if (!item) {
                continue;
            }
            remove_repeat_courtesy_parenthesis(item);
        }
    }
}

void MeasureLayout::remove_repeat_courtesy_parenthesis(EngravingItem *item, const DirectionH direction) {
    if (!item) {
        return;
    }
    if (direction == DirectionH::AUTO) {
        if (Parenthesis *left_paren = item->left_paren()) {
            item->remove(left_paren);
        }
        if (Parenthesis *right_paren = item->right_paren()) {
            item->remove(right_paren);
        }
        return;
    }

    Parenthesis *paren = item->paren(direction);
    if (!paren) {
        return;
    }

    item->remove(paren);
}

void MeasureLayout::set_repeat_courtesies_and_parens(Measure *m, LayoutContext &ctx) {
    const bool show_courtesy_repeats = m->repeat_end() && ctx.conf().style_b(Sid::SHOW_COURTESIES_REPEATS);
    const bool show_courtesy_other_jumps = m->repeat_jump() && ctx.conf().style_b(Sid::SHOW_COURTESIES_OTHER_JUMPS);

    if (show_courtesy_repeats) {
        MeasureLayout::add_repeat_courtesies(m, ctx);
        if (ctx.conf().style_b(Sid::USE_PARENS_REPEAT_COURTESIES)) {
            MeasureLayout::add_repeat_courtesy_parentheses(m, false, ctx);
        } else {
            MeasureLayout::remove_repeat_courtesy_parentheses_measure(m, false, ctx);
        }
    }

    if (show_courtesy_other_jumps) {
        MeasureLayout::add_repeat_courtesies(m, ctx);
        if (ctx.conf().style_b(Sid::USE_PARENS_OTHER_JUMP_COURTESIES)) {
            MeasureLayout::add_repeat_courtesy_parentheses(m, false, ctx);
        } else {
            MeasureLayout::remove_repeat_courtesy_parentheses_measure(m, false, ctx);
        }
    }

    if (!show_courtesy_repeats && !show_courtesy_other_jumps) {
        MeasureLayout::remove_repeat_courtesy_parentheses_measure(m, false, ctx);
        remove_repeat_courtesies(m);
    }

    const bool courtesies_after_cancelling_repeats = m->prev_measure_mm() && m->prev_measure_mm()->repeat_end() &&
                                                     ctx.conf().style_b(Sid::SHOW_COURTESIES_AFTER_CANCELLING_REPEATS) &&
                                                     ctx.conf().style_b(Sid::SHOW_COURTESIES_REPEATS);
    const bool courtesies_after_cancelling_other_jumps = m->prev_measure_mm() && m->prev_measure_mm()->repeat_jump() &&
                                                         ctx.conf().style_b(Sid::SHOW_COURTESIES_AFTER_CANCELLING_OTHER_JUMPS) &&
                                                         ctx.conf().style_b(Sid::SHOW_COURTESIES_OTHER_JUMPS);
    if (courtesies_after_cancelling_repeats) {
        MeasureLayout::add_repeat_continuation_courtesies(m, ctx);
        if (ctx.conf().style_b(Sid::USE_PARENS_REPEAT_COURTESIES_AFTER_CANCELLING)) {
            MeasureLayout::add_repeat_courtesy_parentheses(m, true, ctx);
        } else {
            MeasureLayout::remove_repeat_courtesy_parentheses_measure(m, true, ctx);
        }
    }

    if (courtesies_after_cancelling_other_jumps) {
        MeasureLayout::add_repeat_continuation_courtesies(m, ctx);
        if (ctx.conf().style_b(Sid::USE_PARENS_OTHER_JUMP_COURTESIES_AFTER_CANCELLING)) {
            MeasureLayout::add_repeat_courtesy_parentheses(m, true, ctx);
        } else {
            MeasureLayout::remove_repeat_courtesy_parentheses_measure(m, true, ctx);
        }
    }

    if (!courtesies_after_cancelling_repeats && !courtesies_after_cancelling_other_jumps) {
        MeasureLayout::remove_repeat_courtesy_parentheses_measure(m, true, ctx);
        remove_repeat_continuation_courtesies(m);
    }
}

void MeasureLayout::add_repeat_courtesies(Measure *m, LayoutContext &ctx) {
    const std::vector<Measure *> measures = find_following_repeat_measures(m);
    if (measures.empty()) {
        remove_repeat_courtesies(m);
        return;
    }

    bool has_courtesies = false;
    for (Measure *repeat_start_measure : measures) {
        // Follow section break courtesy property
        const Measure *prev_measure = repeat_start_measure->prev_measure_mm();
        const LayoutBreak *section_break = prev_measure ? prev_measure->section_break_element() : nullptr;
        const bool section_break_hide_courtesies = section_break && !section_break->show_courtesy();

        if (repeat_start_measure == m->next_measure_mm() || section_break_hide_courtesies) {
            continue;
        }
        set_courtesy_clef(m, repeat_start_measure->tick(), m->end_tick(), SegmentType::CLEF_REPEAT_ANNOUNCE, ctx);
        set_courtesy_key_sig(m, repeat_start_measure->tick(), m->end_tick(), SegmentType::KEY_SIG_REPEAT_ANNOUNCE, ctx);
        set_courtesy_time_sig(m, repeat_start_measure->tick(), m->end_tick(), SegmentType::TIME_SIG_REPEAT_ANNOUNCE, ctx);

        has_courtesies = true;
    }

    if (!has_courtesies) {
        remove_repeat_courtesies(m);
    }
}

void MeasureLayout::remove_repeat_courtesies(Measure *m) {
    for (Segment *seg = m->last(); seg != m->first(); seg = seg->prev()) {
        if (seg->is_chord_rest_type()) {
            break;
        }
        if (!seg->is_type(SegmentType::CLEF_REPEAT_ANNOUNCE | SegmentType::TIME_SIG_REPEAT_ANNOUNCE |
                          SegmentType::KEY_SIG_REPEAT_ANNOUNCE)) {
            continue;
        }
        seg->mutldata()->reset();

        if (seg->enabled()) {
            seg->set_enabled(false);
        }
    }
    m->set_has_courtesy_clef(false);
    m->set_has_courtesy_key_sig(false);
    m->set_has_courtesy_time_sig(false);
}

void MeasureLayout::add_repeat_continuation_courtesies(Measure *m, LayoutContext &ctx) {
    set_courtesy_time_sig(m, m->tick(), m->tick(), SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE, ctx);
    set_courtesy_key_sig(m, m->tick(), m->tick(), SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE, ctx);
    set_courtesy_clef(m, m->tick(), m->tick(), SegmentType::CLEF_START_REPEAT_ANNOUNCE, ctx);
}

void MeasureLayout::remove_repeat_continuation_courtesies(Measure *m) {
    for (Segment *seg = m->first(); seg != m->last(); seg = seg->next()) {
        if (seg->is_chord_rest_type()) {
            break;
        }
        if (!seg->is_type(SegmentType::CLEF_START_REPEAT_ANNOUNCE | SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE |
                          SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE)) {
            continue;
        }

        seg->mutldata()->reset();

        if (seg->enabled()) {
            seg->set_enabled(false);
        }
    }
}

Segment *MeasureLayout::add_header_clef(Measure *m, bool is_first_clef, const Staff *staff, LayoutContext &ctx) {
    const staff_idx_t staff_idx = staff->idx();
    const track_idx_t track = staff_idx * ::mu::engraving::voices;
    Segment *c_segment = m->find_first_r(SegmentType::HEADER_CLEF, Fraction(0, 1));
    const StaffType *staff_type = staff->staff_type(m->tick());

    const bool hide_clef =
        staff_type->is_tab_staff() ? ctx.conf().style_b(Sid::HIDE_TAB_CLEF_AFTER_FIRST) : !ctx.conf().style_b(Sid::GEN_CLEF);

    // find the clef type at the previous tick
    ClefTypeList cl = staff->clef_type(m->tick() - Fraction::eps());
    bool show_courtesy = true;
    Segment *s = nullptr;
    if (m->prev_measure()) {
        // look for a clef change at the end of the previous measure
        s = m->prev_measure()->find_segment(SegmentType::CLEF, m->tick());
    } else if (m->is_mm_rest()) {
        // look for a header clef at the beginning of the first underlying measure
        s = m->mm_rest_first()->find_first_r(SegmentType::HEADER_CLEF, Fraction(0, 1));
    }
    if (s) {
        Clef *c = to_clef(s->element(track));
        if (c) {
            cl = c->clef_type_list();
            show_courtesy = c->show_courtesy();
        }
    }
    Clef *clef = nullptr;
    if (c_segment) {
        clef = to_clef(c_segment->element(track));
    }
    if (staff->staff_type_for_element(m)->gen_clef() && (is_first_clef || !hide_clef)) {
        if (!c_segment) {
            c_segment = Factory::create_segment(m, SegmentType::HEADER_CLEF, Fraction(0, 1));
            m->add(c_segment);
        }
        c_segment->set_header(true);
        if (!clef) {
            //
            // create missing clef
            //
            clef = Factory::create_clef(c_segment);
            clef->set_track(track);
            clef->set_generated(true);
            clef->set_show_courtesy(show_courtesy);
            c_segment->add(clef);
        }
        if (clef->generated()) {
            clef->set_clef_type(cl);
        }
        clef->set_small(false);
        clef->mutldata()->reset();
        TLayout::layout_clef(clef, clef->mutldata(), ctx.conf());
        c_segment->set_enabled(true);
    } else if (clef) {
        clef->ownership_parent_item()->remove(clef);
        if (clef->generated()) {
            delete clef;
        }
    }

    return c_segment;
}

Segment *MeasureLayout::add_header_key_sig(Measure *m, bool is_first_keysig, const Staff *staff, LayoutContext &ctx) {
    const staff_idx_t staff_idx = staff->idx();
    const track_idx_t track = staff_idx * ::mu::engraving::voices;
    Segment *k_segment = m->find_first_r(SegmentType::KEY_SIG, Fraction(0, 1));
    // If we need a Key::C KeySig (which would be invisible) and there is
    // a courtesy key sig, don’t create it and switch generated flags.
    // This avoids creating an invisible KeySig which can distort layout.

    KeySigEvent key_idx = staff->key_sig_event(m->tick());
    KeySig *ks_announce = 0;
    if ((is_first_keysig || ctx.conf().style_b(Sid::GEN_KEYSIG)) && (key_idx.key() == Key::C)) {
        Measure *pm = m->prev_measure_mm();
        if (pm && pm->has_courtesy_key_sig()) {
            Segment *ks = pm->first(SegmentType::KEY_SIG_ANNOUNCE);
            if (ks) {
                ks_announce = to_key_sig(ks->element(track));
                if (ks_announce) {
                    is_first_keysig = false;
                    //                                    if (keysig) {
                    //                                          ksAnnounce->setGenerated(false);
                    //TODO                                      keysig->setGenerated(true);
                    //                                          }
                }
            }
        }
    }

    bool is_pitched_staff = staff->is_pitched_staff(m->tick());

    KeySig *keysig = nullptr;
    if (k_segment) {
        keysig = to_key_sig(k_segment->element(track));
    }
    // keep key sigs in TABs: TABs themselves should hide them
    if ((is_first_keysig || ctx.conf().style_b(Sid::GEN_KEYSIG)) && is_pitched_staff) {
        if (!k_segment) {
            k_segment = Factory::create_segment(m, SegmentType::KEY_SIG, Fraction(0, 1));
            m->add(k_segment);
        }
        k_segment->set_header(true);
        if (!keysig) {
            //
            // create missing key signature
            //
            keysig = Factory::create_key_sig(k_segment);
            keysig->set_track(track);
            keysig->set_generated(true);
            k_segment->add(keysig);
        }
        keysig->set_key_sig_event(key_idx);
        keysig->mutldata()->reset();
        TLayout::layout_key_sig(keysig, keysig->mutldata(), ctx.conf());
        k_segment->set_enabled(true);
    } else if (keysig && is_pitched_staff) {
        // do not remove user modified keysigs
        bool remove = true;
        EngravingItem *e = k_segment->element(staff_idx * ::mu::engraving::voices);
        Key key = staff->key(m->tick());
        if ((e && !e->generated()) || (key != key_idx.key())) {
            remove = false;
        }

        if (remove) {
            keysig->ownership_parent_item()->remove(keysig);
            if (keysig->generated()) {
                delete keysig;
            }
        }
    }

    return k_segment;
}

//-------------------------------------------------------------------
//   addSystemHeader
///   Add elements to make this measure suitable as the first measure
///   of a system.
//    The system header can contain a starting BarLine, a Clef,
//    and a KeySig
//-------------------------------------------------------------------

void MeasureLayout::add_system_header(Measure *m, bool is_first_system, LayoutContext &ctx) {
    int staff_idx = 0;
    Segment *k_segment = nullptr;
    Segment *c_segment = nullptr;

    for (const Staff *staff : ctx.dom().staves()) {
        const int track = staff_idx * ::mu::engraving::voices;

        // Check if this is the first VISIBLE appearance
        bool is_first_clef = true;
        bool is_first_key_sig = true;
        if (!is_first_system) {
            const Fraction clef_tick = staff->current_clef_tick(m->tick());
            const Fraction key_sig_tick = staff->current_key_tick(m->tick());
            // Get first measure whether MMR or not
            Measure *search_measure = ctx.mut_dom().tick2measure(std::min(clef_tick, key_sig_tick));
            search_measure = search_measure->has_mm_rest() && ctx.conf().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) ?
                                 search_measure->mm_rest() :
                                 search_measure;
            while (search_measure->tick() < m->tick() && (is_first_clef || is_first_key_sig)) {
                const System *sys = search_measure->system();
                if (is_first_clef && search_measure->tick() >= clef_tick) {
                    // Need to check previous measure for clef change if one not found in this measure
                    Segment *clef_seg = search_measure->find_first_r(SegmentType::CLEF | SegmentType::HEADER_CLEF, Fraction(0, 1));
                    Measure *prev_meas = search_measure->prev_measure();
                    if (prev_meas && !clef_seg) {
                        clef_seg = prev_meas->find_segment(SegmentType::CLEF, m->tick());
                    }
                    if (clef_seg && clef_seg->enabled()) {
                        const Clef *c = to_clef(clef_seg->element(track));
                        if (c && sys && sys->staff(staff_idx)->show()) {
                            is_first_clef = false;
                        }
                    }
                }
                if (is_first_key_sig && search_measure->tick() >= key_sig_tick) {
                    const Segment *ks_seg = search_measure->find_segment(SegmentType::KEY_SIG, search_measure->tick());
                    if (ks_seg && ks_seg->enabled()) {
                        const KeySig *ks = to_key_sig(ks_seg->element(track));
                        if (ks && sys && sys->staff(staff_idx)->show()) {
                            is_first_key_sig = false;
                        }
                    }
                }
                search_measure = search_measure->next_measure_mm();
            }
        }

        c_segment = add_header_clef(m, is_first_system || is_first_clef, staff, ctx);

        k_segment = add_header_key_sig(m, is_first_system || is_first_key_sig, staff, ctx);

        ++staff_idx;
    }
    if (c_segment) {
        c_segment->create_shapes();
    }
    if (k_segment) {
        k_segment->create_shapes();
    }

    MeasureLayout::create_system_begin_bar_line(m, ctx);

    m->check_header();
}

void MeasureLayout::remove_system_header(Measure *m) {
    if (!m->header()) {
        return;
    }
    for (Segment *seg = m->first(); seg; seg = seg->next()) {
        if (seg->is_key_sig_type()) {
            bool key_sig_change_happens_here = false;
            for (Staff *staff : m->score()->staves()) {
                if (staff->key_list()->count(m->tick().ticks()) > 0) {
                    key_sig_change_happens_here = true;
                    break;
                }
            }
            if (seg->header() && !key_sig_change_happens_here) {
                seg->set_enabled(false);
            }
        }
        if (!seg->header()) {
            break;
        }
        if (!seg->is_key_sig_type()) {
            seg->set_enabled(false);
        }
    }
    m->set_header(false);
}

void MeasureLayout::add_system_trailer(Measure *m, Measure *nm, LayoutContext &ctx) {
    const size_t nstaves = ctx.dom().nstaves();
    bool system_break_hide_courtesy = false;
    if (LayoutBreak *section_break_element = m->section_break_element()) {
        system_break_hide_courtesy = !section_break_element->show_courtesy();
    }

    // locate a time sig. in the next measure and, if found,
    // check if it has court. sig. turned off
    if (nm && ctx.conf().style_b(Sid::GEN_COURTESY_TIMESIG) && !system_break_hide_courtesy && !ctx.conf().is_float_mode()) {
        set_courtesy_time_sig(m, nm->tick(), m->end_tick(), SegmentType::TIME_SIG_ANNOUNCE, ctx);
    }

    // courtesy key signatures, clefs
    if (nm && ctx.conf().style_b(Sid::GEN_COURTESY_KEYSIG) && !system_break_hide_courtesy) {
        set_courtesy_key_sig(m, nm->tick(), m->end_tick(), SegmentType::KEY_SIG_ANNOUNCE, ctx);
    }

    Segment *courtesy_clef_seg = m->find_segment_r(SegmentType::CLEF, m->ticks());
    if (courtesy_clef_seg) {
        for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
            const track_idx_t track = staff_idx * ::mu::engraving::voices;

            Clef *courtesy_clef = to_clef(courtesy_clef_seg->element(track));
            if (courtesy_clef) {
                courtesy_clef->set_small(true);
                courtesy_clef->set_is_trailer(true);
            }
        }
    }
    if (courtesy_clef_seg) {
        courtesy_clef_seg->create_shapes();
    }

    m->check_trailer();
}

void MeasureLayout::remove_system_trailer(Measure *m, LayoutContext &ctx) {
    for (Segment *seg = m->last(); seg != m->first(); seg = seg->prev()) {
        if (seg->is_chord_rest_type()) {
            break;
        }

        if (seg->is_clef_type()) {
            for (EngravingItem *el : seg->elist()) {
                if (!el) {
                    continue;
                }
                Clef *clef = to_clef(el);
                clef->set_is_trailer(false);
                TLayout::layout_clef(clef, clef->mutldata(), ctx.conf());
            }
            seg->create_shapes();
        }

        if (seg->is_time_tick_type() || !seg->trailer()) {
            continue;
        }

        if (seg->enabled()) {
            seg->set_enabled(false);
            seg->set_trailer(false);
        }
    }
    m->set_trailer(false);
}

void MeasureLayout::create_system_begin_bar_line(Measure *m, LayoutContext &ctx) {
    if (!m->system()) {
        return;
    }
    Segment *s = m->find_segment(SegmentType::BEGIN_BAR_LINE, m->tick());
    size_t n = 0;
    if (m->system()) {
        for (SysStaff *sys_staff : m->system()->staves()) {
            if (sys_staff->show()) {
                ++n;
            }
        }
    }
    if ((n > 1 && ctx.conf().style_b(Sid::START_BARLINE_MULTIPLE)) ||
        (n == 1 && (ctx.conf().style_b(Sid::START_BARLINE_SINGLE) || m->system()->brackets().size()))) {
        if (!s) {
            s = Factory::create_segment(m, SegmentType::BEGIN_BAR_LINE, Fraction(0, 1));
            m->add(s);
        }
        for (track_idx_t track = 0; track < ctx.dom().ntracks(); track += ::mu::engraving::voices) {
            BarLine *bl = to_bar_line(s->element(track));
            if (!bl) {
                bl = Factory::create_bar_line(s);
                bl->set_track(track);
                bl->set_generated(true);
                bl->set_bar_line_type(BarLineType::NORMAL);
                bl->set_span_staff(true);
                s->add(bl);
            }

            TLayout::layout_bar_line(bl, bl->mutldata(), ctx);
        }
        s->create_shapes();
        s->set_enabled(true);
        s->set_header(true);
        m->set_header(true);
    } else if (s) {
        s->set_enabled(false);
    }
}

void MeasureLayout::stretch_measure_in_practice_mode(Measure *m, double target_width, LayoutContext &ctx) {
    Measure::LayoutData *ldata = m->mutldata();
    ldata->set_width(target_width);

    //---------------------------------------------------
    //    compute stretch
    //---------------------------------------------------

    std::multimap<double, Segment *> springs;

    Segment *seg = m->first();
    while (seg && !seg->enabled()) {
        seg = seg->next();
    }
    double minimum_width = seg ? seg->x() : 0.0;
    for (Segment &s : m->segments()) {
        s.set_stretch(1);

        //        if (!s.enabled() || !s.visible())
        //            ;continue;
        Fraction t = s.ticks();
        if (t.is_not_zero()) {
            springs.insert(std::pair<double, Segment *>(0, &s));
        }
        minimum_width += s.width() + s.spacing();
    }

    //---------------------------------------------------
    //    compute 1/Force for a given Extend
    //---------------------------------------------------

    if (target_width > minimum_width) {
        double chord_rest_segments_width{0};
        for (auto i = springs.begin(); i != springs.end(); i++) {
            chord_rest_segments_width += i->second->width() + i->second->spacing();
        }

        double stretch = 1 + (target_width - minimum_width) / chord_rest_segments_width;

        //---------------------------------------------------
        //    distribute stretch to segments
        //---------------------------------------------------

        for (auto &i : springs) {
            i.second->set_stretch(stretch);
        }

        //---------------------------------------------------
        //    move segments to final position
        //---------------------------------------------------

        Segment *s = m->first();
        while (s && !s->enabled()) {
            s = s->next();
        }

        double x = s ? s->pos().x() : 0.0;
        while (s) {
            //            if (!s->enabled() || !s->visible()) {
            //                s = s->nextEnabled();
            //                continue;
            //            }

            double spacing = s->spacing();
            double width_without_spacing = s->width() - spacing;
            double segment_stretch = s->stretch();
            x += spacing * (muse::real_is_null(segment_stretch) ? 1 : segment_stretch);
            s->mutldata()->set_pos_x(x);
            x += width_without_spacing * (muse::real_is_null(segment_stretch) ? 1 : segment_stretch);
            s = s->next_enabled();
        }
    }

    //---------------------------------------------------
    //    layout individual elements
    //---------------------------------------------------

    for (Segment &s : m->segments()) {
        if (!s.enabled()) {
            continue;
        }
        // After the rest of the spacing is calculated we position grace-notes-after.
        ChordLayout::reposition_grace_notes_after(&s, ctx.dom().ntracks());
        for (EngravingItem *e : s.elist()) {
            if (!e) {
                continue;
            }
            ElementType t = e->type();
            staff_idx_t staff_idx = e->staff_idx();
            if (t == ElementType::MEASURE_REPEAT || (t == ElementType::REST && (m->is_mm_rest() || to_rest(e)->is_full_measure_rest()))) {
                //
                // element has to be centered in free space
                //    x1 - left measure position of free space
                //    x2 - right measure position of free space

                Segment *s1;
                for (s1 = s.prev(); s1 && !s1->enabled(); s1 = s1->prev()) {}
                Segment *s2;
                for (s2 = s.next(); s2; s2 = s2->next()) {
                    if (s2->enabled() && !s2->is_chord_rest_type() && s2->element(staff_idx * ::mu::engraving::voices)) {
                        break;
                    }
                }
                double x1 = s1 ? s1->x() + s1->min_right() : 0;
                double x2 = s2 ? s2->x() - s2->min_left() : target_width;

                if (m->is_mm_rest()) {
                    MMRest *mmrest = to_mm_rest(e);
                    //
                    // center multi measure rest
                    //
                    double d = ctx.conf().style_absolute(Sid::MULTI_MEASURE_REST_MARGIN);
                    double w = x2 - x1 - 2 * d;

                    mmrest->mutldata()->rest_width = w;
                    TLayout::layout_mm_rest(mmrest, mmrest->mutldata(), ctx);
                    e->set_pos(x1 - s.x() + d, e->staff()->staff_height() * .5); // center vertically in measure
                    s.create_shape(staff_idx);
                } else { // if (rest->isFullMeasureRest()) {
                    //
                    // center full measure rest
                    //
                    e->mutldata()->set_pos_x((x2 - x1 - e->width()) * .5 + x1 - s.x() - e->ldata()->bbox().x());
                    s.create_shape(staff_idx); // DEBUG
                }
            } else if (t == ElementType::REST) {
                e->mutldata()->set_pos_x(0);
            } else if (t == ElementType::CHORD) {
                Chord *c = to_chord(e);
                if (c->tremolo_single_chord()) {
                    TremoloLayout::layout(c->tremolo_single_chord(), ctx);
                } else if (c->tremolo_two_chord()) {
                    TremoloTwoChord *tr = c->tremolo_two_chord();
                    Chord *c1 = tr->chord1();
                    Chord *c2 = tr->chord2();
                    if (c1 && !c1->staff_move() && c2 && !c2->staff_move()) {
                        TremoloLayout::layout(tr, ctx);
                    }
                }
            } else if (t == ElementType::BAR_LINE) {
                e->mutldata()->set_pos_y(0.0);
                // for end barlines, x position was set in createEndBarLines
                if (s.segment_type() != SegmentType::END_BAR_LINE) {
                    e->mutldata()->set_pos_x(0.0);
                }
            }
        }
    }
}

void MeasureLayout::layout_time_tick_anchors(Measure *m, LayoutContext &ctx) {
    bool darker = true;
    for (Segment &segment : m->segments()) {
        if (!segment.is_time_tick_type()) {
            continue;
        }

        Segment *ref_cr_seg = m->find_segment_r(SegmentType::CHORD_REST, segment.rtick());
        if (!(ref_cr_seg && ref_cr_seg->is_active())) {
            ref_cr_seg = segment.prev_active();
            while (ref_cr_seg && !ref_cr_seg->is_chord_rest_type()) {
                ref_cr_seg = ref_cr_seg->prev_active();
            }
        }

        if (!ref_cr_seg || ref_cr_seg->ticks().is_zero()) {
            continue;
        }

        Fraction ref_seg_duration = ref_cr_seg->ticks();
        Fraction this_duration = segment.ticks();
        Fraction relative_tick = segment.rtick() - ref_cr_seg->rtick();

        Segment *next_seg = m->find_segment_r(SegmentType::CHORD_REST, ref_cr_seg->rtick() + ref_cr_seg->ticks());
        if (!(next_seg && next_seg->is_active())) {
            next_seg = m->find_segment_r(SegmentType::BAR_LINE_TYPES, ref_cr_seg->rtick() + ref_cr_seg->ticks());
        }
        double width = next_seg ? next_seg->x() - ref_cr_seg->x() : ref_cr_seg->width();

        double relative_x = width * (relative_tick.to_double() / ref_cr_seg->ticks().to_double());
        double relative_width = width * (this_duration.to_double() / ref_seg_duration.to_double());

        segment.mutldata()->set_pos_x(ref_cr_seg->x() + relative_x);
        segment.set_width(relative_width);

        for (EngravingItem *item : segment.elist()) {
            if (item) {
                TLayout::layout_item(item, ctx);
                to_time_tick_anchor(item)->mutldata()->set_darker(darker);
            }
        }
        darker = !darker;
    }
}

//---------------------------------------------------------
//   layoutPartialWidth
///   Layout staff lines for the specified width only, aligned
///   to the left or right of the measure
//---------------------------------------------------------

void MeasureLayout::layout_partial_width(StaffLines *lines, LayoutContext &ctx, double w, double w_partial, bool align_right) {
    StaffLines::LayoutData *ldata = lines->mutldata();
    const Staff *s = lines->staff();
    double spatium_value = lines->spatium();
    w_partial *= spatium_value;
    double dist = spatium_value;
    lines->set_pos(PointF(0.0, 0.0));
    int lines_value;
    if (s) {
        lines->mutldata()->set_mag(s->staff_mag(lines->measure()->tick()));
        lines->set_color(s->color(lines->measure()->tick()));
        const StaffType *st = s->staff_type(lines->measure()->tick());
        dist *= st->line_distance().val();
        lines_value = st->lines();
        lines->mutldata()->set_pos_y(lines->staff_offset_y());
    } else {
        lines_value = 5;
        lines->set_color(lines->configuration()->default_color());
    }
    lines->set_lw(ctx.conf().style_s(Sid::STAFF_LINE_WIDTH).val() * spatium_value);
    double x1 = lines->pos().x();
    double x2 = x1 + w;
    double y = lines->pos().y();
    ldata->set_bbox(x1, -lines->lw() * .5 + y, w, (lines_value - 1) * dist + lines->lw());

    if (lines_value == 1) {
        double extra_size = spatium_value;
        ldata->set_bbox(ldata->bbox().adjusted(0, -extra_size, 0, extra_size));
    }

    std::vector<LineF> ll;
    for (int i = 0; i < lines_value; ++i) {
        if (align_right) {
            ll.push_back(LineF(x2 - w_partial, y, x2, y));
        } else {
            ll.push_back(LineF(x1, y, x1 + w_partial, y));
        }
        y += dist;
    }
    lines->set_lines(ll);
}

void MeasureLayout::update_key_signatures(const Measure *measure, LayoutContext &ctx) {
    Measure *prev_measure = measure->prev_measure();
    if (!prev_measure || !prev_measure->repeat_end()) {
        return;
    }
    for (const Segment &seg : measure->segments()) {
        if (!seg.is_type(SegmentType::KEY_SIG_TYPES)) {
            continue;
        }

        for (EngravingItem *el : seg.elist()) {
            if (!el) {
                continue;
            }

            KeySig *ks = to_key_sig(el);

            TLayout::layout_key_sig(ks, ks->mutldata(), ctx.conf());
        }
    }
}
