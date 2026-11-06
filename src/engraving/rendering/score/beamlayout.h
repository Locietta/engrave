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
#ifndef MU_ENGRAVING_BEAMLAYOUT_DEV_H
#define MU_ENGRAVING_BEAMLAYOUT_DEV_H

#include <vector>

#include "../../types/types.h"
#include "../../dom/beam.h"

#include "layoutcontext.h"

namespace mu::engraving {
class Beam;
class Chord;
class Rest;
class ChordRest;
class Measure;
class Score;
class Segment;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class BeamLayout {
public:

    static void layout(Beam *item, const LayoutContext &ctx);
    static void layout_if_need(Beam *item, const LayoutContext &ctx);
    static void layout1(Beam *item, LayoutContext &ctx);

    static bool is_start_of_non_cross_beam(ChordRest *cr);
    static bool is_start_of_cross_beam(ChordRest *cr);
    static void create_beams(LayoutContext &ctx, Measure *measure);
    static void restore_beams(Measure *m, LayoutContext &ctx);
    static bool measure_may_have_beams_joined_into_next(const Measure *measure);
    static void break_cross_measure_beams(Measure *measure, LayoutContext &ctx);
    static void layout_non_cross_beams(ChordRest *cr, LayoutContext &ctx);
    static void vertical_adjust_beamed_rests(Rest *rest, Beam *beam, LayoutContext &ctx);
    static void check_cross_pos_and_stem_consistency(Beam *beam, LayoutContext &ctx);

    static PointF chord_beam_anchor(const Beam *item, const ChordRest *chord, ChordBeamAnchorType anchor_type);
    static double chord_beam_anchor_y(const Beam *item, const ChordRest *chord);
    static void set_trem_anchors(Beam *item, const LayoutContext &ctx);

private:
    static void beam_grace_notes(LayoutContext &ctx, Chord *main_note, bool after);

    static void layout2(Beam *item, const LayoutContext &ctx, const std::vector<ChordRest *> &chord_rests, SpannerSegmentType, int frag);

    static void create_beam_segments(Beam *item, const LayoutContext &ctx, const std::vector<ChordRest *> &chord_rests);
    static bool calc_is_beamlet_before(const Beam *item, Chord *chord, int i, int level, bool is_after32_break, bool is_after64_break);
    static void create_beam_segment(Beam *item, ChordRest *start_chord, ChordRest *end_chord, int level, bool short_stems);
    static void create_beamlet_segment(Beam *item, const LayoutContext &ctx, ChordRest *chord, bool is_before, int level);

    static bool compute_up_for_moved_cross(Beam *item);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_BEAMLAYOUT_DEV_H
