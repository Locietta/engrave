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

#include "breath.h"

#include "log.h"
#include "measure.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "types/symnames.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
const std::vector<BreathType> Breath::breath_list{
    {SymId::BREATH_MARK_COMMA, false, 0.0},    {SymId::BREATH_MARK_TICK, false, 0.0}, {SymId::BREATH_MARK_SALZEDO, false, 0.0},
    {SymId::BREATH_MARK_UPBOW, false, 0.0},    {SymId::CAESURA_CURVED, true, 2.0},    {SymId::CAESURA, true, 2.0},
    {SymId::CAESURA_SHORT, true, 2.0},         {SymId::CAESURA_THICK, true, 2.0},     {SymId::CHANT_CAESURA, true, 2.0},
    {SymId::CAESURA_SINGLE_STROKE, true, 2.0},
};

//---------------------------------------------------------
//   Breath
//---------------------------------------------------------

Breath::Breath(DummyParentOr<Segment> parent) : EngravingItem(ElementType::BREATH, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    sym_id_ = SymId::BREATH_MARK_COMMA;
    pause_ = 0.0;
}

//---------------------------------------------------------
//   isCaesura
//---------------------------------------------------------

bool Breath::is_caesura() const {
    for (const BreathType &bt : ::mu::engraving::Breath::breath_list) {
        if (bt.id == sym_id_) {
            return bt.is_caesura;
        }
    }
    return false;
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Breath::mag() const { return staff() ? staff()->staff_mag(tick()) : 1.0; }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Breath::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SYMBOL: return PropertyValue::from_value(sym_id_);
        case Pid::PAUSE: return pause_;
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Breath::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SYMBOL: set_sym_id(v.value<SymId>()); break;
        case Pid::PAUSE: set_pause(v.to_double()); break;
        default:
            if (!EngravingItem::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    set_generated(false);
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Breath::property_default(Pid id) const {
    switch (id) {
        case Pid::PAUSE: return 0.0;
        case Pid::PLACEMENT: return track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE;
        default: return EngravingItem::property_default(id);
    }
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Breath::next_segment_element() { return segment()->first_in_next_segments(staff_idx()); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Breath::prev_segment_element() { return segment()->last_in_prev_segments(staff_idx()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Breath::subtype_user_name() const { return SymNames::user_name_for_sym_id(sym_id()); }
} // namespace mu::engraving
