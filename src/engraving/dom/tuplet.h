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

#include <set>

#include "durationelement.h"
#include "property.h"
#include "../types/types.h"

namespace mu::engraving {
class Text;
class Spanner;

//------------------------------------------------------------------------
//   @@ Tuplet
//!     Example of 1/8 triplet:
//!       _baseLen     = 1/8  (tuplet is measured in eighth notes)
//!       _ratio       = 3/2  (3 eighth tuplet notes played in the space of 2 regular eighth notes)
//!
//!    Entire tuplet has a duration of _baseLen * _ratio.denominator().
//!    A single tuplet note has duration of _baseLen * _ratio.denominator() / _ratio.numerator().
//------------------------------------------------------------------------

class Tuplet final : public DurationElement {
    OBJECT_ALLOCATOR(engraving, Tuplet)
    DECLARE_CLASSOF(ElementType::TUPLET)

public:
    Tuplet(DummyParentOr<Measure> parent);
    Tuplet(const Tuplet &);
    ~Tuplet();

    void setOwnershipParent(DummyParentOr<Measure> parent);

    Tuplet *clone() const override { return new Tuplet(*this); }

    void setTrack(track_idx_t val) override;

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    Text *number() const { return number_; }

    void setNumber(Text *t) { number_ = t; }

    void resetNumberProperty();
    static void resetNumberProperty(Text *number);

    void setSelected(bool f) override;

    Measure *measure() const override { return toMeasure(ownershipParent()); }

    TupletNumberType numberType() const { return numberType_; }

    TupletBracketType bracketType() const { return bracketType_; }

    void setNumberType(TupletNumberType val) { numberType_ = val; }

    void setBracketType(TupletBracketType val) { bracketType_ = val; }

    bool hasBracket() const { return hasBracket_; }

    void setHasBracket(bool b) { hasBracket_ = b; }

    Spatium bracketWidth() const { return bracketWidth_; }

    void setBracketWidth(Spatium s) { bracketWidth_ = s; }

    const Fraction &ratio() const { return ratio_; }

    void setRatio(const Fraction &r) { ratio_ = r; }

    void setUserPoint1(PointF p) { userP1_ = p; }

    void setUserPoint2(PointF p) { userP2_ = p; }

    const std::vector<DurationElement *> &elements() const { return currentElements_; }

    void clear() { currentElements_.clear(); }

    bool contains(const DurationElement *el) const {
        return std::find(currentElements_.begin(), currentElements_.end(), el) != currentElements_.end();
    }

    void scanElements(std::function<void(EngravingItem *)> func) override;

    void reset() override;

    TDuration baseLen() const { return baseLen_; }

    void setBaseLen(const TDuration &d) { baseLen_ = d; }

    void dump() const override;

    void setDirection(DirectionV d) { direction_ = d; }

    DirectionV direction() const { return direction_; }

    bool isUp() const { return isUp_; }

    void setIsUp(bool val) { isUp_ = val; }

    bool isSmall() const { return isSmall_; }

    void setIsSmall(bool val) { isSmall_ = val; }

    Fraction tick() const override { return tick_; }

    Fraction rtick() const override;

    void setTick(const Fraction &v) { tick_ = v; }

    Fraction elementsDuration();
    void sortElements();
    bool cross() const;
    staff_idx_t vStaffIdx() const override;

    const PointF &p1() const { return p1_; }

    PointF &p1() { return p1_; }

    void setP1(const PointF &p) { p1_ = p; }

    const PointF &p2() const { return p2_; }

    PointF &p2() { return p2_; }

    void setP2(const PointF &p) { p2_ = p; }

    const PointF &userP1() const { return userP1_; }

    const PointF &userP2() const { return userP2_; }

    void setVisible(bool f) override;
    void setColor(const Color &col) override;

    EngravingObject *propertyDelegate(Pid id) const override;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;
    PropertyValue propertyDefault(Pid id) const override;

    void sanitizeTuplet();
    void addMissingElements();

    bool calcHasBracket(const DurationElement *cr1, const DurationElement *cr2) const;

    static int computeTupletDenominator(int numerator, Fraction totalDuration);

    PointF bracketL[4];
    PointF bracketR[3];

    EngravingItem *nextElement() override;
    EngravingItem *prevElement() override;

private:

    friend class DurationElement;

    void addDurationElement(DurationElement *de);
    void removeDurationElement(DurationElement *de);

    Fraction addMissingElement(const Fraction &startTick, const Fraction &endTick);

    // All DurationElements where `tuplet()` returns this tuplet
    std::set<DurationElement *> allElements_;

    // Those DurationElements that are currently really part of this tuplet
    std::vector<DurationElement *> currentElements_;

    bool beingDestructed_ = false;

    DirectionV direction_ = DirectionV::AUTO;
    TupletNumberType numberType_ = TupletNumberType::SHOW_NUMBER;
    TupletBracketType bracketType_ = TupletBracketType::AUTO_BRACKET;
    Spatium bracketWidth_;

    bool hasBracket_ = false;
    Fraction ratio_;
    TDuration baseLen_; // 1/8 for a triplet of 1/8

    bool isUp_ = true;
    bool isSmall_ = true;

    Fraction tick_;

    PointF p1_, p2_;
    PointF userP1_, userP2_; // user offset

    Text *number_ = nullptr;
};
} // namespace mu::engraving
