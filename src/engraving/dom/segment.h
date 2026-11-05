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

namespace mu::engraving {
class Factory;
class Measure;
class Segment;
class ChordRest;
class Spanner;
class System;

//-------------------------------------------------------------------
//   SegmentType
//
//    Type values determine the order of segments for a given tick
//-------------------------------------------------------------------

enum class SegmentType {
    ///.\{
    Invalid = 0x0,
    BeginBarLine = 0x1,
    HeaderClef = 0x2,
    KeySig = 0x4,
    Ambitus = 0x8,
    Breath = 0x10,
    TimeSig = 0x20,
    StartRepeatBarLine = 0x40,
    ClefStartRepeatAnnounce = 0x80,
    KeySigStartRepeatAnnounce = 0x100,
    TimeSigStartRepeatAnnounce = 0x200,
    Clef = 0x400,
    BarLine = 0x800,
    //--
    TimeTick = 0x1000,
    ChordRest = 0x2000,
    //--
    ClefRepeatAnnounce = 0x4000,
    KeySigRepeatAnnounce = 0x8000,
    TimeSigRepeatAnnounce = 0x10000,
    //--
    EndBarLine = 0x20000,
    KeySigAnnounce = 0x40000,
    TimeSigAnnounce = 0x80000,
    //--
    All = -1, ///< Includes all barline types
    /// Alias for `BeginBarLine | StartRepeatBarLine | BarLine | EndBarLine`
    BarLineTypes = BeginBarLine | StartRepeatBarLine | BarLine | EndBarLine,
    CourtesyTimeSigTypes = TimeSigAnnounce | TimeSigRepeatAnnounce | TimeSigStartRepeatAnnounce,
    CourtesyKeySigTypes = KeySigAnnounce | KeySigRepeatAnnounce | KeySigStartRepeatAnnounce,
    CourtesyClefTypes = ClefRepeatAnnounce | ClefStartRepeatAnnounce,
    TimeSigTypes = TimeSig | CourtesyTimeSigTypes,
    KeySigTypes = KeySig | CourtesyKeySigTypes,
    ClefTypes = Clef | HeaderClef | CourtesyClefTypes,
    // Only types with non-null duration
    Duration = TimeTick | ChordRest,
    ///\}
};

constexpr SegmentType operator|(const SegmentType t1, const SegmentType t2) {
    return static_cast<SegmentType>(static_cast<int>(t1) | static_cast<int>(t2));
}

constexpr bool operator&(const SegmentType t1, const SegmentType t2) { return static_cast<int>(t1) & static_cast<int>(t2); }

constexpr SegmentType operator~(const SegmentType &t) { return static_cast<SegmentType>(~static_cast<int>(t)); }

//------------------------------------------------------------------------
//   @@ Segment
//    A segment holds all vertical aligned staff elements.
//    Segments are typed and contain only Elements of the same type.
//
//    All Elements in a segment start at the same tick. The Segment can store one EngravingItem for
//    each voice in each staff in the score.
//    Some elements (Clef, KeySig, TimeSig etc.) are assumed to always have voice zero
//    and can be found in _elist[staffIdx * VOICES];

//    Segments are children of Measures and store Clefs, KeySigs, TimeSigs,
//    BarLines and ChordRests.
//
//   @P annotations     array[EngravingItem]    the list of annotations (read only)
//   @P next            Segment           the next segment in the whole score; null at last score segment (read-only)
//   @P nextInMeasure   Segment           the next segment in measure; null at last measure segment (read-only)
//   @P prev            Segment           the previous segment in the whole score; null at first score segment (read-only)
//   @P prevInMeasure   Segment           the previous segment in measure; null at first measure segment (read-only)
//   @P segmentType     enum (Segment.All, .Ambitus, .BarLine, .Breath, .ChordRest, .Clef, .EndBarLine, .Invalid, .KeySig, .KeySigAnnounce, .StartRepeatBarLine, .TimeSig, .TimeSigAnnounce)
//   @P tick            int               midi tick position (read only)
//------------------------------------------------------------------------

class Segment final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Segment)
    DECLARE_CLASSOF(ElementType::SEGMENT)

protected:
    EngravingItem *getElement(staff_idx_t staff) const; //??

public:

    ~Segment();

    void setOwnershipParent(DummyParentOr<Measure> parent);

    Segment *clone() const override { return new Segment(*this); }

    void setScore(Score *) override;

    inline bool isActive() const { return !isTimeTickType() && enabled() && visible(); }

    Segment *next() const { return next_; }

    Segment *next(SegmentType) const;
    Segment *nextActive() const;
    Segment *nextEnabled() const;
    Segment *nextInStaff(staff_idx_t staffIdx, SegmentType t = SegmentType::ChordRest) const;

    void setNext(Segment *e) { next_ = e; }

    Segment *prev() const { return prev_; }

    Segment *prev(SegmentType) const;
    Segment *prevWithElementsOnTrack(track_idx_t trackIdx, SegmentType segType = SegmentType::ChordRest) const;
    Segment *prevActive() const;
    Segment *prevEnabled() const;

    void setPrev(Segment *e) { prev_ = e; }

    // don’t stop at measure boundary:
    Segment *next1() const;
    Segment *next1enabled() const;
    Segment *next1MM() const;
    Segment *next1MMenabled() const;
    Segment *next1(SegmentType) const;
    Segment *next1WithElemsOnStaff(staff_idx_t staffIdx, SegmentType segType = SegmentType::ChordRest) const;
    Segment *next1WithElemsOnTrack(track_idx_t trackIdx, SegmentType segType = SegmentType::ChordRest) const;
    Segment *next1MM(SegmentType) const;

    Segment *prev1() const;
    Segment *prev1WithElemsOnStaff(staff_idx_t staffIdx, SegmentType segType = SegmentType::ChordRest) const;
    Segment *prev1WithElemsOnTrack(track_idx_t trackIdx, SegmentType segType = SegmentType::ChordRest) const;
    Segment *prev1enabled() const;
    Segment *prev1MM() const;
    Segment *prev1MMenabled() const;
    Segment *prev1(SegmentType) const;
    Segment *prev1MM(SegmentType) const;

    Segment *nextCR(track_idx_t track = muse::nidx, bool sameStaff = false) const;

    ChordRest *nextChordRest(track_idx_t track, bool backwards = false, bool stopAtMeasureBoundary = false) const;

    EngravingItem *element(track_idx_t track) const;

    const std::vector<EngravingItem *> &elist() const { return elist_; }

    std::vector<EngravingItem *> &elist() { return elist_; }

    void removeElement(track_idx_t track);
    void setElement(track_idx_t track, EngravingItem *el);
    void scanElements(std::function<void(EngravingItem *)> func) override;

    Measure *measure() const { return toMeasure(ownershipParent()); }

    System *system() const;

    double x() const override { return ldata()->pos().x(); }

    void insertStaff(staff_idx_t staff);
    void removeStaff(staff_idx_t staff);

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    void swapElements(track_idx_t i1, track_idx_t i2);

    void sortStaves(std::vector<staff_idx_t> &dst);
    const char *subTypeName() const;

    static const char *subTypeName(SegmentType);
    static SegmentType segmentType(ElementType type);

    SegmentType segmentType() const { return segmentType_; }

    void setSegmentType(SegmentType t);

    bool empty() const { return flag(ElementFlag::EMPTY); }

    bool written() const { return flag(ElementFlag::WRITTEN); }

    void setWritten(bool val) const { setFlag(ElementFlag::WRITTEN, val); }

    bool endOfMeasureChange() const {
        return flag(ElementFlag::END_OF_MEASURE_CHANGE);
    } // Key/time sigs which should be placed at the end of the measure

    void setEndOfMeasureChange(bool val) const { setFlag(ElementFlag::END_OF_MEASURE_CHANGE, val); }

    void fixStaffIdx();

    double stretch() const { return stretch_; }

    void setStretch(double v) { stretch_ = v; }

    Fraction rtick() const override { return tick_; }

    void setRtick(const Fraction &v) {
        assert(v >= Fraction(0, 1));
        tick_ = v;
    }

    Fraction tick() const override;

    Fraction ticks() const { return ticks_; }

    void setTicks(const Fraction &v) { ticks_ = v; }

    double widthInStaff(staff_idx_t staffIdx, SegmentType nextSegType = SegmentType::ChordRest) const;
    Fraction ticksInStaff(staff_idx_t staffIdx) const;

    bool splitsTuplet() const;

    const std::vector<EngravingItem *> &annotations() const { return annotations_; }

    void clearAnnotations();
    void removeAnnotation(EngravingItem *e);
    bool hasAnnotationOrElement(ElementType type, track_idx_t minTrack, track_idx_t maxTrack) const;
    bool hasAnnotationOrElement(staff_idx_t staffIdx) const;
    EngravingItem *findAnnotation(ElementType type, track_idx_t minTrack, track_idx_t maxTrack) const;
    std::vector<EngravingItem *> findAnnotations(ElementType type, track_idx_t minTrack, track_idx_t maxTrack) const;
    bool hasElements() const;
    bool hasElements(track_idx_t minTrack, track_idx_t maxTrack) const;
    bool hasElements(staff_idx_t staffIdx) const;
    bool allElementsInvisible() const;

    Spatium extraLeadingSpace() const { return extraLeadingSpace_; }

    void setExtraLeadingSpace(Spatium v) { extraLeadingSpace_ = v; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    bool operator<(const Segment &) const;
    bool operator>(const Segment &) const;

    EngravingItem *firstInNextSegments(staff_idx_t activeStaff) const; //<
    EngravingItem *lastInPrevSegments(staff_idx_t activeStaff) const;  //<
    EngravingItem *firstElementForNavigation(staff_idx_t staff) const; //<  These methods are used for navigation
    EngravingItem *lastElementForNavigation(staff_idx_t staff) const;  //<  for next-element and prev-element
    EngravingItem *firstElementOfSegment(staff_idx_t activeStaff) const;
    EngravingItem *nextElementOfSegment(EngravingItem *e, staff_idx_t activeStaff) const;
    EngravingItem *prevElementOfSegment(EngravingItem *e, staff_idx_t activeStaff) const;
    EngravingItem *lastElementOfSegment(staff_idx_t activeStaff) const;
    EngravingItem *nextAnnotation(EngravingItem *e) const;
    EngravingItem *prevAnnotation(EngravingItem *e) const;
    EngravingItem *firstAnnotation(staff_idx_t activeStaff) const;
    EngravingItem *lastAnnotation(staff_idx_t activeStaff) const;
    Spanner *firstSpanner(staff_idx_t activeStaff) const;
    Spanner *lastSpanner(staff_idx_t activeStaff) const;
    bool notChordRestType() const;
    using EngravingItem::nextElement;
    EngravingItem *nextElement(staff_idx_t activeStaff);
    using EngravingItem::prevElement;
    EngravingItem *prevElement(staff_idx_t activeStaff);

    EngravingItem *firstElement(staff_idx_t staffIdx) const;

    std::vector<Shape> shapes() { return shapes_; }

    const std::vector<Shape> &shapes() const { return shapes_; }

    const Shape &staffShape(staff_idx_t staffIdx) const { return shapes_[staffIdx]; }

    Shape &staffShape(staff_idx_t staffIdx) { return shapes_[staffIdx]; }

    void createShapes();
    void createShape(staff_idx_t staffIdx);
    double minRight() const;
    double minLeft() const;

    double widthOffset() const { return widthOffset_; }

    void clearWidthOffset() { widthOffset_ = 0.0; }

    void addWidthOffset(double w) { widthOffset_ += w; }

    void setWidthOffset(double w) { widthOffset_ = w; }

    double elementsTopOffsetFromSkyline(staff_idx_t staffIndex) const;
    double elementsBottomOffsetFromSkyline(staff_idx_t staffIndex) const;

    //! spacing is additional width of segment, for example accidental needs this spacing to avoid overlapping
    void setSpacing(double);
    double spacing() const;

    // some helper function
    ChordRest *cr(track_idx_t track) const { return toChordRest(elist_[track]); }

    bool isType(const SegmentType t) const { return int(segmentType_) & int(t); }

    bool isJustType(const SegmentType t) const { return segmentType_ == t; }

    bool isBeginBarLineType() const { return segmentType_ == SegmentType::BeginBarLine; }

    bool isClefType() const { return segmentType_ == SegmentType::Clef; }

    bool isHeaderClefType() const { return segmentType_ == SegmentType::HeaderClef; }

    bool isKeySigType() const { return segmentType_ == SegmentType::KeySig; }

    bool isAmbitusType() const { return segmentType_ == SegmentType::Ambitus; }

    bool isTimeSigType() const { return segmentType_ == SegmentType::TimeSig; }

    bool hasTimeSigAboveStaves() const;
    bool makeSpaceForTimeSigAboveStaves() const;
    bool hasTimeSigAcrossStaves() const;

    bool isStartRepeatBarLineType() const { return segmentType_ == SegmentType::StartRepeatBarLine; }

    bool isBarLineType() const { return segmentType_ == SegmentType::BarLine; }

    bool isBreathType() const { return segmentType_ == SegmentType::Breath; }

    bool isChordRestType() const { return segmentType_ == SegmentType::ChordRest; }

    bool isClefRepeatAnnounceType() const { return segmentType_ == SegmentType::ClefRepeatAnnounce; }

    bool isKeySigRepeatAnnounceType() const { return segmentType_ == SegmentType::KeySigRepeatAnnounce; }

    bool isTimeSigRepeatAnnounceType() const { return segmentType_ == SegmentType::TimeSigRepeatAnnounce; }

    bool isEndBarLineType() const { return segmentType_ == SegmentType::EndBarLine; }

    bool isKeySigAnnounceType() const { return segmentType_ == SegmentType::KeySigAnnounce; }

    bool isTimeSigAnnounceType() const { return segmentType_ == SegmentType::TimeSigAnnounce; }

    bool isCourtesySegment() const {
        return segmentType_ & (SegmentType::CourtesyTimeSigTypes | SegmentType::CourtesyKeySigTypes | SegmentType::CourtesyClefTypes);
    }

    bool isTimeTickType() const { return segmentType_ == SegmentType::TimeTick; }

    bool isRightAligned() const { return isClefType() || isBreathType(); }

    bool isMMRestSegment() const { return isChordRestType() && elist_.front() && elist_.front()->isMMRest(); }

    bool canWriteSpannerStartEnd(track_idx_t track, const Spanner *spanner) const;

    Fraction shortestChordRest() const;

    bool hasAccidentals() const;

    EngravingItem *preAppendedItem(track_idx_t track) { return preAppendedItems_[track]; }

    void preAppend(EngravingItem *item, track_idx_t track) { preAppendedItems_[track] = item; }

    void clearPreAppended(track_idx_t track) { preAppendedItems_[track] = nullptr; }

    bool goesBefore(const Segment *nextSegment) const;

    void checkEmpty() const;

    double xPosInSystemCoords() const;
    void setXPosInSystemCoords(double x);

    bool isTupletSubdivisionOnStaff(staff_idx_t staffIdx) const;
    bool isInsideTupletOnStaff(staff_idx_t staffIdx) const;

private:

    void addArticulationsToShape(const Chord *chord, Shape &shape);

    friend class Factory;
    Segment(DummyParentOr<Measure> m);
    Segment(DummyParentOr<Measure>, SegmentType, const Fraction &);
    Segment(const Segment &);

    void init();
    void checkElement(EngravingItem *, track_idx_t track);

    void setEmpty(bool val) const { setFlag(ElementFlag::EMPTY, val); }

    SegmentType segmentType_ = SegmentType::Invalid;
    Fraction tick_;  // { Fraction(0, 1) };
    Fraction ticks_; // { Fraction(0, 1) };
    Spatium extraLeadingSpace_;
    double stretch_ = 1.0;
    double widthOffset_ = 0.0; // part of the segment width that will not be stretched during system justification

    Segment *next_ = nullptr; // linked list of segments inside a measure
    Segment *prev_ = nullptr;

    std::vector<EngravingItem *> annotations_;
    std::vector<EngravingItem *> elist_; // EngravingItem storage, size = staves * VOICES.
    std::vector<EngravingItem *>
        preAppendedItems_;      // Container for items appended to the left of this segment (example: grace notes), size = staves * VOICES.
    std::vector<Shape> shapes_; // size = staves
    double spacing_ = 0;
};
} // namespace mu::engraving
