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

static const ElementStyle staff_style{
    {Sid::STAFF_TEXT_PLACEMENT, Pid::PLACEMENT},
    {Sid::STAFF_TEXT_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

StaffText::StaffText(DummyParentOr<Segment> parent, TextStyleType tid)
    : StaffTextBase(ElementType::STAFF_TEXT, parent, tid, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::staff_style);
    reset_property(Pid::MUSIC_SYMBOL_SIZE);
}

StaffText::StaffText(const StaffText &t) : StaffTextBase(t) {
    if (t.sound_flag_) {
        set_sound_flag(t.sound_flag_->clone());
    }
}

EngravingItem *StaffText::linked_clone() {
    StaffText *clone = static_cast<StaffText *>(StaffTextBase::linked_clone());

    if (clone->sound_flag_ && sound_flag_) {
        score()->undo(new Link(clone->sound_flag_, sound_flag_));
    }

    return clone;
}

PropertyValue StaffText::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::STAFF;
        default: return StaffTextBase::property_default(id);
    }
}

void StaffText::scan_elements(std::function<void(EngravingItem *)> func) {
    if (sound_flag_) {
        sound_flag_->scan_elements(func);
    }

    StaffTextBase::scan_elements(func);
}

void StaffText::add(EngravingItem *e) {
    e->set_ownership_parent(this);
    e->set_track(track());

    switch (e->type()) {
        case ElementType::SOUND_FLAG:
            set_sound_flag(to_sound_flag(e));
            e->added();
            return;
        default: break;
    }

    StaffTextBase::add(e);
}

void StaffText::remove(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::SOUND_FLAG: {
            if (sound_flag() == e) {
                set_sound_flag(nullptr);
                e->removed();
                return;
            } else {
                LOGD("StaffText::remove: %s %p there is already another sound flag", e->type_name(), e);
            }
            break;
        }
        default: break;
    }

    StaffTextBase::remove(e);
}

void StaffText::set_track(track_idx_t idx) {
    StaffTextBase::set_track(idx);

    if (sound_flag_) {
        sound_flag_->set_track(idx);
    }
}

bool StaffText::has_sound_flag() const { return sound_flag_ != nullptr; }

SoundFlag *StaffText::sound_flag() const { return sound_flag_; }

void StaffText::set_sound_flag(SoundFlag *flag) {
    if (sound_flag_ == flag) {
        return;
    }

    sound_flag_ = flag;

    if (sound_flag_) {
        sound_flag_->set_ownership_parent(this);
        sound_flag_->set_track(track());
    }

    trigger_layout();
}
