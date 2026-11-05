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

#include "durationtype.h"
#include "mscore.h"
#include "pitchspelling.h"
#include "../types/types.h"

#include "selectionfilter.h"

namespace mu::engraving {
class Score;
class Page;
class System;
class ChordRest;
class EngravingItem;
class Segment;
class Note;
class Measure;
class MeasureBase;
class Chord;
class Tuplet;
class GuitarBend;
class Box;

//---------------------------------------------------------
//   ElementPattern
//---------------------------------------------------------

struct ElementPattern {
    std::vector<EngravingItem *> el;
    int type = 0;
    int subtype = 0;
    staff_idx_t staffStart = 0;
    staff_idx_t staffEnd = 0; // exclusive
    voice_idx_t voice = 0;
    bool subtypeValid = false;
    Fraction durationTicks{-1, 1};
    Fraction beat{0, 0};
    const Measure *measure = nullptr;
    const System *system = nullptr;
};

//---------------------------------------------------------
//   NotePattern
//---------------------------------------------------------

struct NotePattern : ElementPattern {
    std::vector<Note *> el;
    int pitch = -1;
    int string = INVALID_STRING_INDEX;
    int tpc = Tpc::TPC_INVALID;
    NoteHeadGroup notehead = NoteHeadGroup::HEAD_INVALID;
    TDuration durationType = TDuration();
    NoteType type = NoteType::INVALID;
};

//---------------------------------------------------------
//   SelState
//---------------------------------------------------------

enum class SelState : char {
    NONE,  // nothing is selected
    LIST,  // disjoint selection
    RANGE, // adjacent selection, a range in one or more staves
    // is selected
};

//---------------------------------------------------------
//   SelectedSymbol
//---------------------------------------------------------

struct SelectedSymbol {
    track_idx_t track = muse::nidx;
    int tickOffset = 0;
    int segDelta = 0;
    EngravingItem *element = nullptr;
};

//-------------------------------------------------------------------
//   Selection
//    For SelState::LIST state only visible elements can be selected
//    (no Chord element etc.).
//-------------------------------------------------------------------

class Selection {
public:
    Selection() {
        score_ = 0;
        state_ = SelState::NONE;
    }

    Selection(Score *);

    Score *score() const { return score_; }

    SelState state() const { return state_; }

    bool isNone() const { return state_ == SelState::NONE; }

    bool isRange() const { return state_ == SelState::RANGE; }

    bool isList() const { return state_ == SelState::LIST; }

    void setState(SelState s);

    //! NOTE If locked, the selected items should not be changed.
    void lock(const String &reason) { lockReason_ = reason; }

    void unlock(const String & /*reason*/) { lockReason_.clear(); } // reason for clarity

    bool isLocked() const { return !lockReason_.isEmpty(); }

    const String &lockReason() const { return lockReason_; }

    const std::vector<EngravingItem *> &elements() const { return el_; }

    std::vector<EngravingItem *> elements(ElementType type) const;
    std::vector<Note *> noteList(track_idx_t track = muse::nidx) const;

    const std::list<EngravingItem *> uniqueElements() const;
    std::vector<Note *> uniqueNotes(track_idx_t track = muse::nidx, bool tied = true) const;

    bool isSingle() const { return (state_ == SelState::LIST) && (el_.size() == 1); }

    bool elementsSelected(const ElementTypeSet &types) const;

    void add(EngravingItem *);
    void deselectAll();
    void remove(EngravingItem *);
    void clear();
    EngravingItem *element() const;
    ChordRest *cr() const;
    ChordRest *firstChordRest(track_idx_t track = muse::nidx) const;
    ChordRest *lastChordRest(track_idx_t track = muse::nidx) const;
    Measure *findMeasure() const;
    MeasureBase *startMeasureBase() const;
    MeasureBase *endMeasureBase() const;
    std::vector<System *> selectedSystems() const;
    std::vector<Page *> pagesContainingSelection() const;
    void update();
    void updateState();
    void dump();
    String mimeType() const;
    muse::ByteArray mimeData() const;

    Segment *startSegment() const { return startSegment_; }

    Segment *endSegment() const { return endSegment_; }

    void setStartSegment(Segment *s);
    void setEndSegment(Segment *s);

    Box *startBox() const { return startBox_; }

    Box *endBox() const { return endBox_; }

    void setRange(Segment *startSegment, Segment *endSegment, staff_idx_t staffStart, staff_idx_t staffEnd, Box *startBox = nullptr,
                  Box *endBox = nullptr);
    void setRangeTicks(const Fraction &tick1, const Fraction &tick2, staff_idx_t staffStart, staff_idx_t staffEnd, Box *startBox = nullptr,
                       Box *endBox = nullptr);

    Segment *activeSegment() const { return activeSegment_; }

    void setActiveSegment(Segment *s) { activeSegment_ = s; }

    ChordRest *activeCR() const;
    bool isStartActive() const;
    bool isEndActive() const;
    ChordRest *currentCR() const;
    Fraction tickStart() const;
    Fraction tickEnd() const;

    staff_idx_t staffStart() const { return staffStart_; }

    staff_idx_t staffEnd() const { return staffEnd_; }

    track_idx_t activeTrack() const { return activeTrack_; }

    void setStaffStart(int v) { staffStart_ = v; }

    void setStaffEnd(int v) { staffEnd_ = v; }

    void setActiveTrack(track_idx_t v) { activeTrack_ = v; }

    bool canCopy() const;
    void updateSelectedElements();
    bool measureBaseRange(MeasureBase **mb1, MeasureBase **mb2) const;
    void extendRangeSelection(ChordRest *cr);
    void extendRangeSelection(Segment *seg, Segment *segAfter, staff_idx_t staffIdx, const Fraction &tick, const Fraction &etick,
                              Box *box = nullptr);
    bool rangeContainsMultiNoteChords() const;

private:
    muse::ByteArray staffMimeData() const;
    muse::ByteArray symbolListMimeData() const;
    SelectionFilter selectionFilter() const;

    bool canSelect(const EngravingItem *e) const { return selectionFilter().canSelect(e); }

    bool canSelectNoteIdx(size_t noteIdx, size_t totalNotesInChord, bool rangeContainsMultiNoteChords) const;

    bool canSelectVoice(track_idx_t track) const { return selectionFilter().canSelectVoice(track); }

    void appendFiltered(EngravingItem *e);
    void appendFiltered(const std::unordered_set<EngravingItem *> &elems);
    void appendChordRest(ChordRest *cr);
    void appendTupletHierarchy(Tuplet *innermostTuplet);
    void appendGuitarBend(GuitarBend *guitarBend);

    ChordRest *firstChordRestInRange(track_idx_t preferredTrack = muse::nidx) const;
    ChordRest *lastChordRestInRange(track_idx_t preferredTrack = muse::nidx) const;

    void validateBoxesAndSegments(); // Checks that the Boxes/segments match (asserts and clears the Box if not)

    bool rangeIsValid() const;

    Score *score_ = nullptr;
    SelState state_ = SelState::NONE;
    std::vector<EngravingItem *> el_; // valid in mode SelState::LIST

    staff_idx_t staffStart_ = 0; // valid if selState is SelState::RANGE
    staff_idx_t staffEnd_ = 0;

    Segment *startSegment_ = nullptr;
    Segment *endSegment_ = nullptr; // next segment after selection

    Box *startBox_ = nullptr;
    Box *endBox_ = nullptr; // the last Box included in the selection

    Fraction plannedTick1_{-1, 1}; // Will be actually selected on updateSelectedElements() call.
    Fraction plannedTick2_{-1, 1}; // Used by setRangeTicks() to restore proper selection after
    // command end in case some changes are expected to segments'
    // structure (e.g. MMRests reconstruction).

    Segment *activeSegment_ = nullptr;
    track_idx_t activeTrack_ = 0;

    Fraction currentTick_; // tracks the most recent selection
    track_idx_t currentTrack_ = 0;

    bool rangeContainsMultiNoteChords_ = false; // cached - calculating this isn't free

    String lockReason_;
};
} // namespace mu::engraving
