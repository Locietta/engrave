/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "parenthesis.h"

#include "segment.h"
#include "types/typesconv.h"

using namespace mu::engraving;

Parenthesis::Parenthesis(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::PARENTHESIS, parent, ElementFlag::MOVABLE) {}

Parenthesis::Parenthesis(const Parenthesis &p) : EngravingItem(p) { direction_value = p.direction_value; }

PropertyValue Parenthesis::get_property(Pid pid) const {
    switch (pid) {
        case Pid::HORIZONTAL_DIRECTION: return direction();
        default: return EngravingItem::get_property(pid);
    }
}

bool Parenthesis::set_property(Pid pid, const PropertyValue &v) {
    switch (pid) {
        case Pid::HORIZONTAL_DIRECTION: set_direction(v.value<DirectionH>()); break;
        default: return EngravingItem::set_property(pid, v);
    }

    return true;
}

PropertyValue Parenthesis::property_default(Pid pid) const {
    switch (pid) {
        case Pid::HORIZONTAL_DIRECTION: return DirectionH::LEFT;
        default: return EngravingItem::property_default(pid);
    }
}

bool Parenthesis::follow_parent_cur_color() const { return follow_parent_color_; }

void Parenthesis::set_follow_parent_color(bool val) { follow_parent_color_ = val; }

Color Parenthesis::cur_color(const rendering::PaintOptions &opt) const {
    if (follow_parent_color_) {
        return layout_parent()->cur_color(opt);
    }

    return EngravingItem::cur_color(get_property(Pid::VISIBLE).to_bool(), get_property(Pid::COLOR).value<Color>(), opt);
}
