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

#include "types/types.h"

namespace mu::engraving {
class Chord;
class ChordBracket;
class EngravingItem;
class Fraction;
class Lyrics;
class Note;
class Rest;
class Shape;
class StemSlash;
class Segment;
class Measure;
class System;
enum class ElementType : unsigned char;
enum class KerningType : unsigned char;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class HorizontalSpacing {
public:

    static double compute_spacing_for_full_system(System *system, double stretch_reduction = 1.0, double squeeze_factor = 1.0,
                                                  bool override_min_measure_width = false);
    static double update_spacing_for_last_added_measure(System *system, bool start_of_continuous_layout_region = false);
    static void squeeze_system_to_fit(System *system, double &cur_sys_width, double target_sys_width);
    static void justify_system(System *system, double cur_sys_width, double target_system_width);

    static double min_horizontal_distance(const Shape &f, const Shape &s, double spatium, double squeeze_factor = 1.0);
    //! NOTE Temporary solution
    static double shape_spatium(const Shape &s);

    static double min_horizontal_distance(const Segment *f, const Segment *ns, double squeeze_factor);
    static double min_left(const Segment *seg, const Shape &ls);

    static double compute_padding(const EngravingItem *item1, const EngravingItem *item2);
    static KerningType compute_kerning(const EngravingItem *item1, const EngravingItem *item2);
    static double compute_vertical_clearance(const EngravingItem *item1, const EngravingItem *item2, double spatium);

private:
    struct HorizontalSpacingContext {
        System *system = nullptr;
        double spatium = 0.0;
        double x_cur = 0.0;
        double x_left_barrier = 0.0;
        bool system_is_full = false;
        Measure *start_meas = nullptr;
        double stretch_reduction = 1.0;
        double squeeze_factor = 1.0;
        double spacing_density = 1.0;
        bool override_min_measure_width = false;
    };

    struct SegmentPosition {
        Segment *segment;
        double x_pos_in_system_coords;
        bool ignore_for_spacing;

        SegmentPosition(Segment *s, double x, bool ignore_for_spacing = false)
            : segment(s), x_pos_in_system_coords(x), ignore_for_spacing(ignore_for_spacing) {}
    };

    struct Spring {
        double spring_const = 0.0;
        double width = 0.0;
        double pre_tension = 0.0;
        Segment *segment = nullptr;

        Spring(double sc, double w, double pt, Segment *s) : spring_const(sc), width(w), pre_tension(pt), segment(s) {}
    };

    struct CrossBeamSpacing {
        bool up_down = false;
        bool down_up = false;
        bool can_be_adjusted = true;
        bool has_opposing_beamlets = false;
        bool prevent_cross_staff_kerning = false;
        bool ensure_min_stem_distance = false;
    };

    static void space_measure_group(const std::vector<Measure *> &measure_group, HorizontalSpacingContext &ctx);
    static double get_first_segment_x_pos(Segment *segment, HorizontalSpacingContext &ctx);
    static std::vector<SegmentPosition> space_segments(const std::vector<Segment *> &seg_list, int start_seg_idx,
                                                       HorizontalSpacingContext &ctx);
    static bool ignore_segment_for_spacing(const Segment *segment);
    static bool ignore_all_segments_for_spacing(const std::vector<SegmentPosition> &segment_positions);
    static void space_against_previous_segments(Segment *segment, std::vector<SegmentPosition> &prev_seg_positions,
                                                HorizontalSpacingContext &ctx);
    static bool stop_checking_previous_segments(const SegmentPosition &prev, const SegmentPosition &cur_seg_pos);
    static void check_lyrics_against_left_margin(Segment *segment, double &x, HorizontalSpacingContext &ctx);
    static void check_lyrics_against_right_margin(std::vector<SegmentPosition> &seg_positions, const HorizontalSpacingContext &ctx);
    static double space_lyrics_against_barlines(Segment *first_seg, Segment *second_seg, const HorizontalSpacingContext &ctx);
    static void check_large_time_sig_against_right_margin(std::vector<SegmentPosition> &seg_positions);
    static void move_right_aligned_segments(std::vector<SegmentPosition> &placed_segments, const HorizontalSpacingContext &ctx);
    static void check_collisions_with_cross_staff_stems(const Segment *this_seg, const Segment *next_seg, staff_idx_t staff_idx,
                                                        double &cur_min_dist);

    static double chord_rest_segment_natural_width(Segment *segment, HorizontalSpacingContext &ctx);
    static double compute_segment_duration_stretch(const Segment *cur_seg, const Segment *prev_seg);
    static double duration_stretch_for_mm_rests(const Segment *segment);
    static double duration_stretch_for_ticks(double slope, const Fraction &ticks);
    static bool needs_cue_size_spacing(const Segment *segment);
    static bool needs_header_spacing_exceptions(const Segment *seg, const Segment *next_seg);

    static void apply_cross_beam_spacing_correction(Segment *this_seg, Segment *next_seg, double &width);
    static CrossBeamSpacing compute_cross_beam_spacing(Segment *this_seg, Segment *next_seg);
    static double min_stem_dist_on_non_adjacent_cross(const Segment *this_seg, const Segment *next_seg);

    static void enforce_minimum_measure_widths(const std::vector<Measure *> measure_group);
    static double compute_min_measure_width(Measure *m);
    static void stretch_measure_to_target_width(Measure *m, double target_width);

    static void stretch_segments_to_width(std::vector<Spring> &springs, double width);

    static void set_positions_and_widths(const std::vector<SegmentPosition> &segment_positions);

    static bool is_special_note_padding_type(ElementType type);
    static void compute_note_padding(const Note *note, const EngravingItem *item2, double &padding, double scaling);
    static void compute_ledger_rest_padding(const Rest *rest2, double &padding);
    static bool is_special_lyrics_padding_type(ElementType type);
    static void compute_lyrics_padding(const Lyrics *lyrics1, const EngravingItem *item2, double &padding);
    static void compute_chord_bracket_padding(const EngravingItem *item1, const ChordBracket *chord_bracket, double &padding);

    static bool is_same_voice_kerning_limited(const EngravingItem *item);
    static bool is_never_kernable(const EngravingItem *item);
    static bool is_always_kernable(const EngravingItem *item);
    static bool ignore_items(const EngravingItem *item1, const EngravingItem *item2);

    static KerningType do_compute_kerning_type(const EngravingItem *item1, const EngravingItem *item2);
    static KerningType compute_note_kerning_type(const Note *note, const EngravingItem *item2);
    static KerningType compute_stem_slash_kerning_type(const StemSlash *stem_slash, const EngravingItem *item2);
    static KerningType compute_lyrics_kerning_type(const Lyrics *lyrics1, const EngravingItem *item2);
    static KerningType compute_articulation_and_fermata_kerning(const EngravingItem *item1, const EngravingItem *item2);

    static void compute_hanging_line_width(const Segment *first_seg, const Segment *next_seg, double &width, bool system_header_gap,
                                           bool system_end);
};
} // namespace mu::engraving::rendering::score
