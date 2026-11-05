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

#include "log.h"
#include "stafftext.h"

#include "soundflag.h"
#include "segment.h"
#include "score.h"
#include "../editing/addremoveelement.h"

using namespace mu::engraving;

static const ElementStyle STAFF_STYLE{
    {Sid::staffTextPlacement, Pid::PLACEMENT},
    {Sid::staffTextMinDistance, Pid::MIN_DISTANCE},
};

StaffText::StaffText(DummyParentOr<Segment> parent, TextStyleType tid)
    : StaffTextBase(ElementType::STAFF_TEXT, parent, tid, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    initElementStyle(&STAFF_STYLE);
    resetProperty(Pid::MUSIC_SYMBOL_SIZE);
}

StaffText::StaffText(const StaffText &t) : StaffTextBase(t) {
    if (t.soundFlag_) {
        setSoundFlag(t.soundFlag_->clone());
    }
}

EngravingItem *StaffText::linkedClone() {
    StaffText *clone = static_cast<StaffText *>(StaffTextBase::linkedClone());

    if (clone->soundFlag_ && soundFlag_) {
        score()->undo(new Link(clone->soundFlag_, soundFlag_));
    }

    return clone;
}

PropertyValue StaffText::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::STAFF;
        default: return StaffTextBase::propertyDefault(id);
    }
}

void StaffText::scanElements(std::function<void(EngravingItem *)> func) {
    if (soundFlag_) {
        soundFlag_->scanElements(func);
    }

    StaffTextBase::scanElements(func);
}

void StaffText::add(EngravingItem *e) {
    e->setOwnershipParent(this);
    e->setTrack(track());

    switch (e->type()) {
        case ElementType::SOUND_FLAG:
            setSoundFlag(toSoundFlag(e));
            e->added();
            return;
        default: break;
    }

    StaffTextBase::add(e);
}

void StaffText::remove(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::SOUND_FLAG: {
            if (soundFlag() == e) {
                setSoundFlag(nullptr);
                e->removed();
                return;
            } else {
                LOGD("StaffText::remove: %s %p there is already another sound flag", e->typeName(), e);
            }
            break;
        }
        default: break;
    }

    StaffTextBase::remove(e);
}

void StaffText::setTrack(track_idx_t idx) {
    StaffTextBase::setTrack(idx);

    if (soundFlag_) {
        soundFlag_->setTrack(idx);
    }
}

bool StaffText::hasSoundFlag() const { return soundFlag_ != nullptr; }

SoundFlag *StaffText::soundFlag() const { return soundFlag_; }

void StaffText::setSoundFlag(SoundFlag *flag) {
    if (soundFlag_ == flag) {
        return;
    }

    soundFlag_ = flag;

    if (soundFlag_) {
        soundFlag_->setOwnershipParent(this);
        soundFlag_->setTrack(track());
    }

    triggerLayout();
}
