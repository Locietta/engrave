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

#include "types/typesconv.h"

#include "bracketitem.h"
#include "instrumentname.h"
#include "measure.h"
#include "score.h"
#include "staff.h"
#include "system.h"
#include "text.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::draw;

//---------------------------------------------------------
//   Bracket
//---------------------------------------------------------

Bracket::Bracket(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::BRACKET, parent) {
    ay1_ = 0;
    firstStaff_ = 0;
    lastStaff_ = 0;
    labelType_ = InstrumentNameType::LONG;
    bi_ = 0;
    braceSymbol_ = SymId::noSym;
    magx_ = 1.;
    setGenerated(true); // brackets are not saved
}

Bracket::~Bracket() {
    if (text_) {
        delete text_;
    }
}

void Bracket::scanElements(std::function<void(EngravingItem *)> func) {
    if (text_) {
        text_->scanElements(func);
    }

    EngravingItem::scanElements(func);
}

//---------------------------------------------------------
//   setStaffSpan
//---------------------------------------------------------

void Bracket::setStaffSpan(size_t a, size_t b) {
    firstStaff_ = a;
    lastStaff_ = b;

    if (bracketType() == BracketType::BRACE && style().styleSt(Sid::musicalSymbolFont) != "Emmentaler" &&
        style().styleSt(Sid::musicalSymbolFont) != "Gonville") {
        int v = static_cast<int>(lastStaff_ - firstStaff_ + 1);

        // if staves inner staves are hidden, decrease span
        for (size_t staffIndex = firstStaff_; staffIndex <= lastStaff_; ++staffIndex) {
            if (system() && !system()->staff(staffIndex)->show()) {
                --v;
            }
        }

        if (style().styleSt(Sid::musicalSymbolFont) == "Leland") {
            v = std::min(4, v);
        }

        // 1.625 is a "magic" number based on akkoladeDistance/4.0 (default value 6.5).
        magx_ = v + ((v - 1) * 1.625);

        if (v == 1) {
            braceSymbol_ = SymId::braceSmall;
        } else if (v <= 2) {
            braceSymbol_ = SymId::brace;
        } else if (v <= 3) {
            braceSymbol_ = SymId::braceLarge;
        } else {
            braceSymbol_ = SymId::braceLarger;
        }
    }
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool Bracket::acceptDrop(EditData &data) const { return data.dropElement->type() == ElementType::BRACKET; }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Bracket::drop(Transaction &, EditData &data) {
    EngravingItem *e = data.dropElement;
    Bracket *b = 0;
    if (e->isBracket()) {
        b = toBracket(e);
        undoChangeProperty(Pid::SYSTEM_BRACKET, int(b->bracketType()));
    }
    delete e;
    return this;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Bracket::getProperty(Pid id) const {
    PropertyValue v = EngravingItem::getProperty(id);
    if (!v.isValid()) {
        v = bi_->getProperty(id);
    }
    return v;
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Bracket::setProperty(Pid id, const PropertyValue &v) { return bi_->setProperty(id, v); }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Bracket::propertyDefault(Pid id) const {
    if (id == Pid::BRACKET_COLUMN) {
        return size_t(0);
    }
    PropertyValue v = EngravingItem::propertyDefault(id);
    if (!v.isValid()) {
        v = bi_->propertyDefault(id);
    }
    return v;
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Bracket::subtypeUserName() const { return TConv::userName(bracketType()); }

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void Bracket::undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::COLOR) {
        setColor(v.value<Color>());
    }

    // brackets do not survive layout() and therefore cannot be on
    // the undo stack; delegate to BracketItem:
    BracketItem *bi = bracketItem();
    bi->undoChangeProperty(id, v, ps);
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

void Bracket::setSelected(bool f) {
    bi_->setSelected(f);
    if (text_) {
        text_->setSelected(f);
    }

    EngravingItem::setSelected(f);
}

bool Bracket::intersects(const Bracket *other) const { return firstStaff_ <= other->lastStaff_ && lastStaff_ >= other->firstStaff_; }

bool Bracket::contains(staff_idx_t staffIdx) const { return staffIdx >= firstStaff_ && staffIdx <= lastStaff_; }
