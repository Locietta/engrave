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
 File handling: loading and saving.
 */

#include "../editing/transaction/transaction.h"
#include "barline.h"
#include "engravingitem.h"
#include "excerpt.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "measurebase.h"
#include "part.h"
#include "repeatlist.h"
#include "score.h"
#include "segment.h"
#include "style/style.h"

namespace mu::engraving {
static void remove_repeat_markings(Score *score) {
    // remove bar-level repeats
    for (Measure *m = score->first_measure(); m; m = m->next_measure()) {
        m->set_repeat_start(false);
        m->set_repeat_end(false);
    }

    // remove volta markers
    auto smap = score->spanner_map().map();
    for (auto it = smap.cbegin(); it != smap.cend(); ++it) {
        Spanner *s = (*it).second;
        if (!s || !s->is_volta()) {
            continue;
        }
        score->remove_spanner(s);
    }

    // remove coda/fine labels and jumps
    std::vector<EngravingItem *> elems = score->get_children(false);
    for (auto e : elems) {
        if (e->is_marker() || e->is_jump()) {
            score->delete_item(e);
        } else if (e->is_bar_line()) {
            to_bar_line(e)->set_bar_line_type(BarLineType::NORMAL);
        }
    }

    // set the last bar line to end symbol
    score->last_measure()->set_end_bar_line_type(BarLineType::END, false);
    Segment *last = score->last_measure()->segments().last();
    if (last->segment_type() == SegmentType::END_BAR_LINE) {
        auto els = last->elist();
        for (size_t i = 0; i < els.size(); i++) {
            if (!els[i]) {
                continue;
            }
            to_bar_line(els[i])->set_bar_line_type(BarLineType::END);
        }
    }
}

//---------------------------------------------------------
//   createExcerpts
//    re-create all the excerpts once the master score
//    has been unrolled
//---------------------------------------------------------

static void create_excerpts(MasterScore *cs, const std::vector<Excerpt *> &excerpts) {
    // borrowed from musescore.cpp endsWith(".pdf")
    for (Excerpt *e : excerpts) {
        Score *nscore = e->master_score()->create_score();
        e->set_excerpt_score(nscore);
        nscore->style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
        cs->transaction_manager()->transaction(TranslatableString("undoableAction", "Create parts"), [&](auto &tx) {
            tx.change_excerpt_membership(e, true);
            Excerpt::create_excerpt(e);

            // borrowed from excerptsdialog.cpp
            // a new excerpt is created in AddExcerpt, make sure the parts are filed
            for (Excerpt *ee : e->master_score()->excerpts()) {
                if (ee->excerpt_score() == nscore && ee != e) {
                    ee->parts().clear();
                    ee->parts().insert(ee->parts().end(), e->parts().begin(), e->parts().end());
                }
            }
        });
    }
}

//---------------------------------------------------------
//   unrollRepeats
//    unroll all the repeats
//---------------------------------------------------------

} // namespace mu::engraving
