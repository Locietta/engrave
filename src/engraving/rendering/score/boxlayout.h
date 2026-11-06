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

#include "dom/box.h"
#include "layoutcontext.h"

namespace mu::engraving::rendering::score {
struct BoxLayout {
    static void layout_box(const Box *item, Box::LayoutData *ldata, const LayoutContext &ctx); // factory

    static void layout_h_box(const HBox *item, HBox::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_h_box2(HBox *item, const LayoutContext &ctx);

    static void layout_v_box(const VBox *item, VBox::LayoutData *ldata, const LayoutContext &ctx);

    static void layout_t_box(const TBox *item, TBox::LayoutData *ldata, const LayoutContext &ctx);

    static void layout_f_box(const FBox *item, FBox::LayoutData *ldata, const LayoutContext &ctx);

private:
    static void layout_base_box(const Box *item, Box::LayoutData *ldata, const LayoutContext &ctx); // base class
};
} // namespace mu::engraving::rendering::score
