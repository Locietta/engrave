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

#include "jump.h"

#include "types/typesconv.h"

#include "measure.h"
#include "score.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   jumpStyle
//---------------------------------------------------------

static const ElementStyle jump_style{
    {Sid::REPEAT_RIGHT_PLACEMENT, Pid::PLACEMENT},
    {Sid::REPEAT_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   JumpTypeTable
//---------------------------------------------------------

const std::vector<JumpTypeTableItem> jump_type_table{
    {JumpType::DC, "D.C.", "start", "end", ""},
    {JumpType::DC_AL_FINE, "D.C. al Fine", "start", "fine", ""},
    {JumpType::DC_AL_CODA, "D.C. al Coda", "start", "coda", "codab"},
    {JumpType::DS_AL_CODA, "D.S. al Coda", "segno", "coda", "codab"},
    {JumpType::DS_AL_FINE, "D.S. al Fine", "segno", "fine", ""},
    {JumpType::DS, "D.S.", "segno", "end", ""},

    {JumpType::DC_AL_DBLCODA, "D.C. al Doppia Coda", "start", "varcoda", "codab"},
    {JumpType::DS_AL_DBLCODA, "D.S. al Doppia Coda", "segno", "varcoda", "codab"},
    {JumpType::DSS, "Dal Doppio Segno", "varsegno", "end", ""},
    {JumpType::DSS_AL_CODA, "D.D.S. al Coda", "varsegno", "coda", "codab"},
    {JumpType::DSS_AL_DBLCODA, "D.D.S. al Doppia Coda", "varsegno", "varcoda", "codab"},
    {JumpType::DSS_AL_FINE, "D.D.S. al Fine", "varsegno", "fine", ""},
};

//---------------------------------------------------------
//   Jump
//---------------------------------------------------------

Jump::Jump(DummyParentOr<Measure> parent)
    : TextBase(ElementType::JUMP, parent, TextStyleType::REPEAT_RIGHT, ElementFlag::MOVABLE | ElementFlag::SYSTEM | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::jump_style);
    play_repeats_ = false;
}

//---------------------------------------------------------
//   setJumpType
//---------------------------------------------------------

void Jump::set_jump_type(JumpType t) {
    for (const JumpTypeTableItem &p : ::mu::engraving::jump_type_table) {
        if (p.type == t) {
            set_xml_text(String::from_ascii(p.text.ascii()));
            set_jump_to(String::from_ascii(p.jump_to.ascii()));
            set_play_until(String::from_ascii(p.play_until.ascii()));
            set_continue_at(String::from_ascii(p.continue_at.ascii()));
            init_text_style_type(TextStyleType::REPEAT_RIGHT);
            break;
        }
    }
}

//---------------------------------------------------------
//   jumpType
//---------------------------------------------------------

JumpType Jump::jump_type() const {
    for (const JumpTypeTableItem &t : ::mu::engraving::jump_type_table) {
        if (jump_to_ == t.jump_to && play_until_ == t.play_until && continue_at_ == t.continue_at) {
            return t.type;
        }
    }
    return JumpType::USER;
}

String Jump::jump_type_user_name() const { return TConv::translated_user_name(jump_type()); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Jump::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::JUMP_TO: return jump_to();
        case Pid::PLAY_UNTIL: return play_until();
        case Pid::CONTINUE_AT: return continue_at();
        case Pid::PLAY_REPEATS: return play_repeats();
        default: break;
    }
    return TextBase::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Jump::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::JUMP_TO: set_jump_to(v.value<String>()); break;
        case Pid::PLAY_UNTIL: set_play_until(v.value<String>()); break;
        case Pid::CONTINUE_AT: set_continue_at(v.value<String>()); break;
        case Pid::PLAY_REPEATS: set_play_repeats(v.to_bool()); break;
        default:
            if (!TextBase::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    score()->invalidate_repeat_list();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Jump::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::JUMP_TO:
        case Pid::PLAY_UNTIL:
        case Pid::CONTINUE_AT: return String();
        case Pid::PLAY_REPEATS: return false;
        case Pid::PLACEMENT: return PlacementV::ABOVE;
        default: break;
    }
    return TextBase::property_default(property_id);
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Jump::next_segment_element() {
    Segment *seg = measure()->last();
    return seg->first_element_for_navigation(staff_idx());
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Jump::prev_segment_element() { return next_segment_element(); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Jump::subtype_user_name() const { return TConv::user_name(jump_type()); }
} // namespace mu::engraving
