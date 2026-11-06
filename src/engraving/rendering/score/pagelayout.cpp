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
#include "pagelayout.h"

#include "arpeggiolayout.h"
#include "beamlayout.h"
#include "chordbracketlayout.h"
#include "chordlayout.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/box.h"
#include "dom/bracketitem.h"
#include "dom/chord.h"
#include "dom/chordrest.h"
#include "dom/durationelement.h"
#include "dom/factory.h"
#include "dom/fingering.h"
#include "dom/measure.h"
#include "dom/measurebase.h"
#include "dom/note.h"
#include "dom/page.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/slur.h"
#include "dom/spacer.h"
#include "dom/spannermap.h"
#include "dom/staff.h"
#include "dom/staffvisibilityindicator.h"
#include "dom/system.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/tuplet.h"
#include "headerfooterlayout.h"
#include "log.h"
#include "masklayout.h"
#include "measurelayout.h"
#include "realfn.h"
#include "slurtielayout.h"
#include "systemheaderlayout.h"
#include "systemlayout.h"
#include "tlayout.h"
#include "tupletlayout.h"
#include "verticalgapdata.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

//---------------------------------------------------------
//   getNextPage
//---------------------------------------------------------

void PageLayout::get_next_page(LayoutContext &ctx) {
    LayoutState &state = ctx.mut_state();
    DomAccessor &dom = ctx.mut_dom();

    if (!state.page() || state.page_idx() >= dom.npages()) {
        state.set_page(Factory::create_page(dom.score()));
        dom.pages().push_back(state.page());
        state.set_prev_system(nullptr);
        state.set_page_old_measure(nullptr);
    } else {
        state.set_page(dom.pages()[state.page_idx()]);
        std::vector<System *> &systems = state.page()->systems();
        state.set_page_old_measure(systems.empty() || systems.back()->measures().empty() ? nullptr : systems.back()->measures().back());
        const system_idx_t i = muse::index_of(systems, state.cur_system());
        if ((i < systems.size()) && i > 0 && systems[i - 1]->page() == state.page()) {
            // Current and previous systems are on the current page.
            // Erase only the current and the following systems
            // as the previous one will not participate in layout.
            systems.erase(systems.begin() + i, systems.end());
        } else { // system is not on the current page (or will be the first one)
            systems.clear();
        }
        state.set_prev_system(systems.empty() ? nullptr : systems.back());
    }
    state.page()->mutldata()->set_bbox(0.0, 0.0, ctx.conf().lo_width(), ctx.conf().lo_height());
    state.page()->set_page_number(state.page_idx());
    double x = 0.0;
    double y = 0.0;
    if (state.page_idx()) {
        const Page *prev_page = dom.pages().at(state.page_idx() - 1);
        if (MScore::vertical_orientation()) {
            y = prev_page->pos().y() + state.page()->height() + MScore::vertical_page_gap;
        } else {
            double gap = (state.page_idx() + ctx.conf().page_number_offset()) & 1 ? MScore::horizontal_page_gap_odd :
                                                                                    MScore::horizontal_page_gap_even;
            x = prev_page->pos().x() + state.page()->width() + gap;
        }
    }
    state.set_page_idx(state.page_idx() + 1);
    state.page()->set_pos(x, y);
}

//---------------------------------------------------------
//   collectPage
//---------------------------------------------------------

void PageLayout::collect_page(LayoutContext &ctx) {
    TRACEFUNC;

    Page *page = ctx.mut_state().page();
    const LayoutConfiguration &conf = ctx.conf();

    LAYOUT_CALL() << "pageNumber: " << page->page_number();

    /* The page count will be wrong unless this is the final page, but we need to
     * take into account the space taken up by headers/footers, when positioning
     * other elements. If there was a page count involved, all headers/footers will
     * be re-computed after all pages are laid out */
    HeaderFooterLayout::layout_header_footer(ctx, page);

    const double slb = conf.style_absolute(Sid::STAFF_LOWER_BORDER);
    const bool break_pages = conf.view_mode() != LayoutMode::SYSTEM;
    const double footer_extension = HeaderFooterLayout::footer_extension(ctx, page);
    const double header_extension = HeaderFooterLayout::header_extension(ctx, page);
    const double header_footer_padding = conf.style_absolute(Sid::STAFF_HEADER_FOOTER_PADDING);
    const double end_y = page->height() - page->bm();
    double y = 0.0;

    System *next_system = nullptr;
    int system_idx = -1;

    auto get_page_lock = [&ctx](MeasureBase *mb) -> const RangeLock * {
        return mb && ctx.conf().view_mode() == LayoutMode::PAGE ? ctx.dom().page_locks()->lock_starting_at(mb) : nullptr;
    };

    const RangeLock *page_lock = nullptr;

    // re-calculate positions for systems before current
    // (they may have been filled on previous layout)
    size_t p_systems = page->systems().size();
    if (p_systems > 0) {
        SystemLayout::restore_layout2(page->system(0), ctx);
        y = page->system(0)->y() + page->system(0)->height();

        // Get existing page lock
        page_lock = get_page_lock(page->system(0)->first());
    } else {
        y = page->tm();
    }

    for (size_t i = 1; i < p_systems; ++i) {
        System *cs = page->system(i);
        System *ps = page->system(i - 1);
        double distance = SystemLayout::min_distance(ps, cs, ctx);
        y += distance;
        cs->set_pos(page->lm(), y);
        SystemLayout::restore_layout2(cs, ctx);
        y += cs->height();
    }

    for (;;) {
        //
        // calculate distance to previous system
        //
        double distance;
        if (ctx.state().prev_system()) {
            distance = SystemLayout::min_distance(ctx.state().prev_system(), ctx.state().cur_system(), ctx);
        } else {
            // this is the first system on page
            // get page lock here
            page_lock = get_page_lock(ctx.state().cur_system()->first());
            if (ctx.state().cur_system()->vbox()) {
                // if the header exists and there is a frame, move the frame downwards
                // to avoid collisions
                distance = header_extension ? header_extension + header_footer_padding : 0.0;
            } else {
                distance = ctx.conf().style_absolute(Sid::STAFF_UPPER_BORDER);
                bool fixed_distance = false;
                for (MeasureBase *mb : ctx.mut_state().cur_system()->measures()) {
                    if (mb->is_measure()) {
                        Measure *m = to_measure(mb);
                        Spacer *sp = m->vspacer_up(0);
                        if (sp) {
                            if (sp->spacer_type() == SpacerType::FIXED) {
                                distance = sp->absolute_gap();
                                fixed_distance = true;
                                break;
                            } else {
                                distance = std::max(distance, sp->absolute_gap());
                            }
                        }
                    }
                }
                if (!fixed_distance) {
                    double top = ctx.state().cur_system()->min_top();
                    // ensure it doesn't collide with header
                    if (header_extension > 0.0) {
                        top += header_extension + header_footer_padding;
                    }
                    distance = std::max(distance, top);
                }
            }
        }

        y += distance;
        ctx.mut_state().cur_system()->set_pos(ctx.state().page()->lm(), y);
        SystemLayout::restore_layout2(ctx.mut_state().cur_system(), ctx);
        ctx.mut_state().page()->append_system(ctx.mut_state().cur_system());
        y += ctx.state().cur_system()->height();

        //
        //  check for page break or if next system will fit on page
        //
        bool collected = false;
        if (ctx.state().range_done()) {
            // take next system unchanged
            if (system_idx > 0) {
                next_system = muse::value(ctx.mut_dom().systems(), system_idx++);
                if (!next_system) {
                    // TODO: handle next movement
                }
            } else {
                next_system = ctx.state().system_list().empty() ? 0 : muse::take_first(ctx.mut_state().system_list());
                if (next_system) {
                    ctx.mut_dom().systems().push_back(next_system);
                }
            }
        } else {
            next_system = SystemLayout::collect_system(ctx);
            if (next_system) {
                collected = true;
            }
        }

        ctx.mut_state().set_prev_system(ctx.mut_state().cur_system());
        assert(ctx.state().cur_system() != next_system);
        ctx.mut_state().set_cur_system(next_system);

        MeasureBase *end_mb = ctx.state().prev_system()->last();
        bool end_of_page_lock = end_mb && end_mb->is_end_of_page_lock();
        const MeasureBase *next_mb = end_mb ? end_mb->next_mm() : nullptr;
        bool page_lock_start = next_mb && next_mb->is_start_of_page_lock();

        bool is_page_break =
            !ctx.state().cur_system() || (break_pages && (ctx.state().prev_system()->page_break() || end_of_page_lock || page_lock_start));

        if (!is_page_break && !page_lock) {
            double dist =
                SystemLayout::min_distance(ctx.state().prev_system(), ctx.state().cur_system(), ctx) + ctx.state().cur_system()->height();
            Box *vbox = ctx.state().cur_system()->vbox();
            if (vbox) {
                if (footer_extension > 0) {
                    dist += footer_extension;
                }
            } else if (!ctx.state().prev_system()->has_fixed_down_distance()) {
                // Down spacers define distance to the following system, not to the page footer.
                // Using them here can visually pin/stretch the current system to page edge.
                double margin = ctx.state().cur_system()->min_bottom();
                // ensure it doesn't collide with footer
                if (footer_extension > 0) {
                    margin += footer_extension + header_footer_padding;
                }
                dist += std::max(margin, slb);
            }
            is_page_break = (y + dist) >= end_y && break_pages;
        }
        if (is_page_break) {
            double dist = ctx.state().prev_system()->min_bottom();
            if (!ctx.conf().is_vertical_spread_enabled()) {
                dist = std::max(dist, ctx.state().prev_system()->spacer_distance(false));
            }
            double footer_padding = 0.0;
            // ensure it doesn't collide with footer
            if (footer_extension > 0) {
                footer_padding = footer_extension + header_footer_padding;
                dist += footer_padding;
            }
            dist = std::max(dist, slb);
            layout_page(ctx, page, end_y - (y + dist), footer_padding, page_lock);
            // if we collected a system we cannot fit onto this page,
            // we need to collect next page in order to correctly set system positions
            if (collected) {
                ctx.mut_state().set_page_old_measure(nullptr);
            }
            break;
        }
    }

    // All staves have been hidden, so correct last system's header
    System *last_sys = ctx.mut_state().page()->systems().back();
    Measure *last_sys_measure = last_sys->first_measure();
    if (last_sys_measure) {
        MeasureLayout::add_system_header(last_sys_measure, false, ctx);
    }

    Fraction stick2 = Fraction(-1, 1);
    for (System *s : page->systems()) {
        for (MeasureBase *mb : s->measures()) {
            if (!mb->is_measure()) {
                continue;
            }
            Measure *m = to_measure(mb);
            if (stick2 == Fraction(-1, 1)) {
                stick2 = m->tick();
            }

            for (size_t track = 0; track < ctx.dom().ntracks(); ++track) {
                for (Segment *segment = m->first(); segment; segment = segment->next()) {
                    EngravingItem *e2 = segment->element(track);
                    if (!e2) {
                        continue;
                    }
                    if (e2->is_chord_rest()) {
                        if (!ctx.dom().staff(track2staff(track))->show()) {
                            continue;
                        }
                        ChordRest *cr = to_chord_rest(e2);
                        if (BeamLayout::is_start_of_cross_beam(cr)) { // layout cross staff beams
                            TLayout::layout_beam(cr->beam(), ctx);
                            BeamLayout::check_cross_pos_and_stem_consistency(cr->beam(), ctx);
                        }
                        if (TupletLayout::not_top_tuplet(cr)) {
                            // fix layout of tuplets
                            DurationElement *de = cr;
                            while (de->tuplet() && de->tuplet()->elements().front() == de) {
                                Tuplet *t = de->tuplet();
                                TLayout::layout_tuplet(t, ctx);
                                de = t;
                            }
                        }

                        if (cr->is_chord()) {
                            Chord *c = to_chord(cr);
                            for (Chord *cc : c->grace_notes()) {
                                if (cc->beam() && cc->beam()->elements().front() == cc) {
                                    TLayout::layout_beam(cc->beam(), ctx);
                                    BeamLayout::check_cross_pos_and_stem_consistency(cc->beam(), ctx);
                                }
                                ChordLayout::layout_spanners(cc, ctx);
                                for (EngravingItem *element : cc->el()) {
                                    if (element->is_slur()) {
                                        TLayout::layout_item(element, ctx);
                                    }
                                }
                            }
                            ArpeggioLayout::layout_arpeggio2(c->arpeggio(), ctx);
                            for (EngravingItem *e : c->el()) {
                                if (e->is_chord_bracket()) {
                                    ChordBracket *bracket = to_chord_bracket(e);
                                    if (c->on_tab_staff()) {
                                        TLayout::layout_item(bracket, ctx);
                                    } else {
                                        ChordBracketLayout::update_vertical_geometry(bracket, ctx);
                                    }
                                }
                            }
                            ChordLayout::layout_spanners(c, ctx);
                            if (c->tremolo_two_chord()) {
                                TremoloTwoChord *t = c->tremolo_two_chord();
                                Chord *c1 = t->chord1();
                                Chord *c2 = t->chord2();
                                if (c1 && c2 && (c1->staff_move() || c2->staff_move())) {
                                    // For cross-staff tremolo, vertical justification may have changed
                                    // staff distances, and therefore also stem directions, so re-compute them
                                    ChordLayout::compute_up(c1, ctx); // This will also call a tremolo layout
                                    ChordLayout::compute_up(c2, ctx);
                                }
                            }
                        }
                    } else if (e2->is_bar_line()) {
                        TLayout::layout_bar_line2(to_bar_line(e2), ctx);
                    }
                }
            }
            MeasureLayout::layout2(m, ctx);
        }
        SystemLayout::layout_system_lock_indicators(s, ctx);
        if (ctx.conf().is_mode(LayoutMode::PAGE)) {
            SystemLayout::layout_page_lock_indicators(s);
        }

        if (StaffVisibilityIndicator *visibility_indicator = s->staff_visibility_indicator()) {
            TLayout::layout_indicator_icon(visibility_indicator, visibility_indicator->mutldata());
        }
    }

    // If this is the last page we layout, we must also relayout the first barlines of the
    // next page, because they may have been altered while collecting the systems.
    MeasureBase *last_of_this_page = ctx.mut_state().page()->systems().back()->measures().back();
    MeasureBase *first_of_next_page = last_of_this_page ? last_of_this_page->next() : nullptr;
    if (first_of_next_page && first_of_next_page->is_measure() && first_of_next_page->tick() > ctx.state().end_tick()) {
        for (Segment &segment : to_measure(first_of_next_page)->segments()) {
            if (!segment.is_type(SegmentType::BAR_LINE_TYPES)) {
                continue;
            }
            for (EngravingItem *item : segment.elist()) {
                if (item && item->is_bar_line()) {
                    TLayout::layout_bar_line2(to_bar_line(item), ctx);
                }
            }
        }
    }

    if (ctx.conf().is_mode(LayoutMode::SYSTEM)) {
        const System *s = page->systems().back();
        double height = s ? s->pos().y() + s->height() + s->min_bottom() : ctx.state().page()->tm();
        page->mutldata()->set_bbox(0.0, 0.0, ctx.conf().lo_width(), height + ctx.state().page()->bm());
    }

    layout_cross_staff_elements(ctx, page);

    for (System *system : page->systems()) {
        SystemLayout::center_elements_between_staves(system);
        ctx.mut_state().remove_system_needing_centering(system);
    }

    if (ctx.conf().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() == TimeSigPlacement::ACROSS_STAVES &&
        ctx.conf().style_b(Sid::TIME_SIG_CENTER_ACROSS_STAVE_GROUP)) {
        for (const System *system : page->systems()) {
            SystemLayout::center_big_time_sigs_across_staves(system);
        }
    }

    MaskLayout::compute_masks(ctx, page);

    layout_system_dividers(ctx, page);

    page->invalidate_bsp_tree();
}

void PageLayout::layout_cross_staff_elements(LayoutContext &ctx, Page *page) {
    for (System *system : page->systems()) {
        if (!system->first_measure()) {
            continue;
        }

        Fraction stick = system->first_measure()->tick();
        Fraction etick = system->end_tick();

        IF_ASSERT_FAILED(stick < etick) { continue; }

        if (etick <= ctx.state().start_tick() || etick > ctx.state().tick()) {
            continue;
        }

        layout_cross_staff_slurs(ctx, system);

        layout_artic_and_fingering_on_cross_staff_beams(ctx, system);
    }
}

void PageLayout::layout_cross_staff_slurs(LayoutContext &ctx, System *system) {
    Fraction stick = system->first_measure()->tick();
    Fraction etick = system->end_tick();

    auto spanners = ctx.dom().spanner_map().find_overlapping(stick.ticks(), etick.ticks());
    for (auto interval : spanners) {
        Spanner *sp = interval.value;
        if (!sp->is_slur() || sp->tick() == system->end_tick()) {
            continue;
        }
        Slur *slur = to_slur(sp);
        if (slur->is_cross_staff() || slur->has_cross_beams()) {
            SlurTieLayout::layout_system(slur, system, ctx);

            ChordRest *scr = to_chord_rest(slur->start_element());
            ChordRest *ecr = to_chord_rest(slur->end_element());
            if (scr && scr->is_chord()) {
                ChordLayout::layout_articulations3(to_chord(scr), slur, ctx);
            }
            if (ecr && ecr->is_chord()) {
                ChordLayout::layout_articulations3(to_chord(ecr), slur, ctx);
            }
        }
    }
}

void PageLayout::layout_artic_and_fingering_on_cross_staff_beams(LayoutContext &ctx, System *system) {
    for (const MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (const Segment &segment : to_measure(mb)->segments()) {
            if (!segment.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *item : segment.elist()) {
                if (!item || !item->is_chord()) {
                    continue;
                }

                Chord *c = to_chord(item);
                bool has_cross_beam = c->beam() && (c->beam()->cross() || c->staff_move() != 0);
                if (!has_cross_beam) {
                    continue;
                }

                ChordLayout::layout_articulations(c, ctx);
                ChordLayout::layout_articulations2(c, ctx, true);

                for (Note *note : c->notes()) {
                    for (EngravingItem *e : note->el()) {
                        if (!e || !e->is_fingering()) {
                            continue;
                        }
                        Fingering *fingering = to_fingering(e);
                        if (fingering->is_on_cross_beam_side()) {
                            TLayout::layout_fingering(fingering, fingering->mutldata());
                            if (fingering->add_to_skyline()) {
                                const Note *n = fingering->note();
                                const RectF r = fingering->ldata()->bbox().translated(fingering->pos() + n->pos() + n->chord()->pos() +
                                                                                      segment.pos() + segment.measure()->pos());
                                system->staff(fingering->note()->chord()->v_staff_idx())->skyline().add(r, fingering);
                            }
                        }
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//   layoutPage
//    restHeight - vertical space which has to be distributed
//                 between systems
//    The algorithm tries to produce most equally spaced
//    systems.
//---------------------------------------------------------

void PageLayout::layout_page(LayoutContext &ctx, Page *page, double rest_height, double footer_padding, bool squeeze_to_fit) {
    TRACEFUNC;
    if (rest_height < 0.0 && !squeeze_to_fit) {
        LOGN("restHeight < 0.0: %f\n", rest_height);
        rest_height = 0;
    }

    int gaps = static_cast<int>(page->systems().size()) - 1;

    std::vector<System *> s_list;

    // build list of systems (excluding last)
    // set initial distance for each to the unstretched minimum distance to next
    for (int i = 0; i < gaps; ++i) {
        System *s1 = page->systems().at(i);
        System *s2 = page->systems().at(i + 1);
        s1->set_distance(s2->y() - s1->y());
        if (s1->vbox() || s2->vbox() || s1->has_fixed_down_distance()) {
            continue;
        }
        s_list.push_back(s1);
    }

    if (s_list.empty() || MScore::no_vertical_stretch || ctx.conf().is_vertical_spread_enabled() ||
        ctx.conf().view_mode() == LayoutMode::SYSTEM) {
        if (ctx.conf().view_mode() == LayoutMode::FLOAT) {
            for (System *system : page->systems()) {
                system->move(PointF(0.0, 0.0));
            }
        } else if ((ctx.conf().view_mode() != LayoutMode::SYSTEM) && ctx.conf().is_vertical_spread_enabled()) {
            distribute_staves(ctx, page, footer_padding, squeeze_to_fit);
        }

        return;
    }

    const bool compact_staves = rest_height <= 0.0;
    auto page_filled = [compact_staves](double space) -> bool { return compact_staves ? (space >= 0.0) : (space <= 0.0); };

    double max_dist = ctx.conf().max_system_distance();

    // allocate space as needed to normalize system distance (bottom of one system to top of next)
    std::sort(s_list.begin(), s_list.end(), [](System *a, System *b) { return a->distance() - a->height() < b->distance() - b->height(); });
    System *s0 = s_list[0];
    double dist = s0->distance() - s0->height(); // distance for shortest system
    for (size_t i = 1; i < s_list.size(); ++i) {
        System *si = s_list[i];
        double ndist = si->distance() - si->height(); // next taller system
        double fill = ndist - dist;                   // amount by which this system distance exceeds next shorter
        if (fill > 0.0) {
            double total_fill = fill * static_cast<double>(i); // space required to add this amount to all shorter systems
            if (total_fill > rest_height) {
                total_fill = rest_height; // too much; adjust amount
                fill = rest_height / static_cast<double>(i);
            }
            for (size_t k = 0; k < i; ++k) { // add amount to all shorter systems
                System *s = s_list[k];
                double d = s->distance() + fill;
                if ((d - s->height()) > max_dist) { // but don't exceed max system distance
                    d = std::max(max_dist + s->height(), s->distance());
                }
                s->set_distance(d);
            }
            rest_height -= total_fill; // reduce available space for next iteration
            if (page_filled(rest_height)) {
                break; // no space left
            }
        }
        dist = ndist; // set up for next iteration
    }

    if (rest_height > 0.0) { // space left?
        double fill = rest_height / static_cast<double>(s_list.size());
        for (System *s : s_list) { // allocate it to systems equally
            double d = s->distance() + fill;
            if ((d - s->height()) > max_dist) { // but don't exceed max system distance
                d = std::max(max_dist + s->height(), s->distance());
            }
            s->set_distance(d);
        }
    }

    double y = page->systems().at(0)->y();
    for (int i = 0; i < gaps; ++i) {
        System *s1 = page->systems().at(i);
        s1->mutldata()->set_pos_y(y);
        y += s1->distance();
    }

    page->systems().back()->mutldata()->set_pos_y(y);
}

void PageLayout::distribute_staves(LayoutContext &ctx, Page *page, double footer_padding, bool squeeze_to_fit) {
    VerticalGapDataList vgdl;

    // Find and classify all gaps between staves.
    int ngaps{0};
    double prev_y_bottom{page->tm()};
    double y_bottom{0.0};
    double spacer_offset{0.0};
    bool vbox{false};
    Spacer *next_spacer{nullptr};
    bool transfer_normal_bracket{false};
    bool transfer_curly_bracket{false};
    for (System *system : page->systems()) {
        if (system->vbox()) {
            VerticalGapData *vgd = new VerticalGapData(&ctx.conf().style(), !ngaps++, system, nullptr, nullptr, nullptr, prev_y_bottom);
            vgd->add_space_around_v_box(true);
            prev_y_bottom = system->y();
            y_bottom = system->y() + system->height();
            vbox = true;
            vgdl.push_back(vgd);
            transfer_normal_bracket = false;
            transfer_curly_bracket = false;
        } else {
            bool new_system{true};
            bool add_space_around_normal_bracket{false};
            bool add_space_around_curly_bracket{false};
            int end_normal_bracket{-1};
            int end_curly_bracket{-1};
            int staff_nr{-1};
            for (SysStaff *sys_staff : system->staves()) {
                const Staff *staff = ctx.dom().staff(++staff_nr);
                IF_ASSERT_FAILED(staff) { break; }
                add_space_around_normal_bracket |= end_normal_bracket == staff_nr;
                add_space_around_curly_bracket |= end_curly_bracket == staff_nr;
                for (const BracketItem *bi : ctx.dom().brackets(staff_nr)) {
                    if (bi->bracket_type() == BracketType::NORMAL) {
                        add_space_around_normal_bracket |= int(staff->idx()) > (end_normal_bracket - 1);
                        end_normal_bracket = std::max(end_normal_bracket, int(staff->idx() + bi->bracket_span()));
                    } else if (bi->bracket_type() == BracketType::BRACE) {
                        add_space_around_curly_bracket |= int(staff->idx()) > (end_curly_bracket - 1);
                        end_curly_bracket = std::max(end_curly_bracket, int(staff->idx() + bi->bracket_span()));
                    }
                }

                if (!sys_staff->show()) {
                    continue;
                }

                VerticalGapData *vgd =
                    new VerticalGapData(&ctx.conf().style(), !ngaps++, system, staff, sys_staff, next_spacer, prev_y_bottom);
                next_spacer = system->down_spacer(staff->idx());

                if (new_system) {
                    vgd->add_space_between_sections();
                    new_system = false;
                }
                if (add_space_around_normal_bracket || transfer_normal_bracket) {
                    vgd->add_space_around_normal_bracket();
                    add_space_around_normal_bracket = false;
                    transfer_normal_bracket = false;
                }
                if (add_space_around_curly_bracket || transfer_curly_bracket) {
                    vgd->add_space_around_curly_bracket();
                    add_space_around_curly_bracket = false;
                    transfer_curly_bracket = false;
                } else if (staff_nr < end_curly_bracket) {
                    vgd->inside_curly_bracket();
                }

                if (vbox) {
                    vgd->add_space_around_v_box(false);
                    vbox = false;
                }

                prev_y_bottom = system->y() + sys_staff->bbox().bottom();
                y_bottom = system->y() + sys_staff->y() + sys_staff->skyline().south().max();
                spacer_offset = sys_staff->skyline().south().max() - sys_staff->bbox().height();
                vgdl.push_back(vgd);
            }
            transfer_normal_bracket = end_normal_bracket >= 0;
            transfer_curly_bracket = end_curly_bracket >= 0;
        }
    }
    --ngaps;
    const double staff_lower_border = ctx.conf().style_absolute(Sid::STAFF_LOWER_BORDER);
    const double combined_bottom_margin = page->bm() + footer_padding;
    const double margin_to_staff = page->bm() + staff_lower_border;
    double space_remaining{std::min(page->height() - combined_bottom_margin - y_bottom, page->height() - margin_to_staff - prev_y_bottom)};

    if (next_spacer) {
        space_remaining -= std::max(0.0, next_spacer->absolute_gap() - spacer_offset - staff_lower_border);
    }
    if (space_remaining <= 0.0 && !squeeze_to_fit) {
        return;
    }

    const bool compact_staves = space_remaining <= 0.0;
    auto break_loop = [compact_staves](double space) -> bool { return compact_staves ? (space >= 0.0) : (space <= 0.0); };

    // Try to make the gaps equal, taking the spread factors and maximum spacing into account.
    static const int max_passes{20}; // Saveguard to prevent endless loops.
    int pass{0};
    while (!muse::real_is_null(space_remaining) && (ngaps > 0) && (++pass < max_passes)) {
        ngaps = 0;
        double smallest{vgdl.smallest()};
        double next_smallest{vgdl.smallest(smallest)};
        if (muse::real_is_null(smallest) || muse::real_is_null(next_smallest)) {
            break;
        }

        if ((next_smallest - smallest) * vgdl.sum_stretch_factor() > space_remaining) {
            next_smallest = smallest + space_remaining / vgdl.sum_stretch_factor();
        }

        double added_space{0.0};
        VerticalGapDataList modified;
        for (VerticalGapData *vgd : vgdl) {
            if (!muse::real_is_null(vgd->spacing() - smallest)) {
                continue;
            }
            double step{next_smallest - vgd->spacing()};
            if (step < 0.0) {
                continue;
            }
            step = vgd->add_spacing(step);
            if (!muse::real_is_null(step)) {
                added_space += step * vgd->factor();
                modified.push_back(vgd);
                ++ngaps;
            }
            if (break_loop(space_remaining - added_space)) {
                break;
            }
        }
        if (break_loop(space_remaining - added_space)) {
            for (VerticalGapData *vgd : modified) {
                vgd->undo_last_add_spacing();
            }
            ngaps = 0;
        } else {
            space_remaining -= added_space;
        }
    }

    // If there is still space left, distribute the space of the staves.
    // However, there is a limit on how much space is added per gap.
    const double max_page_fill = ctx.conf().style_absolute(Sid::MAX_PAGE_FILL_SPREAD);
    space_remaining = std::min(max_page_fill * static_cast<double>(vgdl.size()), space_remaining);
    pass = 0;
    ngaps = 1;
    while (!muse::real_is_null(space_remaining) && !muse::real_is_null(max_page_fill) && (ngaps > 0) && (++pass < max_passes)) {
        ngaps = 0;
        double added_space{0.0};
        double step{space_remaining / vgdl.sum_stretch_factor()};
        for (VerticalGapData *vgd : vgdl) {
            double res{vgd->add_fill_spacing(step, max_page_fill)};
            if (!muse::real_is_null(res)) {
                added_space += res * vgd->factor();
                ++ngaps;
            }
        }
        space_remaining -= added_space;
    }

    std::set<System *> systems;
    double system_shift{0.0};
    double staff_shift{0.0};
    System *prv_system{nullptr};
    for (VerticalGapData *vgd : vgdl) {
        if (vgd->sys_staff) {
            systems.insert(vgd->system);
        }
        system_shift += vgd->actual_added_space();
        if (prv_system == vgd->system) {
            staff_shift += vgd->actual_added_space();
        } else {
            vgd->system->mutldata()->move_y(system_shift);
            if (prv_system) {
                prv_system->set_distance(vgd->system->y() - prv_system->y());
                prv_system->set_height(prv_system->height() + staff_shift);
            }
            staff_shift = 0.0;
        }

        if (vgd->sys_staff) {
            vgd->sys_staff->bbox().translate(0.0, staff_shift);
        }

        prv_system = vgd->system;
    }
    if (prv_system) {
        prv_system->set_height(prv_system->height() + staff_shift);
    }

    for (System *system : systems) {
        SystemLayout::set_measure_height(system, system->height(), ctx);
        SystemHeaderLayout::layout_brackets_vertical(system, ctx);
        SystemHeaderLayout::set_instrument_names_vertical_pos(system, ctx);
    }
    vgdl.delete_all();
}

void PageLayout::layout_system_dividers(LayoutContext &ctx, Page *page) {
    const std::vector<System *> &systems = page->systems();
    for (size_t i = 0; i < systems.size(); ++i) {
        System *system = systems[i];
        if (system->vbox()) {
            continue;
        }

        System *next_system = i + 1 < systems.size() ? systems[i + 1] : nullptr;

        bool needs_divider = next_system && !next_system->vbox();
        bool needs_left_divider = needs_divider && ctx.conf().style_b(Sid::DIVIDER_LEFT);
        bool needs_right_divider = needs_divider && ctx.conf().style_b(Sid::DIVIDER_RIGHT);

        update_system_divider(ctx, system, next_system, SystemDividerType::LEFT, needs_left_divider);
        update_system_divider(ctx, system, next_system, SystemDividerType::RIGHT, needs_right_divider);
    }
}

void PageLayout::update_system_divider(LayoutContext &ctx, System *system, System *next_system, SystemDividerType type,
                                       bool needs_divider) {
    bool left = type == SystemDividerType::LEFT;
    SystemDivider *divider = left ? system->system_divider_left() : system->system_divider_right();
    if (divider) {
        system->remove(divider);
    }

    if (!needs_divider) {
        return;
    }

    DO_ASSERT(system && next_system);

    Score *score = system->score();
    size_t system_idx = muse::index_of(score->systems(), system);
    IF_ASSERT_FAILED(system_idx != muse::nidx) { return; }

    divider = score->system_divider(system_idx, type);
    if (!divider) {
        divider = new SystemDivider(system);
        divider->set_divider_type(type);
        divider->set_generated(true);
        score->add_system_divider(system_idx, divider);
    }

    system->add(divider);

    SystemDivider::LayoutData *ldata = divider->mutldata();
    TLayout::layout_system_divider(divider, ldata, ctx);

    RectF system_b_box = system->ldata()->bbox();
    double x_default = 0.0;
    if (left) {
        if (Measure *first_m = system->first_measure(); first_m && ctx.conf().style_b(Sid::DIVIDER_LEFT_ALIGN_TO_SYSTEM_BARLINE)) {
            // Align to the outermost left system barline
            double left_most_system = first_m->x();
            for (const System *sys : system->page()->systems()) {
                if (sys->vbox()) {
                    continue;
                }
                Measure *fm = sys->first_measure();
                if (fm) {
                    left_most_system = std::min(left_most_system, fm->x());
                }
            }
            x_default = left_most_system - 0.5 * ldata->bbox().width();
        } else {
            x_default = 0.0;
        }
    } else {
        if (Measure *last_m = system->last_measure(); last_m && ctx.conf().style_b(Sid::DIVIDER_RIGHT_ALIGN_TO_SYSTEM_BARLINE)) {
            // Align to the outermost right system barline
            double right_most_system = last_m->x() + last_m->width();
            for (const System *sys : system->page()->systems()) {
                if (sys->vbox()) {
                    continue;
                }
                Measure *lm = sys->last_measure();
                if (lm) {
                    right_most_system = std::max(right_most_system, lm->x() + lm->width());
                }
            }
            x_default = right_most_system - 0.5 * ldata->bbox().width();
        } else {
            x_default = system_b_box.right() - ldata->bbox().width();
        }
    }
    double x_pos = x_default +
                   system->absolute_from_spatium(left ? ctx.conf().style_s(Sid::DIVIDER_LEFT_X) : ctx.conf().style_s(Sid::DIVIDER_RIGHT_X));

    double y_inner_pos =
        -ldata->bbox().top() - 0.5 * ldata->bbox().height() +
        system->absolute_from_spatium(left ? ctx.conf().style_s(Sid::DIVIDER_LEFT_Y) : ctx.conf().style_s(Sid::DIVIDER_RIGHT_Y));

    SysStaff *last_visible_of_this = system->staff(system->last_visible_staff());
    double bottom_of_this_system = last_visible_of_this->bbox().bottom();

    SysStaff *first_visible_of_next = system->staff(system->first_visible_staff());
    double y_next_system = next_system->page_pos().y() - system->page_pos().y();
    double top_of_next_system = y_next_system + first_visible_of_next->bbox().top();

    double y_mid = 0.5 * (bottom_of_this_system + top_of_next_system);

    ldata->set_pos(x_pos, y_inner_pos + y_mid);
}
