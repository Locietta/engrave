/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#include "vectorview.h"

#include "shape.h"

namespace mu::engraving {
class Accidental;
class Chord;
class EngravingItem;
class LedgerLine;
class Shape;
struct ShapeElement;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class LayoutContext;

using AccidentalGroups = std::vector<std::vector<Accidental *>>;

class AccidentalsLayout {
public:
    static void layout_accidentals(const std::vector<Chord *> &chords, LayoutContext &ctx);

private:
    struct AccidentalsLayoutContext {
        AccidentalsLayoutContext(std::vector<Accidental *> acc, std::vector<Chord *> ch);

        std::vector<Accidental *> all_accidentals;
        std::vector<Chord *> chords;
        AccidentalGroups accidental_sub_chords;
        std::vector<Accidental *> stacked_accidentals;
        Shape stacked_accidentals_shape;
        Shape chords_shape;
        size_t sub_chords_count_before_octave_merge = 1;

        double spatium() const { return spatium_; }

        double vertical_accidental_to_accidental_clearance() const { return vertical_accidental_to_accidental_clearance_; }

        double vertical_sharp_to_sharp_clearance() const { return vertical_sharp_to_sharp_clearance_; }

        double vertical_accidental_to_chord_clearance() const { return vertical_accidental_to_chord_clearance_; }

        double accidental_accidental_distance() const { return accidental_accidental_distance_; }

        double additional_padding_for_verticals() const { return additional_padding_for_verticals_; }

        double x_pos_split_threshold() const { return x_pos_split_threshold_; }

        double x_vertical_alignment_threshold() const { return x_vertical_alignment_threshold_; }

        double sharp_and_natural_ledger_line_padding() const { return sharp_and_natural_ledger_line_padding_; }

        double reduced_flat_to_note_padding() const { return reduced_flat_to_note_padding_; }

        double flat_kerning_of_fourth() const { return flat_kerning_of_fourth_; }

        double natural_kerning_of_fourth() const { return natural_kerning_of_fourth_; }

        bool order_follow_note_displacement() const { return order_follow_note_displacement_; }

        bool align_octaves_across_sub_chords() const { return align_octaves_across_sub_chords_; }

        bool keep_seconds_together() const { return keep_seconds_together_; }

        bool align_offset_octaves() const { return align_offset_octaves_; }

        size_t large_group_limit() const {
            return ::mu::engraving::rendering::score::AccidentalsLayout::AccidentalsLayoutContext::large_group_limit_;
        }

        size_t small_group_limit() const {
            return ::mu::engraving::rendering::score::AccidentalsLayout::AccidentalsLayoutContext::small_group_limit_;
        }

    private:
        void init_constants();

        double spatium_ = 0.0;
        double vertical_accidental_to_accidental_clearance_ = 0.0;
        double vertical_sharp_to_sharp_clearance_ = 0.0;
        double vertical_accidental_to_chord_clearance_ = 0.0;
        double accidental_accidental_distance_ = 0.0;
        double additional_padding_for_verticals_ = 0.0;
        double x_pos_split_threshold_ = 0.0;
        double x_vertical_alignment_threshold_ = 0.0;
        double sharp_and_natural_ledger_line_padding_ = 0.0;
        double reduced_flat_to_note_padding_ = 0.0;
        double flat_kerning_of_fourth_ = 0.0;
        double natural_kerning_of_fourth_ = 0.0;
        bool order_follow_note_displacement_ = false;
        bool align_octaves_across_sub_chords_ = false;
        bool keep_seconds_together_ = false;
        bool align_offset_octaves_ = false;

        static constexpr size_t small_group_limit_ = 3;
        static constexpr size_t large_group_limit_ = 6;
    };

    using AccidentalsVectorView = muse::VectorView<Accidental *>;

    static void collect_accidentals(const std::vector<Chord *> chords, std::vector<Accidental *> &all_accidentals,
                                    std::vector<Accidental *> &redundant_accidentals, std::vector<Accidental *> &invisible_accidentals);
    static bool accidental_is_redundant(const Accidental *acc, const std::vector<Accidental *> &all_accidentals);

    static void do_accidental_placement(AccidentalsLayoutContext &ctx);

    static void find_octaves_and_seconds(const AccidentalsLayoutContext &ctx);

    static void split_into_sub_chords(AccidentalsLayoutContext &ctx);
    static void merge_adjacent_sub_groups_if_too_small(AccidentalsLayoutContext &ctx);
    static void merge_sub_groups_with_octaves_across(AccidentalsLayoutContext &ctx);

    static void create_chords_shape(AccidentalsLayoutContext &ctx);

    static void layout_sub_chord(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx);
    static void stack_accidental(Accidental *acc, AccidentalsLayoutContext &ctx);

    static void compute_ordering(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx);
    static AccidentalGroups split_into_priority_groups(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx);

    static AccidentalGroups split_according_to_accid_displacement(std::vector<Accidental *> &accidentals,
                                                                  const AccidentalsLayoutContext &ctx);
    static AccidentalGroups group_accidentals_by_x_pos(std::vector<Accidental *> &accidentals, const AccidentalsLayoutContext &ctx);
    static void move_octaves_to_second_group(AccidentalGroups &sub_groups);

    static AccidentalGroups split_according_to_note_displacement(std::vector<Accidental *> &accidentals,
                                                                 const AccidentalsLayoutContext &ctx);
    static AccidentalGroups group_accidentals_by_note_x_pos(const std::vector<Accidental *> &accidentals);
    static void move_seconds_in_same_group(AccidentalGroups &sub_groups);

    static void determine_stacking_order(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx);
    static void compute_standard_ordering(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx);
    static void compute_compact_ordering(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx);
    static void compute_ordering_with_least_columns(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx);
    static void find_and_insert_second(Accidental *acc, std::vector<Accidental *> &accidentals_placed,
                                       AccidentalsVectorView &accidentals_to_place, AccidentalsLayoutContext &ctx);
    static bool find_and_insert_octave(Accidental *acc, std::vector<Accidental *> &accidentals_placed,
                                       AccidentalsVectorView &accidentals_to_place, AccidentalsLayoutContext &ctx, bool accept_above = true,
                                       bool accept_below = true);

    static void apply_ordering_offsets(std::vector<Accidental *> &accidentals);

    static double min_accidental_to_chord_distance(Accidental *acc, const Shape &acc_shape, const AccidentalsLayoutContext &ctx);
    static double kerning_limitations_into_chord(Accidental *acc, const Shape &acc_shape, const ShapeElement &chord_element,
                                                 const AccidentalsLayoutContext &ctx);
    static double compute_padding(Accidental *acc, const EngravingItem *chord_element, const AccidentalsLayoutContext &ctx);

    static double min_accidental_to_accidental_group_distance(Accidental *acc, Shape acc_shape, const Shape &accidentals_shape,
                                                              const AccidentalsLayoutContext &ctx);
    static double kerning_of_fourth(const Shape &acc_shape, const ShapeElement &acc_group_shape_element,
                                    const AccidentalsLayoutContext &ctx);
    static void check_zero_column(Accidental *acc, const AccidentalsLayoutContext &ctx);

    static double additional_padding_for_verticals(const Accidental *acc, const EngravingItem *item, const AccidentalsLayoutContext &ctx);

    static void vertically_align_accidentals(AccidentalsLayoutContext &ctx);
    static void collect_vertical_sets_of_offset_octaves(std::set<Accidental *> &accidentals_already_grouped,
                                                        std::map<Accidental *, std::vector<Accidental *>> &vertical_sets,
                                                        AccidentalsLayoutContext &ctx);
    static void collect_vertical_sets(std::set<Accidental *> &accidentals_already_grouped,
                                      std::map<Accidental *, std::vector<Accidental *>> &vertical_sets, AccidentalsLayoutContext &ctx);
    static void align_vertical_sets(AccidentalGroups &vert_sets, AccidentalsLayoutContext &ctx);

    static double vertical_padding(const Accidental *acc1, const Accidental *acc2, const AccidentalsLayoutContext &ctx);
    static bool can_fit_in_same_column(const Accidental *acc1, const Accidental *acc2, AccidentalsLayoutContext &ctx);

    static double horizontal_padding(const Accidental *acc1, const Accidental *acc2, const AccidentalsLayoutContext &ctx);

    static bool is_exception_of_fourth(const Accidental *acc1, const Accidental *acc2);
    static bool is_exception_of_naturals_sixth(const Accidental *acc1, const Accidental *acc2);
    static bool is_octave(const Accidental *acc1, const Accidental *acc2);

    static void set_xpos_relative_to_segment(Accidental *accidental, double x);
    static double x_pos_relative_to_segment(const Accidental *accidental);

    static void sort_top_down(std::vector<Accidental *> &accidentals);

    static bool keep_accidentals_close_to_chord(const Chord *chord);
};
} // namespace mu::engraving::rendering::score
