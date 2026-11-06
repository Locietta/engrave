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
class LaissezVib;
class LaissezVibSegment;
class Slur;
class SlurSegment;
struct SlurTiePos;
class SlurTieSegment;
class SpannerSegment;
class System;
class Chord;
class TieSegment;
class Tie;
class TremoloTwoChord;
enum class Grip : signed char;
class Note;
class PartialTie;
class PartialTieSegment;
class TappingHalfSlur;
class TappingHalfSlurSegment;
} // namespace mu::engraving

namespace muse::draw {
class Transform;
} // namespace muse::draw

namespace mu::engraving::rendering::score {
class SlurTieLayout {
public:
    static SpannerSegment *layout_system(Slur *item, System *system, LayoutContext &ctx);

    static TieSegment *layout_tie_for(Tie *item, System *system);
    static TieSegment *layout_tie_back(Tie *item, System *system, LayoutContext &ctx);
    static void resolve_vertical_tie_collisions(const std::vector<TieSegment *> &stacked_ties);

    static void compute_up(Slur *slur, LayoutContext &ctx);
    static void calculate_direction(Tie *item);

    static void compute_bezier(TieSegment *tie_seg, PointF shoulder_offset = PointF());
    static void compute_bezier(SlurSegment *slur_seg, PointF shoulder_offset = PointF());
    static double note_optical_center_for_tie(const Note *note, bool up);
    static void create_slur_segments(Slur *item, LayoutContext &ctx);

    static void adjust_overlapping_slurs(const std::list<SpannerSegment *> &spanner_segments);

    static void layout_laissez_vib_chord(Chord *chord, LayoutContext &ctx);
private:

    static void slur_pos(Slur *item, SlurTiePos *sp, LayoutContext &ctx);
    static void adjust_for_tapping_half_slurs(TappingHalfSlur *item, SlurTiePos *sp, Note *end_note);
    static void avoid_pre_bends_on_tab(const Chord *sc, const Chord *ec, SlurTiePos *sp);
    static void fix_articulations(Slur *item, PointF &pt, Chord *c, double up, bool stem_side);
    static void adjust_end_points(SlurSegment *slur_seg);
    static void adjust_slur_floating_end_point_angles(SlurSegment *slur_seg, PointF &p1, PointF &p2, bool incoming_partial,
                                                      bool outgoing_partial);

    static void layout_segment(SlurSegment *item, const PointF &p1, const PointF &p2);
    static void avoid_collisions(SlurSegment *slur_seg, PointF &pp1, PointF &p2, PointF &p3, PointF &p4,
                                 muse::draw::Transform &to_system_coordinates, double &slur_angle);
    static Shape get_segment_shapes(SlurSegment *slur_seg, ChordRest *start_cr, ChordRest *end_cr);
    static Shape get_segment_shape(SlurSegment *slur_seg, Segment *seg, ChordRest *start_cr, ChordRest *end_cr);
    static void add_min_clearance_to_shapes(Shape &seg_shapes, double spatium, bool slur_up, const ChordRest *start_cr,
                                            const ChordRest *end_cr);
    static double compute_arc_clearance(double spatium, double slur_length, double slur_angle);
    static void compute_adjustment_balance(SlurSegment *slur_seg, const ChordRest *start_cr, const ChordRest *end_cr, double &left_balance,
                                           double &right_balance);
    static bool has_articulation_above(SlurSegment *slur_seg, const ChordRest *chord_rest);
    static double compute_adjustment_step(int up_sign, double spatium, double slur_length);
    static bool stem_side_for_beam(Slur *slur, bool start);

    static bool stem_side_start_for_beam(Slur *slur) { return stem_side_for_beam(slur, true); }

    static bool stem_side_end_for_beam(Slur *slur) { return stem_side_for_beam(slur, false); }

    static bool is_over_beams(Slur *slur);
    static double compute_shoulder_height(SlurSegment *slur_seg, double slur_length_in_sp, PointF shoulder_offset);

    static double ending_x_for_open_ended_slur_tie(const Measure *measure, staff_idx_t staff_idx);

    static bool tie_segment_should_be_skipped(const Tie *item);
    static void compute_start_and_end_system(Tie *item, SlurTiePos &slur_tie_pos);
    static PointF compute_default_start_or_end_point(const Tie *tie, Grip start_or_end);
    static void correct_for_cross_staff(Tie *tie, SlurTiePos &s_pos, SpannerSegmentType type);
    static void force_horizontal(Tie *tie, SlurTiePos &s_pos);
    static void adjust_x(TieSegment *tie_segment, SlurTiePos &s_pos, Grip start_or_end);
    static void adjust_xfor_ledger_lines(TieSegment *tie_segment, bool start, Chord *chord, Note *note, const PointF &chord_system_pos,
                                         double padding, double &resulting_x);
    static void adjust_yfor_ledger_lines(TieSegment *tie_segment, SlurTiePos &s_pos);
    static void adjust_y(TieSegment *tie_segment);
    static bool has_end_point_above_note(TieSegment *tie_segment);

    static double default_stem_length_start(TremoloTwoChord *tremolo);
    static double default_stem_length_end(TremoloTwoChord *tremolo);

    static bool is_direction_mixture(const Chord *c1, const Chord *c2, LayoutContext &ctx);

    static void compute_mid_thickness(SlurTieSegment *slur_tie_seg, double slur_tie_length_in_sp);
    static void fill_shape(SlurTieSegment *slur_tie_seg, double slur_tie_length_in_sp);
    static bool should_hide_slur_segment(SlurSegment *item);

    static void add_line_attach_points(TieSegment *segment);
    static void add_line_attach_points(PartialTieSegment *segment);

    static void calculate_is_inside(Tie *item);

    static LaissezVibSegment *create_laissez_vib_segment(LaissezVib *item);
    static void calculate_laissez_vib_x(LaissezVibSegment *segment, SlurTiePos &s_pos, bool smufl);
    static void calculate_laissez_vib_y(LaissezVibSegment *segment, SlurTiePos &s_pos);

    static PartialTieSegment *create_partial_tie_segment(PartialTie *item);
    static PartialTieSegment *layout_partial_tie(PartialTie *item);

    static void set_partial_tie_end_pos(PartialTie *item, SlurTiePos &s_pos);
};
} // namespace mu::engraving::rendering::score
