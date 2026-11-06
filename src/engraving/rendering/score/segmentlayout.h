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
#ifndef MU_ENGRAVING_SEGMENTLAYOUT_DEV_H
#define MU_ENGRAVING_SEGMENTLAYOUT_DEV_H

#include "dom/segment.h"
#include "layoutcontext.h"

namespace mu::engraving::rendering::score {
struct SegmentLayout {

    static void layout_measure_independent_elements(const Segment &segment, track_idx_t track, const LayoutContext &ctx);
    static void set_chord_mag(const Staff *staff, const Segment &segment, track_idx_t start_track, track_idx_t end_track,
                              const LayoutConfiguration &conf);
    static void check_staff_move_validity(const Segment &segment, track_idx_t start_track, track_idx_t end_track);
    static void layout_chord_drumset(const Staff *staff, const Segment &segment, track_idx_t start_track, track_idx_t end_track,
                                     const LayoutConfiguration &conf);

    static void compute_chords_up(const Segment &segment, track_idx_t start_track, track_idx_t end_track, const LayoutContext &ctx);

    static void layout_chords_stem(const Segment &segment, track_idx_t start_track, track_idx_t end_track, const LayoutContext &ctx);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_SEGMENTLAYOUT_DEV_H
