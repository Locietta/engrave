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
class GuitarBend;
class GuitarBendSegment;
class LineSegment;
class Note;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class GuitarDiveLayout {
public:
    friend class GuitarBendLayout;

    static void update_dive_sequences(const std::vector<mu::engraving::GuitarBend *> &bends, const LayoutContext &ctx);
    static void layout_dive_tab_staff(GuitarBendSegment *item, LayoutContext &);

private:
    static PointF compute_start_pos_on_staff(GuitarBendSegment *item, LayoutContext &ctx);
    static PointF compute_end_pos_on_staff(GuitarBendSegment *item, LayoutContext &ctx);

    static PointF compute_start_pos_above_staff(GuitarBendSegment *item, LayoutContext &ctx);
    static PointF compute_end_pos_above_staff(GuitarBendSegment *item, LayoutContext &);

    static LineSegment *find_prev_hold_or_bend_segment(GuitarBendSegment *item, bool exclude_full_release_dive = false);

    static void layout_dip(GuitarBendSegment *item, LayoutContext &ctx);
    static void layout_scoop(GuitarBendSegment *item);

    static RectF get_note_and_parentheses_shape(const Note *note);
};
} // namespace mu::engraving::rendering::score
