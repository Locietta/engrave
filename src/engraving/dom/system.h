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
 Definition of classes SysStaff and System
*/

#include "engravingitem.h"
#include "instrumentname.h"

namespace mu::engraving {
class Box;
class Bracket;
class InstrumentName;
class MeasureBase;
class Page;
class SpannerSegment;
class StaffVisibilityIndicator;
class RangeLock;

//---------------------------------------------------------
//   SysStaff
///  One staff in a System.
//---------------------------------------------------------

class SysStaff {
public:
    SysStaff() {}

    ~SysStaff();

    const std::unordered_map<InstrumentNameRole, InstrumentName *> &instrumentNames() const { return instrumentNames_; }

    InstrumentName *name(InstrumentNameRole role) const;
    void addInstrumentName(InstrumentName *n);
    void removeInstrumentName(InstrumentNameRole role);

    const RectF &bbox() const { return bbox_; }

    RectF &bbox() { return bbox_; }

    void setbbox(const RectF &r) { bbox_ = r; }

    void setbbox(double x, double y, double w, double h) { bbox_.setRect(x, y, w, h); }

    double y() const { return bbox_.y() + yOff_; }

    void setYOff(double offset) { yOff_ = offset; }

    double yOffset() const { return yOff_; }

    double yBottom() const;

    void saveLayout();
    void restoreLayout();

    double continuousDist() const { return continuousDist_; }

    void setContinuousDist(double val) { continuousDist_ = val; }

    bool show() const { return show_; }

    void setShow(bool v) { show_ = v; }

    const Skyline &skyline() const { return skyline_; }

    Skyline &skyline() { return skyline_; }

private:
    std::unordered_map<InstrumentNameRole, InstrumentName *> instrumentNames_;

    RectF bbox_; // Bbox of StaffLines.
    Skyline skyline_;
    double yOff_ = 0.0;            // offset of top staff line within bbox
    double yPos_ = 0.0;            // y position of bbox after System::layout2
    double height_ = 0.0;          // height of bbox after System::layout2
    double continuousDist_ = -1.0; // distance for continuous mode
    bool show_ = true;             // derived from Staff or false if empty
                                   // staff is hidden
};

//---------------------------------------------------------
//   System
///    One row of measures for all instruments;
///    a complete piece of the timeline.
//---------------------------------------------------------

class System final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, System)
    DECLARE_CLASSOF(ElementType::SYSTEM)

public:
    ~System();

    System *clone() const override { return new System(*this); }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    void change(EngravingItem *o, EngravingItem *n) override;

    void scanElements(std::function<void(EngravingItem *)> func) override;
    EngravingItemList accessibleChildren() const override;

    void appendMeasure(MeasureBase *);
    void removeMeasure(MeasureBase *);
    void removeLastMeasure();

    //! The page this system is currently laid out on; null if not placed.
    Page *page() const { return page_; }

    void setPage(Page *p) { page_ = p; }

    EngravingItem *layoutParent() const override;

    //! A system is owned by its score; a page merely places it, see setPage().
    //! This overload hides EngravingItem::setOwnershipParent, so that no other
    //! parent can be set by accident.
    void setOwnershipParent(Score *score);

    void clear(); ///< Clear measure list.

    std::vector<SysStaff *> &staves() { return staves_; }

    const std::vector<SysStaff *> &staves() const { return staves_; }

    double staffYpage(staff_idx_t staffIdx) const;
    double staffCanvasYpage(staff_idx_t staffIdx) const;
    SysStaff *staff(size_t staffIdx) const;

    bool pageBreak() const;

    SysStaff *insertStaff(int);
    void removeStaff(int);
    void adjustStavesNumber(size_t nstaves);

    staff_idx_t searchStaff(double y, staff_idx_t preferredStaff = muse::nidx, double spacingFactor = 0.5) const;
    Fraction snap(const Fraction &tick, const PointF p) const;
    Fraction snapNote(const Fraction &tick, const PointF p, int staff) const;

    const std::vector<MeasureBase *> &measures() const { return ml_; }

    std::vector<MeasureBase *> &measures() { return ml_; }

    MeasureBase *measure(int idx) { return ml_[idx]; }

    MeasureBase *first() const { return ml_.front(); }

    MeasureBase *last() const { return ml_.back(); }

    Measure *firstMeasure() const;
    Measure *lastMeasure() const;
    Fraction endTick() const;

    MeasureBase *nextMeasure(const MeasureBase *) const;

    double leftMargin() const { return leftMargin_; }

    void setLeftMargin(double val) { leftMargin_ = val; }

    double systemHeight() const { return systemHeight_; }

    void setSystemHeight(double h) { systemHeight_ = h; }

    Box *vbox() const;

    const std::vector<Bracket *> &brackets() const { return brackets_; }

    std::vector<Bracket *> &brackets() { return brackets_; }

    std::list<SpannerSegment *> &spannerSegments() { return spannerSegments_; }

    const std::list<SpannerSegment *> &spannerSegments() const { return spannerSegments_; }

    SystemDivider *systemDividerLeft() const { return systemDividerLeft_; }

    SystemDivider *systemDividerRight() const { return systemDividerRight_; }

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    double topDistance(staff_idx_t staffIdx, const SkylineLine &) const;
    double bottomDistance(staff_idx_t staffIdx, const SkylineLine &) const;
    double minTop() const;
    double minBottom() const;
    double spacerDistance(bool up) const;
    Spacer *upSpacer(staff_idx_t staffIdx, Spacer *prevDownSpacer) const;
    Spacer *downSpacer(staff_idx_t staffIdx) const;

    double firstNoteRestSegmentX(bool leading = false) const;
    double endingXForOpenEndedLines() const;
    ChordRest *lastChordRest(track_idx_t track) const;
    ChordRest *firstChordRest(track_idx_t track) const;

    bool hasFixedDownDistance() const { return fixedDownDistance_; }

    void setFixedDownDistance(bool val) const { fixedDownDistance_ = val; }

    staff_idx_t firstVisibleStaff() const;
    staff_idx_t nextVisibleStaff(staff_idx_t) const;
    staff_idx_t prevVisibleStaff(staff_idx_t) const;
    staff_idx_t lastVisibleStaff() const;

    double distance() const { return distance_; }

    void setDistance(double d) { distance_ = d; }

    staff_idx_t firstSysStaffOfPart(const Part *part) const;
    staff_idx_t firstVisibleSysStaffOfPart(const Part *part) const;
    staff_idx_t firstVisibleSysStaffWithInstrument(const String &instrumentId, staff_idx_t startFrom);
    staff_idx_t lastSysStaffOfPart(const Part *part) const;
    staff_idx_t lastVisibleSysStaffOfPart(const Part *part) const;
    std::vector<staff_idx_t> visibleStavesOfPart(const Part *part) const;
    std::vector<Part *> visiblePartsOfGroup(staff_idx_t start, staff_idx_t end) const;

    size_t getBracketsColumnsCount() const;

    void resetShortestLongestChordRest();

    StaffVisibilityIndicator *staffVisibilityIndicator() const { return staffVisibilityIndicator_; }

    void setHasStaffVisibilityIndicator(bool has);

    bool isLocked() const;
    const RangeLock *systemLock() const;

    const std::vector<SystemLockIndicator *> systemLockIndicators() const { return systemLockIndicators_; }

    void addSystemLockIndicator(SystemLockIndicator *sli);
    void deleteSystemLockIndicators();

    PageLockIndicator *pageLockIndicator() const { return pageLockIndicator_; }

    void setPageLockIndicator(PageLockIndicator *pli);
    void deletePageLockIndicator();

    struct LayoutData : public EngravingItem::LayoutData {
    public:
        bool useLongNames() const { return useLongNames_; }

        void setUseLongNames(bool v) { useLongNames_ = v; }

        double instrumentNameOffset() const { return instrumentNameOffset_; }

        void setInstrumentNameOffset(double v) { instrumentNameOffset_ = v; }

        double firstColumnWidth() const { return firstColumnWidth_; }

        void setFirstColumnWidth(double v) { firstColumnWidth_ = v; }

        double secondColumnWidth() const { return secondColumnWidth_; }

        void setSecondColumnWidth(double v) { secondColumnWidth_ = v; }

        double totalNamesWidth() const { return totalNamesWidth_; }

        void setTotalNamesWidth(double v) { totalNamesWidth_ = v; }

        const std::unordered_map<staff_idx_t, double> &groupBracketsWidth() const { return groupBracketsWidth_; }

        void setGroupBracketsWidthAtStaffIdx(staff_idx_t i, double w) { groupBracketsWidth_[i] = w; }

        double groupBracketsWidthAtStaffIdx(staff_idx_t i) const { return groupBracketsWidth_.count(i) ? groupBracketsWidth_.at(i) : 0.0; }

        void clearGroupBracketsWidth() { groupBracketsWidth_.clear(); }

        const std::unordered_map<Part *, InstrumentName *> &partsWithGroupName() const { return partsWithGroupName_; }

        void addPartWithGroupNames(Part *p, InstrumentName *n) { partsWithGroupName_.emplace(p, n); }

        void clearPartsWithGroupNames() { partsWithGroupName_.clear(); }

    private:
        bool useLongNames_ = false;
        double instrumentNameOffset_ = 0.0;

        double firstColumnWidth_ = 0.0;
        double secondColumnWidth_ = 0.0;
        double totalNamesWidth_ = 0.0;

        std::unordered_map<staff_idx_t, double> groupBracketsWidth_;

        std::unordered_map<Part *, InstrumentName *> partsWithGroupName_;
    };
    DECLARE_LAYOUTDATA_METHODS(System)

private:
    friend class Factory;

    System(Score *parent);

    staff_idx_t firstVisibleSysStaff() const;
    staff_idx_t lastVisibleSysStaff() const;

    staff_idx_t firstVisibleStaffFrom(staff_idx_t startStaffIdx) const;

    Page *page_ = nullptr; // current layout placement; not owned, not copied

    SystemDivider *systemDividerLeft_ = nullptr; // to the next system
    SystemDivider *systemDividerRight_ = nullptr;

    std::vector<MeasureBase *> ml_;
    std::vector<SysStaff *> staves_;
    std::vector<Bracket *> brackets_;
    std::list<SpannerSegment *> spannerSegments_;
    std::vector<SystemLockIndicator *> systemLockIndicators_;
    PageLockIndicator *pageLockIndicator_ = nullptr;

    StaffVisibilityIndicator *staffVisibilityIndicator_ = nullptr;

    double leftMargin_ = 0.0; // left margin for instrument name, brackets etc.
    mutable bool fixedDownDistance_ = false;
    double distance_ = 0.0; // temp. variable used during layout
    double systemHeight_ = 0.0;
};

typedef std::vector<System *>::iterator iSystem;
typedef std::vector<System *>::const_iterator ciSystem;
} // namespace mu::engraving
