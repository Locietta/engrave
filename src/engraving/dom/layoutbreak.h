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

    void setOwnershipParent(DummyParentOr<MeasureBase> parent);

    LayoutBreak *clone() const override { return new LayoutBreak(*this); }

    int subtype() const override { return static_cast<int>(layoutBreakType_); }

    TranslatableString subtypeUserName() const override;

    void setLayoutBreakType(LayoutBreakType);

    LayoutBreakType layoutBreakType() const { return layoutBreakType_; }

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    MeasureBase *measure() const { return (MeasureBase *) ownershipParent(); }

    double pause() const { return pause_; }

    void setPause(double v) { pause_ = v; }

    bool startWithLongNames() const { return startWithLongNames_; }

    void setStartWithLongNames(bool v) { startWithLongNames_ = v; }

    bool startWithMeasureOne() const { return startWithMeasureOne_; }

    void setStartWithMeasureOne(bool v) { startWithMeasureOne_ = v; }

    bool firstSystemIndentation() const { return firstSystemIndentation_; }

    void setFirstSystemIndentation(bool v) { firstSystemIndentation_ = v; }

    bool showCourtesy() const { return showCourtesy_; }

    void setShowCourtesy(bool v) { showCourtesy_ = v; }

    bool isPageBreak() const { return layoutBreakType_ == LayoutBreakType::PAGE; }

    bool isLineBreak() const { return layoutBreakType_ == LayoutBreakType::LINE; }

    bool isSectionBreak() const { return layoutBreakType_ == LayoutBreakType::SECTION; }

    bool isNoBreak() const { return layoutBreakType_ == LayoutBreakType::NOBREAK; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    char16_t iconCode() const;

    muse::draw::Font font() const;

private:

    friend class Factory;
    LayoutBreak(DummyParentOr<MeasureBase> parent);
    LayoutBreak(const LayoutBreak &);

    double pause_ = 0.0;
    bool startWithLongNames_ = false;
    bool startWithMeasureOne_ = false;
    bool firstSystemIndentation_ = false;
    bool showCourtesy_ = false;
    LayoutBreakType layoutBreakType_ = LayoutBreakType::NOBREAK;
};
} // namespace mu::engraving
