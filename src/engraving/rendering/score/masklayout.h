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
struct BarLine;
struct Page;
struct System;
struct StaffLines;
struct TextBase;
struct SlurTieSegment;
enum class SegmentType;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct MaskLayout {
    static void compute_masks(LayoutContext &ctx, Page *page);

private:
    static void compute_barline_masks(const Segment *barline_sement, const System *system, const std::vector<TextBase *> &all_system_text,
                                      LayoutContext &ctx);
    static void mask_barline_for_text(BarLine *barline, const std::vector<TextBase *> &all_system_text);
    static std::vector<TextBase *> collect_all_system_text(const System *system);

    static void compute_slur_tie_masks(SlurTieSegment *slur_tie_segment);

    static Shape create_filtered_item_shape(const Shape &overlying_item_shape, const Shape &masked_item_shape,
                                            const double collision_padding);
    static Shape create_filtered_item_shape(const Shape &overlying_item_shape, const Shape &masked_item_shape,
                                            const double horizontal_collision_padding, const double vertical_collision_padding);

    static void cleanup_mask(const Shape &item_shape, Shape &mask, double min_fragment_length);

    static void mask_tab_string_lines_for_frets(StaffLines *staff_lines, const LayoutContext &ctx);
};
} // namespace mu::engraving::rendering::score
