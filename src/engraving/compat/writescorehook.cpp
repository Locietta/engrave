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
#include "writescorehook.h"

#include "dom/excerpt.h"
#include "dom/masterscore.h"
#include "rw/write/writer.h"
#include "rw/xmlwriter.h"

using namespace mu::engraving;
using namespace mu::engraving::compat;
using namespace mu::engraving::write;

void WriteScoreHook::on_write_style302(Score *score, XmlWriter &xml, bool inline_style) {
    bool is_write_style = inline_style;

    //! NOTE If not the master score, because the Excerpts (parts) have not yet been write to separate files
    if (!score->is_master()) {
        is_write_style = true;
    }

    //! NOTE If the test mode, because the tests have not yet been adapted to the new format
    if (MScore::test_mode && MScore::test_write_style_to_score) {
        is_write_style = true;
    }

    if (is_write_style) {
        score->style().save(xml, true); // save only differences to builtin style (logic from 3.)
    }
}

void WriteScoreHook::on_write_excerpts302(Score *score, XmlWriter &xml, WriteContext &ctx) {
    bool is_write_excerpts = ctx.inline_excerpts();

    //! NOTE If the test mode, because the tests have not yet been adapted to the new format
    if (MScore::test_mode) {
        is_write_excerpts = true;
    }

    if (is_write_excerpts && score->is_master() && !ctx.should_write_range()) {
        MasterScore *m_score = static_cast<MasterScore *>(score);
        for (const Excerpt *excerpt : m_score->excerpts()) {
            if (excerpt->excerpt_score() != score) {
                write::Writer::write(excerpt->excerpt_score(), xml, ctx, *this); // recursion write
            }
        }
    }
}
