/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "layoutcontext.h"

namespace mu::engraving {
class ChordBracket;
class Segment;
class Shape;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class ChordBracketLayout {
public:
    static void layout_segment(Segment *segment, LayoutContext &ctx);
    static void update_horizontal_spacing(const Segment *first_segment, const Segment *second_segment, staff_idx_t staff_idx,
                                          double squeeze_factor, double &min_distance);
    static void update_vertical_geometry(ChordBracket *bracket, LayoutContext &ctx);

private:
    struct BracketVisualSpan {
        staff_idx_t top_staff = 0;
        staff_idx_t bottom_staff = 0;

        // Staff-local coordinates.
        double top_y = 0.0;
        double bottom_y = 0.0;
    };

    static std::vector<ChordBracket *> chord_brackets_in_segment(const Segment *segment);
    static BracketVisualSpan bracket_visual_span(const ChordBracket *bracket);
    static Shape bracket_collision_shape(const ChordBracket *bracket, const BracketVisualSpan &span, staff_idx_t staff_idx, double x);
    static void layout_horizontal(ChordBracket *bracket);
};
} // namespace mu::engraving::rendering::score
