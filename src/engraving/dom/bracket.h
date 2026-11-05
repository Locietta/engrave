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
#include "bracketitem.h"

namespace mu::engraving {
class Factory;
class System;
class Transaction;
enum class BracketType : signed char;
enum class InstrumentNameType : char;

//---------------------------------------------------------
//   @@ Bracket
//---------------------------------------------------------

class Bracket final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Bracket)
    DECLARE_CLASSOF(ElementType::BRACKET)

public:

    ~Bracket();

    Bracket *clone() const override { return new Bracket(*this); }

    void scanElements(std::function<void(EngravingItem *)> func) override;

    void setBracketItem(BracketItem *i) { bi_ = i; }

    BracketItem *bracketItem() const { return bi_; }

    BracketType bracketType() const { return bi_->bracketType(); }

    int subtype() const override { return int(bracketType()); }

    TranslatableString subtypeUserName() const override;

    size_t firstStaff() const { return firstStaff_; }

    size_t lastStaff() const { return lastStaff_; }

    void setStaffSpan(size_t a, size_t b);

    InstrumentNameType labelType() const { return labelType_; }

    void setLabelType(InstrumentNameType t) { labelType_ = t; }

    SymId braceSymbol() const { return braceSymbol_; }

    void setBraceSymbol(const SymId &sym) { braceSymbol_ = sym; }

    size_t column() const { return bi_->column(); }

    size_t span() const { return bi_->bracketSpan(); }

    double magx() const { return magx_; }

    System *system() const { return (System *) ownershipParent(); }

    Measure *measure() const { return measure_; }

    void setMeasure(Measure *measure) { measure_ = measure; }

    const BracketItem *bi() const { return bi_; }

    Color color() const override { return bi_->color(); }

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    void undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    using EngravingObject::undoChangeProperty;

    Fraction tick() const override;

    void setSelected(bool f) override;

    Text *text() const { return text_; }

    void setText(Text *t) { text_ = t; }

    bool intersects(const Bracket *other) const;
    bool contains(staff_idx_t staffIdx) const;

    struct LayoutData : public EngravingItem::LayoutData {
        SymId braceSymbol = SymId::noSym;
        PainterPath path;
        Shape shape;
        ld_field<double> bracketHeight = {"[Bracket] bracketHeight", 0.0};
        ld_field<double> bracketWidth = {"[Bracket] bracketWidth", 0.0};

        double h2(LD_ACCESS mode = LD_ACCESS::CHECK) const { return bracketHeight.value(mode) * 0.5; }
    };
    DECLARE_LAYOUTDATA_METHODS(Bracket)

private:
    friend class Factory;

    Bracket(DummyParentOr<EngravingItem> parent);

    BracketItem *bi_ = nullptr;
    double ay1_ = 0.0;

    size_t firstStaff_ = 0;
    size_t lastStaff_ = 0;

    InstrumentNameType labelType_;

    SymId braceSymbol_ = SymId::noSym;

    // horizontal scaling factor for brace symbol. Cannot be equal to magY or depend on h
    // because layout needs width of brace before knowing height of system...
    double magx_ = 0.0;
    Measure *measure_ = nullptr;

    Text *text_ = nullptr;
};
} // namespace mu::engraving
