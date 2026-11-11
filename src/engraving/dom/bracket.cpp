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

#include "bracket.h"

#include "bracketitem.h"
#include "instrumentname.h"
#include "log.h"
#include "measure.h"
#include "score.h"
#include "staff.h"
#include "system.h"
#include "text.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::draw;

//---------------------------------------------------------
//   Bracket
//---------------------------------------------------------

Bracket::Bracket(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::BRACKET, parent) {
    ay1_ = 0;
    first_staff_ = 0;
    last_staff_ = 0;
    label_type_ = InstrumentNameType::LONG;
    bi_ = 0;
    brace_symbol_ = SymId::NO_SYM;
    magx_ = 1.;
    set_generated(true); // brackets are not saved
}

Bracket::~Bracket() {
    if (text_) {
        delete text_;
    }
}

void Bracket::scan_elements(std::function<void(EngravingItem *)> func) {
    if (text_) {
        text_->scan_elements(func);
    }

    EngravingItem::scan_elements(func);
}

//---------------------------------------------------------
//   setStaffSpan
//---------------------------------------------------------

void Bracket::set_staff_span(size_t a, size_t b) {
    first_staff_ = a;
    last_staff_ = b;

    if (bracket_type() == BracketType::BRACE && style().style_st(Sid::MUSICAL_SYMBOL_FONT) != "Emmentaler" &&
        style().style_st(Sid::MUSICAL_SYMBOL_FONT) != "Gonville") {
        int v = static_cast<int>(last_staff_ - first_staff_ + 1);

        // if staves inner staves are hidden, decrease span
        for (size_t staff_index = first_staff_; staff_index <= last_staff_; ++staff_index) {
            if (system() && !system()->staff(staff_index)->show()) {
                --v;
            }
        }

        if (style().style_st(Sid::MUSICAL_SYMBOL_FONT) == "Leland") {
            v = std::min(4, v);
        }

        // 1.625 is a "magic" number based on akkoladeDistance/4.0 (default value 6.5).
        magx_ = v + ((v - 1) * 1.625);

        if (v == 1) {
            brace_symbol_ = SymId::BRACE_SMALL;
        } else if (v <= 2) {
            brace_symbol_ = SymId::BRACE;
        } else if (v <= 3) {
            brace_symbol_ = SymId::BRACE_LARGE;
        } else {
            brace_symbol_ = SymId::BRACE_LARGER;
        }
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Bracket::get_property(Pid id) const {
    PropertyValue v = EngravingItem::get_property(id);
    if (!v.is_valid()) {
        v = bi_->get_property(id);
    }
    return v;
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Bracket::set_property(Pid id, const PropertyValue &v) { return bi_->set_property(id, v); }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Bracket::property_default(Pid id) const {
    if (id == Pid::BRACKET_COLUMN) {
        return size_t(0);
    }
    PropertyValue v = EngravingItem::property_default(id);
    if (!v.is_valid()) {
        v = bi_->property_default(id);
    }
    return v;
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Bracket::subtype_user_name() const { return TConv::user_name(bracket_type()); }

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void Bracket::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::COLOR) {
        set_color(v.value<Color>());
    }

    // brackets do not survive layout() and therefore cannot be on
    // the undo stack; delegate to BracketItem:
    BracketItem *bi = bracket_item();
    bi->undo_change_property(id, v, ps);
}

Fraction Bracket::tick() const {
    if (measure()) {
        return measure()->tick();
    }
    return EngravingItem::tick();
}

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void Bracket::set_selected(bool f) {
    bi_->set_selected(f);
    if (text_) {
        text_->set_selected(f);
    }

    EngravingItem::set_selected(f);
}

bool Bracket::intersects(const Bracket *other) const { return first_staff_ <= other->last_staff_ && last_staff_ >= other->first_staff_; }

bool Bracket::contains(staff_idx_t staff_idx) const { return staff_idx >= first_staff_ && staff_idx <= last_staff_; }
