/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "edittie.h"

#include "../dom/chord.h"
#include "../dom/factory.h"
#include "../dom/laissezvib.h"
#include "../dom/masterscore.h"
#include "../dom/note.h"
#include "../dom/partialtie.h"
#include "../dom/score.h"
#include "../dom/tie.h"
#include "../dom/tiejumppointlist.h"
#include "../dom/utils.h"
#include "editchord.h"
#include "log.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

void mu::engraving::apply_change_tie_jump_point_active(TieJumpPointList *jump_point_list, muse::String id, bool active) {
    note_change(jump_point_list->start_tie(), CommandType::CHANGE_TIE_END_POINT_ACTIVE);

    TieJumpPoint *jump_point = jump_point_list->find_jump_point(id);
    if (!jump_point) {
        return;
    }

    if (active && jump_point->end_tie() && jump_point->end_tie()->jump_point() != jump_point) {
        jump_point->end_tie()->set_jump_point(jump_point);
    }

    jump_point->set_active(active);
}
