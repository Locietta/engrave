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

#include <vector>

namespace mu::engraving {
class EngravingItem;
class HammerOnPullOff;
class System;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class AlignmentLayout {
public:
    static void align_items_with_their_snapping_chain(const std::vector<EngravingItem *> &elements, const System *system);
    static void align_items_for_system(const std::vector<EngravingItem *> &elements, const System *system);
    static void align_items_group(const std::vector<EngravingItem *> &elements, const System *system);
    static void align_hopo_letters(const HammerOnPullOff *hopo, const System *system);

private:
    static void move_item_to_y(EngravingItem *item, double y, const System *system);
    static double y_optical_center(const EngravingItem *item);
    static bool is_above(const EngravingItem *item);
};
} // namespace mu::engraving::rendering::score
