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

#include "mmrestrange.h"

#include "log.h"
#include "measure.h"
#include "style/style.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   mmRestRangeStyle
//---------------------------------------------------------

static const ElementStyle mm_rest_range_style{{Sid::MM_REST_RANGE_BRACKET_TYPE, Pid::MMREST_RANGE_BRACKET_TYPE},
                                              {Sid::MM_REST_RANGE_V_PLACEMENT, Pid::PLACEMENT},
                                              {Sid::MM_REST_RANGE_H_PLACEMENT, Pid::HPLACEMENT},
                                              {Sid::MM_REST_RANGE_MIN_DISTANCE, Pid::MIN_DISTANCE},
                                              {Sid::MM_REST_RANGE_TEXT_STYLE, Pid::TEXT_STYLE}};

MMRestRange::MMRestRange(DummyParentOr<Measure> parent)
    : MeasureNumberBase(ElementType::MMREST_RANGE, parent, TextStyleType::MMREST_RANGE) {
    init_element_style(&::mu::engraving::mm_rest_range_style);
}

//---------------------------------------------------------
//   MMRestRange
///   Copy constructor
//---------------------------------------------------------

MMRestRange::MMRestRange(const MMRestRange &other) : MeasureNumberBase(other) { init_element_style(&::mu::engraving::mm_rest_range_style); }

PropertyValue MMRestRange::get_property(Pid id) const {
    switch (id) {
        case Pid::MMREST_RANGE_BRACKET_TYPE: return int(bracket_type());
        default: return MeasureNumberBase::get_property(id);
    }
}

bool MMRestRange::set_property(Pid id, const PropertyValue &val) {
    switch (id) {
        case Pid::MMREST_RANGE_BRACKET_TYPE:
            set_bracket_type(MMRestRangeBracketType(val.to_int()));
            mutldata()->layout_invalid = true;
            trigger_layout();
            return true;
        default: return MeasureNumberBase::set_property(id, val);
    }
}

//---------------------------------------------------------
//   setXmlText
///   This is reimplemented from TextBase::setXmlText to take care of the brackets
//---------------------------------------------------------

void MMRestRange::set_xml_text(const String &s) {
    switch (bracket_type()) {
        case MMRestRangeBracketType::BRACKETS: TextBase::set_xml_text(u"[" + s + u"]"); break;
        case MMRestRangeBracketType::PARENTHESES: TextBase::set_xml_text(u"(" + s + u")"); break;
        case MMRestRangeBracketType::NONE:
        default: TextBase::set_xml_text(s); break;
    }
}
} // namespace mu::engraving
