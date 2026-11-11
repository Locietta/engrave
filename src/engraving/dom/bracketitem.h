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

#include "../types/types.h"
#include "engravingitem.h"
#include "stafflabel.h"

namespace mu::engraving {
struct Factory;

//---------------------------------------------------------
//   BracketItem
//---------------------------------------------------------

struct BracketItem final : EngravingItem {

public:
    EngravingItem *clone() const override;

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    size_t bracket_span() const { return bracket_span_; }

    BracketType bracket_type() const { return bracket_type_; }

    void set_bracket_span(size_t v) { bracket_span_ = v; }

    void set_bracket_type(BracketType v) { bracket_type_ = v; }

    staff_idx_t start_staff_idx() const { return start_staff_idx_; }

    void set_start_staff_idx(staff_idx_t staff_idx) { start_staff_idx_ = staff_idx; }

    Staff *start_staff() const;

    size_t column() const { return column_; }

    void set_column(size_t v) { column_ = v; }

    StaffLabel &label() { return name_; }

    const StaffLabel &label() const { return name_; }

    const String &long_name() const { return name_.long_name(); }

    const String &short_name() const { return name_.short_name(); }

    bool show_text() const { return show_text_; }

    bool show_bracket() const { return show_bracket_; }

    bool intersects(const BracketItem *other) const;
    bool intersects(staff_idx_t first, staff_idx_t last) const;

private:

    friend struct Factory;

    BracketItem(DummyParentOr<EngravingItem> parent);
    BracketItem(DummyParentOr<EngravingItem> parent, BracketType bracket_type, size_t span);

    BracketType bracket_type_ = BracketType::NO_BRACKET;
    size_t column_ = 0;
    size_t bracket_span_ = 0;
    staff_idx_t start_staff_idx_ = muse::nidx;

    StaffLabel name_ = StaffLabel(muse::mtrc("systemBrackets", "GROUP"), muse::mtrc("systemBrackets", "GR.", "Short for GROUP"));
    bool show_text_ = true;
    bool show_bracket_ = true;
};
} // namespace mu::engraving
#endif
