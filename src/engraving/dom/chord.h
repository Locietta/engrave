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
 Definition of classes Chord, HelpLine and NoteList.
*/

#include <functional>
#include <set>
#include <vector>

#include "articulation.h"
#include "chordrest.h"

#include "draw/types/color.h"

namespace mu::engraving {
class AccidentalState;
class Arpeggio;
class Chord;
class Hook;
class LedgerLine;
class Note;
class NoteEventList;
class Stem;
class StemSlash;
class Transaction;
class TremoloSingleChord;
class TremoloTwoChord;

enum class NoteType : unsigned char {
    ///.\{
    NORMAL = 0,
    ACCIACCATURA = 0x1,
    APPOGGIATURA = 0x2, // grace notes
    GRACE4 = 0x4,
    GRACE16 = 0x8,
    GRACE32 = 0x10,
    GRACE8_AFTER = 0x20,
    GRACE16_AFTER = 0x40,
    GRACE32_AFTER = 0x80,
    INVALID = 0xFF
    ///\}
};

constexpr NoteType operator|(NoteType t1, NoteType t2) {
    return static_cast<NoteType>(static_cast<unsigned char>(t1) | static_cast<unsigned char>(t2));
}

constexpr bool operator&(NoteType t1, NoteType t2) { return static_cast<unsigned char>(t1) & static_cast<unsigned char>(t2); }

enum class PlayEventType : unsigned char {
    ///.\{
    Auto, ///< Play events for all notes are calculated by MuseScore.
    User, ///< Some play events are modified by user. Those events are written into the mscx file.
    ///.\}
};

class GraceNotesGroup final : public std::vector<Chord *>, public EngravingItem {
    OBJECT_ALLOCATOR(engraving, GraceNotesGroup)
public:
    GraceNotesGroup *clone() const override { return new GraceNotesGroup(*this); }

    GraceNotesGroup(DummyParentOr<Chord> c);

    Chord *parent() const { return _parent; }

    void setPos(double x, double y) override;

    Segment *appendedSegment() const { return _appendedSegment; }

    void setAppendedSegment(Segment *s) { _appendedSegment = s; }

    void addToShape();

private:
    Chord *_parent = nullptr;
    Segment *_appendedSegment = nullptr; // the graceNoteGroup is appended to this segment
};

//---------------------------------------------------------
//   @@ Chord
///    Graphic representation of a chord.
///    Single notes are handled as degenerated chords.
//
//   @P beam          Beam          the beam of the chord, if any (read only)
//   @P graceNotes    array[Chord]  the list of grace note chords (read only)
//   @P hook          Hook          the hook of the chord, if any (read only)
//   @P lyrics        array[Lyrics] the list of lyrics (read only)
//   @P notes         array[Note]   the list of notes (read only)
//   @P stem          Stem          the stem of the chord, if any (read only)
//   @P stemSlash     StemSlash     the stem slash of the chord (acciaccatura), if any (read only)
//   @P stemDirection Direction     the stem direction of the chord: AUTO, UP, DOWN (read only)
//---------------------------------------------------------

struct NoteParenthesisInfo {
    NoteParenthesisInfo(Parenthesis *lParen, Parenthesis *rParen, std::vector<Note *> nList);
    ~NoteParenthesisInfo();

    Parenthesis *leftParen() const { return leftParen_; }

    Parenthesis *rightParen() const { return rightParen_; }

    const std::vector<Note *> &notes() const { return notes_; }

    void insertNote(Note *note);
    void removeNote(Note *note);

private:
    Parenthesis *leftParen_ = nullptr;
    Parenthesis *rightParen_ = nullptr;
    std::vector<Note *> notes_;
};

using NoteParenthesisInfoList = std::vector<NoteParenthesisInfo *>;

class Chord final : public ChordRest {
    OBJECT_ALLOCATOR(engraving, Chord)
    DECLARE_CLASSOF(ElementType::CHORD)

public:

    ~Chord();
    Chord &operator=(const Chord &) = delete;

    void scanElements(std::function<void(EngravingItem *)> func) override;

    Chord *clone() const override { return new Chord(*this, false); }

    EngravingItem *linkedClone() override { return new Chord(*this, true); }

    void undoUnlink() override;

    void setScore(Score *s) override;
    double intrinsicMag() const override;
    double mag() const override;
    double noteHeadWidth() const;

    EngravingItem *drop(Transaction &tx, EditData &) override;

    void setColor(const Color &c) override;
    void setStemDirection(DirectionV d);

    DirectionV stemDirection() const { return stemDirection_; }

    Chord *prev() const;
    Chord *next() const;

    bool isUiItem() const { return isUiItem_; }

    void setIsUiItem(bool val) { isUiItem_ = val; }

    const std::vector<LedgerLine *> &ledgerLines() const { return ledgerLines_; }

    std::vector<LedgerLine *> &ledgerLines() { return ledgerLines_; }

    void resizeLedgerLinesTo(size_t newSize);

    double defaultStemLength() const { return defaultStemLength_; }

    void setDefaultStemLength(double l) { defaultStemLength_ = l; }

    void setBeamExtension(double extension);

    std::vector<Note *> &notes() { return notes_; }

    const std::vector<Note *> &notes() const { return notes_; }

    const NoteParenthesisInfoList &noteParentheses() const { return noteParens_; }

    const NoteParenthesisInfo *findNoteParenthesisInfo(const Note *note) const;
    NoteParenthesisInfo *findNoteParenthesisInfo(const Parenthesis *paren);
    void addNoteParenthesisInfo(NoteParenthesisInfo *noteParenInfo);
    void removeNoteParenthesisInfo(const NoteParenthesisInfo *noteParenInfo);
    void addNoteToParenthesisInfo(Note *note, const Parenthesis *paren);
    void removeNoteFromParenthesisInfo(Note *note, const Parenthesis *paren);

    bool isChordPlayable() const;
    void setIsChordPlayable(const bool isPlayable);

    // Chord has at least one Note
    Note *upNote() const;
    Note *downNote() const;
    int upString() const;
    int downString() const;
    std::vector<int> noteDistances() const;

    double maxHeadWidth() const;

    Note *findNote(int pitch, int skip = 0) const;

    Stem *stem() const { return stem_; }

    Arpeggio *arpeggio() const { return arpeggio_; }

    void setArpeggio(Arpeggio *a) { arpeggio_ = a; }

    Arpeggio *spanArpeggio() const { return spanArpeggio_; }

    void setSpanArpeggio(Arpeggio *a) { spanArpeggio_ = a; }

    void undoChangeSpanArpeggio(Arpeggio *a);

    TremoloType tremoloType() const;
    TremoloTwoChord *tremoloTwoChord() const;
    TremoloSingleChord *tremoloSingleChord() const;

    void setTremoloTwoChord(TremoloTwoChord *tr, bool applyLogic = true);
    void setTremoloSingleChord(TremoloSingleChord *tr);

    ChordLine *chordLine() const;

    bool endsNoteAnchoredLine() const { return endsNoteAnchoredLine_; }

    void setEndsNoteAnchoredLine(bool val) { endsNoteAnchoredLine_ = val; }

    void updateEndsNoteAnchoredLine();

    StemSlash *stemSlash() const { return stemSlash_; }

    bool showStemSlash() const { return showStemSlash_; }

    void setShowStemSlashInAdvance();
    void requestShowStemSlash(bool show);

    void setShowStemSlash(bool show) { showStemSlash_ = show; }

    bool slash();
    void setSlash(bool flag, bool stemless);
    void removeMarkings(bool keepTremolo = false) override;

    const std::vector<Chord *> &graceNotes() const { return graceNotes_; }

    std::vector<Chord *> &graceNotes() { return graceNotes_; }

    std::vector<Chord *> allGraceChordsOfMainChord();

    void removeAllGraceNotes() { graceNotes_.clear(); }

    GraceNotesGroup &graceNotesBefore(bool filterUnplayable = false) const;
    GraceNotesGroup &graceNotesAfter(bool filterUnplayable = false) const;

    size_t graceIndex() const { return graceIndex_; }

    void setGraceIndex(size_t val) { graceIndex_ = val; }

    Chord *graceNoteAt(size_t idx) const;

    int line(bool up) const { return up ? upLine() : downLine(); }

    int line() const { return ldata()->up ? upLine() : downLine(); }

    int upLine() const;
    int downLine() const;
    double rightEdge() const override;

    bool underBeam() const;

    Hook *hook() const { return hook_; }

    void setHook(Hook *h) { hook_ = h; }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    Note *selectedNote() const;

    PointF pagePos() const override; ///< position in page coordinates
    void cmdUpdateNotes(AccidentalState *, staff_idx_t staffIdx);

    NoteType noteType() const { return noteType_; }

    void setNoteType(NoteType t);

    bool isGrace() const { return noteType_ != NoteType::NORMAL; }

    void toGraceAfter();

    bool isPreBendOrGraceBendStart() const;
    bool isGraceBendEnd() const;

    bool isTrillCueNote() const { return isTrillCueNote_; }

    void setIsTrillCueNote(bool v);

    void setTrack(track_idx_t val) override;

    double dotPosX() const { return dotPosX_; }

    void setDotPosX(double x) { dotPosX_ = x; }

    bool noStem() const { return noStem_; }

    void setNoStem(bool val) { noStem_ = val; }

    double spaceLw() { return spaceLw_; }

    void setSpaceLw(double lw) { spaceLw_ = lw; }

    double spaceRw() { return spaceRw_; }

    void setSpaceRw(double rw) { spaceRw_ = rw; }

    AutoOnOff combineVoice() const { return combineVoice_; }

    void setCombineVoice(AutoOnOff v) { combineVoice_ = v; }

    bool shouldCombineVoice() const;
    static bool combineVoice(const Chord *chord1, const Chord *chord2);

    PlayEventType playEventType() const { return playEventType_; }

    void setPlayEventType(PlayEventType v) { playEventType_ = v; }

    std::vector<NoteEventList> getNoteEventLists();
    void setNoteEventLists(std::vector<NoteEventList> &nel);

    TremoloChordType tremoloChordType() const;

    std::vector<Articulation *> &articulations() { return articulations_; }

    const std::vector<Articulation *> &articulations() const { return articulations_; }

    std::set<SymId> articulationSymbolIds() const;
    Articulation *hasArticulation(const Articulation *);
    Tapping *tapping() const;

    bool hasSingleArticulation() const { return articulations_.size() == 1; }

    void updateArticulations(const std::set<SymId> &newArticulationIds,
                             ArticulationsUpdateMode replaceMode = ArticulationsUpdateMode::Insert);

    void localSpatiumChanged(double oldValue, double newValue) override;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;
    bool isUserModified() const override;

    void reset() override;

    Segment *segment() const override;
    Measure *measure() const override;

    void sortNotes();

    Chord *nextTiedChord(bool backwards = false, bool sameSize = true) const;

    Fraction endTickIncludingTied() const;

    EngravingItem *nextElement() override;
    EngravingItem *prevElement() override;
    EngravingItem *nextSegmentElement() override;
    EngravingItem *lastElementBeforeSegment();
    EngravingItem *prevSegmentElement() override;

    Note *firstGraceOrNote();

    void undoChangeProperty(Pid id, const PropertyValue &newValue);
    void undoChangeProperty(Pid id, const PropertyValue &newValue, PropertyFlags ps) override;

    void styleChanged() override;

    bool allowKerningAbove() const { return allowKerningAbove_; }

    bool allowKerningBelow() const { return allowKerningBelow_; }

    void computeKerningExceptions();

    Ornament *findOrnament(bool forPlayback = false) const;

    const std::set<Spanner *> &startingSpanners() const { return startingSpanners_; }

    const std::set<Spanner *> &endingSpanners() const { return endingSpanners_; }

    void addStartingSpanner(Spanner *spanner) { startingSpanners_.insert(spanner); }

    void removeStartingSpanner(Spanner *spanner) { startingSpanners_.erase(spanner); }

    void addEndingSpanner(Spanner *spanner) { endingSpanners_.insert(spanner); }

    void removeEndingSpanner(Spanner *spanner) { endingSpanners_.erase(spanner); }

    bool shouldHaveStem() const;
    bool shouldHaveHook() const;
    void createStem();
    void removeStem();
    void createHook();

    double upPos() const override;
    double downPos() const override;

    struct StartEndSlurs {
        bool startUp = false;
        bool startDown = false;
        bool endUp = false;
        bool endDown = false;

        void reset() {
            startUp = false;
            startDown = false;
            endUp = false;
            endDown = false;
        }
    };

    StartEndSlurs &startEndSlurs() { return startEndSlurs_; }

    bool allNotesTiedToNext() const;
    bool allElementsInvisible() const;

private:

    friend class Factory;

    Chord(DummyParentOr<Segment> parent);
    Chord(const Chord &, bool link = false);

    // `includeTemporarySiblings`: whether items that are deleted & recreated during every layout should also be processed
    void processSiblings(std::function<void(EngravingItem *)> func, bool includeTemporarySiblings) const;

    std::vector<Note *> notes_; // sorted to decreasing line step
    std::vector<LedgerLine *> ledgerLines_;
    NoteParenthesisInfoList noteParens_;

    Stem *stem_ = nullptr;
    Hook *hook_ = nullptr;
    StemSlash *stemSlash_ = nullptr; // for grace notes
    bool showStemSlash_ = false;

    Arpeggio *arpeggio_ = nullptr;     // arpeggio which starts on the chord
    Arpeggio *spanArpeggio_ = nullptr; // arpeggio which spans over this chord

    TremoloTwoChord *tremoloTwoChord_ = nullptr;
    TremoloSingleChord *tremoloSingleChord_ = nullptr;

    bool endsNoteAnchoredLine_ = false; // true if this chord is the ending point of a glissando (needed for layout)
    std::vector<Chord *> graceNotes_;   // storage for all grace notes
    mutable GraceNotesGroup graceNotesBefore_ = GraceNotesGroup(this); // will store before-chord grace notes
    mutable GraceNotesGroup graceNotesAfter_ = GraceNotesGroup(this);  // will store after-chord grace notes
    size_t graceIndex_ = 0;                                            // if this is a grace note, index in parent list

    bool isTrillCueNote_ = false;

    DirectionV stemDirection_ = DirectionV::AUTO;
    NoteType noteType_ = NoteType::NORMAL; // mark grace notes: acciaccatura and appoggiatura
    bool noStem_ = false;
    PlayEventType playEventType_ = PlayEventType::Auto; // play events were modified by user

    double spaceLw_ = 0.0;
    double spaceRw_ = 0.0;

    double defaultStemLength_ = 0.0;

    bool isUiItem_ = false;

    double dotPosX_ = 0.0;

    StartEndSlurs startEndSlurs_;

    std::set<Spanner *> startingSpanners_; // spanners starting on this item
    std::set<Spanner *> endingSpanners_;   // spanners ending on this item

    bool allowKerningAbove_ = true;
    bool allowKerningBelow_ = true;

    AutoOnOff combineVoice_ = AutoOnOff::AUTO;

    std::vector<Articulation *> articulations_;
};
} // namespace mu::engraving
