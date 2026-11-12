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
#include "passresetlayoutdata.h"

#include "dom/score.h"
#include "layoutcontext.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

static void reset_layout_data(EngravingObject *obj) {
    if (obj->is_engraving_item()) {
        EngravingItem *item = to_engraving_item(obj);
        if (item->has_layout_data()) {
            item->mutldata()->reset();
        }
    }

    for (EngravingObject *ch : obj->children()) {
        reset_layout_data(ch);
    }
}

void PassResetLayoutData::do_run(Score *score, LayoutContext &ctx) {
    if (ctx.state().is_layout_all()) {
        reset_layout_data(score);
    } else {
        MeasureBase *m = ctx.mut_state().next_measure();
        while (m && m->tick() <= ctx.state().end_tick()) {
            reset_layout_data(m);
            m = m->next();
        }
    }
}
