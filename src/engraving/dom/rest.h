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

#include "containers.h"

#include "chordrest.h"
#include "notedot.h"

namespace mu::engraving {
class TDuration;
class Transaction;

struct RestVerticalClearance {
public:
    void reset() {
        above_ = 1000; // arbitrary high value
        below_ = 1000; // arbitrary high value
        locked_ = false;
    }

    int above() const { return above_; }

    int below() const { return below_; }

    void setAbove(int v) { above_ = std::max(std::min(above_, v), 0); }

    void setBelow(int v) { below_ = std::max(std::min(below_, v), 0); }

    bool locked() const { return locked_; }

    void setLocked(bool v) { locked_ = v; }

private:
    int above_ = 0.0; // In space units
    int below_ = 0.0; // In space units
    bool locked_ = false;
};

//---------------------------------------------------------
//    @@ Rest
///     This class implements a rest.
//---------------------------------------------------------

class Rest : public ChordRest {
    OBJECT_ALLOCATOR(engraving, Rest)
    DECLARE_CLASSOF(ElementType::REST)

public:

    ~Rest() { muse::DeleteAll(dots_); }

    void hack_toRestType();

    Rest &operator=(const Rest &) = delete;

    Rest *clone() const override { return new Rest(*this, false); }

    EngravingItem *linkedClone() override { return new Rest(*this, true); }

    Measure *measure() const override { return ownershipParent() ? toMeasure(ownershipParent()->ownershipParent()) : 0; }

    double mag() const override;
    double intrinsicMag() const override;

    void scanElements(std::function<void(EngravingItem *)> func) override;
    void setTrack(track_idx_t val) override;

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    bool isGap() const { return gap_; }

    virtual void setGap(bool v) { gap_ = v; }

    virtual void add(EngravingItem *) override;
    virtual void remove(EngravingItem *) override;

    void checkDots();

    NoteDot *dot(int n);
    const std::vector<NoteDot *> &dotList() const;

    int dotLine() const { return dotline_; }

    void setDotLine(int l) { dotline_ = l; }

    static int getDotline(DurationType durationType);
    bool accent();
    void setAccent(bool flag);

    bool isWholeRest() const;
    bool isBreveRest() const;

    DeadSlapped *deadSlapped() const { return deadSlapped_; }

    double rightEdge() const override;
    double centerX() const;

    bool alignWithOtherRests() const { return alignWithOtherRests_; }

    void setAlignWithOtherRests(bool v) { alignWithOtherRests_ = v; }

    void localSpatiumChanged(double oldValue, double newValue) override;
    PropertyValue propertyDefault(Pid) const override;
    void resetProperty(Pid id) override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;
    PropertyValue getProperty(Pid propertyId) const override;
    void undoChangeDotsVisible(bool v);

    EngravingItem *nextElement() override;
    EngravingItem *prevElement() override;

    bool shouldNotBeDrawn() const;
    bool debugDrawGap() const;

    RestVerticalClearance &verticalClearance() { return verticalClearance_; }

    struct LayoutData : public ChordRest::LayoutData {
        std::vector<Rest *> mergedRests; // Rests from other voices that may be merged with this
        ld_field<SymId> sym = {"[Rest] sym", SymId::restQuarter};
    };
    DECLARE_LAYOUTDATA_METHODS(Rest)

    SymId getSymbol(DurationType type, int line, int lines) const;
    double symWidthNoLedgerLines(LayoutData *ldata) const;

protected:
    Rest(const ElementType &type, DummyParentOr<Segment> parent);
    Rest(const ElementType &type, DummyParentOr<Segment> parent, const TDuration &);
    Rest(const Rest &, bool link = false);

    Sid getPropertyStyle(Pid pid) const override;

    virtual RectF numberRect() const { return RectF(); } // TODO: add style to show number over 1-measure rests

private:

    friend class Factory;
    Rest(DummyParentOr<Segment> parent);
    Rest(DummyParentOr<Segment> parent, const TDuration &);

    double upPos() const override;
    double downPos() const override;
    void setOffset(const PointF &o) override;

    // values calculated by layout:

    int dotline_ = -1; // depends on rest symbol
    bool gap_ = false; // invisible and not selectable for user
    std::vector<NoteDot *> dots_;
    DeadSlapped *deadSlapped_ = nullptr;

    RestVerticalClearance verticalClearance_;

    bool alignWithOtherRests_ = true;
};
} // namespace mu::engraving
