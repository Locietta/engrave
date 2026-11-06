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

#include "fingering.h"

#include "beam.h"
#include "chord.h"
#include "measure.h"
#include "note.h"
#include "part.h"
#include "skyline.h"
#include "staff.h"
#include "stem.h"
#include "system.h"
#include "translation.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   fingeringStyle
//---------------------------------------------------------

static const ElementStyle fingering_style{
    {Sid::FINGERING_PLACEMENT, Pid::PLACEMENT},
    {Sid::FINGERING_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   Fingering
//      EngravingItem(Score* = 0, ElementFlags = ElementFlag::NOTHING);
//---------------------------------------------------------

Fingering::Fingering(DummyParentOr<Note> parent, TextStyleType tid, ElementFlags ef) : TextBase(ElementType::FINGERING, parent, tid, ef) {
    set_placement(PlacementV::ABOVE);
    init_element_style(&::mu::engraving::fingering_style);
}

Fingering::Fingering(DummyParentOr<Note> parent, ElementFlags ef) : Fingering(parent, TextStyleType::FINGERING, ef) {}

//---------------------------------------------------------
//   layoutType
//---------------------------------------------------------

ElementType Fingering::layout_type() const {
    switch (text_style_type()) {
        case TextStyleType::FINGERING:
        case TextStyleType::RH_GUITAR_FINGERING:
        case TextStyleType::STRING_NUMBER: return ElementType::CHORD;
        default: return ElementType::NOTE;
    }
}

//---------------------------------------------------------
//   calculatePlacement
//---------------------------------------------------------

PlacementV Fingering::calculate_placement() const {
    Note *n = note();
    if (!n) {
        return PlacementV::ABOVE;
    }
    Chord *chord = n->chord();
    Staff *staff = chord->staff();
    Part *part = staff->part();
    size_t nstaves = part->nstaves();
    bool voices = chord->measure()->has_voices(staff->idx(), chord->tick(), chord->actual_ticks());
    bool below = voices ? !chord->up() : (nstaves > 1) && (staff->rstaff() == nstaves - 1);
    return below ? PlacementV::BELOW : PlacementV::ABOVE;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

bool Fingering::is_on_cross_beam_side() const {
    Chord *chord = note() ? note()->chord() : nullptr;
    if (!chord) {
        return false;
    }
    return layout_type() == ElementType::CHORD && chord->beam() && (chord->beam()->cross() || chord->staff_move() != 0) &&
           place_above() == chord->up();
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

engraving::PropertyValue Fingering::property_default(Pid id) const {
    switch (id) {
        case Pid::PLACEMENT: return calculate_placement();
        case Pid::TEXT_STYLE: return TextStyleType::FINGERING;
        default: return TextBase::property_default(id);
    }
}
} // namespace mu::engraving
