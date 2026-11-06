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

#include "sticking.h"

#include "segment.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   stickingStyle
//---------------------------------------------------------

static const ElementStyle sticking_style{
    {Sid::STICKING_PLACEMENT, Pid::PLACEMENT},
    {Sid::STICKING_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   Sticking
//---------------------------------------------------------

Sticking::Sticking(DummyParentOr<Segment> parent)
    : TextBase(ElementType::STICKING, parent, TextStyleType::STICKING, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::sticking_style);
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

engraving::PropertyValue Sticking::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::STICKING;
        default: return TextBase::property_default(id);
    }
}
} // namespace mu::engraving
