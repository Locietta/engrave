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

#include "systemdivider.h"

#include "types/symnames.h"

#include "score.h"
#include "system.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   SystemDivider
//---------------------------------------------------------

static const ElementStyle divider_style{{Sid::DIVIDER_LEFT_SIZE, Pid::SYMBOLS_SIZE}, // left or right is decided later
                                        {Sid::MUSICAL_SYMBOL_FONT, Pid::SCORE_FONT}};

SystemDivider::SystemDivider(DummyParentOr<System> parent)
    : Symbol(ElementType::SYSTEM_DIVIDER, parent, ElementFlag::SYSTEM | ElementFlag::MOVABLE) {
    // default value, but not valid until setDividerType()
    divider_type_ = SystemDividerType::LEFT;
    sym_ = SymId::SYSTEM_DIVIDER;
}

//---------------------------------------------------------
//   SystemDivider
//---------------------------------------------------------

SystemDivider::SystemDivider(const SystemDivider &sd) : Symbol(sd) { divider_type_ = sd.divider_type_; }

//---------------------------------------------------------
//   setDividerType
//---------------------------------------------------------

void SystemDivider::set_divider_type(SystemDividerType v) {
    divider_type_ = v;

    if (v == SystemDividerType::LEFT) {
        set_sym(SymNames::sym_id_by_name(style().style_st(Sid::DIVIDER_LEFT_SYM)));
    } else {
        set_sym(SymNames::sym_id_by_name(style().style_st(Sid::DIVIDER_RIGHT_SYM)));
    }

    init_element_style(&::mu::engraving::divider_style);
}

void SystemDivider::style_changed() {
    if (divider_type_ == SystemDividerType::LEFT) {
        set_sym(SymNames::sym_id_by_name(style().style_st(Sid::DIVIDER_LEFT_SYM)));
    } else {
        set_sym(SymNames::sym_id_by_name(style().style_st(Sid::DIVIDER_RIGHT_SYM)));
    }

    if (is_styled(Pid::SYMBOLS_SIZE)) {
        symbols_size_ = style().style_d(get_property_style(Pid::SYMBOLS_SIZE));
    }

    Symbol::style_changed();
}

Sid SystemDivider::get_property_style(Pid id) const {
    if (id == Pid::SYMBOLS_SIZE) {
        return divider_type_ == SystemDividerType::LEFT ? Sid::DIVIDER_LEFT_SIZE : Sid::DIVIDER_RIGHT_SIZE;
    }

    return Symbol::get_property_style(id);
}

void SystemDivider::reset() {
    Symbol::reset();
    set_generated(true);
}

//---------------------------------------------------------
//   drag
//---------------------------------------------------------

PropertyValue SystemDivider::property_default(Pid id) const {
    switch (id) {
        case Pid::SYMBOLS_SIZE:
            return style().style_d(divider_type_ == SystemDividerType::LEFT ? Sid::DIVIDER_LEFT_SIZE : Sid::DIVIDER_RIGHT_SIZE);
        default: return Symbol::property_default(id);
    }
}
} // namespace mu::engraving
