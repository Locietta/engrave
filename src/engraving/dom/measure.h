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

/**
 \file
 Definition of class Measure.
*/

#include "measurebase.h"

#include "segmentlist.h"

namespace mu::engraving::read400 {
class MeasureRead;
} // namespace mu::engraving::read400

namespace mu::engraving::read410 {
class MeasureRead;
} // namespace mu::engraving::read410

namespace mu::engraving::read460 {
class MeasureRead;
} // namespace mu::engraving::read460

namespace mu::engraving::read500 {
class MeasureRead;
} // namespace mu::engraving::read500

namespace mu::engraving::write {
class MeasureWrite;
} // namespace mu::engraving::write

namespace mu::engraving {
class AccidentalState;
class Chord;
class ChordRest;
class MMRestRange;
class MeasureNumber;
class MeasureRepeat;
class Note;
class Part;
class Score;
class Spacer;
class Staff;
class System;
class TieMap;
class Transaction;

//---------------------------------------------------------
//   MeasureNumberMode
//---------------------------------------------------------

enum class MeasureNumberMode : char {
    AUTO, // show measure number depending on style
    SHOW, // always show measure number
    HIDE  // don’t show measure number
};

//---------------------------------------------------------
//   MStaff
///   Per staff values of measure.
//---------------------------------------------------------

class MStaff {
public:
    MStaff() {}

    ~MStaff();
    MStaff(const MStaff &);

    void setScore(Score *);
    void setTrack(track_idx_t);

    MeasureNumber *measureNumber() const { return measureNumber_; }

    void setMeasureNumber(MeasureNumber *t) { measureNumber_ = t; }

    MMRestRange *mmRangeText() const { return mmRangeText_; }

    void setMMRangeText(MMRestRange *r) { mmRangeText_ = r; }

    StaffLines *lines() const { return lines_; }

    void setLines(StaffLines *l) { lines_ = l; }

    Spacer *vspacerUp() const { return vspacerUp_; }

    void setVspacerUp(Spacer *s) { vspacerUp_ = s; }

    Spacer *vspacerDown() const { return vspacerDown_; }

    void setVspacerDown(Spacer *s) { vspacerDown_ = s; }

    bool hasVoices() const { return hasVoices_; }

    void setHasVoices(bool val) { hasVoices_ = val; }

    bool visible() const { return visible_; }

    void setVisible(bool val) { visible_ = val; }

    bool stemless() const { return stemless_; }

    void setStemless(bool val) { stemless_ = val; }

    AutoOnOff hideIfEmpty() const { return hideIfEmpty_; }

    void setHideIfEmpty(AutoOnOff val) { hideIfEmpty_ = val; }

    bool corrupted() const { return corrupted_; }

    void setCorrupted(bool val) { corrupted_ = val; }

    int measureRepeatCount() const { return measureRepeatCount_; }

    void setMeasureRepeatCount(int n) { measureRepeatCount_ = n; }

private:
    MeasureNumber *measureNumber_ = nullptr; // Measure number text object
    MMRestRange *mmRangeText_ = nullptr;     // Multi measure rest range text object
    StaffLines *lines_ = nullptr;
    Spacer *vspacerUp_ = nullptr;
    Spacer *vspacerDown_ = nullptr;
    bool hasVoices_ = false; // indicates that MStaff contains more than one voice,
                             // this changes some layout rules

    bool visible_ = true;
    bool stemless_ = false;
    AutoOnOff hideIfEmpty_ = AutoOnOff::AUTO; // whether this MStaff wants its staff to be hidden on its system
                                              // when the staff is empty on that system

    bool corrupted_ = false;
    int measureRepeatCount_ = 0;
};

//---------------------------------------------------------
//   @@ Measure
///    one measure in a system
//
//   @P firstSegment    Segment       the first segment of the measure (read-only)
//   @P lastSegment     Segment       the last segment of the measure (read-only)
//---------------------------------------------------------

class Measure final : public MeasureBase {
    OBJECT_ALLOCATOR(engraving, Measure)
    DECLARE_CLASSOF(ElementType::MEASURE)

public:

    ~Measure();

    Measure *clone() const override { return new Measure(*this); }

    void setScore(Score *s) override;
    Measure *cloneMeasure(Score *, const Fraction &tick, TieMap *);

    void checkMeasure(staff_idx_t idx, bool useGapRests = true);

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    void change(EngravingItem *o, EngravingItem *n) override;
    void spatiumChanged(double oldValue, double newValue) override;

    bool hasVoices(staff_idx_t staffIdx, Fraction stick, Fraction len, bool considerInvisible = false) const;
    bool hasVoices(staff_idx_t staffIdx) const;
    void setHasVoices(staff_idx_t staffIdx, bool v);

    StaffLines *staffLines(staff_idx_t staffIdx) const;
    Spacer *vspacerDown(staff_idx_t staffIdx) const;
    Spacer *vspacerUp(staff_idx_t staffIdx) const;
    void setStaffVisible(staff_idx_t staffIdx, bool visible);
    void setStaffStemless(staff_idx_t staffIdx, bool stemless);
    AutoOnOff hideStaffIfEmpty(staff_idx_t staffIdx) const;
    void setHideStaffIfEmpty(staff_idx_t staffIdx, AutoOnOff hideIfEmpty);

    bool corrupted(staff_idx_t staffIdx) const { return mstaves_[staffIdx]->corrupted(); }

    void setCorrupted(staff_idx_t staffIdx, bool val) { mstaves_[staffIdx]->setCorrupted(val); }

    MeasureNumber *measureNumber(staff_idx_t staffIdx) const { return mstaves_[staffIdx]->measureNumber(); }

    void setMeasureNumber(staff_idx_t staffIdx, MeasureNumber *t) { mstaves_[staffIdx]->setMeasureNumber(t); }

    const std::vector<MStaff *> &mstaves() const { return mstaves_; }

    std::vector<MStaff *> &mstaves() { return mstaves_; }

    void setMMRangeText(staff_idx_t staffIdx, MMRestRange *);
    MMRestRange *mmRangeText(staff_idx_t staffIdx) const;

    void createStaves(staff_idx_t);

    Fraction timesig() const { return timesig_; }

    void setTimesig(const Fraction &f) { timesig_ = f; }

    Fraction stretchedLen(const Staff *) const;

    bool isIrregular() const { return timesig_ != len_; }

    int measureNumber() const { return measureNumber_; }

    void setMeasureNumber(int n) { measureNumber_ = n; }

    int measureNumberOffset() const { return measureNumberOffset_; }

    void setMeasureNumberOffset(int n) { measureNumberOffset_ = n; }

    bool excludeFromNumbering() const { return flag(ElementFlag::EXCLUDE_FROM_NUMBERING); }

    void setExcludeFromNumbering(bool v) { setFlag(ElementFlag::EXCLUDE_FROM_NUMBERING, v); }

    MeasureNumberMode measureNumberMode() const { return measureNumberMode_; }

    void setMeasureNumberMode(MeasureNumberMode v) { measureNumberMode_ = v; }

    bool repeatEnd() const { return flag(ElementFlag::REPEAT_END); }

    void setRepeatEnd(bool v) { setFlag(ElementFlag::REPEAT_END, v); }

    bool repeatStart() const { return flag(ElementFlag::REPEAT_START); }

    void setRepeatStart(bool v) { setFlag(ElementFlag::REPEAT_START, v); }

    bool repeatJump() const { return flag(ElementFlag::REPEAT_JUMP); }

    void setRepeatJump(bool v) { setFlag(ElementFlag::REPEAT_JUMP, v); }

    bool hasCourtesyKeySig() const { return flag(ElementFlag::COURTESY_KEYSIG); }

    void setHasCourtesyKeySig(bool v) { setFlag(ElementFlag::COURTESY_KEYSIG, v); }

    bool hasCourtesyTimeSig() const { return flag(ElementFlag::COURTESY_TIMESIG); }

    void setHasCourtesyTimeSig(bool v) const { setFlag(ElementFlag::COURTESY_TIMESIG, v); }

    bool hasCourtesyClef() const { return flag(ElementFlag::COURTESY_CLEF); }

    void setHasCourtesyClef(bool v) const { setFlag(ElementFlag::COURTESY_CLEF, v); }

    bool endOfMeasureChange() const { return flag(ElementFlag::END_OF_MEASURE_CHANGE); }

    void setEndOfMeasureChange(bool val) const { setFlag(ElementFlag::END_OF_MEASURE_CHANGE, val); }

    Segment *first() const { return segments_.first(); }

    Segment *first(SegmentType t) const { return segments_.first(t); }

    Segment *firstEnabled() const { return segments_.first(ElementFlag::ENABLED); }

    Segment *firstActive() const { return segments_.firstActive(); }

    Segment *last() const { return segments_.last(); }

    Segment *last(SegmentType t) const { return segments_.last(t); }

    Segment *lastEnabled() const { return segments_.last(ElementFlag::ENABLED); }

    SegmentList &segments() { return segments_; }

    const SegmentList &segments() const { return segments_; }

    double userStretch() const;

    void setUserStretch(double v) { userStretch_ = v; }

    void computeTicks();
    Fraction anacrusisOffset() const;
    Fraction maxTicks() const;

    bool showMeasureNumber() const;
    bool showMeasureNumberInAutoMode() const;
    bool showMeasureNumberOnStaff(staff_idx_t staffIdx) const;

    Chord *findChord(Fraction tick, track_idx_t track) const;
    ChordRest *findChordRest(Fraction tick, track_idx_t track) const;
    Fraction snap(const Fraction &tick, const PointF p) const;
    Fraction snapNote(const Fraction &tick, const PointF p, int staff) const;

    Segment *searchSegment(double x, SegmentType st, track_idx_t strack, track_idx_t etrack, const Segment *preferredSegment = nullptr,
                           double spacingFactor = 0.5) const;

    void insertStaff(Staff *, staff_idx_t staff);
    void insertMStaff(MStaff *staff, staff_idx_t idx);
    void removeMStaff(MStaff *staff, staff_idx_t idx);

    void moveTicks(const Fraction &diff) override;

    void cmdRemoveStaves(staff_idx_t s, staff_idx_t e);
    void cmdAddStaves(staff_idx_t s, staff_idx_t e, bool createRest);
    void removeStaves(staff_idx_t s, staff_idx_t e);
    void insertStaves(staff_idx_t s, staff_idx_t e);

    double tick2pos(Fraction) const;
    Segment *tick2segment(const Fraction &tick, SegmentType st = SegmentType::ChordRest);

    void sortStaves(std::vector<staff_idx_t> &dst);

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    int repeatCount() const { return repeatCount_; }

    void setRepeatCount(int val) { repeatCount_ = val; }

    Segment *findSegmentR(SegmentType st, const Fraction &) const;
    Segment *undoGetSegmentR(SegmentType st, const Fraction &f);
    Segment *getSegmentR(SegmentType st, const Fraction &f);
    Segment *findFirstR(SegmentType st, const Fraction &rtick) const;

    // segment routines with absolute tick values
    Segment *findSegment(SegmentType st, const Fraction &f) const { return findSegmentR(st, f - tick()); }

    Segment *undoGetSegment(SegmentType st, const Fraction &f) { return undoGetSegmentR(st, f - tick()); }

    Segment *getSegment(SegmentType st, const Fraction &f) { return getSegmentR(st, f - tick()); }

    Segment *undoGetChordRestOrTimeTickSegment(const Fraction &f);
    Segment *getChordRestOrTimeTickSegment(const Fraction &f);

    void connectTremolo();

    void setEndBarLineType(BarLineType val, track_idx_t track, bool visible = true, Color color = Color());

    void scanElements(std::function<void(EngravingItem *)> func) override;
    void createVoice(int track);
    void adjustToLen(Fraction, bool appendRestsIfNecessary = true);

    AccidentalVal findAccidental(const Note *) const;
    AccidentalVal findAccidental(const Segment *s, staff_idx_t staffIdx, int line, bool &error) const;
    void checkMultiVoices(staff_idx_t staffIdx);
    bool hasVoice(track_idx_t track) const;
    bool isEmpty(staff_idx_t staffIdx) const;
    bool isCutawayClef(staff_idx_t staffIdx) const;
    bool isFullMeasureRest() const;
    bool visible(staff_idx_t staffIdx) const;
    bool stemless(staff_idx_t staffIdx) const;
    bool isFinalMeasureOfSection() const;
    LayoutBreak *sectionBreakElement(bool includeNextFrames = true) const;
    bool isAnacrusis() const;
    bool isFirstInSystem() const;
    bool isLastInSystem() const;
    bool isFirstInSection() const;

    bool breakMultiMeasureRest() const { return breakMultiMeasureRest_; }

    void setBreakMultiMeasureRest(bool val) { breakMultiMeasureRest_ = val; }

    bool empty() const;
    bool isOnlyRests(track_idx_t track) const;
    bool isOnlyGapRests(track_idx_t track) const;

    int playbackCount() const { return playbackCount_; }

    void setPlaybackCount(int val) { playbackCount_ = val; }

    RectF staffPageBoundingRect(staff_idx_t staffIdx) const;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    void undoChangeProperty(Pid id, const PropertyValue &newValue);
    void undoChangeProperty(Pid id, const PropertyValue &newValue, PropertyFlags ps) override;

    bool hasMMRest() const { return mmRest_ != nullptr; }

    bool isMMRest() const { return mmRestCount_ > 0; }

    Measure *mmRest() const { return mmRest_; }

    Measure *coveringMMRestOrThis();
    const Measure *coveringMMRestOrThis() const;

    void setMMRest(Measure *m) { mmRest_ = m; }

    int mmRestCount() const { return mmRestCount_; } // number of measures mmRest_ spans

    void setMMRestCount(int n) { mmRestCount_ = n; }

    Measure *mmRestFirst() const;
    Measure *mmRestLast() const;

    int measureRepeatCount(staff_idx_t staffIdx) const;
    bool containsMeasureRepeat(const staff_idx_t staffIdxFrom, const staff_idx_t staffIdxTo) const;
    void setMeasureRepeatCount(int n, staff_idx_t staffIdx);
    bool isMeasureRepeatGroup(staff_idx_t staffIdx) const;
    bool isMeasureRepeatGroupWithNextM(staff_idx_t staffIdx) const;
    bool isMeasureRepeatGroupWithPrevM(staff_idx_t staffIdx) const;
    Measure *firstOfMeasureRepeatGroup(staff_idx_t staffIdx) const;  // used to find beginning of group
    MeasureRepeat *measureRepeatElement(staff_idx_t staffIdx) const; // get measure repeat element from anywhere within group
    int measureRepeatNumMeasures(staff_idx_t staffIdx) const;
    bool isOneMeasureRepeat(staff_idx_t staffIdx) const;
    bool nextIsOneMeasureRepeat(staff_idx_t staffidx) const;
    bool prevIsOneMeasureRepeat(staff_idx_t staffIdx) const;

    ChordRest *lastChordRest(track_idx_t track) const;
    ChordRest *firstChordRest(track_idx_t track) const;

    EngravingItem *nextElementStaff(staff_idx_t staff, EngravingItem *fromItem = nullptr);
    EngravingItem *prevElementStaff(staff_idx_t staff, EngravingItem *fromItem = nullptr);

    double firstNoteRestSegmentX(bool leading = false) const;
    double endingXForOpenEndedLines() const;

    void styleChanged() override;

    const BarLine *endBarLine() const;
    const BarLine *endBarLine(staff_idx_t staffIdx, bool first = false) const;
    BarLineType endBarLineType() const;
    bool endBarLineVisible() const;
    const BarLine *startBarLine() const;
    const BarLine *startBarLine(staff_idx_t staffIdx, bool first = false) const;
    void triggerLayout() const override;
    void triggerLayout(staff_idx_t staffIdx) const;

    void checkHeader();
    void checkTrailer();
    void checkEndOfMeasureChange();

    void respaceSegments();

    bool canAddStringTunings(staff_idx_t staffIdx) const;
    bool canAddStaffTypeChange(staff_idx_t staffIdx) const;

    struct LayoutData : public MeasureBase::LayoutData {
    private:
        bool needLayout_ = true;
    public:
        bool needLayout() const { return needLayout_; }

        void setNeedLayout(bool v) { needLayout_ = v; }
    };
    DECLARE_LAYOUTDATA_METHODS(Measure)

private:

    friend class Factory;

    Measure(Score *parent);
    Measure(const Measure &);

    void push_back(Segment *e);
    void push_front(Segment *e);

    void fillGap(const Fraction &pos, const Fraction &len, track_idx_t track, const Fraction &stretch, bool useGapRests = true);

    MStaff *mstaff(staff_idx_t staffIndex) const;

    std::vector<MStaff *> mstaves_;
    SegmentList segments_;
    Measure *mmRest_ = nullptr; // multi measure rest which replaces a measure range

    double userStretch_ = 0.0;

    Fraction timesig_;

    int mmRestCount_ = 0; // number of measures an mmrest spans

    int playbackCount_ = 0; // temp. value used in RepeatList
                            // counts how many times this measure was already played

    int repeatCount_ = 0; // end repeat marker and repeat count

    int measureNumber_ = 0; // counting from zero
    int measureNumberOffset_ = 0;
    MeasureNumberMode measureNumberMode_ = MeasureNumberMode::AUTO;
    bool breakMultiMeasureRest_ = false;
};
} // namespace mu::engraving
