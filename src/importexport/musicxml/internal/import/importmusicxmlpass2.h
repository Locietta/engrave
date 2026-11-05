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

#include <array>

#include "importmusicxmlpass1.h"
#include "../shared/musicxmlsupport.h"
#include "../shared/musicxmltypes.h"
#include "musicxmltupletstate.h"

namespace mu::engraving {
class Accidental;
class Beam;
class ChordRest;
class EngravingItem;
class FiguredBass;
class FiguredBassItem;
class Fraction;
class FretDiagram;
class Glissando;
class GradualTempoChange;
class Hairpin;
class Harmony;
class Instrument;
class Jump;
class Lyrics;
class Marker;
class Measure;
class Note;
class Pedal;
class Score;
class SLine;
class Slur;
class Spanner;
class Staff;
class Sticking;
class StringData;
class Text;
class Tie;
class Trill;
class Tuplet;
class Volta;
enum class TupletBracketType : unsigned char;
enum class TupletNumberType : unsigned char;
} // namespace mu::engraving

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   support enums / structs / classes
//---------------------------------------------------------

// One nonempty container, or one existing word-inferred marking. Text contains
// escaped literals and symbol-table-generated markup only; no private metadata.
struct MusicXmlDynamic {
    muse::String text;
    // Escaped literal text with markup-looking content must not be parsed as
    // dynamic symbols. Ranges exist only while importing, not in the score.
    std::vector<std::pair<size_t, size_t>> protectedLiteralRanges;
    engraving::Color color;
    muse::String placement;
    muse::String enclosure;
    bool hasTotalY = false;
    double totalY = 0.0;
};

using GraceChordList = std::vector<engraving::Chord *>;
using FiguredBassList = std::vector<engraving::FiguredBass *>;
using Tuplets = std::map<muse::String, engraving::Tuplet *>;
using Beams = std::map<muse::String, engraving::Beam *>;

//---------------------------------------------------------
//   MusicXmlSlash
//---------------------------------------------------------

enum class MusicXmlSlash : unsigned char { NONE, RHYTHM, SLASH };

//---------------------------------------------------------
//   MusicXmlTupletDesc
//---------------------------------------------------------

/**
 Describe the information extracted from
 a single note/notations/tuplet element.
 */

struct MusicXmlTupletDesc {
    MusicXmlTupletDesc();
    MusicXmlStartStop type;
    engraving::DirectionV direction;
    engraving::TupletBracketType bracket;
    engraving::TupletNumberType shownumber;
};

//---------------------------------------------------------
//   SlurDesc
//---------------------------------------------------------

/**
 The description of Slurs being handled
 */

class SlurDesc {
public:
    enum class State : unsigned char { NONE, START, STOP };

    SlurDesc() : slur_(0), state_(State::NONE) {}

    engraving::Slur *slur() const { return slur_; }

    void start(engraving::Slur *slur) {
        slur_ = slur;
        state_ = State::START;
    }

    void stop(engraving::Slur *slur) {
        slur_ = slur;
        state_ = State::STOP;
    }

    bool isStart() const { return state_ == State::START; }

    bool isStop() const { return state_ == State::STOP; }
private:
    engraving::Slur *slur_ = nullptr;
    State state_;
};

typedef std::map<engraving::SLine *, std::pair<int, int>> MusicXmlSpannerMap;

//---------------------------------------------------------
//   MusicXmlSpannerDesc
//---------------------------------------------------------

struct MusicXmlSpannerDesc {
    engraving::SLine *sp = nullptr;
    engraving::ElementType tp = engraving::ElementType::INVALID;
    int nr = 0;

    MusicXmlSpannerDesc(engraving::SLine *sp, engraving::ElementType tp, int nr) : sp(sp), tp(tp), nr(nr) {}

    MusicXmlSpannerDesc(engraving::ElementType tp, int nr) : sp(0), tp(tp), nr(nr) {}
};

//---------------------------------------------------------
//   MusicXmlExtendedSpannerDesc
//---------------------------------------------------------

struct MusicXmlExtendedSpannerDesc {
    engraving::SLine *sp = nullptr;
    engraving::Fraction tick2{0, 0};
    engraving::track_idx_t track2 = 0;
    bool isStarted = false;
    bool isStopped = false;

    MusicXmlExtendedSpannerDesc() {}

    muse::String toString() const;
};

//---------------------------------------------------------
//   HarmonyDesc
//---------------------------------------------------------

/**
 The description of a chord symbol with or without a fret diagram
 */

struct HarmonyDesc {
    engraving::track_idx_t track_;
    bool fretDiagramVisible() const;
    engraving::Harmony *harmony_;
    engraving::FretDiagram *fretDiagram_;

    const engraving::ChordDescription *descr() const;

    HarmonyDesc(engraving::track_idx_t track_, engraving::Harmony *harmony_, engraving::FretDiagram *fretDiagram_)
        : track_(track_), harmony_(harmony_), fretDiagram_(fretDiagram_) {}

    HarmonyDesc() : track_(muse::nidx), harmony_(nullptr), fretDiagram_(nullptr) {}
};

using HarmonyMap = std::multimap<int, HarmonyDesc>;

//---------------------------------------------------------
//   MusicXmlLyricsExtend
//---------------------------------------------------------

class MusicXmlLyricsExtend {
public:
    MusicXmlLyricsExtend() {}

    void init();
    void addLyric(engraving::Lyrics *const lyric);
    void setExtend(const int verse, const engraving::track_idx_t track, const engraving::Fraction &tick,
                   const engraving::Lyrics *prevAddedLyrics);

private:
    std::set<engraving::Lyrics *> lyrics_;
};

struct GraceNoteLyrics {
    engraving::Lyrics *lyric = nullptr;
    bool extend = false;
    int no = 0;

    GraceNoteLyrics(engraving::Lyrics *lyric, bool extend, int no) : lyric(lyric), extend(extend), no(no) {}
};

struct InferredPercInstr {
    int pitch;
    engraving::track_idx_t track;
    engraving::String name;
    engraving::Fraction tick;

    InferredPercInstr(int pitch, engraving::track_idx_t track, engraving::String name, engraving::Fraction tick)
        : pitch(pitch), track(track), name(name), tick(tick) {}

    InferredPercInstr() : pitch(-1), track(muse::nidx), name(u""), tick(engraving::Fraction(0, -1)) {}
};

typedef std::vector<InferredPercInstr> InferredPercList;

typedef std::map<engraving::String, std::pair<engraving::String, engraving::DurationType>> MetronomeTextMap;

//---------------------------------------------------------
//   MusicXmlParserLyric
//---------------------------------------------------------

class MusicXmlParserLyric {
public:
    MusicXmlParserLyric(const LyricNumberHandler lyricNumberHandler, muse::XmlStreamReader &e, engraving::Score *score,
                        const IMusicXmlConfiguration &configuration, MusicXmlLogger *logger, bool isVoiceStaff);

    std::set<engraving::Lyrics *> extendedLyrics() const { return extendedLyrics_; }

    std::map<int, engraving::Lyrics *> numberedLyrics() const { return numberedLyrics_; }

    std::vector<engraving::Sticking *> inferredStickings() const { return inferredStickings_; }

    void parse(bool visibility = true);
private:
    void skipLogCurrElem();
    void readElision(muse::String &formattedText);
    const LyricNumberHandler lyricNumberHandler_;
    muse::XmlStreamReader &e_;
    const engraving::Score *score_ = nullptr; // the score
    const IMusicXmlConfiguration &configuration_;
    MusicXmlLogger *logger_ = nullptr;                     // Error logger
    std::map<int, engraving::Lyrics *> numberedLyrics_;    // lyrics with valid number
    std::set<engraving::Lyrics *> extendedLyrics_;         // lyrics with the extend flag set
    std::vector<engraving::Sticking *> inferredStickings_; // stickings with valid number
    double defaultY_ = 0.0;
    double relativeY_ = 0.0;
    muse::String placement_;
    muse::String placement() const;

    double totalY() const { return defaultY_ + relativeY_; }

    bool hasTotalY() const { return !muse::RealIsNull(defaultY_) || !muse::RealIsNull(relativeY_); }

    bool isLikelySticking(const muse::String &text, const engraving::LyricsSyllabic syllabic, const bool hasExtend);
    bool isVoiceStaff_ = true;
};

//---------------------------------------------------------
//   Notation
//      Most with text base attributes (font-*, placement)
//---------------------------------------------------------

class Notation {
public:
    Notation(const muse::String &name, const muse::String &parent = {}, const engraving::SymId &symId = engraving::SymId::noSym) {
        name_ = name;
        parent_ = parent;
        symId_ = symId;
    }

    void addAttribute(const muse::String &name, const muse::String &value);
    muse::String attribute(const muse::String &name) const;

    const std::map<muse::String, muse::String> &attributes() const { return attributes_; }

    muse::String name() const { return name_; }

    muse::String parent() const { return parent_; }

    void setSymId(const engraving::SymId &symId) { symId_ = symId; }

    engraving::SymId symId() const { return symId_; }

    void setSubType(const muse::String &subType) { subType_ = subType; }

    muse::String subType() const { return subType_; }

    muse::String print() const;

    void setText(const muse::String &text) { text_ = text; }

    muse::String text() const { return text_; }

    void setVisible(const bool visible) { visible_ = visible; }

    bool visible() const { return visible_; }

    static Notation notationWithAttributes(const muse::String &name, const std::vector<muse::XmlStreamReader::Attribute> &attributes,
                                           const muse::String &parent = {}, const engraving::SymId &symId = engraving::SymId::noSym);
private:
    muse::String name_;
    muse::String parent_;
    engraving::SymId symId_ = engraving::SymId::noSym;
    muse::String subType_;
    muse::String text_;
    std::map<muse::String, muse::String> attributes_;
    bool visible_ = true;
};

//---------------------------------------------------------
//   forward references and defines
//---------------------------------------------------------

struct DelayedArpeggio {
    muse::String arpeggioType_ = u"";
    int arpeggioNo_ = 0;

    DelayedArpeggio(muse::String arpType, int no) : arpeggioType_(arpType), arpeggioNo_(no) {}

    DelayedArpeggio() : arpeggioType_(muse::String(u"")), arpeggioNo_(0) {}

    void clear() {
        arpeggioType_ = u"";
        arpeggioNo_ = 0;
    }
};

class MusicXmlLogger;
class MusicXmlDelayedDirectionElement;
class MusicXmlInferredFingering;
class MusicXmlParserPass2;

using DelayedDirectionsList = std::vector<MusicXmlDelayedDirectionElement *>;
using InferredFingeringsList = std::vector<MusicXmlInferredFingering *>;
using SlurStack = std::array<SlurDesc, MAX_NUMBER_LEVEL>;
using TrillStack = std::array<engraving::Trill *, MAX_NUMBER_LEVEL>;
using BracketsStack = std::array<MusicXmlExtendedSpannerDesc, MAX_NUMBER_LEVEL>;
using OttavasStack = std::array<MusicXmlExtendedSpannerDesc, MAX_NUMBER_LEVEL>;
using HairpinsStack = std::array<MusicXmlExtendedSpannerDesc, MAX_NUMBER_LEVEL>;
using InferredHairpinsStack = std::vector<engraving::Hairpin *>;
using InferredTempoLineStack = std::vector<engraving::GradualTempoChange *>;
using SpannerStack = std::array<MusicXmlExtendedSpannerDesc, MAX_NUMBER_LEVEL>;
using SpannerSet = std::set<engraving::Spanner *>;
using DelayedArpMap = std::map<int, DelayedArpeggio>;
using SegnoStack = std::map<int, engraving::Marker *>;
using SystemElements = std::multimap<int, engraving::EngravingItem *>;

// Ties are identified by the pitch and track of their first note
typedef std::pair<int, engraving::track_idx_t> TieLocation;
using MusicXmlTieMap = std::map<TieLocation, engraving::Tie *>;

//---------------------------------------------------------
//   MusicXmlParserNotations
//---------------------------------------------------------

class MusicXmlParserNotations {
public:
    MusicXmlParserNotations(muse::XmlStreamReader &e, engraving::Score *score, MusicXmlLogger *logger, MusicXmlParserPass2 &pass2);
    void parse();
    void addToScore(engraving::ChordRest *const cr, engraving::Note *const note, const engraving::Fraction &tick, SlurStack &slurs,
                    engraving::Glissando *glissandi[MAX_NUMBER_LEVEL][2], MusicXmlSpannerMap &spanners, TrillStack &trills,
                    MusicXmlTieMap &ties, std::vector<engraving::Note *> &unstartedTieNotes,
                    std::vector<engraving::Note *> &unendedTieNotes, ArpeggioMap &arpMap, DelayedArpMap &delayedArps);

    muse::String errors() const { return errors_; }

    MusicXmlTupletDesc tupletDesc() const { return tupletDesc_; }

    bool hasTremolo() const { return hasTremolo_; }

    muse::String tremoloType() const { return tremoloType_; }

    muse::String tremoloSmufl() const { return tremoloSmufl_; }

    engraving::Color tremoloColor() const { return tremoloColor_; }

    int tremoloNr() const { return tremoloNr_; }

    bool mustStopGraceAfter() const { return (slurStop_ && !slurStart_) || wavyLineStop_; }
private:
    void addError(const muse::String &error); // Add an error to be shown in the GUI
    void addNotation(const Notation &notation, engraving::ChordRest *const cr, engraving::Note *const note);
    void addTechnical(const Notation &notation, engraving::Note *note);
    void arpeggio();
    void harmonic();
    void harmonMute();
    void hole();
    void articulations();
    void dynamics();
    void fermata();
    void glissandoSlide();
    void mordentNormalOrInverted();
    void ornaments();
    void slur();
    void skipLogCurrElem();
    void technical();
    void otherTechnical();
    void tied();
    void tuplet();
    void otherNotation();
    muse::XmlStreamReader &e_;
    MusicXmlParserPass2 &pass2_;
    engraving::Score *score_ = nullptr; // the score
    MusicXmlLogger *logger_ = nullptr;  // the error logger
    muse::String errors_;               // errors to present to the user
    MusicXmlTupletDesc tupletDesc_;
    std::vector<MusicXmlDynamic> dynamicsList_;
    std::vector<Notation> notations_;
    bool hasTremolo_ = false;
    muse::String tremoloType_;
    int tremoloNr_ = 0;
    muse::String tremoloSmufl_;
    engraving::Color tremoloColor_;
    muse::String wavyLineType_;
    int wavyLineNo_ = 0;
    muse::String arpeggioType_;
    int arpeggioNo_ = 0;
    engraving::Color arpeggioColor_;
    bool slurStop_ = false;
    bool slurStart_ = false;
    bool wavyLineStop_ = false;
    bool visible_ = true;
};

//---------------------------------------------------------
//   MusicXmlParserPass2
//---------------------------------------------------------

class MusicXmlParserPass2 {
public:
    MusicXmlParserPass2(engraving::Score *score, MusicXmlParserPass1 &pass1, MusicXmlLogger *logger);

    const IMusicXmlConfiguration *configuration() const { return pass1_.configuration(); }

    engraving::Err parse(const muse::ByteArray &data);

    muse::String errors() const { return errors_; }

    // part specific data interface functions
    void addSpanner(const MusicXmlSpannerDesc &desc);
    MusicXmlExtendedSpannerDesc &getSpanner(const MusicXmlSpannerDesc &desc);
    void clearSpanner(const MusicXmlSpannerDesc &desc);
    void deleteHandledSpanner(engraving::SLine *const &spanner);

    int divs() { return divs_; }

    engraving::SLine *delayedOttava() { return delayedOttava_; }

    void setDelayedOttava(engraving::SLine *ottava) { delayedOttava_ = ottava; }

    void addInferredHairpin(engraving::Hairpin *hp) { inferredHairpins_.push_back(hp); }

    const InferredHairpinsStack &getInferredHairpins() { return inferredHairpins_; }

    void addInferredTempoLine(engraving::GradualTempoChange *hp) { inferredTempoLines_.push_back(hp); }

    const InferredTempoLineStack &getInferredTempoLine() { return inferredTempoLines_; }

    void addSystemElement(engraving::EngravingItem *el, const engraving::Fraction &tick) { sysElements_.insert({tick.ticks(), el}); }

    const SystemElements systemElements() const { return sysElements_; }

    InferredPercInstr inferredPercInstr(const engraving::Fraction &tick, const engraving::track_idx_t trackIdx);

    void addInferredPercInstr(InferredPercInstr instr) { inferredPerc_.push_back(instr); }

    void addElemOffset(engraving::EngravingItem *el, engraving::track_idx_t track, const muse::String &placement,
                       engraving::Measure *measure, const engraving::Fraction &tick);

private:
    void addError(const muse::String &error); // Add an error to be shown in the GUI
    void initPartState(const muse::String &partId);
    SpannerSet findIncompleteSpannersAtPartEnd();
    engraving::Err parse();
    void scorePartwise();
    void partList();
    void scorePart();
    void part();
    void measure(const muse::String &partId, const engraving::Fraction time);
    void measureLayout(engraving::Measure *measure);
    void setMeasureRepeats(const engraving::staff_idx_t scoreRelStaff, engraving::Measure *measure);
    void attributes(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction &tick);
    void measureStyle(engraving::Measure *measure);
    void barline(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction &tick);
    void key(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction &tick);
    void clef(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction &tick);
    void time(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction &tick);
    void divisions();
    engraving::Note *note(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction sTime,
                          const engraving::Fraction prevTime, engraving::Fraction &missingPrev, engraving::Fraction &dura,
                          engraving::Fraction &missingCurr, muse::String &currentVoice, GraceChordList &gcl, size_t &gac, Beams &currBeams,
                          FiguredBassList &fbl, int &alt, MusicXmlTupletStates &tupletStates, Tuplets &tuplets, ArpeggioMap &arpMap,
                          DelayedArpMap &delayedArps);
    void notePrintSpacingNo(engraving::Fraction &dura);
    engraving::FiguredBassItem *figure(const int idx, const bool paren, engraving::FiguredBass *parent);
    engraving::FiguredBass *figuredBass();
    engraving::FretDiagram *frame();
    void harmony(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction &sTime, HarmonyMap &harmonyMap);
    engraving::Accidental *accidental();
    void beam(std::map<int, muse::String> &beamTypes);
    void duration(engraving::Fraction &dura);
    void forward(engraving::Fraction &dura);
    void backup(engraving::Fraction &dura);
    void timeModification(engraving::Fraction &timeMod, engraving::TDuration &normalType);
    void stem(engraving::DirectionV &sd, bool &nost);
    void doEnding(const muse::String &partId, engraving::Measure *measure, const muse::String &number, const muse::String &type,
                  const engraving::Color color, const muse::String &text, const bool print);
    void staffDetails(const muse::String &partId, engraving::Measure *measure = nullptr);
    void staffTuning(engraving::StringData *t);
    void skipLogCurrElem();

    // multi-measure rest state handling
    void setMultiMeasureRestCount(int count);
    int getAndDecMultiMeasureRestCount();

    void xmlSetDrumsetPitch(engraving::Note *note, const engraving::Chord *chord, const engraving::Staff *staff, int step, int octave,
                            engraving::NoteHeadGroup headGroup, engraving::DirectionV &stemDir, engraving::Instrument *instrument);

    // generic pass 2 data

    muse::XmlStreamReader e_;
    int divs_ = 0;                      // the current divisions value
    engraving::Score *score_ = nullptr; // the score
    MusicXmlParserPass1 &pass1_;        // the pass1 results
    MusicXmlLogger *logger_ = nullptr;  // Error logger
    muse::String errors_;               // Errors to present to the user

    // part specific data (TODO: move to part-specific class)

    // Measure duration according to last timesig read
    // TODO: store timesigs read in pass 1, use those instead
    // or use score->sigmap() ?
    engraving::Fraction timeSigDura_;

    SlurStack slurs_{{}};
    TrillStack trills_{{}}; // Current trills
    BracketsStack brackets_;
    OttavasStack ottavas_;   // Current ottavas
    HairpinsStack hairpins_; // Current hairpins
    InferredHairpinsStack inferredHairpins_;
    InferredTempoLineStack inferredTempoLines_;
    MusicXmlExtendedSpannerDesc dummyNewMusicXmlSpannerDesc_;

    engraving::Glissando *glissandi_[MAX_NUMBER_LEVEL][2]{{}}; // Current slides ([0]) / glissandi ([1])

    MusicXmlTieMap ties_;
    std::vector<engraving::Note *> unstartedTieNotes_;
    std::vector<engraving::Note *> unendedTieNotes_;
    engraving::Volta *lastVolta_ = nullptr;
    bool hasDrumset_ = false; // drumset defined TODO: move to pass 1

    MusicXmlSpannerMap spanners_;

    MusicXmlExtendedSpannerDesc pedal_;         // Current pedal
    engraving::Pedal *pedalContinue_ = nullptr; // Current pedal type="change" requiring fixup
    engraving::Harmony *harmony_ = nullptr;     // Current harmony
    engraving::Chord *tremStart_ = nullptr;     // Starting chord for current tremolo
    engraving::FiguredBass *figBass_ = nullptr; // Current figured bass element (to attach to next note)
    engraving::SLine *delayedOttava_ = nullptr; // Current delayed ottava
    SegnoStack segnos_;                         // List of segno markings
    int multiMeasureRestCount_ = 0;
    int measureNumber_ = 0;                        // Current measure number as written in the score
    MusicXmlLyricsExtend extendedLyrics_;          // Lyrics with "extend" requiring fixup
    std::vector<GraceNoteLyrics> graceNoteLyrics_; // Lyrics to be moved from grace note to main note

    MusicXmlSlash measureStyleSlash_ = MusicXmlSlash::NONE; // Are we inside a measure to be displayed as slashes?

    size_t nstaves_ = 0; // Number of staves in current part
    std::vector<int> measureRepeatNumMeasures_;
    std::vector<int> measureRepeatCount_;

    SystemElements sysElements_;
    InferredPercList inferredPerc_;
};

//---------------------------------------------------------
//   MusicXmlParserDirection
//---------------------------------------------------------

class MusicXmlParserDirection {
public:
    MusicXmlParserDirection(muse::XmlStreamReader &e, engraving::Score *score, MusicXmlParserPass1 &pass1, MusicXmlParserPass2 &pass2,
                            MusicXmlLogger *logger);

    const IMusicXmlConfiguration *configuration() const { return pass1_.configuration(); }

    void direction(const muse::String &partId, engraving::Measure *measure, const engraving::Fraction &tick, MusicXmlSpannerMap &spanners,
                   DelayedDirectionsList &delayedDirections, InferredFingeringsList &inferredFingerings, HarmonyMap &harmonyMap,
                   bool &measureHasCoda, SegnoStack &segnos);

    double totalY() const { return defaultY_ + relativeY_; }

    muse::String placement() const;

    void setBpm(const double bpm) { tpoSound_ = bpm; }

private:
    void directionType(std::vector<MusicXmlSpannerDesc> &starts, std::vector<MusicXmlSpannerDesc> &stops);
    void bracket(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                 std::vector<MusicXmlSpannerDesc> &stops);
    void octaveShift(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                     std::vector<MusicXmlSpannerDesc> &stops);
    void pedal(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
               std::vector<MusicXmlSpannerDesc> &stops);
    void dashes(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                std::vector<MusicXmlSpannerDesc> &stops);
    void wedge(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
               std::vector<MusicXmlSpannerDesc> &stops);
    muse::String metronome(double &r);
    void sound();
    void play();
    void swing();
    void dynamics();
    void harpPedal();
    void otherDirection();
    void handleRepeats(engraving::Measure *measure, const engraving::Fraction tick, bool &measureHasCoda, SegnoStack &segnos,
                       DelayedDirectionsList &delayedDirections);
    engraving::Marker *findMarker(const muse::String &repeat) const;
    engraving::Jump *findJump(const muse::String &repeat) const;
    void handleNmiCmi(engraving::Measure *measure, const engraving::Fraction &tick, DelayedDirectionsList &delayedDirections);
    void handleChordSym(const engraving::Fraction &tick, HarmonyMap &harmonyMap);
    void handleTempo(muse::String &wordsString);
    muse::String matchRepeat(const muse::String &plainWords) const;
    void skipLogCurrElem();
    bool isLikelyCredit(const engraving::Fraction &tick) const;
    void textToDynamic(muse::String &text);
    void textToCrescLine(muse::String &text);
    void addInferredHairpin(const engraving::Fraction &tick, const bool isVocalStaff);
    void addInferredTempoLine(const engraving::Fraction &tick);
    bool isLyricBracket() const;
    bool isLikelySubtitle(const engraving::Fraction &tick) const;
    bool isLikelyLegallyDownloaded(const engraving::Fraction &tick) const;
    bool isLikelyTempoText(const engraving::track_idx_t track) const;
    void handleFraction();
    bool isLikelyTempoLine(const engraving::track_idx_t track) const;
    engraving::Text *addTextToHeader(const engraving::TextStyleType textStyleType);
    void hideRedundantHeaderText(const engraving::Text *inferredText, const std::vector<muse::String> metaTags);
    bool isLikelyFingering(const muse::String &fingeringStr) const;
    bool isLikelySticking();
    bool isLikelyDynamicRange() const;
    engraving::PlayingTechniqueType getPlayingTechnique() const;
    void handleDrumInstrument(bool isPerc, engraving::Fraction tick) const;

    // void terminateInferredLine(const std::vector<TextLineBase*> lines, const Fraction& tick);

    bool hasTotalY() const { return hasRelativeY_ || hasDefaultY_; }

    muse::XmlStreamReader &e_;
    engraving::Score *score_ = nullptr; // the score
    MusicXmlParserPass1 &pass1_;        // the pass1 results
    MusicXmlParserPass2 &pass2_;        // the pass2 results
    MusicXmlLogger *logger_ = nullptr;  // Error logger

    engraving::Color color_;
    engraving::Hairpin *inferredHairpinStart_ = nullptr;
    engraving::GradualTempoChange *inferredTempoLineStart_ = nullptr;
    std::vector<MusicXmlDynamic> dynamicsList_;
    muse::String fontFamily_;
    muse::String enclosure_;
    muse::String wordsText_;
    muse::String metroText_;
    muse::String rehearsalText_;
    muse::String justify_;
    muse::String dynaVelocity_;
    muse::String sndCoda_;
    muse::String sndDacapo_;
    muse::String sndDalsegno_;
    muse::String sndFine_;
    muse::String sndSegno_;
    muse::String sndToCoda_;
    muse::String codaId_;
    muse::String segnoId_;
    muse::String placement_;
    muse::String play_;
    bool hasDefaultY_ = false;
    double defaultY_ = 0.0;
    bool hasRelativeY_ = false;
    double relativeY_ = 0.0;
    double relativeX_ = 0.0;
    double tpoMetro_ = 0.0; // tempo according to metronome
    double tpoSound_ = 0.0; // tempo according to sound
    bool visible_ = true;
    bool systemDirection_ = false;
    std::pair<int, int> swing_ = {0, 0};
    std::vector<engraving::EngravingItem *> elems_;
    engraving::Fraction offset_;
    engraving::track_idx_t track_ = muse::nidx;
};

//---------------------------------------------------------
//   MusicXmlDelayedDirectionElement
//---------------------------------------------------------
/**
 Helper class to allow Direction elements to be sorted by _totalY
 before being added to the score.
 */

class MusicXmlDelayedDirectionElement {
public:
    MusicXmlDelayedDirectionElement(double totalY, engraving::EngravingItem *element, engraving::track_idx_t track, muse::String placement,
                                    engraving::Measure *measure, engraving::Fraction tick)
        : totalY_(totalY), element_(element), track_(track), placement_(placement), measure_(measure), tick_(tick) {}

    double totalY() const { return totalY_; }

    engraving::EngravingItem *element() const { return element_; }

    engraving::track_idx_t track() const { return track_; }

    const engraving::Fraction &tick() const { return tick_; }

    const muse::String &placement() const { return placement_; }

    engraving::Measure *measure() const { return measure_; }

private:
    double totalY_ = 0.0;
    engraving::EngravingItem *element_ = nullptr;
    engraving::track_idx_t track_ = muse::nidx;
    muse::String placement_;
    engraving::Measure *measure_ = nullptr;
    engraving::Fraction tick_;
};

//---------------------------------------------------------
//   MusicXmlInferredFingering
//---------------------------------------------------------
/**
 Helper class to allow Direction elements to be reinterpreted as fingerings
 */

class MusicXmlInferredFingering {
public:
    MusicXmlInferredFingering(double totalY, engraving::EngravingItem *element, muse::String &text, engraving::track_idx_t track,
                              muse::String placement, engraving::Measure *measure, engraving::Fraction tick, bool needUseDefaultFont);

    double totalY() const { return totalY_; }

    engraving::Fraction tick() const { return tick_; }

    engraving::track_idx_t track() const { return track_; }

    std::vector<muse::String> fingerings() const { return fingerings_; }

    bool findAndAddToNotes(engraving::Measure *measure);
    MusicXmlDelayedDirectionElement *toDelayedDirection();

private:
    double totalY_;
    engraving::EngravingItem *element_;
    muse::String text_;
    std::vector<muse::String> fingerings_;
    engraving::track_idx_t track_;
    muse::String placement_;
    engraving::Measure *measure_;
    engraving::Fraction tick_;
    bool needUseDefaultFont_ = false;

    void roundTick(engraving::Measure *measure);
    void addToNotes(std::vector<engraving::Note *> &notes) const;
};
} // namespace mu::iex::musicxml
