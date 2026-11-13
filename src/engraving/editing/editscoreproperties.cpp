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

#include "editscoreproperties.h"

#include "../dom/score.h"
#include "../dom/scoreorder.h"

using namespace mu::engraving;

void mu::engraving::apply_change_score_order(Score *score, ScoreOrder order) {
    note_change(score, CommandType::CHANGE_SCORE_ORDER);

    score->set_score_order(order);
}

void mu::engraving::apply_change_page_number_offset(Score *score, int page_offset) {
    note_change(score, CommandType::UNKNOWN);

    score->set_page_number_offset(page_offset);
    score->set_layout_all();
}
