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
#include "log.h"
#include "scorelayout.h"

#include "dom/score.h"
#include "dom/masterscore.h"
#include "dom/system.h"
#include "dom/page.h"

#include "layoutcontext.h"

#include "pagelayout.h"
#include "scorepageviewlayout.h"
#include "scorehorizontalviewlayout.h"
#include "scoreverticalviewlayout.h"

#include "dumplayoutdata.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

struct CmdStateLocker {
private:
    Score *score_ = nullptr;
public:
    CmdStateLocker(Score *s) : score_(s) { score_->cmd_state().lock(); }

    ~CmdStateLocker() { score_->cmd_state().unlock(); }
};

void ScoreLayout::layout_range(Score *score, const Fraction &st, const Fraction &et) {
    TRACEFUNC;

    CmdStateLocker cmd_state_locker(score);
    LayoutContext ctx(score);

    Fraction stick(st);
    Fraction etick(et);
    assert(!(stick == Fraction(-1, 1) && etick == Fraction(-1, 1)));

    // Check empty score
    if (!score->last() || (ctx.conf().is_linear_mode() && !score->first_measure())) {
        LOGD() << "empty score";
        muse::delete_all(score->systems());
        score->systems().clear();
        muse::delete_all(score->pages());
        score->pages().clear();
        PageLayout::get_next_page(ctx);
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

    ctx.mut_state().set_is_layout_all(is_layout_all);

    // Init context and layout
    switch (ctx.conf().view_mode()) {
        case LayoutMode::PAGE:
        case LayoutMode::FLOAT: ScorePageViewLayout::layout_page_view(score, ctx, stick, etick); break;
        case LayoutMode::LINE:
        case LayoutMode::HORIZONTAL_FIXED: ScoreHorizontalViewLayout::layout_horizontal_view(score, ctx, stick, etick); break;
        case LayoutMode::SYSTEM: ScoreVerticalViewLayout::layout_vertical_view(score, ctx, stick, etick); break;
    }

    //LOGDA() << DumpLayoutData::dump(score);
}
