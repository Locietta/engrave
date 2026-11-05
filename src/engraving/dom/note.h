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

#include "containers.h"

#include "engravingitem.h"
#include "interval.h"
#include "noteevent.h"
#include "noteval.h"
#include "pitchspelling.h"
#include "symbol.h"
#include "tie.h"
#include "tiejumppointlist.h"

namespace mu::engraving {
class Accidental;
class AccidentalState;
class Chord;
class Factory;
class NoteDot;
class Score;
class Spanner;
class StaffType;
class Text;
class Tie;
class Transaction;
enum class AccidentalType : unsigned char;
enum class NoteType : unsigned char;
struct NoteParenthesisInfo;

static constexpr int MAX_DOTS = 4;

//--------------------------------------------------------------------------------
// LINE ATTACHMENT POINT
// Represents the attachment point of any line (tie, slur, glissando...)
// with respect to the note. Each note can hold a vector of line attach points, which
// it may use to make spacing decision with the surrounding items.
//--------------------------------------------------------------------------------
class LineAttachPoint {
public:
    LineAttachPoint(EngravingItem *l, double x, double y, bool start) : line_(l), pos_(PointF(x, y)), start_(start) {}

    const EngravingItem *line() const { return line_; }

    const PointF pos() const { return pos_; }

    bool start() const { return start_; }

private:
    EngravingItem *line_ = nullptr;
    PointF pos_ = PointF(0.0, 0.0);
    bool start_ = true;
};

//---------------------------------------------------------
//   @@ NoteHead
//---------------------------------------------------------

class NoteHead final : public Symbol {
    OBJECT_ALLOCATOR(engraving, NoteHead)
    DECLARE_CLASSOF(ElementType::NOTEHEAD)

public:
    NoteHead(DummyParentOr<Note> parent);
    NoteHead(const NoteHead &) = default;
    NoteHead &operator=(const NoteHead &) = delete;

    NoteHead *clone() const override { return new NoteHead(*this); }

    NoteHeadGroup headGroup() const;
};

static const int INVALID_LINE = -10000;

//---------------------------------------------------------------------------------------
//   @@ Note
///    Graphic representation of a note.
//
//   @P accidental       Accidental       note accidental (null if none)
//   @P accidentalType   int              note accidental type
//   @P dots             array[NoteDot]   list of note dots (some can be null, read only)
//   @P dotsCount        int              number of note dots (read only)
//   @P elements         array[EngravingItem]   list of elements attached to notehead
//   @P fret             int              fret number in tablature
//   @P ghost            bool             ghost note (guitar: death note)
//   @P headScheme       enum (NoteHeadScheme.HEAD_AUTO, .HEAD_NORMAL, .HEAD_PITCHNAME, .HEAD_PITCHNAME_NO_ACCIDENTALS, .HEAD_PITCHNAME_GERMAN, .HEAD_PITCHNAME_GERMAN_NO_ACCIDENTALS, .HEAD_SHAPE_NOTE_4, .HEAD_SHAPE_NOTE_7_AIKIN, .HEAD_SHAPE_NOTE_7_FUNK, .HEAD_SHAPE_NOTE_7_WALKER, .HEAD_SOLFEGE, .HEAD_SOLFEGE_FIXED)
//   @P headGroup        enum (NoteHeadGroup.HEAD_NORMAL, .HEAD_BREVIS_ALT, .HEAD_CROSS, .HEAD_DIAMOND, .HEAD_DO, .HEAD_FA, .HEAD_LA, .HEAD_MI, .HEAD_RE, .HEAD_SLASH, .HEAD_LARGE_DIAMOND, .HEAD_SOL, .HEAD_TI, .HEAD_XCIRCLE, .HEAD_TRIANGLE)
//   @P headType         enum (NoteHeadType.HEAD_AUTO, .HEAD_BREVIS, .HEAD_HALF, .HEAD_QUARTER, .HEAD_WHOLE)
//   @P hidden           bool             hidden, not played note (read only)
//   @P line             int              notehead position (read only)
//   @P mirror           bool             mirror notehead on x axis (read only)
//   @P pitch            int              midi pitch
//   @P play             bool             play note
//   @P ppitch           int              actual played midi pitch (honoring ottavas) (read only)
//   @P isSmall          bool             small notehead
//   @P string           int              string number in tablature
//   @P subchannel       int              midi subchannel (for midi articulation) (read only)
//   @P tieBack          Tie              note backward tie (null if none, read only)
//   @P tieFor           Tie              note forward tie (null if none, read only)
//   @P tpc              int              tonal pitch class, as per concert pitch setting
//   @P tpc1             int              tonal pitch class, non transposed
//   @P tpc2             int              tonal pitch class, transposed
//   @P tuning           float            tuning offset in cent
//   @P userDotPosition  enum (Direction.AUTO, Direction.DOWN, Direction.UP)
//   @P userMirror       enum (DirectionH.AUTO, DirectionH.LEFT, DirectionH.RIGHT)
//   @P veloOffset       int
//   @P veloType         enum (Note.OFFSET_VAL, Note.USER_VAL)
//---------------------------------------------------------------------------------------

class Note final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Note)
    DECLARE_CLASSOF(ElementType::NOTE)

public:
    enum class SlideType : unsigned char { Undefined = 0, UpToNote, DownToNote, UpFromNote, DownFromNote };

    enum DisplayFretOption : signed char { Hide = -1, NoHarmonic, NaturalHarmonic, ArtificialHarmonic };

    ~Note();

    std::vector<Note *> compoundNotes() const;

    Note &operator=(const Note &) = delete;

    virtual Note *clone() const override { return new Note(*this, false); }

    Chord *chord() const { return (Chord *) ownershipParent(); }

    void setOwnershipParent(DummyParentOr<Chord> ch);

    void undoUnlink() override;

    double mag() const override;
    EngravingItem *elementBase() const override;

    void scanElements(std::function<void(EngravingItem *)> func) override;
    void setTrack(track_idx_t val) override;

    staff_idx_t vStaffIdx() const override;

    int playTicks() const;
    Fraction playTicksFraction() const;

    double headWidth() const;
    double headHeight() const;
    double tabHeadWidth(const StaffType *tab = 0) const;
    double tabHeadHeight(const StaffType *tab = 0) const;
    PointF stemDownNW() const;
    PointF stemUpSE() const;
    double bboxXShift() const;
    double noteheadCenterX() const;
    double bboxRightPos() const;
    double headBodyWidth() const;
    double outsideTieAttachX(bool up) const;

    NoteHeadScheme headScheme() const { return headScheme_; }

    void updateHeadGroup(const NoteHeadGroup headGroup);

    NoteHeadGroup headGroup() const { return headGroup_; }

    NoteHeadType headType() const { return headType_; }

    void setHeadScheme(NoteHeadScheme val);
    void setHeadGroup(NoteHeadGroup val);
    void setHeadType(NoteHeadType t);

    int subtype() const override { return int(headGroup_); }

    TranslatableString subtypeUserName() const override;

    void setPitch(int val, bool notifyAboutChanged = true);
    void setPitch(int pitch, int tpc1, int tpc2);

    int pitch() const { return pitch_; }

    double centOffset() const { return centOffset_; }

    void setCentOffset(double v) { centOffset_ = v; }

    int quarterToneOffset() const { return std::round(centOffset_ / 50); }

    int ottaveCapoFret() const;
    int linkedOttavaPitchOffset() const;
    int ppitch() const; // playback pitch
    int epitch() const; // effective pitch
    int octave() const;
    int playingOctave() const;

    double tuning() const { return tuning_; }

    void setTuning(double v) { tuning_ = v; }

    double playingTuning() const;
    void undoSetTpc(int v);
    int transposition() const;

    bool fixed() const { return fixed_; }

    void setFixed(bool v) { fixed_ = v; }

    int fixedLine() const { return fixedLine_; }

    void setFixedLine(int v) { fixedLine_ = v; }

    int tpc() const;

    int tpc1() const { return tpc_[0]; } // non transposed tpc

    int tpc2() const { return tpc_[1]; } // transposed tpc

    String tpcUserName(bool explicitAccidental = false, bool full = false) const;

    void setTpc(int v);

    void setTpc1(int v) { tpc_[0] = v; }

    void setTpc2(int v) { tpc_[1] = v; }

    void setTpcFromPitch(Prefer prefer = Prefer::NEAREST);
    int tpc1default(int pitch) const;
    int tpc2default(int pitch) const;
    int transposeTpc(int tpc) const;

    int playingTpc() const;

    Accidental *accidental() const { return accidental_; }

    void setAccidental(Accidental *a) { accidental_ = a; }

    AccidentalType accidentalType() const;
    void setAccidentalType(AccidentalType type);

    int line() const;

    void setLine(int n) { line_ = n; }

    int fret() const { return fret_; }

    void setFret(int val) { fret_ = val; }

    float harmonicFret() const { return harmonicFret_; }

    void setHarmonicFret(float val) { harmonicFret_ = val; }

    int harmonicPitchOffset() const { return harmonicPitchOffset_; }

    void setHarmonicPitchOffset(int val) { harmonicPitchOffset_ = val; }

    DisplayFretOption displayFret() const { return displayFret_; }

    void setDisplayFret(DisplayFretOption val) { displayFret_ = val; }

    String fretString() const { return fretString_; }

    void setFretString(const String &s) { fretString_ = s; }

    bool negativeFretUsed() const;

    int string() const { return string_; }

    void setString(int val) { string_ = val; }

    int stringOrLine() const;

    bool ghost() const { return ghost_; }

    void setGhost(bool val) { ghost_ = val; }

    bool deadNote() const { return deadNote_; }

    void setDeadNote(bool deadNote) { deadNote_ = deadNote; }

    bool fretConflict() const { return fretConflict_; }

    void setFretConflict(bool val) { fretConflict_ = val; }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    bool isSmall() const { return isSmall_; }

    void setSmall(bool val);

    bool play() const { return play_; }

    void setPlay(bool val) { play_ = val; }

    GuitarBend *bendFor() const;
    GuitarBend *bendBack() const;
    GuitarBend *diveFor() const;
    GuitarBend *diveBack() const;

    Tie *tieFor() const { return tieFor_; }

    Tie *tieBack() const { return tieBack_; }

    Tie *tieForNonPartial() const;
    Tie *tieBackNonPartial() const;
    LaissezVib *laissezVib() const;
    PartialTie *incomingPartialTie() const;
    PartialTie *outgoingPartialTie() const;
    void setTieFor(Tie *t);
    void setTieBack(Tie *t);
    Note *firstTiedNote(bool ignorePlayback = true) const;
    Note *lastTiedNote(bool ignorePlayback = true) const;

    int unisonIndex() const;
    void disconnectTiedNotes();
    void connectTiedNotes();

    void setupAfterRead(const Fraction &tick, bool pasteMode);

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    bool hidden() const { return hidden_; }

    void setHidden(bool val) { hidden_ = val; }

    bool dotsHidden() const { return dotsHidden_; }

    void setDotsHidden(bool val) { dotsHidden_ = val; }

    NoteType noteType() const;
    String noteTypeUserName() const;

    ElementList &el() { return el_; }

    const ElementList &el() const { return el_; }

    int subchannel() const { return subchannel_; }

    void setSubchannel(int val) { subchannel_ = val; }

    DirectionH userMirror() const { return userMirror_; }

    void setUserMirror(DirectionH d) { userMirror_ = d; }

    DirectionV userDotPosition() const { return userDotPosition_; }

    void setUserDotPosition(DirectionV d) { userDotPosition_ = d; }

    DirectionV dotPosition() const { return dotPosition_; }

    void setDotPosition(DirectionV d) { dotPosition_ = d; }

    bool dotIsUp() const; // actual dot position

    void reset() override;

    float userVelocityFraction() const;

    int userVelocity() const { return userVelocity_; }

    void setUserVelocity(int v) { userVelocity_ = v; }

    void setOnTimeOffset(int v);
    void setOffTimeOffset(int v);

    int customizeVelocity(int velo) const;

    NoteDot *dot(int n) { return dots_.at(n); }

    const std::vector<NoteDot *> &dots() const { return dots_; }

    std::vector<NoteDot *> &dots() { return dots_; }

    void updateAccidental(AccidentalState *);
    void updateLine();
    void setNval(const NoteVal &, Fraction = {-1, 1});

    NoteEventList &playEvents() { return playEvents_; }

    const NoteEventList &playEvents() const { return playEvents_; }

    NoteEvent *noteEvent(int idx) { return &playEvents_[idx]; }

    void setPlayEvents(const NoteEventList &l) { playEvents_ = l; }

    const std::vector<Spanner *> &spannerFor() const { return spannerFor_; }

    const std::vector<Spanner *> &spannerBack() const { return spannerBack_; }

    void addSpannerBack(Spanner *e) {
        if (!muse::contains(spannerBack_, e)) {
            spannerBack_.push_back(e);
        }
    }

    bool removeSpannerBack(Spanner *e) { return muse::remove(spannerBack_, e); }

    void addSpannerFor(Spanner *e) {
        if (!muse::contains(spannerFor_, e)) {
            spannerFor_.push_back(e);
        }
    }

    bool removeSpannerFor(Spanner *e) { return muse::remove(spannerFor_, e); }

    bool transposeDiatonic(int interval, bool keepAlterations, bool useDoubleAccidentals);
    bool transpose(Interval interval, bool useDoubleSharpsFlats);

    void localSpatiumChanged(double oldValue, double newValue) override;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;
    void styleChanged() override;

    bool mark() const { return mark_; }

    void setMark(bool v) const { mark_ = v; }

    void setScore(Score *s) override;

    static SymId noteHead(int direction, NoteHeadGroup, NoteHeadType, int tpc, Key key, NoteHeadScheme scheme);
    static SymId noteHead(int direction, NoteHeadGroup, NoteHeadType);
    NoteVal noteVal() const;

    EngravingItem *nextInEl(EngravingItem *e);
    EngravingItem *prevInEl(EngravingItem *e);
    EngravingItem *nextElement() override;
    EngravingItem *prevElement() override;
    EngravingItem *lastElementBeforeSegment();
    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    std::vector<Note *> tiedNotes() const;

    void setOffTimeType(int v) { offTimeType_ = v; }

    void setOnTimeType(int v) { onTimeType_ = v; }

    int offTimeType() const { return offTimeType_; }

    int onTimeType() const { return onTimeType_; }

    void attachSlide(SlideType slideType);

    bool hasSlideToNote() const;
    bool hasSlideFromNote() const;

    SlideType slideToType() const { return slideToType_; }

    SlideType slideFromType() const { return slideFromType_; }

    void setParenthesesMode(const ParenthesesMode &v, bool addToLinked = true, bool generated = false) override;

    const NoteParenthesisInfo *parenthesisInfo() const;

    void setHarmonic(bool val) { harmonic_ = val; }

    bool harmonic() const { return harmonic_; }

    bool isGrace() const;

    bool isPreBendOrDiveStart() const;
    bool isGraceBendStart() const;
    bool isContinuationOfBend() const;

    bool hasAnotherStraightAboveOrBelow(bool above) const;

    std::vector<LineAttachPoint> &lineAttachPoints() { return lineAttachPoints_; }

    const std::vector<LineAttachPoint> &lineAttachPoints() const { return lineAttachPoints_; }

    void addStartLineAttachPoint(PointF point, EngravingItem *line) { addLineAttachPoint(point, line, true); }

    void addEndLineAttachPoint(PointF point, EngravingItem *line) { addLineAttachPoint(point, line, false); }

    PointF posInStaffCoordinates();

    bool isTrillCueNote() const { return isTrillCueNote_; }

    void setIsTrillCueNote(bool v);

    SymId noteHead() const;
    bool isNoteName() const;

    void updateFrettingForTiesAndBends();
    bool shouldHideFret() const;
    bool shouldForceShowFret() const;

    void setVisible(bool v) override;

    bool overrideBendVisibilityRules() const { return overrideBendVisibilityRules_; }

    void setOverrideBendVisibilityRules(bool v) { overrideBendVisibilityRules_ = v; }

    bool hideGeneratedParens() const { return hideGeneratedParens_; }

    void setHideGeneratedParens(bool v) { hideGeneratedParens_ = v; }

    TieJumpPointList *tieJumpPoints() { return &jumpPoints_; }

    const TieJumpPointList *tieJumpPoints() const { return &jumpPoints_; }

    bool isExactUnison(Note *other);

    struct LayoutData : public EngravingItem::LayoutData {
        ld_field<bool> useTablature = {"[Note] useTablature", false};
        ld_field<SymId> cachedNoteheadSym = {"[Note] cachedNoteheadSym", SymId::noSym}; // use in draw to avoid recomputing at every update
        ld_field<SymId> cachedSymNull = {"[Note] cachedSymNull", SymId::noSym};         // additional symbol for some transparent notehead
        ld_field<bool> mirror = {"[Note] mirror", false};                               // True if note is mirrored at stem.
        ld_field<bool> hasGeneratedParens = {"[Note] hasGeneratedParens", false};       // Should generated parens be created
    };
    DECLARE_LAYOUTDATA_METHODS(Note)

private:

    friend class Factory;
    Note(DummyParentOr<Chord> ch);
    Note(const Note &, bool link = false);

    void addSpanner(Spanner *);
    void removeSpanner(Spanner *);
    int concertPitchIdx() const;
    void updateRelLine(int absLine, bool undoable);

    static std::vector<Note *> findTiedNotes(Note *startNote, bool followPartialTies = true);

    void getNoteListForDots(std::vector<Note *> &topDownNotes, std::vector<Note *> &bottomUpNotes, std::vector<int> &anchoredDots);

    void addLineAttachPoint(PointF point, EngravingItem *line, bool start);

    bool ghost_ = false;    // ghost note
    bool deadNote_ = false; // dead note

    bool isTrillCueNote_ = false;

    bool hidden_ = false;       // marks this note as the hidden one if there are
                                // overlapping notes; hidden notes are not played
                                // and heads + accidentals are not shown
    bool dotsHidden_ = false;   // dots of hidden notes are hidden too
                                // except if only one note is dotted
    bool fretConflict_ = false; // used by TAB staves to mark a fretting conflict:
                                // two or more notes on the same string
    bool isSmall_ = false;
    bool play_ = true;          // note is not played if false
    mutable bool mark_ = false; // for use in sequencer
    bool fixed_ = false;        // for slash notation

    bool overrideBendVisibilityRules_ = false;

    bool hideGeneratedParens_ = false;

    SlideType slideToType_ = SlideType::Undefined;
    SlideType slideFromType_ = SlideType::Undefined;

    DirectionH userMirror_ = DirectionH::AUTO;      ///< user override of mirror
    DirectionV userDotPosition_ = DirectionV::AUTO; ///< user override of dot position
    DirectionV dotPosition_ = DirectionV::AUTO;     // used as an intermediate step when resolving dot conflicts

    NoteHeadScheme headScheme_ = NoteHeadScheme::HEAD_AUTO;
    NoteHeadGroup headGroup_ = NoteHeadGroup::HEAD_NORMAL;
    NoteHeadType headType_ = NoteHeadType::HEAD_AUTO;

    VeloType veloType_ = VeloType::USER_VAL;

    int offTimeType_ = 0; // compatibility only 1 - user(absolute), 2 - offset (%)
    int onTimeType_ = 0;  // compatibility only 1 - user, 2 - offset

    int subchannel_ = 0;      // articulation
    int line_ = INVALID_LINE; // y-Position; 0 - top line.
    int fret_ = -1;           // for tablature view
    float harmonicFret_ = -1.0;
    int harmonicPitchOffset_ = 0;
    DisplayFretOption displayFret_ = DisplayFretOption::NoHarmonic;
    int string_ = -1;
    mutable int tpc_[2] = {Tpc::TPC_INVALID, Tpc::TPC_INVALID}; // tonal pitch class  (concert/transposing)
    mutable int pitch_ = 0;                                     // Note pitch as midi value (0 - 127).
    mutable double centOffset_ = 0.0;                           // Pitch offset in cents (100 cents = 1 semitone)

    int userVelocity_ = 0; // velocity user offset in percent, or absolute velocity for this note
    int fixedLine_ = 0;    // fixed line number if _fixed == true
    double tuning_ = 0.0;  // pitch offset in cent, playable only by internal synthesizer

    Accidental *accidental_ = nullptr;

    Tie *tieFor_ = nullptr;
    Tie *tieBack_ = nullptr;

    bool harmonic_ = false;

    bool hasParens_ = false;

    ElementList el_; // fingering, other text, symbols or images
    std::vector<NoteDot *> dots_;
    NoteEventList playEvents_;
    std::vector<Spanner *> spannerFor_;
    std::vector<Spanner *> spannerBack_;

    String fretString_;

    std::vector<LineAttachPoint> lineAttachPoints_;
    TieJumpPointList jumpPoints_{this};
};
} // namespace mu::engraving
