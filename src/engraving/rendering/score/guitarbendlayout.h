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
class GuitarBendHoldSegment;
class Note;
class SLine;
class System;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class GuitarBendLayout {
public:
    /* If system is different from nullptr, only the segment belonging to it is laid out.
     * Otherwise, all segments which have a system are laid out. */
    static void update_segments_and_layout(SLine *item, LayoutContext &ctx, System *system = nullptr);

    static void layout_standard_staff(GuitarBendSegment *item, LayoutContext &ctx);
    static void layout_tab_staff(GuitarBendSegment *item, LayoutContext &);
    static void layout_hold_line(GuitarBendHoldSegment *item);

private:
    // Standard staff layout
    static void layout_angular_bend(GuitarBendSegment *item, LayoutContext &ctx);
    static void compute_up(GuitarBend *item);
    static void compute_is_inside(GuitarBend *item);
    static void avoid_bad_staff_line_intersection(GuitarBendSegment *item, PointF &point);
    static void adjust_x(GuitarBendSegment *item, PointF &start_pos, PointF &end_pos, const Note *start_note, const Note *end_note);
    static void layout_slight_bend(GuitarBendSegment *item, LayoutContext &);

    // TAB layout
    static void layout_bend_tab_staff(GuitarBendSegment *item, LayoutContext &);
    static PointF compute_start_pos(GuitarBendSegment *item, Note *start_note, double dist_above_tab, double vertical_pad,
                                    double arrow_height);
    static PointF compute_end_pos(GuitarBendSegment *item, Note *end_note, double dist_above_tab, double vertical_pad, double arrow_height,
                                  double arrow_width, const PointF &start_pos, const PointF &prev_end_point);
    static void check_conflict_with_other_bends(GuitarBendSegment *item);
    static bool start_on_end_note(GuitarBend *bend);
};
} // namespace mu::engraving::rendering::score
