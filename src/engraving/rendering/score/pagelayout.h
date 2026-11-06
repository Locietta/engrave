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

#pragma once

#include "layoutcontext.h"

namespace mu::engraving {
struct Page;
struct System;
enum class SystemDividerType : unsigned char;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct PageLayout {

    static void get_next_page(LayoutContext &ctx);
    static void collect_page(LayoutContext &ctx);

private:
    static void layout_page(LayoutContext &ctx, Page *page, double rest_height, double footer_padding, bool squeeze_to_fit);
    static void distribute_staves(LayoutContext &ctx, Page *page, double footer_padding, bool squeeze_to_fit);

    static void layout_cross_staff_elements(LayoutContext &ctx, Page *page);
    static void layout_cross_staff_slurs(LayoutContext &ctx, System *system);
    static void layout_artic_and_fingering_on_cross_staff_beams(LayoutContext &ctx, System *system);

    static void layout_system_dividers(LayoutContext &ctx, Page *page);
    static void update_system_divider(LayoutContext &ctx, System *system, System *next_system, SystemDividerType type, bool needs_divider);
};
} // namespace mu::engraving::rendering::score
