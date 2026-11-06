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

#include "../layoutoptions.h"
#include "layoutcontext.h"

namespace mu::engraving {
struct Measure;
struct MeasureBase;
struct Parenthesis;
struct Score;
struct Segment;
struct StaffLines;
enum class SegmentType;
enum class CourtesySegmentPosition : char;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct MeasureLayout {
    struct MeasureStartEndPos {
        MeasureStartEndPos(double x1, double x2) : x1(x1), x2(x2) {}

        double x1;
        double x2;
    };

    MeasureLayout() = default;

    static void layout2(Measure *item, LayoutContext &ctx);

    static void get_next_measure(LayoutContext &ctx);
    static void update_ticks_meas_numbers_and_mm_rests(MeasureBase *current_mb, LayoutContext &ctx);
    static void layout_measure(MeasureBase *current_mb, LayoutContext &ctx);

    static void compute_pre_spacing_items(Measure *m, LayoutContext &ctx);

    static void layout_staff_lines(Measure *m, LayoutContext &ctx);
    static void layout_play_count_text(Measure *m, LayoutContext &ctx);
    static void layout_measure_number(Measure *m, LayoutContext &ctx);
    static void layout_measure_elements(Measure *m, LayoutContext &ctx);

    static void create_end_bar_lines(Measure *m, bool is_last_measure_in_system, LayoutContext &ctx);
    static Segment *add_header_clef(Measure *m, bool is_first_clef, const Staff *staff, LayoutContext &ctx);
    static Segment *add_header_key_sig(Measure *m, bool is_first_keysig, const Staff *staff, LayoutContext &ctx);
    static void add_system_header(Measure *m, bool is_first_system, LayoutContext &ctx);
    static void remove_system_header(Measure *m);
    static void add_system_trailer(Measure *m, Measure *nm, LayoutContext &ctx);
    static void remove_system_trailer(Measure *m, LayoutContext &ctx);

    static void set_repeat_courtesies_and_parens(Measure *m, LayoutContext &ctx);

    static void update_grace_notes(Measure *measure, LayoutContext &ctx);

    static void create_system_begin_bar_line(Measure *m, LayoutContext &ctx);

    static void stretch_measure_in_practice_mode(Measure *m, double target_width, LayoutContext &ctx);

    static void layout_time_tick_anchors(Measure *m, LayoutContext &ctx);

    static MeasureStartEndPos get_measure_start_end_pos(const Measure *measure, const Segment *first_cr_seg, const staff_idx_t staff_idx,
                                                        const bool needs_header_exception, const bool modern_mm_rest,
                                                        const LayoutContext &ctx);

    static void update_key_signatures(const Measure *measure, LayoutContext &ctx);

    static int adjust_measure_number(Measure *m, int measure_number);

private:
    static void bar_lines_set_span(Segment *seg, LayoutContext &ctx);

    static void layout_partial_width(StaffLines *lines, LayoutContext &ctx, double w, double w_partial, bool align_left);

    static void move_to_next_measure(LayoutContext &ctx);

    static void check_staff_move_validity(Measure *measure, const LayoutContext &ctx);

    static void set_clef_seg_visibility(Measure *m, bool is_last_measure_in_system, LayoutContext &ctx);

    static void set_courtesy_time_sig(Measure *m, const Fraction &ref_sig_tick, const Fraction &courtesy_sig_tick,
                                      const SegmentType seg_type, LayoutContext &ctx);
    static void set_courtesy_key_sig(Measure *m, const Fraction &ref_sig_tick, const Fraction &courtesy_sig_tick,
                                     const SegmentType seg_type, LayoutContext &ctx);
    static void set_courtesy_clef(Measure *m, const Fraction &ref_sig_tick, const Fraction &courtesy_sig_tick, const SegmentType seg_type,
                                  LayoutContext &ctx);

    static void remove_repeat_courtesy_parentheses_measure(Measure *m, const bool continuation, LayoutContext &ctx);
    static void remove_repeat_courtesy_parenthesis(EngravingItem *item, const DirectionH direction = DirectionH::AUTO);
    static void add_repeat_courtesy_parentheses(Measure *m, const bool continuation, LayoutContext &ctx);
    static void add_repeat_courtesies(Measure *m, LayoutContext &ctx);
    static void remove_repeat_courtesies(Measure *m);
    static void add_repeat_continuation_courtesies(Measure *m, LayoutContext &ctx);
    static void remove_repeat_continuation_courtesies(Measure *m);
};
} // namespace mu::engraving::rendering::score
