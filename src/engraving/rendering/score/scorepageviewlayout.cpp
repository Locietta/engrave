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
#include "scorepageviewlayout.h"

#include "containers.h"

#include "dom/score.h"
#include "dom/system.h"
#include "dom/bracket.h"
#include "dom/layoutbreak.h"
#include "dom/page.h"

#include "passresetlayoutdata.h"
#include "passlayoutindependentitems.h"

#include "headerfooterlayout.h"
#include "measurelayout.h"
#include "systemlayout.h"
#include "pagelayout.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void ScorePageViewLayout::init_layout_context(const Score *score, LayoutContext &ctx, const Fraction &stick, const Fraction &etick) {
    LAYOUT_CALL();

    LayoutState &state = ctx.mut_state();

    state.set_end_tick(etick);

    MeasureBase *m = nullptr;
    {
        m = score->tick2measure(stick);
        if (m == nullptr) {
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
    }

    if (!state.is_layout_all() && m->system()) {
        System *system = m->system();
        system_idx_t system_index = muse::index_of(score->systems(), system);

        // set current system
        state.set_start_tick(system->measures().front()->tick());
        state.set_cur_system(system);
        state.set_system_list(muse::mid(score->systems(), system_index));

        // set current page
        state.set_page(system->page());
        page_idx_t page_idx = score->page_idx(state.page());
        if (page_idx == muse::nidx) {
            page_idx = 0;
        }
        state.set_page_idx(page_idx);

        MeasureBase *next_measure = nullptr;
        if (system_index == 0) {
            next_measure = ctx.conf().is_show_v_box() ? score->first() : score->first_measure();
        } else {
            System *prev_system = score->systems().at(system_index - 1);
            next_measure = prev_system->measures().back()->next();
        }
        state.set_next_measure(next_measure);

        if (!state.next_measure()->prev_measure()) {
            state.set_measure_number(0);
            state.set_tick(Fraction(0, 1));
        } else {
            const MeasureBase *mb = state.next_measure()->prev();
            if (mb) {
                mb = mb->mb_with_preceding_section_break();
            }

            const LayoutBreak *layout_break = mb->section_break_element();
            // TODO: also use mb in else clause here?
            // probably not, only actual measures have meaningful numbers
            if (layout_break && layout_break->start_with_measure_one()) {
                state.set_measure_number(0);
            } else {
                state.set_measure_number(
                    state.next_measure()->prev_measure()->measure_number()                       // will be adjusted later with respect
                    + (state.next_measure()->prev_measure()->exclude_from_numbering() ? 0 : 1)); // to the user-defined offset.
            }
            state.set_tick(ctx.state().next_measure()->tick());
        }
    } else {
        state.set_next_measure(ctx.conf().is_show_v_box() ? score->first() : score->first_measure());
    }

    state.set_prev_measure(nullptr);
}

void ScorePageViewLayout::prepare_score(Score *score, const LayoutContext &ctx) {
    LAYOUT_CALL();

    if (!ctx.state().is_layout_all() && ctx.state().cur_system()) {
        system_idx_t system_index = muse::index_of(score->systems(), ctx.state().cur_system());
        score->systems().erase(score->systems().begin() + system_index, score->systems().end());
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
    }
}

void ScorePageViewLayout::layout_page_view(Score *score, LayoutContext &ctx, const Fraction &stick, const Fraction &etick) {
    TRACEFUNC;

    // initLayoutContext moves the tail of Score::systems() into the layout's list of
    // reusable systems; the layout then re-collects them, deletes them or hands them
    // back at layoutFinished. If something throws in between (a bad_alloc while the
    // layout pushes a command, for example) they stay parented to the score without
    // being in Score::systems(), still pointing at their page, and outlive it. Give
    // them back, so that the next layout collects, reuses or deletes them again.
    struct ReturnsReusableSystems {
        Score *score;
        std::vector<System *> &reusable;

        ~ReturnsReusableSystems() {
            if (reusable.empty()) {
                return;
            }
            // This runs while unwinding, where a throw would terminate the process:
            // if growing the score's list cannot allocate, leave them leaked instead.
            try {
                score->systems().insert(score->systems().end(), reusable.begin(), reusable.end());
                reusable.clear();
            } catch (...) {}
        }
    } returns_reusable_systems{score, ctx.mut_state().system_list()};

    LAYOUT_CALL_CLEAR();
    LAYOUT_CALL();

    init_layout_context(score, ctx, stick, etick);

    prepare_score(score, ctx);

    //! NOTE Reset pass need anyway
    //#ifdef MUE_ENABLE_ENGRAVING_LD_PASSES
    PassResetLayoutData reset_pass;
    reset_pass.run(score, ctx);
    //#endif

#ifdef MUE_ENABLE_ENGRAVING_LD_PASSES
    if (ctx.state().is_layout_all()) {
        PassLayoutIndependentItems independent_pass;
        independent_pass.run(score, ctx);
    }
#endif

    do_layout(ctx);

    layout_finished(score, ctx);

    LAYOUT_CALL_PRINT();
}

void ScorePageViewLayout::do_layout(LayoutContext &ctx) {
    LAYOUT_CALL();

    LayoutState &state = ctx.mut_state();
    MeasureLayout::get_next_measure(ctx);
    state.set_cur_system(SystemLayout::collect_system(ctx));

    const MeasureBase *lmb = nullptr;
    do {
        PageLayout::get_next_page(ctx);
        PageLayout::collect_page(ctx);

        if (state.page() && !state.page()->systems().empty()) {
            lmb = state.page()->systems().back()->measures().back();
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
    } while (state.cur_system() && !(state.range_done() && lmb == state.page_old_measure()));
    // && page->system(0)->measures().back()->tick() > endTick // FIXME: perhaps the first measure was meant? Or last system?

    SystemLayout::center_pending_systems(ctx);
}

void ScorePageViewLayout::layout_finished(Score *score, LayoutContext &ctx) {
    LAYOUT_CALL();

    LayoutState &state = ctx.mut_state();

    if (!state.cur_system()) {
        // The end of the score. The remaining systems are not needed...
        muse::delete_all(state.system_list());
        state.system_list().clear();

        // ...and the remaining pages too
        while (score->npages() > state.page_idx()) {
            Page *p = score->pages().back();
            score->pages().pop_back();
            delete p;
        }
    } else {
        Page *p = state.cur_system()->page();
        if (p && (p != state.page())) {
            p->invalidate_bsp_tree();
        }
    }

    score->systems().insert(score->systems().end(), state.system_list().begin(), state.system_list().end());
    // The reusable systems now belong to the score again; anything left in the list
    // would make layoutPageView's guard insert them a second time.
    state.system_list().clear();

    /* If the headers/footers involve page count, we need to re-calculate them. This needs to
     * be done after laying out all pages, so that the total number of pages will be known: */
    if (state.must_recompute_headers_footers()) {
        do_layout_headers_footers(ctx);
        state.set_must_recompute_headers_footers(false);
    }
}

void ScorePageViewLayout::layout_headers_footers(LayoutContext &ctx) {
    TRACEFUNC;

    LAYOUT_CALL_CLEAR();
    LAYOUT_CALL();

    do_layout_headers_footers(ctx);

    LAYOUT_CALL_PRINT();
}

void ScorePageViewLayout::do_layout_headers_footers(LayoutContext &ctx) {
    for (const auto &page : ctx.dom().pages()) {
        HeaderFooterLayout::layout_header_footer(ctx, page);
    }
}
