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

#include "../editing/elementeditdata.h"

namespace mu::engraving {
class Factory;
class Segment;
class Transaction;

static constexpr int MIN_BARLINE_FROMTO_DIST = 2;
static constexpr int MIN_BARLINE_SPAN_FROMTO = -2;

// bar line span for 1-line staves is special: goes from 2sp above the line to 2sp below the line;
static constexpr int BARLINE_SPAN_1LINESTAFF_FROM = -4;
static constexpr int BARLINE_SPAN_1LINESTAFF_TO = 4;

// data for some preset bar line span types
static constexpr int BARLINE_SPAN_TICK1_FROM = -1;
static constexpr int BARLINE_SPAN_TICK1_TO = -7;
static constexpr int BARLINE_SPAN_TICK2_FROM = -2;
static constexpr int BARLINE_SPAN_TICK2_TO = -6;
static constexpr int BARLINE_SPAN_SHORT1_FROM = 2;
static constexpr int BARLINE_SPAN_SHORT1_TO = -2;
static constexpr int BARLINE_SPAN_SHORT2_FROM = 1;
static constexpr int BARLINE_SPAN_SHORT2_TO = -1;

//---------------------------------------------------------
//   BarLineTableItem
//---------------------------------------------------------

struct BarLineTableItem {
    BarLineType type;
    const muse::TranslatableString &userName;
};

//---------------------------------------------------------
//   @@ BarLine
//
//   @P barLineType  enum  (BarLineType.NORMAL, .DOUBLE, .START_REPEAT, .END_REPEAT, .BROKEN, .END, .END_START_REPEAT, .DOTTED)
//---------------------------------------------------------

class BarLine final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, BarLine)
    DECLARE_CLASSOF(ElementType::BAR_LINE)

public:
    ~BarLine() override;

    BarLine &operator=(const BarLine &) = delete;

    void setOwnershipParent(DummyParentOr<Segment> parent);

    BarLine *clone() const override { return new BarLine(*this); }

    PointF canvasPos() const override; ///< position in canvas coordinates
    PointF pagePos() const override;   ///< position in page coordinates

    void scanElements(std::function<void(EngravingItem *)> func) override;
    void setTrack(track_idx_t t) override;
    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    Segment *segment() const { return toSegment(ownershipParent()); }

    Measure *measure() const { return ownershipParent() ? toMeasure(ownershipParent()->ownershipParent()) : nullptr; }

    void setSpanStaff(const bool val) { spanStaff_ = val; }

    void setSpanFrom(int val) { spanFrom_ = val; }

    void setSpanTo(int val) { spanTo_ = val; }

    void setShowTips(bool val);

    bool spanStaff() const { return spanStaff_; }

    int spanFrom() const { return spanFrom_; }

    int spanTo() const { return spanTo_; }

    bool showTips() const;

    const ElementList *el() const { return &el_; }

    static String translatedUserTypeName(BarLineType);

    void setBarLineType(BarLineType i) { barLineType_ = i; }

    BarLineType barLineType() const { return barLineType_; }

    bool isTop() const;
    bool isBottom() const;

    int subtype() const override { return int(barLineType_); }

    TranslatableString subtypeUserName() const override;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid propertyId) const override;
    void undoChangeProperty(Pid id, const PropertyValue &, PropertyFlags ps) override;
    using EngravingObject::undoChangeProperty;
    EngravingObject *propertyDelegate(Pid) const override;

    void setPlayCount(int playCount) { playCount_ = playCount; }

    int playCount() const { return playCount_; }

    PlayCountText *playCountText() const;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    void setSelected(bool f) override;

    void styleChanged() override;

    static const std::vector<BarLineTableItem> barLineTable;

    void calcY();

    struct LayoutData : public EngravingItem::LayoutData {
        double y1 = 0.0;
        double y2 = 0.0;
    };

    DECLARE_LAYOUTDATA_METHODS(BarLine)

private:

    friend class Factory;
    BarLine(DummyParentOr<Segment> parent);
    BarLine(const BarLine &);

    bool spanStaff_ = false; // span barline to next staff if true
    int spanFrom_ = 0;       // line number on start and end staves
    int spanTo_ = 0;
    BarLineType barLineType_ = BarLineType::NORMAL;

    ElementList el_; ///< fermata or other articulations

    int playCount_ = -1; // For use during copy & paste
};
} // namespace mu::engraving
