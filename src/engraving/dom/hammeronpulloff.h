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

#include "slur.h"
#include "textbase.h"

namespace mu::engraving {
class HammerOnPullOffText final : public TextBase {
    OBJECT_ALLOCATOR(engraving, HammerOnPullOffText)
    DECLARE_CLASSOF(ElementType::HAMMER_ON_PULL_OFF_TEXT)

public:
    HammerOnPullOffText(HammerOnPullOffSegment *parent = nullptr);
    HammerOnPullOffText(const HammerOnPullOffText &h);

    HammerOnPullOffText *clone() const override { return new HammerOnPullOffText(*this); }

    HammerOnPullOffSegment *hopoSegment() const { return toHammerOnPullOffSegment(ownershipParent()); }

    Chord *startChord() const { return startChord_; }

    Chord *endChord() const { return endChord_; }

    void setStartChord(Chord *c) { startChord_ = c; }

    void setEndChord(Chord *c) { endChord_ = c; }

    bool isUserModified() const override;

    bool isValid() const { return isValid_; }

    void setIsValid(bool v) { isValid_ = v; }

    bool isHammerOn() const { return isHammerOn_; }

    void setIsHammerOn(bool v) { isHammerOn_ = v; }

    Color curColor(const rendering::PaintOptions &opt) const override;

    PropertyValue propertyDefault(Pid id) const override;

    bool positionRelativeToNoteheadRest() const override { return false; }

private:
    Chord *startChord_ = nullptr;
    Chord *endChord_ = nullptr;
    bool isValid_ = true;
    bool isHammerOn_ = true;
};

class HammerOnPullOffSegment final : public SlurSegment {
    OBJECT_ALLOCATOR(engraving, HammerOnPullOffSegment)
    DECLARE_CLASSOF(ElementType::HAMMER_ON_PULL_OFF_SEGMENT)

public:
    HammerOnPullOffSegment(HammerOnPullOff *sp);
    HammerOnPullOffSegment(const HammerOnPullOffSegment &other);

    HammerOnPullOffSegment *clone() const override { return new HammerOnPullOffSegment(*this); }

    HammerOnPullOff *hammerOnPullOff() const { return toHammerOnPullOff(spanner()); }

    Color curColor(const rendering::PaintOptions &opt) const override;

    void scanElements(std::function<void(EngravingItem *)> func) override;

    void setTrack(track_idx_t idx) override;
    void setSelected(bool f) override;

    void updateHopoText();

    void addHopoText(HammerOnPullOffText *t) { hopoText_.push_back(t); }

    const std::vector<HammerOnPullOffText *> &hopoText() const { return hopoText_; }

    bool isUserModified() const override;
    bool isValid() const;

    void reset() override;

private:
    struct HopoTextRegion {
        Chord *startChord = nullptr;
        Chord *endChord = nullptr;
        bool isHammerOn = true;
        bool isValid = true;
    };

    std::vector<HopoTextRegion> computeHopoTextRegions(Chord *startChord, Chord *endChord);
    void resolveStartEndNotes(Note **startNote, Note **endNote, Chord *startChord, Chord *endChord, bool isTabStaff);

    std::vector<HammerOnPullOffText *> hopoText_;
};

class HammerOnPullOff final : public Slur {
    OBJECT_ALLOCATOR(engraving, HammerOnPullOff)
    DECLARE_CLASSOF(ElementType::HAMMER_ON_PULL_OFF)

public:
    HammerOnPullOff *clone() const override { return new HammerOnPullOff(*this); }

    SlurTieSegment *newSlurTieSegment() override { return new HammerOnPullOffSegment(this); }

    friend class Factory;

    HammerOnPullOff(DummyParentOr<EngravingItem> parent);
    HammerOnPullOff(const HammerOnPullOff &);
};
} // namespace mu::engraving
