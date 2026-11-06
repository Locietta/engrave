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
#ifndef MU_ENGRAVING_SCOREHORIZONTALVIEWLAYOUT_DEV_H
#define MU_ENGRAVING_SCOREHORIZONTALVIEWLAYOUT_DEV_H

#include "layoutcontext.h"

namespace mu::engraving::rendering::score {
class ScoreHorizontalViewLayout {
public:

    static void layout_horizontal_view(Score *score, LayoutContext &ctx, const Fraction &stick, const Fraction &etick);

private:
    static void layout_linear(LayoutContext &ctx, bool layout_all);
    static void layout_linear(LayoutContext &ctx);
    static void reset_systems(LayoutContext &ctx, bool layout_all);
    static void collect_linear_system(LayoutContext &ctx);
    static void layout_system_lock_indicators(System *system);

    //! puts segments on the positions according to their length
    static void layout_segments_with_duration(Measure *m, const std::vector<int> &visible_parts);

    /*! \brief callulate width of segment and additional spacing of segment depends on duration of segment
     *  \return pair of {spacing, width}
     */
    static std::pair<double, double> compute_cell_width(const Segment *s, const std::vector<int> &visible_parts);

    /*! \brief get among all ChordRests of segment the ChordRest with minimum ticks,
    * take into account visibleParts
    */
    static ChordRest *chord_rest_with_min_duration(const Segment *seg, const std::vector<int> &visible_parts);

    static Fraction calculate_quantum_cell(const Measure *m, const std::vector<int> &visible_parts);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_SCOREHORIZONTALVIEWLAYOUT_DEV_H
