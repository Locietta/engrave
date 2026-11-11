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

#include "bracketitem.h"
#include "engravingitem.h"

namespace mu::engraving {
struct Factory;
struct System;
struct Transaction;
enum class BracketType : signed char;
enum class InstrumentNameType : char;

//---------------------------------------------------------
//   @@ Bracket
//---------------------------------------------------------

struct Bracket final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::BRACKET)

public:

    ~Bracket();

    Bracket *clone() const override { return new Bracket(*this); }

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void set_bracket_item(BracketItem *i) { bi_ = i; }

    BracketItem *bracket_item() const { return bi_; }

    BracketType bracket_type() const { return bi_->bracket_type(); }

    int subtype() const override { return int(bracket_type()); }

    TranslatableString subtype_user_name() const override;

    size_t first_staff() const { return first_staff_; }

    size_t last_staff() const { return last_staff_; }

    void set_staff_span(size_t a, size_t b);

    InstrumentNameType label_type() const { return label_type_; }

    void set_label_type(InstrumentNameType t) { label_type_ = t; }

    SymId brace_symbol() const { return brace_symbol_; }

    void set_brace_symbol(const SymId &sym) { brace_symbol_ = sym; }

    size_t column() const { return bi_->column(); }

    size_t span() const { return bi_->bracket_span(); }

    double magx() const { return magx_; }

    System *system() const { return (System *) ownership_parent(); }

    Measure *measure() const { return measure_; }

    void set_measure(Measure *measure) { measure_ = measure; }

    const BracketItem *bi() const { return bi_; }

    Color color() const override { return bi_->color(); }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    void undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    using EngravingObject::undo_change_property;

    Fraction tick() const override;

    void set_selected(bool f) override;

    Text *text() const { return text_; }

    void set_text(Text *t) { text_ = t; }

    bool intersects(const Bracket *other) const;
    bool contains(staff_idx_t staff_idx) const;

    struct LayoutData : public EngravingItem::LayoutData {
        SymId brace_symbol = SymId::NO_SYM;
        PainterPath path;
        Shape shape;
        ld_field<double> bracket_height = {"[Bracket] bracketHeight", 0.0};
        ld_field<double> bracket_width = {"[Bracket] bracketWidth", 0.0};

        double h2(LdAccess mode = LdAccess::CHECK) const { return bracket_height.value(mode) * 0.5; }
    };
    DECLARE_LAYOUTDATA_METHODS(Bracket)

private:
    friend struct Factory;

    Bracket(DummyParentOr<EngravingItem> parent);

    BracketItem *bi_ = nullptr;
    double ay1_ = 0.0;

    size_t first_staff_ = 0;
    size_t last_staff_ = 0;

    InstrumentNameType label_type_;

    SymId brace_symbol_ = SymId::NO_SYM;

    // horizontal scaling factor for brace symbol. Cannot be equal to magY or depend on h
    // because layout needs width of brace before knowing height of system...
    double magx_ = 0.0;
    Measure *measure_ = nullptr;

    Text *text_ = nullptr;
};
} // namespace mu::engraving
