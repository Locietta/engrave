/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include <vector>
#include <set>
#include <unordered_set>

#include "../../types/fraction.h"
#include "../../types/types.h"

#include "../../style/style.h"
#include "../../iengravingfont.h"
#include "../../dom/mscore.h"

#include "../layoutoptions.h"

#ifdef MUE_ENABLE_ENGRAVING_RENDER_DEBUG
#include "logstream.h"
#include "logstream.h"
#define LAYOUT_CALL_CLEAR mu::engraving::rendering::dev::LayoutDebug::instance()->callClear
#define LAYOUT_CALL_BEGIN(name) mu::engraving::rendering::dev::LayoutDebug::CallBegin(name)
#define LAYOUT_CALL_END mu::engraving::rendering::dev::LayoutDebug::instance()->callEnd
#define LAYOUT_CALL()                                               \
    mu::engraving::rendering::dev::LayoutDebug::CallMarker _ldcall; \
    _ldcall.begin(FUNCNAME).stream
#define LAYOUT_ITEM_INFO(item) item->typeName() << "(" << item->eid() << ")"

#define LAYOUT_CALL_PRINT mu::engraving::rendering::dev::LayoutDebug::instance()->callPrint
#else
#define LAYOUT_CALL_CLEAR()
#define LAYOUT_CALL_BEGIN(name, info)
#define LAYOUT_CALL_END()
#define LAYOUT_CALL() \
    if (0) muse::logger::Stream()
#define LAYOUT_ITEM_INFO(item) ""
#define LAYOUT_CALL_PRINT()
#endif

namespace mu::engraving {
class DummyParent;
class EngravingItem;
class MeasureBase;
class Part;
class Page;
class Score;
class Spanner;
class SpannerMap;
class System;
class RangeLocks;
class Staff;
class Measure;
class ChordRest;
class Segment;
class BracketItem;
struct PaddingTable;

class UndoableCommand;
class EditData;

class Selection;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class IGetScoreInternal {
private:
    friend class LayoutConfiguration;
    friend class DomAccessor;

    virtual Score *score() = 0;
};

class LayoutConfiguration {
public:
    LayoutConfiguration(IGetScoreInternal *s);

    LayoutMode viewMode() const { return options().mode; }

    bool isMode(LayoutMode m) const { return options().isMode(m); }

    bool isLineMode() const { return isMode(LayoutMode::LINE); }

    bool isLinearMode() const { return options().isLinearMode(); }

    bool isFloatMode() const { return isMode(LayoutMode::FLOAT); }

    bool isPrintingMode() const;

    bool isShowVBox() const { return options().isShowVBox; }

    double noteHeadWidth() const { return options().noteHeadWidth; }

    bool isShowInvisible() const;
    int pageNumberOffset() const;
    bool isVerticalSpreadEnabled() const;
    double maxSystemDistance() const;
    bool isShowInstrumentNames() const;

    std::shared_ptr<const IEngravingFont> engravingFont() const;

    const MStyle &style() const;

    const PropertyValue &styleV(Sid idx) const { return style().styleV(idx); }

    Spatium styleS(Sid idx) const { return style().styleS(idx); }

    double styleAbsolute(Sid idx) const { return style().styleAbsolute(idx); }

    String styleSt(Sid idx) const { return style().styleSt(idx); }

    bool styleB(Sid idx) const { return style().styleB(idx); }

    double styleD(Sid idx) const { return style().styleD(idx); }

    int styleI(Sid idx) const { return style().styleI(idx); }

    double spatium() const { return styleD(Sid::spatium); }

    double defaultSpatium() const { return style().defaultSpatium(); }

    double point(const Spatium sp) const { return sp.val() * spatium(); }

    double magS(double mag) const { return mag * (spatium() / defaultSpatium()); }

    double loWidth() const { return styleD(Sid::pageWidth) * DPI; }

    double loHeight() const { return styleD(Sid::pageHeight) * DPI; }

    bool firstSystemIndent() const { return styleB(Sid::enableIndentationOnFirstSystem); }

private:
    const Score *score() const;
    const LayoutOptions &options() const;

    IGetScoreInternal *getScore_ = nullptr;
};

class DomAccessor {
public:

    DomAccessor(IGetScoreInternal *s);

    // Const access
    const std::vector<Part *> &parts() const;
    size_t visiblePartCount() const;
    std::vector<Part *> visibleParts() const;

    size_t npages() const;
    const std::vector<Page *> &pages() const;

    const std::vector<System *> &systems() const;

    size_t nstaves() const;
    size_t visibleStaffCount() const;
    const std::vector<Staff *> &staves() const;
    const Staff *staff(staff_idx_t idx) const;
    bool allStavesInvisible() const;

    size_t ntracks() const;

    size_t nmeasures() const;

    const Measure *tick2measure(const Fraction &tick) const;
    const Measure *tick2measureMM(const Fraction &tick) const;
    const Measure *firstMeasure() const;
    const Measure *lastMeasure() const;
    const Measure *lastMeasureMM() const;

    const SpannerMap &spannerMap() const;

    const Segment *lastSegment() const;

    const ChordRest *findCR(Fraction tick, track_idx_t track) const;

    const RangeLocks *systemLocks() const;
    const RangeLocks *pageLocks() const;

    const PaddingTable &paddingTable() const;

    const std::vector<BracketItem *> &brackets(staff_idx_t staffIdx) const;

    // Mutable access
    std::vector<Page *> &pages();
    std::vector<System *> &systems();

    MeasureBase *first();
    Measure *firstMeasure();
    Measure *lastMeasure();
    Measure *lastMeasureMM();
    Measure *tick2measure(const Fraction &tick);
    Measure *tick2measureMM(const Fraction &tick);

    ChordRest *findCR(Fraction tick, track_idx_t track);

    // Create/Remove
    const Score *score() const;
    Score *score();
    DummyParent *dummyParent() const;
    void doUndoAddElement(EngravingItem *);
    void undoAddElement(EngravingItem *item, bool addToLinkedStaves = true, bool ctrlModifier = false);
    void doUndoRemoveElement(EngravingItem *item);
    void undoRemoveElement(EngravingItem *item);
    void undo(UndoableCommand *cmd) const;
    void addElement(EngravingItem *item);
    void removeElement(EngravingItem *item);
    void updateLocksOnCreateMMRest(Measure *first, Measure *last);
    void undoChangeParent(EngravingItem *element, EngravingItem *parent, staff_idx_t staff, bool changeLinksParents = true);

    void addUnmanagedSpanner(Spanner *s);
    const std::set<Spanner *> &unmanagedSpanners() const;

private:
    IGetScoreInternal *getScore_ = nullptr;
};

class LayoutState {
public:

    // Const
    bool firstSystem() const { return firstSystem_; }

    bool firstSystemIndent() const { return firstSystemIndent_; }

    bool startWithLongNames() const { return startWithLongNames_; }

    const Fraction &tick() const { return tick_; }

    const Fraction &startTick() const { return startTick_; }

    const Fraction &endTick() const { return endTick_; }

    bool isLayoutAll() const { return isLayoutAll_; }

    const Page *page() const { return page_; }

    page_idx_t pageIdx() const { return pageIdx_; }

    const std::vector<System *> &systemList() const { return systemList_; }

    const System *prevSystem() const { return prevSystem_; }

    const System *curSystem() const { return curSystem_; }

    const MeasureBase *prevMeasure() const { return prevMeasure_; }

    const MeasureBase *curMeasure() const { return curMeasure_; }

    const MeasureBase *nextMeasure() const { return nextMeasure_; }

    const MeasureBase *systemOldMeasure() const { return systemOldMeasure_; }

    const MeasureBase *pageOldMeasure() const { return pageOldMeasure_; }

    int measureNumber() const { return measureNumber_; }

    bool rangeDone() const { return rangeDone_; }

    const std::unordered_set<System *> &systemsNeedingCentering() const { return systemsNeedingCentering_; }

    bool mustRecomputeHeadersFooters() const { return mustRecomputeHeadersFooters_; }

    double totalBracketsWidth() const { return totalBracketsWidth_; }

    // Mutable
    void setFirstSystem(bool val) { firstSystem_ = val; }

    void setFirstSystemIndent(bool val) { firstSystemIndent_ = val; }

    void setStartWithLongNames(bool val) { startWithLongNames_ = val; }

    void setTick(const Fraction &t) { tick_ = t; }

    void setStartTick(const Fraction &t) { startTick_ = t; }

    void setEndTick(const Fraction &t) { endTick_ = t; }

    void setIsLayoutAll(bool v) { isLayoutAll_ = v; }

    Page *page() { return page_; }

    void setPage(Page *p) { page_ = p; }

    void setPageIdx(page_idx_t idx) { pageIdx_ = idx; }

    std::vector<System *> &systemList() { return systemList_; }

    void setSystemList(const std::vector<System *> &l) { systemList_ = l; }

    System *prevSystem() { return prevSystem_; }

    void setPrevSystem(System *s) { prevSystem_ = s; }

    System *curSystem() { return curSystem_; }

    void setCurSystem(System *s) { curSystem_ = s; }

    MeasureBase *prevMeasure() { return prevMeasure_; }

    void setPrevMeasure(MeasureBase *m) { prevMeasure_ = m; }

    MeasureBase *curMeasure() { return curMeasure_; }

    void setCurMeasure(MeasureBase *m) { curMeasure_ = m; }

    MeasureBase *nextMeasure() { return nextMeasure_; }

    void setNextMeasure(MeasureBase *m) { nextMeasure_ = m; }

    void setSystemOldMeasure(MeasureBase *m) { systemOldMeasure_ = m; }

    void setPageOldMeasure(MeasureBase *m) { pageOldMeasure_ = m; }

    void setMeasureNumber(int n) { measureNumber_ = n; }

    void setRangeDone(bool val) { rangeDone_ = val; }

    void addSystemNeedingCentering(System *s) { systemsNeedingCentering_.insert(s); }

    void removeSystemNeedingCentering(System *s) { systemsNeedingCentering_.erase(s); }

    void clearSystemsNeedingCentering() { systemsNeedingCentering_.clear(); }

    void setMustRecomputeHeadersFooters(bool val) { mustRecomputeHeadersFooters_ = val; }

    void setTotalBracketsWidth(double val) { totalBracketsWidth_ = val; }

private:

    bool firstSystem_ = true;
    bool firstSystemIndent_ = true;
    bool startWithLongNames_ = true;

    Fraction tick_{0, 1};
    Fraction startTick_;
    Fraction endTick_;
    bool isLayoutAll_ = false;

    Page *page_ = nullptr;
    page_idx_t pageIdx_ = 0; // index in Score->page()s

    std::vector<System *> systemList_; // reusable systems
    System *prevSystem_ = nullptr;     // used during page layout
    System *curSystem_ = nullptr;

    MeasureBase *prevMeasure_ = nullptr;
    MeasureBase *curMeasure_ = nullptr;
    MeasureBase *nextMeasure_ = nullptr;
    MeasureBase *systemOldMeasure_ = nullptr;
    MeasureBase *pageOldMeasure_ = nullptr;
    int measureNumber_ = 0;

    bool rangeDone_ = false;

    std::unordered_set<System *>
        systemsNeedingCentering_; // systems which have been laid out and still need SystemLayout::centerElementsBetweenStaves

    bool mustRecomputeHeadersFooters_ =
        false; // we may need to re-compute headers/footers after laying out all pages if they contained a page count

    // cache
    double totalBracketsWidth_ = -1.0;
};

class LayoutDebug {
public:

    static LayoutDebug *instance();

    struct CallBeginMarker {
        std::string_view name;
        muse::logger::Stream stream;

        CallBeginMarker(const std::string_view &n) : name(n) {}

        ~CallBeginMarker() { LayoutDebug::instance()->callBegin(name, stream.str()); }
    };

    struct CallMarker {
        CallBeginMarker begin(const std::string_view &n) { return CallBeginMarker(n); }

        ~CallMarker() { LayoutDebug::instance()->callEnd(); }
    };

    void callClear();
    void callBegin(const std::string_view &name, const std::string_view &info);
    void callEnd();

    void callDump(std::stringstream &ss) const;

    std::string callDump() const {
        std::stringstream ss;
        callDump(ss);
        return ss.str();
    }

    void callPrint();

private:

    struct Call {
        std::string name;
        std::string info;
        Call *top = nullptr;
        std::vector<Call> nested;
    };

    static void callDump(const LayoutDebug::Call &c, std::stringstream &ss, int &indent);

    Call *currentCall_;
    std::vector<Call> calls_;
};

class LayoutContext : public IGetScoreInternal {
public:
    LayoutContext(Score *s);
    ~LayoutContext();

    LayoutContext(const LayoutContext &) = delete;
    LayoutContext &operator=(const LayoutContext &) = delete;

    bool isValid() const;

    // Conf
    IEngravingFontPtr engravingFont() const;

    const LayoutConfiguration &conf() const { return configuration_; }

    // Dom access
    const DomAccessor &dom() const;
    DomAccessor &mutDom();

    // State
    const LayoutState &state() const;
    LayoutState &mutState();

    // Other
    const Selection &selection() const;
    void select(EngravingItem *item, SelectType = SelectType::SINGLE, staff_idx_t staff = 0);
    void deselect(EngravingItem *el);

private:

    Score *score() override { return score_; }

    Score *score_ = nullptr;

    LayoutConfiguration configuration_;
    DomAccessor dom_;
    LayoutState state_;
};
} // namespace mu::engraving::rendering::score
