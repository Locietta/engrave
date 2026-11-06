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

#pragma once

#include "dom/textbase.h"
#include "draw/types/font.h"
#include "layoutcontext.h"

namespace mu::engraving::rendering::score {
struct TextLayout {
    static void layout_base_text_base(const TextBase *item, TextBase::LayoutData *data);
    static void layout_base_text_base(TextBase *item, LayoutContext &ctx);
    static void layout_base_text_base1(const TextBase *item, TextBase::LayoutData *data);
    static void layout_base_text_base1(TextBase *item, const LayoutContext &ctx);

    static void compute_text_high_res_shape(const TextBase *item, TextBase::LayoutData *ldata);
    static void layout_text_block(TextBlock *item, const TextBase *t);

    static void substitute_music_font(muse::draw::Font &font, double size);

private:
    static void text_horizontal_layout(const TextBase *item, Shape &shape, double max_block_width, TextBase::LayoutData *ldata);
    static void justify_line(const TextBase *item, TextBlock *text_block, double max_block_width);
    static double music_symbol_base_line_adjust(const TextBlock *block, const TextBase *t, const TextFragment &f,
                                                const std::list<TextFragment>::iterator fi);
};
} // namespace mu::engraving::rendering::score
