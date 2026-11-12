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
#include "systemlayout.h"

#include <cfloat>
#include <unordered_set>

#include "alignmentlayout.h"
#include "autoplace.h"
#include "beamlayout.h"
#include "beamtremololayout.h"
#include "boxlayout.h"
#include "chordlayout.h"
#include "defer.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/box.h"
#include "dom/bracket.h"
#include "dom/bracketitem.h"
#include "dom/chord.h"
#include "dom/dynamic.h"
#include "dom/factory.h"
#include "dom/guitarbend.h"
#include "dom/instrumentname.h"
#include "dom/layoutbreak.h"
#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/measurenumber.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/note.h"
#include "dom/ornament.h"
#include "dom/page.h"
#include "dom/pagelockindicator.h"
#include "dom/parenthesis.h"
#include "dom/part.h"
#include "dom/pedal.h"
#include "dom/playcounttext.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/slur.h"
#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/stafflines.h"
#include "dom/system.h"
#include "dom/systemlockindicator.h"
#include "dom/tie.h"
#include "dom/timesig.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/tuplet.h"
#include "dom/volta.h"
#include "dom/whammybar.h"
#include "dynamicslayout.h"
#include "guitarbendlayout.h"
#include "guitardivelayout.h"
#include "harmonylayout.h"
#include "horizontalspacing.h"
#include "log.h"
#include "lyricslayout.h"
#include "measurelayout.h"
#include "mmrestlayout.h"
#include "realfn.h"
#include "restlayout.h"
#include "slurtielayout.h"
#include "stavesharinglayout.h"
#include "style/defaultstyle.h"
#include "systemheaderlayout.h"
#include "tlayout.h"
#include "tupletlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

//---------------------------------------------------------
//   collectSystem
//---------------------------------------------------------

System *SystemLayout::collect_system(LayoutContext &ctx) {

    if (!ctx.state().cur_measure()) {
        return nullptr;
    }

    const MeasureBase *measure = ctx.dom().systems().empty() ? 0 : ctx.dom().systems().back()->measures().back();
    if (measure) {
        measure = measure->mb_with_preceding_section_break();
    }

    bool first_sys_long_name =
        ctx.conf().style_v(Sid::FIRST_SYSTEM_INST_NAME_VISIBILITY).value<InstrumentLabelVisibility>() == InstrumentLabelVisibility::LONG;
    if (measure) {
        ctx.mut_state().set_first_system(measure->section_break() && !ctx.conf().is_float_mode());
        if (const LayoutBreak *layout_break = measure->section_break_element()) {
            ctx.mut_state().set_first_system_indent(ctx.state().first_system() && ctx.conf().first_system_indent() &&
                                                    layout_break->first_system_indentation());
            ctx.mut_state().set_start_with_long_names(ctx.state().first_system() && first_sys_long_name &&
                                                      layout_break->start_with_long_names());
        }
    } else {
        ctx.mut_state().set_start_with_long_names(ctx.state().first_system() && first_sys_long_name);
    }

    System *system = get_next_system(ctx);
    for (SysStaff *staff : system->staves()) {
        staff->skyline().clear();
    }

    LAYOUT_CALL() << LAYOUT_ITEM_INFO(system);

    double cur_sys_width = 0.0;
    double leading_h_boxes_width = 0.0;
    double target_system_width = ctx.conf().style_d(Sid::PAGE_PRINTABLE_WIDTH) * ::mu::engraving::dpi;
    system->set_width(target_system_width);

    // save state of measure
    MeasureBase *break_measure = nullptr;

    System *old_system = nullptr;

    MeasureState prev_measure_state;
    prev_measure_state.cur_header = ctx.state().cur_measure()->header();
    prev_measure_state.cur_trailer = ctx.state().cur_measure()->trailer();

    const RangeLock *system_lock = ctx.conf().view_mode() == LayoutMode::PAGE || ctx.conf().view_mode() == LayoutMode::SYSTEM ?
                                       ctx.dom().system_locks()->lock_starting_at(ctx.state().cur_measure()) :
                                       nullptr;

    if (system_lock) {
        StaveSharingLayout::update_stave_sharing_for_full_system(system_lock->start_mb(), system_lock->end_mb(), ctx);
    }

    while (ctx.state().cur_measure()) { // collect measure for system
        old_system = ctx.mut_state().cur_measure()->system();
        system->append_measure(ctx.mut_state().cur_measure());

        bool shared_track_map_changed = false;
        if (!system_lock) {
            for (Part *p : ctx.dom().parts()) {
                if (p->is_shared_part()) {
                    SharedPart *sp = to_shared_part(p);
                    prev_measure_state.shared_track_maps[sp] = sp->track_map_at_tick(ctx.mut_state().cur_measure()->tick());
                }
            }

            shared_track_map_changed = StaveSharingLayout::update_stave_sharing_for_last_added_measure(system, ctx);
        }

        MeasureLayout::layout_measure(ctx.mut_state().cur_measure(), ctx);

        if (ctx.state().cur_measure()->is_measure()) {
            Measure *m = to_measure(ctx.mut_state().cur_measure());
            if (!(old_system && old_system->page() && old_system->page() != ctx.state().page())) {
                MeasureLayout::compute_pre_spacing_items(m, ctx);
            }

            if (measure_has_cross_stuff_or_modified_beams(m)) {
                update_cross_beams(system, ctx);
            }

            if (m->is_first_in_system()) {
                leading_h_boxes_width = cur_sys_width;
                SystemLayout::layout_system(system, ctx, cur_sys_width);
                MeasureLayout::add_system_header(m, ctx.state().first_system(), ctx);
            } else {
                bool create_header = ctx.state().prev_measure()->is_h_box() && to_h_box(ctx.state().prev_measure())->create_system_header();
                if (create_header) {
                    MeasureLayout::add_system_header(m, false, ctx);
                } else if (m->header()) {
                    MeasureLayout::remove_system_header(m);
                }
            }

            MeasureLayout::create_end_bar_lines(m, true, ctx);

            if (m->no_break() || system_lock) {
                MeasureLayout::remove_system_trailer(m, ctx);
            } else {
                MeasureLayout::add_system_trailer(m, m->next_measure(), ctx);
            }

            MeasureLayout::set_repeat_courtesies_and_parens(m, ctx);

            MeasureLayout::update_grace_notes(m, ctx);

            if (!system_lock) {
                cur_sys_width = HorizontalSpacing::update_spacing_for_last_added_measure(system);
            }
        } else if (ctx.state().cur_measure()->is_h_box()) {
            if (!system_lock) {
                cur_sys_width = HorizontalSpacing::update_spacing_for_last_added_measure(system);
            }
        } else {
            // vbox:
            MeasureLayout::get_next_measure(ctx);
            SystemLayout::layout2(system, ctx); // compute staff distances
            return system;
        }

        bool do_break =
            !system_lock && system->measures().size() > 1 && cur_sys_width > target_system_width && !ctx.state().prev_measure()->no_break();
        if (do_break) {
            break_measure = ctx.mut_state().cur_measure();
            system->remove_last_measure();
            ctx.mut_state().cur_measure()->set_system(old_system);
            while (ctx.state().prev_measure() && ctx.state().prev_measure()->no_break() && system->measures().size() > 1) {
                ctx.mut_state().set_tick(ctx.state().tick() - ctx.state().cur_measure()->ticks());
                if (ctx.state().cur_measure()->is_measure()) {
                    ctx.mut_state().set_measure_number(to_measure(ctx.state().cur_measure())->measure_number());
                }

                ctx.mut_state().set_next_measure(ctx.mut_state().cur_measure());
                ctx.mut_state().set_cur_measure(ctx.mut_state().prev_measure());
                ctx.mut_state().set_prev_measure(ctx.mut_state().cur_measure()->prev());

                system->remove_last_measure();
                ctx.mut_state().cur_measure()->set_system(old_system);
            }

            if (shared_track_map_changed) {
                // The last added measure caused sharedTrackMap to change. Now we are removing that measure so we must restore it.
                StaveSharingLayout::update_stave_sharing_for_full_system(system->first(), system->last(), ctx);
            }

            break;
        }

        if (old_system && system != old_system && muse::contains(ctx.state().system_list(), old_system)) {
            old_system->clear();
        }

        if (ctx.state().prev_measure() && ctx.state().prev_measure()->is_measure() && ctx.state().prev_measure()->system() == system) {
            Measure *m = to_measure(ctx.mut_state().prev_measure());

            MeasureLayout::create_end_bar_lines(m, false, ctx);

            if (m->trailer()) {
                MeasureLayout::remove_system_trailer(m, ctx);
            }

            MeasureLayout::set_repeat_courtesies_and_parens(m, ctx);

            MeasureLayout::update_grace_notes(m, ctx);

            if (!system_lock) {
                cur_sys_width = HorizontalSpacing::update_spacing_for_last_added_measure(system);
            }

            if (ctx.state().cur_measure()->is_measure()) {
                MeasureLayout::update_key_signatures(to_measure(ctx.state().cur_measure()), ctx);
            }
        }

        const MeasureBase *mb = ctx.state().cur_measure();
        const MeasureBase *next = mb->next_mm();
        bool line_break = false;
        switch (ctx.conf().view_mode()) {
            case LayoutMode::PAGE:
            case LayoutMode::SYSTEM:
                line_break = mb->page_break() || mb->line_break() || mb->section_break() || mb->is_end_of_system_lock() ||
                             mb->is_end_of_page_lock() || (next && next->is_start_of_system_lock()) ||
                             (next && next->is_start_of_page_lock());
                break;
            case LayoutMode::FLOAT:
            case LayoutMode::LINE:
            case LayoutMode::HORIZONTAL_FIXED: line_break = false; break;
        }

        // preserve state of next measure (which is about to become current measure)
        if (ctx.state().next_measure()) {
            MeasureBase *nmb = ctx.mut_state().next_measure();
            if (nmb->is_measure() && ctx.conf().style_b(Sid::CREATE_MULTI_MEASURE_RESTS)) {
                Measure *nm = to_measure(nmb);
                if (nm->has_mm_rest()) {
                    nmb = nm->mm_rest();
                }
            }
            if (nmb->is_measure()) {
                prev_measure_state.clear();
                prev_measure_state.measure = to_measure(nmb);
                prev_measure_state.measure_pos = nmb->x();
                prev_measure_state.measure_width = nmb->width();
                for (Segment &seg : to_measure(nmb)->segments()) {
                    prev_measure_state.element_positions.emplace(&seg, seg.ldata()->pos());
                    prev_measure_state.element_widths.emplace(&seg, seg.width());
                    for (EngravingItem *item : seg.annotations()) {
                        if (item->is_harmony() || item->is_fret_diagram()) {
                            prev_measure_state.element_positions.emplace(item, item->ldata()->pos());
                        }
                    }
                }
            }
            if (!ctx.state().cur_measure()->no_break()) {
                // current measure is not a nobreak,
                // so next measure could possibly start a system
                prev_measure_state.cur_header = nmb->header();
            }
            if (!nmb->no_break()) {
                // next measure is not a nobreak
                // so it could possibly end a system
                prev_measure_state.cur_trailer = nmb->trailer();
            }
        }

        MeasureLayout::get_next_measure(ctx);

        // ElementType nt = lc.curMeasure ? lc.curMeasure->type() : ElementType::INVALID;
        mb = ctx.state().cur_measure();
        if (line_break || !mb || mb->is_v_box_base()) {
            break;
        }
    }

    assert(ctx.state().prev_measure());

    if (ctx.state().end_tick() < ctx.state().prev_measure()->tick()) {
        // we've processed the entire range
        // but we need to continue layout until we reach a system whose last measure is the same as previous layout
        MeasureBase *cur_mb = ctx.mut_state().cur_measure();
        Measure *m = cur_mb && cur_mb->is_measure() ? to_measure(cur_mb) : nullptr;
        bool cur_measure_may_have_joined_beams = m && BeamLayout::measure_may_have_beams_joined_into_next(m);
        if (ctx.state().prev_measure() == ctx.state().system_old_measure() && !cur_measure_may_have_joined_beams) {
            // If current measure has possible beams joining to the next, we need to continue layout. This needs a better solution in future. [M.S.]
            // this system ends in the same place as the previous layout
            // ok to stop
            if (m) {
                // we may have previously processed first measure(s) of next system
                // so now we must restore to original state
                if (m->repeat_start()) {
                    Segment *s = m->find_segment_r(SegmentType::START_REPEAT_BAR_LINE, Fraction(0, 1));
                    if (!s->enabled()) {
                        s->set_enabled(true);
                    }
                }
                const MeasureBase *pbmb = ctx.state().prev_measure()->mb_with_preceding_section_break();
                bool local_first_system = pbmb->section_break() && !ctx.conf().is_mode(LayoutMode::FLOAT);
                MeasureBase *nm = break_measure ? break_measure : m;
                if (prev_measure_state.cur_header) {
                    MeasureLayout::add_system_header(m, local_first_system, ctx);
                } else {
                    MeasureLayout::remove_system_header(m);
                }
                for (;;) {
                    // TODO: what if the nobreak group takes the entire system - is this correct?
                    if (prev_measure_state.cur_trailer && !m->no_break()) {
                        MeasureLayout::add_system_trailer(m, m->next_measure(), ctx);
                    } else {
                        MeasureLayout::remove_system_trailer(m, ctx);
                    }

                    MeasureLayout::set_repeat_courtesies_and_parens(m, ctx);

                    MeasureLayout::update_grace_notes(m, ctx);

                    prev_measure_state.restore_measure(ctx);
                    MeasureLayout::layout_measure_elements(m, ctx);
                    BeamLayout::restore_beams(m, ctx);
                    SystemLayout::restore_old_system_layout(m->system(), ctx);

                    if (m == nm || !m->no_break()) {
                        break;
                    }
                    m = m->next_measure();
                }
            }
            ctx.mut_state().set_range_done(true);
        }
    }

    if (ctx.dom().all_staves_invisible()) {
        // Edge case. Can only happen if all instruments have been deleted.
        return system;
    }

    /*************************************************************
     * SYSTEM NOW HAS A COMPLETE SET OF MEASURES
     * Now perform all operation to finalize system.
     * **********************************************************/

    // Break cross-measure beams
    if (ctx.state().prev_measure() && ctx.state().prev_measure()->is_measure()) {
        Measure *pm = to_measure(ctx.mut_state().prev_measure());
        BeamLayout::break_cross_measure_beams(pm, ctx);
    }

    // Hide empty staves
    hide_empty_staves(system, ctx, ctx.state().first_system());

    // Re-create shapes to account for newly hidden/unhidden staves
    // (and for potential forgotten shape updates, for example in MeasureLayout::setRepeatCourtesiesAndParens)
    for (MeasureBase *mb : system->measures()) {
        if (mb->is_measure()) {
            for (Segment &seg : to_measure(mb)->segments()) {
                seg.create_shapes();
            }
        }
    }

    // Relayout system to account for newly hidden/unhidden staves
    SystemLayout::layout_system(system, ctx, leading_h_boxes_width);

    // Create end barlines and system trailer if needed (cautionary time/key signatures etc)
    Measure *lm = system->last_measure();
    if (lm) {
        MeasureLayout::create_end_bar_lines(lm, true, ctx);
        Measure *nm = lm->next_measure();
        if (nm) {
            MeasureLayout::add_system_trailer(lm, nm, ctx);
        }
    }

    update_time_sig_above_staves_x_pos(system, ctx);

    // Recompute spacing to account for the last changes (barlines, hidden staves, etc)
    cur_sys_width = HorizontalSpacing::compute_spacing_for_full_system(system);

    if (cur_sys_width > target_system_width) {
        HorizontalSpacing::squeeze_system_to_fit(system, cur_sys_width, target_system_width);
    }

    if (should_be_justified(system, cur_sys_width, target_system_width, ctx)) {
        HorizontalSpacing::justify_system(system, cur_sys_width, target_system_width);
    }

    clear_big_time_sig_not_shown(system, ctx);

    // LAYOUT MEASURES
    bool create_brackets = false;
    for (MeasureBase *mb : system->measures()) {
        if (mb->is_measure()) {
            mb->set_system(system);
            Measure *m = to_measure(mb);
            MeasureLayout::layout_measure_elements(m, ctx);
            MeasureLayout::layout_staff_lines(m, ctx);
            if (create_brackets) {
                SystemHeaderLayout::add_brackets(system, to_measure(mb), ctx);
                create_brackets = false;
            }
        } else if (mb->is_h_box()) {
            HBox *cur_h_box = to_h_box(mb);
            TLayout::layout_measure_base(cur_h_box, ctx);
            create_brackets = cur_h_box->create_system_header();
        }
    }

    layout_system_elements(system, ctx);
    SystemLayout::layout2(system, ctx); // compute staff distances

    if (ctx.state().range_done() && old_system && !old_system->measures().empty() &&
        old_system->measures().front()->tick() >= system->end_tick() && !(old_system->page() && old_system->page() != ctx.state().page())) {
        // We may have unfinished layouts of certain elements in the next system
        // - ties & bends (in LayoutChords::updateLineAttachPoints())
        // - fret diagrams & harmony (in layoutMeasure)
        // Restore them to the correct state.
        SystemLayout::restore_old_system_layout(old_system, ctx);
    }

    return system;
}

bool SystemLayout::should_be_justified(System *system, double cur_sys_width, double target_system_width, LayoutContext &ctx) {
    bool should_justify = true;

    MeasureBase *lm = system->measures().back();
    if ((cur_sys_width / target_system_width) < ctx.conf().style_d(Sid::LAST_SYSTEM_FILL_LIMIT)) {
        should_justify = false;
        const MeasureBase *last_mb = ctx.state().cur_measure();

        // For systems with a section break, don't justify
        if (lm && lm->section_break()) {
            last_mb = nullptr;
        }

        // Justify if system is followed by a measure or HBox
        while (last_mb) {
            if (last_mb->is_measure() || last_mb->is_h_box()) {
                should_justify = true;
                break;
            }
            // Frames can contain section breaks too, account for that here
            if (last_mb->section_break()) {
                should_justify = false;
                break;
            }
            last_mb = last_mb->next_measure();
        }
    }

    return should_justify && !MScore::no_horizontal_stretch;
}

void SystemLayout::layout_system_lock_indicators(System *system, [[maybe_unused]] LayoutContext &ctx) {

    const std::vector<SystemLockIndicator *> lock_indicators = system->system_lock_indicators();
    // In PAGE view, at most ONE lock indicator can exist per system.
    assert(lock_indicators.size() <= 1);
    system->delete_system_lock_indicators();

    const RangeLock *lock = system->system_lock();
    if (!lock) {
        return;
    }

    SystemLockIndicator *lock_indicator = Factory::create_system_lock_indicator(system, lock);
    lock_indicator->set_track(0);
    system->add_system_lock_indicator(lock_indicator);

    TLayout::layout_indicator_icon(lock_indicator, lock_indicator->mutldata());
}

void SystemLayout::layout_page_lock_indicators(System *system) {
    Page *page = system->page();
    system->delete_page_lock_indicator();

    IF_ASSERT_FAILED(!page->systems().empty()) { return; }

    const RangeLock *lock = page->page_lock();
    if (!lock || page->systems().back() != system) {
        return;
    }

    PageLockIndicator *lock_indicator = Factory::create_page_lock_indicator(system, lock);
    lock_indicator->set_track(0);
    system->set_page_lock_indicator(lock_indicator);

    TLayout::layout_page_lock_indicator(lock_indicator, lock_indicator->mutldata());
}

//---------------------------------------------------------
//   getNextSystem
//---------------------------------------------------------

System *SystemLayout::get_next_system(LayoutContext &ctx) {
    bool is_v_box = ctx.state().cur_measure()->is_v_box();
    System *system = nullptr;
    if (ctx.state().system_list().empty()) {
        system = Factory::create_system(ctx.mut_dom().score());
        ctx.mut_state().set_system_old_measure(nullptr);
    } else {
        system = muse::take_first(ctx.mut_state().system_list());
        ctx.mut_state().set_system_old_measure(system->measures().empty() ? 0 : system->measures().back());
        system->clear(); // remove measures from system
    }
    ctx.mut_dom().systems().push_back(system);
    if (!is_v_box) {
        size_t nstaves = ctx.dom().nstaves();
        system->adjust_staves_number(nstaves);
        for (staff_idx_t i = 0; i < nstaves; ++i) {
            system->staff(i)->set_show(ctx.dom().staff(i)->show());
        }
    }
    return system;
}

enum class StaffHideMode { HIDE_WHEN_STAFF_EMPTY, HIDE_WHEN_INSTRUMENT_EMPTY, ALWAYS_SHOW };

static StaffHideMode compute_hide_mode(const System *system, const Staff *staff, const staff_idx_t staff_idx,
                                       const bool global_hide_if_empty, bool &has_system_specific_overrides) {
    if (Part *part = staff->part(); part && part->is_shared_part() && staff != part->staves().front()) {
        return StaffHideMode::HIDE_WHEN_STAFF_EMPTY;
    }

    // Check for system-specific overrides
    bool has_system_specific_override_hide = false;
    bool has_system_specific_override_dont_hide = false;

    for (const MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }

        const Measure *measure = to_measure(mb);
        const AutoOnOff hide_if_empty = measure->hide_staff_if_empty(staff_idx);

        has_system_specific_override_hide |= (hide_if_empty == AutoOnOff::ON);
        has_system_specific_override_dont_hide |= (hide_if_empty == AutoOnOff::OFF);
    }

    if (has_system_specific_override_dont_hide) {
        has_system_specific_overrides = true;
        return StaffHideMode::ALWAYS_SHOW;
    } else if (has_system_specific_override_hide) {
        has_system_specific_overrides = true;
        return StaffHideMode::HIDE_WHEN_STAFF_EMPTY;
    }

    // Consider staff setting
    AutoOnOff staff_hide_mode = staff->hide_when_empty();
    switch (staff_hide_mode) {
        case AutoOnOff::ON: return StaffHideMode::HIDE_WHEN_STAFF_EMPTY;
        case AutoOnOff::OFF: return StaffHideMode::ALWAYS_SHOW;
        case AutoOnOff::AUTO: break;
    }

    // Consider part setting
    AutoOnOff part_hide_mode = staff->part()->hide_when_empty();
    switch (part_hide_mode) {
        case AutoOnOff::ON: break;
        case AutoOnOff::OFF: return StaffHideMode::ALWAYS_SHOW;
        case AutoOnOff::AUTO:
            // Consider global setting
            if (!global_hide_if_empty) {
                return StaffHideMode::ALWAYS_SHOW;
            }
    }

    return staff->part()->hide_staves_when_individually_empty() ? StaffHideMode::HIDE_WHEN_STAFF_EMPTY :
                                                                  StaffHideMode::HIDE_WHEN_INSTRUMENT_EMPTY;
}

static bool compute_show_sys_staff(const System *system, const Staff *staff, const staff_idx_t staff_idx, const Fraction &stick,
                                   const SpannerMap::IntervalList &spanners, const StaffHideMode hide_mode) {
    if (hide_mode == StaffHideMode::ALWAYS_SHOW) {
        return true;
    }

    // Check if there are spanners
    for (const auto &spanner : spanners) {
        if (spanner.value->staff() == staff && !spanner.value->system_flag() &&
            !(spanner.stop == stick.ticks() && !spanner.value->is_slur())) {
            return true;
        }
    }

    // Check if the staff is empty in the system
    for (const MeasureBase *m : system->measures()) {
        if (!m->is_measure()) {
            continue;
        }
        const Measure *measure = to_measure(m);
        if (!measure->is_empty(staff_idx)) {
            return true;
        }
    }

    // check if notes moved into this staff
    Part *part = staff->part();
    const size_t n = part->nstaves();
    if (n > 1) {
        staff_idx_t idx = part->staves().front()->idx();
        for (staff_idx_t i = 0; i < n; ++i) {
            staff_idx_t st = idx + i;

            for (MeasureBase *mb : system->measures()) {
                if (!mb->is_measure()) {
                    continue;
                }

                const Measure *m = to_measure(mb);
                bool empty = m->is_empty(st);
                if (hide_mode == StaffHideMode::HIDE_WHEN_INSTRUMENT_EMPTY && !empty) {
                    return true;
                } else if (empty) {
                    continue;
                }

                for (Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
                        ChordRest *cr = s->cr(st * ::mu::engraving::voices + voice);
                        int staff_move = cr ? cr->staff_move() : 0;
                        if (!cr || cr->is_rest() || cr->staff_move() == 0) {
                            // The case staffMove == 0 has already been checked by measure->isEmpty()
                            continue;
                        }
                        if (staff_idx == st + staff_move) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

void SystemLayout::hide_empty_staves(System *system, LayoutContext &ctx, bool is_first_system) {
    if (ctx.dom().nstaves() == 0) {
        // No staves, nothing to hide
        return;
    }

    if (ctx.dom().nstaves() == 1) {
        // One staff, show if not manually hidden score-wide
        const bool show = ctx.dom().staves().front()->show();
        system->staves().front()->set_show(show);
        system->set_has_staff_visibility_indicator(false);
        return;
    }

    const Fraction stick = system->first()->tick();
    const Fraction etick = system->last()->end_tick();
    const auto &spanners = ctx.dom().spanner_map().find_overlapping(stick.ticks(), etick.ticks() - 1);

    const bool global_hide_if_empty =
        ctx.conf().style_b(Sid::HIDE_EMPTY_STAVES) && !(is_first_system && ctx.conf().style_b(Sid::DONT_HIDE_STAVES_IN_FIRST_SYSTEM));

    bool has_system_specific_overrides = false;
    bool has_hidden_staves = false;
    bool system_is_empty = true;
    staff_idx_t staff_idx = 0;

    for (const Staff *staff : ctx.dom().staves()) {
        SysStaff *ss = system->staff(staff_idx);

        DEFER { ++staff_idx; };

        if (!staff->show()) {
            ss->set_show(false);
            continue;
        }

        const StaffHideMode hide_mode = compute_hide_mode(system, staff, staff_idx, global_hide_if_empty, has_system_specific_overrides);
        const bool show = compute_show_sys_staff(system, staff, staff_idx, stick, spanners, hide_mode);
        ss->set_show(show);
        if (show) {
            system_is_empty = false;
        } else {
            has_hidden_staves = true;
        }
    }

    system->set_has_staff_visibility_indicator(has_system_specific_overrides || has_hidden_staves);

    // If the system is empty, unhide the staves with `showIfEntireSystemEmpty` set to true, if any
    const Staff *first_visible = nullptr;
    if (system_is_empty) {
        for (const Staff *staff : ctx.dom().staves()) {
            SysStaff *ss = system->staff(staff->idx());
            if (staff->show_if_entire_system_empty() && !ss->show()) {
                ss->set_show(true);
                system_is_empty = false;
            } else if (!first_visible && staff->show()) {
                first_visible = staff;
            }
        }
    }

    // If there are no such staves, unhide the first one
    if (system_is_empty) {
        const Staff *staff = first_visible ? first_visible : ctx.dom().staves().front();
        SysStaff *ss = system->staff(staff->idx());
        ss->set_show(true);
    }
}

bool SystemLayout::can_change_sys_staff_visibility(const System *system, const staff_idx_t staff_idx) {
    if (system->staves().size() <= 1) {
        // Only one staff; always visible
        return false;
    }

    const Staff *staff = system->score()->staff(staff_idx);

    if (system->staff(staff_idx)->show()) {
        // SysStaff is visible; check if can hide
        const Fraction stick = system->first()->tick();
        const Fraction etick = system->last()->end_tick();
        const auto &spanners = system->score()->spanner_map().find_overlapping(stick.ticks(), etick.ticks() - 1);

        return !compute_show_sys_staff(system, staff, staff_idx, system->first()->tick(), spanners, StaffHideMode::HIDE_WHEN_STAFF_EMPTY);
    }

    // SysStaff is hidden; check if can show
    if (!staff->show()) {
        return false;
    }

    return true;
}

void SystemLayout::update_time_sig_above_staves_x_pos(System *system, LayoutContext &ctx) {
    if (ctx.conf().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() != TimeSigPlacement::ABOVE_STAVES) {
        return;
    }

    staff_idx_t nstaves = ctx.dom().nstaves();
    bool center_on_barline = ctx.conf().style_b(Sid::TIME_SIG_CENTER_ON_BARLINE);

    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        Measure *measure = to_measure(mb);
        for (Segment &seg : measure->segments()) {
            if (!seg.is_type(SegmentType::TIME_SIG_TYPES)) {
                continue;
            }

            std::set<TimeSig *> time_sig_to_keep;
            for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
                TimeSig *time_sig = to_time_sig(seg.element(staff2track(staff_idx)));
                if (!time_sig || !time_sig->show_on_this_staff()) {
                    continue;
                }

                time_sig_to_keep.insert(time_sig);
                if (system->staff(staff_idx)->show()) {
                    continue;
                }

                staff_idx_t next_vis_staff = system->next_visible_staff(staff_idx);
                if (next_vis_staff == muse::nidx) {
                    continue;
                }

                TimeSig *next_vis_time_sig = to_time_sig(seg.element(staff2track(next_vis_staff)));
                if (next_vis_time_sig) {
                    time_sig_to_keep.insert(next_vis_time_sig);
                }
            }

            Segment *prev_barline_seg = nullptr;
            Segment *prev_repeat_announce_time_sig_seg = nullptr;
            if (center_on_barline) {
                for (Segment *prev_seg = seg.prev1(); prev_seg && prev_seg->tick() == seg.tick(); prev_seg = prev_seg->prev1()) {
                    if (prev_seg->is_end_bar_line_type()) {
                        prev_barline_seg = prev_seg;
                    } else if (prev_seg->is_time_sig_repeat_announce_type()) {
                        prev_repeat_announce_time_sig_seg = prev_seg;
                    }
                }
            }

            for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
                TimeSig *time_sig = to_time_sig(seg.element(staff2track(staff_idx)));
                if (!time_sig) {
                    continue;
                }
                Parenthesis *left_paren = time_sig->left_paren();
                Parenthesis *right_paren = time_sig->right_paren();
                if (!muse::contains(time_sig_to_keep, time_sig)) {
                    time_sig->mutldata()->reset(); // Eliminates the shape
                    if (left_paren) {
                        left_paren->mutldata()->reset();
                    }
                    if (right_paren) {
                        right_paren->mutldata()->reset();
                    }
                    continue;
                }

                if (prev_barline_seg && prev_barline_seg->system() == system && !prev_repeat_announce_time_sig_seg) {
                    // Center timeSig on its segment
                    RectF bbox = time_sig->ldata()->bbox();
                    double new_x_pos = -0.5 * (bbox.right() + bbox.left());
                    time_sig->mutldata()->set_pos_x(new_x_pos);
                } else if (!seg.is_time_sig_repeat_announce_type()) {
                    // Left-align to parenthesis if present
                    double x_left_parens = DBL_MAX;
                    if (left_paren) {
                        x_left_parens = left_paren->x() + left_paren->ldata()->bbox().left();
                    }
                    if (x_left_parens != DBL_MAX) {
                        time_sig->mutldata()->move_x(-x_left_parens);
                    }
                } else {
                    // TimeSigRepeatAnnounce: right-align to segment
                    double x_right = -DBL_MAX;
                    x_right = std::max(x_right, time_sig->shape().right() + time_sig->x());
                    if (left_paren) {
                        x_right = std::max(x_right, left_paren->ldata()->bbox().right() + left_paren->x());
                    }
                    if (right_paren) {
                        x_right = std::max(x_right, right_paren->ldata()->bbox().right() + right_paren->x());
                    }
                    if (x_right != -DBL_MAX) {
                        time_sig->mutldata()->move_x(-x_right);
                    }
                }
            }

            seg.create_shapes();
        }
    }
}

void SystemLayout::clear_big_time_sig_not_shown(System *system, LayoutContext &ctx) {
    if (ctx.conf().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() == TimeSigPlacement::NORMAL) {
        return;
    }

    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &seg : to_measure(mb)->segments()) {
            if (!seg.is_type(SegmentType::TIME_SIG_TYPES)) {
                continue;
            }
            for (staff_idx_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
                TimeSig *ts = to_time_sig(seg.element(staff2track(staff_idx)));
                if (!ts) {
                    continue;
                }
                if (!ts->show_on_this_staff() || ts->effective_staff_idx() == muse::nidx) {
                    ts->mutldata()->reset(); // Deletes shape
                }
            }
        }
    }
}

void SystemLayout::layout_sticking(const std::vector<Sticking *> stickings, System *system, LayoutContext &ctx) {
    if (stickings.empty()) {
        return;
    }

    for (Sticking *sticking : stickings) {
        TLayout::layout_item(sticking, ctx);
    }

    std::vector<EngravingItem *> sticking_items(stickings.begin(), stickings.end());
    AlignmentLayout::align_items_for_system(sticking_items, system);
}

void SystemLayout::layout_lyrics(const ElementsToLayout &elements, LayoutContext &ctx) {
    System *system = elements.system;
    // NOTE: in continuous view, this means we layout spanners for the entire score.
    // TODO: find way to optimize this and only layout where necessary.
    Fraction stick = system->measures().front()->tick();
    Fraction etick = system->measures().back()->end_tick();

    for (Spanner *sp : elements.partial_lyrics_lines) {
        TLayout::layout_system(sp, system, ctx);
    }

    //-------------------------------------------------------------
    // Lyric
    //-------------------------------------------------------------
    // Layout lyrics dashes and melisma
    // NOTE: loop on a *copy* of unmanagedSpanners because in some cases
    // the underlying operation may invalidate some of the iterators.
    // TODO: figure out why lyrics lines are in this "unmanagedSpanners" container
    bool dash_on_first_note_syllable = ctx.conf().style().style_b(Sid::LYRICS_SHOW_DASH_IF_SYLLABLE_ON_FIRST_NOTE);
    std::set<Spanner *> unmanaged_spanners = ctx.dom().unmanaged_spanners();
    for (Spanner *sp : unmanaged_spanners) {
        if (!sp->system_flag() && sp->staff() && !sp->staff()->show()) {
            continue;
        }
        bool dash_on_first = dash_on_first_note_syllable && !to_lyrics_line(sp)->is_end_melisma();
        if (sp->tick() >= etick || sp->tick2() < stick || (sp->tick2() == stick && !dash_on_first)) {
            continue;
        }
        TLayout::layout_system(sp, system, ctx);
    }
    LyricsLayout::compute_vertical_positions(system, ctx);
}

void SystemLayout::layout_voltas(const ElementsToLayout &elements_to_layout, LayoutContext &ctx) {
    System *system = elements_to_layout.system;

    process_lines(system, ctx, elements_to_layout.voltas);

    std::set<Volta *> aligned_voltas;
    for (size_t i = 0; i < elements_to_layout.voltas.size(); ++i) {
        Volta *volta1 = to_volta(elements_to_layout.voltas[i]);
        if (aligned_voltas.count(volta1)) {
            continue;
        }

        std::vector<EngravingItem *> voltas_to_align;
        voltas_to_align.push_back(volta1->back_segment());

        for (size_t j = i + 1; j < elements_to_layout.voltas.size(); ++j) {
            Volta *volta2 = to_volta(elements_to_layout.voltas[j]);
            if (volta2->staff_idx() == volta1->staff_idx() && volta2->tick() == volta1->tick2()) {
                voltas_to_align.push_back(volta2->front_segment());
                volta1 = volta2;
            }
        }

        if (voltas_to_align.size() > 1) {
            AlignmentLayout::align_items_group(voltas_to_align, system);
            for (EngravingItem *vs : voltas_to_align) {
                aligned_voltas.insert(to_volta_segment(vs)->volta());
            }
        }
    }
}

void SystemLayout::layout_dynamic_expression_and_hairpins(const ElementsToLayout &elements_to_layout, LayoutContext &ctx) {
    System *system = elements_to_layout.system;

    std::vector<EngravingItem *> dynamics_expr_and_hairpins_to_align;

    for (Dynamic *dynamic : elements_to_layout.dynamics) {
        TLayout::layout_item(dynamic, ctx);
        if (dynamic->autoplace()) {
            Autoplace::autoplace_segment_element(dynamic, dynamic->mutldata());
            dynamics_expr_and_hairpins_to_align.push_back(dynamic);
        }
    }

    for (Expression *e : elements_to_layout.expressions) {
        TLayout::layout_item(e, ctx);
        if (e->add_to_skyline()) {
            dynamics_expr_and_hairpins_to_align.push_back(e);
        }
    }

    process_lines(system, ctx, elements_to_layout.hairpins);

    for (SpannerSegment *spanner_segment : system->spanner_segments()) {
        if (spanner_segment->is_hairpin_segment()) {
            dynamics_expr_and_hairpins_to_align.push_back(spanner_segment);
        }
    }

    AlignmentLayout::align_items_with_their_snapping_chain(dynamics_expr_and_hairpins_to_align, system);
}

void SystemLayout::layout_parenthesis_and_big_time_sigs(const ElementsToLayout &elements_to_layout) {
    System *system = elements_to_layout.system;

    for (Parenthesis *e : elements_to_layout.parenthesis) {
        Segment *s = e->segment();
        if (s->is_type(SegmentType::TIME_SIG_TYPES)) {
            EngravingItem *el = s->element(e->track());
            TimeSig *time_sig = el ? to_time_sig(el) : nullptr;
            if (!time_sig) {
                continue;
            }
            TimeSigPlacement time_sig_placement = time_sig->style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>();
            if (time_sig_placement == TimeSigPlacement::ACROSS_STAVES) {
                if (!time_sig->show_on_this_staff()) {
                    e->mutldata()->reset();
                }
                continue;
            }
        }

        staff_idx_t si = e->staff_idx();
        Measure *m = s->measure();
        system->staff(si)->skyline().add(e->shape().translate(e->pos() + s->pos() + m->pos() + e->staff_offset()));
    }

    for (TimeSig *time_sig : elements_to_layout.time_sig_above_staves) {
        Autoplace::autoplace_segment_element(time_sig, time_sig->mutldata());
    }
}

static void autoplace_harmony(EngravingItem *harmony) {
    FretDiagram *fd_parent = harmony->parent()->is_fret_diagram() ? to_fret_diagram(harmony->parent()) : nullptr;
    if (fd_parent && !fd_parent->visible()) {
        harmony->mutldata()->move_y(-fd_parent->pos().y());
    }
    Autoplace::autoplace_segment_element(harmony, harmony->mutldata());
}

void SystemLayout::layout_harmonies(const std::vector<Harmony *> harmonies, System *system, LayoutContext &ctx) {
    if (!ctx.conf().style_b(Sid::VERTICALLY_ALIGN_CHORD_SYMBOLS)) {
        for (Harmony *harmony : harmonies) {
            autoplace_harmony(harmony);
        }
        return;
    }

    // Only vertically align one chord symbol per tick & staff
    std::set<std::pair<Fraction, staff_idx_t>> harmony_positions;
    std::vector<EngravingItem *> harmony_items_align;
    std::vector<EngravingItem *> harmony_items_no_align;

    for (Harmony *h : harmonies) {
        if (muse::contains(harmony_positions, {h->tick(), h->staff_idx()})) {
            harmony_items_no_align.push_back(h);
            continue;
        }

        TLayout::layout_harmony(h, h->mutldata(), ctx);
        autoplace_harmony(h);
        harmony_items_align.push_back(h);
        harmony_positions.insert({h->tick(), h->staff_idx()});
    }

    AlignmentLayout::align_items_for_system(harmony_items_align, system);

    for (EngravingItem *harmony : harmony_items_no_align) {
        autoplace_harmony(harmony);
    }
}

void SystemLayout::layout_fret_diagrams(const ElementsToLayout &elements, System *system, LayoutContext &ctx) {
    if (!ctx.conf().style_b(Sid::VERTICALLY_ALIGN_CHORD_SYMBOLS)) {
        for (FretDiagram *fret_diag : elements.fret_diagrams) {
            Autoplace::autoplace_segment_element(fret_diag, fret_diag->mutldata());
        }

        for (Harmony *harmony : elements.harmonies) {
            autoplace_harmony(harmony);
        }

        for (FretDiagram *fret_diag : elements.fret_diagrams) {
            if (Harmony *harmony = fret_diag->harmony()) {
                SkylineLine &skl = system->staff(fret_diag->staff_idx())->skyline().north();
                Segment *s = fret_diag->segment();
                Shape harm_shape = harmony->ldata()->shape().translated(harmony->pos() + fret_diag->pos() + s->pos() + s->measure()->pos());
                skl.add(harm_shape);
            }
        }

        return;
    }

    // Only vertically align one fd per tick & staff
    std::set<std::pair<Fraction, staff_idx_t>> fret_harmony_positions;
    std::vector<EngravingItem *> fret_items_align;
    std::vector<EngravingItem *> fret_or_harmony_items_no_align;
    std::vector<Harmony *> harmony_items_align(elements.harmonies.begin(), elements.harmonies.end());

    for (FretDiagram *fd : elements.fret_diagrams) {
        if (muse::contains(fret_harmony_positions, {fd->tick(), fd->staff_idx()})) {
            fret_or_harmony_items_no_align.push_back(fd);
            if (fd->harmony()) {
                harmony_items_align.erase(std::remove(harmony_items_align.begin(), harmony_items_align.end(), fd->harmony()));
                fret_or_harmony_items_no_align.push_back(fd->harmony());
            }
            continue;
        }

        TLayout::layout_fret_diagram(fd, fd->mutldata(), ctx);
        Autoplace::autoplace_segment_element(fd, fd->mutldata());
        fret_items_align.push_back(fd);
        fret_harmony_positions.insert({fd->tick(), fd->staff_idx()});
    }

    // Find harmony with no fret diagram at the same tick as a fret diagram
    for (Harmony *h : elements.harmonies) {
        if (h->get_parent_fret_diagram()) {
            continue;
        }
        if (muse::contains(fret_harmony_positions, {h->tick(), h->staff_idx()})) {
            harmony_items_align.erase(std::remove(harmony_items_align.begin(), harmony_items_align.end(), h));
            fret_or_harmony_items_no_align.push_back(h);
            continue;
        }

        Autoplace::autoplace_segment_element(h, h->mutldata());
        fret_harmony_positions.insert({h->tick(), h->staff_idx()});
    }

    // align 1 fret diagram per tick & staff
    AlignmentLayout::align_items_for_system(fret_items_align, system);

    layout_harmonies(harmony_items_align, system, ctx);

    // autoplace everything else
    for (EngravingItem *item : fret_or_harmony_items_no_align) {
        if (item->is_fret_diagram()) {
            Autoplace::autoplace_segment_element(item, item->mutldata());
            Harmony *harmony = to_fret_diagram(item)->harmony();
            if (harmony) {
                autoplace_harmony(harmony);
            }
        } else if (item->is_harmony()) {
            autoplace_harmony(item);
        }
    }
}

void SystemLayout::layout_system_elements(System *system, LayoutContext &ctx) {

    if (ctx.dom().nstaves() == 0) {
        return;
    }

    // Continuous view does not center items between staves (it collects no pages and doesn't call centerPendingSystems)
    if (!ctx.conf().is_linear_mode()) {
        ctx.mut_state().add_system_needing_centering(system);
    }

    ElementsToLayout elements_to_layout(system);

    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        if (ctx.conf().is_linear_mode() && (mb->tick() < ctx.state().start_tick() || mb->tick() > ctx.state().end_tick())) {
            // in continuous view, entire score is one system but we only need to process the range
            continue;
        }

        Measure *measure = to_measure(mb);

        MeasureLayout::layout_measure_number(measure, ctx);
        MMRestLayout::layout_mm_rest_range(measure, ctx);
        MeasureLayout::layout_play_count_text(measure, ctx);
        MeasureLayout::layout_time_tick_anchors(measure, ctx);

        collect_elements_to_layout(measure, elements_to_layout, ctx);
    }

    const std::vector<Segment *> &sl = elements_to_layout.segments;
    if (sl.empty()) {
        return;
    }

    for (Chord *chord : elements_to_layout.chords) {
        if (TabDurationSymbol *symbol = chord->tab_dur()) {
            TLayout::layout_tab_duration_symbol2(symbol, symbol->mutldata());
            ChordLayout::fill_shape(chord, chord->mutldata());
        }
    }

    for (ChordRest *cr : elements_to_layout.chord_rests) {
        BeamLayout::layout_non_cross_beams(cr, ctx);
    }

    // Beam layout changes grace stem lengths after horizontal spacing. Refresh
    // the chord/group shapes before skyline collection, including on first layout.
    for (Chord *chord : elements_to_layout.chords) {
        for (Chord *grace : chord->grace_notes()) ChordLayout::fill_shape(grace, grace->mutldata(), ctx.conf());
        if (!chord->grace_notes().empty()) {
            GraceNotesGroup &grace_before = ctx.update_grace_group(chord, false);
            GraceNotesGroup &grace_after = ctx.update_grace_group(chord, true);
            TLayout::layout_grace_notes_group2(&grace_before, grace_before.mutldata());
            TLayout::layout_grace_notes_group2(&grace_after, grace_after.mutldata());
        }
    }

    RestLayout::align_rests(elements_to_layout.system, ctx);
    RestLayout::check_full_measure_rest_collisions(elements_to_layout.system, ctx);

    for (BarLine *bl : elements_to_layout.barlines) {
        TLayout::update_barline_shape(bl, bl->mutldata(), ctx);
    }

    create_skylines(elements_to_layout, ctx);

    /* The skyline grows as elements are laid out, so whatever is laid out first ends up closest to the
     * staff. We changed the stacking order of lyrics, from after pedals to before dynamics, so we use
     * this style to keep compatibility for <5.0 scores. This will no longer be necessary if we implement
     * a proper customisable stacking order. */
    const LyricsStackingOrder lyrics_stacking_order = ctx.conf().style_v(Sid::LYRICS_STACKING_ORDER).value<LyricsStackingOrder>();

    layout_ties_and_bends(elements_to_layout, ctx);

    if (ctx.conf().is_linear_mode()) {
        // TODO: get rid of this
        do_layout_note_spanners_linear(system, ctx);
    }

    for (Chord *c : elements_to_layout.chords) {
        ChordLayout::layout_articulations(c, ctx);
        ChordLayout::layout_articulations2(c, ctx);
        ChordLayout::layout_chord_base_fingering(c, system, ctx);
    }

    layout_tuplets(elements_to_layout.chord_rests, ctx);

    collect_spanners_to_layout(elements_to_layout, ctx);

    process_lines(system, ctx, elements_to_layout.slurs);

    for (Spanner *sp : elements_to_layout.slurs) {
        Slur *slur = to_slur(sp);
        ChordRest *scr = to_chord_rest(slur->start_element());
        ChordRest *ecr = to_chord_rest(slur->end_element());
        if (scr && scr->is_chord()) {
            ChordLayout::layout_articulations3(to_chord(scr), slur, ctx);
        }
        if (ecr && ecr->is_chord()) {
            ChordLayout::layout_articulations3(to_chord(ecr), slur, ctx);
        }
        if (slur->is_hammer_on_pull_off()) {
            StaffType *staff_type = slur->staff()->staff_type(slur->tick());
            if ((staff_type->is_tab_staff() && ctx.conf().style_b(Sid::HOPO_ALIGN_LETTERS_TAB_STAVES)) ||
                (!staff_type->is_tab_staff() && ctx.conf().style_b(Sid::HOPO_ALIGN_LETTERS_STANDARD_STAVES))) {
                AlignmentLayout::align_hopo_letters(to_hammer_on_pull_off(slur), system);
            }
        }
    }

    process_lines(system, ctx, elements_to_layout.trills);

    layout_sticking(elements_to_layout.stickings, system, ctx);

    for (EngravingItem *item : elements_to_layout.fermatas_and_tremolo_bars) {
        TLayout::layout_item(item, ctx);
    }

    for (FiguredBass *item : elements_to_layout.figured_bass) {
        TLayout::layout_item(item, ctx);
        if (item->autoplace()) {
            Autoplace::autoplace_segment_element(item, item->mutldata(), true);
        }
    }

    if (lyrics_stacking_order == LyricsStackingOrder::LYRICS_BEFORE_DYNAMICS) {
        layout_lyrics(elements_to_layout, ctx);
    }

    layout_dynamic_expression_and_hairpins(elements_to_layout, ctx);

    process_lines(system, ctx, elements_to_layout.all_other_spanners);

    for (MeasureNumber *mno : elements_to_layout.measure_numbers) {
        if (!mno->visible()) {
            continue;
        }
        Autoplace::autoplace_measure_element(mno, mno->mutldata());
        system->staff(mno->staff_idx())
            ->skyline()
            .add(mno->ldata()->bbox().translated(mno->measure()->pos() + mno->pos() + mno->staff_offset()), mno);
    }

    for (MMRestRange *mmrr : elements_to_layout.mmr_ranges) {
        Autoplace::autoplace_measure_element(mmrr, mmrr->mutldata());
        system->staff(mmrr->staff_idx())->skyline().add(mmrr->ldata()->bbox().translated(mmrr->measure()->pos() + mmrr->pos()), mmrr);
    }

    process_lines(system, ctx, elements_to_layout.ottavas);
    process_lines(system, ctx, elements_to_layout.pedal, /*align=*/true);

    if (lyrics_stacking_order == LyricsStackingOrder::LYRICS_AFTER_PEDALS) {
        layout_lyrics(elements_to_layout, ctx);
    }

    for (HarpPedalDiagram *hpd : elements_to_layout.harp_diagrams) {
        TLayout::layout_item(hpd, ctx);
    }

    bool has_fret_diagram = elements_to_layout.fret_diagrams.size() > 0;
    if (!has_fret_diagram) {
        layout_harmonies(elements_to_layout.harmonies, system, ctx);
    }

    for (StaffTextBase *st : elements_to_layout.staff_text) {
        TLayout::layout_item(st, ctx);
    }

    for (InstrumentChange *ic : elements_to_layout.instr_changes) {
        TLayout::layout_item(ic, ctx);
    }

    for (EngravingItem *item : elements_to_layout.play_tech_capo_string_tun_triplet_feel) {
        TLayout::layout_item(item, ctx);
    }

    if (has_fret_diagram) {
        layout_fret_diagrams(elements_to_layout, system, ctx);
    }

    for (SystemText *system_text : elements_to_layout.system_text) {
        TLayout::layout_system_text(system_text, system_text->mutldata());
    }

    layout_voltas(elements_to_layout, ctx);

    for (RehearsalMark *rm : elements_to_layout.reh_marks) {
        TLayout::layout_item(rm, ctx);
    }

    std::vector<EngravingItem *> tempo_elements_to_align;
    for (TempoText *tt : elements_to_layout.tempo_text) {
        TLayout::layout_item(tt, ctx);
        tempo_elements_to_align.push_back(tt);
    }

    process_lines(system, ctx, elements_to_layout.tempo_change_lines);
    for (SpannerSegment *spanner_seg : system->spanner_segments()) {
        if (spanner_seg->is_gradual_tempo_change_segment()) {
            tempo_elements_to_align.push_back(spanner_seg);
        }
    }

    for (PlayCountText *pt : elements_to_layout.play_count_text) {
        TLayout::layout_play_count_text(pt, pt->mutldata());
        if (pt->autoplace()) {
            Autoplace::autoplace_segment_element(pt, pt->mutldata());
        }
    }

    AlignmentLayout::align_items_with_their_snapping_chain(tempo_elements_to_align, system);

    for (RehearsalMark *rehears_mark : elements_to_layout.reh_marks) {
        Autoplace::autoplace_segment_element(rehears_mark, rehears_mark->mutldata());
    }

    for (EngravingItem *item : elements_to_layout.markers_and_jumps) {
        TLayout::layout_item(item, ctx);
    }

    for (Image *image : elements_to_layout.images) {
        TLayout::layout_item(image, ctx);
    }

    layout_parenthesis_and_big_time_sigs(elements_to_layout);
}

void SystemLayout::collect_elements_to_layout(Measure *measure, ElementsToLayout &elements, const LayoutContext &ctx) {
    elements.measures.push_back(measure);

    System *system = elements.system;
    for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        if (measure->show_measure_number_on_staff(staff_idx)) {
            if (MeasureNumber *mno = measure->measure_number(staff_idx)) {
                elements.measure_numbers.push_back(mno);
            }
        }

        if (!system->staff(staff_idx)->show()) {
            continue;
        }

        MMRestRange *mmrr = measure->mm_range_text(staff_idx);
        if (mmrr && mmrr->add_to_skyline()) {
            elements.mmr_ranges.push_back(mmrr);
        }

        for (EngravingItem *item : measure->el()) {
            if (item->effective_staff_idx() == staff_idx && (item->is_marker() || item->is_jump())) {
                elements.markers_and_jumps.push_back(item);
            }
        }
    }

    track_idx_t n_tracks = ctx.dom().ntracks();
    for (Segment *s = measure->first(); s; s = s->next()) {
        if (s->is_chord_rest_type() || !s->annotations().empty()) {
            elements.segments.push_back(s);
        }

        for (track_idx_t track = 0; track < n_tracks; /* intentionally empty*/) {
            if (s->has_time_sig_above_staves()) {
                TimeSig *time_sig = to_time_sig(s->element(track));
                if (time_sig && time_sig->show_on_this_staff()) {
                    elements.time_sig_above_staves.push_back(time_sig);
                }
                track += ::mu::engraving::voices;
                continue;
            }

            if (!system->staff(track2staff(track))->show()) {
                track += ::mu::engraving::voices;
                continue;
            }

            if (s->is_type(SegmentType::BAR_LINE_TYPES)) {
                if (BarLine *bl = to_bar_line(s->element(track))) {
                    elements.barlines.push_back(bl);
                }
                track += ::mu::engraving::voices;
                continue;
            }

            if (s->is_chord_rest_type()) {
                if (ChordRest *cr = to_chord_rest(s->element(track))) {
                    elements.chord_rests.push_back(cr);
                    if (cr->is_chord()) {
                        elements.chords.push_back(to_chord(cr));

                        auto collect_bends = [&elements, system](Chord *chord) {
                            for (Note *note : chord->notes()) {
                                for (Spanner *sp : note->spanner_back()) {
                                    if (sp->is_guitar_bend()) {
                                        elements.guitar_bends.push_back(to_guitar_bend(sp));
                                    }
                                }

                                for (Spanner *sp : note->spanner_for()) {
                                    if (sp->is_guitar_bend()) {
                                        GuitarBend *bend = to_guitar_bend(sp);
                                        Note *end_note = bend->end_note();
                                        if (bend->bend_type() == GuitarBendType::SLIGHT_BEND ||
                                            (end_note && end_note->tick() >= system->end_tick())) {
                                            elements.guitar_bends.push_back(bend);
                                        }
                                    }
                                }
                            }
                        };

                        Chord *chord = to_chord(cr);
                        for (Chord *grace : chord->grace_notes_before()) {
                            collect_bends(grace);
                        }
                        collect_bends(chord);
                        for (Chord *grace : chord->grace_notes_after()) {
                            collect_bends(grace);
                        }
                    }
                }
                ++track;
                continue;
            }

            ++track;
        }

        for (EngravingItem *item : s->annotations()) {
            if (!item->system_flag() && !system->staff(item->staff_idx())->show()) {
                continue;
            }
            switch (item->type()) {
                case ElementType::STICKING: elements.stickings.push_back(to_sticking(item)); break;
                case ElementType::FERMATA:
                case ElementType::TREMOLOBAR: elements.fermatas_and_tremolo_bars.push_back(item); break;
                case ElementType::FIGURED_BASS: elements.figured_bass.push_back(to_figured_bass(item)); break;
                case ElementType::DYNAMIC: elements.dynamics.push_back(to_dynamic(item)); break;
                case ElementType::EXPRESSION: elements.expressions.push_back(to_expression(item)); break;
                case ElementType::HARP_DIAGRAM: elements.harp_diagrams.push_back(to_harp_pedal_diagram(item)); break;
                case ElementType::FRET_DIAGRAM:
                    elements.fret_diagrams.push_back(to_fret_diagram(item));
                    if (Harmony *h = to_fret_diagram(item)->harmony()) {
                        elements.harmonies.push_back(h);
                    }
                    break;
                case ElementType::STAFF_TEXT:
                case ElementType::STAVE_SHARING_LABEL: elements.staff_text.push_back(to_staff_text_base(item)); break;
                case ElementType::INSTRUMENT_CHANGE: elements.instr_changes.push_back(to_instrument_change(item)); break;
                case ElementType::PLAYTECH_ANNOTATION:
                case ElementType::CAPO:
                case ElementType::STRING_TUNINGS:
                case ElementType::TRIPLET_FEEL: elements.play_tech_capo_string_tun_triplet_feel.push_back(item); break;
                case ElementType::SYSTEM_TEXT: elements.system_text.push_back(to_system_text(item)); break;
                case ElementType::REHEARSAL_MARK: elements.reh_marks.push_back(to_rehearsal_mark(item)); break;
                case ElementType::TEMPO_TEXT: elements.tempo_text.push_back(to_tempo_text(item)); break;
                case ElementType::IMAGE: elements.images.push_back(to_image(item)); break;
                case ElementType::PARENTHESIS: elements.parenthesis.push_back(to_parenthesis(item)); break;
                case ElementType::HARMONY: elements.harmonies.push_back(to_harmony(item)); break;
                case ElementType::PLAY_COUNT_TEXT: elements.play_count_text.push_back(to_play_count_text(item)); break;
                default: break;
            }
        }
    }
}

void SystemLayout::collect_spanners_to_layout(ElementsToLayout &elements, const LayoutContext &ctx) {
    const System *system = elements.system;

    // NOTE: in continuous view, this means we layout spanners for the entire score.
    // TODO: find way to optimize this and only layout where necessary.
    Fraction stick = system->measures().front()->tick();
    Fraction etick = system->measures().back()->end_tick();

    auto spanners = ctx.dom().spanner_map().find_overlapping(stick.ticks(), etick.ticks());
    std::sort(spanners.begin(), spanners.end(), [](const auto &sp1, const auto &sp2) { return sp1.value->tick() < sp2.value->tick(); });

    std::vector<Spanner *> all_spanners;
    all_spanners.reserve(spanners.size());
    for (auto item : spanners) {
        all_spanners.push_back(item.value);
    }

    for (Spanner *spanner : all_spanners) {
        if (!spanner->system_flag() && !system->staff(spanner->staff_idx())->show()) {
            continue;
        }
        if (spanner->tick() >= etick || spanner->tick2() < stick) {
            continue;
        }

        if (spanner->tick2() == stick) {
            // Only these two spanner types are laid out if at the end of the previous system
            // TODO: pedal makes sense, but slur doesn't... to figure out
            if (spanner->is_slur() && !to_slur(spanner)->is_cross_staff() && !to_slur(spanner)->has_cross_beams()) {
                elements.slurs.push_back(spanner);
            } else if (spanner->is_pedal() && to_pedal(spanner)->connect45_hook_to_next()) {
                elements.pedal.push_back(spanner);
            }
        } else {
            switch (spanner->type()) {
                case ElementType::SLUR:
                case ElementType::HAMMER_ON_PULL_OFF:
                    if (!to_slur(spanner)->is_cross_staff() && !to_slur(spanner)->has_cross_beams()) {
                        elements.slurs.push_back(spanner);
                    }
                    break;
                case ElementType::PEDAL: elements.pedal.push_back(spanner); break;
                case ElementType::TRILL: elements.trills.push_back(spanner); break;
                case ElementType::VOLTA: elements.voltas.push_back(spanner); break;
                case ElementType::HAIRPIN: elements.hairpins.push_back(spanner); break;
                case ElementType::GRADUAL_TEMPO_CHANGE: elements.tempo_change_lines.push_back(spanner); break;
                case ElementType::PARTIAL_LYRICSLINE: elements.partial_lyrics_lines.push_back(spanner); break;
                case ElementType::OTTAVA:
                    if (!spanner->staff()->staff_type()->is_tab_staff()) {
                        elements.ottavas.push_back(spanner);
                    }
                    break;
                default: elements.all_other_spanners.push_back(spanner); break;
            }
        }
    }
}

void SystemLayout::create_skylines(const ElementsToLayout &elements_to_layout, LayoutContext &ctx) {
    System *system = elements_to_layout.system;
    for (size_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        SysStaff *ss = system->staff(staff_idx);
        Skyline &skyline = ss->skyline();
        skyline.clear();
        for (Measure *m : elements_to_layout.measures) {
            if (m->staff_lines(staff_idx)->add_to_skyline()) {
                ss->skyline().add(m->staff_lines(staff_idx)->ldata()->bbox().translated(m->pos()), m->staff_lines(staff_idx));
            }
            for (Segment &s : m->segments()) {
                if (!s.enabled()) {
                    continue;
                }
                PointF p(s.pos() + m->pos());
                if (s.is_type(SegmentType::BAR_LINE_TYPES)) {
                    BarLine *bl = to_bar_line(s.element(staff_idx * ::mu::engraving::voices));
                    if (bl && bl->add_to_skyline()) {
                        skyline.add(bl->shape().translated(bl->pos() + p + bl->staff_offset()));
                    }
                } else if (s.is_type(SegmentType::TIME_SIG_TYPES)) {
                    TimeSig *ts = to_time_sig(s.element(staff_idx * ::mu::engraving::voices));
                    if (ts && ts->add_to_skyline() && ts->show_on_this_staff()) {
                        TimeSigPlacement time_sig_placement = ts->style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>();
                        if (time_sig_placement != TimeSigPlacement::ACROSS_STAVES) {
                            skyline.add(ts->shape().translate(ts->pos() + p + ts->staff_offset()));
                        }
                    }
                } else {
                    track_idx_t strack = staff_idx * ::mu::engraving::voices;
                    track_idx_t etrack = strack + ::mu::engraving::voices;
                    for (EngravingItem *e : s.elist()) {
                        if (!e) {
                            continue;
                        }
                        track_idx_t effective_track = e->v_staff_idx() * ::mu::engraving::voices + e->voice();
                        if (effective_track < strack || effective_track >= etrack) {
                            continue;
                        }

                        // add element to skyline
                        if (e->add_to_skyline()) {
                            const PointF offset = e->staff_offset();
                            Shape shape = e->shape();
                            // add grace notes to skyline
                            if (e->is_chord()) {
                                Chord *chord = to_chord(e);
                                const GraceNotesGroup *grace_before = ctx.grace_group(chord, false);
                                const GraceNotesGroup *grace_after = ctx.grace_group(chord, true);
                                if (grace_before && !grace_before->empty()) {
                                    skyline.add(grace_before->shape().translate(grace_before->pos() + p + offset));
                                }
                                if (grace_after && !grace_after->empty()) {
                                    skyline.add(grace_after->shape().translate(grace_after->pos() + p + offset));
                                }

                                // If present, add ornament cue note to skyline
                                Ornament *ornament = chord->find_ornament();
                                if (ornament) {
                                    Chord *cue = ornament->cue_note_chord();
                                    if (cue && cue->up_note()->visible()) {
                                        skyline.add(cue->shape().translate(cue->pos() + p + cue->staff_offset()));
                                    }
                                }

                                // Don't include cross-staff arpeggios
                                shape.remove_if([chord](ShapeElement &s) {
                                    return (s.item()->is_arpeggio() || s.item()->is_chord_bracket()) &&
                                           to_arpeggio(s.item()) == chord->span_arpeggio();
                                });
                                Arpeggio *arp = chord->span_arpeggio();
                                if (arp) {
                                    RectF staff_bbox = ss->bbox();
                                    RectF arp_bbox = arp->ldata()->bbox().translated(e->pos() + p + offset);
                                    if (chord->track() == arp->track()) {
                                        staff_bbox.set_top(arp_bbox.top());
                                    } else if (chord->track() == arp->end_track()) {
                                        staff_bbox.set_bottom(arp_bbox.bottom());
                                    }
                                    shape.add(arp_bbox & staff_bbox, arp);
                                }
                            }
                            skyline.add(shape.translate(e->pos() + p + offset));
                        }

                        // add tremolo to skyline
                        if (e->is_chord()) {
                            Chord *ch = item_cast<Chord *>(e);
                            // tremoloSingleChord is added directly to chord shape
                            if (ch->tremolo_two_chord()) {
                                TremoloTwoChord *t = ch->tremolo_two_chord();
                                Chord *c1 = t->chord1();
                                Chord *c2 = t->chord2();
                                if (c1 && !c1->staff_move() && c2 && !c2->staff_move()) {
                                    if (t->chord() == e && t->add_to_skyline()) {
                                        skyline.add(t->shape().translate(t->pos() + e->pos() + p));
                                    }
                                }
                            }
                        }

                        // add beams to skline
                        if (e->is_chord_rest()) {
                            ChordRest *cr = to_chord_rest(e);
                            if (BeamLayout::is_start_of_non_cross_beam(cr)) {
                                Beam *b = cr->beam();
                                b->add_skyline(skyline);
                            }
                        }
                    }
                }
            }
        }
    }
}

void SystemLayout::do_layout_ties(System *system, const std::vector<Segment *> &sl, const Fraction &stick,
                                  [[maybe_unused]] const Fraction &etick, LayoutContext &ctx) {

    for (Segment *s : sl) {
        for (EngravingItem *e : s->elist()) {
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *c = to_chord(e);
            for (Chord *ch : c->grace_notes()) {
                layout_ties(ch, system, stick, ctx);
            }
            layout_ties(c, system, stick, ctx);
        }
    }
}

void SystemLayout::layout_tuplets(const std::vector<ChordRest *> &chord_rests, LayoutContext &ctx) {
    std::set<Tuplet *> laidout_tuplets;
    for (auto rev_iter = chord_rests.rbegin(); rev_iter != chord_rests.rend(); ++rev_iter) {
        ChordRest *cr = *rev_iter;
        Tuplet *tuplet = cr->top_tuplet();

        if (!tuplet || muse::contains(laidout_tuplets, tuplet)) {
            continue;
        }

        TupletLayout::layout_tuplet_and_nested_tuplets(tuplet, ctx); // this lays out also the inner tuplets
        laidout_tuplets.insert(tuplet);
    }
}

void SystemLayout::layout_ties_and_bends(const ElementsToLayout &elements_to_layout, LayoutContext &ctx) {
    System *system = elements_to_layout.system;
    if (elements_to_layout.measures.empty()) {
        return;
    }
    Fraction stick = elements_to_layout.measures.front()->tick();

    for (Chord *chord : elements_to_layout.chords) {
        for (Chord *grace : chord->grace_notes_before()) {
            layout_ties(grace, system, stick, ctx);
        }
        layout_ties(chord, system, stick, ctx);
        for (Chord *grace : chord->grace_notes_after()) {
            layout_ties(grace, system, stick, ctx);
        }
    }

    GuitarDiveLayout::update_dive_sequences(elements_to_layout.guitar_bends, ctx);

    for (GuitarBend *bend : elements_to_layout.guitar_bends) {
        /* Bends which cross a system break are collected by both systems they touch, so we
         * lay out only the segment which belongs to this one: */
        TLayout::layout_guitar_bend(bend, ctx, system);
    }
}

void SystemLayout::layout_note_anchored_spanners(System *system, Chord *chord) {
    // Add all spanners attached to notes, otherwise these will be removed if outside of the layout range
    for (Note *note : chord->notes()) {
        for (Spanner *spanner : note->spanner_for()) {
            for (SpannerSegment *spanner_seg : spanner->spanner_segments()) {
                spanner_seg->move_to_system(system);
            }
        }
    }
}

void SystemLayout::do_layout_note_spanners_linear(System *system, LayoutContext &ctx) {
    constexpr Fraction start = Fraction(0, 1);
    for (Measure *measure = system->first_measure(); measure; measure = measure->next_measure()) {
        for (Segment *segment = measure->first(); segment; segment = segment->next()) {
            if (!segment->is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *e : segment->elist()) {
                if (!e || !e->is_chord()) {
                    continue;
                }
                Chord *c = to_chord(e);
                for (Chord *ch : c->grace_notes()) {
                    layout_ties(ch, system, start, ctx);
                    layout_note_anchored_spanners(system, ch);
                }
                layout_ties(c, system, start, ctx);
                layout_note_anchored_spanners(system, c);
            }
        }
    }
}

void SystemLayout::process_lines(System *system, LayoutContext &ctx, const std::vector<Spanner *> &lines, bool align) {
    std::vector<SpannerSegment *> segments;
    for (Spanner *sp : lines) {
        SpannerSegment *ss = TLayout::layout_system(sp, system, ctx); // create/layout spanner segment for this system
        if (ss->autoplace()) {
            segments.push_back(ss);
        }
    }

    if (align && segments.size() > 1) {
        const size_t nstaves = system->staves().size();
        const double default_y = segments[0]->ldata()->pos().y();
        std::vector<double> y_above(nstaves, -DBL_MAX);
        std::vector<double> y_below(nstaves, -DBL_MAX);

        for (SpannerSegment *ss : segments) {
            if (ss->visible()) {
                double &staff_y = ss->spanner() && ss->spanner()->place_above() ? y_above[ss->staff_idx()] : y_below[ss->staff_idx()];
                staff_y = std::max(staff_y, ss->ldata()->pos().y());
            }
        }
        for (SpannerSegment *ss : segments) {
            const double &staff_y = ss->spanner() && ss->spanner()->place_above() ? y_above[ss->staff_idx()] : y_below[ss->staff_idx()];
            if (staff_y > -DBL_MAX) {
                ss->mutldata()->set_pos_y(staff_y);
            } else {
                ss->mutldata()->set_pos_y(default_y);
            }
        }
    }

    if (segments.size() > 0 && segments.front()->is_slur_segment()) {
        SlurTieLayout::adjust_overlapping_slurs(system->spanner_segments());
    }

    //
    // Fix harmonic marks and vibrato overlaps
    //
    SpannerSegment *prev_segment = nullptr;
    bool fixed = false;

    for (SpannerSegment *ss : segments) {
        if (fixed) {
            fixed = false;
            prev_segment = ss;
            continue;
        }
        if (prev_segment) {
            if (prev_segment->visible() && ss->visible() && prev_segment->is_harmonic_mark_segment() && ss->is_vibrato_segment() &&
                muse::real_is_equal(prev_segment->x(), ss->x())) {
                double diff = ss->ldata()->bbox().bottom() - prev_segment->ldata()->bbox().bottom() + prev_segment->ldata()->bbox().top();
                prev_segment->mutldata()->move_y(diff);
                fixed = true;
            }
            if (prev_segment->visible() && ss->visible() && prev_segment->is_vibrato_segment() && ss->is_harmonic_mark_segment() &&
                muse::real_is_equal(prev_segment->x(), ss->x())) {
                double diff = prev_segment->ldata()->bbox().bottom() - ss->ldata()->bbox().bottom() + ss->ldata()->bbox().top();
                ss->mutldata()->move_y(diff);
                fixed = true;
            }
        }

        prev_segment = ss;
    }

    //
    // add shapes to skyline
    //
    for (SpannerSegment *ss : segments) {
        if (ss->add_to_skyline()) {
            staff_idx_t stf_idx = ss->effective_staff_idx();
            if (stf_idx == muse::nidx) {
                continue;
            }
            system->staff(stf_idx)->skyline().add(ss->shape().translate(ss->pos()));
            if (ss->is_hammer_on_pull_off_segment()) {
                TLayout::layout_hammer_on_pull_off_segment(to_hammer_on_pull_off_segment(ss), ctx);
            }
        }
    }
}

void SystemLayout::layout_ties(Chord *ch, System *system, const Fraction &stick, LayoutContext &ctx) {
    SysStaff *staff = system->staff(ch->staff_idx());
    if (!staff->show()) {
        return;
    }
    std::vector<TieSegment *> stacked_forward_ties;
    std::vector<TieSegment *> stacked_backward_ties;
    for (Note *note : ch->notes()) {
        Tie *t = note->tie_for();
        if (t && !t->is_laissez_vib()) {
            TieSegment *ts = SlurTieLayout::layout_tie_for(t, system);
            if (ts && ts->add_to_skyline()) {
                staff->skyline().add(ts->shape().translate(ts->pos()));
                stacked_forward_ties.push_back(ts);
            }
        }
        t = note->tie_back();
        if (t) {
            if (note->incoming_partial_tie() || t->start_note()->tick() < stick) {
                TieSegment *ts = SlurTieLayout::layout_tie_back(t, system, ctx);
                if (ts && ts->add_to_skyline()) {
                    staff->skyline().add(ts->shape().translate(ts->pos()));
                    stacked_backward_ties.push_back(ts);
                }
            }
        }
    }

    SlurTieLayout::layout_laissez_vib_chord(ch, ctx);

    if (!ch->staff_type()->is_tab_staff()) {
        SlurTieLayout::resolve_vertical_tie_collisions(stacked_forward_ties);
        SlurTieLayout::resolve_vertical_tie_collisions(stacked_backward_ties);
    }
}

bool SystemLayout::measure_has_cross_stuff_or_modified_beams(const Measure *measure) {
    for (const Segment &seg : measure->segments()) {
        if (!seg.is_chord_rest_type()) {
            continue;
        }
        for (const EngravingItem *e : seg.elist()) {
            if (!e || !e->is_chord_rest()) {
                continue;
            }
            const Beam *beam = to_chord_rest(e)->beam();
            if (beam && (beam->cross() || beam->user_modified())) {
                return true;
            }
            const Chord *c = e->is_chord() ? to_chord(e) : nullptr;
            if (c && c->tremolo_two_chord()) {
                const TremoloTwoChord *trem = c->tremolo_two_chord();
                const Chord *c1 = trem->chord1();
                const Chord *c2 = trem->chord2();
                if (trem->user_modified() || c1->staff_move() != c2->staff_move()) {
                    return true;
                }
            }
            if (e->is_chord() && !to_chord(e)->grace_notes().empty()) {
                for (const Chord *grace : to_chord(e)->grace_notes()) {
                    if (grace->beam() && (grace->beam()->cross() || grace->beam()->user_modified())) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void SystemLayout::update_cross_beams(System *system, LayoutContext &ctx) {
    LAYOUT_CALL() << LAYOUT_ITEM_INFO(system);

    SystemLayout::layout2(system, ctx); // Computes staff distances, essential for the rest of the calculations
    // Update grace cross beams
    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &seg : to_measure(mb)->segments()) {
            if (!seg.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *e : seg.elist()) {
                if (!e || !e->is_chord()) {
                    continue;
                }
                for (Chord *grace : to_chord(e)->grace_notes()) {
                    if (grace->beam() && (grace->beam()->cross() || grace->beam()->user_modified()) &&
                        grace->beam()->elements().front() == grace) {
                        BeamLayout::layout(grace->beam(), ctx);
                    }
                }
            }
        }
    }
    // Update normal chords cross beams and respective segments
    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        bool something_changed = false;
        for (Segment &seg : to_measure(mb)->segments()) {
            if (!seg.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *e : seg.elist()) {
                if (!e || !e->is_chord()) {
                    continue;
                }
                Chord *chord = to_chord(e);
                if (chord->beam() && (chord->beam()->cross() || chord->beam()->user_modified()) &&
                    chord->beam()->elements().front() == chord) {
                    bool prev_up = chord->up();
                    Stem *stem = chord->stem();
                    double prev_stem_length = stem ? stem->length() : 0.0;
                    BeamLayout::layout(chord->beam(), ctx);
                    if (chord->up() != prev_up || (stem && stem->length() != prev_stem_length)) {
                        // If the chord has changed direction needs to be re-laid out
                        ChordLayout::layout_chords1(ctx, &seg, chord->v_staff_idx());
                        something_changed = true;
                    }
                } else if (chord->tremolo_two_chord()) {
                    TremoloTwoChord *t = chord->tremolo_two_chord();
                    Chord *c1 = t->chord1();
                    Chord *c2 = t->chord2();
                    if (t->user_modified() || (c1->staff_move() != 0 || c2->staff_move() != 0)) {
                        bool prev_up = chord->up();
                        ChordLayout::compute_up(chord, ctx);
                        if (chord->up() != prev_up) {
                            ChordLayout::layout_chords1(ctx, &seg, chord->v_staff_idx());
                            something_changed = true;
                        }
                    }
                }
            }
            if (something_changed) {
                seg.create_shapes();
            }
        }
    }
}

void SystemLayout::restore_old_system_layout(System *system, LayoutContext &ctx) { layout_system_elements(system, ctx); }

void SystemLayout::layout_system(System *system, LayoutContext &ctx, double leading_h_boxes_width) {
    SystemHeaderLayout::update_system_header_width(system, ctx);

    size_t nstaves = system->staves().size();

    for (size_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        SysStaff *s = system->staves().at(staff_idx);
        const Staff *staff = ctx.dom().staff(staff_idx);
        if (!staff->show() || !s->show()) {
            s->setbbox(RectF());
            continue;
        }

        double staff_mag = staff->staff_mag(Fraction(0, 1)); // ??? TODO
        int staff_lines = staff->lines(Fraction(0, 1));
        if (staff_lines <= 1) {
            double h = staff->line_distance(Fraction(0, 1)) * staff_mag * system->spatium();
            s->setbbox(system->left_margin() + leading_h_boxes_width, -h, 0.0, 2 * h);
        } else {
            double h = (staff_lines - 1) * staff->line_distance(Fraction(0, 1));
            h = h * staff_mag * system->spatium();
            s->setbbox(system->left_margin() + leading_h_boxes_width, 0.0, 0.0, h);
        }
    }

    //---------------------------------------------------
    //  layout brackets
    //---------------------------------------------------

    SystemHeaderLayout::set_brackets_x_position(system, system->left_margin() + leading_h_boxes_width);

    SystemHeaderLayout::set_instrument_names_horizontal_pos(system);
    SystemHeaderLayout::set_group_brackets_horizontal_pos(system);

    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        Measure *m = to_measure(mb);
        if (m == system->measures().front() || (m->prev() && m->prev()->is_h_box())) {
            MeasureLayout::create_system_begin_bar_line(m, ctx);
        }
    }
}

//---------------------------------------------------------
//   layout2
//    called after measure layout
//    adjusts staff distance
//---------------------------------------------------------

void SystemLayout::layout2(System *system, LayoutContext &ctx) {
    LAYOUT_CALL() << LAYOUT_ITEM_INFO(system);

    Box *vb = system->vbox();
    if (vb) {
        BoxLayout::layout_box(vb, vb->mutldata(), ctx);
        system->setbbox(vb->ldata()->bbox());
        return;
    }

    system->set_pos(0.0, 0.0);
    std::vector<std::pair<size_t, SysStaff *>> visible_staves;

    for (size_t i = 0; i < system->staves().size(); ++i) {
        const Staff *s = ctx.dom().staff(i);
        SysStaff *ss = system->staves().at(i);
        if (s->show() && ss->show()) {
            visible_staves.push_back(std::pair<size_t, SysStaff *>(i, ss));
        } else {
            ss->setbbox(RectF()); // already done in layout() ?
        }
    }

    double spatium_value = system->spatium();
    double y = 0.0;
    double min_vertical_distance = ctx.conf().style_absolute(Sid::MIN_VERTICAL_DISTANCE);
    double staff_distance = ctx.conf().style_absolute(Sid::STAFF_DISTANCE);
    double akkolade_distance = ctx.conf().style_absolute(Sid::AKKOLADE_DISTANCE);
    if (ctx.conf().is_vertical_spread_enabled()) {
        staff_distance = ctx.conf().style_absolute(Sid::MIN_STAFF_SPREAD);
        akkolade_distance = ctx.conf().style_absolute(Sid::MIN_STAFF_SPREAD);
    }

    if (visible_staves.empty()) {
        return;
    }

    for (auto i = visible_staves.begin();; ++i) {
        SysStaff *ss = i->second;
        staff_idx_t si1 = i->first;
        const Staff *staff = ctx.dom().staff(si1);
        auto ni = std::next(i);

        double dist = staff->staff_height();
        double y_offset;
        double h;
        if (staff->lines(Fraction(0, 1)) == 1) {
            y_offset = spatium_value * ::mu::engraving::barline_span_1_linestaff_to * 0.5;
            h = spatium_value * (::mu::engraving::barline_span_1_linestaff_to - ::mu::engraving::barline_span_1_linestaff_from) * 0.5;
        } else {
            y_offset = 0.0;
            h = staff->staff_height();
        }
        if (ni == visible_staves.end()) {
            ss->set_y_off(y_offset);
            ss->setbbox(system->left_margin(), y - y_offset, system->width() - system->left_margin(), h);
            ss->save_layout();
            break;
        }

        staff_idx_t si2 = ni->first;
        const Staff *staff2 = ctx.dom().staff(si2);

        if (staff->part() == staff2->part()) {
            Measure *m = system->first_measure();
            double mag = m ? staff->staff_mag(m->tick()) : 1.0;
            dist += akkolade_distance * mag;
        } else {
            dist += staff_distance;
        }
        dist += staff2->absolute_from_spatium(staff2->user_dist());
        bool fixed_space = false;
        for (const MeasureBase *mb : system->measures()) {
            if (!mb->is_measure()) {
                continue;
            }
            const Measure *m = to_measure(mb);
            Spacer *sp = m->vspacer_down(si1);
            if (sp) {
                if (sp->spacer_type() == SpacerType::FIXED) {
                    dist = staff->staff_height(m->tick()) + sp->absolute_gap();
                    fixed_space = true;
                    break;
                } else {
                    dist = std::max(dist, staff->staff_height(m->tick()) + sp->absolute_gap());
                }
            }
            sp = m->vspacer_up(si2);
            if (sp) {
                dist = std::max(dist, staff->staff_height(m->tick()) + sp->absolute_gap());
            }
        }
        if (!fixed_space) {
            // check minimum distance to next staff
            // note that in continuous view, we normally only have a partial skyline for the system
            // a full one is only built when triggering a full layout
            // therefore, we don't know the value we get from minDistance will actually be enough
            // so we remember the value between layouts and increase it when necessary
            // (the first layout on switching to continuous view gives us good initial values)
            // the result is space is good to start and grows as needed
            // it does not, however, shrink when possible - only by trigger a full layout
            // (such as by toggling to page view and back)
            const double min_horizontal_clearance = ctx.conf().style_absolute(Sid::SKYLINE_MIN_HORIZONTAL_CLEARANCE);
            double d = ss->skyline().min_distance(system->System::staff(si2)->skyline(), min_horizontal_clearance);
            if (ctx.conf().is_line_mode()) {
                double previous_dist = ss->continuous_dist();
                if (d > previous_dist) {
                    ss->set_continuous_dist(d);
                } else {
                    d = previous_dist;
                }
            }
            dist = std::max(dist, d + min_vertical_distance);
            dist = std::max(dist, min_vert_space_for_cross_staff_beams(system, si1, si2, ctx));
        }
        ss->set_y_off(y_offset);
        ss->setbbox(system->left_margin(), y - y_offset, system->width() - system->left_margin(), h);
        ss->save_layout();
        y += dist;
    }

    system->set_system_height(system->staff(visible_staves.back().first)->bbox().bottom());
    system->set_height(system->system_height());

    SystemLayout::set_measure_height(system, system->system_height(), ctx);

    //---------------------------------------------------
    //  layout brackets vertical position
    //---------------------------------------------------

    SystemHeaderLayout::layout_brackets_vertical(system, ctx);

    //---------------------------------------------------
    //  layout instrument names
    //---------------------------------------------------

    SystemHeaderLayout::set_instrument_names_vertical_pos(system, ctx);
}

double SystemLayout::min_vert_space_for_cross_staff_beams(System *system, staff_idx_t staff_idx1, staff_idx_t staff_idx2,
                                                          LayoutContext &ctx) {
    double min_space = -DBL_MAX;
    track_idx_t start_track = staff_idx1 * ::mu::engraving::voices;
    track_idx_t end_track = staff_idx2 * ::mu::engraving::voices + ::mu::engraving::voices;
    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &segment : to_measure(mb)->segments()) {
            if (!segment.is_chord_rest_type()) {
                continue;
            }
            for (track_idx_t track = start_track; track < end_track; ++track) {
                EngravingItem *item = segment.element(track);
                if (!item || !item->is_chord()) {
                    continue;
                }
                Beam *beam = to_chord(item)->beam();
                if (!beam || !beam->autoplace() || beam->elements().front() != item) {
                    continue;
                }
                if (beam->ldata()->cross_staff_beam_pos != BeamBase::CrossStaffBeamPosition::BETWEEN) {
                    continue;
                }
                const Chord *limiting_chord_above = nullptr;
                const Chord *limiting_chord_below = nullptr;
                double limit_from_above = -DBL_MAX;
                double limit_from_below = DBL_MAX;
                for (const ChordRest *cr : beam->elements()) {
                    if (!cr->is_chord()) {
                        continue;
                    }
                    const Chord *chord = to_chord(cr);
                    double min_stem_length = BeamTremoloLayout::min_stem_length(chord, beam->ldata()) * (chord->spatium() / 4);
                    bool is_under_cross_beam = cr->is_below_cross_beam(beam);
                    if (is_under_cross_beam && chord->v_staff_idx() == staff_idx2) {
                        const Note *top_note = chord->up_note();
                        double note_limit = top_note->y() - min_stem_length;
                        if (note_limit < limit_from_below) {
                            limit_from_below = note_limit;
                            limiting_chord_below = chord;
                        }
                        limit_from_below = std::min(limit_from_below, note_limit);
                    } else if (!is_under_cross_beam && chord->v_staff_idx() == staff_idx1) {
                        const Note *bottom_note = chord->down_note();
                        double note_limit = bottom_note->y() + min_stem_length;
                        if (note_limit > limit_from_above) {
                            limit_from_above = note_limit;
                            limiting_chord_above = chord;
                        }
                    }
                }
                if (limiting_chord_above && limiting_chord_below) {
                    double min_space_required = limit_from_above - limit_from_below;
                    beam->mutldata()->limiting_chord_above = limiting_chord_above;
                    beam->mutldata()->limiting_chord_below = limiting_chord_below;

                    const BeamSegment *top_beam_segment_for_chord_above = beam->top_level_segment_for_element(limiting_chord_above);
                    const BeamSegment *top_beam_segment_for_chord_below = beam->top_level_segment_for_element(limiting_chord_below);
                    if (top_beam_segment_for_chord_above->above == top_beam_segment_for_chord_below->above) {
                        // In this case the two opposing stems overlap the beam height, so we must subtract it
                        int stroke_count = std::min(BeamTremoloLayout::stroke_count(beam->ldata(), limiting_chord_above),
                                                    BeamTremoloLayout::stroke_count(beam->ldata(), limiting_chord_below));
                        double beam_height = ctx.conf().style_absolute(Sid::BEAM_WIDTH) + beam->beam_dist() * (stroke_count - 1);
                        min_space_required -= beam_height;
                    }
                    min_space = std::max(min_space, min_space_required);
                }
            }
        }
    }

    return min_space;
}

void SystemLayout::restore_layout2(System *system, LayoutContext &ctx) {
    if (system->vbox()) {
        return;
    }

    for (SysStaff *s : system->staves()) {
        s->restore_layout();
    }

    system->set_height(system->system_height());
    SystemLayout::set_measure_height(system, system->system_height(), ctx);

    // Reused systems can move across pages during partial relayout.
    // Refresh geometry derived from SysStaff vertical positions.
    SystemHeaderLayout::layout_brackets_vertical(system, ctx);
    SystemHeaderLayout::set_instrument_names_vertical_pos(system, ctx);
}

void SystemLayout::set_measure_height(System *system, double height, const LayoutContext &ctx) {
    double spatium = system->spatium();
    for (MeasureBase *m : system->measures()) {
        MeasureBase::LayoutData *mldata = m->mutldata();
        if (m->is_measure()) {
            // note that the factor 2 * _spatium must be corrected for when exporting
            // system distance in MusicXML (issue #24733)
            mldata->set_bbox(0.0, -spatium, m->width(), height + 2.0 * spatium);
        } else if (m->is_h_box()) {
            mldata->set_bbox(m->absolute_from_spatium(to_h_box(m)->top_gap()), 0.0, m->width(), height);
            BoxLayout::layout_h_box2(to_h_box(m), ctx);
        } else if (m->is_t_box()) {
            BoxLayout::layout_t_box(to_t_box(m), to_t_box(m)->mutldata(), ctx);
        } else if (m->is_f_box()) {
            BoxLayout::layout_f_box(to_f_box(m), to_f_box(m)->mutldata(), ctx);
        } else {
            LOGD("unhandled measure type %s", m->type_name());
        }
    }
}

//---------------------------------------------------------
//   minDistance
//    Return the minimum distance between this system and s2
//    without any element collisions.
//
//    top - top system
//    bottom   - bottom system
//---------------------------------------------------------

double SystemLayout::min_distance(const System *top, const System *bottom, const LayoutContext &ctx) {

    const LayoutConfiguration &conf = ctx.conf();
    const DomAccessor &dom = ctx.dom();

    const VBox *top_v_box = static_cast<VBox *>(top->vbox());
    const VBox *bottom_v_box = static_cast<VBox *>(bottom->vbox());

    if (top_v_box && !bottom_v_box) {
        return std::max(top_v_box->absolute_from_spatium(top_v_box->bottom_gap()),
                        bottom->min_top() + top_v_box->absolute_from_spatium(top_v_box->padding_to_notation_below()));
    } else if (!top_v_box && bottom_v_box) {
        return std::max(bottom_v_box->absolute_from_spatium(bottom_v_box->top_gap()),
                        top->min_bottom() + bottom_v_box->absolute_from_spatium(bottom_v_box->padding_to_notation_above()));
    } else if (top_v_box && bottom_v_box) {
        const double top_to_bottom_gap = top_v_box->absolute_from_spatium(top_v_box->bottom_gap());
        const double bottom_to_top_gap = bottom_v_box->absolute_from_spatium(bottom_v_box->top_gap());
        if (top_to_bottom_gap >= 0 && bottom_to_top_gap >= 0) {
            double largest_gap = std::max(bottom_to_top_gap, top_to_bottom_gap);
            return largest_gap;
        } else {
            return top_to_bottom_gap + bottom_to_top_gap;
        }
    }

    if (top->staves().empty() || bottom->staves().empty()) {
        return 0.0;
    }

    double min_vertical_distance = conf.style_absolute(Sid::MIN_VERTICAL_DISTANCE);
    double dist =
        conf.is_vertical_spread_enabled() ? conf.style_absolute(Sid::MIN_SYSTEM_SPREAD) : conf.style_absolute(Sid::MIN_SYSTEM_DISTANCE);
    size_t first_staff = 0;
    size_t last_staff = 0;

    for (first_staff = 0; first_staff < top->staves().size() - 1; ++first_staff) {
        if (dom.staff(first_staff)->show() && bottom->staff(first_staff)->show()) {
            break;
        }
    }
    for (last_staff = top->staves().size() - 1; last_staff > 0; --last_staff) {
        if (dom.staff(last_staff)->show() && top->staff(last_staff)->show()) {
            break;
        }
    }

    const Staff *staff = dom.staff(first_staff);
    double user_dist = staff ? staff->absolute_from_spatium(staff->user_dist()) : 0.0;
    dist = std::max(dist, user_dist);
    top->set_fixed_down_distance(false);

    const SysStaff *sys_staff = top->staff(last_staff);
    const double min_horizontal_clearance = conf.style_absolute(Sid::SKYLINE_MIN_HORIZONTAL_CLEARANCE);
    double sld = sys_staff ? sys_staff->skyline().min_distance(bottom->staff(first_staff)->skyline(), min_horizontal_clearance) : 0;
    sld -= sys_staff ? sys_staff->bbox().height() - min_vertical_distance : 0;

    if (conf.is_float_mode()) {
        return std::max(dist, sld);
    }

    for (const MeasureBase *mb1 : top->measures()) {
        if (mb1->is_measure()) {
            const Measure *m = to_measure(mb1);
            const Spacer *sp = m->vspacer_down(last_staff);
            if (sp) {
                if (sp->spacer_type() == SpacerType::FIXED) {
                    dist = sp->absolute_gap();
                    top->set_fixed_down_distance(true);
                    break;
                } else {
                    dist = std::max(dist, sp->absolute_gap());
                }
            }
        }
    }
    if (!top->has_fixed_down_distance()) {
        for (const MeasureBase *mb2 : bottom->measures()) {
            if (mb2->is_measure()) {
                const Measure *m = to_measure(mb2);
                const Spacer *sp = m->vspacer_up(first_staff);
                if (sp) {
                    dist = std::max(dist, sp->absolute_gap());
                }
            }
        }

        dist = std::max(dist, sld);
    }
    return dist;
}

static bool item_is_placed_above(const EngravingItem *item) {
    if (item->is_articulation_family()) {
        return to_articulation(item)->up();
    }
    if (item->is_lyrics_line_segment()) {
        return to_lyrics_line_segment(item)->lyrics_place_above();
    }
    return item->place_above();
}

void SystemLayout::remove_element_from_skyline(EngravingItem *element, const System *system) {
    Skyline &skyline = system->staff(element->staff_idx())->skyline();
    SkylineLine &skyline_line = item_is_placed_above(element) ? skyline.north() : skyline.south();

    skyline_line.remove_if([element](ShapeElement &shape_el) {
        return shape_el.item() && (element == shape_el.item() || element == shape_el.item()->ownership_parent());
    });
}

void SystemLayout::update_skyline_for_element(EngravingItem *element, const System *system, double y_move) {
    Skyline &skyline = system->staff(element->staff_idx())->skyline();
    SkylineLine &skyline_line = item_is_placed_above(element) ? skyline.north() : skyline.south();
    for (ShapeElement &shape_el : skyline_line.elements()) {
        const EngravingItem *item_in_skyline = shape_el.item();
        if (item_in_skyline && item_in_skyline->is_text()) {
            const EngravingItem *owner = item_in_skyline->ownership_parent_item();
            if (owner && owner->is_s_line_segment()) {
                item_in_skyline = owner;
            }
        }
        if (item_in_skyline == element) {
            shape_el.translate(0.0, y_move);
        }
    }
}

void SystemLayout::center_elements_between_staves(const System *system) {
    std::vector<Gap> gaps;                    // populated by collectCenterableItems
    std::vector<MMRest *> mm_rests_to_center; // populated by collectCenterableItems

    collect_centerable_items(system, gaps, mm_rests_to_center);

    for (MMRest *mmrest : mm_rests_to_center) {
        center_mm_rest_between_staves(mmrest, system);
    }

    const double min_horizontal_clearance = system->style().style_absolute(Sid::SKYLINE_MIN_HORIZONTAL_CLEARANCE);

    for (const Gap &gap : gaps) {
        if (!gap.items.empty()) {
            center_items_in_gap(gap, system, min_horizontal_clearance);
        }
    }
}

void SystemLayout::center_pending_systems(LayoutContext &ctx) {
    for (System *system : ctx.state().systems_needing_centering()) {
        center_elements_between_staves(system);
    }

    ctx.mut_state().clear_systems_needing_centering();
}

static Shape shape_relative_to_staff(const EngravingItem *item, double system_x) {
    // Offsets are applied after centering, so measure from the non-offset position
    const double x = item->page_x() - system_x - item->offset().x();
    const double y = item->is_lyrics() ? to_lyrics(item)->y_relative_to_staff() - item->offset().y() : item->ldata()->pos().y();
    return item->ldata()->shape().translated(PointF(x, y));
}

void SystemLayout::collect_centerable_items(const System *system, std::vector<Gap> &gaps, std::vector<MMRest *> &mm_rests_to_center) {
    const size_t n_staves = system->staves().size();
    if (n_staves < 2) {
        return;
    }

    struct GapsOfStaff {
        size_t for_items_below = muse::nidx;
        size_t for_items_above = muse::nidx;
    };

    std::vector<GapsOfStaff> gaps_of_staff(n_staves);
    gaps.reserve(n_staves - 1);

    staff_idx_t upper_staff_idx = system->first_visible_staff();
    staff_idx_t lower_staff_idx = muse::nidx;
    for (; upper_staff_idx < n_staves; upper_staff_idx = lower_staff_idx) {
        lower_staff_idx = system->next_visible_staff(upper_staff_idx);
        if (lower_staff_idx == muse::nidx) {
            break; // the last visible staff of the system
        }

        gaps_of_staff[upper_staff_idx].for_items_below = gaps.size();
        gaps_of_staff[lower_staff_idx].for_items_above = gaps.size();
        gaps.emplace_back(upper_staff_idx, lower_staff_idx, system->staff(lower_staff_idx)->y() - system->staff(upper_staff_idx)->y());
    }

    const double system_x = system->page_x();

    auto add_item = [&](EngravingItem *item, bool above, bool is_lyrics) {
        const staff_idx_t staff_idx = item->staff_idx();
        if (staff_idx >= n_staves) {
            return;
        }
        const size_t gap_idx = above ? gaps_of_staff[staff_idx].for_items_above : gaps_of_staff[staff_idx].for_items_below;
        if (gap_idx == muse::nidx) {
            // The staff is hidden, or there is no staff on that side to center against
            return;
        }

        Shape shape = shape_relative_to_staff(item, system_x);
        shape.remove_if([](ShapeElement &shape_el) { return shape_el.ignore_for_layout(); });

        // Items left out of the skyline (e.g. invisible) must not take up space in the gap
        const bool takes_space =
            item->is_lyrics_line_segment() ? to_lyrics_line_segment(item)->lyrics_add_to_skyline() : item->add_to_skyline();
        if (!takes_space) {
            shape.clear();
        }

        Gap &gap = gaps[gap_idx];
        if (above) {
            // Inside a gap, everything is expressed relative to its upper staff
            shape.translate(PointF(0.0, gap.y_staff_diff));
        }
        gap.items.push_back(CenterableItem{item, std::move(shape), is_lyrics, !above});
    };

    for (SpannerSegment *spanner_seg : system->spanner_segments()) {
        if ((spanner_seg->is_hairpin_segment() && snap_chain_should_be_centered_between_staves(spanner_seg, system)) ||
            (spanner_seg->is_whammy_bar_segment() &&
             whammy_bar_should_be_centered_between_staves(to_whammy_bar_segment(spanner_seg), system))) {
            add_item(spanner_seg, spanner_seg->place_above(), false);
        } else if (spanner_seg->is_lyrics_line_segment()) {
            bool above = to_lyrics_line_segment(spanner_seg)->lyrics_place_above();
            if (element_should_be_centered_between_staves(spanner_seg, system, above)) {
                add_item(spanner_seg, above, true);
            }
        }
    }

    for (const MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (const Segment &seg : to_measure(mb)->segments()) {
            if (seg.is_chord_rest_type()) {
                for (EngravingItem *item : seg.elist()) {
                    if (!item) {
                        continue;
                    }
                    if (item->is_mm_rest() && mm_rest_should_be_centered_between_staves(to_mm_rest(item), system)) {
                        mm_rests_to_center.push_back(to_mm_rest(item));
                    } else if (item->is_chord_rest()) {
                        for (Lyrics *lyrics : to_chord_rest(item)->lyrics()) {
                            bool above = lyrics->place_above();
                            if (element_should_be_centered_between_staves(lyrics, system, above)) {
                                add_item(lyrics, above, true);
                            }
                        }
                    }
                }
            }
            for (EngravingItem *item : seg.annotations()) {
                if ((item->is_dynamic() || item->is_expression()) && snap_chain_should_be_centered_between_staves(item, system)) {
                    add_item(item, item->place_above(), false);
                }
            }
        }
    }
}

bool SystemLayout::shapes_stack_vertically(const Shape &shape1, const Shape &shape2, double min_horizontal_clearance) {
    for (const ShapeElement &shape_el1 : shape1.elements()) {
        if (shape_el1.height() <= 0.0) {
            continue;
        }
        for (const ShapeElement &shape_el2 : shape2.elements()) {
            if (shape_el2.height() <= 0.0) {
                continue;
            }
            if (intersects(shape_el1.left(), shape_el1.right(), shape_el2.left(), shape_el2.right(), min_horizontal_clearance)) {
                return true;
            }
        }
    }

    return false;
}

void SystemLayout::center_big_time_sigs_across_staves(const System *system) {
    staff_idx_t nstaves = system->score()->nstaves();
    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &segment : to_measure(mb)->segments()) {
            if (!segment.is_type(SegmentType::TIME_SIG_TYPES)) {
                continue;
            }
            for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
                TimeSig *time_sig = to_time_sig(segment.element(staff2track(staff_idx)));
                if (!time_sig || !time_sig->show_on_this_staff()) {
                    continue;
                }
                staff_idx_t this_staff_idx = time_sig->effective_staff_idx();
                if (this_staff_idx == muse::nidx) {
                    continue;
                }
                staff_idx_t next_staff_idx = this_staff_idx;
                for (staff_idx_t idx = this_staff_idx + 1; idx < nstaves; ++idx) {
                    TimeSig *next_time_sig = to_time_sig(segment.element(staff2track(idx)));
                    if (next_time_sig && next_time_sig->show_on_this_staff()) {
                        staff_idx_t next_time_sig_stave = next_time_sig->effective_staff_idx();
                        if (next_time_sig_stave != muse::nidx) {
                            next_staff_idx = system->prev_visible_staff(next_time_sig_stave);
                            break;
                        }
                    }
                    if (idx == nstaves - 1) {
                        next_staff_idx = system->prev_visible_staff(nstaves);
                        break;
                    }
                }
                double y_top = system->staff(this_staff_idx)->y() + system->score()->staff(this_staff_idx)->staff_height(segment.tick());
                double y_bottom = system->staff(next_staff_idx)->y() + system->score()->staff(next_staff_idx)->staff_height(segment.tick());
                double new_y_pos = 0.5 * (y_bottom - y_top);
                time_sig->mutldata()->set_pos_y(new_y_pos);
            }
        }
    }
}

bool SystemLayout::snap_chain_should_be_centered_between_staves(EngravingItem *item, const System *system) {
    // Snapped items are centered as one, so if any of them cannot be centered, none of them can
    bool all_centerable = true;
    item->scan_connected_items_in_snapping_chain(system, [&all_centerable, system](EngravingItem *chain_item) {
        all_centerable = all_centerable && element_should_be_centered_between_staves(chain_item, system, chain_item->place_above());
    });

    return all_centerable;
}

bool SystemLayout::element_should_be_centered_between_staves(const EngravingItem *item, const System *system, bool place_above) {
    const Part *item_part = item->part();
    IF_ASSERT_FAILED(item_part) { return false; }

    const Staff *other_staff = item->staff_to_center_against(place_above, system);
    if (!other_staff) {
        return false;
    }

    const bool same_instrument = other_staff->part() == item_part;

    Sid center_style_id;
    if (item->is_lyrics() || item->is_lyrics_line_segment()) {
        center_style_id = same_instrument ? Sid::LYRICS_AUTO_CENTER_ON_GRAND_STAFF : Sid::LYRICS_AUTO_CENTER_ON_VOCAL_STAVES;
    } else if (item->is_dynamic() || item->is_expression() || item->is_hairpin_segment()) {
        center_style_id =
            same_instrument ? Sid::DYNAMICS_HAIRPINS_AUTO_CENTER_ON_GRAND_STAFF : Sid::DYNAMICS_HAIRPINS_AUTO_CENTER_ON_VOCAL_STAVES;
    } else {
        return false;
    }

    AutoOnOff center_property;
    if (item->is_lyrics_line_segment() && !item->is_partial_lyrics_line_segment()) {
        // A dash or melisma line belonging to a lyric has no setting of its own: it always
        // follows that lyric. Only partial lyrics lines are configurable individually.
        const Lyrics *lyrics = to_lyrics_line_segment(item)->lyrics();
        center_property = lyrics ? lyrics->center_between_staves() : AutoOnOff::AUTO;
    } else {
        center_property = item->get_property(Pid::CENTER_BETWEEN_STAVES).value<AutoOnOff>();
    }

    const bool center_style = item->style().style_b(center_style_id);
    if (center_property == AutoOnOff::OFF || (!center_style && center_property != AutoOnOff::ON)) {
        return false;
    }

    if (same_instrument && center_property != AutoOnOff::ON && !item_part->instrument()->is_normally_multi_stave_instrument()) {
        return false;
    }

    if (item->is_dynamic() || item->is_expression() || item->is_hairpin_segment()) {
        if (!(center_property == AutoOnOff::ON || item->applies_to_all_voices_in_instrument())) {
            return false;
        }
    }

    return true;
}

bool SystemLayout::mm_rest_should_be_centered_between_staves(const MMRest *mm_rest, const System *system) {
    if (!mm_rest->style().style_b(Sid::MM_REST_BETWEEN_STAVES)) {
        return false;
    }

    const Part *item_part = mm_rest->part();
    if (item_part->nstaves() <= 1) {
        return false;
    }

    staff_idx_t this_staff_idx = mm_rest->staff_idx();
    staff_idx_t prev_staff_idx = system->prev_visible_staff(this_staff_idx);

    return prev_staff_idx != muse::nidx && mm_rest->score()->staff(prev_staff_idx)->part() == item_part;
}

bool SystemLayout::whammy_bar_should_be_centered_between_staves(const WhammyBarSegment *wbar, const System *system) {
    staff_idx_t staff_idx = wbar->staff_idx();
    Staff *this_staff = wbar->staff();
    Staff *next_staff = wbar->score()->staff(staff_idx + 1);
    bool next_is_linked_tab = next_staff && next_staff->is_tab_staff(wbar->tick()) && this_staff->is_linked(next_staff);
    SysStaff *next_sys_staff = system->staff(staff_idx + 1);
    return wbar->place_below() && next_is_linked_tab && next_sys_staff && next_sys_staff->show();
}

bool SystemLayout::element_has_another_stacked_outside(const EngravingItem *element, const Shape &element_shape,
                                                       const SkylineLine &skyline_line) {
    double elem_shape_left = element_shape.left();
    double elem_shape_right = element_shape.right();
    double elem_shape_top = element_shape.top();
    double elem_shape_bottom = element_shape.bottom();

    for (const ShapeElement &skyline_element : skyline_line.elements()) {
        const EngravingItem *skyline_item = skyline_element.item();
        if (!skyline_item || skyline_item == element || skyline_item->parent() == element || skyline_item->is_accidental() ||
            Autoplace::items_should_ignore_each_other(element, skyline_item)) {
            continue;
        }
        bool intersect_horizontally = elem_shape_right > skyline_element.left() && elem_shape_left < skyline_element.right();
        if (!intersect_horizontally) {
            continue;
        }
        bool skyline_element_is_stacked_on_it =
            skyline_line.is_north() ? skyline_element.bottom() <= elem_shape_top : skyline_element.top() >= elem_shape_bottom;
        if (skyline_element_is_stacked_on_it) {
            return true;
        }
    }

    return false;
}

static SkylineLine skyline_facing_gap(const System *system, staff_idx_t staff_idx, bool is_upper_staff_of_gap, double y_shift,
                                      const std::unordered_set<const EngravingItem *> &items_to_ignore) {
    const SkylineLine &skyline =
        is_upper_staff_of_gap ? system->staff(staff_idx)->skyline().south() : system->staff(staff_idx)->skyline().north();

    SkylineLine filtered_skyline = skyline.get_filtered_copy([&items_to_ignore](const ShapeElement &shape_el) {
        const EngravingItem *shape_item = shape_el.item();
        return shape_item && (muse::contains(items_to_ignore, shape_item) ||
                              muse::contains(items_to_ignore, const_cast<const EngravingItem *>(shape_item->ownership_parent_item())));
    });

    if (y_shift != 0.0) {
        filtered_skyline.translate_y(y_shift);
    }

    return filtered_skyline;
}

void SystemLayout::center_items_in_gap(const Gap &gap, const System *system, double min_horizontal_clearance) {
    /* Nothing which is itself centered in this gap may take part in measuring it */
    std::unordered_set<const EngravingItem *> movable_items;
    for (const CenterableItem &gap_item : gap.items) {
        movable_items.insert(gap_item.item);
    }

    auto get_verse = [](const EngravingItem *item) {
        if (item->is_lyrics()) {
            return to_lyrics(item)->verse();
        } else if (item->is_lyrics_line_segment()) {
            return to_lyrics_line_segment(item)->verse();
        }
        return -1;
    };

    /* An item with something non-centered stacked outside it has no gap to be centered in: it
     * stays where it is and joins the skyline, so that it constrains the items which do move */
    while (!movable_items.empty()) {
        const SkylineLine upper_skyline = skyline_facing_gap(system, gap.upper_staff_idx, true, 0.0, movable_items);
        const SkylineLine lower_skyline = skyline_facing_gap(system, gap.lower_staff_idx, false, gap.y_staff_diff, movable_items);

        std::vector<const CenterableItem *> items_to_center;
        bool can_center_group = true;
        for (const CenterableItem &gap_item : gap.items) {
            const EngravingItem *item = gap_item.item;
            if (!muse::contains(movable_items, item)) {
                continue;
            }

            const Shape item_shape = gap_item.shape.adjusted(-min_horizontal_clearance, 0.0, min_horizontal_clearance, 0.0);
            const SkylineLine &own_skyline = gap_item.on_upper_gap_staff ? upper_skyline : lower_skyline;
            if (!item_shape.empty() && element_has_another_stacked_outside(item, item_shape, own_skyline)) {
                movable_items.erase(item);
                can_center_group = false;

                // Snapped items stay together:
                gap_item.item->scan_connected_items_in_snapping_chain(
                    system, [&movable_items](EngravingItem *chain_item) { movable_items.erase(chain_item); });

                // A lyrics verse stays together:
                if (gap_item.is_lyrics) {
                    int verse = get_verse(item);
                    for (const CenterableItem &i : gap.items) {
                        if (i.is_lyrics && i.on_upper_gap_staff == gap_item.on_upper_gap_staff && get_verse(i.item) == verse) {
                            movable_items.erase(i.item);
                        }
                    }
                }
            } else {
                items_to_center.push_back(&gap_item);
            }
        }

        if (!can_center_group) {
            continue;
        }

        for (const std::vector<const CenterableItem *> &group : group_items_to_center_together(items_to_center, min_horizontal_clearance)) {
            center_item_group(group, system, upper_skyline, lower_skyline, gap.y_staff_diff, min_horizontal_clearance);
        }
        return;
    }
}

std::vector<std::vector<const SystemLayout::CenterableItem *>>
SystemLayout::group_items_to_center_together(const std::vector<const CenterableItem *> &items, double min_horizontal_clearance) {
    /* We center lyrics as a single block so that the verses will not lose their alignment (separating the ones
     * below the upper staff from the ones above the lower staff if they don't overlap). Other centerable items
     * join the group if they overlap horizontally, or if they are snapped to each other, so that they will
     * preserve their relative positions. */
    const size_t n_items = items.size();
    std::vector<size_t> group_ids(n_items, muse::nidx);
    size_t n_group_ids = 0;

    for (size_t i = 0; i < n_items; ++i) {
        if (group_ids[i] == muse::nidx) {
            group_ids[i] = n_group_ids++;
        }
        for (size_t j = i + 1; j < n_items; ++j) {
            const bool are_verses_of_the_same_block =
                items[i]->is_lyrics && items[j]->is_lyrics && items[i]->on_upper_gap_staff == items[j]->on_upper_gap_staff;
            const bool are_snapped_together = items[i]->item->ldata()->item_snapped_before() == items[j]->item ||
                                              items[i]->item->ldata()->item_snapped_after() == items[j]->item;
            const bool must_move_together = are_verses_of_the_same_block || are_snapped_together ||
                                            shapes_stack_vertically(items[i]->shape, items[j]->shape, min_horizontal_clearance);
            if (!must_move_together) {
                continue;
            }
            if (group_ids[j] == muse::nidx) {
                group_ids[j] = group_ids[i];
            } else if (group_ids[j] != group_ids[i]) {
                const size_t id_to_merge = group_ids[j];
                const size_t id_to_keep = group_ids[i];
                for (size_t &group_id : group_ids) {
                    if (group_id == id_to_merge) {
                        group_id = id_to_keep;
                    }
                }
            }
        }
    }

    std::vector<size_t> group_id_to_index(n_group_ids, muse::nidx);
    std::vector<std::vector<const CenterableItem *>> groups;

    for (size_t i = 0; i < n_items; ++i) {
        size_t &index = group_id_to_index[group_ids[i]];
        if (index == muse::nidx) {
            index = groups.size();
            groups.emplace_back();
        }
        groups[index].push_back(items[i]);
    }

    return groups;
}

// How far an item moves when its group is converged: only the half sitting above the lower staff moves to meet the upper one
double SystemLayout::convergence_move_for(const CenterableItem *centerable_item, double converge_distance) {
    return centerable_item->on_upper_gap_staff ? 0.0 : -converge_distance;
}

double SystemLayout::gap_converge_distance(const std::vector<const CenterableItem *> &group, double y_staff_diff,
                                           double min_horizontal_clearance) {
    /* A group that has items below the upper staff and above the lower one, spreads over the whole gap and
     * has no space left to center the items, so we bring the items together into the middle of the gap. This
     * function calculates how much slack there is for the lower items to converge towards the upper ones
     * before centering, so that they will be centered together. */

    double slack = DBL_MAX;
    for (const CenterableItem *upper : group) {
        if (!upper->on_upper_gap_staff || upper->shape.empty()) {
            continue;
        }

        for (const CenterableItem *lower : group) {
            if (lower->on_upper_gap_staff || lower->shape.empty()) {
                continue;
            }
            if (!shapes_stack_vertically(upper->shape, lower->shape, min_horizontal_clearance)) {
                continue;
            }

            const EngravingItem *upper_item = upper->item;
            const EngravingItem *lower_item = lower->item;
            const Lyrics *upper_lyrics = upper_item->is_lyrics() ? to_lyrics(upper_item) : nullptr;
            const Lyrics *lower_lyrics = lower_item->is_lyrics() ? to_lyrics(lower_item) : nullptr;

            if (upper_lyrics && lower_lyrics) {
                // The distance between the top-staff and bottom-staff lyrics blocks will be like between verses:
                const double verse_space = std::max(upper_lyrics->line_spacing(), lower_lyrics->line_spacing()) *
                                           upper_item->style().style_d(Sid::LYRICS_LINE_HEIGHT);
                const double space_between_lyrics = (lower_lyrics->y_relative_to_staff() - lower_lyrics->offset().y()) + y_staff_diff -
                                                    (upper_lyrics->y_relative_to_staff() - upper_lyrics->offset().y());
                slack = std::min(slack, space_between_lyrics - verse_space);
                continue;
            }

            // If it's not lyrics, we just use the items' min distance
            const double min_dist = std::max(upper_item->absolute_from_spatium(upper_item->min_distance()),
                                             lower_item->absolute_from_spatium(lower_item->min_distance()));
            slack = std::min(slack, upper->shape.vertical_clearance(lower->shape, min_horizontal_clearance) - min_dist);
        }
    }

    // Nothing to do for a group that is on one staff only, or whose halves are already as close as possible
    return slack == DBL_MAX ? 0.0 : std::max(slack, 0.0);
}

void SystemLayout::center_item_group(const std::vector<const CenterableItem *> &group, const System *system,
                                     const SkylineLine &upper_skyline, const SkylineLine &lower_skyline, double y_staff_diff,
                                     double min_horizontal_clearance) {
    // The least space each half has on either side
    double upper_half_space_above = DBL_MAX;
    double upper_half_space_below = DBL_MAX;
    double lower_half_space_above = DBL_MAX;
    double lower_half_space_below = DBL_MAX;

    for (const CenterableItem *centerable_item : group) {
        const Shape item_shape = centerable_item->shape.adjusted(-min_horizontal_clearance, 0.0, min_horizontal_clearance, 0.0);
        if (item_shape.empty()) {
            continue;
        }

        const bool on_upper_staff = centerable_item->on_upper_gap_staff;
        const SkylineLine own_skyline =
            (on_upper_staff ? upper_skyline : lower_skyline).get_filtered_copy([centerable_item](const ShapeElement &shape_el) {
                const EngravingItem *shape_item = shape_el.item();
                return shape_item &&
                       (shape_item->is_accidental() || Autoplace::items_should_ignore_each_other(centerable_item->item, shape_item));
            });

        const double min_dist = centerable_item->item->absolute_from_spatium(centerable_item->item->min_distance());
        const double space_above = (on_upper_staff ? own_skyline : upper_skyline).vertical_clearance_below(item_shape) - min_dist;
        const double space_below = (on_upper_staff ? lower_skyline : own_skyline).vertical_clearance_above(item_shape) - min_dist;

        if (on_upper_staff) {
            upper_half_space_above = std::min(upper_half_space_above, space_above);
            upper_half_space_below = std::min(upper_half_space_below, space_below);
        } else {
            lower_half_space_above = std::min(lower_half_space_above, space_above);
            lower_half_space_below = std::min(lower_half_space_below, space_below);
        }
    }

    double converge_distance = gap_converge_distance(group, y_staff_diff, min_horizontal_clearance);
    if (lower_half_space_above != DBL_MAX && upper_half_space_below != DBL_MAX) {
        /* Convergence only measures the items of the two halves, but there might be other non-centered
         * items in the gap, so we cap it at the space the group actually has in the gap */
        converge_distance = std::min(converge_distance, std::max(lower_half_space_above + upper_half_space_below, 0.0));
    }

    // Space left above and below the entire group once converged, i.e. how far it can move as one rigid unit
    const double group_space_above = std::min(upper_half_space_above, lower_half_space_above - converge_distance);
    const double group_space_below = std::min(upper_half_space_below, lower_half_space_below + converge_distance);

    if (group_space_above == DBL_MAX || group_space_below == DBL_MAX) {
        return;
    }

    const double y_move = 0.5 * (group_space_below - group_space_above);

    // Move the items
    for (const CenterableItem *centerable_item : group) {
        const double item_move = y_move + convergence_move_for(centerable_item, converge_distance);
        centerable_item->item->mutldata()->move_y(item_move);
        update_skyline_for_element(centerable_item->item, system, item_move);
    }
}

void SystemLayout::center_mm_rest_between_staves(MMRest *mm_rest, const System *system) {
    staff_idx_t this_idx = mm_rest->staff_idx();
    IF_ASSERT_FAILED(this_idx > 0) { return; }

    staff_idx_t prev_idx = system->prev_visible_staff(this_idx);
    IF_ASSERT_FAILED(prev_idx != muse::nidx) { return; }

    SysStaff *this_staff = system->staff(this_idx);
    SysStaff *prev_staff = system->staff(prev_idx);
    double prev_staff_height = system->score()->staff(prev_idx)->staff_height(mm_rest->tick());
    double y_staff_diff = prev_staff->y() + prev_staff_height - this_staff->y();

    PointF mm_rest_default_number_position = mm_rest->number_pos() - PointF(0.0, mm_rest->absolute_from_spatium(mm_rest->number_offset()));
    RectF number_bbox = mm_rest->number_rect().translated(mm_rest_default_number_position + mm_rest->pos());
    double y_base_line = 0.5 * (y_staff_diff - number_bbox.height());
    double y_diff = y_base_line - number_bbox.top();

    mm_rest->mutldata()->y_number_pos += y_diff;
}

void SystemLayout::MeasureState::clear() {
    measure = nullptr;
    measure_width = 0.0;
    measure_pos = 0.0;
    element_positions.clear();
    element_widths.clear();
    shared_track_maps.clear();
}

void SystemLayout::MeasureState::restore_measure(LayoutContext &ctx) {
    bool map_changed = false;
    Fraction tick = measure->tick();
    for (auto &[shared_part, shared_track_map] : shared_track_maps) {
        if (shared_part->track_map_at_tick(tick) != shared_track_map) {
            shared_part->set_track_map_at_tick(shared_track_map, tick);
            map_changed = true;
        }
    }
    if (map_changed) {
        StaveSharingLayout::update_notation_without_recomputing_track_map(measure, ctx);
    }

    measure->mutldata()->set_pos_x(measure_pos);
    measure->set_width(measure_width);
    for (auto pair : element_positions) {
        pair.first->set_pos(pair.second);
    }
    for (auto pair : element_widths) {
        pair.first->set_width(pair.second);
    }
}
