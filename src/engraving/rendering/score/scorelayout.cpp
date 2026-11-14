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
#include "scorelayout.h"

#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/page.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/spanner.h"
#include "dom/system.h"
#include "dumplayoutdata.h"
#include "editing/transaction/transaction.h"
#include "layoutcontext.h"
#include "layoutdatastore.h"
#include "log.h"
#include "pagelayout.h"
#include "scorepageviewlayout.h"
#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

struct CmdStateLocker {
private:
    Score *score_ = nullptr;
public:
    CmdStateLocker(Score *s) : score_(s) { score_->cmd_state().lock(); }

    ~CmdStateLocker() {
        score_->cmd_state().unlock();
        score_->transaction_manager()->cleanup_detached();
    }
};

void ScoreLayout::layout_range(Score *score, const Fraction &st, const Fraction &et) {

    CmdStateLocker cmd_state_locker(score);
    LayoutContext ctx(score);

    Fraction stick(st);
    Fraction etick(et);
    assert(!(stick == Fraction(-1, 1) && etick == Fraction(-1, 1)));

    // Check empty score
    if (!score->last()) {
        LOGD() << "empty score";
        ctx.mut_layout_data().discard_score_output();
        PageLayout::get_next_page(ctx);
        for (Page *page : score->pages()) page->rebuild_bsp_tree();
        return;
    }

    // Check range
    bool is_layout_all = stick <= Fraction(0, 1) && (etick < Fraction(0, 1) || etick == Fraction::max());
    if (stick < Fraction(0, 1)) {
        stick = Fraction(0, 1);
    }
    if (etick < Fraction(0, 1) || etick == Fraction::max()) {
        etick = score->last()->end_tick();
    }

    // A source insertion can supersede output still borrowed by a reused
    // system. Retire those views before deleting nodes, and rebuild that system.
    const auto retire_collisions = [&](Measure *measure) {
        bool collision = measure->ldata()->segment_output_invalid ||
                         (measure->ldata()->output_ticks >= Fraction(0, 1) && measure->ldata()->output_ticks != measure->ticks());
        for (auto *output : measure->ldata()->segments) {
            collision |=
                measure->find_segment_r(output->segment_type(), output->rtick()) != nullptr || output->elist().size() != score->ntracks();
        }
        for (auto *segment = measure->rendered_first(); segment; segment = segment->rendered_next()) {
            if (segment->ldata()->suppressed || !segment->ldata()->display_copies.empty()) {
                measure->mutldata()->segment_output_invalid = true;
                collision = true;
            }
        }
        for (auto *source = measure->first(); source; source = source->next()) {
            const auto &generated = source->ldata()->generated;
            collision |= !generated.empty() && generated.size() != score->ntracks();
        }
        if (!collision) return;
        for (auto *spanner : score->spanner_list()) {
            auto *start = spanner->start_element();
            auto *end = spanner->end_element();
            if (start && !source_object(*start) && start->find_measure() == measure) spanner->set_start_element(nullptr);
            if (end && !source_object(*end) && end->find_measure() == measure) spanner->set_end_element(nullptr);
        }
        if (auto *system = measure->system()) {
            if (auto *page = system->page()) page->invalidate_bsp_tree();
            for (auto *staff : system->staves()) staff->skyline().clear();
            for (auto *base : system->measures()) {
                if (!base->is_measure()) continue;
                auto *m = to_measure(base);
                for (auto *source = m->first(); source; source = source->next()) source->clear_shapes();
                for (auto *output : m->ldata()->segments) output->clear_shapes();
            }
            if (auto *first = system->first_measure()) stick = std::min(stick, first->tick());
            if (auto *last = system->last_measure()) etick = std::max(etick, last->end_tick());
        }
    };
    for (auto *m = score->first_measure(); m; m = m->next_measure()) {
        retire_collisions(m);
        if (auto *mmr = m->mm_rest()) retire_collisions(mmr);
    }
    ctx.mut_layout_data().retire_shared_auxiliary_output();
    for (auto *m = score->first_measure(); m; m = m->next_measure()) {
        m->reconcile_output_segments();
        if (auto *mmr = m->mm_rest()) mmr->reconcile_output_segments();
    }
    if (!is_layout_all) {
        if (auto *m = score->tick2measure(stick); m && m->prev_measure()) stick = m->prev_measure()->tick();
        if (auto *m = score->tick2measure(etick); m && m->next_measure()) etick = m->next_measure()->end_tick();
    }
    ctx.mut_state().set_is_layout_all(is_layout_all);

    // Init context and layout
    ScorePageViewLayout::layout_page_view(score, ctx, stick, etick);
    ctx.mut_layout_data().for_each_output(
        [&](EngravingItem &item, EngravingItem::LayoutData &data) { TLayout::complete_item_shape(item, data, ctx.conf()); });

    for (Page *page : score->pages()) page->rebuild_bsp_tree();
}
