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

#include "textbase.h"

namespace muse::draw {
class Painter;
} // namespace muse::draw

namespace mu::engraving {
class Transaction;
class Measure;
class Segment;

struct Dyn {
    DynamicType type = DynamicType::OTHER;
    int velocity = -1; // associated midi velocity (0-127, -1 = none)
    int changeInVelocity = 0;
    bool accent = 0;            // if true add velocity to current chord velocity
    const char *text = nullptr; // utf8 text of dynamic
};

//-----------------------------------------------------------------------------
//   @@ Dynamic
///    dynamics marker; determines midi velocity
//
//   @P range  enum (Dynamic.STAFF, .PART, .SYSTEM)
//-----------------------------------------------------------------------------

class Dynamic final : public TextBase {
    OBJECT_ALLOCATOR(engraving, Dynamic)
    DECLARE_CLASSOF(ElementType::DYNAMIC)

public:
    struct ChangeSpeedItem {
        DynamicSpeed speed = DynamicSpeed::NORMAL;
        const char *name = nullptr;
    };

    Dynamic(DummyParentOr<Segment> parent);
    Dynamic(const Dynamic &);

    Dynamic *clone() const override { return new Dynamic(*this); }

    Segment *segment() const { return (Segment *) ownershipParent(); }

    Measure *measure() const { return (Measure *) ownershipParent()->ownershipParent(); }

    void setDynamicType(DynamicType val) { dynamicType_ = val; }

    void setDynamicType(const String &);

    DynamicType dynamicType() const { return dynamicType_; }

    int subtype() const override { return static_cast<int>(dynamicType_); }

    TranslatableString subtypeUserName() const override;

    void reset() override;

    void setVelocity(int v) { velocity_ = v; }

    int velocity() const;

    int changeInVelocity() const;
    void setChangeInVelocity(int val);
    Fraction velocityChangeLength(BeatsPerSecond tempo) const;
    bool isVelocityChangeAvailable() const;

    DynamicSpeed velChangeSpeed() const { return velChangeSpeed_; }

    void setVelChangeSpeed(DynamicSpeed val) { velChangeSpeed_ = val; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;
    Sid getPropertyStyle(Pid) const override;

    static String dynamicText(DynamicType t);

    bool hasCustomText() const { return dynamicText(dynamicType_) != xmlText(); }

    Expression *snappedExpression() const;
    HairpinSegment *findSnapBeforeHairpinAcrossSystemBreak() const;

    bool playDynamic() const { return playDynamic_; }

    void setPlayDynamic(bool v) { playDynamic_ = v; }

    bool acceptDrop(EditData &ed) const override;
    EngravingItem *drop(Transaction &tx, EditData &ed) override;

    static int dynamicVelocity(DynamicType t);

    static const std::vector<Dyn> &dynamicList() { return DYN_LIST; }

    bool hasVoiceAssignmentProperties() const override { return true; }

    bool positionRelativeToNoteheadRest() const override { return true; }

    bool sizeIsSpatiumDependent() const override { return true; }

    Hairpin *leftHairpin() const { return leftHairpin_; }

    Hairpin *rightHairpin() const { return rightHairpin_; }

    bool hasLeftGrip() const;
    bool hasRightGrip() const;

    void resetLeftDragOffset() { leftDragOffset_ = 0.0; }

    void resetRightDragOffset() { rightDragOffset_ = 0.0; }

    double leftDragOffset() const { return leftDragOffset_; }

    double rightDragOffset() const { return rightDragOffset_; }

    void findAdjacentHairpins();

    Shape symShapeWithCutouts(SymId id) const override;

    static Dyn dynInfo(DynamicType type);

private:

    M_PROPERTY(bool, avoidBarLines, setAvoidBarLines)
    M_PROPERTY(bool, centerOnNotehead, setCenterOnNotehead)

    std::pair<DynamicType, String> parseDynamicText(const String &) const;
    DynamicType dynamicType_ = DynamicType::OTHER;
    bool playDynamic_ = true;

    mutable PointF dragOffset_;
    int velocity_ = -1; // associated midi velocity 0-127

    int changeInVelocity_ = 128;
    DynamicSpeed velChangeSpeed_ = DynamicSpeed::NORMAL;

    static const std::vector<Dyn> DYN_LIST;

    double leftDragOffset_ = 0.0;
    double rightDragOffset_ = 0.0;

    Hairpin *leftHairpin_ = nullptr;
    Hairpin *rightHairpin_ = nullptr;
};
} // namespace mu::engraving
