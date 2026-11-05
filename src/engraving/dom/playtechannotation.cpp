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

#include "segment.h"

#include "types/typesconv.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

static const ElementStyle annotationStyle{
    {Sid::staffTextPlacement, Pid::PLACEMENT},
    {Sid::staffTextMinDistance, Pid::MIN_DISTANCE},
};

PlayTechAnnotation::PlayTechAnnotation(DummyParentOr<Segment> parent, PlayingTechniqueType techniqueType, TextStyleType tid)
    : StaffTextBase(ElementType::PLAYTECH_ANNOTATION, parent, tid, ElementFlag::MOVABLE | ElementFlag::ON_STAFF),
      techniqueType_(techniqueType) {
    initElementStyle(&annotationStyle);
}

PlayingTechniqueType PlayTechAnnotation::techniqueType() const { return techniqueType_; }

void PlayTechAnnotation::setTechniqueType(const PlayingTechniqueType techniqueType) {
    techniqueType_ = techniqueType;
    resetProperty(Pid::TEXT_STYLE);
}

PlayTechAnnotation *PlayTechAnnotation::clone() const { return new PlayTechAnnotation(*this); }

TranslatableString PlayTechAnnotation::subtypeUserName() const { return TConv::userName(techniqueType_); }

bool PlayTechAnnotation::isHandbellsSymbol() const {
    return static_cast<int>(techniqueType_) >= static_cast<int>(PlayingTechniqueType::HandbellsSwing) &&
           static_cast<int>(techniqueType_) <= static_cast<int>(PlayingTechniqueType::HandbellsR);
}

PropertyValue PlayTechAnnotation::getProperty(Pid id) const {
    switch (id) {
        case Pid::PLAY_TECH_TYPE: return techniqueType_;
        case Pid::PLAY: return playPlayTechAnnotation_;
        default: return StaffTextBase::getProperty(id);
    }
}

bool PlayTechAnnotation::setProperty(Pid propertyId, const PropertyValue &val) {
    switch (propertyId) {
        case Pid::PLAY_TECH_TYPE: setTechniqueType(PlayingTechniqueType(val.toInt())); break;
        case Pid::PLAY: setPlayPlayTechAnnotation(val.toBool()); break;
        default:
            if (!StaffTextBase::setProperty(propertyId, val)) {
                return false;
            }
            break;
    }

    triggerLayout();
    return true;
}

PropertyValue PlayTechAnnotation::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return isHandbellsSymbol() ? TextStyleType::ARTICULATION : TextStyleType::STAFF;
        case Pid::PLAY_TECH_TYPE: return PlayingTechniqueType::Natural;
        case Pid::PLAY: return true;
        default: return StaffTextBase::propertyDefault(id);
    }
}
