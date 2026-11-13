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
#include "beambase.h"

#include "beam.h"

using namespace mu::engraving;

BeamBase::BeamBase(const ElementType &type, EngravingObject *parent, ElementFlags flags) : EngravingItem(type, parent, flags) {}

BeamBase::BeamBase(const BeamBase &b) : EngravingItem(b) {
    beam_segments_.reserve(b.beam_segments_.size());
    for (const BeamSegment *bs : b.beam_segments_) {
        beam_segments_.push_back(new BeamSegment(*bs));
    }
    cross_staff_move_value = b.cross_staff_move_value;
    direction_ = b.direction_;
    user_modified_[0] = b.user_modified_[0];
    user_modified_[1] = b.user_modified_[1];
}

void BeamBase::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::BEAM_CROSS_STAFF_MOVE) {
        if (!accept_cross_staff_move(v.to_int())) {
            return;
        }
        undo_reset_property(Pid::USER_MODIFIED);
        undo_reset_property(Pid::BEAM_POS);
    }
    EngravingItem::undo_change_property(id, v, ps);
}

PropertyValue BeamBase::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::BEAM_CROSS_STAFF_MOVE: return cross_staff_move();
        case Pid::STEM_DIRECTION: return direction();
        case Pid::USER_MODIFIED: return user_modified();
        default: return EngravingItem::get_property(property_id);
    }
}

bool BeamBase::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::BEAM_CROSS_STAFF_MOVE: set_cross_staff_move(v.to_int()); break;
        case Pid::STEM_DIRECTION: set_direction(v.value<DirectionV>()); break;
        case Pid::USER_MODIFIED: set_user_modified(v.to_bool()); break;
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

PropertyValue BeamBase::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::BEAM_CROSS_STAFF_MOVE: return 0;
        case Pid::STEM_DIRECTION: return DirectionV::AUTO;
        case Pid::USER_MODIFIED: return false;
        default: return EngravingItem::property_default(property_id);
    }
}

int BeamBase::cross_staff_idx() const {
    const auto *ld = ldata();
    return default_cross_staff_idx() + (ld ? ld->effective_cross_staff_move.value_or(cross_staff_move_value) : cross_staff_move_value);
}

int BeamBase::default_cross_staff_idx() const {
    double average = 0.5 * (static_cast<double>(min_cr_move()) + static_cast<double>(max_cr_move()));
    return std::ceil(average);
}

bool BeamBase::accept_cross_staff_move(int move) const {
    int new_cross_staff_idx = default_cross_staff_idx() + move;
    return new_cross_staff_idx >= min_cr_move() && new_cross_staff_idx <= max_cr_move() + 1;
}

bool BeamBase::user_modified() const {
    int idx = direction_idx();
    if (const auto *table = authored_override_table(this)) {
        const auto &entries = table->entries;
        return std::any_of(entries.begin(), entries.end(), [idx](const auto &entry) {
            return entry.child.role == AuthoredChildRole::BEAM_FRAGMENT && entry.child.direction == idx && entry.pid == Pid::BEAM_POS;
        });
    }
    return user_modified_[idx];
}

void BeamBase::set_user_modified(bool val) {
    int idx = direction_idx();
    if (is_beam()) {
        auto *beam = static_cast<Beam *>(this);
        if (val) {
            if (!user_modified()) beam->set_beam_pos(PairF(0.0, 0.0));
        } else {
            std::erase_if(beam->authored_overrides.entries, [idx](const auto &entry) {
                return entry.child.role == AuthoredChildRole::BEAM_FRAGMENT && entry.child.direction == idx;
            });
        }
        return;
    }
    if (auto *table = authored_override_table(this)) {
        const AuthoredChildKey key{AuthoredChildRole::BEAM_FRAGMENT, 0, static_cast<uint8_t>(idx)};
        if (val && !table->find(key, Pid::BEAM_POS))
            table->set({key, Pid::BEAM_POS, PropertyValue::from_value(PairF(0.0, 0.0)), PropertyFlags::NOSTYLE});
        else if (!val)
            table->erase(key, Pid::BEAM_POS);
        return;
    }
    user_modified_[idx] = val;
}

void BeamBase::clear_beam_segments() {
    muse::delete_all(beam_segments_);
    beam_segments_.clear();
}
