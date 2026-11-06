/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#include "testutils.h"

#include "gtest/gtest.h"

#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"

using namespace mu::engraving;

Score *TestUtils::create_part(MasterScore *master_score, size_t part_number) {
    std::vector<Part *> parts;
    parts.push_back(master_score->parts().at(part_number));
    Score *nscore = master_score->create_score();
    if (!nscore) {
        ADD_FAILURE() << "nscore is null";
        return nullptr;
    }

    Excerpt *ex = new Excerpt(master_score);
    ex->set_excerpt_score(nscore);
    ex->set_parts(parts);
    ex->set_name(parts.front()->part_name());
    Excerpt::create_excerpt(ex);

    master_score->excerpts().push_back(ex);
    master_score->set_excerpts_changed(true);

    return nscore;
}

Score *TestUtils::create_empty_part(MasterScore *master_score) {
    Score *nscore = master_score->create_score();
    if (!nscore) {
        ADD_FAILURE() << "nscore is null";
        return nullptr;
    }

    Excerpt *ex = new Excerpt(master_score);
    ex->set_excerpt_score(nscore);

    Excerpt::create_excerpt(ex);

    master_score->excerpts().push_back(ex);
    master_score->set_excerpts_changed(true);

    return nscore;
}

void TestUtils::create_parts(MasterScore *master_score, size_t number_of_parts) {
    EXPECT_TRUE(number_of_parts > 0);

    for (size_t part = 0; part < number_of_parts; part++) {
        create_part(master_score, part);
    }
}
