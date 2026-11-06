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

#include "playtechannotation.h"

#include "log.h"
#include "segment.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;

static const ElementStyle annotation_style{
    {Sid::STAFF_TEXT_PLACEMENT, Pid::PLACEMENT},
    {Sid::STAFF_TEXT_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

PlayTechAnnotation::PlayTechAnnotation(DummyParentOr<Segment> parent, PlayingTechniqueType technique_type, TextStyleType tid)
    : StaffTextBase(ElementType::PLAYTECH_ANNOTATION, parent, tid, ElementFlag::MOVABLE | ElementFlag::ON_STAFF),
      technique_type_(technique_type) {
    init_element_style(&::annotation_style);
}

PlayingTechniqueType PlayTechAnnotation::technique_type() const { return technique_type_; }

void PlayTechAnnotation::set_technique_type(const PlayingTechniqueType technique_type) {
    technique_type_ = technique_type;
    reset_property(Pid::TEXT_STYLE);
}

PlayTechAnnotation *PlayTechAnnotation::clone() const { return new PlayTechAnnotation(*this); }

TranslatableString PlayTechAnnotation::subtype_user_name() const { return TConv::user_name(technique_type_); }

bool PlayTechAnnotation::is_handbells_symbol() const {
    return static_cast<int>(technique_type_) >= static_cast<int>(PlayingTechniqueType::HANDBELLS_SWING) &&
           static_cast<int>(technique_type_) <= static_cast<int>(PlayingTechniqueType::HANDBELLS_R);
}

PropertyValue PlayTechAnnotation::get_property(Pid id) const {
    switch (id) {
        case Pid::PLAY_TECH_TYPE: return technique_type_;
        case Pid::PLAY: return play_play_tech_annotation_;
        default: return StaffTextBase::get_property(id);
    }
}

bool PlayTechAnnotation::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::PLAY_TECH_TYPE: set_technique_type(PlayingTechniqueType(val.to_int())); break;
        case Pid::PLAY: set_play_play_tech_annotation(val.to_bool()); break;
        default:
            if (!StaffTextBase::set_property(property_id, val)) {
                return false;
            }
            break;
    }

    trigger_layout();
    return true;
}

PropertyValue PlayTechAnnotation::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return is_handbells_symbol() ? TextStyleType::ARTICULATION : TextStyleType::STAFF;
        case Pid::PLAY_TECH_TYPE: return PlayingTechniqueType::NATURAL;
        case Pid::PLAY: return true;
        default: return StaffTextBase::property_default(id);
    }
}
