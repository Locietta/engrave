/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited
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

#include "dom/instrumentname.h"
#include "dom/sharedpart.h"
#include "layoutcontext.h"

namespace mu::engraving {
struct Bracket;
struct BracketItem;
struct InstrumentName;
struct Staff;
struct System;
struct SysStaff;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct SystemHeaderLayout {
    static void update_system_header_width(System *system, LayoutContext &ctx);

    static double layout_brackets(System *system, LayoutContext &ctx);
    static void add_brackets(System *system, Measure *measure, LayoutContext &ctx);
    static double total_bracket_offset(LayoutContext &ctx);
    static void set_brackets_x_position(System *system, double x_position);
    static void layout_brackets_vertical(System *system, LayoutContext &ctx);

    static void compute_instrument_name_offset(System *system, LayoutContext &ctx);
    static void compute_instrument_names_width(System *system, LayoutContext &ctx);
    static void set_instrument_names_vertical_pos(System *system, LayoutContext &ctx);
    static void set_instrument_names_horizontal_pos(System *system);
    static void set_group_brackets_horizontal_pos(System *system);
    static void set_instrument_names(System *system, LayoutContext &ctx);

    static String format_shared_voice_label(const std::vector<const Instrument *> &instruments, bool trailing_dot_single,
                                            bool trailing_dot_multiple, int hyphen_limit);

private:
    static Bracket *create_bracket(System *system, LayoutContext &ctx, BracketItem *bi, size_t column, staff_idx_t staff_idx,
                                   std::vector<Bracket *> &bl, Measure *measure);
    static void compute_group_brackets_widths(System *system, LayoutContext &ctx);
    static void compute_stacked_brackets_width(Bracket *first, const std::vector<Bracket *> &all_group_brackets_ordered_by_column,
                                               double &width, std::vector<Bracket *> &stack);

    static void set_shared_part_names(SharedPart *shared_part, staff_idx_t start_staff_idx, System *system, LayoutContext &ctx,
                                      const Fraction &tick = {0, 1});

    static void update_group_names(System *system, LayoutContext &ctx, const Fraction &tick);
    static bool use_group_names(const String &instrument_group, LayoutContext &ctx);
    static InstrumentName *update_name(System *system, staff_idx_t staff_idx, LayoutContext &ctx, const String &name,
                                       InstrumentNameType type, InstrumentNameRole role);
    static String formatted_instrument_name(System *system, Part *part, const Fraction &tick, bool long_names);
    static String formatted_group_name(System *system, Part *part, const Fraction &tick, bool long_names);

    static InstrumentLabelVisibility resolve_instrument_label_visibility(const Staff *staff, const Fraction &tick, LayoutContext &ctx,
                                                                         bool first_system);
    static InstrumentNameType resolve_instrument_name_type(InstrumentLabelVisibility visibility);

    static String formatted_shared_staff_label(staff_idx_t staff_idx, const SharedTrackMap &track_map,
                                               const std::vector<Part *> &origin_parts);
    static String format_vertical_shared_label(const std::vector<const Instrument *> &instruments, bool trailing_dot_single);

    static String &resolve_tokens(String &str, const String &name, const String &transposition, const String &number);
    static double name_width_including_group_brackets(InstrumentName *name, System *system);

    static bool stack_labels_vertically(System *system);
};
} // namespace mu::engraving::rendering::score
