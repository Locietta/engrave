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

#ifndef MU_ENGRAVING_BRACKET_ITEM_H
#define MU_ENGRAVING_BRACKET_ITEM_H

#include "engravingitem.h"
#include "stafflabel.h"

#include "../types/types.h"

namespace mu::engraving {
class Factory;

//---------------------------------------------------------
//   BracketItem
//---------------------------------------------------------

class BracketItem final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, BracketItem)

public:
    EngravingItem *clone() const override;

    PropertyValue getProperty(Pid) const override;
    bool setProperty(Pid, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    size_t bracketSpan() const { return bracketSpan_; }

    BracketType bracketType() const { return bracketType_; }

    void setBracketSpan(size_t v) { bracketSpan_ = v; }

    void setBracketType(BracketType v) { bracketType_ = v; }

    staff_idx_t startStaffIdx() const { return startStaffIdx_; }

    void setStartStaffIdx(staff_idx_t staffIdx) { startStaffIdx_ = staffIdx; }

    Staff *startStaff() const;

    size_t column() const { return column_; }

    void setColumn(size_t v) { column_ = v; }

    StaffLabel &label() { return name_; }

    const StaffLabel &label() const { return name_; }

    const String &longName() const { return name_.longName(); }

    const String &shortName() const { return name_.shortName(); }

    bool showText() const { return showText_; }

    bool showBracket() const { return showBracket_; }

    bool intersects(const BracketItem *other) const;
    bool intersects(staff_idx_t first, staff_idx_t last) const;

private:

    friend class Factory;

    BracketItem(DummyParentOr<EngravingItem> parent);
    BracketItem(DummyParentOr<EngravingItem> parent, BracketType bracketType, size_t span);

    BracketType bracketType_ = BracketType::NO_BRACKET;
    size_t column_ = 0;
    size_t bracketSpan_ = 0;
    staff_idx_t startStaffIdx_ = muse::nidx;

    StaffLabel name_ = StaffLabel(muse::mtrc("systemBrackets", "GROUP"), muse::mtrc("systemBrackets", "GR.", "Short for GROUP"));
    bool showText_ = true;
    bool showBracket_ = true;
};
} // namespace mu::engraving
#endif
