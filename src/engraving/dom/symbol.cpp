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

#include "score.h"

#include "draw/fontmetrics.h"
#include "iengravingfont.h"

#include "types/symnames.h"

#include "image.h"
#include "staff.h"

#include "log.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   Symbol
//---------------------------------------------------------

Symbol::Symbol(const ElementType &type, EngravingObject *parent, ElementFlags f) : BSymbol(type, parent, f) {
    sym_ = SymId::accidentalSharp; // arbitrary valid default
}

Symbol::Symbol(EngravingObject *parent, ElementFlags f) : Symbol(ElementType::SYMBOL, parent, f) {}

Symbol::Symbol(const Symbol &s) : BSymbol(s) {
    sym_ = s.sym_;
    scoreFont_ = s.scoreFont_;
    symbolsSize_ = s.symbolsSize_;
    symAngle_ = s.symAngle_;
}

void Symbol::setSym(SymId s, const std::shared_ptr<IEngravingFont> &sf) {
    sym_ = s;

    if (sf.get()) {
        scoreFont_ = sf;
    }
}

//---------------------------------------------------------
//   symName
//---------------------------------------------------------

AsciiStringView Symbol::symName() const { return SymNames::nameForSymId(sym_); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Symbol::subtypeUserName() const { return SymNames::userNameForSymId(sym_); }

//---------------------------------------------------------
//   Symbol::getProperty
//---------------------------------------------------------
PropertyValue Symbol::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::SYMBOL: return PropertyValue::fromValue(sym_);
        case Pid::SCORE_FONT:
            if (scoreFont_) {
                return PropertyValue::fromValue(String::fromStdString(scoreFont_->name()));
            } else {
                return PropertyValue::fromValue(String());
            }
        case Pid::SYMBOLS_SIZE: return PropertyValue::fromValue(symbolsSize_);
        case Pid::SYMBOL_ANGLE: return PropertyValue::fromValue(symAngle_);
        default: break;
    }
    return BSymbol::getProperty(propertyId);
}

//---------------------------------------------------------
//   Symbol::setProperty
//---------------------------------------------------------

bool Symbol::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::SYMBOL: sym_ = v.value<SymId>(); break;
        case Pid::SCORE_FONT: scoreFont_ = score()->engravingFonts()->fontByName(v.value<String>().toStdString()); break;
        case Pid::SYMBOLS_SIZE: symbolsSize_ = v.toDouble(); break;
        case Pid::SYMBOL_ANGLE: symAngle_ = v.toDouble(); break;
        default: break;
    }
    triggerLayout();
    return BSymbol::setProperty(propertyId, v);
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Symbol::propertyDefault(Pid propertyId) const {
    switch (propertyId) {
        case Pid::SYMBOL: return SymId::accidentalSharp;
        case Pid::SYMBOL_ANGLE: return 0.0;
        case Pid::SYMBOLS_SIZE: return 1.0;
        case Pid::SCORE_FONT:
            if (scoreFont_) {
                return style().styleSt(Sid::musicalSymbolFont);
            } else {
                return String();
            }
        default: break;
    }
    return EngravingItem::propertyDefault(propertyId);
}

void Symbol::reset() {
    undoResetProperty(Pid::SYMBOLS_SIZE);
    undoResetProperty(Pid::SYMBOL_ANGLE);
    undoResetProperty(Pid::SCORE_FONT);
    BSymbol::reset();
}

//---------------------------------------------------------
//   FSymbol
//---------------------------------------------------------

FSymbol::FSymbol(DummyParentOr<EngravingItem> parent) : BSymbol(ElementType::FSYMBOL, parent) {
    code_ = 0;
    font_.setNoFontMerging(true);
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

String FSymbol::toString() const { return String::fromUcs4(code_); }

//---------------------------------------------------------
//   accessibleInfo
// Screen readers know how to pronounce the common font symbols so we can
// return just the character itself. Similarly, common characters should
// be rendered correctly by Braille terminals.
//---------------------------------------------------------

//---------------------------------------------------------
//   setFont
//---------------------------------------------------------

void FSymbol::setFont(const Font &f) {
    font_ = f;
    font_.setNoFontMerging(true);
}
} // namespace mu::engraving
