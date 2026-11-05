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
 Definition of Score class.
*/

#include <set>
#include <memory>
#include <optional>

#include "global/types/ret.h"

#include "../engravingservices.h"

#include "../automation/automationtypes_fwd.h"
#include "../types/constants.h"

#include "../rendering/layoutoptions.h"
#include "../rendering/paddingtable.h"

#include "../style/style.h"
#include "../style/pagestyle.h"

#include "../editing/cmd.h"

#include "chordlist.h"
#include "dummyparent.h"
#include "guitarbend.h"
#include "input.h"
#include "mscore.h"
#include "property.h"
#include "scoreorder.h"
#include "segment.h"
#include "select.h"
#include "spannermap.h"
#include "synthesizerstate.h"
#include "rangelock.h"
#include "tuplet.h"

namespace mu::engraving {
class IMimeData;
} // namespace mu::engraving

namespace mu::engraving::read114 {
class Read114;
} // namespace mu::engraving::read114

namespace mu::engraving::read302 {
class Read302;
} // namespace mu::engraving::read302

namespace mu::engraving::read400 {
class Read400;
} // namespace mu::engraving::read400

namespace mu::engraving::read410 {
class Read410;
} // namespace mu::engraving::read410

namespace mu::engraving::read460 {
class Read460;
} // namespace mu::engraving::read460

namespace mu::engraving::read500 {
class Read500;
} // namespace mu::engraving::read500

namespace mu::engraving::write {
class Writer;
} // namespace mu::engraving::write

namespace mu::engraving::compat {
class WriteScoreHook;
} // namespace mu::engraving::compat

namespace mu::engraving {
class Articulation;
class Box;
class Bracket;
class Chord;
class ChordRest;
class Element;
class EventsHolder;
class Excerpt;
class FiguredBass;
class Harmony;
class InstrumentTemplate;
class InputState;
class KeyList;
class KeySigEvent;
class LinkedObjects;
class Lyrics;
class MasterScore;
class Measure;
class MeasureBase;
class Note;
class Page;
class Part;
class RehearsalMark;
class RepeatList;
struct RepeatSegmentInfo;
class Rest;
class RootItem;
class Score;
class IEngravingFont;
class Segment;
class Slur;
class Spanner;
class Staff;
class System;
class TDuration;
class TempoTimeline;
class Text;
class TimeSig;
class TimeSigMap;
class Transaction;
class TransactionManager;
class Tuplet;
class UndoableCommand;
class UndoStack;

class ShadowNote;

struct Interval;
struct NoteVal;
struct ShowAnchors;

enum class BeatType : unsigned char;
enum class Key : signed char;
enum class SegmentType;
enum class OttavaType : unsigned char;
enum class Prefer : char;
enum class AccidentalType : unsigned char;
enum class LayoutBreakType : unsigned char;
enum class CommandType : signed char;
enum class SystemDividerType : unsigned char;

struct Position {
    Segment *segment = nullptr;
    staff_idx_t staffIdx = muse::nidx;
    int line = 0;
    int fret = INVALID_FRET_INDEX;
    PointF pos;
    bool beyondScore = false;
};

struct ShowAnchors {
    ShowAnchors() = default;

    ShowAnchors(voice_idx_t vIdx, staff_idx_t stfIdx, const Fraction &sTickMain, const Fraction &eTickMain, const Fraction &sTickExt,
                const Fraction &eTickExt)
        : voiceIdx(vIdx), staffIdx(stfIdx), endStaffIdx(stfIdx + 1), startTickMainRegion(sTickMain), endTickMainRegion(eTickMain),
          startTickExtendedRegion(sTickExt), endTickExtendedRegion(eTickExt) {}

    ShowAnchors(voice_idx_t vIdx, staff_idx_t stfIdx, staff_idx_t endStfIdx, const Fraction &sTickMain, const Fraction &eTickMain,
                const Fraction &sTickExt, const Fraction &eTickExt)
        : voiceIdx(vIdx), staffIdx(stfIdx), endStaffIdx(endStfIdx), startTickMainRegion(sTickMain), endTickMainRegion(eTickMain),
          startTickExtendedRegion(sTickExt), endTickExtendedRegion(eTickExt) {}

    void reset() {
        voiceIdx = muse::nidx;
        staffIdx = muse::nidx;
        startTickMainRegion = Fraction(-1, 1);
        endTickMainRegion = Fraction(-1, 1);
        startTickExtendedRegion = Fraction(-1, 1);
        endTickExtendedRegion = Fraction(-1, 1);
    }

    bool operator==(const ShowAnchors &other) const {
        return voiceIdx == other.voiceIdx && staffIdx == other.staffIdx && endStaffIdx == other.endStaffIdx &&
               startTickMainRegion == other.startTickMainRegion && endTickMainRegion == other.endTickMainRegion &&
               startTickExtendedRegion == other.startTickExtendedRegion && endTickExtendedRegion == other.endTickExtendedRegion;
    }

    bool operator!=(const ShowAnchors &other) const { return !(*this == other); }

    voice_idx_t voiceIdx = muse::nidx;
    staff_idx_t staffIdx = muse::nidx;
    staff_idx_t endStaffIdx = muse::nidx;
    Fraction startTickMainRegion = Fraction(-1, 1);
    Fraction endTickMainRegion = Fraction(-1, 1);
    Fraction startTickExtendedRegion = Fraction(-1, 1);
    Fraction endTickExtendedRegion = Fraction(-1, 1);
};

struct ScoreChanges {
    int tickFrom = -1;
    int tickTo = -1;
    staff_idx_t staffIdxFrom = muse::nidx;
    staff_idx_t staffIdxTo = muse::nidx;

    bool isTextEditing = false;

    std::map<EngravingObject *, std::unordered_set<CommandType>> changedObjects;
    ElementTypeSet changedTypes;
    PropertyIdSet changedPropertyIdSet;
    StyleIdSet changedStyleIdSet;

    bool isValidBoundary() const {
        bool tickRangeValid = (tickFrom != -1 && tickTo != -1);
        bool staffRangeValid = (staffIdxFrom != muse::nidx && staffIdxTo != muse::nidx);

        return tickRangeValid && staffRangeValid;
    }

    bool isValid() const { return isValidBoundary() || !changedTypes.empty(); }

    void clear() { *this = ScoreChanges(); }
};

// A cache that follows the committed changes of a master score (the playback
// model). MasterScore calls it synchronously, in registration order, when a
// transaction commits or an undo/redo completes.
class IScoreChangesListener {
public:
    virtual ~IScoreChangesListener() = default;
    virtual void onScoreChanges(const ScoreChanges &changes) = 0;
};

//---------------------------------------------------------------------------------------
//   @@ Score
//   @P composer        string            composer of the score (read only)
//   @P duration        int               duration of score in seconds (read only)
//   @P excerpts        array[Excerpt]    the list of the excerpts (linked parts)
//   @P firstMeasure    Measure           the first measure of the score (read only)
//   @P firstMeasureMM  Measure           the first multi-measure rest measure of the score (read only)
//   @P harmonyCount    int               number of harmony items (read only)
//   @P hasHarmonies    bool              score has chord symbols (read only)
//   @P hasLyrics       bool              score has lyrics (read only)
//   @P keysig          int               key signature at the start of the score (read only)
//   @P lastMeasure     Measure           the last measure of the score (read only)
//   @P lastMeasureMM   Measure           the last multi-measure rest measure of the score (read only)
//   @P lastSegment     Segment           the last score segment (read-only)
//   @P lyricCount      int               number of lyric items (read only)
//   @P name            string            name of the score
//   @P nmeasures       int               number of measures (read only)
//   @P npages          int               number of pages (read only)
//   @P nstaves         int               number of staves (read only)
//   @P ntracks         int               number of tracks (staves * 4) (read only)
// not to be documented?
//   @P parts           array[Part]       the list of parts (read only)
//
//    a Score has always an associated MasterScore
//---------------------------------------------------------------------------------------

class Score : public EngravingObject {
    OBJECT_ALLOCATOR(engraving, Score)
    DECLARE_CLASSOF(ElementType::SCORE)

public:
    const EngravingServices &services() const { return services_; }

    const std::shared_ptr<IEngravingConfiguration> &configuration() const { return services_.configuration; }

    const std::shared_ptr<IEngravingFontsProvider> &engravingFonts() const { return services_.engravingFonts; }

    const std::shared_ptr<IEngravingContextConfiguration> &contextConfiguration() const { return services_.contextConfiguration; }

    const std::shared_ptr<rendering::IScoreRenderer> &renderer() const { return services_.renderer; }

    Score(const Score &) = delete;
    Score &operator=(const Score &) = delete;
    virtual ~Score();
    Score *clone();

    virtual bool isMaster() const { return false; }

    virtual bool readOnly() const;

    static void onElementDestruction(EngravingItem *se);
    // Whether the score has not started destructing: code that reaches a score
    // through a pointer it does not own checks this before touching it.
    static bool isAlive(const Score *score);

    void scanElements(std::function<void(EngravingItem *)> func) override;

    RootItem *rootItem() const { return rootItem_; }

    DummyParent *dummy() const { return dummy_; }

    ShadowNote *shadowNote() const;

    void rebuildBspTree();

    bool noStaves() const { return staves_.empty(); }

    void insertPart(Part *, size_t targetPartIdx);
    void appendPart(Part *);
    void removePart(Part *);
    void insertStaff(Staff *, staff_idx_t);
    void appendStaff(Staff *);
    void cmdRemoveStaff(staff_idx_t staffIdx);
    void removeStaff(Staff *);
    void addMeasure(MeasureBase *, MeasureBase *);

    Excerpt *excerpt() { return excerpt_; }

    void setExcerpt(Excerpt *e) { excerpt_ = e; }

    void cmdRemovePart(Part *);
    void cmdAddOttava(OttavaType);
    void addNoteLine();

    void cmdAddGrace(NoteType, int);
    void cmdToggleLayoutBreak(LayoutBreakType);
    void removeChordRest(ChordRest *cr, bool clearSegment);
    void cmdToggleMmrest();
    void cmdToggleHideEmpty();
    void cmdSetHideStaffIfEmptyOverride(staff_idx_t staffIdx, System *system, engraving::AutoOnOff value);
    void cmdMoveRest(Rest *, DirectionV);
    void cmdMoveLyrics(Lyrics *, DirectionV);

    bool appendMeasuresFromScore(Score *score, const Fraction &startTick, const Fraction &endTick);
    bool appendScore(Score *, bool addPageBreak = false, bool addSectionBreak = true);

    const std::vector<Staff *> &staves() const { return staves_; }

    size_t nstaves() const { return staves_.size(); }

    size_t visibleStavesCount() const;
    bool allStavesInvisible() const;

    size_t ntracks() const { return staves_.size() * VOICES; }

    staff_idx_t staffIdx(const Staff *) const;
    staff_idx_t staffIdx(const Part *) const;

    Staff *staff(size_t n) const { return (n < staves_.size()) ? staves_.at(n) : nullptr; }

    Staff *staffById(const muse::ID &staffId) const;
    Part *partById(const muse::ID &partId) const;

    void clearSystemObjectStaves();
    void addSystemObjectStaff(Staff *staff);
    void removeSystemObjectStaff(Staff *staff);

    const std::vector<Staff *> &systemObjectStaves() const { return systemObjectStaves_; }

    const std::vector<Staff *> systemObjectStavesWithTopStaff() const;

    MeasureBase *pos2measureBase(const PointF &, bool scanMeasuresOnly, staff_idx_t *staffIdx, int *pitch, Segment **,
                                 PointF *offset) const;
    Measure *pos2measure(const PointF &p, staff_idx_t *staffIdx, int *pitch, Segment **seg, PointF *offset) const;
    void dragPosition(const PointF &, staff_idx_t *staffIdx, Segment **, double spacingFactor = 0.5, bool allowTimeAnchor = false) const;

    void undoAddElement(EngravingItem *element, bool addToLinkedStaves = true, bool ctrlModifier = false,
                        EngravingItem *elementToRelink = nullptr);
    void undoAddCR(ChordRest *element, Measure *, const Fraction &tick);
    void undoRemoveElement(EngravingItem *element, bool removeLinked = true);
    void undoRemoveHopoText(HammerOnPullOffText *hopoText);
    void undoChangeSpannerElements(Spanner *spanner, EngravingItem *startElement, EngravingItem *endElement);
    void undoChangeElement(EngravingItem *oldElement, EngravingItem *newElement);
    void undoChangeChordRestLen(ChordRest *cr, const TDuration &);
    void undoRemovePart(Part *part, size_t partIdx = muse::nidx);
    void undoInsertPart(Part *part, size_t targetPartIndex);
    void undoRemoveStaff(Staff *staff);
    void undoInsertStaff(Staff *staff, staff_idx_t idx, bool createRests = true);
    bool undoPropertyChanged(EngravingItem *item, Pid propId, const PropertyValue &propValue,
                             PropertyFlags propFlags = PropertyFlags::NOSTYLE);
    void undoPropertyChanged(EngravingObject *, Pid, const PropertyValue &v, PropertyFlags ps = PropertyFlags::NOSTYLE);
    UndoStack *undoStack() const;
    TransactionManager *transactionManager() const;
    void undo(UndoableCommand *) const;
    void undoRemoveMeasures(Measure *, Measure *, bool preserveTies = false, bool moveStaffTypeChanges = true);
    void undoChangeMeasureRepeatCount(Measure *m, int count, staff_idx_t staffIdx);
    void undoInsertTime(const Fraction &tick, const Fraction &len);
    void undoChangeStyleVal(Sid idx, const PropertyValue &v);
    void undoChangeStyleValues(std::unordered_map<Sid, PropertyValue> values);
    void undoChangePageNumberOffset(int po);
    void undoChangeParent(EngravingItem *element, EngravingItem *parent, staff_idx_t _staff, bool changeLinksParents = true);
    void undoResetPlayCountTextSettings(BarLine *bl);
    void undoUpdatePlayCountText(Measure *m);
    void undoChangeBarLineType(BarLine *bl, BarLineType barType, bool allStaves, bool replace = false);

    void updateInstrumentChangeTranspositions(KeySigEvent &key, Staff *staff, const Fraction &tick);

    Measure *undoGetMeasure(const Fraction &tick);

    Note *setGraceNote(Chord *, int pitch, NoteType type, int len);

    GuitarBend *addGuitarBend(GuitarBendType type, Note *note, Note *endNote = nullptr);

    // Musical insertion with explicit targets; no selection or input cursor changes.
    Segment *setNoteRestAt(Segment *, track_idx_t track, NoteVal nval, Fraction, DirectionV stemDirection = DirectionV::AUTO,
                           bool forceAccidental = false, const std::set<SymId> &articulationIds = {}, bool rhythmic = false,
                           EngravingItem **lastEntered = nullptr);
    Segment *setNoteRest(Segment *, track_idx_t track, NoteVal nval, Fraction, DirectionV stemDirection = DirectionV::AUTO,
                         bool forceAccidental = false, const std::set<SymId> &articulationIds = {}, bool rhythmic = false,
                         InputState *externalInputState = nullptr);
    void changeCRlen(ChordRest *cr, const TDuration &);
    void changeCRlen(ChordRest *cr, const Fraction &, bool fillWithRest = true);
    void createCRSequence(const Fraction &f, ChordRest *cr, const Fraction &tick);

    Fraction makeGap(Segment *, track_idx_t track, const Fraction &, Tuplet *, bool keepChord = false, bool deleteAnnotations = true);
    bool makeGapVoice(Segment *seg, track_idx_t track, Fraction len, const Fraction &tick, bool deleteAnnotations = true);

    Rest *addRest(const Fraction &tick, track_idx_t track, TDuration, Tuplet *);
    Rest *addRest(Segment *seg, track_idx_t track, TDuration d, Tuplet *);
    Chord *addChord(const Fraction &tick, TDuration d, Chord *oc, bool genTie, Tuplet *tuplet);

    Tuplet *addTuplet(ChordRest *destinationChordRest, Fraction ratio, TupletNumberType numberType, TupletBracketType bracketType);
    Tuplet *addTuplet(ChordRest *destinationChordRest, const Fraction &ratio, const TDuration &baseLen, TupletNumberType numberType,
                      TupletBracketType bracketType);
    bool canAddTuplet(const ChordRest *chordRest, const Fraction &ratio, const TDuration &baseLen) const;

    ChordRest *addClone(ChordRest *cr, const Fraction &tick, const TDuration &d);
    Rest *setRest(const Fraction &tick, track_idx_t track, const Fraction &, bool useDots, Tuplet *tuplet, bool useFullMeasureRest = true);
    std::vector<Rest *> setRests(const Fraction &tick, track_idx_t track, const Fraction &l, bool useDots, Tuplet *tuplet,
                                 bool useFullMeasureRest = true);

    ChordRest *searchNote(const Fraction &tick, track_idx_t track) const;

    // undo/redo ops
    void addElement(EngravingItem *);
    void doUndoAddElement(EngravingItem *);
    void removeElement(EngravingItem *);
    void doUndoRemoveElement(EngravingItem *);

    Slur *addSlur(ChordRest *firstChordRest, ChordRest *secondChordRest, const Slur *slurTemplate);
    TextBase *addText(TextStyleType type, EngravingItem *destinationElement = nullptr);

    void deleteItem(EngravingItem *);
    void deleteMeasures(MeasureBase *firstMeasure, MeasureBase *lastMeasure, bool preserveTies = false);
    void restoreInitialKeySigAndTimeSig();
    void reconnectSlurs(MeasureBase *mbStart, MeasureBase *mbLast);
    void cmdDeleteSelection();
    std::vector<ChordRest *> deleteRange(Segment *segStart, Segment *segEnd, track_idx_t trackStart, track_idx_t trackEnd,
                                         const SelectionFilter &filter, bool selectionContainsMultiNoteChords);
    void cmdFullMeasureRest();

    void startCmd(const TranslatableString &actionName);                                                  // start undoable command
    void endCmd(bool rollback = false, bool layoutAllParts = false, bool keepRolledBackElements = false); // end undoable command
    void update();
    void undoRedo(bool undo, EditData *);

    virtual void setUpdateAll();
    void setLayoutAll(staff_idx_t staff = muse::nidx, const EngravingItem *e = nullptr);
    void setLayout(const Fraction &tick, staff_idx_t staff, const EngravingItem *e = nullptr);
    void setLayout(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff1, staff_idx_t staff2, const EngravingItem *e = nullptr);
    virtual CmdState &cmdState();
    virtual const CmdState &cmdState() const;
    virtual void addLayoutFlags(LayoutFlags);
    virtual void setInstrumentsChanged(bool);

    void cmdToggleAutoplace(bool all);

    bool playNote() const { return updateState_.playNote; }

    void setPlayNote(bool v) { updateState_.playNote = v; }

    bool playChord() const { return updateState_.playChord; }

    void setPlayChord(bool v) { updateState_.playChord = v; }

    bool selectionEmpty() const { return selection_.staffStart() == selection_.staffEnd(); }

    bool selectionChanged() const { return updateState_.selectionChanged; }

    void setSelectionChanged(bool val) { updateState_.selectionChanged = val; }

    const RectF &refreshRect() const { return updateState_.refresh; }

    void addRefresh(const RectF &);

    void clearRefreshRect() { updateState_.refresh = RectF(); }

    const std::vector<Part *> &parts() const;
    size_t visiblePartCount() const;
    std::vector<Part *> visibleParts() const;
    std::vector<SharedPart *> sharedParts() const;
    bool hasSharedParts() const;

    using StaffAccepted = std::function<bool(const Staff &)>;
    std::set<staff_idx_t> staffIdxSetFromRange(const track_idx_t trackFrom, const track_idx_t trackTo,
                                               StaffAccepted staffAccepted = StaffAccepted()) const;

    void appendPart(const InstrumentTemplate *);
    void sortSystemObjects(std::vector<staff_idx_t> &dst);
    void sortStaves(std::vector<staff_idx_t> &dst);

    bool isShowInvisible() const { return showInvisible_; }

    bool showUnprintable() const { return showUnprintable_; }

    bool showFrames() const { return showFrames_; }

    bool showPageborders() const { return showPageborders_; }

    bool showSoundFlags() const { return showSoundFlags_; }

    bool markIrregularMeasures() const { return markIrregularMeasures_; }

    bool showInstrumentNames() const { return showInstrumentNames_; }

    void setShowInvisible(bool v);
    void setShowUnprintable(bool v);
    void setShowFrames(bool v);
    void setShowPageborders(bool v);
    void setShowSoundFlags(bool v);
    void setMarkIrregularMeasures(bool v);

    void setShowInstrumentNames(bool v) { showInstrumentNames_ = v; }

    void hideAnchors() { showAnchors_.reset(); }

    void setShowAnchors(const ShowAnchors &showAnchors);

    const ShowAnchors &showAnchors() const { return showAnchors_; }

    ChordRest *getSelectedChordRest() const;
    std::set<ChordRest *> getSelectedChordRests() const;
    void getSelectedStartEndChordRests(ChordRest *&cr1, ChordRest *&cr2) const;

    void select(EngravingItem *item, SelectType = SelectType::SINGLE, staff_idx_t staff = 0);
    void select(const std::vector<EngravingItem *> &items, SelectType = SelectType::SINGLE, staff_idx_t staff = 0);
    void selectSimilar(EngravingItem *e, bool sameStaff);
    void selectSimilarInRange(EngravingItem *e);
    static void collectMatch(ElementPattern *p, EngravingItem *e);
    static void collectNoteMatch(NotePattern *p, EngravingItem *e);
    void deselect(EngravingItem *obj);

    void deselectAll() { selection_.deselectAll(); }

    void updateSelection() { selection_.update(); }

    EngravingItem *getSelectedElement() const { return selection_.element(); }

    const Selection &selection() const { return selection_; }

    Selection &selection() { return selection_; }

    const SelectionFilter &selectionFilter() const { return selectionFilter_; }

    SelectionFilter &selectionFilter() { return selectionFilter_; }

    void setSelection(const Selection &s);

    bool canReselectItem(const EngravingItem *item) const;

    Fraction pos();
    Measure *tick2measure(const Fraction &tick) const;
    Measure *tick2measureMM(const Fraction &tick) const;
    MeasureBase *tick2measureBase(const Fraction &tick) const;
    Segment *tick2segment(const Fraction &tick, bool first, SegmentType st, bool useMMrest = false) const;
    Segment *tick2segment(const Fraction &tick) const;
    Segment *tick2segment(const Fraction &tick, bool first) const;
    Segment *tick2segmentMM(const Fraction &tick, bool first, SegmentType st) const;
    Segment *tick2segmentMM(const Fraction &tick) const;
    Segment *tick2segmentMM(const Fraction &tick, bool first) const;
    Segment *tick2leftSegment(const Fraction &tick, bool useMMrest = false, SegmentType segType = SegmentType::ChordRest) const;
    Segment *tick2rightSegment(const Fraction &tick, bool useMMrest = false, SegmentType segType = SegmentType::ChordRest) const;

    Segment *tick2leftSegmentMM(const Fraction &tick) { return tick2leftSegment(tick, /* useMMRest */ true); }

    void updateTicksAndTimeSigMap();

    void updateTicksAndTimeSigMapLater() { needUpdateTicksAndTimeSigMap_ = true; }

    bool needUpdateTicksAndTimeSigMap() const { return needUpdateTicksAndTimeSigMap_; }

    EngravingItem *nextElement();
    EngravingItem *prevElement();
    ChordRest *nextTrack(ChordRest *cr, bool skipMeasureRepeatRests = true);
    ChordRest *prevTrack(ChordRest *cr, bool skipMeasureRepeatRests = true);
    ChordRest *cmdNextPrevSystem(ChordRest *, bool);
    Box *cmdNextPrevFrame(MeasureBase *, bool) const;
    EngravingItem *cmdNextPrevSection(EngravingItem *, bool) const;
    MeasureBase *getNextPrevSectionBreak(MeasureBase *, bool) const;
    EngravingItem *getScoreElementOfMeasureBase(MeasureBase *) const;

    int fileDivision(int t) const { return static_cast<int>(((int64_t) t * Constants::DIVISION + fileDivision_ / 2) / fileDivision_); }

    void setFileDivision(int t) { fileDivision_ = t; }

    bool dirty() const;

    bool savedCapture() const { return savedCapture_; }

    void setSavedCapture(bool v) { savedCapture_ = v; }

    bool printing() const { return printing_; }

    void setPrinting(bool val) { printing_ = val; }

    bool hasCorruptedMeasures() const { return corrupted_; }

    void setHasCorruptedMeasures(bool val) { corrupted_ = val; }

    bool isOpen() const;
    void setIsOpen(bool open);

    ChordList *chordList() { return &chordList_; }

    const ChordList *chordList() const { return &chordList_; }

    void checkChordList();

    MStyle &style() { return style_; }

    const MStyle &style() const { return style_; }

    PageSizeGetAccessor pageSize() const { return PageSizeGetAccessor(style_); }

    PageSizeSetAccessor pageSize() { return PageSizeSetAccessor(style_); }

    void resetStyleValue(Sid styleToReset);
    void resetStyleValues(const StyleIdSet &styleIdSet);

    void setStyle(const MStyle &s, const bool overlap = false);

    TranslatableString getTextStyleUserName(TextStyleType tid) const;

    // These position are in ticks and not uticks
    Fraction loopInTick() const { return loopBoundaryTick(LoopBoundaryType::LoopIn); }

    Fraction loopOutTick() const { return loopBoundaryTick(LoopBoundaryType::LoopOut); }

    void setLoopInTick(const Fraction &tick) { setLoopBoundaryTick(LoopBoundaryType::LoopIn, tick); }

    void setLoopOutTick(const Fraction &tick) { setLoopBoundaryTick(LoopBoundaryType::LoopOut, tick); }

    Fraction loopBoundaryTick(LoopBoundaryType type) const;
    void setLoopBoundaryTick(LoopBoundaryType type, Fraction tick);

    bool noteEntryMode() const { return inputState().noteEntryMode(); }

    bool usingNoteEntryMethod(NoteEntryMethod m) { return inputState().usingNoteEntryMethod(m); }

    const InputState &inputState() const { return is_; }

    InputState &inputState() { return is_; }

    void setInputState(const InputState &st) { is_ = st; }

    void spatiumChanged(double oldValue, double newValue);
    void styleChanged() override;

    BeatType tick2beatType(const Fraction &tick) const;

    int mscVersion() const { return mscVersion_; }

    void setMscVersion(int v) { mscVersion_ = v; }

    void addLyrics(const Fraction &tick, staff_idx_t staffIdx, const String &);

    void updateSwing();

    void updateCapo(bool ignoreNotationUpdate = false);
    void updateChannel();

    void cmdConcertPitchChanged(bool);

    virtual TimeSigMap *sigmap() const;

    BeatsPerSecond tempo(const Fraction &tick) const;
    BeatsPerSecond multipliedTempo(const Fraction &tick) const;
    BeatsPerSecond multipliedTempoAtUtick(int utick) const;

    Text *getText(TextStyleType subtype) const;

    ScoreOrder scoreOrder() const;
    void setScoreOrder(ScoreOrder order);
    void updateBracesAndBarlines(Part *part, size_t index);
    void setBracketsAndBarlines();
    void remapBracketsAndBarlines();

    void lassoSelect(const RectF &);
    void lassoSelectEnd();

    Page *searchPage(const PointF &) const;
    std::vector<System *> searchSystem(const PointF &p, bool includeMeasurelessSystems = true, const System *preferredSystem = nullptr,
                                       double spacingFactor = 0.5, double preferredSpacingFactor = 1.0) const;
    MeasureBase *searchMeasureBase(const PointF &p, bool scanMeasuresOnly = false, const System *preferredSystem = nullptr,
                                   double spacingFactor = 0.5, double preferredSpacingFactor = 1.0) const;

    bool getPosition(Position *pos, const PointF &, voice_idx_t voice) const;

    void cmdDeleteTuplet(Tuplet *, bool replaceWithRest);

    void adjustKeySigs(track_idx_t sidx, track_idx_t eidx, KeyList km);
    KeyList keyList() const;

    /// To be used together with setExpandRepeats.
    /// For bigger operations, where suboperations might also use it,
    /// where those need to have the same value for expandRepeats.
    const RepeatList &repeatList() const;
    /// For small, one-step operations, where you need to get the relevant repeatList just once
    const RepeatList &repeatList(bool expandRepeats, bool updateTies = true) const;
    /// Read-only snapshot, safe to call mid-edit (e.g. before a structural change invalidates the current repeatList)
    std::vector<RepeatSegmentInfo> repeatSegmentInfoList(bool expandRepeats) const;
    /// Always the repeat-expanded list, regardless of the current playback setting
    const RepeatList &expandedRepeatList() const;

    void invalidateRepeatList();

    double utick2utime(int tick) const;
    int utime2utick(double utime) const;

    const TempoTimeline &tempoTimeline() const;
    const TempoTimeline &tempoTimeline(bool expandRepeats) const;
    void setTempoTimelineOverride(std::optional<TempoTimeline> timeline);

    virtual size_t npages() const { return pages_.size(); }

    virtual page_idx_t pageIdx(const Page *page) const { return muse::indexOf(pages_, page); }

    virtual const std::vector<Page *> &pages() const { return pages_; }

    virtual std::vector<Page *> &pages() { return pages_; }

    const std::vector<System *> &systems() const { return systems_; }

    std::vector<System *> &systems() { return systems_; }

    MeasureBaseList *measures() { return &measures_; }

    bool checkHasMeasures() const;
    MeasureBase *first() const;
    MeasureBase *firstMM() const;
    MeasureBase *last() const;
    Measure *firstMeasure() const;
    Measure *firstMeasureMM() const;
    Measure *lastMeasure() const;
    Measure *lastMeasureMM() const;
    MeasureBase *measure(int idx) const;
    Measure *crMeasure(int idx) const;

    Fraction endTick() const;

    Segment *firstSegment(SegmentType s) const;
    Segment *firstSegmentMM(SegmentType s) const;
    Segment *lastSegment() const;
    Segment *lastSegmentMM() const;

    void connectTies(bool silent = false);
    void undoRemoveStaleTieJumpPoints(bool undo = true);

    void scanElementsInRange(std::function<void(EngravingItem *)> func);

    int fileDivision() const { return fileDivision_; } ///< division of current loading *.msc file

    void splitStaff(staff_idx_t staffIdx, int splitPoint);
    FiguredBass *addFiguredBass();
    void expandVoice(Segment *s, track_idx_t track);
    void expandVoice();

    void cmdEnterRest(const TDuration &d);
    void enterRest(const TDuration &d, InputState *externalInputState = nullptr);
    void addInterval(int, const std::vector<Note *> &);
    void cmdCreateTuplet(ChordRest *, Tuplet *);

    bool autoLayoutEnabled() const;

    void doLayout();
    void doLayoutRange(const Fraction &st, const Fraction &et);

    SynthesizerState &synthesizerState() { return synthesizerState_; }

    void setSynthesizerState(const SynthesizerState &s);

    MasterScore *masterScore() const { return masterScore_; }

    void setMasterScore(MasterScore *s) { masterScore_ = s; }

    const std::map<String, String> &metaTags() const { return metaTags_; }

    std::map<String, String> &metaTags() { return metaTags_; }

    void setMetaTags(const std::map<String, String> &t) { metaTags_ = t; }

    String metaTag(const String &tag) const;
    void setMetaTag(const String &tag, const String &val);

    int pageNumberOffset() const { return pageNumberOffset_; }

    void setPageNumberOffset(int v) { pageNumberOffset_ = v; }

    String mscoreVersion() const { return mscoreVersion_; }

    int mscoreRevision() const { return mscoreRevision_; }

    void setMscoreVersion(const String &val) { mscoreVersion_ = val; }

    void setMscoreRevision(int val) { mscoreRevision_ = val; }

    //! NOTE Layout
    const LayoutOptions &layoutOptions() const { return layoutOptions_; }

    void setLayoutMode(LayoutMode lm) { layoutOptions_.mode = lm; }

    void setShowVBox(bool v) { layoutOptions_.isShowVBox = v; }

    double noteHeadWidth() const { return layoutOptions_.noteHeadWidth; }

    void setNoteHeadWidth(double n) { layoutOptions_.noteHeadWidth = n; }

    // temporary methods
    bool isLayoutMode(LayoutMode lm) const { return layoutOptions_.isMode(lm); }

    LayoutMode layoutMode() const { return layoutOptions_.mode; }

    bool lineMode() const { return layoutOptions_.isMode(LayoutMode::LINE); }

    bool linearMode() const { return layoutOptions_.isLinearMode(); }

    // ----

    void cmdSelectAll();
    void cmdSelectSection();

    struct InsertMeasureOptions {
        InsertMeasureOptions() {}

        bool createMeasureRests = false;
        bool moveSignaturesClef = true;
        bool needDeselectAll = true;
        bool cloneBoxToAllParts = true;
        bool moveStaffTypeChanges = true;
        bool ignoreBarLines = false;
    };

    MeasureBase *insertMeasure(ElementType type, MeasureBase *beforeMeasure = nullptr,
                               const InsertMeasureOptions &options = InsertMeasureOptions());
    MeasureBase *insertBox(ElementType type, MeasureBase *beforeMeasure = nullptr,
                           const InsertMeasureOptions &options = InsertMeasureOptions());
    MeasureBase *insertBox(MeasureBase *box, MeasureBase *beforeMeasure = nullptr,
                           const InsertMeasureOptions &options = InsertMeasureOptions());

    std::list<Score *> scoreList();

    void appendMeasures(int numMeasures);

    const std::multimap<int, Spanner *> &spanner() const { return spanner_.map(); }

    SpannerMap &spannerMap() { return spanner_; }

    const SpannerMap &spannerMap() const { return spanner_; }

    std::vector<Spanner *> spannerList() const; // Return all spanners as a vector for Plugin API
    bool isSpannerStartEnd(const Fraction &tick, track_idx_t track) const;
    void removeSpanner(Spanner *);
    void addSpanner(Spanner *, bool computeStartEnd = true);
    void checkSpanner(const Fraction &startTick, const Fraction &lastTick, bool removeOrphans = true);

    const std::set<Spanner *> &unmanagedSpanners() const { return unmanagedSpanner_; }

    void addUnmanagedSpanner(Spanner *);
    void removeUnmanagedSpanner(Spanner *);

    ChordRest *findCR(Fraction tick, track_idx_t track) const;
    ChordRest *findChordRestEndingBeforeTickInStaff(const Fraction &tick, staff_idx_t staffIdx) const;
    ChordRest *findChordRestEndingBeforeTickInStaffAndVoice(const Fraction &tick, staff_idx_t staffIdx, voice_idx_t voice) const;
    ChordRest *findChordRestEndingBeforeTickInTrack(const Fraction &tick, track_idx_t trackIdx) const;
    void insertTime(const Fraction &tickPos, const Fraction &tickLen, const std::vector<RepeatSegmentInfo> &oldSegments);

    std::shared_ptr<IEngravingFont> engravingFont() const { return engravingFont_; }

    void setEngravingFont(std::shared_ptr<IEngravingFont> f) { engravingFont_ = f; }

    std::vector<staff_idx_t> uniqueStaves() const;

    size_t nmeasures() const;

    bool hasHarmonies() const;
    int harmonyCount() const;

    bool hasLyrics() const;
    int lyricCount() const;
    std::vector<Lyrics *> lyrics() const;
    String extractLyrics() const;

    int keysig() const;
    int duration() const;
    int durationWithoutRepeats() const;

    void cmdRemoveEmptyTrailingMeasures();

    Measure *firstTrailingMeasure(ChordRest **cr = nullptr);

    muse::Ret sanityCheckLocal();

    bool checkKeys();

    void switchToPageMode();

    PropertyValue getProperty(Pid) const override;
    bool setProperty(Pid, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    /// For MasterScores: returns the filename without extension
    /// For Scores: returns the excerpt name
    virtual String name() const;

    void cmdTimeDelete();
    bool checkTimeDelete(Segment *startSegment, Segment *endSegment);
    void doTimeDelete(Segment *startSegment, Segment *endSegment);
    void doTimeDeleteForMeasure(Measure *, Segment *, const Fraction &);

    void forAllLyrics(std::function<void(Lyrics *)> f);

    void updatePaddingTables();

    const PaddingTables &paddingTables() const { return paddingTables_; }

    const RangeLocks *systemLocks() const { return &systemLocks_; }

    void addSystemLock(const RangeLock *lock);
    void removeSystemLock(const RangeLock *lock);

    void clearSystemLocks() { systemLocks_.clear(); }

    const RangeLocks *pageLocks() const { return &pageLocks_; }

    void addPageLock(const RangeLock *lock);
    void removePageLock(const RangeLock *lock);

    void rebuildFretBox();

    const std::map<size_t, std::array<SystemDivider *, 2>> systemDividers() const { return systemDividers_; }

    SystemDivider *systemDivider(size_t systemIdx, SystemDividerType type) const;
    void addSystemDivider(size_t systemIdx, SystemDivider *divider);

    virtual AutomationDataConstPtr automationData() const;
    virtual void editAutomationPoints(const AutomationCurveKey &key, AutomationPointEdits &edits, bool undoable = true);

    friend class Chord;

    BracketType bracketType(staff_idx_t staffIdx, size_t idx) const;
    size_t bracketSpan(staff_idx_t staffIdx, size_t idx) const;
    const std::vector<BracketItem *> &brackets(staff_idx_t staffIdx) const;
    std::vector<BracketItem *> &brackets(staff_idx_t staffIdx);
    size_t bracketLevels(staff_idx_t staffIdx) const;

protected:
    virtual void onTimeInserted(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &oldSegments);

    friend class MasterScore;
    explicit Score(const EngravingServices &services);
    Score(MasterScore *, bool forcePartStyle = true);
    Score(MasterScore *, const MStyle &);

    int fileDivision_ = 0; // division of current loading *.msc file
    SynthesizerState synthesizerState_;

private:

    friend class read302::Read302;
    friend class read400::Read400;
    friend class read410::Read410;
    friend class read460::Read460;
    friend class read500::Read500;
    friend class write::Writer;

    static std::set<Score *> validScores;

    Note *getSelectedNote();
    ChordRest *findChordRestEndingBeforeTickInStaffAndVoice(const Fraction &tick, staff_idx_t staffIdx, bool forceVoice,
                                                            voice_idx_t voice) const;

    void checkScore();

    std::vector<Fraction> splitGapToMeasureBoundaries(ChordRest *, Fraction);

    void doSelect(EngravingItem *e, SelectType type, staff_idx_t staffIdx);
    void selectSingle(EngravingItem *e, staff_idx_t staffIdx);
    void selectAdd(EngravingItem *e);
    void selectRange(EngravingItem *e, staff_idx_t staffIdx);

    bool trySelectSimilarInRange(EngravingItem *e);
    bool tryExtendSingleSelectionToRange(EngravingItem *e, staff_idx_t staffIdx);
    bool tryExtendRangeSelectionToElem(EngravingItem *e);

    void rebuildTimeSigMap(Measure *m);

    void doUndoRemoveStaleTieJumpPoints(Tie *tie, bool undo = true);
    void doUndoResetPartialSlur(Slur *slur, bool undo);

    void deleteOrShortenOutSpannersFromRange(const Fraction &t1, const Fraction &t2, track_idx_t trackStart, track_idx_t trackEnd,
                                             const SelectionFilter &filter);
    void deleteSlursFromRange(const Fraction &t1, const Fraction &t2, track_idx_t trackStart, track_idx_t trackEnd,
                              const SelectionFilter &filter);
    void deleteAnnotationsFromRange(Segment *segStart, Segment *segEnd, track_idx_t trackStart, track_idx_t trackEnd,
                                    const SelectionFilter &filter);

    void deleteRangeAtTrack(std::vector<ChordRest *> &crsToSelect, const track_idx_t track, Segment *startSeg, const Fraction &endTick,
                            Tuplet *currentTuplet, const SelectionFilter &filter, bool selectionContainsMultiNoteChords);

    muse::ID newStaffId() const;
    muse::ID newPartId() const;

    void assignIdIfNeed(Staff &staff) const;
    void assignIdIfNeed(Part &part) const;

    void updateStavesNumberForSystems();

    FBox *findFretBox() const;

    const EngravingServices &services_;
    MasterScore *masterScore_ = nullptr;
    Excerpt *excerpt_ = nullptr;

    String mscoreVersion_;
    int mscoreRevision_ = 0;

    std::shared_ptr<IEngravingFont> engravingFont_ = nullptr;

    int pageNumberOffset_ = 0; // Offset for page numbers.

    UpdateState updateState_;

    MeasureBaseList measures_; // here are the notes
    std::vector<Part *> parts_;
    std::vector<Staff *> staves_;
    std::vector<Staff *> systemObjectStaves_;
    RangeLocks systemLocks_;

    RangeLocks pageLocks_;

    SpannerMap spanner_;
    std::set<Spanner *> unmanagedSpanner_;

    std::vector<std::vector<BracketItem *>> brackets_;

    //
    // objects generated by layout:
    //
    std::vector<Page *> pages_;     // pages are build from systems
    std::vector<System *> systems_; // measures are accumulated to systems

    std::map<size_t, std::array<SystemDivider *, 2>> systemDividers_; // list of system dividers (left and right) indexed by system

    InputState is_;
    MStyle style_;
    ChordList chordList_;

    bool showInvisible_ = true;
    bool showUnprintable_ = true;
    bool showFrames_ = true;
    bool showPageborders_ = false;
    bool showSoundFlags_ = true;
    bool markIrregularMeasures_ = true;
    bool showInstrumentNames_ = true;
    bool printing_ = false;     // True if we are drawing to a printer
    bool savedCapture_ = false; // True if we saved an image capture
    bool corrupted_ = false;
    bool needUpdateTicksAndTimeSigMap_ = false;

    ShowAnchors showAnchors_;

    ScoreOrder scoreOrder_;                   // used for score ordering
    int mscVersion_ = Constants::MSC_VERSION; // version of current loading *.msc file

    bool isOpen_ = false;

    std::map<String, String> metaTags_;

    Selection selection_;
    SelectionFilter selectionFilter_;

    RootItem *rootItem_ = nullptr;
    DummyParent *dummy_ = nullptr;
    LayoutOptions layoutOptions_;

    ShadowNote *shadowNote_ = nullptr;

    PaddingTables paddingTables_;
    double minimumPaddingUnit_ = 0.0;
};

static inline Score *toScore(EngravingObject *e) {
    assert(!e || e->isScore());
    return static_cast<Score *>(e);
}

static inline const Score *toScore(const EngravingObject *e) {
    assert(!e || e->isScore());
    return static_cast<const Score *>(e);
}

//---------------------------------------------------------
//   ScoreLoad
//---------------------------------------------------------

class ScoreLoad {
    static int loading_;

public:
    ScoreLoad() { ++loading_; }

    ~ScoreLoad() { --loading_; }

    static bool loading() { return loading_ > 0; }
};

DECLARE_OPERATORS_FOR_FLAGS(LayoutFlags)
} // namespace mu::engraving
