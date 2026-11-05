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

#include <functional>

#include "../types/types.h"

#include "durationelement.h"
#include "fermata.h"

namespace mu::engraving {
enum class CrossMeasure : signed char { UNKNOWN = -1, NONE = 0, FIRST, SECOND };

class Articulation;
class BeamBase;
class BeamSegment;
class Lyrics;
class Measure;
class Score;
class Segment;
class Slur;
class TabDurationSymbol;
class Transaction;
enum class SegmentType;

//-------------------------------------------------------------------
//   ChordRest
//    Virtual base class. Chords and rests can be part of a beam
//-------------------------------------------------------------------

class ChordRest : public DurationElement {
    OBJECT_ALLOCATOR(engraving, ChordRest)
    DECLARE_CLASSOF(ElementType::INVALID) // dummy

public:
    ChordRest(const ElementType &type, DummyParentOr<Segment> parent);
    ChordRest(const ChordRest &, bool link = false);
    ChordRest &operator=(const ChordRest &) = delete;
    ~ChordRest();

    virtual void scanElements(std::function<void(EngravingItem *)> func) override;

    bool acceptDrop(EditData &) const override;
    virtual EngravingItem *drop(Transaction &tx, EditData &) override;
    virtual void undoUnlink() override;

    virtual Segment *segment() const { return (Segment *) ownershipParent(); }

    void setBeamMode(BeamMode m) { beamMode_ = m; }

    BeamMode beamMode() const { return beamMode_; }

    void setBeam(Beam *b);
    void setBeamlet(BeamSegment *b);

    BeamSegment *beamlet() const { return beamlet_; }

    virtual Beam *beam() const final;

    int beams() const { return durationType_.hooks(); }

    virtual double upPos() const = 0;
    virtual double downPos() const = 0;

    virtual double rightEdge() const = 0;

    bool isSmall() const { return isSmall_; }

    void setSmall(bool val) { isSmall_ = val; }

    int staffMove() const { return staffMove_; }

    void setStaffMove(int val) { staffMove_ = val; }

    int storedStaffMove() const { return storedStaffMove_; }

    staff_idx_t vStaffIdx() const override { return staffIdx() + staffMove_; }

    void checkStaffMoveValidity();

    const TDuration durationType() const { return crossMeasure_ == CrossMeasure::FIRST ? crossMeasureTDur_ : durationType_; }

    const TDuration actualDurationType() const { return durationType_; }

    void setDurationType(DurationType t);
    void setDurationType(const Fraction &ticks);
    void setDurationType(TDuration v);

    void setDots(int n) { durationType_.setDots(n); }

    int dots() const {
        return crossMeasure_ == CrossMeasure::FIRST ? crossMeasureTDur_.dots() :
                                                      (crossMeasure_ == CrossMeasure::SECOND ? 0 : durationType_.dots());
    }

    int actualDots() const { return durationType_.dots(); }

    Fraction durationTypeTicks() const { return crossMeasure_ == CrossMeasure::FIRST ? crossMeasureTDur_.ticks() : durationType_.ticks(); }

    String durationUserName() const;

    void setTrack(track_idx_t val) override;

    const std::vector<Lyrics *> &lyrics() const { return lyrics_; }

    std::vector<Lyrics *> &lyrics() { return lyrics_; }

    Lyrics *lyrics(int verse) const;
    Lyrics *lyrics(int verse, PlacementV) const;
    int lastVerse(PlacementV) const;
    bool isMelismaEnd() const;
    void setMelismaEnd(bool v);

    virtual void add(EngravingItem *) override;
    virtual void remove(EngravingItem *) override;
    void removeDeleteBeam(bool beamed);
    void replaceBeam(Beam *newBeam);

    const ElementList &el() const { return el_; }

    //! TODO Look like a hack, see using
    void addFermata(Fermata *f) { addEl(f); }

    void removeFermata(Fermata *f) { removeEl(f); }

    //! --------------------------------

    Slur *slur(const ChordRest *secondChordRest = nullptr) const;

    CrossMeasure crossMeasure() const { return crossMeasure_; }

    void setCrossMeasure(CrossMeasure val) { crossMeasure_ = val; }

    // the following two functions should not be used, unless absolutely necessary;
    // the cross-measure duration is best managed through setDuration() and crossMeasureSetup()
    TDuration crossMeasureDurationType() const { return crossMeasureTDur_; }

    void setCrossMeasureDurationType(TDuration v) { crossMeasureTDur_ = v; }

    void undoChangeProperty(Pid id, const PropertyValue &newValue, PropertyFlags ps = PropertyFlags::NOSTYLE) override;
    void localSpatiumChanged(double oldValue, double newValue) override;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;
    bool isGrace() const;
    bool isGraceBefore() const;
    bool isGraceAfter() const;
    Breath *hasBreathMark() const;

    Segment *nextSegmentAfterCR(SegmentType types) const;

    void setScore(Score *s) override;
    EngravingItem *nextArticulationOrLyric(EngravingItem *e);
    EngravingItem *prevArticulationOrLyric(EngravingItem *e);
    virtual EngravingItem *nextElement() override;
    virtual EngravingItem *prevElement() override;
    EngravingItem *lastElementBeforeSegment();
    virtual EngravingItem *nextSegmentElement() override;
    virtual EngravingItem *prevSegmentElement() override;

    virtual void computeUp();

    bool isFullMeasureRest() const { return durationType_ == DurationType::V_MEASURE; }

    virtual void removeMarkings(bool keepTremolo = false);

    bool isBefore(const EngravingItem *) const override;

    virtual double intrinsicMag() const = 0;

    TabDurationSymbol *tabDur() const { return tabDur_; }

    void setTabDur(TabDurationSymbol *s) { tabDur_ = s; }

    bool isBelowCrossBeam(const BeamBase *beamBase) const;

    bool hasFollowingJumpItem() const;
    bool hasPrecedingJumpItem() const;

    struct LayoutData : public DurationElement::LayoutData {
        ld_field<bool> up = {"[ChordRest] up", true}; // actual stem direction
    };
    DECLARE_LAYOUTDATA_METHODS(ChordRest)

    //! DEPRECATED ------
    void setUp(bool val) { mutldata()->up = val; }

    bool up() const { return ldata()->up; }

    //! -----------------

protected:

    void addEl(EngravingItem *e) { el_.push_back(e); }

    bool removeEl(EngravingItem *e) { return el_.remove(e); }

    void clearEls() { el_.clear(); }

    std::vector<Lyrics *> lyrics_;
    TabDurationSymbol *tabDur_ = nullptr; // stores a duration symbol in tablature staves

    Beam *beam_ = nullptr;
    BeamSegment *beamlet_ = nullptr;
    BeamMode beamMode_ = BeamMode::INVALID;
    bool isSmall_ = false;
    bool melismaEnd_ = false;

    // CrossMeasure: combine 2 tied notes if across a bar line and can be combined in a single duration
    CrossMeasure crossMeasure_ = CrossMeasure::UNKNOWN; ///< 0: no cross-measure modification; 1: 1st note of a mod.; -1: 2nd note
    TDuration crossMeasureTDur_;                        ///< the total Duration type of the combined notes

private:

    void processSiblings(std::function<void(EngravingItem *)> func);

    ElementList el_;
    TDuration durationType_;
    int staffMove_ = 0;       // -1, 0, +1, used for crossbeaming
    int storedStaffMove_ = 0; // used to remember and re-apply staff move if needed
};
} // namespace mu::engraving
