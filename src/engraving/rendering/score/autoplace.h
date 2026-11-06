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

#include "dom/articulation.h"
#include "dom/engravingitem.h"
#include "dom/spanner.h"

namespace mu::engraving::rendering::score {
struct Autoplace {

    static void autoplace_segment_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, bool above, bool add);
    static void autoplace_measure_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, bool above, bool add);

    static void autoplace_segment_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, bool add = true) {
        autoplace_segment_element(item, ldata, item->place_above(), add);
    }

    static void autoplace_measure_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, bool add = true) {
        autoplace_measure_element(item, ldata, item->place_above(), add);
    }

    static void autoplace_spanner_segment(const SpannerSegment *item, SpannerSegment::LayoutData *ldata);

    static double distance_to_move_item(const EngravingItem *item, const SkylineLine &staff_skyline, const Shape &shape, bool above,
                                        double min_skyline_horizontal_clearance, double min_distance);

    static void do_autoplace_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, const System *system, bool above,
                                     bool add, double min_skyline_horizontal_clearance, double spatium);

    static bool items_should_ignore_each_other(const EngravingItem *item_to_autoplace, const EngravingItem *item_in_skyline);
};
} // namespace mu::engraving::rendering::score
