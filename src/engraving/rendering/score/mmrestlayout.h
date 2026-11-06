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

#include "layoutcontext.h"

namespace mu::engraving {
struct Measure;
struct Segment;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct MMRestLayout {

    static void create_multi_measure_rests_if_need(Measure *first_measure, LayoutContext &ctx);
    static void layout_mm_rest_range(Measure *m, LayoutContext &ctx);

private:
    static void create_mm_rest(LayoutContext &ctx, Measure *first_measure, Measure *last_measure, const Fraction &len);

    static void reuse_existing_mm_rest(LayoutContext &ctx, Measure *mmr_measure, Measure *last_measure, Fraction len);
    static void remove_mm_rest_elements(Measure *mm_rest_measure, LayoutContext &ctx);
    static void change_measure_el_parents(Measure *first_measure, Measure *last_measure, Measure *mmr_measure, LayoutContext &ctx);
    static void restore_measure_el_parents(Measure *first_measure, Measure *last_measure, Measure *mmr_measure, LayoutContext &ctx);
    static void change_annotations_parent(Segment *old_parent, Segment *new_parent, const LayoutContext &ctx, bool check_visibility);
    static Segment *change_elements_parent(Segment *old_seg, Measure *new_measure, const Fraction &new_seg_tick, LayoutContext &ctx,
                                           bool check_visibility);

    static bool valid_mm_rest_measure(const LayoutContext &ctx, const Measure *m);
    static bool break_mmr_for_element(Measure *measure, Measure *prev_measure, const LayoutContext &ctx);
    static bool break_multi_measure_rest(const LayoutContext &ctx, Measure *m);
};
} // namespace mu::engraving::rendering::score
