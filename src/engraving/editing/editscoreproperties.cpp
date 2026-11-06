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

//---------------------------------------------------------
//   ChangeMetaTags
//---------------------------------------------------------

void ChangeMetaTags::flip() {
    std::map<String, String> t = score->meta_tags();
    score->set_meta_tags(meta_tags);
    score->set_layout_all();
    meta_tags = t;
}

//---------------------------------------------------------
//   ChangeMetaText
//---------------------------------------------------------

void ChangeMetaText::flip() {
    String s = score->meta_tag(id);
    score->set_meta_tag(id, text);
    score->set_layout_all();
    text = s;
}

//---------------------------------------------------------
//   ChangeScoreOrder
//---------------------------------------------------------

void ChangeScoreOrder::flip() {
    ScoreOrder s = score->score_order();
    score->set_score_order(order);
    order = s;
}

//---------------------------------------------------------
//   ChangePageNumberOffset
//---------------------------------------------------------

void ChangePageNumberOffset::flip() {
    int po = score->page_number_offset();

    score->set_page_number_offset(page_offset);
    score->set_layout_all();

    page_offset = po;
}
