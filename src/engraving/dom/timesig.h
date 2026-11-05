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

#include "groups.h"

namespace mu::engraving {
class Segment;
class Transaction;

//---------------------------------------------------------
//   TimeSigType
//---------------------------------------------------------

enum class TimeSigType : unsigned char {
    NORMAL,     // use sz/sn text
    FOUR_FOUR,  // common time (4/4)
    ALLA_BREVE, // cut time (2/2)
    CUT_BACH,   // cut time (Bach)
    CUT_TRIPLE, // cut triple time (9/8)
};

//---------------------------------------------------------------------------------------
//   @@ TimeSig
///    This class represents a time signature.
//---------------------------------------------------------------------------------------

class TimeSig final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, TimeSig)
    DECLARE_CLASSOF(ElementType::TIMESIG)

    M_PROPERTY2(bool, isCourtesy, setIsCourtesy, false)

public:

    void setOwnershipParent(DummyParentOr<Segment> parent);

    String ssig() const;
    void setSSig(const String &);

    TimeSig *clone() const override { return new TimeSig(*this); }

    TimeSigType timeSigType() const { return timeSigType_; }

    int subtype() const override;
    TranslatableString subtypeUserName() const override;

    bool operator==(const TimeSig &) const;

    bool operator!=(const TimeSig &ts) const { return !(*this == ts); }

    double mag() const override;

    Fraction sig() const { return sig_; }

    void setSig(const Fraction &f, TimeSigType st = TimeSigType::NORMAL);

    int numerator() const { return sig_.numerator(); }

    int denominator() const { return sig_.denominator(); }

    Fraction stretch() const { return stretch_; }

    void setStretch(const Fraction &s) { stretch_ = s; }

    int numeratorStretch() const { return stretch_.numerator(); }

    int denominatorStretch() const { return stretch_.denominator(); }

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    Segment *segment() const { return (Segment *) ownershipParent(); }

    Measure *measure() const { return (Measure *) ownershipParent()->ownershipParent(); }

    bool showCourtesySig() const { return showCourtesySig_; }

    void setShowCourtesySig(bool v) { showCourtesySig_ = v; }

    const String &numeratorString() const { return numeratorString_; }

    void setNumeratorString(const String &);

    const String &denominatorString() const { return denominatorString_; }

    void setDenominatorString(const String &);

    bool largeParentheses() const { return largeParentheses_; }

    void setLargeParentheses(bool v) { largeParentheses_ = v; }

    void setFrom(const TimeSig *);

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    const Groups &groups() const { return groups_; }

    void setGroups(const Groups &e) { groups_ = e; }

    bool isLocal() const { return stretch_ != Fraction(1, 1); }

    PointF staffOffset() const override;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    void initElementStyle(const ElementStyle *) override;
    void styleChanged() override;
    Sid getPropertyStyle(Pid id) const override;

    bool showOnThisStaff() const;
    bool isAboveStaves() const;
    bool isAcrossStaves() const;
    TimeSigPlacement timeSigPlacement() const;
    TimeSigStyle timeSigStyle() const;
    double numDist() const;
    double yPos() const;

    const ScaleF &scale() const { return scale_; }

    void setScale(const ScaleF &s) { scale_ = s; } // TODO: think about what to do with this

    struct LayoutData : public EngravingItem::LayoutData {
        SymIdList ns;
        SymIdList ds;
        PointF pz;
        PointF pn;
        PointF pointLargeLeftParen;
        PointF pointLargeRightParen;
    };
    DECLARE_LAYOUTDATA_METHODS(TimeSig)

protected:
    void added() override;
    void removed() override;

private:

    friend class Factory;
    TimeSig(DummyParentOr<Segment> parent);

    String numeratorString_; // calculated from actualSig() if !customText
    String denominatorString_;

    Fraction sig_;
    Fraction stretch_; // localSig / globalSig
    Groups groups_;

    ScaleF scale_ = ScaleF(1.0, 1.0);
    TimeSigType timeSigType_ = TimeSigType::NORMAL;
    bool showCourtesySig_ = false;
    bool largeParentheses_ = false;
};
} // namespace mu::engraving
