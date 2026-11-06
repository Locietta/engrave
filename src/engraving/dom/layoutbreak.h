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

#pragma once

#include "engravingitem.h"
#include "draw/types/font.h"

namespace mu::engraving {
class Factory;
class Transaction;

//---------------------------------------------------------
//   @@ LayoutBreak
///    symbols for line break, page break etc.
//---------------------------------------------------------
class LayoutBreak final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, LayoutBreak)
    DECLARE_CLASSOF(ElementType::LAYOUT_BREAK)

public:

    void set_ownership_parent(DummyParentOr<MeasureBase> parent);

    LayoutBreak *clone() const override { return new LayoutBreak(*this); }

    int subtype() const override { return static_cast<int>(layout_break_type_); }

    TranslatableString subtype_user_name() const override;

    void set_layout_break_type(LayoutBreakType);

    LayoutBreakType layout_break_type() const { return layout_break_type_; }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    MeasureBase *measure() const { return (MeasureBase *) ownership_parent(); }

    double pause() const { return pause_; }

    void set_pause(double v) { pause_ = v; }

    bool start_with_long_names() const { return start_with_long_names_; }

    void set_start_with_long_names(bool v) { start_with_long_names_ = v; }

    bool start_with_measure_one() const { return start_with_measure_one_; }

    void set_start_with_measure_one(bool v) { start_with_measure_one_ = v; }

    bool first_system_indentation() const { return first_system_indentation_; }

    void set_first_system_indentation(bool v) { first_system_indentation_ = v; }

    bool show_courtesy() const { return show_courtesy_; }

    void set_show_courtesy(bool v) { show_courtesy_ = v; }

    bool is_page_break() const { return layout_break_type_ == LayoutBreakType::PAGE; }

    bool is_line_break() const { return layout_break_type_ == LayoutBreakType::LINE; }

    bool is_section_break() const { return layout_break_type_ == LayoutBreakType::SECTION; }

    bool is_no_break() const { return layout_break_type_ == LayoutBreakType::NOBREAK; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    char16_t icon_code() const;

    muse::draw::Font font() const;

private:

    friend class Factory;
    LayoutBreak(DummyParentOr<MeasureBase> parent);
    LayoutBreak(const LayoutBreak &);

    double pause_ = 0.0;
    bool start_with_long_names_ = false;
    bool start_with_measure_one_ = false;
    bool first_system_indentation_ = false;
    bool show_courtesy_ = false;
    LayoutBreakType layout_break_type_ = LayoutBreakType::NOBREAK;
};
} // namespace mu::engraving
