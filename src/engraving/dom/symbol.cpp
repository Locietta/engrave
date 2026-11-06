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

#include "symbol.h"

#include "draw/fontmetrics.h"
#include "iengravingfont.h"
#include "image.h"
#include "log.h"
#include "score.h"
#include "staff.h"
#include "types/symnames.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   Symbol
//---------------------------------------------------------

Symbol::Symbol(const ElementType &type, EngravingObject *parent, ElementFlags f) : BSymbol(type, parent, f) {
    sym_ = SymId::ACCIDENTAL_SHARP; // arbitrary valid default
}

Symbol::Symbol(EngravingObject *parent, ElementFlags f) : Symbol(ElementType::SYMBOL, parent, f) {}

Symbol::Symbol(const Symbol &s) : BSymbol(s) {
    sym_ = s.sym_;
    score_font_ = s.score_font_;
    symbols_size_ = s.symbols_size_;
    sym_angle_ = s.sym_angle_;
}

void Symbol::set_sym(SymId s, const std::shared_ptr<IEngravingFont> &sf) {
    sym_ = s;

    if (sf.get()) {
        score_font_ = sf;
    }
}

//---------------------------------------------------------
//   symName
//---------------------------------------------------------

AsciiStringView Symbol::sym_name() const { return SymNames::name_for_sym_id(sym_); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Symbol::subtype_user_name() const { return SymNames::user_name_for_sym_id(sym_); }

//---------------------------------------------------------
//   Symbol::getProperty
//---------------------------------------------------------
PropertyValue Symbol::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SYMBOL: return PropertyValue::from_value(sym_);
        case Pid::SCORE_FONT:
            if (score_font_) {
                return PropertyValue::from_value(String::from_std_string(score_font_->name()));
            } else {
                return PropertyValue::from_value(String());
            }
        case Pid::SYMBOLS_SIZE: return PropertyValue::from_value(symbols_size_);
        case Pid::SYMBOL_ANGLE: return PropertyValue::from_value(sym_angle_);
        default: break;
    }
    return BSymbol::get_property(property_id);
}

//---------------------------------------------------------
//   Symbol::setProperty
//---------------------------------------------------------

bool Symbol::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SYMBOL: sym_ = v.value<SymId>(); break;
        case Pid::SCORE_FONT: score_font_ = score()->engraving_fonts()->font_by_name(v.value<String>().to_std_string()); break;
        case Pid::SYMBOLS_SIZE: symbols_size_ = v.to_double(); break;
        case Pid::SYMBOL_ANGLE: sym_angle_ = v.to_double(); break;
        default: break;
    }
    trigger_layout();
    return BSymbol::set_property(property_id, v);
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Symbol::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::SYMBOL: return SymId::ACCIDENTAL_SHARP;
        case Pid::SYMBOL_ANGLE: return 0.0;
        case Pid::SYMBOLS_SIZE: return 1.0;
        case Pid::SCORE_FONT:
            if (score_font_) {
                return style().style_st(Sid::MUSICAL_SYMBOL_FONT);
            } else {
                return String();
            }
        default: break;
    }
    return EngravingItem::property_default(property_id);
}

void Symbol::reset() {
    undo_reset_property(Pid::SYMBOLS_SIZE);
    undo_reset_property(Pid::SYMBOL_ANGLE);
    undo_reset_property(Pid::SCORE_FONT);
    BSymbol::reset();
}

//---------------------------------------------------------
//   FSymbol
//---------------------------------------------------------

FSymbol::FSymbol(DummyParentOr<EngravingItem> parent) : BSymbol(ElementType::FSYMBOL, parent) {
    code_ = 0;
    font_.set_no_font_merging(true);
}

FSymbol::FSymbol(const FSymbol &s) : BSymbol(s) {
    font_ = s.font_;
    code_ = s.code_;
}

//---------------------------------------------------------
//   toString
// FSymbol is a single code point but code points above 2^16 cannot be
// represented by a single Char, hence we return a String instead. Char
// and String use the UTF-16 encoding internally (like QChar, QString).
//---------------------------------------------------------

String FSymbol::to_string() const { return String::from_ucs4(code_); }

//---------------------------------------------------------
//   accessibleInfo
// Screen readers know how to pronounce the common font symbols so we can
// return just the character itself. Similarly, common characters should
// be rendered correctly by Braille terminals.
//---------------------------------------------------------

//---------------------------------------------------------
//   setFont
//---------------------------------------------------------

void FSymbol::set_font(const Font &f) {
    font_ = f;
    font_.set_no_font_merging(true);
}
} // namespace mu::engraving
