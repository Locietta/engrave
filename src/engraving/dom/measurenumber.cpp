/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "measurenumber.h"

#include "measure.h"
#include "score.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   measureNumberStyle
//---------------------------------------------------------

static const ElementStyle measure_number_style{{Sid::MEASURE_NUMBER_V_PLACEMENT, Pid::PLACEMENT},
                                               {Sid::MEASURE_NUMBER_MIN_DISTANCE, Pid::MIN_DISTANCE},
                                               {Sid::MEASURE_NUMBER_TEXT_STYLE, Pid::TEXT_STYLE}};

//---------------------------------------------------------
//   MeasureNumber
//---------------------------------------------------------

MeasureNumber::MeasureNumber(DummyParentOr<Measure> parent, TextStyleType tid)
    : MeasureNumberBase(ElementType::MEASURE_NUMBER, parent, tid) {
    init_element_style(&::mu::engraving::measure_number_style);
}

//---------------------------------------------------------
//   MeasureNumber
//     Copy constructor
//---------------------------------------------------------

MeasureNumber::MeasureNumber(const MeasureNumber &other) : MeasureNumberBase(other) {
    init_element_style(&::mu::engraving::measure_number_style);
}

bool MeasureNumber::is_system_object_below_bottom_staff() const {
    return style().style_v(Sid::MEASURE_NUMBER_PLACEMENT_MODE).value<MeasureNumberPlacement>() == MeasureNumberPlacement::BELOW_SYSTEM ||
           EngravingItem::is_system_object_below_bottom_staff();
}
} // namespace mu::engraving
