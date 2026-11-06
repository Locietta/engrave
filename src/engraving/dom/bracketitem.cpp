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

#include "bracketitem.h"

#include "property.h"
#include "score.h"
#include "staff.h"

#include "../editing/editstaffbrackets.h"

using namespace mu;

namespace mu::engraving {
BracketItem::BracketItem(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::BRACKET_ITEM, parent) {}

BracketItem::BracketItem(DummyParentOr<EngravingItem> parent, BracketType bracket_type, size_t span)
    : EngravingItem(ElementType::BRACKET_ITEM, parent), bracket_type_(bracket_type), bracket_span_(span) {}

EngravingItem *BracketItem::clone() const { return new BracketItem(*this); }

Staff *BracketItem::start_staff() const { return score() ? score()->staff(start_staff_idx_) : nullptr; }

PropertyValue BracketItem::get_property(Pid id) const {
    switch (id) {
        case Pid::SYSTEM_BRACKET: return int(bracket_type_);
        case Pid::BRACKET_COLUMN: return column_;
        case Pid::BRACKET_SPAN: return static_cast<int>(bracket_span_);
        case Pid::STAFF_LONG_NAME: return long_name();
        case Pid::STAFF_SHORT_NAME: return short_name();
        case Pid::GROUP_BRACKET_SHOW_TEXT: return show_text_;
        case Pid::GROUP_BRACKET_SHOW_BRACKET: return show_bracket_;
        default: return EngravingItem::get_property(id);
    }
}

bool BracketItem::set_property(Pid id, const PropertyValue &v) {
    switch (id) {
        case Pid::SYSTEM_BRACKET:
            EditStaffBrackets::set_bracket_type(score(), start_staff_idx(), column(),
                                                BracketType(v.to_int())); // change bracket type global
            break;
        case Pid::BRACKET_COLUMN:
            EditStaffBrackets::change_bracket_column(score(), start_staff_idx(), column(), static_cast<size_t>(v.to_int()));
            break;
        case Pid::BRACKET_SPAN: bracket_span_ = static_cast<size_t>(v.to_int()); break;
        case Pid::VISIBLE: set_visible(v.to_bool()); break;
        case Pid::STAFF_LONG_NAME: name_.set_long_name(v.value<String>()); break;
        case Pid::STAFF_SHORT_NAME: name_.set_short_name(v.value<String>()); break;
        case Pid::GROUP_BRACKET_SHOW_TEXT: show_text_ = v.to_bool(); break;
        case Pid::GROUP_BRACKET_SHOW_BRACKET: show_bracket_ = v.to_bool(); break;
        default: return EngravingItem::set_property(id, v);
    }
    score()->set_layout_all();
    return true;
}

PropertyValue BracketItem::property_default(Pid id) const {
    switch (id) {
        case Pid::SYSTEM_BRACKET: return int(BracketType::NORMAL);
        case Pid::BRACKET_COLUMN: return size_t(0);
        case Pid::STAFF_LONG_NAME: return muse::mtrc("systemBrackets", "GROUP");
        case Pid::STAFF_SHORT_NAME: return muse::mtrc("systemBrackets", "GR.", "Short for GROUP");
        case Pid::GROUP_BRACKET_SHOW_TEXT:
        case Pid::GROUP_BRACKET_SHOW_BRACKET: return true;
        default: return EngravingItem::property_default(id);
    }
}

bool BracketItem::intersects(const BracketItem *other) const {
    staff_idx_t first_of_other = other->start_staff_idx();
    staff_idx_t last_of_other = first_of_other + other->bracket_span() - 1;
    return intersects(first_of_other, last_of_other);
}

bool BracketItem::intersects(staff_idx_t first, staff_idx_t last) const {
    staff_idx_t first_of_this = start_staff_idx_;
    staff_idx_t last_of_this = first_of_this + bracket_span() - 1;
    return first_of_this <= last && last_of_this >= first;
}
} // namespace mu::engraving
