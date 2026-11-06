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

#include "engraving/dom/measurenumber.h"
#include "engraving/dom/mmrestrange.h"

namespace mu::engraving::rendering::score {
class MeasureNumberLayout {
public:
    static void layout_measure_number(MeasureNumber *item, MeasureNumber::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_mm_rest_range(MMRestRange *item, MMRestRange::LayoutData *ldata, const LayoutContext &ctx);

private:
    static void layout_measure_number_base(MeasureNumberBase *item, MeasureNumberBase::LayoutData *ldata);

    static const Segment *ref_barline_segment(const MeasureNumber *item, bool align_to_barline, AlignH h_placement);
    static void check_barline_collisions(const MeasureNumber *item, const Segment *barline_seg, AlignH h_placement,
                                         MeasureNumber::LayoutData *ldata);
};
} // namespace mu::engraving::rendering::score
