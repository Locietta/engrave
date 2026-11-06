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
#include "scoreverticalviewlayout.h"

#include "containers.h"
#include "dom/bracket.h"
#include "dom/layoutbreak.h"
#include "dom/page.h"
#include "dom/score.h"
#include "dom/system.h"
#include "log.h"
#include "measurelayout.h"
#include "pagelayout.h"
#include "passlayoutindependentitems.h"
#include "passresetlayoutdata.h"
#include "systemlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void ScoreVerticalViewLayout::layout_vertical_view(Score *score, LayoutContext &ctx, const Fraction &stick, const Fraction &etick) {
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

    if (!ctx.state().is_layout_all() && m->system()) {
        System *system = m->system();
        system_idx_t system_index = muse::index_of(score->systems(), system);
        ctx.mut_state().set_page(system->page());
        ctx.mut_state().set_page_idx(score->page_idx(ctx.state().page()));
        if (ctx.state().page_idx() == muse::nidx) {
            ctx.mut_state().set_page_idx(0);
        }
        ctx.mut_state().set_cur_system(system);
        ctx.mut_state().set_system_list(muse::mid(score->systems(), system_index));

        if (system_index == 0) {
            ctx.mut_state().set_next_measure(ctx.conf().is_show_v_box() ? score->first() : score->first_measure());
        } else {
            System *prev_system = score->systems()[system_index - 1];
            ctx.mut_state().set_next_measure(prev_system->measures().back()->next());
        }

        score->systems().erase(score->systems().begin() + system_index, score->systems().end());
        if (!ctx.state().next_measure()->prev_measure()) {
            ctx.mut_state().set_measure_number(0);
            ctx.mut_state().set_tick(Fraction(0, 1));
        } else {
            const MeasureBase *mb = ctx.state().next_measure()->prev();
            if (mb) {
                mb = mb->mb_with_preceding_section_break();
            }

            const LayoutBreak *layout_break = mb->section_break_element();
            // TODO: also use mb in else clause here?
            // probably not, only actual measures have meaningful numbers
            if (layout_break && layout_break->start_with_measure_one()) {
                ctx.mut_state().set_measure_number(0);
            } else {
                ctx.mut_state().set_measure_number(
                    ctx.state().next_measure()->prev_measure()->measure_number() // will be adjusted later with respect
                    + (ctx.state().next_measure()->prev_measure()->exclude_from_numbering() ? 0 : 1)); // to the user-defined offset.
            }
            ctx.mut_state().set_tick(ctx.state().next_measure()->tick());
        }
    } else {
        for (System *s : score->systems()) {
            for (Bracket *b : s->brackets()) {
                if (b->selected()) {
                    bool selected = b->selected();
                    score->selection().remove(b);
                    BracketItem *item = b->bracket_item();
                    item->set_selected(selected);
                    score->set_selection_changed(true);
                }
            }
            // no need to clear the page placement: the systems are deleted below,
            // and ~System unlinks itself from its page
        }
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            mb->set_system(nullptr);
            if (mb->is_measure() && to_measure(mb)->mm_rest()) {
                to_measure(mb)->mm_rest()->set_system(nullptr);
            }
        }
        muse::delete_all(score->systems());
        score->systems().clear();

        muse::delete_all(score->pages());
        score->pages().clear();

        ctx.mut_state().set_next_measure(ctx.conf().is_show_v_box() ? score->first() : score->first_measure());
    }

    ctx.mut_state().set_prev_measure(nullptr);

    PassResetLayoutData reset_pass;
    reset_pass.run(score, ctx);

    MeasureLayout::get_next_measure(ctx);
    ctx.mut_state().set_cur_system(SystemLayout::collect_system(ctx));

    if (ctx.state().is_layout_all()) {
        PassLayoutIndependentItems independent_pass;
        independent_pass.run(score, ctx);
    }

    do_layout(ctx);
}

void ScoreVerticalViewLayout::do_layout(LayoutContext &ctx) {
    const MeasureBase *lmb = nullptr;
    do {
        PageLayout::get_next_page(ctx);
        PageLayout::collect_page(ctx);

        if (ctx.state().page() && !ctx.state().page()->systems().empty()) {
            lmb = ctx.state().page()->systems().back()->measures().back();
        } else {
            lmb = nullptr;
        }

        // we can stop collecting pages when:
        // 1) we reach the end of score (curSystem is nullptr)
        // or
        // 2) we have fully processed the range and reached a point of stability:
        //    a) we have completed layout for the range (rangeDone is true)
        //    b) we haven't collected a system that will need to go on the next page
        //    c) this page ends with the same measure as the previous layout
        //    pageOldMeasure will be last measure from previous layout if range was completed on or before this page
        //    it will be nullptr if this page was never laid out or if we collected a system for next page
    } while (ctx.state().cur_system() && !(ctx.state().range_done() && lmb == ctx.state().page_old_measure()));
    // && page->system(0)->measures().back()->tick() > endTick // FIXME: perhaps the first measure was meant? Or last system?

    SystemLayout::center_pending_systems(ctx);

    if (!ctx.state().cur_system()) {
        // The end of the score. The remaining systems are not needed...
        muse::delete_all(ctx.mut_state().system_list());
        ctx.mut_state().system_list().clear();
        // ...and the remaining pages too
        while (ctx.dom().npages() > ctx.state().page_idx()) {
            Page *p = ctx.mut_dom().pages().back();
            ctx.mut_dom().pages().pop_back();
            delete p;
        }
    } else {
        Page *p = ctx.mut_state().cur_system()->page();
        if (p && (p != ctx.state().page())) {
            p->invalidate_bsp_tree();
        }
    }
    ctx.mut_dom().systems().insert(ctx.mut_dom().systems().end(), ctx.state().system_list().begin(), ctx.state().system_list().end());
}
