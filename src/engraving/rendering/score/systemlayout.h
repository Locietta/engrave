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

#include "layoutcontext.h"

#include "../../dom/measure.h"
#include "../../dom/segment.h"
#include "../../dom/sharedpart.h"
#include "../../types/types.h"

namespace mu::engraving {
struct BarLine;
struct Bracket;
struct BracketItem;
struct Chord;
struct Dynamic;
struct Expression;
struct FiguredBass;
struct FretDiagram;
struct Harmony;
struct HarpPedalDiagram;
struct Image;
struct InstrumentChange;
struct Measure;
struct MeasureNumber;
struct MMRest;
struct MMRestRange;
struct Parenthesis;
struct RehearsalMark;
struct Score;
struct Segment;
struct SkylineLine;
struct Spanner;
struct StaffText;
struct Sticking;
struct System;
struct SystemText;
struct TempoText;
struct TimeSig;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct SystemLayout {
    static System *collect_system(LayoutContext &ctx);
    static void layout_system_elements(System *system, LayoutContext &ctx);

    static void layout_system(System *system, LayoutContext &ctx, double xo1);

    static void hide_empty_staves(System *system, LayoutContext &ctx, bool is_first_system);
    static bool can_change_sys_staff_visibility(const System *system, const staff_idx_t staff_idx);

    static void layout2(System *system, LayoutContext &ctx);
    static void restore_layout2(System *system, LayoutContext &ctx);
    static void set_measure_height(System *system, double height, const LayoutContext &ctx);

    static double min_distance(const System *top, const System *bottom, const LayoutContext &ctx);

    static void center_elements_between_staves(const System *system);
    /* Centers the systems which were laid out during this layout but whose page was not
     * collected (e.g. the first system of the next page, re-laid out by a partial layout),
     * so centerElementsBetweenStaves has not been called for them yet. Call it after the
     * system/page collection loop, before any leftover systems are deleted. */
    static void center_pending_systems(LayoutContext &ctx);
    static void center_big_time_sigs_across_staves(const System *system);

    static void update_skyline_for_element(EngravingItem *element, const System *system, double y_move);
    static void remove_element_from_skyline(EngravingItem *element, const System *system);

    static void layout_system_lock_indicators(System *system, LayoutContext &ctx);
    static void layout_page_lock_indicators(System *system);

private:
    struct MeasureState {
        Measure *measure = nullptr;
        double measure_width = 0.0;
        double measure_pos = 0.0;
        std::map<EngravingItem *, PointF> element_positions;
        std::map<EngravingItem *, double> element_widths;
        std::map<SharedPart *, SharedTrackMap> shared_track_maps;
        bool cur_header = false;
        bool cur_trailer = false;

        void clear();

        void restore_measure(LayoutContext &ctx);
    };

    struct ElementsToLayout {
        System *system;
        std::vector<Measure *> measures;
        std::vector<Segment *> segments;

        std::vector<ChordRest *> chord_rests;
        std::vector<Chord *> chords;
        std::vector<BarLine *> barlines;
        std::vector<TimeSig *> time_sig_above_staves;

        std::vector<MeasureNumber *> measure_numbers;
        std::vector<MMRestRange *> mmr_ranges;
        std::vector<EngravingItem *> markers_and_jumps;

        std::vector<Sticking *> stickings;
        std::vector<EngravingItem *> fermatas_and_tremolo_bars;
        std::vector<FiguredBass *> figured_bass;
        std::vector<Dynamic *> dynamics;
        std::vector<Expression *> expressions;
        std::vector<HarpPedalDiagram *> harp_diagrams;
        std::vector<FretDiagram *> fret_diagrams;
        std::vector<StaffTextBase *> staff_text;
        std::vector<InstrumentChange *> instr_changes;
        std::vector<SystemText *> system_text;
        std::vector<EngravingItem *> play_tech_capo_string_tun_triplet_feel;
        std::vector<RehearsalMark *> reh_marks;
        std::vector<TempoText *> tempo_text;
        std::vector<Image *> images;
        std::vector<Parenthesis *> parenthesis;
        std::vector<Harmony *> harmonies;
        std::vector<PlayCountText *> play_count_text;

        std::vector<Spanner *> slurs;
        std::vector<Spanner *> trills;
        std::vector<Spanner *> hairpins;
        std::vector<Spanner *> ottavas;
        std::vector<Spanner *> pedal;
        std::vector<Spanner *> voltas;
        std::vector<Spanner *> tempo_change_lines;
        std::vector<Spanner *> partial_lyrics_lines;
        std::vector<Spanner *> all_other_spanners;

        std::vector<GuitarBend *> guitar_bends;

        ElementsToLayout(System *s) : system(s) {}
    };

    static void collect_elements_to_layout(Measure *measure, ElementsToLayout &elements, const LayoutContext &ctx);
    static void collect_spanners_to_layout(ElementsToLayout &elements, const LayoutContext &ctx);

    static System *get_next_system(LayoutContext &lc);
    static void create_skylines(const ElementsToLayout &elements_to_layout, LayoutContext &ctx);
    static void process_lines(System *system, LayoutContext &ctx, const std::vector<Spanner *> &lines, bool align = false);
    static void layout_ties(Chord *ch, System *system, const Fraction &stick, LayoutContext &ctx);
    static void do_layout_ties(System *system, const std::vector<Segment *> &sl, const Fraction &stick, const Fraction &etick,
                               LayoutContext &ctx);
    static void do_layout_note_spanners_linear(System *system, LayoutContext &ctx);
    static void layout_note_anchored_spanners(System *system, Chord *chord);
    static void update_cross_beams(System *system, LayoutContext &ctx);
    static bool measure_has_cross_stuff_or_modified_beams(const Measure *measure);
    static void restore_old_system_layout(System *system, LayoutContext &ctx);
    static void layout_tuplets(const std::vector<ChordRest *> &chord_rests, LayoutContext &ctx);

    static void layout_ties_and_bends(const ElementsToLayout &elements_to_layout, LayoutContext &ctx);

    static double min_vert_space_for_cross_staff_beams(System *system, staff_idx_t staff_idx1, staff_idx_t staff_idx2, LayoutContext &ctx);

    struct CenterableItem {
        EngravingItem *item = nullptr;
        Shape shape; // x relative to the system, y relative to the upper staff of the item's gap
        bool is_lyrics = false;
        bool on_upper_gap_staff = false; // whether the item hangs below the upper staff of its gap or sits above the lower one
    };

    // The space between two vertically adjacent visible staves
    struct Gap {
        staff_idx_t upper_staff_idx = muse::nidx;
        staff_idx_t lower_staff_idx = muse::nidx;
        double y_staff_diff =
            0.0; // Inside a gap, everything is expressed relative to its upper staff, so we need the y diff between staves
        std::vector<CenterableItem> items;
    };

    static bool snap_chain_should_be_centered_between_staves(EngravingItem *item, const System *system);
    static bool element_should_be_centered_between_staves(const EngravingItem *item, const System *system, bool place_above);
    static bool mm_rest_should_be_centered_between_staves(const MMRest *mm_rest, const System *system);
    static bool whammy_bar_should_be_centered_between_staves(const WhammyBarSegment *wbar, const System *system);
    static bool element_has_another_stacked_outside(const EngravingItem *element, const Shape &element_shape,
                                                    const SkylineLine &skyline_line);
    static bool shapes_stack_vertically(const Shape &shape1, const Shape &shape2, double min_horizontal_clearance);
    static void collect_centerable_items(const System *system, std::vector<Gap> &gaps, std::vector<MMRest *> &mm_rests_to_center);
    static std::vector<std::vector<const CenterableItem *>> group_items_to_center_together(const std::vector<const CenterableItem *> &items,
                                                                                           double min_horizontal_clearance);
    static void center_items_in_gap(const Gap &gap, const System *system, double min_horizontal_clearance);
    static double gap_converge_distance(const std::vector<const CenterableItem *> &group, double y_staff_diff,
                                        double min_horizontal_clearance);
    static double convergence_move_for(const CenterableItem *centerable_item, double converge_distance);
    static void center_item_group(const std::vector<const CenterableItem *> &group, const System *system, const SkylineLine &upper_skyline,
                                  const SkylineLine &lower_skyline, double y_staff_diff, double min_horizontal_clearance);
    static void center_mm_rest_between_staves(MMRest *mm_rest, const System *system);

    static bool should_be_justified(System *system, double cur_sys_width, double target_system_width, LayoutContext &ctx);

    static void update_time_sig_above_staves_x_pos(System *system, LayoutContext &ctx);
    static void clear_big_time_sig_not_shown(System *system, LayoutContext &ctx);

    static void layout_sticking(const std::vector<Sticking *> stickings, System *system, LayoutContext &ctx);

    static void layout_lyrics(const ElementsToLayout &elements, LayoutContext &ctx);

    static void layout_voltas(const ElementsToLayout &elements_to_layout, LayoutContext &ctx);

    static void layout_dynamic_expression_and_hairpins(const ElementsToLayout &elements_to_layout, LayoutContext &ctx);

    static void layout_parenthesis_and_big_time_sigs(const ElementsToLayout &elements_to_layout);

    static void layout_harmonies(const std::vector<Harmony *> harmonies, System *system, LayoutContext &ctx);
    static void layout_fret_diagrams(const ElementsToLayout &elements, System *system, LayoutContext &ctx);

    static void align_rests(const ElementsToLayout &elements_to_layout, LayoutContext &ctx);
    static void check_full_measure_rest_collisions(const ElementsToLayout &elements_to_layout, LayoutContext &ctx);
};
} // namespace mu::engraving::rendering::score
