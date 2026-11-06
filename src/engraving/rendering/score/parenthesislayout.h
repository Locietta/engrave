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

#include "dom/parenthesis.h"
#include "layoutcontext.h"

namespace mu::engraving {
struct Segment;
struct System;
struct ParenPaddingTable;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct ParenthesisLayout {
    static void layout_parentheses(const EngravingItem *parent, const LayoutContext &ctx);
    static void layout_chord_parentheses(const Chord *chord, const LayoutContext &ctx);
    static void layout_parenthesis(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx);

    static double compute_parenthesis_padding(const EngravingItem *item1, const EngravingItem *item2);
    static double compute_internal_parenthesis_padding(const EngravingItem *item1, const EngravingItem *item2);

    static void create_path_and_shape(Parenthesis *item, Parenthesis::LayoutData *ldata);
    static void create_smufl_shape(Parenthesis *item, Parenthesis::LayoutData *ldata);
private:
    static void layout_parentheses(Parenthesis *left_paren, Parenthesis *right_paren, Shape &dummy_item_shape, bool item_add_to_skyline,
                                   const LayoutContext &ctx);

    static void set_layout_values(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx);
    static void set_clef_values(Parenthesis *item, Parenthesis::LayoutData *ldata);
    static void set_time_sig_values(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx);
    static void set_chord_values(Parenthesis *item, Parenthesis::LayoutData *ldata);
    static void set_harmony_values(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx);
    static void set_default_values(Parenthesis *item, Parenthesis::LayoutData *ldata);

    static Shape get_parent_shape(const EngravingItem *parent);

    static Shape get_note_shape(const Note *note, Parenthesis *paren);

    static bool is_internal_paren_padding(const EngravingItem *item1, const EngravingItem *item2);
    static double compute_external_parenthesis_padding(const EngravingItem *item1, const EngravingItem *item2);
};
} // namespace mu::engraving::rendering::score
