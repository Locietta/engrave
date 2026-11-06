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

#include "dom/beambase.h"
#include "layoutcontext.h"
#include "types/types.h"

namespace mu::engraving {
struct Chord;
struct ChordRest;
struct Beam;
struct TremoloTwoChord;
enum class ActionIconType : signed char;
enum class SpannerSegmentType : unsigned char;
enum class ChordBeamAnchorType : unsigned char;
} // namespace mu::engraving

enum class SlopeConstraint : char {
    NO_CONSTRAINT,
    FLAT,
    SMALL_SLOPE,
};

namespace mu::engraving::rendering::score {
struct BeamTremoloLayout {

    static void setup_l_data(const BeamBase *item, BeamBase::LayoutData *ldata, const LayoutContext &ctx);

    static bool calculate_anchors(const BeamBase *item, BeamBase::LayoutData *ldata, const LayoutContext &ctx,
                                  const std::vector<ChordRest *> &chord_rests, const std::vector<BeamBase::NotePosition> &note_positions);

    static double chord_beam_anchor_x(const BeamBase::LayoutData *ldata, const ChordRest *chord, ChordBeamAnchorType anchor_type);
    static double chord_beam_anchor_y(const BeamBase::LayoutData *ldata, const ChordRest *chord);
    static PointF chord_beam_anchor(const BeamBase::LayoutData *ldata, const ChordRest *chord, ChordBeamAnchorType anchor_type);
    static int get_max_slope(const BeamBase::LayoutData *ldata);
    static void extend_stem(const BeamBase::LayoutData *ldata, Chord *chord, double addition);
    static int min_stem_length(const ChordRest *cr, const BeamBase::LayoutData *ldata);
    static int stroke_count(const BeamBase::LayoutData *ldata, const ChordRest *cr);

private:

    static int get_target_staff_line(const BeamBase::LayoutData *ldata, const LayoutContext &ctx, const ChordRest *start_chord,
                                     const ChordRest *end_chord, const int staff_lines, const staff_idx_t beam_staff_idx,
                                     const staff_idx_t actual_beam_staff_idx);
    static int compute_desired_slant(const BeamBase *item, const BeamBase::LayoutData *ldata, const BeamBase::NotePosition &start_pos,
                                     const BeamBase::NotePosition &end_pos, std::vector<Chord *> closest_chords_to_beam, int target_line,
                                     int dictator, int pointer);
    static SlopeConstraint get_slope_constraint(const BeamBase::LayoutData *ldata, const BeamBase::NotePosition &start_pos,
                                                const BeamBase::NotePosition &end_pos);
    static void offset_beam_with_anchor_shortening(const BeamBase::LayoutData *ldata, const std::vector<ChordRest *> &chord_rests,
                                                   int &dictator, int &pointer, int staff_lines, bool is_start_dictator,
                                                   int stem_length_dictator, const int target_line);
    static bool is_valid_beam_position(const bool is_up, int y_pos, bool is_start, bool is_ascending, bool is_flat, int staff_lines,
                                       bool is_outer);
    static bool is_beam_inside_staff(int y_pos, int staff_lines, bool allow_floater);
    static void set_small_inner_beam_pos(const BeamBase::LayoutData *ldata, int &dictator, int &pointer, const int staff_lines,
                                         const bool is_flat, const bool is_small, const LayoutContext &ctx);
    static int get_outer_beam_pos_offset(const BeamBase::LayoutData *ldata, int inner_beam, int beam_count, int staff_lines);
    static void offset_beam_to_remove_collisions(const BeamBase *item, const BeamBase::LayoutData *ldata,
                                                 const std::vector<ChordRest *> &chord_rests, int &dictator, int &pointer,
                                                 const double start_x, const double end_x, bool is_flat, bool is_start_dictator);
    static int get_beam_count(const BeamBase::LayoutData *ldata, const std::vector<const ChordRest *> &chord_rests);
    static bool is64th_beam_position_exception(const int beam_spacing, int &y_pos, int staff_lines);
    static int find_valid_beam_offset(const BeamBase::LayoutData *ldata, int outer, int beam_count, int staff_lines, bool is_start,
                                      bool is_ascending, bool is_flat);
    static void set_valid_beam_positions(const BeamBase::LayoutData *ldata, int &dictator, int &pointer, int beam_count_d, int beam_count_p,
                                         int staff_lines, bool is_start_dictator, bool is_flat, bool is_ascending);
    static void add_middle_line_slant(const BeamBase::LayoutData *ldata, int &dictator, int &pointer, int beam_count, int target_line,
                                      int interval, int desired_slant);
    static void add8th_space_slant(BeamBase::LayoutData *ldata, PointF &dictator_anchor, int dictator, int pointer, int beam_count,
                                   int interval, int target_line, bool flat);
    static bool no_slope(const Beam *beam);

    static bool calculate_anchors_cross(const BeamBase *item, BeamBase::LayoutData *ldata, const LayoutConfiguration &conf);
    static bool compute_tremolo_up(const BeamBase::LayoutData *ldata);
};
} // namespace mu::engraving::rendering::score
