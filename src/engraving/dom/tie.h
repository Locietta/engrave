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

#include "slurtie.h"

namespace mu::engraving {
class TieJumpPointList;
class TieJumpPoint;
//---------------------------------------------------------
//   @@ TieSegment
///    a single segment of a tie
//---------------------------------------------------------

class TieSegment : public SlurTieSegment {
    OBJECT_ALLOCATOR(engraving, TieSegment)
    DECLARE_CLASSOF(ElementType::TIE_SEGMENT)

public:
    TieSegment(Tie *sp);
    TieSegment(const TieSegment &s);

    TieSegment *clone() const override { return new TieSegment(*this); }

    void addAdjustmentOffset(const PointF &offset, Grip grip) { adjustmentOffsets_[static_cast<size_t>(grip)] += offset; }

    void resetAdjustmentOffset() { adjustmentOffsets_.fill(PointF()); }

    PointF adjustmentOffset(Grip grip) { return adjustmentOffsets_[static_cast<size_t>(grip)]; }

    void consolidateAdjustmentOffsetIntoUserOffset();

    bool isEdited() const;

    Tie *tie() const { return (Tie *) spanner(); }

    void setStaffMove(int val) { staffMove_ = val; }

    staff_idx_t vStaffIdx() const override { return staffIdx() + staffMove_; }

    virtual double minShoulderHeight() const;
    virtual double maxShoulderHeight() const;
    double endWidth() const override;
    double midWidth() const override;
    double dottedWidth() const override;

    struct LayoutData : public SlurTieSegment::LayoutData {
        bool allJumpPointsInactive = false;
    };
    DECLARE_LAYOUTDATA_METHODS(TieSegment)

protected:
    TieSegment(const ElementType &type, Tie *sp);

private:
    int staffMove_ = 0;
    std::array<PointF, static_cast<size_t>(Grip::GRIPS)> adjustmentOffsets_;
};

//---------------------------------------------------------
//   @@ Tie
//!    a Tie has a Note as startElement/endElement
//---------------------------------------------------------

class Tie : public SlurTie {
    OBJECT_ALLOCATOR(engraving, Tie)
    DECLARE_CLASSOF(ElementType::TIE)

public:
    Tie(EngravingObject *parent = 0);
    Tie(const Tie &t);

    Tie *clone() const override { return new Tie(*this); }

    Anchor anchor() const override { return Anchor::NOTE; }

    virtual ~Tie() {}

    virtual Note *startNote() const;
    virtual void setStartNote(Note *note);
    virtual Note *endNote() const;

    virtual void setEndNote(Note *note) { setEndElement((EngravingItem *) note); }

    bool isInside() const { return isInside_; }

    void setIsInside(bool val) { isInside_ = val; }

    virtual bool isOuterTieOfChord(Grip startOrEnd) const;
    bool hasTiedSecondInside() const;
    bool isCrossStaff() const;

    PropertyValue getProperty(Pid propertyId) const override;
    PropertyValue propertyDefault(Pid id) const override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;

    TieSegment *frontSegment() { return toTieSegment(Spanner::frontSegment()); }

    const TieSegment *frontSegment() const { return toTieSegment(Spanner::frontSegment()); }

    TieSegment *backSegment() { return toTieSegment(Spanner::backSegment()); }

    const TieSegment *backSegment() const { return toTieSegment(Spanner::backSegment()); }

    TieSegment *segmentAt(int n) { return toTieSegment(Spanner::segmentAt(n)); }

    const TieSegment *segmentAt(int n) const { return toTieSegment(Spanner::segmentAt(n)); }

    SlurTieSegment *newSlurTieSegment() override { return new TieSegment(this); }

    double scalingFactor() const override;

    const TiePlacement &tiePlacement() const { return tiePlacement_; }

    void setTiePlacement(const TiePlacement &val) { tiePlacement_ = val; }

    // Outgoing ties before repeats
    void updatePossibleJumpPoints();
    void addTiesToJumpPoints();
    void undoRemoveTiesFromJumpPoints();
    virtual bool allJumpPointsInactive() const;
    virtual TieJumpPointList *tieJumpPoints();
    virtual const TieJumpPointList *tieJumpPoints() const;

    // Incoming ties after repeats
    void setJumpPoint(TieJumpPoint *jumpPoint) { jumpPoint_ = jumpPoint; }

    void updateStartTieOnRemoval();

    TieJumpPoint *jumpPoint() const { return jumpPoint_; }

    Tie *startTie() const;

    static void changeTieType(Tie *oldTie, Note *endNote = nullptr);

protected:
    Tie(const ElementType &type, EngravingObject *parent = nullptr);

    bool isInSpannerMap() const override { return false; }

    bool isInside_ = false;
    TiePlacement tiePlacement_ = TiePlacement::AUTO;

    // Jump point information for incoming ties after repeats
    TieJumpPoint *jumpPoint_ = nullptr;
    TieJumpPointList *startTieJumpPoints() const;
};
} // namespace mu::engraving
