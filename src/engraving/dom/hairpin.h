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

#include "../types/types.h"

#include "textlinebase.h"

namespace mu::engraving {
class Hairpin;
class Transaction;

enum class HairpinType : signed char { INVALID = -1, CRESC_HAIRPIN, DIM_HAIRPIN, CRESC_LINE, DIM_LINE };

//---------------------------------------------------------
//   @@ HairpinSegment
//---------------------------------------------------------

class HairpinSegment final : public TextLineBaseSegment {
    OBJECT_ALLOCATOR(engraving, HairpinSegment)
    DECLARE_CLASSOF(ElementType::HAIRPIN_SEGMENT)

public:
    HairpinSegment(Hairpin *sp);

    HairpinSegment *clone() const override { return new HairpinSegment(*this); }

    Hairpin *hairpin() const { return (Hairpin *) spanner(); }

    bool drawCircledTip() const { return drawCircledTip_; }

    void setDrawCircledTip(bool arg) { drawCircledTip_ = arg; }

    double circledTipRadius() const { return circledTipRadius_; }

    void setCircledTipRadius(double r) { circledTipRadius_ = r; }

    PointF circledTip() const { return circledTip_; }

    void setCircledTip(const PointF &p) { circledTip_ = p; }

    EngravingObject *propertyDelegate(Pid) const override;

    bool hasVoiceAssignmentProperties() const override { return spanner()->hasVoiceAssignmentProperties(); }

    EngravingItem *findElementToSnapBefore(bool ignoreInvisible = true) const;
    EngravingItem *findElementToSnapAfter(bool ignoreInvisible = true, bool requirePlayable = false) const;

private:
    TextBase *findStartDynamicOrExpression(bool ignoreInvisible = true) const;
    TextBase *findEndDynamicOrExpression(bool ignoreInvisible = true, bool requirePlayable = false) const;

    Sid getPropertyStyle(Pid) const override;

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void setPropertyFlags(Pid id, PropertyFlags f) override;

    bool drawCircledTip_ = false;
    PointF circledTip_;
    double circledTipRadius_ = 0.0;
};

//---------------------------------------------------------
//   @@ Hairpin
//   @P hairpinType  enum (Hairpin.CRESCENDO, Hairpin.DIMINUENDO)
//   @P veloChange   int
//---------------------------------------------------------

class Hairpin final : public TextLineBase {
    OBJECT_ALLOCATOR(engraving, Hairpin)
    DECLARE_CLASSOF(ElementType::HAIRPIN)

public:
    Hairpin(DummyParentOr<EngravingItem> parent);

    Hairpin *clone() const override { return new Hairpin(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    DynamicType dynamicTypeFrom() const;
    DynamicType dynamicTypeTo() const;

    const Dynamic *dynamicSnappedBefore() const;
    const Dynamic *dynamicSnappedAfter() const;

    HairpinType hairpinType() const { return hairpinType_; }

    void setHairpinType(HairpinType val);

    LineSegment *createLineSegment() override;

    bool hairpinCircledTip() const { return hairpinCircledTip_; }

    void setHairpinCircledTip(bool val) { hairpinCircledTip_ = val; }

    int veloChange() const { return veloChange_; }

    void setVeloChange(int v) { veloChange_ = v; }

    Spatium hairpinHeight() const { return hairpinHeight_; }

    void setHairpinHeight(Spatium val) { hairpinHeight_ = val; }

    Spatium hairpinContHeight() const { return hairpinContHeight_; }

    void setHairpinContHeight(Spatium val) { hairpinContHeight_ = val; }

    bool singleNoteDynamics() const { return singleNoteDynamics_; }

    void setSingleNoteDynamics(bool val) { singleNoteDynamics_ = val; }

    ChangeMethod veloChangeMethod() const { return veloChangeMethod_; }

    void setVeloChangeMethod(ChangeMethod val) { veloChangeMethod_ = val; }

    bool isCrescendo() const { return hairpinType_ == HairpinType::CRESC_HAIRPIN || hairpinType_ == HairpinType::CRESC_LINE; }

    bool isDecrescendo() const { return hairpinType_ == HairpinType::DIM_HAIRPIN || hairpinType_ == HairpinType::DIM_LINE; }

    PropertyValue getProperty(Pid id) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    bool isLineType() const { return hairpinType_ == HairpinType::CRESC_LINE || hairpinType_ == HairpinType::DIM_LINE; }

    PointF linePos(Grip grip, System **system) const override;

    bool hasVoiceAssignmentProperties() const override { return true; }

    void reset() override;

    void setVoiceAssignment(VoiceAssignment v) { voiceAssignment_ = v; }

    VoiceAssignment voiceAssignment() const { return voiceAssignment_; }

    void setDirection(DirectionV v) { direction_ = v; }

    DirectionV direction() const { return direction_; }

    void setCenterBetweenStaves(AutoOnOff v) { centerBetweenStaves_ = v; }

    AutoOnOff centerBetweenStaves() const { return centerBetweenStaves_; }

    bool snapToItemBefore() const { return snapToItemBefore_; }

    void setSnapToItemBefore(bool v) { snapToItemBefore_ = v; }

    bool snapToItemAfter() const { return snapToItemAfter_; }

    void setSnapToItemAfter(bool v) { snapToItemAfter_ = v; }

    int subtype() const override { return int(hairpinType_); }

    TranslatableString subtypeUserName() const override;

protected:
    Sid defaultPosSid() const override;

private:

    Sid getPropertyStyle(Pid) const override;

    HairpinType hairpinType_ = HairpinType::INVALID;
    int veloChange_ = 0;
    bool hairpinCircledTip_ = false;
    bool singleNoteDynamics_ = false;
    ChangeMethod veloChangeMethod_ = ChangeMethod::NORMAL;

    Spatium hairpinHeight_;
    Spatium hairpinContHeight_;

    VoiceAssignment voiceAssignment_ = VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
    DirectionV direction_ = DirectionV::AUTO;
    AutoOnOff centerBetweenStaves_ = AutoOnOff::AUTO;

    bool snapToItemBefore_ = true;
    bool snapToItemAfter_ = true;
};
} // namespace mu::engraving
