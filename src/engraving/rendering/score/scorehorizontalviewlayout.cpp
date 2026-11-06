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
#include "scorehorizontalviewlayout.h"

#include "containers.h"

#include "dom/durationelement.h"
#include "dom/factory.h"
#include "dom/score.h"
#include "dom/masterscore.h"
#include "dom/system.h"
#include "dom/systemlockindicator.h"
#include "dom/spanner.h"
#include "dom/page.h"
#include "dom/durationelement.h"
#include "dom/staff.h"
#include "dom/chordrest.h"
#include "dom/chord.h"
#include "dom/tuplet.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/part.h"
#include "dom/box.h"
#include "dom/slur.h"

#include "passresetlayoutdata.h"

#include "tlayout.h"
#include "systemlayout.h"
#include "beamlayout.h"
#include "tupletlayout.h"
#include "chordlayout.h"
#include "chordbracketlayout.h"
#include "arpeggiolayout.h"
#include "measurelayout.h"
#include "horizontalspacing.h"
#include "masklayout.h"
#include "tremololayout.h"
#include "slurtielayout.h"
#include "systemheaderlayout.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void ScoreHorizontalViewLayout::layout_horizontal_view(Score *score, LayoutContext &ctx, const Fraction &stick, const Fraction &etick) {
    ctx.mut_state().set_end_tick(etick);

    //---------------------------------------------------
    //    initialize layout context lc
    //---------------------------------------------------

    MeasureBase *m = score->tick2measure(stick);
    if (m == 0) {
        m = score->first();
    }
    // start layout one measure earlier to handle clefs and cautionary elements
    if (m->prev_measure_mm()) {
        m = m->prev_measure_mm();
    } else if (m->prev()) {
        m = m->prev();
    }
    while (!m->is_measure() && m->prev()) {
        m = m->prev();
    }

    // if the first measure of the score is part of a multi measure rest
    // m->system() will return a nullptr. We need to find the multi measure
    // rest which replaces the measure range

    if (!m->system() && m->is_measure() && to_measure(m)->has_mm_rest()) {
        LOGD("  don’t start with mmrest");
        m = to_measure(m)->mm_rest();
    }

    ctx.mut_state().set_prev_measure(nullptr);
    ctx.mut_state().set_next_measure(m); //_showVBox ? first() : firstMeasure();
    ctx.mut_state().set_start_tick(m->tick());

    PassResetLayoutData reset_pass;
    reset_pass.run(score, ctx);

    layout_linear(ctx, ctx.state().is_layout_all());
}

void ScoreHorizontalViewLayout::layout_linear(LayoutContext &ctx, bool layout_all) {
    reset_systems(ctx, layout_all);

    collect_linear_system(ctx);

    layout_linear(ctx);
}

//  in linear mode there is only one page
//  which contains one system
void ScoreHorizontalViewLayout::reset_systems(LayoutContext &ctx, bool layout_all) {
    DomAccessor &mut_dom = ctx.mut_dom();
    Page *page = 0;
    if (layout_all) {
        for (System *s : mut_dom.systems()) {
            for (SpannerSegment *ss : s->spanner_segments()) {
                ss->set_system(nullptr);
            }
        }
        muse::delete_all(mut_dom.systems());
        mut_dom.systems().clear();
        muse::delete_all(mut_dom.pages());
        mut_dom.pages().clear();
        if (!ctx.dom().first_measure()) {
            LOGD("no measures");
            return;
        }

        for (MeasureBase *mb = ctx.mut_dom().first(); mb; mb = mb->next()) {
            mb->set_system(nullptr);
        }

        page = Factory::create_page(ctx.mut_dom().score());
        ctx.mut_dom().pages().push_back(page);
        page->mutldata()->set_bbox(0.0, 0.0, ctx.conf().lo_width(), ctx.conf().lo_height());
        page->set_page_number(0);

        System *system = Factory::create_system(page->score());
        ctx.mut_dom().systems().push_back(system);
        page->append_system(system);
        system->adjust_staves_number(ctx.dom().nstaves());
    } else {
        if (ctx.dom().pages().empty()) {
            return;
        }
        page = ctx.mut_dom().pages().front();
        System *system = ctx.mut_dom().systems().front();
        system->clear();
        system->adjust_staves_number(ctx.dom().nstaves());
    }
    ctx.mut_state().set_page(page);
}

void ScoreHorizontalViewLayout::layout_linear(LayoutContext &ctx) {
    System *system = ctx.mut_dom().systems().front();

    SystemLayout::layout_system_elements(system, ctx);

    SystemLayout::layout2(system, ctx); // compute staff distances

    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        Measure *m = to_measure(mb);

        for (size_t track = 0; track < ctx.dom().ntracks(); ++track) {
            for (Segment *segment = m->first(); segment; segment = segment->next()) {
                EngravingItem *e = segment->element(track);
                if (!e) {
                    continue;
                }
                if (e->is_chord_rest()) {
                    if (m->tick() < ctx.state().start_tick() || m->tick() > ctx.state().end_tick()) {
                        continue;
                    }
                    if (!ctx.dom().staff(track2staff(track))->show()) {
                        continue;
                    }
                    ChordRest *cr = to_chord_rest(e);
                    if (BeamLayout::is_start_of_cross_beam(cr)) { // layout cross staff beams
                        TLayout::layout_beam(cr->beam(), ctx);
                    }
                    if (TupletLayout::not_top_tuplet(cr)) {
                        // fix layout of tuplets
                        DurationElement *de = cr;
                        while (de->tuplet() && de->tuplet()->elements().front() == de) {
                            Tuplet *t = de->tuplet();
                            TLayout::layout_tuplet(t, ctx);
                            de = de->tuplet();
                        }
                    }

                    if (cr->is_chord()) {
                        Chord *c = to_chord(cr);
                        for (Chord *cc : c->grace_notes()) {
                            if (cc->beam() && cc->beam()->elements().front() == cc) {
                                TLayout::layout_beam(cc->beam(), ctx);
                            }
                            ChordLayout::layout_spanners(cc, ctx);
                            for (EngravingItem *element : cc->el()) {
                                if (element->is_slur()) {
                                    TLayout::layout_item(element, ctx);
                                }
                            }
                        }
                        ArpeggioLayout::layout_arpeggio2(c->arpeggio(), ctx);
                        for (EngravingItem *element : c->el()) {
                            if (element->is_chord_bracket()) {
                                ChordBracket *bracket = to_chord_bracket(element);
                                if (c->on_tab_staff()) {
                                    TLayout::layout_item(bracket, ctx);
                                } else {
                                    ChordBracketLayout::update_vertical_geometry(bracket, ctx);
                                }
                            }
                        }
                        ChordLayout::layout_spanners(c, ctx);
                        if (c->tremolo_single_chord()) {
                            TremoloLayout::layout(c->tremolo_single_chord(), ctx);
                        } else if (c->tremolo_two_chord()) {
                            TremoloTwoChord *t = c->tremolo_two_chord();
                            Chord *c1 = t->chord1();
                            Chord *c2 = t->chord2();
                            if (c1 && c2 && (c1->staff_move() || c2->staff_move())) {
                                TremoloLayout::layout(t, ctx);
                            }
                        }
                    }
                } else if (e->is_bar_line()) {
                    TLayout::layout_bar_line2(to_bar_line(e), ctx);
                }
            }
        }
        MeasureLayout::layout2(m, ctx);
    }

    auto spanners = ctx.dom().spanner_map().find_overlapping(system->tick().ticks(), system->end_tick().ticks());
    for (auto interval : spanners) {
        Spanner *sp = interval.value;
        if (sp->is_slur() && (to_slur(sp)->is_cross_staff() || to_slur(sp)->has_cross_beams())) {
            SlurTieLayout::layout_system(to_slur(sp), system, ctx);
        }
    }

    const double lm = ctx.state().page()->lm();
    const double tm = ctx.state().page()->tm() + ctx.conf().style_absolute(Sid::STAFF_UPPER_BORDER);
    const double rm = ctx.state().page()->rm();
    const double bm = ctx.state().page()->bm() + ctx.conf().style_absolute(Sid::STAFF_LOWER_BORDER);

    layout_system_lock_indicators(system);

    ctx.mut_state().page()->set_pos(0, 0);
    system->set_pos(lm, tm);
    ctx.mut_state().page()->set_width(lm + system->width() + rm);
    ctx.mut_state().page()->set_height(tm + system->height() + bm);
    MaskLayout::compute_masks(ctx, ctx.mut_state().page());
    ctx.mut_state().page()->invalidate_bsp_tree();
}

void ScoreHorizontalViewLayout::layout_system_lock_indicators(System *system) {
    // TODO: layout StaffVisibilityIndicator here

    system->delete_system_lock_indicators();

    std::vector<const RangeLock *> system_locks = system->score()->system_locks()->all_locks();
    for (const RangeLock *lock : system_locks) {
        SystemLockIndicator *lock_indicator = Factory::create_system_lock_indicator(system, lock);
        lock_indicator->set_track(0);
        system->add_system_lock_indicator(lock_indicator);
        TLayout::layout_indicator_icon(lock_indicator, lock_indicator->mutldata());
    }
}

// Append all measures to System. VBox is not included to System
void ScoreHorizontalViewLayout::collect_linear_system(LayoutContext &ctx) {
    std::vector<int> visible_parts;
    visible_parts.reserve(ctx.dom().parts().size());
    for (size_t part_idx = 0; part_idx < ctx.dom().parts().size(); part_idx++) {
        if (ctx.dom().parts().at(part_idx)->show()) {
            visible_parts.push_back(static_cast<int>(part_idx));
        }
    }

    System *system = ctx.mut_dom().systems().front();
    SystemHeaderLayout::set_instrument_names(system, ctx);

    double target_system_width = ctx.dom().nmeasures() * ctx.conf().style_absolute(Sid::MIN_MEASURE_WIDTH);
    system->set_width(target_system_width);

    double cur_system_width = 0.0;
    bool first_measure_in_score = true; //lc.startTick.isZero();
    bool first_measure_in_layout = true;

    //set first measure to lc.nextMeasures for following
    //utilizing in getNextMeasure()
    ctx.mut_state().set_next_measure(ctx.mut_dom().first());
    ctx.mut_state().set_tick(Fraction(0, 1));
    MeasureLayout::get_next_measure(ctx);

    std::set<Measure *> measures_to_layout;

    while (ctx.state().cur_measure()) {
        if (ctx.state().cur_measure()->is_v_box_base()) {
            ctx.mut_state().cur_measure()->set_system(nullptr);
            MeasureLayout::get_next_measure(ctx);
            MeasureLayout::layout_measure(ctx.mut_state().cur_measure(), ctx);
            continue;
        }
        system->append_measure(ctx.mut_state().cur_measure());
        MeasureLayout::layout_measure(ctx.mut_state().cur_measure(), ctx);
        bool create_header = ctx.state().prev_measure() && ctx.state().prev_measure()->is_h_box() &&
                             to_h_box(ctx.state().prev_measure())->create_system_header();
        if (ctx.state().cur_measure()->is_measure()) {
            Measure *m = to_measure(ctx.mut_state().cur_measure());
            if (m->mm_rest()) {
                m->mm_rest()->set_system(nullptr);
            }
            if (first_measure_in_score) {
                SystemLayout::layout_system(system, ctx, cur_system_width);
                if (m->repeat_start()) {
                    Segment *s = m->find_segment_r(SegmentType::START_REPEAT_BAR_LINE, Fraction(0, 1));
                    if (!s->enabled()) {
                        s->set_enabled(true);
                    }
                }
                MeasureLayout::add_system_header(m, true, ctx);
                cur_system_width += system->left_margin();
                first_measure_in_score = false;
            } else if (create_header) {
                MeasureLayout::add_system_header(m, false, ctx);
            } else if (m->header()) {
                MeasureLayout::remove_system_header(m);
            }
            if (m->trailer()) {
                MeasureLayout::remove_system_trailer(m, ctx);
            }

            if (m->tick() >= ctx.state().start_tick() && m->tick() <= ctx.state().end_tick()) {
                // for measures in range, do full layout
                if (ctx.conf().is_mode(LayoutMode::HORIZONTAL_FIXED)) {
                    MeasureLayout::create_end_bar_lines(m, true, ctx);
                    layout_segments_with_duration(m, visible_parts);
                    double measure_width = m->width();
                    MeasureLayout::stretch_measure_in_practice_mode(m, measure_width, ctx);
                    m->set_pos(cur_system_width, m->y());
                    cur_system_width += measure_width;
                } else {
                    MeasureLayout::compute_pre_spacing_items(m, ctx);
                    MeasureLayout::create_end_bar_lines(m, false, ctx);
                    MeasureLayout::set_repeat_courtesies_and_parens(m, ctx);
                    MeasureLayout::update_grace_notes(m, ctx);
                    cur_system_width = HorizontalSpacing::update_spacing_for_last_added_measure(system, first_measure_in_layout);
                    measures_to_layout.insert(m);
                    if (first_measure_in_layout) {
                        first_measure_in_layout = false;
                    }
                }
            } else {
                // for measures not in range, use existing layout
                double measure_width = m->width();
                if (!muse::real_is_equal(m->x(), cur_system_width)) {
                    // fix beam positions
                    // other elements with system as parent are processed in layoutSystemElements()
                    // but full beam processing is expensive and not needed if we adjust position here
                    PointF p = PointF(cur_system_width, 0.0) - m->pos();
                    for (const Segment &s : m->segments()) {
                        if (!s.is_chord_rest_type()) {
                            continue;
                        }
                        for (size_t track = 0; track < ctx.dom().ntracks(); ++track) {
                            EngravingItem *e = s.element(static_cast<track_idx_t>(track));
                            if (!e) {
                                continue;
                            }
                            ChordRest *cr = to_chord_rest(e);
                            if (cr->beam() && cr->beam()->elements().front() == cr) {
                                cr->beam()->mutldata()->move(p);
                            }
                        }
                    }
                }
                m->set_pos(cur_system_width, m->y());
                cur_system_width += measure_width;
            }
        } else if (ctx.state().cur_measure()->is_h_box()) {
            cur_system_width = HorizontalSpacing::update_spacing_for_last_added_measure(system);
        }

        MeasureLayout::get_next_measure(ctx);
    }

    for (Measure *m : measures_to_layout) {
        MeasureLayout::layout_measure_elements(m, ctx);
    }

    for (MeasureBase *m : system->measures()) {
        if (!m->is_measure()) {
            continue;
        }
        MeasureLayout::layout_staff_lines(to_measure(m), ctx);
    }

    SystemLayout::hide_empty_staves(system, ctx, true);

    system->set_width(cur_system_width);
}

static Segment *find_first_enabled_segment(Measure *measure) {
    Segment *current = measure->first();
    while (current && !current->enabled()) {
        current = current->next();
    }

    return current;
}

void ScoreHorizontalViewLayout::layout_segments_with_duration(Measure *m, const std::vector<int> &visible_parts) {
    double current_x_pos = 0;

    Segment *current = find_first_enabled_segment(m);

    auto [spacing, width] = compute_cell_width(current, visible_parts);
    current_x_pos = m->style().style_absolute(Sid::BAR_NOTE_DISTANCE);
    current->mutldata()->set_pos_x(current_x_pos);
    current->set_width(width);
    current->set_spacing(spacing);
    current_x_pos += width;

    current = current->next();
    while (current) {
        //        if (!current->enabled() || !current->visible()) {
        //            current = current->next();
        //            continue;
        //        }

        auto [spacing2, width2] = compute_cell_width(current, visible_parts);
        current->set_width(width2 + spacing2);
        current->set_spacing(spacing2);
        current_x_pos += spacing2;
        current->mutldata()->set_pos_x(current_x_pos);
        current_x_pos += width2;
        current = current->next();
    }

    m->set_width(current_x_pos);
}

std::pair<double, double> ScoreHorizontalViewLayout::compute_cell_width(const Segment *s, const std::vector<int> &visible_parts) {
    if (!s->enabled()) {
        return {0, 0};
    }

    Fraction quantum = calculate_quantum_cell(s->measure(), visible_parts);

    auto calculate_width = [quantum, sc = s->score()->master_score()](ChordRest *cr) {
        //! width of a segment cell, in spatiums per quantum unit
        static constexpr double width_of_segment_cell = 3;
        return width_of_segment_cell * sc->style().spatium() * cr->global_ticks().numerator() / cr->global_ticks().denominator() *
               quantum.denominator() / quantum.numerator();
    };

    if (s->is_chord_rest_type()) {
        float width = 0.0;
        float spacing = 0.0;

        ChordRest *cr = nullptr;

        cr = chord_rest_with_min_duration(s, visible_parts);

        if (cr) {
            width = calculate_width(cr);

            if (cr->is_rest()) {
                //spacing = 0; //!not necessary. It is to more clearly understanding code
            } else if (cr->is_chord()) {
                Chord *ch = to_chord(cr);

                //! check that gracenote exist. If exist add additional spacing
                //! to avoid colliding between grace note and previous chord
                if (!ch->grace_notes().empty()) {
                    Segment *prev_seg = s->prev();
                    if (prev_seg && prev_seg->segment_type() == SegmentType::CHORD_REST) {
                        ChordRest *prev_cr = chord_rest_with_min_duration(prev_seg, visible_parts);

                        if (prev_cr && prev_cr->global_ticks() < quantum) {
                            spacing = calculate_width(prev_cr);
                            return {spacing, width};
                        }
                    }
                }

                //! check that accidental exist in the chord. If exist add additional spacing
                //! to avoid colliding between grace note and previous chord
                for (auto note : ch->notes()) {
                    if (note->accidental()) {
                        Segment *prev_seg = s->prev();
                        if (prev_seg && prev_seg->segment_type() == SegmentType::CHORD_REST) {
                            ChordRest *prev_cr = chord_rest_with_min_duration(prev_seg, visible_parts);

                            if (prev_cr && prev_cr->global_ticks() < quantum) {
                                spacing = calculate_width(prev_cr);
                                return {spacing, width};
                            }
                        }
                    }
                }
            }
        }

        return {spacing, width};
    }

    Segment *next_seg = s->next_active();
    if (!next_seg) {
        next_seg = s->next(SegmentType::BAR_LINE_TYPES);
    }

    if (next_seg) {
        return {0, HorizontalSpacing::min_horizontal_distance(s, next_seg, 1.0)};
    }

    return {0, s->min_right()};
}

ChordRest *ScoreHorizontalViewLayout::chord_rest_with_min_duration(const Segment *seg, const std::vector<int> &visible_parts) {
    ChordRest *chord_rest_with_min_duration = nullptr;
    int min_ticks = std::numeric_limits<int>::max();
    for (int part_idx : visible_parts) {
        for (const Staff *staff : seg->score()->parts().at(part_idx)->staves()) {
            staff_idx_t staff_idx = staff->idx();
            for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
                if (auto element = seg->elist().at(staff_idx * ::mu::engraving::voices + voice)) {
                    if (!element->is_chord_rest()) {
                        continue;
                    }

                    ChordRest *cr = to_chord_rest(element);
                    int chord_ticks = cr->ticks().ticks();
                    if (chord_ticks > min_ticks) {
                        continue;
                    }
                    min_ticks = chord_ticks;
                    chord_rest_with_min_duration = cr;
                }
            }
        }
    }

    return chord_rest_with_min_duration;
}

Fraction ScoreHorizontalViewLayout::calculate_quantum_cell(const Measure *m, const std::vector<int> &visible_parts) {
    Fraction quantum = {1, 16};
    for (const Segment &s : m->segments()) {
        ChordRest *cr = chord_rest_with_min_duration(&s, visible_parts);

        if (cr && cr->actual_ticks() < quantum) {
            quantum = cr->actual_ticks();
        }
    }

    return quantum;
}
