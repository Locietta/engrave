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

#ifndef MU_ENGRAVING_CLEF_H
#define MU_ENGRAVING_CLEF_H

/**
 \file
 Definition of classes Clef
*/

#include "engravingitem.h"

namespace mu::engraving {
class Transaction;
class Factory;
class Segment;

static const int NO_CLEF = -1000;

//---------------------------------------------------------
//   ClefTypeList
//---------------------------------------------------------

struct ClefTypeList {
    ClefType concertClef = ClefType::G;
    ClefType transposingClef = ClefType::G;

    ClefTypeList() {}

    ClefTypeList(ClefType a, ClefType b) : concertClef(a), transposingClef(b) {}

    ClefTypeList(ClefType a) : concertClef(a), transposingClef(a) {}

    bool operator==(const ClefTypeList &t) const;
    bool operator!=(const ClefTypeList &t) const;
};

//---------------------------------------------------------
//   ClefInfo
///   Info about a clef.
//---------------------------------------------------------

struct ClefInfo {
    static const ClefInfo clefTable[];

    ClefType type_ = ClefType::INVALID;
    int line_ = 0;        ///< Line positioning on the staff
    int pitchOffset_ = 0; ///< Pitch offset for line 0.
    signed char lines_[14];
    SymId symId_ = SymId::noSym;
    StaffGroup staffGroup_ = StaffGroup::STANDARD;

    static int line(ClefType t) { return clefTable[int(t)].line_; }

    static int pitchOffset(ClefType t) { return clefTable[int(t)].pitchOffset_; }

    static SymId symId(ClefType t) { return clefTable[int(t)].symId_; }

    static const signed char *lines(ClefType t) { return clefTable[int(t)].lines_; }

    static StaffGroup staffGroup(ClefType t) { return clefTable[int(t)].staffGroup_; }
};

//---------------------------------------------------------
//   @@ Clef
///    Graphic representation of a clef.
//
//   @P showCourtesy  bool    show/hide courtesy clef when applicable
//   @P isSmall       bool    small, mid-staff clef (read only, set by layout)
//---------------------------------------------------------

class Clef final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Clef)
    DECLARE_CLASSOF(ElementType::CLEF)

public:

    Clef *clone() const override { return new Clef(*this); }

    double mag() const override;

    Segment *segment() const { return (Segment *) ownershipParent(); }

    Measure *measure() const { return (Measure *) ownershipParent()->ownershipParent(); }

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    bool isSmall() const { return isSmall_; }

    void setSmall(bool val);

    bool showCourtesy() const { return showCourtesy_; }

    void setShowCourtesy(bool v) { showCourtesy_ = v; }

    void undoSetShowCourtesy(bool v);
    Clef *otherClef();

    ClefType clefType() const;
    void setClefType(ClefType i);

    bool isCourtesy() const { return isCourtesy_; }

    void setIsCourtesy(const bool v) { isCourtesy_ = v; }

    int subtype() const override { return int(clefType()); }

    TranslatableString subtypeUserName() const override;

    void setForInstrumentChange(bool forInstrumentChange) { forInstrumentChange_ = forInstrumentChange; }

    bool forInstrumentChange() const { return forInstrumentChange_; }

    ClefTypeList clefTypeList() const { return clefTypes_; }

    ClefType concertClef() const { return clefTypes_.concertClef; }

    ClefType transposingClef() const { return clefTypes_.transposingClef; }

    void setConcertClef(ClefType val);
    void setTransposingClef(ClefType val);

    void setClefType(const ClefTypeList &ctl) { clefTypes_ = ctl; }

    void spatiumChanged(double oldValue, double newValue) override;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    void clear();

    void changeClefToBarlinePos(ClefToBarlinePosition newPos);

    void undoChangeProperty(Pid id, const PropertyValue &v) { EngravingObject::undoChangeProperty(id, v); }

    void undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) override;

    ClefToBarlinePosition clefToBarlinePosition() const { return clefToBarlinePosition_; }

    void setClefToBarlinePosition(ClefToBarlinePosition val) { clefToBarlinePosition_ = val; }

    bool isHeader() const;

    bool isTrailer() const { return isTrailer_; }

    void setIsTrailer(bool val) { isTrailer_ = val; }

    bool isMidMeasureClef() const;

    bool canBeExcludedFromOtherParts() const override { return !isHeader(); }

    void manageExclusionFromParts(bool exclude) override;

    struct LayoutData : public EngravingItem::LayoutData {
        SymId symId = SymId::noSym;
    };
    DECLARE_LAYOUTDATA_METHODS(Clef)

private:

    friend class Factory;
    Clef(DummyParentOr<Segment> parent);

    bool showCourtesy_ = true;
    bool isSmall_ = false;
    bool forInstrumentChange_ = false;
    bool isCourtesy_ = false;
    bool isTrailer_ = false;
    ClefToBarlinePosition clefToBarlinePosition_ = ClefToBarlinePosition::AUTO;
    ClefTypeList clefTypes_ = ClefType::INVALID;
};
} // namespace mu::engraving
#endif
