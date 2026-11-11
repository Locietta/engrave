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
#include "stem.h"

#include <cmath>

#include "chord.h"
#include "hook.h"
#include "log.h"
#include "tremolosinglechord.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

static const ElementStyle stem_style{{Sid::STEM_WIDTH, Pid::LINE_WIDTH}};

Stem::Stem(DummyParentOr<Chord> parent) : EngravingItem(ElementType::STEM, parent) {
    init_element_style(&::stem_style);
    reset_property(Pid::USER_LEN);
}

EngravingItem *Stem::element_base() const { return chord(); }

staff_idx_t Stem::v_staff_idx() const { return staff_idx() + chord()->staff_move(); }

bool Stem::up() const { return chord() ? chord()->up() : true; }

void Stem::set_base_length(Spatium base_length) { base_length_ = Spatium(std::abs(base_length.val())); }

double Stem::line_width_mag() const { return absolute_from_spatium(line_width_) * chord()->intrinsic_mag(); }

//! In chord coordinates
PointF Stem::flag_position() const { return pos() + PointF(ldata()->bbox().left(), up() ? -length() : length()); }

void Stem::reset() {
    undo_change_property(Pid::USER_LEN, 0.0_sp);
    EngravingItem::reset();
}

PropertyValue Stem::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::LINE_WIDTH: return line_width();
        case Pid::USER_LEN: return user_length();
        case Pid::STEM_DIRECTION: return PropertyValue::from_value<DirectionV>(chord()->stem_direction());
        case Pid::APPEARANCE_LINKED_TO_MASTER: return chord() ? chord()->is_property_linked_to_master(Pid::STEM_DIRECTION) : true;
        default: return EngravingItem::get_property(property_id);
    }
}

bool Stem::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::LINE_WIDTH: set_line_width(v.value<Spatium>()); break;
        case Pid::USER_LEN: set_user_length(v.value<Spatium>()); break;
        case Pid::STEM_DIRECTION: chord()->set_stem_direction(v.value<DirectionV>()); break;
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            if (chord() && v.to_bool() == true) {
                chord()->relink_property_to_master(Pid::STEM_DIRECTION);
                break;
            }
        // fall through
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

PropertyValue Stem::property_default(Pid id) const {
    switch (id) {
        case Pid::USER_LEN: return 0.0_sp;
        case Pid::STEM_DIRECTION: return PropertyValue::from_value<DirectionV>(DirectionV::AUTO);
        default: return EngravingItem::property_default(id);
    }
}
