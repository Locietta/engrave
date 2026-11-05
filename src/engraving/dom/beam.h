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

#include "beambase.h"
#include "engravingitem.h"
#include "property.h"

namespace mu::engraving {
class Chord;
class ChordRest;
class Factory;
class Skyline;
class System;
class Transaction;
enum class ActionIconType : signed char;
enum class SpannerSegmentType : unsigned char;

struct TremAnchor {
    ChordRest *chord1 = nullptr;
    double y1 = 0.;
    double y2 = 0.;
};

//---------------------------------------------------------
//   @@ Beam
//---------------------------------------------------------

class Beam final : public BeamBase {
    OBJECT_ALLOCATOR(engraving, Beam)
    DECLARE_CLASSOF(ElementType::BEAM)

public:
    ~Beam();

    Beam *clone() const override { return new Beam(*this); }

    Fraction tick() const override;
    Fraction rtick() const override;
    Fraction ticks() const;

    void spatiumChanged(double /*oldValue*/, double /*newValue*/) override;

    void reset() override;

    //! A beam has no owner: its lifetime is managed by the chord/rests it beams,
    //! which drop it through ChordRest::removeDeleteBeam() once it runs empty.
    //! The system it is drawn on is that of its first chord/rest.
    System *system() const;
    EngravingItem *layoutParent() const override;
    EngravingItem *accessibleParentItem() const override;

    const std::vector<ChordRest *> &elements() const { return elements_; }

    std::vector<ChordRest *> &elements() { return elements_; }

    void clear() { elements_.clear(); }

    bool empty() const { return elements_.empty(); }

    bool contains(const ChordRest *cr) const { return std::find(elements_.begin(), elements_.end(), cr) != elements_.end(); }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    void move(const PointF &) override;

    void setDirection(DirectionV d) override;

    void calcBeamBreaks(const ChordRest *chord, const ChordRest *prevChord, int level, bool &isBroken32, bool &isBroken64) const;

    //!Note Unfortunately we have no FEATHERED_BEAM_MODE for now int BeamMode enum, so we'll handle this locally
    void setAsFeathered(const bool slower);
    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    double growLeft() const { return growLeft_; }

    double growRight() const { return growRight_; }

    void setGrowLeft(double val) { growLeft_ = val; }

    void setGrowRight(double val) { growRight_ = val; }

    PairF beamPos() const;
    void setBeamPos(const PairF &bp);

    double beamDist() const { return beamDist_; }

    void setBeamDist(double d) { beamDist_ = d; }

    double beamWidth() const { return beamWidth_; }

    void setBeamWidth(double w) { beamWidth_ = w; }

    int beamSpacing() const { return beamSpacing_; }

    void setBeamSpacing(int val) { beamSpacing_ = val; }

    bool noSlope() const { return noSlope_; }

    void setNoSlope(bool b);

    double slope() const { return slope_; }

    void computeAndSetSlope();

    void setSlope(double val) { slope_ = val; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    void setIsGrace(bool val) { isGrace_ = val; }

    bool isGrace() const { return isGrace_; }

    bool cross() const { return cross_; }

    void setCross(bool val) { cross_ = val; }

    bool fullCross() const { return fullCross_; }

    void setFullCross(bool v) { fullCross_ = v; }

    int minCRMove() const override { return minCRMove_; }

    void setMinMove(int val) { minCRMove_ = val; }

    int maxCRMove() const override { return maxCRMove_; }

    void setMaxMove(int val) { maxCRMove_ = val; }

    void addSkyline(Skyline &);

    void triggerLayout() const override;

    static ActionIconType actionIconTypeForBeamMode(BeamMode);

    bool isMovable() const override;

    bool hasAllRests();

    const std::vector<TremAnchor> &tremAnchors() const { return tremAnchors_; }

    std::vector<TremAnchor> &tremAnchors() { return tremAnchors_; }

    const std::vector<BeamFragment *> &beamFragments() const { return fragments_; }

    std::vector<BeamFragment *> &beamFragments() { return fragments_; }

    void addBeamFragment(BeamFragment *f) { fragments_.push_back(f); }

    void clearBeamSegments() override;

    const StaffType *tab() const { return tab_; }

    void setTab(const StaffType *t) { tab_ = t; }

    bool isBesideTabStaff() const { return isBesideTabStaff_; }

    void setIsBesideTabStaff(bool val) { isBesideTabStaff_ = val; }

    const std::vector<NotePosition> &notePositions() const { return notePositions_; }

    std::vector<NotePosition> &notePositions() { return notePositions_; }

    const Chord *findChordWithCustomStemDirection() const;

    const BeamSegment *topLevelSegmentForElement(const ChordRest *element) const;

private:

    friend class Factory;
    friend class BeamSegment;
    Beam(Score *parent);
    Beam(const Beam &);

    static constexpr std::array MAX_SLOPES = {0, 1, 2, 3, 4, 5, 6, 7};

    void addChordRest(ChordRest *a);
    void removeChordRest(ChordRest *a);

    std::vector<ChordRest *> elements_; // must be sorted by tick

    bool isGrace_ = false;
    bool cross_ = false;
    bool fullCross_ = false;

    double growLeft_ = 1.0; // define "feather" beams
    double growRight_ = 1.0;
    double beamDist_ = 0.0;
    int beamSpacing_ = 3;    // how far apart beams are spaced in quarter spaces
    double beamWidth_ = 0.0; // how wide each beam is

    // for tabs
    bool isBesideTabStaff_ = false;
    const StaffType *tab_ = nullptr;

    std::vector<BeamFragment *> fragments_; // beam splits across systems

    int minCRMove_ = 0; // set in layout1()
    int maxCRMove_ = 0;
    int crossBeamPos_ = 0;

    bool noSlope_ = false;
    real_t slope_ = 0.0;

    std::vector<NotePosition> notePositions_;
    std::vector<TremAnchor> tremAnchors_;
};
} // namespace mu::engraving
