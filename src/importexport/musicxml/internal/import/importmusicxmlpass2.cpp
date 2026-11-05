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

#include <algorithm>
#include <cmath>
#include <memory>
#include <charconv>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "containers.h"

#include "engraving/types/symnames.h"
#include "engraving/types/typesconv.h"
#include "engraving/iengravingfont.h"

#include "engraving/dom/accidental.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/box.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/chordlist.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/figuredbass.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/gradualtempochange.h"
#include "engraving/dom/hammeronpulloff.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrchange.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/interval.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/line.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pedal.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tapping.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/trill.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/utils.h"
#include "engraving/dom/volta.h"
#include "engraving/editing/editmeasurerepeat.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/editing/transpose.h"
#include "engraving/engravingerrors.h"
#include "engraving/rendering/iscorerenderer.h"

#include "importmusicxmllogger.h"
#include "importmusicxmlnoteduration.h"
#include "importmusicxmlnotepitch.h"
#include "importmusicxmlpass1.h"
#include "importmusicxmlpass2.h"
#include "../shared/musicxmlfonthandler.h"
#include "../shared/musicxmlsupport.h"
#include "../shared/musicxmltypes.h"

#include "importexport/musicxml/imusicxmlconfiguration.h"
#include "engraving/iengravingfontsprovider.h"
#include "engraving/rendering/score/tlayout.h"

#include "log.h"

using namespace muse;
using namespace mu;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;
using namespace mu::iex::musicxml;

namespace mu::iex::musicxml {

//---------------------------------------------------------
//   local defines for debug output
//---------------------------------------------------------

//#define DEBUG_VOICE_MAPPER true

constexpr int MAX_LYRICS = 16;

//---------------------------------------------------------
//   function declarations
//---------------------------------------------------------

static void addTie(const Notation &notation, Note *note, const track_idx_t track, MusicXmlTieMap &tie,
                   std::vector<Note *> &unstartedTieNotes, std::vector<Note *> &unendedTieNotes, MusicXmlLogger *logger,
                   const XmlStreamReader *const xmlreader, bool importLayout);

//---------------------------------------------------------
//   support enums / structs / classes
//---------------------------------------------------------

//---------------------------------------------------------
//   MusicXmlTupletDesc
//---------------------------------------------------------

MusicXmlTupletDesc::MusicXmlTupletDesc()
    : type(MusicXmlStartStop::NONE), direction(DirectionV::AUTO), bracket(TupletBracketType::AUTO_BRACKET),
      shownumber(TupletNumberType::SHOW_NUMBER) {
    // nothing
}

//---------------------------------------------------------
//   MusicXmlLyricsExtend
//---------------------------------------------------------

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void MusicXmlLyricsExtend::init() { lyrics_.clear(); }

//---------------------------------------------------------
//   addLyric
//---------------------------------------------------------

// add a single lyric to be extended later
// called when lyric with "extend" or "extend type=start" is found

void MusicXmlLyricsExtend::addLyric(Lyrics *const lyric) { lyrics_.insert(lyric); }

//---------------------------------------------------------
//   lastChordTicks
//---------------------------------------------------------

// find the duration of the chord starting at or after s and ending at tick

static Fraction lastChordTicks(const Segment *s, const Fraction &tick, const track_idx_t track) {
    while (s && s->tick() < tick) {
        for (EngravingItem *el : s->elist()) {
            if (el && el->isChordRest() && el->track() == track) {
                ChordRest *cr = toChordRest(el);
                if (cr->tick() + cr->actualTicks() == tick) {
                    return cr->actualTicks();
                }
            }
        }
        s = s->nextCR(muse::nidx, true);
    }
    return Fraction(0, 1);
}

static bool spannerExists(const std::vector<MusicXmlSpannerDesc> &spanners, int number, engraving::ElementType elementType) {
    for (const MusicXmlSpannerDesc &desc : spanners) {
        if (desc.nr == number && desc.tp == elementType) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   setExtend
//---------------------------------------------------------

// set extend for lyric no in *staff* to end at tick
// called when lyric (with or without "extend") or note with "extend type=stop" is found
// // note that no == -1 means all lyrics in this *track*

void MusicXmlLyricsExtend::setExtend(const int verse, const track_idx_t track, const Fraction &tick,
                                     const Lyrics *prevAddedLyrics = nullptr) {
    std::vector<Lyrics *> list;
    for (Lyrics *l : lyrics_) {
        const ChordRest *par = l->chordRest();
        if (par) {
            // no = -1: stop all extends on this track
            // otherwise, stop all extends in the stave with the same no and placement
            if ((verse == -1 && par->track() == track) || (l->verse() == verse && track2staff(par->track()) == track2staff(track) &&
                                                           prevAddedLyrics && prevAddedLyrics->placement() == l->placement())) {
                Fraction lct = lastChordTicks(l->segment(), tick, track);
                if (lct > Fraction(0, 1)) {
                    // set lyric tick to the total length from the lyric note
                    // plus all notes covered by the melisma minus the last note length
                    l->setTicks(tick - par->tick() - lct);
                }
                list.push_back(l);
            }
        }
    }
    // cleanup
    for (Lyrics *l : list) {
        muse::remove(lyrics_, l);
    }
}

//---------------------------------------------------------
//   MusicXmlStepAltOct2Pitch
//---------------------------------------------------------

/**
 Convert MusicXML \a step (0=C, 1=D, etc.) / \a alter / \a octave to midi pitch.
 Note: same code is in pass 1 and in pass 2.
 TODO: combine
 */

static int MusicXmlStepAltOct2Pitch(int step, int alter, int octave) {
    //                       c  d  e  f  g  a   b
    static int table[7] = {0, 2, 4, 5, 7, 9, 11};
    if (step < 0 || step > 6) {
        LOGD("MusicXmlStepAltOct2Pitch: illegal step %d", step);
        return -1;
    }
    int pitch = table[step] + alter + (octave + 1) * 12;

    if (!pitchIsValid(pitch)) {
        pitch = -1;
    }

    return pitch;
}

//---------------------------------------------------------
//   xmlSetPitch
//---------------------------------------------------------

/**
 Convert MusicXML \a step / \a alter / \a octave to midi pitch,
 set pitch and tpc.
 Note that n's staff and track have not been set yet
 */

static void xmlSetPitch(Note *n, int step, int alter, double tuning, int octave, const int octaveShift, const Instrument *const instr) {
    //LOGD("xmlSetPitch(n=%p, step=%d, alter=%d, octave=%d, octaveShift=%d)",
    //       n, step, alter, octave, octaveShift);

    //const Staff* staff = n->score()->staff(track / VOICES);
    //const Instrument* instr = staff->part()->instr();

    const Interval intval = instr->transpose();

    //LOGD("  staff=%p instr=%p dia=%d chro=%d",
    //       staff, instr, static_cast<int>(intval.diatonic), static_cast<int>(intval.chromatic));

    int pitch = MusicXmlStepAltOct2Pitch(step, alter, octave);
    pitch += intval.chromatic; // assume not in concert pitch
    pitch += 12 * octaveShift; // correct for octave shift
    // ensure sane values
    pitch = std::clamp(pitch, 0, 127);

    int tpc2 = step2tpc(step, AccidentalVal(alter));
    int tpc1 = Transpose::transposeTpc(tpc2, intval, true);
    n->setPitch(pitch, tpc1, tpc2);
    n->setTuning(tuning);
    //LOGD("  pitch=%d tpc1=%d tpc2=%d", n->pitch(), n->tpc1(), n->tpc2());
}

//---------------------------------------------------------
//   fillGap
//---------------------------------------------------------

/**
 Fill one gap (tstart - tend) in this track in this measure with rest(s).
 */

static void fillGap(Measure *measure, track_idx_t track, const Fraction &tstart, const Fraction &tend) {
    Fraction ctick = tstart;
    Fraction restLen = tend - tstart;
    // LOGD("\nfillGIFV     fillGap(measure %p track %d tstart %d tend %d) restLen %d len",
    //        measure, track, tstart, tend, restLen);
    // note: as Constants::division (#ticks in a quarter note) equals 480
    // Constants::division / 64 (#ticks in a 256th note) equals 7.5 but is rounded down to 7
    while (restLen > Fraction(1, 256)) {
        Fraction len = restLen;
        TDuration d(DurationType::V_INVALID);
        if (measure->ticks() == restLen) {
            d.setType(DurationType::V_MEASURE);
        } else {
            d.setVal(len.ticks());
        }
        Segment *s = measure->getSegment(SegmentType::ChordRest, tstart);
        Rest *rest = Factory::createRest(s, d);
        rest->setTicks(len);
        rest->setTrack(track);
        rest->setVisible(false);
        s->add(rest);
        len = rest->globalTicks();
        // LOGD(" %d", len);
        ctick += len;
        restLen -= len;
    }
}

//---------------------------------------------------------
//   fillGapsInFirstVoices
//---------------------------------------------------------

/**
 Fill gaps in first voice of every staff in this measure for this part with rest(s).
 */

static void fillGapsInFirstVoices(Measure *measure, Part *part) {
    IF_ASSERT_FAILED(measure) { return; }
    IF_ASSERT_FAILED(part) { return; }

    Fraction measTick = measure->tick();
    Fraction measLen = measure->ticks();
    Fraction nextMeasTick = measTick + measLen;
    staff_idx_t staffIdx = part->score()->staffIdx(part);
    /*
     LOGD("fillGIFV measure %p part %p idx %d nstaves %d tick %d - %d (len %d)",
     measure, part, staffIdx, part->nstaves(),
     measTick, nextMeasTick, measLen);
     */
    for (staff_idx_t st = 0; st < part->nstaves(); ++st) {
        track_idx_t track = (staffIdx + st) * VOICES;
        Fraction endOfLastCR = measTick;
        for (Segment *s = measure->first(); s; s = s->next()) {
            // LOGD("fillGIFV   segment %p tp %s", s, s->subTypeName());
            EngravingItem *el = s->element(track);
            if (el) {
                // LOGD(" el[%d] %p", track, el);
                if (s->isChordRestType()) {
                    ChordRest *cr = toChordRest(el);
                    Fraction crTick = cr->tick();
                    Fraction crLen = cr->globalTicks();
                    Fraction nextCrTick = crTick + crLen;
                    /*
                     LOGD(" chord/rest tick %d - %d (len %d)",
                     crTick, nextCrTick, crLen);
                     */
                    if (crTick > endOfLastCR) {
                        /*
                         LOGD(" GAP: track %d tick %d - %d",
                         track, endOfLastCR, crTick);
                         */
                        fillGap(measure, track, endOfLastCR, crTick);
                    }
                    endOfLastCR = nextCrTick;
                }
            }
        }
        if (nextMeasTick > endOfLastCR) {
            /*
             LOGD("fillGIFV   measure end GAP: track %d tick %d - %d",
             track, endOfLastCR, nextMeasTick);
             */
            fillGap(measure, track, endOfLastCR, nextMeasTick);
        }
    }
}

//---------------------------------------------------------
//   hasDrumset
//---------------------------------------------------------

/**
 Determine if \a instruments contains a valid drumset.
 This is the case if any instrument has a midi-unpitched element.
 */

static bool hasDrumset(const MusicXmlInstruments &instruments) {
    bool res = false;
    for (const auto &p : instruments) {
        // debug: dump the instruments
        //LOGD("instrument: %s %s", muPrintable(ii.key()), muPrintable(ii.value().toString()));
        // find valid unpitched values
        int unpitched = p.second.unpitched;
        if (0 <= unpitched && unpitched <= 127) {
            res = true;
        }
    }

    /*
    for (const auto& instr : instruments) {
          // MusicXML elements instrument-name, midi-program, instrument-sound, virtual-library, virtual-name
          // in a shell script use "mscore ... 2>&1 | grep GREP_ME | cut -d' ' -f3-" to extract
          LOGD("GREP_ME '%s',%d,'%s','%s','%s'",
                 muPrintable(instr.name),
                 instr.midiProgram + 1,
                 muPrintable(instr.sound),
                 muPrintable(instr.virtLib),
                 muPrintable(instr.virtName)
                 );
          }
     */

    return res;
}

//---------------------------------------------------------
//   initDrumset
//---------------------------------------------------------

/**
 Initialize drumset \a drumset.
 */

// first determine if the part contains a drumset
// (this is assumed if any instrument has a valid midi-unpitched element,
// which stored in the MusicXmlInstrument pitch field)
// then if the part contains a drumset, Drumset drumset is initialized

static void initDrumset(Drumset *drumset, const MusicXmlInstruments &instruments) {
    drumset->clear();

    for (const auto &ii : instruments) {
        // debug: also dump the drumset for this part
        //LOGD("initDrumset: instrument: %s %s", muPrintable(ii.key()), muPrintable(ii.value().toString()));
        int unpitched = ii.second.unpitched;
        if (0 <= unpitched && unpitched <= 127) {
            drumset->drum(ii.second.unpitched) =
                DrumInstrument(ii.second.name, ii.second.notehead, ii.second.line, ii.second.stemDirection);
        }
    }
}

//---------------------------------------------------------
//   setStaffTypePercussion
//---------------------------------------------------------

/**
 Set staff type to percussion
 */

static void setStaffTypePercussion(Part *part, Drumset *drumset) {
    for (staff_idx_t j = 0; j < part->nstaves(); ++j) {
        if (part->staff(j)->lines(Fraction(0, 1)) == 5 && !part->staff(j)->isDrumStaff(Fraction(0, 1))) {
            part->staff(j)->setStaffType(Fraction(0, 1), *StaffType::preset(StaffTypes::PERC_DEFAULT));
        }
    }
    // set drumset for instrument
    part->instrument()->setDrumset(drumset);
    part->instrument()->channel(0)->setBank(128);
}

static std::pair<String, String> processInstrName(const String &name) {
    // Make corrections so instruments match our names, then separate transpositions and remove unneeded info
    String n = name;
    n.replace(u"♭", u"b").replace(u"♯", u"#");
    n.replace(u"Sax", u"Saxophone");
    n.replace(u"Bari", u"Baritone");
    std::pair<String, String> ret;
    // Find transposition information
    static const std::regex transpositionRegex("(^|(?:\\s|\\u00A0))([ABCDEF][b#]?)((?:\\s|\\u00A0)|$)");
    // Remove: ' in', part numbers '1, 2' which may be bracketed, any text after a slash
    static const std::regex removeRegex("(?:\\s|\u00A0)in|(\\(?[0-9]([,](?:\\s|\u00A0)?)?)\\)?|\\.|(\\/.*)");

    const StringList results = n.search(transpositionRegex, {2});
    if (!results.empty()) {
        ret.second = convertPitchStringFlatsAndSharpsToUnicode(results.front());
        n.remove(transpositionRegex);
    }
    n.remove(removeRegex);
    ret.first = n.simplified();

    return ret;
}

//---------------------------------------------------------
//   createInstrument
//---------------------------------------------------------

/**
 Create an Instrument based on the information in \a mxmlInstr.
 */

static Instrument createInstrument(const MusicXmlInstrument &mxmlInstr, const Interval interval) {
    // The interval may not match the instrument's default traitName.
    const auto &[trackName, traitName] = processInstrName(mxmlInstr.name);
    Trait trait;
    trait.name = traitName;

    // Initialize instrument with data from MusicXML.
    Instrument instr;
    InstrChannel *channel = instr.channel(0);
    instr.setMusicXmlId(mxmlInstr.sound);
    instr.setTrackName(trackName);
    instr.setLongName(trackName);
    instr.setShortName(trackName);
    instr.setTrait(trait);
    instr.setTranspose(interval);
    channel->setBank(0);
    channel->setProgram(mxmlInstr.midiProgram);

    // Is it similar to one of our built-in instruments?
    if (const InstrumentTemplate *templ = combinedTemplateSearch(instr)) {
        instr = Instrument::fromTemplate(templ); // Re-initialize with built-in data.
        channel = instr.channel(0);
    } else {
        // set articulations to default (global articulations)
        instr.setArticulation(midiArticulations);
        // set default program
        channel->setProgram(mxmlInstr.midiProgram < 0 ? 0 : mxmlInstr.midiProgram);
    }

    // (Re-)apply values from MusicXML.
    instr.setTrackName(mxmlInstr.name);
    instr.setTranspose(interval);
    channel->setPan(mxmlInstr.midiPan);
    channel->setVolume(mxmlInstr.midiVolume);

    return instr;
}

//---------------------------------------------------------
//   updatePartWithInstrument
//---------------------------------------------------------

static void updatePartWithInstrument(Part *const part, const MusicXmlInstrument &mxmlInstr, const Interval interval,
                                     const bool hasDrumset = false) {
    Instrument instr = createInstrument(mxmlInstr, interval);
    if (hasDrumset) {
        instr.channel(0)->setBank(128);
    }
    part->setInstrument(instr);
    if (mxmlInstr.midiChannel >= 0) {
        part->setMidiChannel(mxmlInstr.midiChannel, mxmlInstr.midiPort);
    }
    // note: setMidiProgram() does more than simply setting the MIDI program
    if (mxmlInstr.midiProgram >= 0) {
        part->setMidiProgram(mxmlInstr.midiProgram);
    }
}

//---------------------------------------------------------
//   createInstrumentChange
//---------------------------------------------------------

/**
 Create an InstrumentChange based on the information in \a mxmlInstr.
 */

static InstrumentChange *createInstrumentChange(Score *score, const MusicXmlInstrument &mxmlInstr, const Interval interval,
                                                const track_idx_t track, const Instrument *curInstr) {
    const Instrument instr = createInstrument(mxmlInstr, interval);

    if (curInstr->id() == instr.id() && instr.longName() == curInstr->longName() && instr.shortName() == curInstr->shortName()) {
        return nullptr;
    }

    InstrumentChange *instrChange = Factory::createInstrumentChange(score->dummy(), instr);
    instrChange->setTrack(track);

    // for text use instrument name (if known) else use "Instrument change"
    const String text = mxmlInstr.name;
    instrChange->setXmlText(text.empty() ? "Instrument change" : text.toUtf8().constChar());
    instrChange->setVisible(false);

    return instrChange;
}

//---------------------------------------------------------
//   updatePartWithInstrumentChange
//---------------------------------------------------------

static void updatePartWithInstrumentChange(Part *const part, const MusicXmlInstrument &mxmlInstr, const Interval interval,
                                           Segment *const segment, const track_idx_t track, const Fraction tick) {
    const Instrument *curInstr = part->instrument(tick);
    InstrumentChange *const ic = createInstrumentChange(part->score(), mxmlInstr, interval, track, curInstr);

    if (!ic) {
        return;
    }

    segment->add(ic); // note: includes part::setInstrument(instr);

    // setMidiChannel() depends on setInstrument() already been done
    if (mxmlInstr.midiChannel >= 0) {
        part->setMidiChannel(mxmlInstr.midiChannel, mxmlInstr.midiPort, tick);
    }
}

static void setPercussionInstrument(MusicXmlInstrument &mxmlInstr, const String &partName) {
    // If there are multiple unpitched instruments we should set the part's instrument to drumkit or percussion
    if (partName.contains(u"drumset", CaseSensitivity::CaseInsensitive) ||
        partName.contains(u"drumkit", CaseSensitivity::CaseInsensitive)) {
        mxmlInstr = MusicXmlInstrument();
        mxmlInstr.name = u"Drum Kit";
    } else if (partName.contains(u"percussion", CaseSensitivity::CaseInsensitive)) {
        mxmlInstr = MusicXmlInstrument();
        mxmlInstr.name = u"Percussion";
        mxmlInstr.sound = u"drum.group.set";
    }
}

//---------------------------------------------------------
//   setPartInstruments
//---------------------------------------------------------

/**
 Set instruments for Part \a part
 Note:
 - MusicXmlInstrList: which instrument plays when
 - MusicXmlInstruments: instrument details from score-part and part
 */

static void setPartInstruments(MusicXmlLogger *logger, const XmlStreamReader *xmlreader, Part *part, const String &partId,
                               const Score *score, const MusicXmlInstrList &instrList, const MusicXmlIntervalList &intervList,
                               const MusicXmlInstruments &instruments, const String &partName) {
    if (instruments.empty()) {
        // no instrument details found, create a default instrument
        //LOGD("no instrument details");
        updatePartWithInstrument(part, {}, intervList.interval({0, 1}));
        return;
    }

    if (hasDrumset(instruments)) {
        // do not create multiple instruments for a drum part
        //LOGD("hasDrumset");
        MusicXmlInstrument mxmlInstr = instruments.begin()->second;
        if (instruments.size() > 1) {
            setPercussionInstrument(mxmlInstr, partName);
        }
        updatePartWithInstrument(part, mxmlInstr, {}, true);
        return;
    }

    if (instrList.empty()) {
        // instrument details found, but no instrument ids found
        // -> only a single instrument is playing in the part
        //LOGD("single instrument");
        MusicXmlInstrument mxmlInstr = instruments.begin()->second;
        updatePartWithInstrument(part, mxmlInstr, intervList.interval({0, 1}));
        return;
    }

    // either a single instrument is playing, or forwards / rests resulted in gaps in the instrument map
    // (and thus multiple entries)
    //LOGD("possibly multiple instruments");
    String prevInstrId;
    for (auto it = instrList.cbegin(); it != instrList.cend(); ++it) {
        Fraction tick = (*it).first;
        if (it == instrList.cbegin()) {
            prevInstrId = (*it).second; // first instrument id
            MusicXmlInstrument mxmlInstr = muse::value(instruments, prevInstrId);
            updatePartWithInstrument(part, mxmlInstr, intervList.interval(tick));
        } else {
            String instrId = (*it).second;
            bool mustInsert = instrId != prevInstrId;
            /*
             LOGD("tick %s previd %s id %s mustInsert %d",
             muPrintable(tick.print()),
             muPrintable(prevInstrId),
             muPrintable(instrId),
             mustInsert);
             */
            if (mustInsert) {
                const staff_idx_t staff = score->staffIdx(part);
                const track_idx_t track = staff * VOICES;
                //LOGD("instrument change: tick %s (%d) track %d instr '%s'",
                //       muPrintable(tick.print()), tick.ticks(), track, muPrintable(instrId));

                Measure *const m = score->tick2measure(tick);
                Segment *const segment = m->getSegment(SegmentType::ChordRest, tick);

                if (!segment) {
                    logger->logError(String(u"segment for instrument change at tick %1 not found").arg(tick.ticks()), xmlreader);
                } else if (!muse::contains(instruments, instrId)) {
                    logger->logError(
                        String(u"changed instrument '%1' at tick %2 not found in part '%3'").arg(instrId).arg(tick.ticks()).arg(partId),
                        xmlreader);
                } else {
                    MusicXmlInstrument mxmlInstr = muse::value(instruments, instrId);
                    updatePartWithInstrumentChange(part, mxmlInstr, intervList.interval(tick), segment, track, tick);
                }
            }
            prevInstrId = instrId;
        }
    }
}

//---------------------------------------------------------
//   text2syms
//---------------------------------------------------------

/**
 Convert SMuFL code points to MuseScore <sym>...</sym> notation.
 When \p isDolet is true (i.e. the file was produced by Dolet and the text comes
 from a MusicXML \c \<words\> or \c \<rehearsal\> element), every occurrence of \c $
 or \c Ø within the text is treated as a Dolet-plugin segno/coda shorthand and replaced
 with the corresponding SMuFL symbol — including when embedded in longer strings such as
 "Dal $ al Ø" or "To Ø".  In all other contexts (non-Dolet files, or lyric text) the
 characters are preserved as-is.
 */
namespace xmlpass2 {
static String text2syms(const String &t, const IEngravingFontsProvider &fonts, bool isDolet = false) {
    //QTime time;
    //time.start();

    // first create a map from symbol (Unicode) text to symId
    // note that this takes about 1 msec on a Core i5,
    // caching does not gain much

    IEngravingFontPtr sf = fonts.fallbackFont();
    std::map<String, SymId> map;
    size_t maxStringSize = 0; // maximum string size found

    for (int i = int(SymId::noSym); i < int(SymId::lastSym); ++i) {
        SymId id((SymId(i)));
        String string = sf->toString(id);
        // insert all syms except space to prevent matching all regular spaces
        if (id != SymId::space) {
            map.insert({string, id});
        }
        if (string.size() > maxStringSize) {
            maxStringSize = string.size();
        }
    }

    if (isDolet) {
        map.insert({u"$", SymId::segno});
        map.insert({u"Ø", SymId::coda});
    }

    //LOGD("text2syms map count %d maxsz %d filling time elapsed: %d ms",
    //       map.size(), maxStringSize, time.elapsed());

    // then look for matches
    String in = t;
    String res;

    while (!in.empty()) {
        // try to find the largest match possible
        int maxMatch = int(std::min(in.size(), maxStringSize));
        AsciiStringView sym;
        while (maxMatch > 0) {
            String toBeMatched = in.left(maxMatch);
            if (muse::contains(map, toBeMatched)) {
                sym = SymNames::nameForSymId(map.at(toBeMatched));
                break;
            }
            maxMatch--;
        }
        if (maxMatch > 0) {
            // found a match, add sym to res and remove match from string in
            res += u"<sym>";
            res += String::fromAscii(sym.ascii());
            res += u"</sym>";
            in.remove(0, maxMatch);
        } else {
            // not found, move one char from res to in
            res += in.left(1);
            in.remove(0, 1);
        }
    }

    //LOGD("text2syms total time elapsed: %d ms, res '%s'", time.elapsed(), muPrintable(res));
    return res;
}
} // namespace xmlpass2

//---------------------------------------------------------
//   nextPartOfFormattedString
//---------------------------------------------------------

// TODO: probably should be shared between pass 1 and 2

namespace xmlpass2 {
static String nextPartOfFormattedString(XmlStreamReader &e, const IMusicXmlConfiguration &configuration,
                                        const IEngravingFontsProvider &fonts, bool isDolet = false) {
    //String lang       = e.attribute(String("xml:lang"), "it");
    String fontWeight = e.attribute("font-weight");
    String fontSize = e.attribute("font-size");
    String fontStyle = e.attribute("font-style");
    String underline = e.attribute("underline");
    String strike = e.attribute("line-through");
    String fontFamily = e.attribute("font-family");
    // TODO: color, enclosure, yoffset in only part of the text, ...

    String txt = e.readText();
    // replace HTML entities
    txt = String::decodeXmlEntities(txt);
    String syms = xmlpass2::text2syms(txt, fonts, isDolet);

    String importedtext;

    if (!fontSize.empty()) {
        bool ok = true;
        float size = fontSize.toFloat(&ok);
        if (ok) {
            importedtext += String(u"<font size=\"%1\"/>").arg(size);
        }
    }

    bool needUseDefaultFont = configuration.needUseDefaultFont();
    if (!fontFamily.empty() && txt == syms && !needUseDefaultFont) {
        // add font family only if no <sym> replacement made
        importedtext += String(u"<font face=\"%1\"/>").arg(fontFamily);
    }
    if (fontWeight == u"bold") {
        importedtext += u"<b>";
    }
    if (fontStyle == u"italic") {
        importedtext += u"<i>";
    }
    if (!underline.empty()) {
        bool ok = true;
        int lines = underline.toInt(&ok);
        if (ok && (lines > 0)) { // 1, 2, or 3 underlines are imported as single underline
            importedtext += u"<u>";
        } else {
            underline.clear();
        }
    }
    if (!strike.empty()) {
        bool ok = true;
        int lines = strike.toInt(&ok);
        if (ok && (lines > 0)) { // 1, 2, or 3 strikes are imported as single strike
            importedtext += u"<s>";
        } else {
            strike.clear();
        }
    }
    if (txt == syms) {
        txt.replace(String(u"\r"), String()); // convert Windows line break \r\n -> \n
        importedtext += txt.toXmlEscaped();
    } else {
        // <sym> replacement made, should be no need for line break or other conversions
        importedtext += syms;
    }
    if (!strike.empty()) {
        importedtext += u"</s>";
    }
    if (!underline.empty()) {
        importedtext += u"</u>";
    }
    if (fontStyle == u"italic") {
        importedtext += u"</i>";
    }
    if (fontWeight == u"bold") {
        importedtext += u"</b>";
    }
    //LOGD("importedtext '%s'", muPrintable(importedtext));
    return importedtext;
}
} // namespace xmlpass2

//---------------------------------------------------------
//   colorItem
//---------------------------------------------------------

static void colorItem(EngravingItem *item, const Color color) {
    if (color.isValid()) {
        item->setProperty(Pid::COLOR, color);
        item->setPropertyFlags(Pid::COLOR, PropertyFlags::UNSTYLED);
    }
}

// Convert a MusicXML percentage without a locale change or an unsafe cast.
static int musicXmlDynamicVelocity(const String &value, int minimum, int fallback) {
    const std::string text = value.toStdString();
    const auto isXmlSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    const char *first = text.data();
    const char *last = first + text.size();
    while (first != last && isXmlSpace(*first)) ++first;
    while (first != last && isXmlSpace(*(last - 1))) --last;
    if (first == last) return fallback;
    // Floating-point from_chars accepts a minus sign, but not a leading plus.
    if (*first == '+') {
        if (++first == last || *first == '+' || *first == '-') return fallback;
    }

    double percent = 0.0;
    const auto result = std::from_chars(first, last, percent, std::chars_format::general);
    if (result.ptr != last) return fallback;
    if (result.ec == std::errc::result_out_of_range) {
        if (*first == '-') return minimum;

        // from_chars reports both decimal overflow and underflow as out of
        // range. The order of the first nonzero digit distinguishes them,
        // including very long mantissas and exponents, without a regex stack.
        int64_t integerDigits = 0;
        int64_t digitIndex = 0;
        int64_t firstNonzero = -1;
        bool afterPoint = false;
        const char *p = first;
        for (; p != last && *p != 'e' && *p != 'E'; ++p) {
            if (*p == '.') {
                afterPoint = true;
            } else {
                if (!afterPoint) ++integerDigits;
                if (*p != '0' && firstNonzero < 0) firstNonzero = digitIndex;
                ++digitIndex;
            }
        }
        if (firstNonzero < 0) return minimum;
        constexpr int64_t limit = INT64_MAX / 4;
        int64_t exponent = 0;
        bool exponentNegative = false;
        if (p != last) {
            ++p;
            if (p != last && (*p == '+' || *p == '-')) exponentNegative = *p++ == '-';
            for (; p != last; ++p) {
                const int digit = *p - '0';
                if (exponent > (limit - digit) / 10)
                    exponent = limit;
                else
                    exponent = exponent * 10 + digit;
            }
        }
        if (exponentNegative) exponent = -exponent;
        const int64_t order = integerDigits - firstNonzero - 1 + exponent;
        return order > 0 ? 127 : minimum;
    }
    if (result.ec != std::errc{} || !std::isfinite(percent)) return fallback;
    if (percent <= minimum / 0.9) return minimum;
    if (percent >= 127.0 / 0.9) return 127;
    return static_cast<int>(std::round(percent * 0.9));
}

// readText() keeps only the last character token. Accumulate the tokens
// locally for other-dynamics (including CDATA/comments); never interpret input
// text as TextBase markup. Nested elements are outside MusicXML simple content.
static String readDynamicsText(XmlStreamReader &e) {
    String text;
    while (!e.atEnd()) {
        e.readNext();
        if (e.isEndElement()) {
            break;
        }
        if (e.isCharacters()) {
            text += e.text();
        } else if (e.isStartElement()) {
            e.skipCurrentElement();
        }
    }
    return text;
}

static String dynamicLettersToXml(const String &text) {
    String result;
    for (size_t i = 0; i < text.size(); ++i) {
        switch (text.at(i).unicode()) {
            case u'p': result += u"<sym>dynamicPiano</sym>"; break;
            case u'm': result += u"<sym>dynamicMezzo</sym>"; break;
            case u'f': result += u"<sym>dynamicForte</sym>"; break;
            case u'r': result += u"<sym>dynamicRinforzando</sym>"; break;
            case u's': result += u"<sym>dynamicSforzando</sym>"; break;
            case u'z': result += u"<sym>dynamicZ</sym>"; break;
            case u'n': result += u"<sym>dynamicNiente</sym>"; break;
            default: return {};
        }
    }
    return result;
}

// Protect literal tag-like spans and the contents of matched pairs from the
// dynamic classifier, while allowing ordinary text between spans to classify.
static void appendDynamicLiteral(MusicXmlDynamic &result, const String &value) {
    std::vector<std::pair<size_t, size_t>> spans;
    std::vector<std::pair<String, size_t>> openTags;
    size_t scan = 0;
    while (scan < value.size()) {
        const size_t begin = value.indexOf(u'<', scan);
        const size_t end = begin == muse::nidx ? muse::nidx : value.indexOf(u'>', begin + 1);
        if (end == muse::nidx) {
            break;
        }
        spans.emplace_back(begin, end + 1);
        size_t nameBegin = begin + 1;
        const bool closing = nameBegin < end && value.at(nameBegin).unicode() == u'/';
        if (closing) {
            ++nameBegin;
        }
        size_t nameEnd = nameBegin;
        while (nameEnd < end) {
            const char16_t c = value.at(nameEnd).unicode();
            if (!((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9') || c == u'_' || c == u'-' ||
                  c == u':')) {
                break;
            }
            ++nameEnd;
        }
        if (nameEnd > nameBegin) {
            const String name = value.mid(nameBegin, nameEnd - nameBegin);
            if (closing && !openTags.empty() && openTags.back().first == name) {
                spans.emplace_back(openTags.back().second, end + 1);
                openTags.pop_back();
            } else if (!closing && value.at(end - 1).unicode() != u'/') {
                openTags.emplace_back(name, begin);
            }
        }
        scan = end + 1;
    }
    std::sort(spans.begin(), spans.end());
    std::vector<std::pair<size_t, size_t>> merged;
    for (const auto &span : spans) {
        if (!merged.empty() && span.first <= merged.back().second) {
            merged.back().second = std::max(merged.back().second, span.second);
        } else {
            merged.push_back(span);
        }
    }
    size_t cursor = 0;
    for (const auto &[begin, end] : merged) {
        result.text += TextBase::plainToXmlText(value.mid(cursor, begin - cursor));
        const String literal = TextBase::plainToXmlText(value.mid(begin, end - begin));
        result.protectedLiteralRanges.emplace_back(result.text.size(), literal.size());
        result.text += literal;
        cursor = end;
    }
    result.text += TextBase::plainToXmlText(value.mid(cursor));
}

// Keep literal text distinct from markup generated from the symbol table.
static MusicXmlDynamic parseDynamicsContainer(XmlStreamReader &e, MusicXmlLogger *logger) {
    MusicXmlDynamic result;
    result.color = Color::fromString(e.attribute("color"));
    result.placement = e.attribute("placement");
    result.enclosure = e.attribute("enclosure");
    while (e.readNextStartElement()) {
        if (e.name() == "other-dynamics") {
            const String smufl = e.attribute("smufl");
            const String value = readDynamicsText(e);
            const SymId id = SymNames::symIdByName(smufl);
            // symIdByName(String) converts to ASCII and then uses a C string.
            // Require an exact round trip to reject lossy/truncated names too.
            const String canonical = id == SymId::noSym ? String() : String::fromAscii(SymNames::nameForSymId(id).ascii());
            // The generated standard block ends before engine/font extensions.
            if (!smufl.empty() && id != SymId::noSym && id < SymId::noteheadDoubleWholeAlt && canonical == smufl) {
                result.text += u"<sym>" + canonical + u"</sym>";
            } else {
                if (!smufl.empty() && logger) {
                    logger->logError(String(u"Unsupported dynamics SMuFL name '%1'; using literal text").arg(smufl), &e);
                }
                const String symbols = dynamicLettersToXml(value);
                if (smufl.empty() && !symbols.empty()) {
                    result.text += symbols;
                } else {
                    appendDynamicLiteral(result, value);
                }
            }
        } else {
            const String name = String::fromAscii(e.name().ascii());
            const String symbols = dynamicLettersToXml(name);
            // Upstream retains unrecognized tag names as literal dynamics text.
            result.text += symbols.empty() ? TextBase::plainToXmlText(name) : symbols;
            e.skipCurrentElement();
        }
    }
    return result;
}

// Pick one marker prefix absent from the input with a single scan, so many
// protected spans do not cause repeated whole-string searches.
static String dynamicLiteralMarkerPrefix(const String &text) {
    const std::string input = text.toStdString();
    const std::string prefix = "~LITERAL_";
    std::set<size_t> used;
    size_t offset = 0;
    while ((offset = input.find(prefix, offset)) != std::string::npos) {
        const size_t begin = offset + prefix.size();
        size_t number = 0;
        const auto parsed = std::from_chars(input.data() + begin, input.data() + input.size(), number);
        if (parsed.ec == std::errc() && parsed.ptr != input.data() + begin && parsed.ptr != input.data() + input.size() &&
            *parsed.ptr == '_') {
            used.insert(number);
        }
        offset = begin;
    }
    size_t nonce = 0;
    while (used.contains(nonce)) {
        ++nonce;
    }
    return String(u"~LITERAL_%1_").arg(nonce);
}

static Dynamic *createImportedDynamic(Score *score, const MusicXmlDynamic &entry) {
    Dynamic *dynamic = Factory::createDynamic(score->dummy());
    // The dynamic classifier also recognizes plain words. Hide markup-looking
    // literals from it temporarily, while still classifying genuine symbols and
    // ordinary prose in the rest of the container.
    String classificationText;
    std::vector<std::pair<String, String>> protectedLiterals;
    const String markerPrefix = entry.protectedLiteralRanges.empty() ? String() : dynamicLiteralMarkerPrefix(entry.text);
    size_t cursor = 0;
    for (size_t i = 0; i < entry.protectedLiteralRanges.size(); ++i) {
        const auto [start, length] = entry.protectedLiteralRanges[i];
        classificationText += entry.text.mid(cursor, start - cursor);
        const String marker = markerPrefix + String::number(i) + u"~";
        classificationText += marker;
        protectedLiterals.emplace_back(marker, entry.text.mid(start, length));
        cursor = start + length;
    }
    classificationText += entry.text.mid(cursor);
    dynamic->setDynamicType(classificationText);
    if (!protectedLiterals.empty()) {
        const String text = dynamic->xmlText();
        String restored;
        restored.reserve(entry.text.size());
        size_t position = 0;
        bool valid = true;
        for (const auto &[marker, literal] : protectedLiterals) {
            const size_t next = text.indexOf(marker, position);
            if (next == muse::nidx) {
                valid = false;
                break;
            }
            restored += text.mid(position, next - position);
            restored += literal;
            position = next + marker.size();
        }
        if (valid) {
            restored += text.mid(position);
        }
        dynamic->setXmlText(valid ? restored : entry.text);
    }
    colorItem(dynamic, entry.color);
    if (entry.enclosure == u"circle") {
        dynamic->setFrameType(FrameType::CIRCLE);
        dynamic->setPropertyFlags(Pid::FRAME_TYPE, PropertyFlags::UNSTYLED);
    } else if (entry.enclosure == u"none") {
        dynamic->setFrameType(FrameType::NO_FRAME);
        dynamic->setPropertyFlags(Pid::FRAME_TYPE, PropertyFlags::UNSTYLED);
    } else if (entry.enclosure == u"rectangle") {
        dynamic->setFrameType(FrameType::RECTANGLE);
        dynamic->setFrameRound(0_sp);
        dynamic->setPropertyFlags(Pid::FRAME_TYPE, PropertyFlags::UNSTYLED);
        dynamic->setPropertyFlags(Pid::FRAME_ROUND, PropertyFlags::UNSTYLED);
    }
    return dynamic;
}

//---------------------------------------------------------
//   addLyric
//---------------------------------------------------------

/**
 Add a single lyric to the score or delete it (if number too high)
 */

static void addLyric(MusicXmlLogger *logger, const XmlStreamReader *const xmlreader, ChordRest *cr, Lyrics *l, int lyricNo,
                     MusicXmlLyricsExtend &extendedLyrics) {
    if (lyricNo > MAX_LYRICS) {
        logger->logError(String(u"too much lyrics (>%1)").arg(MAX_LYRICS), xmlreader);
        delete l;
    } else {
        l->setVerse(lyricNo);
        l->initTextStyleType(l->isEven() ? TextStyleType::LYRICS_EVEN : TextStyleType::LYRICS_ODD, /*preserveDifferent*/ true);
        cr->add(l);
        extendedLyrics.setExtend(lyricNo, cr->track(), cr->tick(), l);
    }
}

//---------------------------------------------------------
//   addLyrics
//---------------------------------------------------------

/**
 Add a notes lyrics to the score
 */

static void addLyrics(MusicXmlLogger *logger, const XmlStreamReader *const xmlreader, ChordRest *cr,
                      const std::map<int, Lyrics *> &numbrdLyrics, const std::set<Lyrics *> &extLyrics,
                      MusicXmlLyricsExtend &extendedLyrics) {
    for (const auto &[lyricNo, lyric] : numbrdLyrics) {
        addLyric(logger, xmlreader, cr, lyric, lyricNo, extendedLyrics);
        if (muse::contains(extLyrics, lyric)) {
            extendedLyrics.addLyric(lyric);
        }
    }
}

static void addGraceNoteLyrics(const std::map<int, Lyrics *> &numberedLyrics, std::set<Lyrics *> extendedLyrics,
                               std::vector<GraceNoteLyrics> &gnLyrics) {
    for (const auto &[lyricNo, lyric] : numberedLyrics) {
        if (lyric) {
            bool extend = muse::contains(extendedLyrics, lyric);
            const GraceNoteLyrics gnl = GraceNoteLyrics(lyric, extend, lyricNo);
            gnLyrics.push_back(gnl);
        }
    }
}

static void addInferredStickings(ChordRest *cr, const std::vector<Sticking *> &numberedStickings) {
    for (Sticking *sticking : numberedStickings) {
        sticking->setOwnershipParent(cr->segment());
        sticking->setTrack(cr->track());
        cr->score()->addElement(sticking);
    }
}

//---------------------------------------------------------
//   addElemOffset
//---------------------------------------------------------

void MusicXmlParserPass2::addElemOffset(engraving::EngravingItem *el, engraving::track_idx_t track, const muse::String &placement,
                                        engraving::Measure *measure, const engraving::Fraction &tick) {
    if (!measure) {
        return;
    }
    Fraction elTick = tick;

    if (!placement.empty()) {
        if (el->hasVoiceAssignmentProperties()) {
            el->setProperty(Pid::DIRECTION, placement == u"above" ? DirectionV::UP : DirectionV::DOWN);
            el->setPropertyFlags(Pid::DIRECTION, PropertyFlags::UNSTYLED);
        } else {
            el->setPlacement(placement == u"above" ? PlacementV::ABOVE : PlacementV::BELOW);
            el->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
            if (!el->isSticking()) {
                el->resetProperty(Pid::OFFSET);
            }
        }
    }
    const Fraction &endTick = measure->score()->endTick();
    if (elTick > endTick) {
        elTick = endTick;
    }

    el->setTrack(el->isTempoText() ? 0 : track); // TempoText must be in track 0
    Segment *s = measure->getSegment(SegmentType::ChordRest, elTick);

    if (el->isSticking()) {
        if (el->propertyFlags(Pid::OFFSET) == PropertyFlags::UNSTYLED) {
            const EngravingItem *item = s->element(el->track());
            const Chord *chord = item && item->isChord() ? toChord(item) : nullptr;
            const bool hasGraceNotes = chord && !chord->graceNotes().empty();

            if (!hasGraceNotes) {
                el->resetProperty(Pid::OFFSET);
            }
        }
    }

    if (el->systemFlag()) {
        const bool finale = pass1_.exporterSoftware() == MusicXmlExporterSoftware::FINALE;
        Score *score = measure->score();
        Staff *st = score->staff(track2staff(track));

        if (!st->isSystemObjectStaff() && st->idx() != 0) {
            if (finale) {
                delete el;
                return;
            }
            score->addSystemObjectStaff(st);
        }

        bool found = false;
        for (EngravingItem *existingEl : muse::values(systemElements(), elTick.ticks())) {
            if (el->type() == existingEl->type()) {
                if (el->isTextBase()) {
                    TextBase *elText = toTextBase(el);
                    TextBase *existingText = toTextBase(existingEl);
                    if (elText->xmlText() == existingText->xmlText()) {
                        found = true;
                        break;
                    } else {
                        continue;
                    }
                }
                found = true;
                break;
            }
        }
        if (!found) {
            if (el->isMarker() || el->isJump()) {
                el->setOwnershipParent(measure);
            } else {
                el->setOwnershipParent(s);
            }
            addSystemElement(el, elTick);
        }
    } else {
        s->add(el);
    }
}

//---------------------------------------------------------
//   calculateTupletDuration
//---------------------------------------------------------

/**
 Calculate the duration of all notes in the tuplet combined
 */

static Fraction calculateTupletDuration(const Tuplet *const t) {
    Fraction res;

    for (DurationElement *de : t->elements()) {
        if (de->isChordRest()) {
            const ChordRest *cr = toChordRest(de);
            const Fraction fraction = cr->ticks(); // TODO : take care of nested tuplets
            if (fraction.isValid()) {
                res += fraction;
            }
        }
    }
    res /= t->ratio();

    return res;
}

//---------------------------------------------------------
//   determineTupletBaseLen
//---------------------------------------------------------

/**
 Determine tuplet baseLen as determined by the tuplet ratio
 and duration.
 */

static TDuration determineTupletBaseLen(const Tuplet *const t) {
    Fraction tupletFraction;
    Fraction tupletFullDuration;
    MusicXmlTupletState::determineTupletFractionAndFullDuration(calculateTupletDuration(t), tupletFraction, tupletFullDuration);

    Fraction baseLen = tupletFullDuration * Fraction(1, t->ratio().denominator());
    /*
    LOGD("tupletFraction %s tupletFullDuration %s ratio %s baseLen %s",
           muPrintable(tupletFraction.toString()),
           muPrintable(tupletFullDuration.toString()),
           muPrintable(t->ratio().toString()),
           muPrintable(baseLen.toString())
           );
     */

    return TDuration(baseLen);
}

//---------------------------------------------------------
//   handleTupletStart
//---------------------------------------------------------

static void handleTupletStart(const ChordRest *const cr, Tuplet *&tuplet, const int actualNotes, const int normalNotes,
                              const MusicXmlTupletDesc &tupletDesc) {
    tuplet = new Tuplet(cr->measure());
    tuplet->setTrack(cr->track());
    tuplet->setRatio(Fraction(actualNotes, normalNotes));
    tuplet->setTick(cr->tick());
    tuplet->setBracketType(tupletDesc.bracket);
    tuplet->setNumberType(tupletDesc.shownumber);
    tuplet->setDirection(tupletDesc.direction);
}

//---------------------------------------------------------
//   handleTupletStop
//---------------------------------------------------------

static void handleTupletStop(Tuplet *&tuplet, const int normalNotes) {
    // allow handleTupletStop to be called w/o error of no tuplet active
    if (!tuplet) {
        return;
    }

    // set baselen
    TDuration td = determineTupletBaseLen(tuplet);
    tuplet->setBaseLen(td);
    Fraction f(normalNotes, td.fraction().denominator());
    f.reduce();
    if (!f.isValid()) {
        LOGD("MusicXml::import: tuplet stop but note values too small");
        tuplet = nullptr;
        return;
    }
    tuplet->setTicks(f);
    // TODO determine usefulness of following check
    int totalDuration = 0;
    int ticksPerNote = f.ticks() / tuplet->ratio().numerator();
    bool ticksCorrect = true;
    for (DurationElement *de : tuplet->elements()) {
        if (de->isChordRest()) {
            int globalTicks = de->globalTicks().ticks();
            if (globalTicks != ticksPerNote) {
                ticksCorrect = false;
            }
            totalDuration += globalTicks;
        }
    }
    if (totalDuration != f.ticks()) {
        LOGD("MusicXml::import: tuplet stop but bad duration"); // TODO
    }
    if (!ticksCorrect) {
        LOGD("MusicXml::import: tuplet stop but uneven note ticks"); // TODO
    }
    tuplet = 0;
}

//---------------------------------------------------------
//   setElementPropertyFlags
//---------------------------------------------------------

static void setElementPropertyFlags(EngravingObject *element, const Pid propertyId, const String value1, const String value2 = String()) {
    if (value1.empty()) { // Set as an implicit value
        element->setPropertyFlags(propertyId, PropertyFlags::STYLED);
    } else if (!value1.empty() || !value2.empty()) { // Set as an explicit value
        element->setPropertyFlags(propertyId, PropertyFlags::UNSTYLED);
    }
}

//---------------------------------------------------------
//   addArticulationToChord
//---------------------------------------------------------

static void addArticulationToChord(const Notation &notation, ChordRest *cr) {
    const SymId articSym = notation.symId();
    const String dir = notation.attribute(u"type");
    const String place = notation.attribute(u"placement");
    Articulation *na = Factory::createArticulation(cr);
    if (articSym != SymId::noSym) {
        na->setSymId(articSym);
    }
    if (!notation.text().empty()) {
        na->setTextType(TConv::fromXml(notation.text().toAscii().constChar(), ArticulationTextType::NO_TEXT));
    }
    na->setVisible(notation.visible());
    colorItem(na, Color::fromString(notation.attribute(u"color")));

    if (dir == "up" || dir == "down") {
        na->setUp(dir == "up");
        na->setPropertyFlags(Pid::DIRECTION, PropertyFlags::UNSTYLED);
    }

    // when setting anchor, assume type up/down without explicit placement
    // implies placement above/below
    if (place == "above" || (dir == "up" && place.empty())) {
        na->setAnchor(ArticulationAnchor::TOP);
        na->setPropertyFlags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    } else if (place == "below" || (dir == "down" && place.empty())) {
        na->setAnchor(ArticulationAnchor::BOTTOM);
        na->setPropertyFlags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    }

    if (cr->isChord() && toChord(cr)->hasArticulation(na)) {
        return;
    }

    cr->add(na);
}

//---------------------------------------------------------
//   addFermataToChord
//---------------------------------------------------------

/**
 Add a MusicXML fermata.
 Note: MusicXML common.mod: "The fermata type is upright if not specified."
 */

static void addFermataToChord(const Notation &notation, ChordRest *cr) {
    const SymId articSym = notation.symId();
    const String direction = notation.attribute(u"type");
    Segment *seg = cr->segment();
    Fermata *fermata = Factory::createFermata(parentOrDummy(seg, cr->score()->dummy()));
    fermata->setSymIdAndTimeStretch(articSym);
    fermata->setTrack(cr->track());
    fermata->setVisible(notation.visible());
    colorItem(fermata, Color::fromString(notation.attribute(u"color")));
    if (!direction.empty()) {
        fermata->setPlacement(direction == "inverted" ? PlacementV::BELOW : PlacementV::ABOVE);
        fermata->resetProperty(Pid::OFFSET);
    } else {
        fermata->setPlacement(fermata->propertyDefault(Pid::PLACEMENT).value<PlacementV>());
    }
    setElementPropertyFlags(fermata, Pid::PLACEMENT, direction);
    if (!seg) {
        assert(cr->isGrace());
        cr->addFermata(fermata); // store for later move to segment
    } else {
        cr->segment()->add(fermata);
    }
}

//---------------------------------------------------------
//   addTapToChord
//---------------------------------------------------------

/**
 Add Tap to Chord.
 */

static void addTapToChord(const Notation &notation, ChordRest *cr) {
    const String place = notation.attribute(u"placement");
    const TappingHand hand = notation.attribute(u"hand") == u"right" ? TappingHand::RIGHT : TappingHand::LEFT;

    Tapping *tap = Factory::createTapping(cr);
    tap->setTrack(cr->track());
    tap->setHand(hand);
    if (place == u"above") {
        tap->setAnchor(ArticulationAnchor::TOP);
        tap->setPropertyFlags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    } else if (place == u"below") {
        tap->setAnchor(ArticulationAnchor::BOTTOM);
        tap->setPropertyFlags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    } else {
        tap->setAnchor(ArticulationAnchor::AUTO);
    }
    colorItem(tap, Color::fromString(notation.attribute(u"color")));
    cr->add(tap);
}

//---------------------------------------------------------
//   addMordentToChord
//---------------------------------------------------------

/**
 Add Mordent to Chord.
 */

static void addMordentToChord(const Notation &notation, ChordRest *cr) {
    const String name = notation.name();
    const String attrLong = notation.attribute(u"long");
    const String attrAppr = notation.attribute(u"approach");
    const String attrDep = notation.attribute(u"departure");
    SymId articSym = SymId::noSym; // legal but impossible ArticulationType value here indicating "not found"
    if (name == "inverted-mordent") {
        if ((attrLong.empty() || attrLong == "no") && attrAppr.empty() && attrDep.empty()) {
            articSym = SymId::ornamentShortTrill;
        } else if (attrLong == "yes" && attrAppr.empty() && attrDep.empty()) {
            articSym = SymId::ornamentTremblement;
        } else if (attrLong == "yes" && attrAppr == "below" && attrDep.empty()) {
            articSym = SymId::ornamentUpPrall;
        } else if (attrLong == "yes" && attrAppr == "above" && attrDep.empty()) {
            articSym = SymId::ornamentPrecompMordentUpperPrefix;
        } else if (attrLong == "yes" && attrAppr.empty() && attrDep == "below") {
            articSym = SymId::ornamentPrallDown;
        } else if (attrLong == "yes" && attrAppr.empty() && attrDep == "above") {
            articSym = SymId::ornamentPrallUp;
        }
    } else if (name == "mordent") {
        if ((attrLong.empty() || attrLong == "no") && attrAppr.empty() && attrDep.empty()) {
            articSym = SymId::ornamentMordent;
        } else if (attrLong == "yes" && attrAppr.empty() && attrDep.empty()) {
            articSym = SymId::ornamentPrallMordent;
        } else if (attrLong == "yes" && attrAppr == "below" && attrDep.empty()) {
            articSym = SymId::ornamentUpMordent;
        } else if (attrLong == "yes" && attrAppr == "above" && attrDep.empty()) {
            articSym = SymId::ornamentDownMordent;
        }
    }
    if (articSym != SymId::noSym) {
        const String place = notation.attribute(u"placement");
        Ornament *mordent = Factory::createOrnament(cr);
        mordent->setSymId(articSym);
        if (place == u"above") {
            mordent->setAnchor(ArticulationAnchor::TOP);
        } else if (place == u"below") {
            mordent->setAnchor(ArticulationAnchor::BOTTOM);
        } else {
            mordent->setAnchor(ArticulationAnchor::AUTO);
        }
        mordent->setVisible(notation.visible());
        colorItem(mordent, Color::fromString(notation.attribute(u"color")));
        cr->add(mordent);
    } else {
        LOGD("unknown ornament: name '%s' long '%s' approach '%s' departure '%s'", muPrintable(name), muPrintable(attrLong),
             muPrintable(attrAppr), muPrintable(attrDep)); // TODO
    }
}

//---------------------------------------------------------
//   addTurnToChord
//---------------------------------------------------------

/**
 Add Turn to Chord.
 */

static void addTurnToChord(const Notation &notation, ChordRest *cr) {
    SymId turnSym = notation.symId();
    const String place = notation.attribute(u"placement");
    if (notation.attribute(u"slash") == "yes") {
        // TODO: currently this is the only available SMuFL turn with a slash
        turnSym = SymId::ornamentTurnSlash;
    }
    Ornament *turn = Factory::createOrnament(cr);
    turn->setSymId(turnSym);
    if (place == u"above") {
        turn->setAnchor(ArticulationAnchor::TOP);
    } else if (place == u"below") {
        turn->setAnchor(ArticulationAnchor::BOTTOM);
    } else {
        turn->setAnchor(ArticulationAnchor::AUTO);
    }
    turn->setVisible(notation.visible());
    colorItem(turn, Color::fromString(notation.attribute(u"color")));
    cr->add(turn);
}

//---------------------------------------------------------
//   addOtherOrnamentToChord
//---------------------------------------------------------

/**
 Add Other Ornament to Chord.
 */

static void addOtherOrnamentToChord(const Notation &notation, ChordRest *cr) {
    const String name = notation.name();
    const String symname = notation.attribute(u"smufl");
    SymId sym = SymId::noSym; // legal but impossible ArticulationType value here indicating "not found"
    sym = SymNames::symIdByName(symname);

    if (sym != SymId::noSym) {
        Ornament *ornam = Factory::createOrnament(cr);
        ornam->setSymId(sym);
        ornam->setVisible(notation.visible());
        colorItem(ornam, Color::fromString(notation.attribute(u"color")));
        cr->add(ornam);
    } else {
        LOGD("unknown ornament: name '%s': '%s'.", muPrintable(name), muPrintable(symname));
    }
}

//---------------------------------------------------------
//   convertArticulationToSymId
//---------------------------------------------------------

/**
 Convert a MusicXML articulation to a chord as a "simple" MuseScore articulation.
 These are the articulations that can be
 - represented by an enum class SymId
 - added to a ChordRest
 Return true (articulation recognized and converted)
 or false (articulation not recognized).
 Note simple implementation: MusicXML syntax is not strictly
 checked, the articulations parent element does not matter.
 */

static bool convertArticulationToSymId(const String &mxmlName, SymId &id) {
    // map MusicXML notations name to MuseScore symbol
    static const std::map<String, SymId> map{// ornaments
                                             {u"delayed-turn", SymId::ornamentTurn},
                                             {u"inverted-turn", SymId::ornamentTurnInverted},
                                             {u"vertical-turn", SymId::ornamentTurnUp},
                                             {u"inverted-vertical-turn", SymId::ornamentTurnUpS},
                                             {u"turn", SymId::ornamentTurn},
                                             {u"shake", SymId::ornamentTremblementCouperin},
                                             {u"schleifer", SymId::ornamentPrecompSlide},
                                             {u"haydn", SymId::ornamentHaydn},
                                             // articulations
                                             {u"accent", SymId::articAccentAbove},
                                             {u"strong-accent", SymId::articMarcatoAbove},
                                             {u"staccato", SymId::articStaccatoAbove},
                                             {u"tenuto", SymId::articTenutoAbove},
                                             {u"detached-legato", SymId::articTenutoStaccatoAbove},
                                             {u"staccatissimo", SymId::articStaccatissimoAbove},
                                             {u"spiccato", SymId::articStaccatissimoStrokeAbove},
                                             {u"stress", SymId::articStressAbove},
                                             {u"unstress", SymId::articUnstressAbove},
                                             {u"soft-accent", SymId::articSoftAccentAbove},
                                             // technical
                                             {u"up-bow", SymId::stringsUpBow},
                                             {u"down-bow", SymId::stringsDownBow},
                                             {u"open-string", SymId::brassMuteOpen},
                                             {u"thumb-position", SymId::stringsThumbPosition},
                                             {u"double-tongue", SymId::doubleTongueAbove},
                                             {u"triple-tongue", SymId::tripleTongueAbove},
                                             {u"stopped", SymId::brassMuteClosed},
                                             {u"snap-pizzicato", SymId::pluckedSnapPizzicatoAbove},
                                             {u"heel", SymId::keyboardPedalHeel1},
                                             {u"toe", SymId::keyboardPedalToe2},
                                             {u"fingernails", SymId::pluckedWithFingernails},
                                             {u"brass-bend", SymId::brassBend},
                                             {u"flip", SymId::brassFlip},
                                             {u"smear", SymId::brassSmear},
                                             {u"open", SymId::brassMuteOpen},
                                             {u"half-muted", SymId::brassMuteHalfClosed}, // ignoring the smufl attribute
                                             {u"golpe", SymId::guitarGolpe},

                                             {u"belltree", SymId::handbellsBelltree},
                                             {u"damp", SymId::handbellsDamp3},
                                             {u"echo", SymId::handbellsEcho1},
                                             {u"gyro", SymId::handbellsGyro},
                                             {u"hand martellato", SymId::handbellsHandMartellato},
                                             {u"mallet lift", SymId::handbellsMalletLft},
                                             {u"mallet table", SymId::handbellsMalletBellOnTable},
                                             {u"martellato", SymId::handbellsMartellato},
                                             {u"martellato lift", SymId::handbellsMartellatoLift},
                                             {u"muted martellato", SymId::handbellsMutedMartellato},
                                             {u"pluck lift", SymId::handbellsPluckLift},
                                             {u"swing", SymId::handbellsSwing}};

    auto it = map.find(mxmlName);
    if (it != map.end()) {
        id = it->second;
        return true;
    } else {
        id = SymId::noSym;
        return false;
    }
}

//---------------------------------------------------------
//   convertFermataToSymId
//---------------------------------------------------------

/**
 Convert a MusicXML fermata name to a MuseScore fermata.
 */

static SymId convertFermataToSymId(const String &mxmlName) {
    // map MusicXML fermata name to MuseScore symbol
    static const std::map<String, SymId> map{
        {u"normal", SymId::fermataAbove},
        {u"angled", SymId::fermataShortAbove},
        {u"square", SymId::fermataLongAbove},
        {u"double-angled", SymId::fermataVeryShortAbove},
        {u"double-square", SymId::fermataVeryLongAbove},
        {u"double-dot", SymId::fermataLongHenzeAbove},
        {u"half-curve", SymId::fermataShortHenzeAbove},
        {u"curlew", SymId::curlewSign},
    };

    auto it = map.find(mxmlName);
    if (it != map.end()) {
        return it->second;
    }

    return SymId::fermataAbove;
}

//---------------------------------------------------------
//   convertNotehead
//---------------------------------------------------------

/**
 Convert a MusicXML notehead name to a MuseScore headgroup.
 */

static NoteHeadGroup convertNotehead(String mxmlName) {
    // map MusicXML notehead name to a MuseScore headgroup
    static const std::map<String, NoteHeadGroup> map{{u"slash", NoteHeadGroup::HEAD_SLASH},
                                                     {u"triangle", NoteHeadGroup::HEAD_TRIANGLE_UP},
                                                     {u"diamond", NoteHeadGroup::HEAD_DIAMOND},
                                                     {u"cross", NoteHeadGroup::HEAD_PLUS},
                                                     {u"x", NoteHeadGroup::HEAD_CROSS},
                                                     {u"circle-x", NoteHeadGroup::HEAD_XCIRCLE},
                                                     {u"inverted triangle", NoteHeadGroup::HEAD_TRIANGLE_DOWN},
                                                     {u"slashed", NoteHeadGroup::HEAD_SLASHED1},
                                                     {u"back slashed", NoteHeadGroup::HEAD_SLASHED2},
                                                     {u"normal", NoteHeadGroup::HEAD_NORMAL},
                                                     {u"do", NoteHeadGroup::HEAD_DO},
                                                     {u"re", NoteHeadGroup::HEAD_RE},
                                                     {u"mi", NoteHeadGroup::HEAD_MI},
                                                     {u"fa", NoteHeadGroup::HEAD_FA},
                                                     {u"fa up", NoteHeadGroup::HEAD_FA},
                                                     {u"so", NoteHeadGroup::HEAD_SOL},
                                                     {u"la", NoteHeadGroup::HEAD_LA},
                                                     {u"ti", NoteHeadGroup::HEAD_TI}};

    auto it = map.find(mxmlName);
    if (it != map.end()) {
        return it->second;
    } else {
        LOGD("unknown notehead %s", muPrintable(mxmlName)); // TODO
    }

    // default: return 0
    return NoteHeadGroup::HEAD_NORMAL;
}

//---------------------------------------------------------
//   addTextToNote
//---------------------------------------------------------

/**
 Add Text to Note.
 */

static void addTextToNote(int64_t byteOffset, String txt, String placement, String fontWeight, double fontSize, String fontStyle,
                          String fontFamily, bool visible, Color color, TextStyleType subType, const Score *, Note *note,
                          bool needUseDefaultFont) {
    if (note) {
        if (!txt.empty()) {
            TextBase *t = Factory::createFingering(note, subType);
            t->setPlainText(txt);
            if (!fontFamily.empty() && !needUseDefaultFont) {
                t->setFamily(fontFamily);
                t->setPropertyFlags(Pid::FONT_FACE, PropertyFlags::UNSTYLED);
            }
            if (std::isnormal(fontSize) && fontSize > 0.0) {
                t->setSize(fontSize);
                t->setPropertyFlags(Pid::FONT_SIZE, PropertyFlags::UNSTYLED);
            }
            if (!fontWeight.empty()) {
                t->setBold(fontWeight == u"bold");
                t->setPropertyFlags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
            if (!fontStyle.empty()) {
                t->setItalic(fontStyle == u"italic");
                t->setPropertyFlags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
            if (!placement.empty()) {
                t->setPlacement(placement == u"below" ? PlacementV::BELOW : PlacementV::ABOVE);
                t->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
                t->resetProperty(Pid::OFFSET);
            }
            t->setVisible(visible);
            colorItem(t, color);
            note->add(t);
        }
    } else {
        LOGD("%s", muPrintable(String(u"Error at byte offset %1: no note for text").arg(byteOffset))); // TODO
    }
}

//---------------------------------------------------------
//   setSLinePlacement
//---------------------------------------------------------

/**
 Helper for direction().
 SLine placement is modified by changing the first segments user offset
 As the SLine has just been created, it does not have any segment yet
 */

static void setSLinePlacement(SLine *sli, const String &placement) {
    if (placement == u"above" || placement == u"below") {
        if (sli->hasVoiceAssignmentProperties()) {
            sli->setProperty(Pid::DIRECTION, placement == u"above" ? DirectionV::UP : DirectionV::DOWN);
            sli->setPropertyFlags(Pid::DIRECTION, PropertyFlags::UNSTYLED);
        } else {
            sli->setPlacement(placement == u"above" ? PlacementV::ABOVE : PlacementV::BELOW);
            sli->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
            sli->resetProperty(Pid::OFFSET);
        }
    }
}

//---------------------------------------------------------
//   handleSpannerStart
//---------------------------------------------------------

// note that in case of overlapping spanners, handleSpannerStart is called for every spanner
// as spanners std::map allows only one value per key, this does not hurt at all

static void handleSpannerStart(SLine *new_sp, track_idx_t track, String &placement, const Fraction &tick, MusicXmlSpannerMap &spanners) {
    new_sp->setTrack(track);
    setSLinePlacement(new_sp, placement);
    spanners[new_sp] = std::pair<int, int>(tick.ticks(), -1);
}

//---------------------------------------------------------
//   handleSpannerStop
//---------------------------------------------------------

static void handleSpannerStop(SLine *cur_sp, track_idx_t track2, const Fraction &tick, MusicXmlSpannerMap &spanners) {
    //LOGD("handleSpannerStop(sp %p, track2 %d, tick %s (%d))", cur_sp, track2, muPrintable(tick.print()), tick.ticks());
    if (!cur_sp) {
        return;
    }

    cur_sp->setTrack2(track2);
    spanners[cur_sp].second = tick.ticks();
}

//---------------------------------------------------------
//   The MusicXML parser, pass 2
//---------------------------------------------------------

//---------------------------------------------------------
//   MusicXmlParserPass2
//---------------------------------------------------------

MusicXmlParserPass2::MusicXmlParserPass2(Score *score, MusicXmlParserPass1 &pass1, MusicXmlLogger *logger)
    : divs_(0), score_(score), pass1_(pass1), logger_(logger) {
    // nothing
}

//---------------------------------------------------------
//   addError
//---------------------------------------------------------

void MusicXmlParserPass2::addError(const String &error) {
    if (!error.empty()) {
        logger_->logError(error, &e_);
        errors_ += errorStringWithLocation(e_.byteOffset(), error) + u'\n';
    }
}

//---------------------------------------------------------
//   setChordRestDuration
//---------------------------------------------------------

/**
 * Set \a cr duration
 */

static void setChordRestDuration(ChordRest *cr, TDuration duration, const Fraction dura) {
    if (duration.isMeasure()) {
        cr->setDurationType(duration);
        cr->setTicks(dura);
    } else {
        cr->setDurationType(duration);
        cr->setTicks(cr->durationType().fraction());
    }
}

//---------------------------------------------------------
//   addRest
//---------------------------------------------------------

/**
 * Add a rest to the score
 * TODO: display step handling
 */

static Rest *addRest(Score *, Measure *m, const Fraction &tick, const track_idx_t track, const int move, const TDuration duration,
                     const Fraction dura) {
    Segment *s = m->getSegment(SegmentType::ChordRest, tick);
    // Sibelius might export two rests at the same place, ignore the 2nd one
    // <?DoletSibelius Two NoteRests in same voice at same position may be an error?>
    // Same issue may result from trying to import incomplete tuplets
    if (s->element(track)) {
        LOGD("cannot add rest at tick %s (%d) track %zu: element already present", muPrintable(tick.toString()), tick.ticks(),
             track); // TODO
        return nullptr;
    }

    Rest *cr = Factory::createRest(s);
    setChordRestDuration(cr, duration, dura);
    cr->setTrack(track);
    cr->setStaffMove(move);
    s->add(cr);
    return cr;
}

//---------------------------------------------------------
//   resetTuplets
//---------------------------------------------------------

static void resetTuplets(Tuplets &tuplets) {
    for (auto &pair : tuplets) {
        Tuplet *tuplet = pair.second;
        if (tuplet) {
            const Fraction actualDuration = tuplet->elementsDuration() / tuplet->ratio();
            const Fraction missingDuration = MusicXmlTupletState::missingTupletDuration(actualDuration);
            LOGD("tuplet %p not stopped at end of measure, tick %s duration %s missing %s", tuplet, muPrintable(tuplet->tick().toString()),
                 muPrintable(actualDuration.toString()), muPrintable(missingDuration.toString()));
            if (actualDuration > Fraction(0, 1) && missingDuration > Fraction(0, 1)) {
                LOGD("add missing %s to previous tuplet", muPrintable(missingDuration.toString()));
                const DurationElement *firstElement = tuplet->elements().at(0);
                // appended the rest to the current end of the tuplet (firstElement->tick() + actualDuration)
                Rest *const extraRest = addRest(firstElement->score(), firstElement->measure(), firstElement->tick() + actualDuration,
                                                firstElement->track(), 0, TDuration{missingDuration * tuplet->ratio()}, missingDuration);
                if (extraRest) {
                    extraRest->setTuplet(tuplet);
                    tuplet->add(extraRest);
                }
            }
            const int normalNotes = tuplet->ratio().denominator();
            handleTupletStop(tuplet, normalNotes);
        }
    }
}

//---------------------------------------------------------
//   initPartState
//---------------------------------------------------------

/**
 Initialize members as required for reading the MusicXML part element.
 TODO: factor out part reading into a separate class
 TODO: preferably use automatically initialized variables
 */

void MusicXmlParserPass2::initPartState(const String &partId) {
    UNUSED(partId);
    timeSigDura_ = Fraction(0, 0); // invalid
    ties_.clear();
    unstartedTieNotes_.clear();
    unendedTieNotes_.clear();
    lastVolta_ = 0;
    hasDrumset_ = false;
    for (int i = 0; i < MAX_NUMBER_LEVEL; ++i) {
        slurs_[i] = SlurDesc();
    }
    for (int i = 0; i < MAX_NUMBER_LEVEL; ++i) {
        trills_[i] = 0;
    }
    for (auto &i : glissandi_) {
        i[0] = i[1] = 0;
    }
    pedalContinue_ = 0;
    harmony_ = 0;
    tremStart_ = 0;
    figBass_ = 0;
    delayedOttava_ = 0;
    multiMeasureRestCount_ = -1;
    measureStyleSlash_ = MusicXmlSlash::NONE;
    extendedLyrics_.init();
    graceNoteLyrics_.clear();
    inferredHairpins_.clear();
    inferredTempoLines_.clear();
    inferredPerc_.clear();

    nstaves_ = pass1_.getPart(partId)->nstaves();
    measureRepeatNumMeasures_.assign(nstaves_, 0);
    measureRepeatCount_.assign(nstaves_, 0);
}

//---------------------------------------------------------
//   findIncompleteSpannersInStack
//---------------------------------------------------------

static void findIncompleteSpannersInStack(const String &spannerType, SpannerStack &stack, SpannerSet &res) {
    for (MusicXmlExtendedSpannerDesc &desc : stack) {
        if (desc.sp) {
            LOGD("%s not terminated at end of part", muPrintable(spannerType));
            res.insert(desc.sp);
            desc = {};
        }
    }
}

//---------------------------------------------------------
//   findIncompleteSpannersAtPartEnd
//---------------------------------------------------------

SpannerSet MusicXmlParserPass2::findIncompleteSpannersAtPartEnd() {
    SpannerSet res;
    findIncompleteSpannersInStack(u"bracket", brackets_, res);
    findIncompleteSpannersInStack(u"wedge", hairpins_, res);
    findIncompleteSpannersInStack(u"octave-shift", ottavas_, res);
    if (pedal_.sp) {
        LOGD("pedal not terminated at end of part");
        res.insert(pedal_.sp);
        pedal_ = {};
    }
    return res;
}

//---------------------------------------------------------
//   cleanupUnterminatedTie
//---------------------------------------------------------
/**
 Delete tie and add Laissez Vibrer where it was
 */

static void cleanupUnterminatedTie(Tie *tie, const Score *score, bool fixForCrossStaff = false) {
    Note *unterminatedTieNote = tie->startNote();
    const Chord *unterminatedChord = unterminatedTieNote->chord();

    // Dolet 6 doesn't export cross staff information
    // If a tie is unterminated, try to find a candidate to tie it to on a different track/stave
    if (fixForCrossStaff) {
        const Segment *nextSeg = score->tick2leftSegment(unterminatedChord->tick() + unterminatedChord->ticks());
        if (nextSeg) {
            const Part *part = unterminatedTieNote->part();
            const TrackRange trackRange = part->trackRange();
            for (track_idx_t track = trackRange.startTrack; track <= trackRange.endTrack; ++track) {
                const EngravingItem *el = nextSeg->element(track);
                if (el && el->isChord()) {
                    Note *matchingNote = toChord(el)->findNote(unterminatedTieNote->pitch());
                    if (matchingNote && matchingNote->tpc() == unterminatedTieNote->tpc()) {
                        tie->setEndNote(matchingNote);
                        matchingNote->setTieBack(tie);
                        return;
                    }
                }
            }
        }
    }

    // Delete unterminated ties pending fully featured l.v. ties & ties over repeats
    unterminatedTieNote->remove(tie);
    delete tie;

    // Add Laissez Vibrer instead
    LaissezVib *lvTie = Factory::createLaissezVib(unterminatedTieNote);
    unterminatedTieNote->score()->undoAddElement(lvTie);
}

//---------------------------------------------------------
//   isLikelyIncorrectPartName
//---------------------------------------------------------
/**
 Sibelius exports part names of the form "P#" rather than
 specifying print-object="no". This finds those.
 */

static bool isLikelyIncorrectPartName(const String &partName) {
    static const std::wregex re(L"^P[0-9]+$");
    return partName.contains(re);
}

//---------------------------------------------------------
// multi-measure rest state handling
//---------------------------------------------------------

// If any multi-measure rest is found, the "create multi-measure rest" style setting is enabled.
// First measure in a multi-measure rest gets setBreakMultiMeasureRest(true), then count down
// the remaining number of measures.
// The first measure after a multi-measure rest gets setBreakMultiMeasureRest(true).
// For all other measures breakMultiMeasureRest is unchanged (stays default (false)).

//---------------------------------------------------------
//   setMultiMeasureRestCount
//---------------------------------------------------------

/**
 Set the multi-measure rest counter.
 */

void MusicXmlParserPass2::setMultiMeasureRestCount(int count) { multiMeasureRestCount_ = count; }

//---------------------------------------------------------
//   getAndDecMultiMeasureRestCount
//---------------------------------------------------------

/**
 Return current multi-measure rest counter.
 Decrement counter if possible (not beyond -1).
 */

int MusicXmlParserPass2::getAndDecMultiMeasureRestCount() {
    int res = multiMeasureRestCount_;
    if (multiMeasureRestCount_ >= 0) {
        multiMeasureRestCount_--;
    }
    return res;
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserDirection::skipLogCurrElem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserPass2::skipLogCurrElem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Parse MusicXML in \a device and extract pass 2 data.
 */

Err MusicXmlParserPass2::parse(const ByteArray &data) {
    //LOGD("MusicXmlParserPass2::parse()");
    e_.setData(data);
    Err res = parse();
    //LOGD("MusicXmlParserPass2::parse() res %d", int(res));
    return res;
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Start the parsing process, after verifying the top-level node is score-partwise
 */

Err MusicXmlParserPass2::parse() {
    bool found = false;
    while (e_.readNextStartElement()) {
        if (e_.name() == "score-partwise") {
            found = true;
            scorePartwise();
        } else {
            logger_->logError(u"this is not a MusicXML score-partwise file", &e_);
            e_.skipCurrentElement();
            return Err::FileBadFormat;
        }
    }

    if (!found) {
        logger_->logError(u"this is not a MusicXML score-partwise file", &e_);
        return Err::FileBadFormat;
    }

    return Err::NoError;
}

//---------------------------------------------------------
//   createBarline
//---------------------------------------------------------

/*
 * Create a barline of the specified type.
 */

static std::unique_ptr<BarLine> createBarline(const Score *score, const track_idx_t track, const BarLineType type, const bool visible,
                                              const String &barStyle, const bool spanStaff) {
    std::unique_ptr<BarLine> barline(Factory::createBarLine(score->dummy()));
    barline->setTrack(track);
    barline->setBarLineType(type);
    barline->setSpanStaff(spanStaff);
    barline->setVisible(visible);
    if (barStyle == u"tick") {
        barline->setSpanFrom(BARLINE_SPAN_TICK1_FROM);
        barline->setSpanTo(BARLINE_SPAN_TICK1_TO);
    } else if (barStyle == u"short") {
        barline->setSpanFrom(BARLINE_SPAN_SHORT1_FROM);
        barline->setSpanTo(BARLINE_SPAN_SHORT1_TO);
    }
    return barline;
}

//---------------------------------------------------------
//   addBarlineToMeasure
//---------------------------------------------------------

/*
 * Add barline to the measure at tick.
 */

static void addBarlineToMeasure(Measure *measure, const Fraction tick, std::unique_ptr<BarLine> barline) {
    SegmentType st = SegmentType::BarLine;
    if (tick == measure->endTick()) {
        st = SegmentType::EndBarLine;
    } else if (tick == measure->tick()) {
        st = SegmentType::BeginBarLine;
    }
    Segment *const segment = measure->getSegment(st, tick);
    segment->add(barline.release());
}

//---------------------------------------------------------
//   scorePartwise
//---------------------------------------------------------

/**
 Parse the MusicXML top-level (XPath /score-partwise) node.
 */

void MusicXmlParserPass2::scorePartwise() {
    while (e_.readNextStartElement()) {
        if (e_.name() == "part") {
            part();
        } else if (e_.name() == "part-list") {
            partList();
        } else {
            skipLogCurrElem();
        }
    }
    // set last measure barline to normal or MuseScore will generate light-heavy EndBarline
    // this creates non-generated barlines spanning only the current instrument
    // BarLine::_spanStaff is set using the default in Staff::_barLineSpan
    Measure *const lm = score_->lastMeasure();
    if (lm && lm->endBarLineType() & BarLineType::NORMAL) {
        for (staff_idx_t staffidx = 0; staffidx < score_->nstaves(); ++staffidx) {
            const Staff *staff = score_->staff(staffidx);
            auto b = createBarline(score_, staffidx * VOICES, BarLineType::NORMAL, true, u"", staff->barLineSpan());
            addBarlineToMeasure(lm, lm->endTick(), std::move(b));
        }
    }
    addError(checkAtEndElement(e_, u"score-partwise"));

    for (const auto &[_, sysEl] : sysElements_) {
        score_->undoAddElement(sysEl);

        // Remove potential duplicated text for text and text lines
        const bool elIsText = (sysEl->isTextBase() || sysEl->isTextLineBase()) && !sysEl->isMarker() && !sysEl->isJump();
        if (!elIsText) {
            continue;
        }
        const Score *score = sysEl->score();
        const Segment *seg = score ? score->tick2segment(sysEl->tick(), false, SegmentType::ChordRest) : nullptr;
        if (!seg) {
            continue;
        }
        const String sysElText = sysEl->isTextBase() ? toTextBase(sysEl)->plainText() : toTextLineBase(sysEl)->beginText();
        std::vector<EngravingItem *> annotations = seg->annotations();
        for (EngravingItem *existingEl : annotations) {
            const bool bothText = (existingEl->isTextBase() || existingEl->isTextLineBase()) && elIsText;
            if (existingEl && existingEl != sysEl && bothText) {
                const String existingText =
                    existingEl->isTextBase() ? toTextBase(existingEl)->plainText() : toTextLineBase(existingEl)->beginText();
                const bool textMatches = existingText == sysElText;
                const bool placementMatches = existingEl->placement() == sysEl->placement();
                const bool staffMatches = existingEl->staffIdx() == sysEl->staffIdx();
                if (textMatches && (!existingEl->systemFlag() || staffMatches) && placementMatches) {
                    score_->removeElement(existingEl);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   partList
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list node.
 */

void MusicXmlParserPass2::partList() {
    while (e_.readNextStartElement()) {
        if (e_.name() == "score-part") {
            scorePart();
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   scorePart
//---------------------------------------------------------

// Parse the /score-partwise/part-list/score-part node.
// TODO: nothing required for pass 2 ?

void MusicXmlParserPass2::scorePart() {
    while (e_.readNextStartElement()) {
        if (e_.name() == "midi-instrument") {
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "score-instrument") {
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "part-name") {
            e_.skipCurrentElement(); // skip but don't log
        } else {
            skipLogCurrElem();
        }
    }
}

static void createTimeTick(const Score *score, const Fraction &tick, const staff_idx_t staffIdx) {
    Measure *const measure = score->tick2measure(tick);
    if (!measure->findSegment(SegmentType::TimeTick | SegmentType::ChordRest, tick)) {
        EditTimeTickAnchors::createTimeTickAnchor(measure, tick - measure->tick(), staffIdx);
    }
}

static String replacePartNameAccidentals(const String &partName) {
    String name = partName;
    static const std::wregex re(L"((^|\\s|\\u00A0)[ABCDEF][b#\\u00AF]($|\\s|\\u00A0))");
    StringList res = name.search(re, {1}, SplitBehavior::SkipEmptyParts);
    if (!res.empty()) {
        String transp = res.at(0).replace(u"b", u"♭").replace(u"¯", u"♭").replace(u"#", u"♯");
        name.replace(re, transp);
        return name;
    }
    return partName;
}

//---------------------------------------------------------
//   part
//---------------------------------------------------------

/**
 Parse the /score-partwise/part node.
 */

void MusicXmlParserPass2::part() {
    const String id = e_.attribute("id");

    if (!pass1_.hasPart(id)) {
        logger_->logError(String(u"MusicXmlParserPass2::part cannot find part '%1'").arg(id), &e_);
        skipLogCurrElem();
        return;
    }

    initPartState(id);

    const MusicXmlInstruments &instruments = pass1_.getInstruments(id);
    hasDrumset_ = hasDrumset(instruments);

    // set the part name
    Part *part = pass1_.getPart(id);
    MusicXmlPart mxmlPart = pass1_.getMusicXmlPart(id);
    String partName = mxmlPart.getName();
    setPartInstruments(logger_, &e_, part, id, score_, pass1_.getInstrList(id), pass1_.getIntervals(id), instruments, partName);
    partName = replacePartNameAccidentals(partName);
    const bool inconsistentVisibility = mxmlPart.getPrintName() != mxmlPart.getPrintAbbr();
    if (!isLikelyIncorrectPartName(partName)) {
        part->setLongNameAll(partName);
    } else {
        part->setLongNameAll(u"");
    }
    if (!mxmlPart.getAbbr().empty()) {
        part->setPlainShortNameAll(mxmlPart.getAbbr());
        if (inconsistentVisibility) {
            // TODO: improve logic to set visibility of part names
            part->setLongNameAll(u"");
        }
    } else {
        part->setPlainShortNameAll(u"");
    }

    if (pass1_.nparts() == 1 && mxmlPart.getPrintName() && mxmlPart.getPrintAbbr()) {
        score_->style().set(Sid::hideInstrumentNameIfOneInstrument, false);
    }

#ifdef DEBUG_VOICE_MAPPER
    VoiceList voicelist = _pass1.getVoiceList(id);
    // debug: print voice mapper contents
    LOGD("voiceMapperStats: part '%s'", muPrintable(id));
    for (std::map<String, mu::engraving::VoiceDesc>::const_iterator i = voicelist.cbegin(); i != voicelist.cend(); ++i) {
        LOGD("voiceMapperStats: voice %s staff data %s", muPrintable(i.key()), muPrintable(i.value().toString()));
    }
#endif

    // read the measures
    size_t nr = 0;      // current measure sequence number (always increments by one for each measure)
    measureNumber_ = 0; // written measure number (doesn't always increment by 1)
    while (e_.readNextStartElement()) {
        if (e_.name() == "measure") {
            Fraction t = pass1_.getMeasureStart(nr);
            if (t.isValid()) {
                measure(id, t);
            } else {
                logger_->logError(String(u"no valid start time for measure %1").arg(nr + 1), &e_);
                e_.skipCurrentElement();
            }
            ++nr;
        } else {
            skipLogCurrElem();
        }
    }

    // stop all remaining extends for this part and add remaining ottava if present
    Measure *lm = part->score()->lastMeasure();
    if (lm) {
        track_idx_t strack = pass1_.trackForPart(id);
        track_idx_t etrack = strack + part->nstaves() * VOICES;
        Fraction lastTick = lm->endTick();
        for (track_idx_t trk = strack; trk < etrack; trk++) {
            extendedLyrics_.setExtend(-1, trk, lastTick);
        }
        if (delayedOttava_ && delayedOttava_->tick2() < lastTick) {
            handleSpannerStop(delayedOttava_, delayedOttava_->track2(), lastTick, spanners_);
            delayedOttava_ = nullptr;
        }
    }

    if (configuration()->inferTextType()) {
        for (Hairpin *hp : inferredHairpins_) {
            hp->score()->addElement(hp);
        }
        for (GradualTempoChange *line : inferredTempoLines_) {
            addSystemElement(line, line->tick());
        }
    }

    const SpannerSet incompleteSpanners = findIncompleteSpannersAtPartEnd();
    //LOGD("spanner list:");
    auto i = spanners_.cbegin();
    while (i != spanners_.cend()) {
        SLine *const sp = i->first;
        Fraction tick1 = Fraction::fromTicks(i->second.first);
        Fraction tick2 = Fraction::fromTicks(i->second.second);
        //LOGD("spanner %p tp %d isHairpin %d tick1 %s tick2 %s track1 %d track2 %d start %p end %p",
        //       sp, sp->type(), sp->isHairpin(), muPrintable(tick1.toString()), muPrintable(tick2.toString()),
        //       sp->track(), sp->track2(), sp->startElement(), sp->endElement());
        if (incompleteSpanners.find(sp) == incompleteSpanners.end()) {
            // complete spanner found
            // Create time tick for hairpin
            if (sp->isHairpin()) {
                createTimeTick(score_, tick1, track2staff(sp->track()));
            }
            sp->setTick(tick1);
            sp->setTick2(tick2);
            sp->score()->addElement(sp);
        } else {
            // incomplete spanner -> cleanup
            delete sp;
        }
        ++i;
    }
    spanners_.clear();

    // Clean up any remaining ties
    MusicXmlTieMap tieMap = ties_;
    for (auto &tie : tieMap) {
        if (tie.second) {
            unendedTieNotes_.push_back(tie.second->startNote());
            ties_.erase(tie.first);
        }
    }
    // Find ties between different voices which may have been missed
    for (Note *startNote : unendedTieNotes_) {
        Tie *unendedTie = startNote->tieFor();
        if (!unendedTie) {
            continue;
        }
        const Chord *startChord = startNote->chord();
        const Measure *startMeasure = startChord ? startChord->measure() : nullptr;
        for (Note *endNote : unstartedTieNotes_) {
            if (endNote->tieBack()) {
                continue;
            }
            const Chord *endChord = endNote->chord();
            if (startNote->pitch() == endNote->pitch() && startChord->tick() < endChord->tick() &&
                (startMeasure == endChord->measure() || startChord->tick() + startChord->actualTicks() == endChord->tick())) {
                unendedTie->setEndNote(endNote);
                endNote->setTieBack(unendedTie);
            }
        }

        if (!unendedTie->endNote()) {
            // Tie started with no matching end tags
            // Find a note of the same pitch in the same voice immediately following the start chord
            Segment *nextSeg = startChord ? startChord->segment() : nullptr;
            while (nextSeg && nextSeg->tick() < startChord->tick() + startChord->ticks()) {
                nextSeg = nextSeg->nextCR(startChord->track(), true);
            }
            EngravingItem *nextEl = nextSeg ? nextSeg->element(startNote->track()) : nullptr;
            Chord *nextChord = nextEl && nextEl->isChord() ? toChord(nextEl) : nullptr;
            Note *matchingNote = nextChord ? nextChord->findNote(startNote->pitch()) : nullptr;

            if (matchingNote && matchingNote->tpc() == startNote->tpc() && matchingNote != startNote) {
                unendedTie->setEndNote(matchingNote);
                matchingNote->setTieBack(unendedTie);
            } else if (startChord) {
                // try other voices in the stave
                const Part *p = startChord->part();
                const TrackRange trackRange = p->trackRange();
                for (track_idx_t track = trackRange.startTrack; track < trackRange.endTrack + VOICES; track++) {
                    nextEl = nextSeg ? nextSeg->element(track) : nullptr;
                    nextChord = nextEl && nextEl->isChord() ? toChord(nextEl) : nullptr;
                    if (nextChord && nextChord->vStaffIdx() != startChord->vStaffIdx()) {
                        continue;
                    }
                    matchingNote = nextChord ? nextChord->findNote(startNote->pitch()) : nullptr;
                    if (matchingNote && matchingNote->tpc() == startNote->tpc() && matchingNote != startNote) {
                        unendedTie->setEndNote(matchingNote);
                        matchingNote->setTieBack(unendedTie);
                    }
                }
            }
        }

        if (!unendedTie->endNote()) {
            cleanupUnterminatedTie(unendedTie, score_, pass1_.exporterSoftware() == MusicXmlExporterSoftware::DOLET6);
        }
    }

    ties_.clear();
    unstartedTieNotes_.clear();
    unendedTieNotes_.clear();

    if (hasDrumset_) {
        Drumset *drumset = new Drumset;
        const MusicXmlInstruments &instrumentsAfterPass2 = pass1_.getInstruments(id);
        initDrumset(drumset, instrumentsAfterPass2);
        // set staff type to percussion if incorrectly imported as pitched staff
        // Note: part has been read, staff type already set based on clef type and staff-details
        // but may be incorrect for a percussion staff that does not use a percussion clef
        setStaffTypePercussion(part, drumset);
    }

    bool showPart = false;
    for (Staff *staff : part->staves()) {
        if (staff->visible()) {
            showPart = true;
            break;
        }
    }

    part->setShow(showPart);

    addError(checkAtEndElement(e_, u"part"));
}

//---------------------------------------------------------
//   findMeasure
//---------------------------------------------------------

/**
 In Score \a score find the measure starting at \a tick.
 */

static Measure *findMeasure(const Score *score, const Fraction &tick) {
    for (Measure *m = score->firstMeasure(); m; m = m->nextMeasure()) {
        if (m->tick() == tick) {
            return m;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   removeBeam
//---------------------------------------------------------

/**
 Set beam mode for all elements and remove the beam
 */

static void removeBeam(Beam *&beam) {
    for (size_t i = 0; i < beam->elements().size(); ++i) {
        beam->elements().at(i)->setBeamMode(BeamMode::NONE);
    }
    delete beam;
    beam = nullptr;
}

//---------------------------------------------------------
//   handleBeamAndStemDir
//---------------------------------------------------------

static void handleBeamAndStemDir(ChordRest *cr, const BeamMode bm, const DirectionV sd, Beam *&beam, bool hasBeamingInfo, Color beamColor,
                                 const String fan) {
    if (!cr) {
        return;
    }
    // create a new beam
    if (bm == BeamMode::BEGIN) {
        // if currently in a beam, delete it
        if (beam) {
            LOGD("handleBeamAndStemDir() new beam, removing previous incomplete beam %p", beam);
            removeBeam(beam);
        }
        // create a new beam
        beam = Factory::createBeam(cr->score());
        beam->setTrack(cr->track());
        beam->setDirection(sd);
        colorItem(beam, beamColor);
        if (!fan.empty() && fan != u"none") {
            beam->setAsFeathered(fan == u"rit");
        }
    }
    // add ChordRest to beam
    if (beam) {
        // verify still in the same track (if still in the same voice)
        // and in a beam ...
        // (note no check is done on correct order of beam begin/continue/end)
        // TODO: Some BEGINs are being skipped
        if (cr->track() != beam->track()) {
            LOGD("handleBeamAndStemDir() from track %zu to track %zu -> abort beam", beam->track(), cr->track());
            // reset beam mode for all elements and remove the beam
            removeBeam(beam);
        } else if (bm == BeamMode::NONE) {
            LOGD("handleBeamAndStemDir() in beam, bm BeamMode::NONE -> abort beam");
            // reset beam mode for all elements and remove the beam
            removeBeam(beam);
        } else if (!(bm == BeamMode::BEGIN || bm == BeamMode::MID || bm == BeamMode::END || bm == BeamMode::BEGIN16 ||
                     bm == BeamMode::BEGIN32)) {
            LOGD("handleBeamAndStemDir() in beam, bm %d -> abort beam", static_cast<int>(bm));
            // reset beam mode for all elements and remove the beam
            removeBeam(beam);
        } else {
            // actually add cr to the beam
            beam->add(cr);
        }
    }
    // if no beam, set stem direction on chord itself
    if (!beam) {
        toChord(cr)->setStemDirection(sd);
        // set beam to none if score has beaming information and note can get beam, otherwise
        // set to auto
        bool canGetBeam = (cr->durationType().type() >= DurationType::V_EIGHTH && cr->durationType().type() <= DurationType::V_1024TH);
        if (hasBeamingInfo && canGetBeam) {
            cr->setBeamMode(BeamMode::NONE);
        } else {
            cr->setBeamMode(BeamMode::AUTO);
        }
    }
    // terminate the current beam and add to the score
    if (beam && bm == BeamMode::END) {
        beam = nullptr;
    }
}

//---------------------------------------------------------
//   markUserAccidentals
//---------------------------------------------------------

/**
 Check for "superfluous" accidentals to mark them as USER accidentals.
 The candidate map alterMap is ordered on note address. Check it here segment after segment.
 */

static void markUserAccidentals(const staff_idx_t firstStaff, const size_t staves, const Key key, const Measure *measure,
                                const std::map<Note *, int> &alterMap) {
    std::map<int, bool> accTmp;

    AccidentalState currAcc;
    currAcc.init(key);
    SegmentType st = SegmentType::ChordRest;
    for (mu::engraving::Segment *segment = measure->first(st); segment; segment = segment->next(st)) {
        for (track_idx_t track = 0; track < staves * VOICES; ++track) {
            EngravingItem *e = segment->element(firstStaff * VOICES + track);
            if (!e || !e->isChord()) {
                continue;
            }
            Chord *chord = toChord(e);
            for (Note *nt : chord->notes()) {
                if (muse::contains(alterMap, nt)) {
                    int alter = alterMap.at(nt);
                    int ln = absStep(nt->tpc(), nt->pitch());
                    bool error = false;
                    AccidentalVal currAccVal = currAcc.accidentalVal(ln, error);
                    if (error) {
                        continue;
                    }
                    if ((alter == -1 && currAccVal == AccidentalVal::FLAT && nt->accidental()->accidentalType() == AccidentalType::FLAT &&
                         !muse::value(accTmp, ln, false)) ||
                        (alter == 0 && currAccVal == AccidentalVal::NATURAL &&
                         nt->accidental()->accidentalType() == AccidentalType::NATURAL && !muse::value(accTmp, ln, false)) ||
                        (alter == 1 && currAccVal == AccidentalVal::SHARP && nt->accidental()->accidentalType() == AccidentalType::SHARP &&
                         !muse::value(accTmp, ln, false))) {
                        nt->accidental()->setRole(AccidentalRole::USER);
                    } else if (Accidental::isMicrotonal(nt->accidental()->accidentalType()) &&
                               nt->accidental()->accidentalType() < AccidentalType::END) {
                        // microtonal accidental
                        nt->accidental()->setRole(AccidentalRole::USER);
                        accTmp.insert({ln, false});
                    } else {
                        accTmp.insert({ln, true});
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//   coerceGraceCue
//---------------------------------------------------------

/**
 If the mainChord is small and/or silent, the grace note should likely
 match this. Exporters tend to incorrectly omit <cue> or <type size="cue">
 from grace notes.
 */

static void coerceGraceCue(Chord *mainChord, Chord *graceChord) {
    if (mainChord->isSmall()) {
        graceChord->setSmall(true);
    }
    bool anyPlays = false;
    for (const Note *n : mainChord->notes()) {
        anyPlays |= n->play();
    }
    if (!anyPlays) {
        for (Note *gn : graceChord->notes()) {
            gn->setPlay(false);
        }
    }
}

//---------------------------------------------------------
//   addGraceChordsAfter
//---------------------------------------------------------

/**
 Move \a gac grace chords from grace chord list \a gcl
 to the chord \a c grace note after list
 */

static void addGraceChordsAfter(Chord *c, GraceChordList &gcl, size_t &gac) {
    if (!c) {
        return;
    }

    while (gac > 0) {
        if (gcl.size() > 0) {
            Chord *graceChord = muse::takeFirst(gcl);
            std::vector<EngravingItem *> el = graceChord->el(); // copy, because modified during loop
            for (EngravingItem *e : el) {
                if (e->isFermata()) {
                    e->setOwnershipParent(c->segment());
                    c->segment()->add(e);
                    graceChord->removeFermata(toFermata(e));
                }
            }
            graceChord->toGraceAfter();
            c->add(graceChord); // TODO check if same voice ?
            coerceGraceCue(c, graceChord);
            LOGD("addGraceChordsAfter chord %p grace after chord %p", c, graceChord);
        }
        gac--;
    }
}

//---------------------------------------------------------
//   addGraceChordsBefore
//---------------------------------------------------------

/**
 Move grace chords from grace chord list \a gcl
 to the chord \a c grace note before list
 */

static void addGraceChordsBefore(Chord *c, GraceChordList &gcl) {
    for (int i = static_cast<int>(gcl.size()) - 1; i >= 0; i--) {
        Chord *gc = gcl.at(i);
        std::vector<EngravingItem *> el = gc->el(); // copy, because modified during loop
        for (EngravingItem *e : el) {
            if (e->isFermata()) {
                e->setOwnershipParent(c->segment());
                c->segment()->add(e);
                gc->removeFermata(toFermata(e));
            }
        }
        c->add(gc); // TODO check if same voice ?
        coerceGraceCue(c, gc);
    }
    gcl.clear();
}

static bool canAddTempoText(const Score *const score, const int tick) {
    const Measure *measure = score->tick2measure(Fraction::fromTicks(tick));
    const Segment *segment = measure ? measure->findSegment(SegmentType::ChordRest, Fraction::fromTicks(tick)) : nullptr;
    if (!segment) {
        return true;
    }

    for (const EngravingItem *e : segment->annotations()) {
        if (e->isTempoText() && toTempoText(e)->tempo() != Constants::DEFAULT_TEMPO) {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure node.
 */

void MusicXmlParserPass2::measure(const String &partId, const Fraction time) {
    // "measure numbers" don't have to be actual numbers in MusicXML
    bool isNumericMeasureNumber = false;
    int parsedMeasureNumber = 0;

    AsciiStringView numberA = e_.asciiAttribute("number");
    if (!numberA.empty()) {
        if (numberA.at(0).ascii() == 'X') {
            isNumericMeasureNumber = false;
        } else {
            parsedMeasureNumber = numberA.toInt(&isNumericMeasureNumber);
        }
    }

    //LOGD("measure %d start", parsedMeasureNumber);

    Measure *measure = findMeasure(score_, time);
    if (!measure) {
        logger_->logError(String(u"measure at tick %1 not found!").arg(time.ticks()), &e_);
        skipLogCurrElem();
        return;
    }

    if (e_.asciiAttribute("implicit") == "yes") {
        // Implicit measure: expect measure number to be unchanged.
        measure->setExcludeFromNumbering(true);
    } else {
        // Normal measure: expect number to have increased by one.
        ++measureNumber_;
    }

    if (isNumericMeasureNumber) {
        // Actual measure number may differ from expected value.
        measure->setMeasureNumberOffset(parsedMeasureNumber - measureNumber_);
        measureNumber_ = parsedMeasureNumber;
    }

    // set measure's RepeatFlag to none because musicXML is allowing single measure repeat and no ordering in repeat start and end barlines
    measure->setRepeatStart(false);
    measure->setRepeatEnd(false);

    /* TODO: for cutaway measures, i believe we can expect the staff to continue to be cutaway until another
    * print-object="yes" attribute is found. Here is the code that does that, though I don't want to actually commit this until
    * we have the exporter dealing with this sort of stuff as well.
    *
    * When print-object="yes" is encountered, the measure will explicitly be set to visible (see MusicXmlParserPass2::staffDetails)

    MeasureBase* prevBase = measure->prev();
    if (prevBase) {
        Part* part = _pass1.getPart(partId);
        staff_idx_t staffIdx = _score->staffIdx(part);
        if (!toMeasure(prevBase)->visible(staffIdx)) {
            measure->setStaffVisible(staffIdx, false);
        }
    } */

    Fraction mTime;                            // current time stamp within measure
    Fraction prevTime;                         // time stamp within measure previous chord
    Chord *prevChord = 0;                      // previous chord
    Fraction mDura;                            // current total measure duration
    GraceChordList gcl;                        // grace chords collected sofar
    size_t gac = 0;                            // grace after count in the grace chord list
    Beams beams;                               // Current beam for each voice in the current part
    String cv = u"1";                          // current voice for chords, default is 1
    FiguredBassList fbl;                       // List of figured bass elements under a single note
    MusicXmlTupletStates tupletStates;         // Tuplet state for each voice in the current part
    Tuplets tuplets;                           // Current tuplet for each voice in the current part
    DelayedDirectionsList delayedDirections;   // Directions to be added to score *after* collecting all and sorting
    InferredFingeringsList inferredFingerings; // Directions to be reinterpreted as Fingerings
    ArpeggioMap arpMap;
    DelayedArpMap delayedArps;
    HarmonyMap delayedHarmony;
    bool measureHasCoda = false;
    String tempoString; // helper for Dorico imports

    // collect candidates for courtesy accidentals to work out at measure end
    std::map<Note *, int> alterMap;

    while (e_.readNextStartElement()) {
        if (e_.name() == "attributes") {
            attributes(partId, measure, time + mTime);
        } else if (e_.name() == "direction") {
            MusicXmlParserDirection dir(e_, score_, pass1_, *this, logger_);
            if (!tempoString.empty() && pass1_.exporterSoftware() == MusicXmlExporterSoftware::DORICO) {
                dir.setBpm(tempoString.toDouble());
                tempoString.clear();
            }
            dir.direction(partId, measure, time + mTime, spanners_, delayedDirections, inferredFingerings, delayedHarmony, measureHasCoda,
                          segnos_);
        } else if (e_.name() == "figured-bass") {
            FiguredBass *fb = figuredBass();
            if (fb) {
                fbl.push_back(fb);
            }
        } else if (e_.name() == "harmony") {
            harmony(partId, measure, time + mTime, delayedHarmony);
        } else if (e_.name() == "note") {
            if (!tempoString.empty()) {
                // sound tempo="..."
                // create an invisible default TempoText
                // to prevent duplicates, only if none is present yet
                Fraction tick = time + mTime;

                if (canAddTempoText(score_, tick.ticks())) {
                    double tpo = tempoString.toDouble() / 60;
                    TempoText *t = Factory::createTempoText(score_->dummy());
                    t->setXmlText(
                        String(u"%1 = %2").arg(TempoText::duration2tempoTextString(TDuration(DurationType::V_QUARTER)), tempoString));
                    t->setVisible(false);
                    t->setTempo(tpo);
                    t->setFollowText(true);

                    addElemOffset(t, pass1_.trackForPart(partId), u"above", measure, tick);
                }
                tempoString.clear();
            }

            // Correct delayed ottava tick
            if (delayedOttava_ && delayedOttava_->tick2() < time + mTime) {
                handleSpannerStop(delayedOttava_, delayedOttava_->track2(), time + mTime, spanners_);
                delayedOttava_ = nullptr;
            }
            Fraction missingPrev;
            Fraction dura;
            Fraction missingCurr;
            int alt = -10; // any number outside range of xml-tag "alter"
            // note: chord and grace note handling done in note()
            // dura > 0 if valid rest or first note of chord found
            Note *n = note(partId, measure, time + mTime, time + prevTime, missingPrev, dura, missingCurr, cv, gcl, gac, beams, fbl, alt,
                           tupletStates, tuplets, arpMap, delayedArps);
            if (n && !n->chord()->isGrace()) {
                prevChord = n->chord(); // remember last non-grace chord
            }
            if (n && n->accidental() && n->accidental()->accidentalType() != AccidentalType::NONE) {
                alterMap.insert({n, alt});
            }
            if (missingPrev.isValid()) {
                mTime += missingPrev;
            }
            if (dura.isValid() && dura > Fraction(0, 1)) {
                prevTime = mTime; // save time stamp last chord created
                mTime += dura;
                if (mTime > mDura) {
                    mDura = mTime;
                }
            }
            if (missingCurr.isValid()) {
                mTime += missingCurr;
            }
            //LOGD("added note %p chord %p gac %d", n, n ? n->chord() : 0, gac);
        } else if (e_.name() == "forward") {
            Fraction dura;
            forward(dura);
            if (dura.isValid()) {
                mTime += dura;
                if (mTime > mDura) {
                    mDura = mTime;
                }
            }
        } else if (e_.name() == "backup") {
            Fraction dura;
            backup(dura);
            if (dura.isValid()) {
                if (dura <= mTime) {
                    mTime -= dura;
                } else {
                    logger_->logError(u"backup beyond measure start", &e_);
                    mTime.set(0, 1);
                }
                // check if the tick position is smaller than the minimum division resolution
                // (possibly caused by rounding errors) and in that case set position to 0
                if (mTime.isNotZero() && (divs_ > 0) && (mTime < Fraction(1, 4 * divs_))) {
                    logger_->logError(u"backup to a fractional tick smaller than the minimum division", &e_);
                    mTime.set(0, 1);
                }
            }
        } else if (e_.name() == "sound") {
            tempoString = e_.attribute("tempo");
            e_.skipCurrentElement();
        } else if (e_.name() == "barline") {
            barline(partId, measure, time + mTime);
        } else if (e_.name() == "print") {
            if (score_->parts()[0] == pass1_.getPart(partId)) {
                // only process for first part
                while (e_.readNextStartElement()) {
                    if (e_.name() == "page-layout") {
                        e_.skipCurrentElement(); // skip but don't log
                    } else if (e_.name() == "system-layout") {
                        e_.skipCurrentElement(); // skip but don't log
                    } else if (e_.name() == "staff-layout") {
                        e_.skipCurrentElement(); // skip but don't log
                    } else if (e_.name() == "measure-layout") {
                        measureLayout(measure);
                    } else {
                        skipLogCurrElem();
                    }
                }
            } else {
                e_.skipCurrentElement();
            }
        } else {
            skipLogCurrElem();
        }

        /*
         LOGD("mTime %s (%s) mDura %s (%s)",
         muPrintable(mTime.print()),
         muPrintable(mTime.reduced().print()),
         muPrintable(mDura.print()),
         muPrintable(mDura.reduced().print()));
         */
        mDura.reduce();
        mTime.reduce();
    }

    // convert remaining grace chords to grace after
    gac = gcl.size();
    addGraceChordsAfter(prevChord, gcl, gac);

    // prevent tuplets from crossing measure boundaries
    resetTuplets(tuplets);

    // fill possible gaps in voice 1
    Part *part = pass1_.getPart(partId); // should not fail, we only get here if the part exists
    fillGapsInFirstVoices(measure, part);

    // Prevent any beams from extending into the next measure
    for (auto &[_, beam] : beams) {
        if (beam) {
            removeBeam(beam);
        }
    }

    // Sort and add inferred fingerings
    std::sort(inferredFingerings.begin(), inferredFingerings.end(),
              // Lambda: sort by absolute value of totalY
              [](const MusicXmlInferredFingering *a, const MusicXmlInferredFingering *b) -> bool {
                  return std::abs(a->totalY()) < std::abs(b->totalY());
              });
    for (MusicXmlInferredFingering *inferredFingering : inferredFingerings) {
        if (!inferredFingering->findAndAddToNotes(measure)) {
            // Could not find notes to add to; print as direction
            delayedDirections.push_back(inferredFingering->toDelayedDirection());
        }
        delete inferredFingering;
    }

    for (auto &delHarmony : delayedHarmony) {
        HarmonyDesc harmonyDesc = delHarmony.second;
        Fraction tick = Fraction::fromTicks(delHarmony.first);
        if (FretDiagram *fd = harmonyDesc.fretDiagram_) {
            fd->setTrack(harmonyDesc.track_);
            Segment *s = measure->getSegment(SegmentType::ChordRest, tick);
            if (Harmony *harmony = harmonyDesc.harmony_) {
                harmony->setProperty(Pid::ALIGN, Align(AlignH::HCENTER, AlignV::TOP));
                harmony->setTrack(harmonyDesc.track_);
                fd->add(harmony);
            }
            s->add(fd);
        } else if (Harmony *harmony = harmonyDesc.harmony_) {
            harmony->setTrack(harmonyDesc.track_);
            Segment *s = measure->getSegment(SegmentType::ChordRest, tick);
            s->add(harmony);
        }
    }

    // Sort and add delayed directions
    std::sort(delayedDirections.begin(), delayedDirections.end(),
              // Lambda: sort by absolute value of totalY
              [](const MusicXmlDelayedDirectionElement *a, const MusicXmlDelayedDirectionElement *b) -> bool {
                  return std::abs(a->totalY()) < std::abs(b->totalY());
              });
    for (MusicXmlDelayedDirectionElement *direction : delayedDirections) {
        addElemOffset(direction->element(), direction->track(), direction->placement(), direction->measure(), direction->tick());
    }

    // TODO:
    // - how to handle _timeSigDura.isZero (shouldn't happen ?)
    // - how to handle unmetered music
    if (timeSigDura_.isValid() && !timeSigDura_.isZero()) {
        measure->setTimesig(timeSigDura_);
    }

    // mark superfluous accidentals as user accidentals
    const staff_idx_t scoreRelStaff = score_->staffIdx(part);
    const Key key = score_->staff(scoreRelStaff)->keySigEvent(time).key();
    markUserAccidentals(scoreRelStaff, part->nstaves(), key, measure, alterMap);

    // multi-measure rest handling
    if (getAndDecMultiMeasureRestCount() == 0) {
        // measure is first measure after a multi-measure rest
        measure->setBreakMultiMeasureRest(true);
    }

    setMeasureRepeats(scoreRelStaff, measure);

    addError(checkAtEndElement(e_, u"measure"));
}

//---------------------------------------------------------
//   setMeasureRepeats
//---------------------------------------------------------

/**
 Measure repeat handling, based on values set in measureStyle().
 */

void MusicXmlParserPass2::setMeasureRepeats(const staff_idx_t scoreRelStaff, Measure *measure) {
    for (staff_idx_t i = 0; i < nstaves_; ++i) {
        staff_idx_t staffIdx = scoreRelStaff + i;
        track_idx_t track = staff2track(staffIdx);
        if (measureRepeatNumMeasures_[i]) {
            // delete anything already added to measure
            score_->makeGap(measure->first(SegmentType::ChordRest), track, measure->stretchedLen(score_->staff(staffIdx)), 0);

            if (measureRepeatCount_[i] == measureRepeatNumMeasures_[i]) {
                // starting a new one, not continuing a multi-measure group
                measureRepeatCount_[i] = 1;
            } else {
                // continue building measure repeat group
                ++measureRepeatCount_[i];
            }

            if (((measureRepeatNumMeasures_[i] % 2) && (measureRepeatCount_[i] - 1 == measureRepeatNumMeasures_[i] / 2)) ||
                (!(measureRepeatNumMeasures_[i] % 2) && (measureRepeatCount_[i] == measureRepeatNumMeasures_[i] / 2))) {
                // MeasureRepeat element goes in center measure of group if odd-numbered,
                // or last measure of first half of group if even-numbered
                Transaction &tx = score_->transactionManager()->currentOrDummyTransaction();
                EditMeasureRepeat::addMeasureRepeat(tx, score_, measure->tick(), track, measureRepeatNumMeasures_[i]);
            } else {
                // measures that are part of group but do not contain the element have undisplayed whole rests
                score_->addRest(measure->tick(), track, TDuration(DurationType::V_MEASURE), 0);
            }
        } else {
            // measureStyle() hit a "stop" element most recently
            measureRepeatCount_[i] = 0;
        }
        measure->setMeasureRepeatCount(measureRepeatCount_[i], staffIdx);
    }
}

//---------------------------------------------------------
//   measureLayout
//---------------------------------------------------------

void MusicXmlParserPass2::measureLayout(Measure *measure) {
    while (e_.readNextStartElement()) {
        if (e_.name() == "measure-distance") {
            const Spatium val(e_.readDouble() / 10.0);
            if (!measure->prev() || !measure->prev()->isHBox()) {
                MeasureBase *gap = score_->insertBox(ElementType::HBOX, measure);
                toHBox(gap)->setBoxWidth(val);
            }
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   attributes
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes node.
 */

/* Notes:
 * Number of staves has already been set in pass 1
 * MusicXML order is key, time, clef
 * -> check if it is necessary to insert them in order
 */

void MusicXmlParserPass2::attributes(const String &partId, Measure *measure, const Fraction &tick) {
    while (e_.readNextStartElement()) {
        if (e_.name() == "clef") {
            clef(partId, measure, tick);
        } else if (e_.name() == "divisions") {
            divisions();
        } else if (e_.name() == "key") {
            key(partId, measure, tick);
        } else if (e_.name() == "measure-style") {
            measureStyle(measure);
        } else if (e_.name() == "staff-details") {
            staffDetails(partId, measure);
        } else if (e_.name() == "time") {
            time(partId, measure, tick);
        } else if (e_.name() == "transpose") {
            e_.skipCurrentElement(); // skip but don't log
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   setStaffLines
//---------------------------------------------------------

/**
 Set stafflines and barline span for a single staff
 */

static void setStaffLines(Score *score, staff_idx_t staffIdx, int stafflines) {
    score->staff(staffIdx)->setLines(Fraction(0, 1), stafflines);
    score->staff(staffIdx)->setBarLineTo(0); // default
}

//---------------------------------------------------------
//   staffDetails
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/staff-details node.
 */

void MusicXmlParserPass2::staffDetails(const String &partId, Measure *measure) {
    //logDebugTrace("MusicXmlParserPass2::staffDetails");

    Part *part = pass1_.getPart(partId);
    IF_ASSERT_FAILED(part) { return; }
    size_t staves = part->nstaves();

    String strNumber = e_.attribute("number");
    int n = 0; // default
    if (!strNumber.empty()) {
        n = pass1_.getMusicXmlPart(partId).staffNumberToIndex(strNumber.toInt());
        if (n < 0 || n >= int(staves)) {
            logger_->logError(String(u"invalid staff-details number %1 (may be hidden)").arg(strNumber), &e_);
            n = 0;
        }
    }

    staff_idx_t staffIdx = score_->staffIdx(part) + n;

    StringData stringData;
    AsciiStringView visible = e_.asciiAttribute("print-object");
    AsciiStringView spacing = e_.asciiAttribute("print-spacing");
    if (visible == "no") {
        // EITHER:
        //  1) this indicates an empty staff that is hidden
        //  2) this indicates a cutaway measure. if it is a cutaway measure then print-spacing will be yes
        if (spacing == "yes") {
            measure->setStaffVisible(staffIdx, false);
        } else if (measure && !measure->hasVoices(staffIdx) && measure->isOnlyRests(staffIdx * VOICES)) {
            // measures with print-object="no" are generally exported by exporters such as dolet when empty staves are hidden.
            // for this reason, if we see print-object="no" (and no print-spacing), we can assume that this indicates we should set
            // the hide empty staves style.
            score_->style().set(Sid::hideEmptyStaves, true);
            score_->style().set(Sid::dontHideStavesInFirstSystem, false);
        } else {
            // this doesn't apply to a measure, so we'll assume the entire staff has to be hidden.
            score_->staff(staffIdx)->setVisible(false);
        }
    } else if (visible == "yes" || visible.empty()) {
        if (measure) {
            score_->staff(staffIdx)->setVisible(true);
            measure->setStaffVisible(staffIdx, true);
        }
    } else {
        logger_->logError(String(u"print-object should be \"yes\" or \"no\""));
    }

    int staffLines = 0;
    while (e_.readNextStartElement()) {
        if (e_.name() == "staff-lines") {
            // save staff lines for later
            staffLines = e_.readInt();
            // for a TAB staff also resize the string table and init with zeroes
            if (0 < staffLines) {
                stringData.stringList() = std::vector<instrString>(staffLines);
            } else {
                logger_->logError(String(u"illegal staff-lines %1").arg(staffLines), &e_);
            }
        } else if (e_.name() == "line-detail") {
            const Color color = Color::fromString(e_.attribute("color"));
            if (color.isValid()) {
                score_->staff(staffIdx)->staffType(Fraction(0, 1))->setColor(color);
            }
            if (e_.attribute("print-object") == u"no") {
                score_->staff(staffIdx)->staffType(Fraction(0, 1))->setInvisible(true);
            }
            e_.skipCurrentElement();
        } else if (e_.name() == "staff-tuning") {
            staffTuning(&stringData);
        } else if (e_.name() == "staff-size") {
            const double scaling = e_.doubleAttribute("scaling", 100.0);
            const Spatium val(e_.readDouble() / scaling);
            score_->staff(staffIdx)->setProperty(Pid::MAG, scaling / 100.0);
            score_->staff(staffIdx)->setProperty(Pid::LINE_DISTANCE, val);
        } else {
            skipLogCurrElem();
        }
    }

    if (staffLines > 0) {
        setStaffLines(score_, staffIdx, staffLines);
    }

    Instrument *i = part->instrument();
    if (score_->staff(staffIdx)->isTabStaff(Fraction(0, 1))) {
        if (i->stringData()->frets() == 0) {
            stringData.setFrets(25);
        } else {
            stringData.setFrets(i->stringData()->frets());
        }
        if (stringData.strings() > 0) {
            i->setStringData(stringData);
        }
    } else if (stringData.strings() > 0) {
        logger_->logError(u"trying to change string data for non-TAB staff (not supported)", &e_);
    }
}

//---------------------------------------------------------
//   staffTuning
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/staff-details/staff-tuning node.
 */

void MusicXmlParserPass2::staffTuning(StringData *t) {
    //logDebugTrace("MusicXmlParserPass2::staffTuning");

    // ignore <staff-tuning> if not a TAB staff
    if (!t) {
        logger_->logError(u"<staff-tuning> on non-TAB staff", &e_);
        skipLogCurrElem();
        return;
    }

    int line = e_.intAttribute("line");
    int step = 0;
    int alter = 0;
    int octave = 0;
    while (e_.readNextStartElement()) {
        if (e_.name() == "tuning-alter") {
            alter = e_.readText().trimmed().toInt();
        } else if (e_.name() == "tuning-octave") {
            octave = e_.readInt();
        } else if (e_.name() == "tuning-step") {
            String strStep = e_.readText();
            int pos = static_cast<int>(String(u"CDEFGAB").indexOf(strStep));
            if (strStep.size() == 1 && pos >= 0 && pos < 7) {
                step = pos;
            } else {
                logger_->logError(String(u"invalid step '%1'").arg(strStep), &e_);
            }
        } else {
            skipLogCurrElem();
        }
    }

    if (0 < line && line <= static_cast<int>(t->stringList().size())) {
        int pitch = MusicXmlStepAltOct2Pitch(step, alter, octave);
        if (pitch >= 0) {
            t->stringList()[line - 1].pitch = pitch;
        } else {
            logger_->logError(String(u"invalid string %1 tuning step/alter/oct %2/%3/%4").arg(line).arg(step).arg(alter).arg(octave), &e_);
        }
    }
}

//---------------------------------------------------------
//   measureStyle
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/measure-style node.
 - Initializes the "in multi-measure rest" state
 - Set/reset the "rhythmic/slash notation" state
 */

void MusicXmlParserPass2::measureStyle(Measure *measure) {
    AsciiStringView staffNumberString = e_.asciiAttribute("number");

    // by default, apply to all staves in part
    int startStaff = 0;
    int endStaff = static_cast<int>(nstaves_) - 1;

    // but if a staff number was specified in the measure-style tag, use that instead
    if (!staffNumberString.empty()) {
        int staffNumber = staffNumberString.toInt();
        if (staffNumber < 1 || staffNumber > static_cast<int>(nstaves_)) {
            logger_->logError(String(u"measure-style staff number can only be int from 1 to _nstaves."));
        }
        --staffNumber; // convert to 0-based
        endStaff = startStaff = staffNumber;
    }

    while (e_.readNextStartElement()) {
        if (e_.name() == "multiple-rest") {
            int multipleRest = e_.readInt();
            if (multipleRest > 1) {
                multiMeasureRestCount_ = multipleRest;
                score_->style().set(Sid::createMultiMeasureRests, true);
                measure->setBreakMultiMeasureRest(true);
            } else {
                logger_->logError(String(u"multiple-rest %1 not supported").arg(multipleRest), &e_);
            }
        } else if (e_.name() == "measure-repeat") {
            String startStop = e_.attribute("type");
            // note: possible "slashes" attribute is either redundant with numMeasures or not supported by MuseScore, so ignored either way
            if (startStop == u"start") {
                int numMeasures = e_.readInt();
                for (int i = startStaff; i <= endStaff; i++) {
                    measureRepeatNumMeasures_[i] = numMeasures;
                    measureRepeatCount_[i] =
                        numMeasures; // measure repeat(s) haven't actually started yet in current measure, so this is a lie,
                                     // but if we pretend it's true then everything is set for the next measure to restart
                }
            } else { // "stop"
                for (int i = startStaff; i <= endStaff; i++) {
                    measureRepeatNumMeasures_[i] = 0;
                }
                e_.skipCurrentElement(); // since not reading any text inside stop tag, we are done with this element
            }
        } else if (e_.name() == "slash") {
            String type = e_.attribute("type");
            String stems = e_.attribute("use-stems");
            measureStyleSlash_ = type == u"start" ? (stems == u"yes" ? MusicXmlSlash::RHYTHM : MusicXmlSlash::SLASH) : MusicXmlSlash::NONE;
            e_.skipCurrentElement();
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   preventNegativeTick
//---------------------------------------------------------
/**
  Prevent an offset that would result in a negative tick (set offset to -tick instead, resulting in a tick of 0)
 */

static void preventNegativeTick(const Fraction &tick, Fraction &offset, MusicXmlLogger *logger) {
    if (tick + offset < Fraction(0, 1)) {
        logger->logError(String(u"illegal offset %1 at tick %2").arg(offset.ticks()).arg(tick.ticks()));
        offset = -tick;
    }
}

String MusicXmlParserDirection::placement() const {
    if (placement_.empty() && hasTotalY()) {
        return totalY() < 0 ? u"above" : u"below";
    } else {
        return placement_;
    }
}

static GradualTempoChangeType getTempoChangeTypeFromString(String txt) {
    txt = txt.toLower();
    if (txt.contains(u"accel")) {
        return GradualTempoChangeType::Accelerando;
    } else if (txt.contains(u"allarg")) {
        return GradualTempoChangeType::Allargando;
    } else if (txt == u"calando") {
        return GradualTempoChangeType::Calando;
    } else if (txt.contains(u"lent")) {
        return GradualTempoChangeType::Lentando;
    } else if (txt == u"morendo") {
        return GradualTempoChangeType::Morendo;
    } else if (txt == u"precipitando") {
        return GradualTempoChangeType::Precipitando;
    } else if (txt.contains(u"rall")) {
        return GradualTempoChangeType::Rallentando;
    } else if (txt.contains(u"rit")) {
        return GradualTempoChangeType::Ritardando;
    } else if (txt.contains(u"smorz")) {
        return GradualTempoChangeType::Smorzando;
    } else if (txt == u"stringendo") {
        return GradualTempoChangeType::Stringendo;
    } else {
        return GradualTempoChangeType::Undefined;
    }
}

static void terminateInferredLine(const std::vector<TextLineBase *> lines, const Fraction &tick, const track_idx_t track,
                                  bool inferTextType) {
    // Check staff and end any lines which are waiting
    if (inferTextType) {
        // To avoid extending lines which aren't intended to be terminated by their end markings,
        // only extend lines to markings within 24 quarter notes
        static const Fraction MAX_INFERRED_LINE_LEN = Fraction(24, 4);
        for (TextLineBase *line : lines) {
            Fraction diff = tick - line->tick();
            if (line && line->staffIdx() == track2staff(track) && line->ticks() == Fraction(0, 1) && diff <= MAX_INFERRED_LINE_LEN) {
                line->setTrack2(track);
                line->setTick2(tick);
            }
        }
    }
}

//---------------------------------------------------------
//   direction
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction node.
 */

void MusicXmlParserDirection::direction(const String &partId, Measure *measure, const Fraction &tick, MusicXmlSpannerMap &spanners,
                                        DelayedDirectionsList &delayedDirections, InferredFingeringsList &inferredFingerings,
                                        HarmonyMap &harmonyMap, bool &measureHasCoda, SegnoStack &segnos) {
    //LOGD("direction tick %s", muPrintable(tick.print()));

    placement_ = e_.attribute("placement");
    track_ = pass1_.trackForPart(partId);
    bool isVocalStaff = pass1_.isVocalStaff(partId);
    bool isPercussionStaff = pass1_.isPercussionStaff(partId);
    bool isExpressionText = false;
    bool delayOttava = pass1_.exporterSoftware() == MusicXmlExporterSoftware::SIBELIUS;
    systemDirection_ = e_.attribute("system") == "only-top";
    //LOGD("direction track %d", track);
    std::vector<MusicXmlSpannerDesc> starts;
    std::vector<MusicXmlSpannerDesc> stops;
    bool isDynamicRange = false;
    bool tempoTextAdded = false;

    // note: file order is direction-type first, then staff
    // this means staff is still unknown when direction-type is handled
    // easiest solution is to put spanners on a stop and start list
    // and handle these after the while loop

    // note that placement is a <direction> attribute (which is currently supported by MS)
    // but the <direction-type> children also have formatting attributes
    // (currently NOT supported by MS, at least not for spanners when <direction> children)

    while (e_.readNextStartElement()) {
        if (e_.name() == "direction-type") {
            directionType(starts, stops);
        } else if (e_.name() == "offset") {
            offset_ = pass1_.calcTicks(e_.readInt(), pass2_.divs(), &e_);
            preventNegativeTick(tick, offset_, logger_);
        } else if (e_.name() == "sound") {
            sound();
        } else if (e_.name() == "staff") {
            String strStaff = e_.readText();
            int staff = pass1_.getMusicXmlPart(partId).staffNumberToIndex(strStaff.toInt());
            if (staff >= 0) {
                track_ += staff * VOICES;
            }
        } else {
            skipLogCurrElem();
        }
    }

    handleRepeats(measure, tick + offset_, measureHasCoda, segnos, delayedDirections);
    handleNmiCmi(measure, tick + offset_, delayedDirections);
    handleFraction();
    handleChordSym(tick + offset_, harmonyMap);
    handleDrumInstrument(isPercussionStaff, tick + offset_);

    // fix for Sibelius 7.1.3 (direct export) which creates metronomes without <sound tempo="..."/>:
    // if necessary, use the value calculated by metronome()
    // note: no floating point comparisons with 0 ...
    if (tpoSound_ < 0.1 && tpoMetro_ > 0.1) {
        tpoSound_ = tpoMetro_;
    }

    //LOGD("words '%s' rehearsal '%s' metro '%s' tpo %g",
    //       muPrintable(_wordsText), muPrintable(_rehearsalText), muPrintable(_metroText), _tpoSound);

    // create text if any text was found
    if (isLyricBracket()) {
        return;
    } else if (isLikelyCredit(tick)) {
        Text *inferredText = addTextToHeader(TextStyleType::COMPOSER);
        if (inferredText) {
            pass1_.setHasInferredHeaderText(true);
            hideRedundantHeaderText(inferredText, {u"lyricist", u"composer", u"poet"});
        }
    } else if (isLikelySubtitle(tick)) {
        Text *inferredText = addTextToHeader(TextStyleType::SUBTITLE);
        if (inferredText) {
            pass1_.setHasInferredHeaderText(true);
            if (score_->metaTag(u"source").isEmpty()) {
                score_->setMetaTag(u"source", inferredText->plainText());
            }
            hideRedundantHeaderText(inferredText, {u"source"});
        }
    } else if (isLikelyLegallyDownloaded(tick)) {
        // Ignore (TBD: print to footer?)
        return;
    } else if (isLikelyTempoText(track_)) {
        TempoText *tt = Factory::createTempoText(score_->dummy());
        tt->setXmlText(wordsText_ + metroText_);
        if (tpoSound_ > 0 && canAddTempoText(score_, tick.ticks())) {
            double tpo = tpoSound_ / 60;
            tt->setTempo(tpo);
            if (tt->plainText().contains('=')) {
                tt->setFollowText(true);
            }
        }
        tt->setVisible(visible_);

        pass2_.addElemOffset(tt, track_, placement(), measure, tick + offset_);
        tempoTextAdded = true;
    } else if (isLikelyTempoLine(track_)) {
        String simplifiedText = MScoreTextToMusicXml::toPlainText(wordsText_).simplified();
        wordsText_.clear();

        GradualTempoChangeType gtc = getTempoChangeTypeFromString(simplifiedText);

        GradualTempoChange *tempoLine = Factory::createGradualTempoChange(score_->dummy());
        tempoLine->setTempoChangeType(gtc);
        tempoLine->setBeginText(simplifiedText);
        tempoLine->setContinueText(u"");
        inferredTempoLineStart_ = tempoLine;
    } else if (isLikelySticking()) {
        Sticking *sticking = Factory::createSticking(score_->dummy());
        sticking->setXmlText(wordsText_);
        if (!RealIsNull(relativeX_)) {
            PointF offset = sticking->offset();
            offset.setX(relativeX_);
            sticking->setOffset(offset);
            sticking->setPropertyFlags(Pid::OFFSET, PropertyFlags::UNSTYLED);
        }

        // Add element to score later, after collecting all the others and sorting by default-y
        // This allows default-y to be at least respected by the order of elements
        // We also need to check whether the chord at the current tick has a grace note. Directions can appear
        // before the note they apply to, so this may not have been read yet
        MusicXmlDelayedDirectionElement *delayedDirection =
            new MusicXmlDelayedDirectionElement(totalY(), sticking, track_, placement(), measure, tick + offset_);
        delayedDirections.push_back(delayedDirection);
    } else if (isLikelyDynamicRange()) {
        isDynamicRange = true;
    } else if (!wordsText_.empty() || !rehearsalText_.empty() || !metroText_.empty()) {
        TextBase *t = 0;
        if (tpoSound_ > 0.1) {
            if (canAddTempoText(score_, tick.ticks())) {
                tpoSound_ /= 60;
                t = Factory::createTempoText(score_->dummy());
                String rawWordsText = wordsText_;
                static const std::regex re("(<.*?>)");
                rawWordsText.remove(re);
                String sep = !metroText_.empty() && !rawWordsText.empty() && rawWordsText.back() != ' ' ? u" " : String();
                t->setXmlText(wordsText_ + sep + metroText_);
                ((TempoText *) t)->setTempo(tpoSound_);
                if (t->plainText().contains('=')) {
                    ((TempoText *) t)->setFollowText(true);
                }
                tempoTextAdded = true;
            }
        } else {
            if (!wordsText_.empty() || !metroText_.empty()) {
                PlayingTechniqueType technique = PlayingTechniqueType::Undefined;
                if (!play_.empty()) {
                    technique = TConv::fromXml(play_.toAscii().constChar(), PlayingTechniqueType::Undefined);
                    play_.clear();
                } else {
                    technique = getPlayingTechnique();
                }
                isExpressionText = wordsText_.contains(u"<i>") && metroText_.empty() && placement() == u"below";
                if (isExpressionText) {
                    t = Factory::createExpression(score_->dummy());
                } else if (systemDirection_) {
                    t = Factory::createSystemText(score_->dummy());
                } else if (technique != PlayingTechniqueType::Undefined) {
                    t = Factory::createPlayTechAnnotation(score_->dummy(), technique, TextStyleType::STAFF);
                } else {
                    t = Factory::createStaffText(score_->dummy());
                }
                t->setXmlText(wordsText_ + metroText_);
            } else {
                t = Factory::createRehearsalMark(score_->dummy());
                if (!rehearsalText_.contains(u"<b>")) {
                    rehearsalText_ = u"<b></b>" + rehearsalText_; // explicitly turn bold off
                }
                t->setXmlText(rehearsalText_);
                if (!hasDefaultY_) {
                    t->setPlacement(PlacementV::ABOVE); // crude way to force placement TODO improve ?
                    t->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
                    t->resetProperty(Pid::OFFSET);
                }
            }
        }

        if (t) {
            if (enclosure_ == "circle") {
                t->setFrameType(FrameType::CIRCLE);
            } else if (enclosure_ == "none") {
                t->setFrameType(FrameType::NO_FRAME);
            } else if (enclosure_ == "rectangle") {
                t->setFrameType(FrameType::RECTANGLE);
                t->setFrameRound(0_sp);
            }

            colorItem(t, color_);

            if (configuration()->importLayout()) {
                if (justify_ == u"right") {
                    t->setAlign(AlignH::RIGHT);
                } else if (justify_ == u"center") {
                    t->setAlign(AlignH::HCENTER);
                } else {
                    t->setAlign(AlignH::LEFT);
                }
            }

            t->setVisible(visible_);

            if (swing_.second != 0) {
                toStaffTextBase(t)->setSwing(true);
                toStaffTextBase(t)->setSwingParameters(swing_.first,
                                                       swing_.first ? swing_.second : toStaffTextBase(t)->style().styleI(Sid::swingRatio));
                swing_.second = 0;
            }

            String wordsPlacement = placement_;
            // Case-based defaults
            if (wordsPlacement.empty()) {
                if (isVocalStaff) {
                    wordsPlacement = u"above";
                } else if (isExpressionText) {
                    wordsPlacement = u"below";
                }
            }

            String fingeringStr = wordsText_;
            static const std::regex xmlFormating("(<.*?>)");
            fingeringStr.remove(xmlFormating).remove(u'\u00A0');
            if (isLikelyFingering(fingeringStr)) {
                logger_->logDebugInfo(String(u"Inferring fingering: %1").arg(fingeringStr));
                t->setXmlText(fingeringStr);
                MusicXmlInferredFingering *inferredFingering = new MusicXmlInferredFingering(
                    totalY(), t, fingeringStr, track_, placement(), measure, tick + offset_, configuration()->needUseDefaultFont());
                inferredFingerings.push_back(inferredFingering);
            } else {
                if (hasTotalY()) {
                    // Add element to score later, after collecting all the others and sorting by default-y
                    // This allows default-y to be at least respected by the order of elements
                    MusicXmlDelayedDirectionElement *delayedDirection =
                        new MusicXmlDelayedDirectionElement(totalY(), t, track_, placement(), measure, tick + offset_);
                    delayedDirections.push_back(delayedDirection);
                } else {
                    pass2_.addElemOffset(t, track_, placement(), measure, tick + offset_);
                }
            }
        }
    } else if (tpoSound_ > 0) {
        // direction without text but with sound tempo="..."
        // create an invisible default TempoText

        if (canAddTempoText(score_, tick.ticks())) {
            double tpo = tpoSound_ / 60;
            TempoText *t = Factory::createTempoText(score_->dummy());
            t->setXmlText(String(u"%1 = %2").arg(TempoText::duration2tempoTextString(TDuration(DurationType::V_QUARTER))).arg(tpoSound_));
            t->setVisible(false);
            t->setTempo(tpo);
            t->setFollowText(true);

            pass2_.addElemOffset(t, track_, placement(), measure, tick + offset_);
            tempoTextAdded = true;
        }
    }

    if (tempoTextAdded) {
        const InferredTempoLineStack &lines = pass2_.getInferredTempoLine();
        terminateInferredLine(std::vector<TextLineBase *>(lines.begin(), lines.end()), tick + offset_, track_,
                              configuration()->inferTextType());
    }
    addInferredTempoLine(tick + offset_);

    Dynamic *firstDyn = nullptr;

    // do dynamics
    for (auto it = dynamicsList_.begin(); it != dynamicsList_.end(); ++it) {
        Dynamic *dynamic = createImportedDynamic(score_, *it);
        // Preserve the existing direction visibility policy.
        dynamic->setVisible(visible_);

        if (isDynamicRange) {
            if (it == dynamicsList_.begin()) {
                firstDyn = dynamic;
            } else if (it == dynamicsList_.end() - 1 && firstDyn) {
                // append hyphen and this dynamic to first
                firstDyn->setXmlText(firstDyn->xmlText() + u"<sym>dynamicCombinedSeparatorHyphen</sym>" + dynamic->xmlText());
                delete dynamic;
                continue;
            }
        }

        if (!dynaVelocity_.empty()) {
            dynamic->setVelocity(musicXmlDynamicVelocity(dynaVelocity_, 0, 0));
        }

        String dynamicsPlacement = it->placement;
        // Case-based defaults
        if (dynamicsPlacement.empty()) {
            dynamicsPlacement = isVocalStaff ? u"above" : u"below";
        }

        const InferredHairpinsStack &hairpins = pass2_.getInferredHairpins();
        terminateInferredLine(std::vector<TextLineBase *>(hairpins.begin(), hairpins.end()), tick + offset_, track_,
                              configuration()->inferTextType());

        // Add element to score later, after collecting all the others and sorting by default-y
        // This allows default-y to be at least respected by the order of elements
        MusicXmlDelayedDirectionElement *delayedDirection = new MusicXmlDelayedDirectionElement(
            it->hasTotalY ? it->totalY : 100, dynamic, track_, dynamicsPlacement, measure, tick + offset_);
        delayedDirections.push_back(delayedDirection);
    }

    addInferredHairpin(tick + offset_, isVocalStaff);

    // handle the elems
    for (EngravingItem *elem : elems_) {
        // TODO (?) if (_hasDefaultY) elem->setYoff(_defaultY);
        if (hasTotalY()) {
            MusicXmlDelayedDirectionElement *delayedDirection =
                new MusicXmlDelayedDirectionElement(totalY(), elem, track_, placement(), measure, tick + offset_);
            delayedDirections.push_back(delayedDirection);
        } else {
            pass2_.addElemOffset(elem, track_, placement(), measure, tick + offset_);
        }
    }

    // handle the spanner stops first
    for (MusicXmlSpannerDesc desc : stops) {
        MusicXmlExtendedSpannerDesc &spdesc = pass2_.getSpanner({desc.tp, desc.nr});
        if (spdesc.isStopped) {
            logger_->logError(u"spanner already stopped", &e_);
            delete desc.sp;
        } else {
            if (spdesc.isStarted) {
                // Adjustments to ottavas by the offset value are unwanted
                const Fraction spTick = spdesc.sp && spdesc.sp->isOttava() ? tick : tick + offset_;
                if (spdesc.sp && spdesc.sp->isOttava() && delayOttava) {
                    // Sibelius writes ottava ends 1 note too early
                    pass2_.setDelayedOttava(spdesc.sp);
                    pass2_.delayedOttava()->setTrack2(track_);
                    pass2_.delayedOttava()->setTick2(spTick);
                    // need to set tick again later
                    pass2_.clearSpanner(desc);
                } else {
                    handleSpannerStop(spdesc.sp, track_, spTick, spanners);
                    pass2_.clearSpanner(desc);
                }
            } else {
                spdesc.sp = desc.sp;
                const Fraction spTick = spdesc.sp && spdesc.sp->isOttava() ? tick : tick + offset_;
                spdesc.tick2 = spTick;
                spdesc.track2 = track_;
                spdesc.isStopped = true;
            }
        }
    }

    // then handle the spanner starts
    // TBD handle offset ?
    for (MusicXmlSpannerDesc desc : starts) {
        MusicXmlExtendedSpannerDesc &spdesc = pass2_.getSpanner({desc.tp, desc.nr});
        if (spdesc.isStarted) {
            logger_->logError(u"spanner already started", &e_);
            delete desc.sp;
        } else {
            String spannerPlacement = placement();
            // Case-based defaults
            if (spannerPlacement.empty()) {
                if (desc.sp->isHairpin()) {
                    spannerPlacement = isVocalStaff ? u"above" : u"below";
                } else {
                    spannerPlacement = totalY() < 0 ? u"above" : u"below";
                }
            }
            desc.sp->setVisible(visible_);
            if (spdesc.isStopped) {
                pass2_.addSpanner(desc);
                // handleSpannerStart and handleSpannerStop must be called in order
                // due to allocation of elements in the map
                handleSpannerStart(desc.sp, track_, spannerPlacement, tick + offset_, spanners);
                handleSpannerStop(spdesc.sp, spdesc.track2, spdesc.tick2, spanners);
                pass2_.clearSpanner(desc);
            } else {
                pass2_.addSpanner(desc);
                handleSpannerStart(desc.sp, track_, spannerPlacement, tick + offset_, spanners);
                spdesc.isStarted = true;
            }
        }
    }
}

//---------------------------------------------------------
//   isLikelyCredit
//---------------------------------------------------------

bool MusicXmlParserDirection::isLikelyCredit(const Fraction &tick) const {
    if (!configuration()->inferTextType()) {
        return false;
    }
    return (tick + offset_ < Fraction(5, 1)) // Only early in the piece
           && rehearsalText_.empty() && metroText_.empty() && tpoSound_ < 0.1 && isLikelyCreditText(wordsText_, false);
}

//---------------------------------------------------------
//   isLyricBracket
//    Dolet exports lyric brackets as staff text,
//    which we ought not render.
//---------------------------------------------------------

bool MusicXmlParserDirection::isLyricBracket() const {
    if (wordsText_.empty()) {
        return false;
    }

    if (!(wordsText_.front() == u'}' || wordsText_.back() == u'{')) {
        return false;
    }

    return rehearsalText_.empty() && metroText_.empty() && dynamicsList_.empty() && tpoSound_ < 0.1;
}

bool MusicXmlParserDirection::isLikelySubtitle(const Fraction &tick) const {
    if (!configuration()->inferTextType()) {
        return false;
    }
    return (tick + offset_ < Fraction(5, 1)) // Only early in the piece
           && rehearsalText_.empty() && metroText_.empty() && tpoSound_ < 0.1 && isLikelySubtitleText(wordsText_, false);
}

bool MusicXmlParserDirection::isLikelyLegallyDownloaded(const Fraction &tick) const {
    if (!configuration()->inferTextType()) {
        return false;
    }
    return (tick + offset_ < Fraction(5, 1)) // Only early in the piece
           && rehearsalText_.empty() && metroText_.empty() && tpoSound_ < 0.1 &&
           wordsText_.contains(std::wregex(L"This music has been legally downloaded\\.(?:\\s|\\u00A0)Do not photocopy\\."));
}

bool MusicXmlParserDirection::isLikelyTempoText(const track_idx_t track) const {
    if (!configuration()->inferTextType() || wordsText_.contains(u"<i>") || wordsText_.contains(u"“") || wordsText_.contains(u"”") ||
        placement() == u"below" || track2staff(track) || wordsText_.empty() || swing_.second) {
        return false;
    }

    const String plainText = MScoreTextToMusicXml::toPlainText(wordsText_.simplified());
    static const std::array<String, 29> tempoStrs = {
        u"adag",  u"alleg",  u"andant", u"a tempo",  u"ballad", u"brisk",        u"determination", u"dolce",      u"expressive", u"fast",
        u"free",  u"gently", u"grave",  u"larg",     u"lento",  u"stesso tempo", u"lively",        u"maestoso",   u"moderat",    u"mosso",
        u"prest", u"rubato", u"slow",   u"straight", u"tango",  u"tempo i",      u"tenderly",      u"triumphant", u"vivace"};

    for (const String &str : tempoStrs) {
        if (plainText.contains(str, CaseSensitivity::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

void MusicXmlParserDirection::handleFraction() {
    if (!configuration()->inferTextType()) {
        return;
    }

    String rawWordsText = wordsText_;
    static const std::regex re("(<.*?>)");
    rawWordsText.remove(re);
    rawWordsText = rawWordsText.simplified();
    //                         UTF-16 encoding: 0x00BC, 0x00BD, 0x00BE, 0x2150, 0x2151, 0x2152,  etc...
    static const std::array<String, 18> fracs{u"1/4", u"1/2", u"3/4", u"1/7", u"1/9", u"1/10", u"1/3", u"2/3", u"1/5",
                                              u"2/5", u"3/5", u"4/5", u"1/6", u"5/6", u"1/8",  u"3/8", u"5/8", u"7/8"};

    for (size_t n = 0; n < fracs.size(); n++) {
        if (rawWordsText.contains(fracs.at(n))) {
            size_t p = n <= 2 ? 0x00BC + n : 0x2150 + n - 3;
            rawWordsText.replace(fracs.at(n), String(char16_t(p)));
            wordsText_ = rawWordsText;
            return;
        }
    }
}

bool MusicXmlParserDirection::isLikelyTempoLine(const track_idx_t track) const {
    if (!configuration()->inferTextType() || wordsText_.contains(u"“") || wordsText_.contains(u"”") || placement() == u"below" ||
        track2staff(track) != 0 || wordsText_.empty()) {
        return false;
    }

    const String plainText = MScoreTextToMusicXml::toPlainText(wordsText_.simplified());
    static const std::wregex re(L"(^|(?:\\s|\u00A0))(accel|faster|mosso|rit|rubato|slower|stretto)", std::regex_constants::icase);

    return plainText.contains(re);
}

Text *MusicXmlParserDirection::addTextToHeader(const TextStyleType textStyleType) {
    Text *t = Factory::createText(score_->dummy(), textStyleType);
    t->setXmlText(wordsText_.trimmed());
    t->renderer()->layoutText1(t);
    MeasureBase *const firstMeasure = score_->measures()->first();
    VBox *vbox =
        firstMeasure->isVBox() ? toVBox(firstMeasure) : MusicXmlParserPass1::createAndAddVBoxForCreditWords(score_, Fraction(0, 1));
    double spatium = score_->style().styleD(Sid::spatium);
    vbox->setBoxHeight(vbox->boxHeight() + Spatium(t->height() / spatium / 2)); // add some height
    vbox->add(t);
    return t;
}

//---------------------------------------------------------
//   hideRedundantHeaderText
//    After inferring header text, hide redundant text.
//    Redundant text is detected by checking all the
//    Text elements of the inferred text's VBox against
//    the contents of the eligible metaTags
//---------------------------------------------------------

void MusicXmlParserDirection::hideRedundantHeaderText(const Text *inferredText, const std::vector<String> metaTags) {
    if (!inferredText->parent()->isVBox()) {
        return;
    }

    for (EngravingItem *e : toVBox(inferredText->parent())->el()) {
        if (e == inferredText || !e->isText()) {
            continue;
        }

        Text *t = toText(e);
        for (const String &metaTag : metaTags) {
            if (t->plainText() == score_->metaTag(metaTag)) {
                t->setVisible(false);
                continue;
            }
        }
    }
}

//---------------------------------------------------------
//   directionType
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type node.
 */

void MusicXmlParserDirection::directionType(std::vector<MusicXmlSpannerDesc> &starts, std::vector<MusicXmlSpannerDesc> &stops) {
    while (e_.readNextStartElement()) {
        defaultY_ = e_.asciiAttribute("default-y").toDouble(&hasDefaultY_) * -0.1;
        relativeX_ = e_.doubleAttribute("relative-x") / 10 * score_->style().spatium();
        visible_ = e_.asciiAttribute("print-object") != "no"; // only available for "metronome" and "other-direction"
        const String number = e_.attribute("number");
        int n = 0;
        if (!number.empty()) {
            n = number.toInt();
            if (n <= 0) {
                logger_->logError(String(u"invalid number %1").arg(number), &e_);
            } else {
                n--; // make zero-based
            }
        }
        const String type = e_.attribute("type");
        color_ = Color::fromString(e_.asciiAttribute("color").ascii());
        justify_ = e_.attribute("justify");
        if (e_.name() == "metronome") {
            metroText_ = metronome(tpoMetro_);
        } else if (e_.name() == "words") {
            enclosure_ = e_.attribute("enclosure");
            fontFamily_ = e_.attribute("font-family");
            String nextPart = xmlpass2::nextPartOfFormattedString(e_, *configuration(), *score_->engravingFonts(), pass1_.dolet());

            textToDynamic(nextPart);
            textToCrescLine(nextPart);
            handleTempo(nextPart);
            wordsText_ += nextPart;
        } else if (e_.name() == "rehearsal") {
            enclosure_ = e_.attribute("enclosure");
            if (enclosure_.empty()) {
                enclosure_ = u"square"; // note different default
            }
            rehearsalText_ += xmlpass2::nextPartOfFormattedString(e_, *configuration(), *score_->engravingFonts(), pass1_.dolet());
        } else if (e_.name() == "harp-pedals") {
            harpPedal();
        } else if (e_.name() == "pedal") {
            pedal(type, n, starts, stops);
        } else if (e_.name() == "octave-shift") {
            octaveShift(type, n, starts, stops);
        } else if (e_.name() == "dynamics") {
            dynamics();
        } else if (e_.name() == "bracket") {
            bracket(type, n, starts, stops);
        } else if (e_.name() == "dashes") {
            dashes(type, n, starts, stops);
        } else if (e_.name() == "wedge") {
            wedge(type, n, starts, stops);
        } else if (e_.name() == "coda") {
            const String smufl = e_.attribute("smufl");
            if (!smufl.empty()) {
                wordsText_ += u"<sym>" + smufl + u"</sym>";
            } else {
                wordsText_ += u"<sym>coda</sym>";
            }
            e_.skipCurrentElement();
        } else if (e_.name() == "segno") {
            const String smufl = e_.attribute("smufl");
            if (!smufl.empty()) {
                wordsText_ += u"<sym>" + smufl + u"</sym>";
            } else {
                wordsText_ += u"<sym>segno</sym>";
            }
            e_.skipCurrentElement();
        } else if (e_.name() == "eyeglasses") {
            wordsText_ += u"<sym>miscEyeglasses</sym>";
            e_.skipCurrentElement();
        } else if (e_.name() == "symbol") {
            const String smufl = e_.readText();
            if (!smufl.empty()) {
                wordsText_ += u"<sym>" + smufl + u"</sym>";
            }
        } else if (e_.name() == "string-mute") {
            if (type == u"on") {
                wordsText_ += u"<sym>stringsMuteOn</sym>";
            } else if (type == u"off") {
                wordsText_ += u"<sym>stringsMuteOff</sym>";
            }
            e_.skipCurrentElement();
        } else if (e_.name() == "other-direction") {
            otherDirection();
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   sound
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/sound node.
 */

void MusicXmlParserDirection::sound() {
    sndCoda_ = e_.attribute("coda");
    sndDacapo_ = e_.attribute("dacapo");
    sndDalsegno_ = e_.attribute("dalsegno");
    sndFine_ = e_.attribute("fine");
    sndSegno_ = e_.attribute("segno");
    sndToCoda_ = e_.attribute("tocoda");
    tpoSound_ = e_.doubleAttribute("tempo");
    dynaVelocity_ = e_.attribute("dynamics");

    const String pizz = e_.attribute("pizzicato");
    if (pizz == u"yes") {
        play_ = u"pizzicato";
    } else if (pizz == u"no") {
        play_ = u"natural";
    }

    while (e_.readNextStartElement()) {
        if (e_.name() == "play") {
            play();
        } else if (e_.name() == "swing") {
            swing();
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   play
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/sound/play node.
 */

void MusicXmlParserDirection::play() {
    while (e_.readNextStartElement()) {
        if (e_.name() == "mute") {
            const String muted = e_.readText();
            play_ = (muted == u"off") ? u"open" : u"mute";
        } else if (e_.name() == "other-play") {
            play_ = e_.attribute("type");
            e_.skipCurrentElement();
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   swing
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/sound/swing node.
 */

void MusicXmlParserDirection::swing() {
    int swingNumerator = 1;
    int swingDenominator = 1;
    int swingUnit = 0;
    while (e_.readNextStartElement()) {
        if (e_.name() == "straight") {
            // unused
            e_.skipCurrentElement();
        } else if (e_.name() == "first") {
            swingDenominator = e_.readText().toInt();
        } else if (e_.name() == "second") {
            swingNumerator = e_.readText().toInt();
        } else if (e_.name() == "swing-type") {
            const String swingType = e_.readText();
            if (swingType == u"eighth") {
                swingUnit = Constants::DIVISION / 2;
            } else if (swingType == u"16th") {
                swingUnit = Constants::DIVISION / 4;
            }
        } else if (e_.name() == "swing-style") {
            // unused
            e_.skipCurrentElement();
        } else {
            skipLogCurrElem();
        }
    }
    swing_.first = swingUnit;
    swing_.second = (swingNumerator * 100) / swingDenominator;
}

//---------------------------------------------------------
//   dynamics
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/dynamics node.
 */

void MusicXmlParserDirection::dynamics() {
    MusicXmlDynamic dynamic = parseDynamicsContainer(e_, logger_);
    // MusicXML dynamics placement is ignored in directions; the parent wins.
    dynamic.placement = placement();
    dynamic.hasTotalY = hasTotalY();
    dynamic.totalY = totalY();
    if (!dynamic.text.empty()) {
        dynamicsList_.push_back(dynamic);
    }
}

//---------------------------------------------------------
//   harpPedal
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/hair-pedal node.
 */

void MusicXmlParserDirection::harpPedal() {
    const std::vector<String> pedalSteps = {u"D", u"C", u"B", u"E", u"F", u"G", u"A"};
    const Color color = Color::fromString(e_.attribute("color"));

    HarpPedalDiagram *hpd = Factory::createHarpPedalDiagram(score_->dummy());
    while (e_.readNextStartElement()) {
        int stepIndex = 0;
        PedalPosition pedpos = PedalPosition::UNSET;
        while (e_.readNextStartElement()) {
            if (e_.name() == "pedal-step") {
                stepIndex = std::distance(std::begin(pedalSteps), std::find(std::begin(pedalSteps), std::end(pedalSteps), e_.readText()));
            } else if (e_.name() == "pedal-alter") {
                pedpos = static_cast<PedalPosition>(e_.readText().trimmed().toInt() + 1);
            } else {
                skipLogCurrElem();
            }
        }
        hpd->setPedal(static_cast<HarpStringType>(stepIndex), pedpos);
        colorItem(hpd, color);
    }
    elems_.push_back(hpd);
}

void MusicXmlParserDirection::otherDirection() {
    // <other-direction> element is used to define any <direction> symbols not yet in the MusicXML format
    const String smufl = e_.attribute("smufl");
    const Color color = Color::fromString(e_.attribute("color"));

    // Read smufl symbol
    if (!smufl.empty()) {
        SymId id = SymNames::symIdByName(smufl, SymId::noSym);
        if (id != SymId::noSym) {
            Symbol *smuflSym = Factory::createSymbol(score_->dummy());
            smuflSym->setSym(id);
            colorItem(smuflSym, color);
            elems_.push_back(smuflSym);
        }
        e_.skipCurrentElement();
    } else {
        // TODO: Multiple sets of maps for exporters other than Dolet 6/Sibelius
        // TODO: Add more symbols from Sibelius
        std::map<String, String> otherDirectionStrings;
        if (pass1_.dolet()) {
            otherDirectionStrings = {
                {String(u"To Coda"), String(u"To Coda")},
                {String(u"Segno"), String(u"<sym>segno</sym>")},
                {String(u"CODA"), String(u"CODA")},
            };
        }
        static const std::map<String, SymId> otherDirectionSyms = {{String(u"Rhythm dot"), SymId::augmentationDot},
                                                                   {String(u"Whole rest"), SymId::restWhole},
                                                                   {String(u"l.v. down"), SymId::articLaissezVibrerBelow},
                                                                   {String(u"8vb"), SymId::ottavaBassaVb},
                                                                   {String(u"Treble clef"), SymId::gClef},
                                                                   {String(u"Bass clef"), SymId::fClef},
                                                                   {String(u"Caesura"), SymId::caesura},
                                                                   {String(u"Thick caesura"), SymId::caesuraThick}};
        String t = e_.readText();
        String val = muse::value(otherDirectionStrings, t);
        if (!val.empty()) {
            wordsText_ += val;
        } else {
            SymId sym = muse::value(otherDirectionSyms, t);
            Symbol *smuflSym = Factory::createSymbol(score_->dummy());
            smuflSym->setSym(sym);
            colorItem(smuflSym, color);
            elems_.push_back(smuflSym);
        }
    }
}

static const String getMarkerId(const String &words) {
    // Helper function to set the marker ID string
    // eg. extract 'II' from 'To Coda II'
    static const std::regex number("(i{0,3})");
    const StringList nums = words.search(number, {1}, SplitBehavior::SkipEmptyParts);
    if (!nums.empty()) {
        return nums.at(0);
    }
    return String();
}

String MusicXmlParserDirection::matchRepeat(const String &plainWords) const {
    static const std::wregex daCapo(L"^(d\\.? ?|da )(c\\.?|capo)$");
    static const std::wregex daCapoAlFine(L"^(d\\.? ?|da )(c\\.? ?|capo )al fine$");
    static const std::wregex daCapoAlCoda(L"^(d\\.? ?|da )(c\\.? ?|capo )al coda ?(i{0,3})?$");
    static const std::wregex dalSegno(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno) ?(i{0,3})?$");
    static const std::wregex dalSegnoAlFine(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})? ?(\\(.*\\))? al fine$");
    static const std::wregex dalSegnoAlCoda(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})? ?(\\(.*\\))? al coda ?(i{0,3})?$");
    static const std::wregex fine(L"^fine$");
    static const std::wregex segno(L"^segno( segno)? ?(i{0,3})?$");
    static const std::wregex toCoda(L"^to coda( coda)? ?(i{0,3})?$");
    static const std::wregex coda(L"^coda( coda)? ?(i{0,3})?$");

    if (plainWords.contains(daCapo)) {
        return u"daCapo";
    }
    if (plainWords.contains(daCapoAlFine)) {
        return u"daCapoAlFine";
    }
    if (plainWords.contains(daCapoAlCoda)) {
        return u"daCapoAlCoda";
    }
    if (plainWords.contains(dalSegno)) {
        return u"dalSegno";
    }
    if (plainWords.contains(dalSegnoAlFine)) {
        return u"dalSegnoAlFine";
    }
    if (plainWords.contains(dalSegnoAlCoda)) {
        return u"dalSegnoAlCoda";
    }
    if (plainWords.contains(segno)) {
        return u"segno";
    }
    if (plainWords.contains(fine)) {
        return u"fine";
    }
    if (plainWords.contains(toCoda)) {
        return u"toCoda";
    }
    if (plainWords.contains(coda)) {
        return u"coda";
    }
    return String();
}

static void addSymbolsToCoda(String &wordsString, const String &codaId) {
    // Append correct number of symbols to coda text
    if (!wordsString.contains(u"<sym>coda</sym>")) {
        String end = u"\n";
        if (codaId.size() > 0) {
            end.append(codaId);
            end.replace(u"i", u"<sym>coda</sym>");
        } else {
            end.append(u"<sym>coda</sym>");
        }
        wordsString.append(end);
    }
}

//---------------------------------------------------------
//   isLikelyFingering
//---------------------------------------------------------

bool MusicXmlParserDirection::isLikelyFingering(const String &fingeringStr) const {
    if (!configuration()->inferTextType()) {
        return false;
    }

    // One or more newline-separated digits, possibly lead or trailed by whitespace
    static const std::wregex re(L"^(?:\\s|\\u00A0)*[-–]?(?:\\s|\\u00A0)?[0-5pimac](?:[-–][0-5pimac])?(?:\n[-–]?(?:\\s|\\u00A0)?[0-5pimac](?"
                                L":[-–][0-5pimac])?)*(?:\\s|\\u00A0)*$");
    return fingeringStr.contains(re) && rehearsalText_.empty() && metroText_.empty() && tpoSound_ < 0.1;
}

//---------------------------------------------------------
//   MusicXmlInferredFingering
//---------------------------------------------------------

MusicXmlInferredFingering::MusicXmlInferredFingering(double totalY, EngravingItem *element, String &text, track_idx_t track,
                                                     String placement, Measure *measure, Fraction tick, bool needUseDefaultFont)
    : totalY_(totalY), element_(element), text_(text), track_(track), placement_(placement), measure_(measure), tick_(tick),
      needUseDefaultFont_(needUseDefaultFont) {
    fingerings_ = text_.simplified().split(u" ");
}

//---------------------------------------------------------
//   roundTick
//---------------------------------------------------------
/**
 Round tick to multiple of gcd of measure
 */

void MusicXmlInferredFingering::roundTick(Measure *measure) {
    measure->computeTicks();
    int gcdTicks = Fraction(1, 1).ticks();
    for (auto &s : measure->segments()) {
        if (s.isChordRestType()) {
            gcdTicks = std::gcd(gcdTicks, s.ticks().ticks());
        }
    }
    if (!gcdTicks || gcdTicks == Fraction(1, 1).ticks() || !(tick_.ticks() % gcdTicks)) {
        return;
    }
    int roundedTick = std::round(static_cast<double>(tick_.ticks()) / static_cast<double>(gcdTicks)) * (gcdTicks);
    tick_ = Fraction::fromTicks(roundedTick);
}

//---------------------------------------------------------
//   findAndAddToNotes
//---------------------------------------------------------
/**
 Attempts to find an eligible collection of notes to add inferred
 fingerings to. Adds notes and returns true if successful, else no-op
 and returns false.
 */
bool MusicXmlInferredFingering::findAndAddToNotes(Measure *measure) {
    roundTick(measure);
    std::vector<Note *> collectedNotes;
    for (track_idx_t track = track_; track < track_ + 4; ++track) {
        Chord *candidateChord = measure->findChord(tick(), track);
        if (candidateChord) {
            if (candidateChord->notes().size() >= fingerings().size()) {
                addToNotes(candidateChord->notes());
                return true;
            } else {
                collectedNotes.insert(collectedNotes.begin(), candidateChord->notes().begin(), candidateChord->notes().end());
                if (collectedNotes.size() >= fingerings().size()) {
                    addToNotes(collectedNotes);
                    return true;
                }
            }
        }
    }
    // No suitable notes found
    return false;
}

//---------------------------------------------------------
//   addToNotes
//---------------------------------------------------------
/**
 Add the n fingerings to the first n collected notes
 */
void MusicXmlInferredFingering::addToNotes(std::vector<Note *> &notes) const {
    assert(notes.size() >= fingerings_.size());
    for (size_t i = 0; i < fingerings_.size(); ++i) {
        // Fingerings in reverse order
        addTextToNote(-1, fingerings_[fingerings_.size() - 1 - i], placement_, u"", -1, u"", u"", true, Color::BLACK,
                      TextStyleType::FINGERING, notes[i]->score(), notes[i], needUseDefaultFont_);
    }
}

//---------------------------------------------------------
//   toDelayedDirection
//---------------------------------------------------------

MusicXmlDelayedDirectionElement *MusicXmlInferredFingering::toDelayedDirection() {
    MusicXmlDelayedDirectionElement *dd = new MusicXmlDelayedDirectionElement(totalY_, element_, track_, placement_, measure_, tick_);
    return dd;
}

//---------------------------------------------------------
//   textToDynamic
//---------------------------------------------------------
/**
 Attempts to convert text to dynamic text. No-op if unable.
 */
void MusicXmlParserDirection::textToDynamic(String &text) {
    if (!configuration()->inferTextType()) {
        return;
    }
    String simplifiedText = MScoreTextToMusicXml::toPlainText(text).simplified();
    // Correct finale's incorrect dynamic export
    if (pass1_.exporterSoftware() == MusicXmlExporterSoftware::FINALE) {
        static const std::map<String, String> finaleDynamicSubs = {{u"π", u"pp"},  {u"P", u"mp"},  {u"F", u"mf"},
                                                                   {u"ƒ", u"ff"},  {u"Ï", u"fff"}, {u"S", u"sf"},
                                                                   {u"ß", u"sfz"}, {u"Z", u"fz"},  {u"Í", u"fp"}};
        for (const auto &sub : finaleDynamicSubs) {
            if (simplifiedText == sub.first) {
                simplifiedText = sub.second;
            }
        }
    }
    // We don't want to count a single 'm', 'r', 's' or 'z' as a whole dynamic
    static const std::wregex singleCharDynamic = std::wregex(L"^[mrsz]$");
    // try to find a dynamic - xml representation or
    // if found add to dynamics list and set text to blank string
    if (!simplifiedText.contains(singleCharDynamic) && TConv::dynamicValid(simplifiedText.toStdString())) {
        DynamicType dt = TConv::fromXml(simplifiedText.toStdString(), DynamicType::OTHER);
        if (dt != DynamicType::OTHER) {
            dynaVelocity_ = String::number(round(Dynamic::dynamicVelocity(dt) / 0.9));
            MusicXmlDynamic dynamic;
            dynamic.text = Dynamic::dynamicText(dt);
            dynamic.placement = placement();
            dynamic.enclosure = enclosure_;
            dynamic.hasTotalY = hasTotalY();
            dynamic.totalY = totalY();
            dynamicsList_.push_back(dynamic);
            text.clear();
        }
    }
}

void MusicXmlParserDirection::textToCrescLine(String &text) {
    if (!configuration()->inferTextType()) {
        return;
    }
    String simplifiedText = MScoreTextToMusicXml::toPlainText(text).simplified();
    bool cresc = simplifiedText.contains(u"cresc");
    bool dim = simplifiedText.contains(u"dim");
    if (!cresc && !dim) {
        return;
    }

    // Create line
    text.clear();
    Hairpin *line = Factory::createHairpin(score_->dummy());

    line->setHairpinType(cresc ? HairpinType::CRESC_LINE : HairpinType::DIM_LINE);
    line->setBeginText(simplifiedText);
    line->setContinueText(u"");
    line->setProperty(Pid::LINE_VISIBLE, false);
    inferredHairpinStart_ = line;
}

void MusicXmlParserDirection::addInferredHairpin(const Fraction &tick, const bool isVocalStaff) {
    if (!configuration()->inferTextType() || !inferredHairpinStart_) {
        return;
    }

    inferredHairpinStart_->setTrack(track_);
    inferredHairpinStart_->setTick(tick + offset_);

    String spannerPlacement = placement_;
    if (placement_.empty()) {
        spannerPlacement = isVocalStaff ? u"above" : u"below";
    }
    setSLinePlacement(inferredHairpinStart_, spannerPlacement);

    pass2_.addInferredHairpin(inferredHairpinStart_);
}

void MusicXmlParserDirection::addInferredTempoLine(const Fraction &tick) {
    if (!configuration()->inferTextType() || !inferredTempoLineStart_) {
        return;
    }

    inferredTempoLineStart_->setTrack(track_);
    inferredTempoLineStart_->setTick(tick + offset_);

    String spannerPlacement = placement();

    setSLinePlacement(inferredTempoLineStart_, spannerPlacement);

    pass2_.addInferredTempoLine(inferredTempoLineStart_);
}

static String findDetachedRepeatNumber(const Measure *measure, const track_idx_t track, const Fraction tick, const String &placement,
                                       DelayedDirectionsList &directions) {
    static const std::wregex number(L"^(i{1,3})$");
    DelayedDirectionsList::iterator it;

    for (it = directions.begin(); it != directions.end(); ++it) {
        MusicXmlDelayedDirectionElement d = **it;
        const EngravingItem *el = d.element();
        if (d.track() == track && d.tick() == tick && d.placement() == placement && el->isTextBase()) {
            const String text = toTextBase(el)->plainText();
            if (text.toLower().contains(number)) {
                it = directions.erase(it);
                return u" " + text;
            }
        }
    }

    Segment *s = measure->findSegment(SegmentType::ChordRest, tick);
    if (s) {
        for (EngravingItem *el : s->annotations()) {
            if (el->tick() == tick && el->track() == track && el->placeAbove() == (placement == u"above") && el->isTextBase()) {
                const String text = toTextBase(el)->plainText();
                if (text.toLower().contains(number)) {
                    s->removeAnnotation(el);
                    return u" " + text;
                }
            }
        }
    }

    return String();
}

static String countSegno(const String &plainWords) {
    // Count how many 's' in D.S. or D.S.S...
    std::wsmatch matches;
    const std::wstring str = plainWords.toStdWString();
    static const std::wregex dalSegnoAlCoda(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})?(\\(.*\\))? al coda ?(i{0,3})?$");
    if (std::regex_search(str, matches, dalSegnoAlCoda)) {
        const String segnoGroup = String::fromStdWString(matches.str(2)).toLower();
        const int count = segnoGroup.count('s');
        return String::fromStdString(std::string(count, 'i'));
    }
    return String();
}

//---------------------------------------------------------
//   handleRepeats
//---------------------------------------------------------

void MusicXmlParserDirection::handleRepeats(Measure *measure, const Fraction tick, bool &measureHasCoda, SegnoStack &segnos,
                                            DelayedDirectionsList &delayedDirections) {
    // Try to recognize the various repeats
    // The explicit repeat attributes of the <sound> element are always honoured;
    // only the purely text-based fallback depends on the inferTextType preference
    String repeat;
    const String plainWords = MScoreTextToMusicXml::toPlainText(wordsText_.toLower().simplified());
    const String wordsRepeat = matchRepeat(plainWords);
    if (!sndCoda_.empty()) {
        repeat = u"coda";
    } else if (!sndDacapo_.empty()) {
        // the accompanying words may refine the jump type (e.g. "D.C. al Fine")
        repeat = wordsRepeat.startsWith(u"daCapo") ? wordsRepeat : u"daCapo";
    } else if (!sndDalsegno_.empty()) {
        repeat = wordsRepeat.startsWith(u"dalSegno") ? wordsRepeat : u"dalSegno";
    } else if (!sndFine_.empty()) {
        repeat = u"fine";
    } else if (!sndSegno_.empty()) {
        repeat = u"segno";
    } else if (!sndToCoda_.empty()) {
        repeat = u"toCoda";
    } else if (configuration()->inferTextType()) {
        repeat = wordsRepeat;
    }
    // Check if repeat number has become detached
    if (repeat == u"coda" || repeat == u"segno") {
        wordsText_ += findDetachedRepeatNumber(measure, track_, tick, placement(), delayedDirections);
    }

    // Check if coda is numbered by checking numerals after 'coda' or 'segno'
    if (repeat == u"daCapoAlCoda" || repeat == u"toCoda" || repeat == u"coda") {
        codaId_ = getMarkerId(plainWords);
    } else if (repeat == u"dalSegnoAlFine" || repeat == u"segno" || repeat == u"dalSegno") {
        segnoId_ = getMarkerId(plainWords);
    } else if (repeat == u"dalSegnoAlCoda") {
        // Find numerals eg D.S I al Coda II
        std::wsmatch matches;
        std::wstring str = plainWords.toStdWString();
        static const std::wregex dalSegnoAlCoda(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})?(\\(.*\\))? al coda ?(i{0,3})?$");
        if (std::regex_search(str, matches, dalSegnoAlCoda)) {
            segnoId_ = String::fromStdWString(matches.str(3));
            codaId_ = String::fromStdWString(matches.str(5));
        }
    }

    if ((repeat == u"dalSegno" || repeat == u"dalSegnoAlCoda") && segnoId_.empty()) {
        // If no numeral, count how many 's' in D.S. or D.S.S...
        segnoId_ = countSegno(plainWords);
    }

    if (!repeat.empty()) {
        TextBase *tb = nullptr;
        if (codaId_ == u"i") {
            codaId_.clear();
        }
        if (segnoId_ == u"i") {
            segnoId_.clear();
        }
        wordsText_ = TextBase::unEscape(wordsText_);
        if ((repeat == u"coda" || repeat == u"toCoda") && measureHasCoda) {
            // If there is already a coda in the bar, discard this marker
            wordsText_.clear();
            return;
        } else if ((tb = findJump(repeat)) || (tb = findMarker(repeat))) {
            if (tb->isMarker()) {
                if (repeat == u"segno") {
                    if (Marker *prevSegno = muse::value(segnos, tick.ticks(), nullptr)) {
                        // If there is another segno at this tick, combine their text and increase the label
                        // eg. two segno symbols will have the label 'segnoii'
                        if (prevSegno->xmlText().contains(u"<sym>segno</sym>") && wordsText_.contains(u"<sym>segno</sym>")) {
                            prevSegno->setXmlText(prevSegno->xmlText() + u" " + wordsText_);
                            wordsText_.clear();
                            const int segnoCount = prevSegno->xmlText().count(u"<sym>segno</sym>");
                            const String label = String::fromStdString(std::string(segnoCount, 'i'));
                            prevSegno->setLabel(prevSegno->label() + label);

                            return;
                        }
                    } else {
                        segnos.insert(std::pair<int, Marker *>(tick.ticks(), toMarker(tb)));
                    }
                } else {
                    if (repeat == u"coda" || repeat == u"toCoda") {
                        measureHasCoda = true;
                        addSymbolsToCoda(wordsText_, codaId_);
                    }
                }
            }

            tb->setTrack(track_);
            if (!wordsText_.empty()) {
                tb->setXmlText(wordsText_);
                wordsText_.clear();
            } else {
                tb->setVisible(false);
            }
            // Sometimes Jumps and Markers are exported on the incorrect side
            // of the barline (i.e. end of mm. 29 vs. beginning of mm. 30).
            // This fixes that.
            bool closerToLeft = tick - measure->tick() < measure->endTick() - tick;
            if (tb->textStyleType() == TextStyleType::REPEAT_RIGHT && closerToLeft && measure->prevMeasure()) {
                measure = measure->prevMeasure();
            } else if (tb->textStyleType() == TextStyleType::REPEAT_LEFT && !closerToLeft && measure->nextMeasure()) {
                measure = measure->nextMeasure();
            }
            // Temporary solution to indent codas - add a horizontal frame at start of system or midway through
            MeasureBase *prevMeasureBase = measure->prev();
            bool hbox = prevMeasureBase && prevMeasureBase->isHBox();
            if (tb->isMarker() && toMarker(tb)->markerType() == MarkerType::CODA && !hbox) {
                MeasureBase *gap = score_->insertBox(ElementType::HBOX, measure);
                toHBox(gap)->setBoxWidth(10_sp);
            }
            tb->setVisible(visible_);
            measure->add(tb);
        }
    }
}

Marker *MusicXmlParserDirection::findMarker(const String &repeat) const {
    Marker *m = nullptr;
    if (repeat == u"segno") {
        m = Factory::createMarker(score_->dummy());
        // note: Marker::read() also contains code to set text style based on type
        // avoid duplicated code
        // apparently this MUST be after setTextStyle
        m->setMarkerType(MarkerType::SEGNO);
        m->setLabel(m->propertyDefault(Pid::LABEL).value<String>() + segnoId_);
    } else if (repeat == u"coda") {
        m = Factory::createMarker(score_->dummy());
        m->setMarkerType(MarkerType::CODA);
        m->setLabel(m->propertyDefault(Pid::LABEL).value<String>() + codaId_);
    } else if (repeat == u"fine") {
        m = Factory::createMarker(score_->dummy(), TextStyleType::REPEAT_RIGHT);
        m->setMarkerType(MarkerType::FINE);
        m->resetProperty(Pid::LABEL);
    } else if (repeat == u"toCoda") {
        m = Factory::createMarker(score_->dummy(), TextStyleType::REPEAT_RIGHT);
        m->setMarkerType(MarkerType::TOCODA);
        m->setLabel(m->propertyDefault(Pid::LABEL).value<String>() + codaId_);
    }
    return m;
}

Jump *MusicXmlParserDirection::findJump(const String &repeat) const {
    Jump *jp = nullptr;
    if (repeat == u"daCapo") {
        jp = Factory::createJump(score_->dummy());
        jp->setJumpType(JumpType::DC);
    } else if (repeat == u"daCapoAlCoda") {
        jp = Factory::createJump(score_->dummy());
        jp->setJumpType(JumpType::DC_AL_CODA);
        jp->setPlayUntil(jp->playUntil() + codaId_);
        jp->setContinueAt(jp->continueAt() + codaId_);
    } else if (repeat == u"daCapoAlFine") {
        jp = Factory::createJump(score_->dummy());
        jp->setJumpType(JumpType::DC_AL_FINE);
    } else if (repeat == u"dalSegno") {
        jp = Factory::createJump(score_->dummy());
        jp->setJumpType(JumpType::DS);
        jp->setJumpTo(jp->jumpTo() + segnoId_);
    } else if (repeat == u"dalSegnoAlCoda") {
        jp = Factory::createJump(score_->dummy());
        jp->setJumpType(JumpType::DS_AL_CODA);
        jp->setJumpTo(jp->jumpTo() + segnoId_);
        jp->setPlayUntil(jp->playUntil() + codaId_);
        jp->setContinueAt(jp->continueAt() + codaId_);
    } else if (repeat == u"dalSegnoAlFine") {
        jp = Factory::createJump(score_->dummy());
        jp->setJumpType(JumpType::DS_AL_FINE);
        jp->setJumpTo(jp->jumpTo() + segnoId_);
    }
    return jp;
}

//---------------------------------------------------------
//   handleNmiCmi
//    Dolet strangely exports N.C. chord symbols as the
//    text direction "NmiCmi".
//---------------------------------------------------------

void MusicXmlParserDirection::handleNmiCmi(Measure *measure, const Fraction &tick, DelayedDirectionsList &delayedDirections) {
    if (!configuration()->inferTextType()) {
        return;
    }
    if (!wordsText_.contains(u"NmiCmi")) {
        return;
    }
    Harmony *ha = new Harmony(score_->dummy());
    HarmonyInfo *info = new HarmonyInfo(score_);
    info->setRootTpc(Tpc::TPC_INVALID);
    info->setId(-1);
    info->setTextName(u"N.C.");
    ha->setTrack(track_);
    ha->addChord(info);
    MusicXmlDelayedDirectionElement *delayedDirection = new MusicXmlDelayedDirectionElement(totalY(), ha, track_, u"above", measure, tick);
    delayedDirections.push_back(delayedDirection);
    wordsText_.replace(u"NmiCmi", u"N.C.");
}

void MusicXmlParserDirection::handleChordSym(const Fraction &tick, HarmonyMap &harmonyMap) {
    if (!configuration()->inferTextType() || placement() == "below") {
        return;
    }

    static const std::wregex re(L"^([abcdefg])(([#b♯♭])\3?)?(maj|min|m)?[769]?((add[#b♯♭]?(9|11|))|(sus[24]?))?(\\(.*\\))?$");
    String plainWords = wordsText_.simplified().toLower();
    if (!plainWords.contains(re)) {
        return;
    }

    Harmony *ha = Factory::createHarmony(score_->dummy());
    ha->setHarmony(wordsText_);
    ha->setTrack(track_);
    ha->setPlacement(placement() == u"above" ? PlacementV::ABOVE : PlacementV::BELOW);
    ha->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    ha->resetProperty(Pid::OFFSET);
    ha->setVisible(visible_);
    const HarmonyInfo *info = ha->chords().empty() ? nullptr : ha->chords().front();
    const ChordDescription *desc = info ? info->descr() : nullptr;
    HarmonyDesc newHarmonyDesc(track_, ha, nullptr);

    const int ticks = tick.ticks();
    bool insert = true;
    for (auto itr = harmonyMap.begin(); itr != harmonyMap.end(); itr++) {
        if (itr->first != ticks) {
            continue;
        }
        HarmonyDesc &foundHarmonyDesc = itr->second;

        // Don't insert if there is a matching chord symbol
        // This symbol doesn't have a fret diagram, so no need to check that here
        if (track2staff(foundHarmonyDesc.track_) == track2staff(track_) && foundHarmonyDesc.descr() == desc) {
            insert = false;
        }
    }

    if (insert) {
        harmonyMap.insert(std::pair<int, HarmonyDesc>(ticks, newHarmonyDesc));
    }
    wordsText_.clear();
}

void MusicXmlParserDirection::handleTempo(String &wordsString) {
    if (!configuration()->inferTextType()) {
        return;
    }

    // Map Sibelius' representation of note types to their SMuFL counterparts and duration types
    static const MetronomeTextMap sibeliusSyms = {{u"y", {u"<sym>metNote32ndUp</sym>", DurationType::V_32ND}},
                                                  {u"x", {u"<sym>metNote16thUp</sym>", DurationType::V_16TH}},
                                                  {u"e", {u"<sym>metNote8thUp</sym>", DurationType::V_EIGHTH}},
                                                  {u"q", {u"<sym>metNoteQuarterUp</sym>", DurationType::V_QUARTER}},
                                                  {u"h", {u"<sym>metNoteHalfUp</sym>", DurationType::V_HALF}},
                                                  {u"w", {u"<sym>metNoteWhole</sym>", DurationType::V_WHOLE}},
                                                  {u"V", {u"<sym>metNoteDoubleWholeSquare</sym>", DurationType::V_BREVE}},
                                                  {u"W", {u"<sym>metNoteDoubleWhole</sym>", DurationType::V_BREVE}}};

    static const MetronomeTextMap metTimesSyms = {
        {u"Œ", {u"<sym>metNoteQuarterUp</sym>", DurationType::V_QUARTER}},
        {u"Ó", {u"<sym>metNoteHalfUp</sym>", DurationType::V_HALF}},
    };

    MetronomeTextMap textMap;
    if (pass1_.sibOrDolet()) {
        textMap = sibeliusSyms;
    } else if (fontFamily_ == u"MetTimes Plain") {
        textMap = metTimesSyms;
    }
    if (textMap.empty()) {
        return;
    }
    String substitutions;
    for (auto &entry : textMap) {
        substitutions.append(entry.first);
    }

    // Pick up any tempo markings which may have been exported as <words>
    // eg. andante (q = c. 90)
    // Sibelius uses a symbol font with the characters 'yxeqhVwW' each drawn as a different duration
    // which we need to map to SMuFL syms
    String plainWords = MScoreTextToMusicXml::toPlainText(wordsString.simplified());

    static const std::wregex tempo(L".*([" + substitutions.toStdWString() + L"])(\\.?)(?:\\s|\\u00A0)*=[^0-9]*([0-9]+).*");
    StringList tempoMatches = plainWords.search(tempo, {1, 2, 3}, SplitBehavior::SkipEmptyParts);

    // Not a tempo
    if (tempoMatches.size() < 2) {
        return;
    }

    const String dur = tempoMatches.at(0);
    const bool dot = tempoMatches.size() == 3;
    const String val = tempoMatches.at(dot ? 2 : 1);

    const String dotStr = dot ? u"<sym>space</sym><sym>metAugmentationDot</sym>" : u"";

    static const std::wregex replace(L"(.*)[" + substitutions.toStdWString() + L"]\\.?((?:\\s|\\u00A0)*=[^0-9]*[0-9]+.*)");
    const String newStr = u"$1" + textMap.at(dur).first + dotStr + u"$2";
    plainWords.replace(replace, newStr);
    wordsString = plainWords;

    if (!val.empty() && !dur.empty()) {
        bool ok;
        double d = val.toDouble(&ok);
        TDuration duration = TDuration(textMap.at(dur).second);
        duration.setDots(dot);

        if (ok && duration.isValid()) {
            // convert fraction to beats per minute
            tpoMetro_ = 4 * duration.fraction().numerator() * d / duration.fraction().denominator();
        }
    }
}

bool MusicXmlParserDirection::isLikelySticking() {
    if (!configuration()->inferTextType()) {
        return false;
    }

    String plainWords = MScoreTextToMusicXml::toPlainText(wordsText_.simplified());
    static const std::wregex sticking(L"^[lrbLRB]+$");
    return plainWords.contains(sticking) && rehearsalText_.empty() && metroText_.empty() && tpoSound_ < 0.1;
}

bool MusicXmlParserDirection::isLikelyDynamicRange() const {
    if (!configuration()->inferTextType()) {
        return false;
    }

    String rawWordsText = wordsText_.simplified();
    static const std::regex re("(<.*?>)");
    rawWordsText.remove(re);

    // If there are two dynamics and a hyphen in a single direction node, this is most likely something like "mp-f"
    return (rawWordsText.simplified() == u"-" || rawWordsText.simplified() == u"—") && dynamicsList_.size() == 2;
}

PlayingTechniqueType MusicXmlParserDirection::getPlayingTechnique() const {
    if (!configuration()->inferTextType()) {
        return PlayingTechniqueType::Undefined;
    }

    String plainWords = MScoreTextToMusicXml::toPlainText(wordsText_.simplified()).toLower();
    plainWords.remove(std::wregex(L"[\\.,;:]+")); // remove punctuation

    // Ordered longest-first so specific phrases match before their substrings
    // (e.g. "without mute" before "mute", "con sord" before "ord")
    static const std::vector<std::pair<muse::String, PlayingTechniqueType>> textToPlayTechniqueType{
        {u"palm mute", PlayingTechniqueType::Undefined}, {u"palmmute", PlayingTechniqueType::Undefined},
        {u"without mute", PlayingTechniqueType::Open},   {u"distortion", PlayingTechniqueType::Distortion},
        {u"senza sord", PlayingTechniqueType::Open},     {u"harmonics", PlayingTechniqueType::Harmonics},
        {u"overdrive", PlayingTechniqueType::Overdrive}, {u"sul pont", PlayingTechniqueType::SulPonticello},
        {u"sul tast", PlayingTechniqueType::SulTasto},   {u"con sord", PlayingTechniqueType::Mute},
        {u"detache", PlayingTechniqueType::Detache},     {u"martele", PlayingTechniqueType::Martele},
        {u"natural", PlayingTechniqueType::Natural},     {u"no mute", PlayingTechniqueType::Open},
        {u"unmuted", PlayingTechniqueType::Open},        {u"vibrato", PlayingTechniqueType::Vibrato},
        {u"legato", PlayingTechniqueType::Legato},       {u"normal", PlayingTechniqueType::Natural},
        {u"legno", PlayingTechniqueType::ColLegno},      {u"open", PlayingTechniqueType::Open},
        {u"arco", PlayingTechniqueType::Natural},        {u"jazz", PlayingTechniqueType::JazzTone},
        {u"mute", PlayingTechniqueType::Mute},           {u"pizz", PlayingTechniqueType::Pizzicato},
        {u"sord", PlayingTechniqueType::Mute},           {u"trem", PlayingTechniqueType::Tremolo},
        {u"ord", PlayingTechniqueType::Natural},
    };

    for (const auto &technique : textToPlayTechniqueType) {
        if (plainWords.contains(technique.first)) {
            return technique.second;
        }
    }

    return PlayingTechniqueType::Undefined;
}

void MusicXmlParserDirection::handleDrumInstrument(bool isPerc, Fraction tick) const {
    if (!configuration()->inferTextType() || wordsText_.empty() || !rehearsalText_.empty() || !metroText_.empty() || tpoSound_ > 0.1 ||
        !isPerc) {
        return;
    }

    String plainWords = wordsText_;

    static const std::regex to("to ", std::regex_constants::icase);
    static const std::regex brackets("\\(.*\\)");
    plainWords.remove(to);
    plainWords.remove(brackets);

    plainWords = MScoreTextToMusicXml::toPlainText(plainWords.simplified());

    const InstrumentTemplate *it = searchTemplateForInstrNameList({plainWords}, true, false);

    // Ignore marching percussion, as these won't map correctly to the standard drumkit
    if (it && it->familyId() != u"batterie") {
        int pitch = it->useDrumset && it->drumset ? it->drumset->nextPitch(0) : 0;

        // Text under the staff will be for voice 2 and 4 even if it's in voice 1
        track_idx_t track = placement() == u"below" && !(track_ & 1) ? track_ + 1 : track_;

        InferredPercInstr instr = InferredPercInstr(pitch, track, it->id, tick);
        pass2_.addInferredPercInstr(instr);
    }
}

//---------------------------------------------------------
//   bracket
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/bracket node.
 */

void MusicXmlParserDirection::bracket(const String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                                      std::vector<MusicXmlSpannerDesc> &stops) {
    const AsciiStringView lineEnd = e_.asciiAttribute("line-end");
    const AsciiStringView lineType = e_.asciiAttribute("line-type");
    const AsciiStringView endLength = e_.asciiAttribute("end-length");
    const bool isWavy = lineType == "wavy";
    const ElementType elementType = isWavy ? ElementType::TRILL : ElementType::TEXTLINE;
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.getSpanner({elementType, number});
    if (type == "start") {
        if (spannerExists(starts, number, elementType)) {
            logger_->logError(String(u"%1 with number %2 already started").arg(TConv::userName(elementType).translated()).arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        SLine *sline = spdesc.isStopped ? spdesc.sp : 0;
        if ((sline && sline->isTrill()) || (!sline && isWavy)) {
            if (!sline) {
                sline = Factory::createTrill(score_->dummy());
            }
            Trill *trill = toTrill(sline);
            trill->setTrillType(TrillType::PRALLPRALL_LINE);

            if (!lineEnd.empty() && lineEnd != "none") {
                logger_->logError(String(u"line-end not supported for line-type \"wavy\""));
            }
        } else if ((sline && sline->isTextLine()) || (!sline && !isWavy)) {
            if (!sline) {
                sline = new TextLine(score_->dummy());
                sline->setSystemFlag(systemDirection_);
            }
            TextLine *textLine = toTextLine(sline);
            // if (placement.empty()) placement = "above";  // TODO ? set default

            if (!endLength.empty()) {
                double length = endLength.toDouble();
                textLine->setBeginHookHeight(Spatium(lineEnd == "both" ? length / 20 : length / 10));
                textLine->setPropertyFlags(Pid::BEGIN_HOOK_HEIGHT, PropertyFlags::UNSTYLED);
            }
            if (lineEnd == "up") {
                textLine->setBeginHookType(HookType::HOOK_90);
            } else if (lineEnd == "down") {
                textLine->setBeginHookType(HookType::HOOK_90);
                textLine->setBeginHookHeight(-1 * textLine->beginHookHeight());
            } else if (lineEnd == "both") {
                textLine->setBeginHookType(HookType::HOOK_90T);
            } else if (lineEnd == "arrow") {
                textLine->setBeginHookType(HookType::ARROW_FILLED);
            } else if (lineEnd == "none") {
                textLine->setBeginHookType(HookType::NONE);
            }

            // hack: combine with a previous words element
            if (!wordsText_.empty()) {
                // TextLine supports only limited formatting, remove all (compatible with 1.3)
                textLine->setBeginText(MScoreTextToMusicXml::toPlainText(wordsText_));
                wordsText_.clear();
            }

            if (lineType == "solid") {
                textLine->setLineStyle(LineType::SOLID);
                textLine->setPropertyFlags(Pid::LINE_STYLE, PropertyFlags::UNSTYLED);
            } else if (lineType == "dashed") {
                textLine->setLineStyle(LineType::DASHED);
                textLine->setPropertyFlags(Pid::LINE_STYLE, PropertyFlags::UNSTYLED);
            } else if (lineType == "dotted") {
                textLine->setLineStyle(LineType::DOTTED);
                textLine->setPropertyFlags(Pid::LINE_STYLE, PropertyFlags::UNSTYLED);
            } else if (lineType != "wavy") {
                logger_->logError(String(u"unsupported line-type: %1").arg(String::fromAscii(lineType.ascii())), &e_);
            }
            colorItem(textLine, Color::fromString(e_.asciiAttribute("color").ascii()));
        }

        starts.push_back(MusicXmlSpannerDesc(sline, elementType, number));
    } else if (type == "stop") {
        if (spannerExists(stops, number, elementType)) {
            logger_->logError(String(u"%1 with number %2 already stopped").arg(TConv::userName(elementType).translated()).arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        SLine *sline = spdesc.isStarted ? spdesc.sp : 0;
        if ((sline && sline->isTrill()) || (!sline && isWavy)) {
            if (!sline) {
                sline = new Trill(score_->dummy());
            }
            if (!lineEnd.empty() && lineEnd != "none") {
                logger_->logError(String(u"line-end not supported for line-type \"wavy\""));
            }
        } else if ((sline && sline->isTextLine()) || (!sline && !isWavy)) {
            if (!sline) {
                sline = new TextLine(score_->dummy());
            }
            TextLine *textLine = toTextLine(sline);

            if (!endLength.empty()) {
                double length = endLength.toDouble();
                textLine->setEndHookHeight(Spatium(lineEnd == "both" ? length / 20 : length / 10));
                textLine->setPropertyFlags(Pid::END_HOOK_HEIGHT, PropertyFlags::UNSTYLED);
            }
            if (lineEnd == "up") {
                textLine->setEndHookType(HookType::HOOK_90);
            } else if (lineEnd == "down") {
                textLine->setEndHookType(HookType::HOOK_90);
                textLine->setEndHookHeight(-1 * textLine->endHookHeight());
            } else if (lineEnd == "both") {
                textLine->setEndHookType(HookType::HOOK_90T);
            } else if (lineEnd == "arrow") {
                textLine->setEndHookType(HookType::ARROW_FILLED);
            } else if (lineEnd == "none") {
                textLine->setEndHookType(HookType::NONE);
            }
        }

        stops.push_back(MusicXmlSpannerDesc(sline, elementType, number));
    }
    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   dashes
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/dashes node.
 */

void MusicXmlParserDirection::dashes(const String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                                     std::vector<MusicXmlSpannerDesc> &stops) {
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.getSpanner({ElementType::HAIRPIN, number});
    if (type == u"start") {
        if (spannerExists(starts, number, ElementType::HAIRPIN)) {
            logger_->logError(String(u"hairpin with number %1 already started").arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        TextLineBase *b = spdesc.isStopped ? toTextLineBase(spdesc.sp) : Factory::createTextLine(score_->dummy());
        // if (placement.empty()) placement = "above";  // TODO ? set default

        // hack: combine with a previous words element
        if (!wordsText_.empty()) {
            // TextLine supports only limited formatting, remove all (compatible with 1.3)
            String simplifiedText = MScoreTextToMusicXml::toPlainText(wordsText_).simplified();
            if (isLikelyTempoLine(track_)) {
                b = Factory::createGradualTempoChange(score_->dummy());
                GradualTempoChangeType gtc = getTempoChangeTypeFromString(simplifiedText);
                toGradualTempoChange(b)->setTempoChangeType(gtc);
            }
            b->setBeginText(simplifiedText);
            wordsText_.clear();
        }

        b->setBeginHookType(HookType::NONE);
        b->setEndHookType(HookType::NONE);
        b->setLineStyle(LineType::DASHED);
        // TODO brackets and dashes now share the same storage
        // because they both use ElementType::TEXTLINE
        // use MusicXML specific type instead
        starts.push_back(MusicXmlSpannerDesc(b, ElementType::TEXTLINE, number));
    } else if (type == u"stop") {
        if (spannerExists(stops, number, ElementType::HAIRPIN)) {
            logger_->logError(String(u"hairpin with number %1 already stopped").arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        TextLineBase *b = spdesc.isStarted ? toTextLineBase(spdesc.sp) : Factory::createTextLine(score_->dummy());
        stops.push_back(MusicXmlSpannerDesc(b, ElementType::TEXTLINE, number));
    }
    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   octaveShift
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/octave-shift node.
 */

void MusicXmlParserDirection::octaveShift(const String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                                          std::vector<MusicXmlSpannerDesc> &stops) {
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.getSpanner({ElementType::OTTAVA, number});
    if (type == u"up" || type == u"down") {
        if (spannerExists(starts, number, ElementType::OTTAVA)) {
            logger_->logError(String(u"ottava with number %1 already started").arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        int ottavasize = e_.intAttribute("size");
        if (!(ottavasize == 8 || ottavasize == 15 || ottavasize == 22)) {
            logger_->logError(String(u"unknown octave-shift size %1").arg(ottavasize), &e_);
        } else {
            Ottava *o = spdesc.isStopped ? toOttava(spdesc.sp) : Factory::createOttava(score_->dummy());

            if (type == u"down") {
                placement_ = placement_.empty() ? u"above" : placement_;
                if (ottavasize == 8) {
                    o->setOttavaType(OttavaType::OTTAVA_8VA);
                } else if (ottavasize == 15) {
                    o->setOttavaType(OttavaType::OTTAVA_15MA);
                } else if (ottavasize == 22) {
                    o->setOttavaType(OttavaType::OTTAVA_22MA);
                }
            } else if (type == u"up") {
                placement_ = placement_.empty() ? u"below" : placement_;
                if (ottavasize == 8) {
                    o->setOttavaType(OttavaType::OTTAVA_8VB);
                } else if (ottavasize == 15) {
                    o->setOttavaType(OttavaType::OTTAVA_15MB);
                } else if (ottavasize == 22) {
                    o->setOttavaType(OttavaType::OTTAVA_22MB);
                }
            }

            colorItem(o, Color::fromString(e_.asciiAttribute("color").ascii()));

            starts.push_back(MusicXmlSpannerDesc(o, ElementType::OTTAVA, number));
        }
    } else if (type == u"stop") {
        if (spannerExists(stops, number, ElementType::OTTAVA)) {
            logger_->logError(String(u"ottava with number %1 already stopped").arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        Ottava *o = spdesc.isStarted ? toOttava(spdesc.sp) : Factory::createOttava(score_->dummy());
        stops.push_back(MusicXmlSpannerDesc(o, ElementType::OTTAVA, number));
    }
    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   pedal
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/pedal node.
 */

void MusicXmlParserDirection::pedal(const String &type, const int /* number */, std::vector<MusicXmlSpannerDesc> &starts,
                                    std::vector<MusicXmlSpannerDesc> &stops) {
    const int number{0};
    AsciiStringView line = e_.asciiAttribute("line");
    String sign = e_.attribute("sign");
    const Color color = Color::fromString(e_.asciiAttribute("color").ascii());

    // We have found that many exporters omit "sign" even when one is originally present,
    // therefore we will default to "yes", even though this is technically against the spec.
    bool overrideDefaultSign = true; // TODO: set this flag based on the exporting software
    if (sign.empty()) {
        if (line != "yes" || (overrideDefaultSign && type == "start")) {
            sign = u"yes"; // MusicXML 2.0 compatibility
        } else if (line == "yes") {
            sign = u"no"; // MusicXML 2.0 compatibility
        }
    }
    MusicXmlExtendedSpannerDesc &spdesc = pass2_.getSpanner({ElementType::PEDAL, number});
    if (type == u"start" || type == u"resume" || type == u"sostenuto") {
        if (spdesc.isStarted && !spdesc.isStopped) {
            // Previous pedal unterminated
            // if previous pedal was a change, create a new change instead of a new pedal start
            if (toPedal(spdesc.sp)->beginHookType() == HookType::HOOK_45) {
                Pedal *p = Factory::createPedal(score_->dummy());
                p->setBeginHookType(HookType::HOOK_45);
                p->setEndHookType(HookType::HOOK_90);
                if (line == "yes") {
                    p->setLineVisible(true);
                } else {
                    p->setLineVisible(false);
                }
                if (sign == u"no") {
                    p->setBeginText(u"");
                    p->setContinueText(u"");
                    p->setEndText(u"");
                }
                colorItem(p, color);
                starts.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
                e_.skipCurrentElement();

                return;
            } else {
                // likely an unrecorded "discontinue", so delete the line.
                pass2_.deleteHandledSpanner(spdesc.sp);
                spdesc.isStarted = false;
            }
        }
        Pedal *p = spdesc.isStopped ? toPedal(spdesc.sp) : Factory::createPedal(score_->dummy());
        if (line == "yes") {
            p->setLineVisible(true);
        } else {
            p->setLineVisible(false);
        }
        if (!p->lineVisible() || sign == u"yes") {
            p->setBeginText(score_->style().styleSt(Sid::pedalText));
            p->setContinueText(score_->style().styleSt(Sid::pedalContinueText));
            if (type == "sostenuto") {
                p->setBeginText(u"<sym>keyboardPedalSost</sym>");
                p->setContinueText(u"<sym>keyboardPedalParensLeft</sym><sym>keyboardPedalSost</sym><sym>keyboardPedalParensRight</sym>");
            }
        } else {
            p->setBeginText(u"");
            p->setContinueText(u"");
            p->setBeginHookType(type == "resume" ? HookType::NONE : HookType::HOOK_90);
        }
        p->setEndHookType(HookType::NONE);
        colorItem(p, color);
        starts.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
    } else if (type == u"stop" || type == u"discontinue") {
        Pedal *p = spdesc.isStarted ? toPedal(spdesc.sp) : Factory::createPedal(score_->dummy());
        if (line == "yes") {
            p->setLineVisible(true);
        } else if (line == "no") {
            p->setLineVisible(false);
        }
        if ((!p->lineVisible() || sign == u"yes") && p->endHookType() == HookType::NONE) {
            p->setEndText(u"<sym>keyboardPedalUp</sym>");
        } else {
            p->setEndHookType(type == "discontinue" ? HookType::NONE : HookType::HOOK_90);
        }
        stops.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
    } else if (type == "change") {
        // pedal change is implemented as two separate pedals
        // first stop the first one
        if (spdesc.isStarted && !spdesc.isStopped) {
            Pedal *p = toPedal(spdesc.sp);
            p->setEndHookType(HookType::HOOK_45);
            if (line == "yes") {
                p->setLineVisible(true);
            } else if (line == "no") {
                p->setLineVisible(false);
            }
            stops.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
        } else {
            logger_->logError(String(u"\"change\" type pedal created without existing pedal"), &e_);
        }
        // then start a new one
        Pedal *p = Factory::createPedal(score_->dummy());
        p->setBeginHookType(HookType::HOOK_45);
        p->setEndHookType(HookType::HOOK_90);
        if (line == "yes") {
            p->setLineVisible(true);
        } else {
            p->setLineVisible(false);
        }
        if (sign == u"no") {
            p->setBeginText(u"");
            p->setContinueText(u"");
        }
        colorItem(p, color);
        starts.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
    } else if (type == "continue") {
        // ignore
    } else {
        LOGD("unknown pedal type %s", muPrintable(type));
    }

    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   wedge
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/wedge node.
 */

void MusicXmlParserDirection::wedge(const String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                                    std::vector<MusicXmlSpannerDesc> &stops) {
    AsciiStringView niente = e_.asciiAttribute("niente");
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.getSpanner({ElementType::HAIRPIN, number});
    if (type == "crescendo" || type == "diminuendo") {
        if (spannerExists(starts, number, ElementType::HAIRPIN)) {
            logger_->logError(String(u"hairpin with number %1 already started").arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        Hairpin *h = spdesc.isStopped ? toHairpin(spdesc.sp) : Factory::createHairpin(score_->dummy());
        h->setHairpinType(type == "crescendo" ? HairpinType::CRESC_HAIRPIN : HairpinType::DIM_HAIRPIN);
        if (niente == "yes") {
            h->setHairpinCircledTip(true);
        }
        colorItem(h, Color::fromString(e_.asciiAttribute("color").ascii()));
        AsciiStringView lineType = e_.asciiAttribute("line-type");
        if (lineType == "dashed") {
            h->setLineStyle(LineType::DASHED);
        } else if (lineType == "dotted") {
            h->setLineStyle(LineType::DOTTED);
        }
        const String spread = e_.attribute("spread");
        if (!spread.empty() && configuration()->importLayout()) {
            const Spatium val(spread.toDouble() / 10.0);
            h->setHairpinHeight(val);
        }
        starts.push_back(MusicXmlSpannerDesc(h, ElementType::HAIRPIN, number));
    } else if (type == "stop") {
        if (spannerExists(stops, number, ElementType::HAIRPIN)) {
            logger_->logError(String(u"hairpin with number %1 already stopped").arg(number), &e_);
            e_.skipCurrentElement();
            return;
        }
        Hairpin *h = spdesc.isStarted ? toHairpin(spdesc.sp) : Factory::createHairpin(score_->dummy());
        if (niente == "yes") {
            h->setHairpinCircledTip(true);
        }
        stops.push_back(MusicXmlSpannerDesc(h, ElementType::HAIRPIN, number));
    }
    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   toString
//---------------------------------------------------------

String MusicXmlExtendedSpannerDesc::toString() const {
    String spStr = sp ? String::fromStdString(sp->eid().toStdString()) : u"null";
    return String(u"sp %1 tp %2 tick2 %3 track2 %4 %5 %6")
        .arg(spStr, tick2.toString())
        .arg(static_cast<int>(track2))
        .arg(isStarted ? u"started" : u"", isStopped ? u"stopped" : u"");
}

//---------------------------------------------------------
//   addSpanner
//---------------------------------------------------------

void MusicXmlParserPass2::addSpanner(const MusicXmlSpannerDesc &d) {
    MusicXmlExtendedSpannerDesc &spdesc = getSpanner(d);
    spdesc.sp = d.sp;
}

//---------------------------------------------------------
//   getSpanner
//---------------------------------------------------------

MusicXmlExtendedSpannerDesc &MusicXmlParserPass2::getSpanner(const MusicXmlSpannerDesc &d) {
    if (d.tp == ElementType::HAIRPIN && 0 <= d.nr && d.nr < MAX_NUMBER_LEVEL) {
        return hairpins_[d.nr];
    } else if (d.tp == ElementType::OTTAVA && 0 <= d.nr && d.nr < MAX_NUMBER_LEVEL) {
        return ottavas_[d.nr];
    } else if (d.tp == ElementType::PEDAL && 0 == d.nr) {
        return pedal_;
    } else if ((d.tp == ElementType::TEXTLINE || d.tp == ElementType::TRILL) && 0 <= d.nr && d.nr < MAX_NUMBER_LEVEL) {
        return brackets_[d.nr];
    }
    logger_->logError(String(u"invalid number %1").arg(d.nr + 1), &e_);
    return dummyNewMusicXmlSpannerDesc_;
}

//---------------------------------------------------------
//   clearSpanner
//---------------------------------------------------------

void MusicXmlParserPass2::clearSpanner(const MusicXmlSpannerDesc &d) {
    MusicXmlExtendedSpannerDesc &spdesc = getSpanner(d);
    spdesc = {};
}

//---------------------------------------------------------
//   deleteHandledSpanner
//---------------------------------------------------------
/**
 Delete a spanner that's already been added to _spanners.
 This is used to remove pedal markings that are never stopped
 */

void MusicXmlParserPass2::deleteHandledSpanner(SLine *const &spanner) {
    muse::remove(spanners_, spanner);
    delete spanner;
}

InferredPercInstr MusicXmlParserPass2::inferredPercInstr(const Fraction &tick, const track_idx_t trackIdx) {
    InferredPercInstr instr = InferredPercInstr();

    for (InferredPercList::iterator iter = inferredPerc_.begin(); iter != inferredPerc_.end();) {
        if (iter->tick == tick && iter->track == trackIdx) {
            instr = *iter;
            iter = inferredPerc_.erase(iter);
            break;
        } else {
            ++iter;
        }
    }

    return instr;
}

//---------------------------------------------------------
//   metronome
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/metronome node.
 Convert to text and set r to calculated tempo.
 */

String MusicXmlParserDirection::metronome(double &r) {
    r = 0;
    String tempoText;
    String perMinute;
    bool parenth = e_.asciiAttribute("parentheses") == "yes";

    if (parenth) {
        tempoText += u"(";
    }

    TDuration dur1;
    TDuration dur2;

    while (e_.readNextStartElement()) {
        if (e_.name() == "metronome-note" || e_.name() == "metronome-relation") {
            skipLogCurrElem();
            continue;
        }
        String txt = e_.readText();
        if (e_.name() == "beat-unit") {
            // set first dur that is still invalid
            ByteArray ba = txt.toAscii();
            if (!dur1.isValid()) {
                dur1.setType(TConv::fromXml(ba.constChar(), DurationType::V_INVALID));
            } else if (!dur2.isValid()) {
                dur2.setType(TConv::fromXml(ba.constChar(), DurationType::V_INVALID));
            }
        } else if (e_.name() == "beat-unit-dot") {
            if (dur2.isValid()) {
                dur2.setDots(1);
            } else if (dur1.isValid()) {
                dur1.setDots(1);
            }
        } else if (e_.name() == "per-minute") {
            perMinute = txt;
        } else {
            skipLogCurrElem();
        }
    }

    if (dur1.isValid()) {
        tempoText += TempoText::duration2tempoTextString(dur1);
    }
    if (dur2.isValid()) {
        tempoText += u" = ";
        tempoText += TempoText::duration2tempoTextString(dur2);
    } else if (!perMinute.empty()) {
        tempoText += u" = ";
        tempoText += perMinute;
    }
    if (dur1.isValid() && !dur2.isValid() && !perMinute.empty()) {
        bool ok;
        double d = perMinute.toDouble(&ok);
        if (ok) {
            // convert fraction to beats per minute
            r = 4 * dur1.fraction().numerator() * d / dur1.fraction().denominator();
        }
    }

    if (parenth) {
        tempoText += u")";
    }

    return tempoText;
}

//---------------------------------------------------------
//   determineBarLineType
//---------------------------------------------------------

static bool determineBarLineType(const String &barStyle, const String &repeat, BarLineType &type, bool &visible,
                                 const MusicXmlExporterSoftware &exporter) {
    // set defaults
    type = BarLineType::NORMAL;
    visible = true;

    if (barStyle == u"light-heavy" && repeat == u"backward") {
        type = BarLineType::END_REPEAT;
    } else if (barStyle == u"heavy-light" && repeat == u"forward") {
        type = BarLineType::START_REPEAT;
    } else if (barStyle == u"light-heavy" && repeat == u"forward" && exporter == MusicXmlExporterSoftware::NOTEFLIGHT) {
        type = BarLineType::START_REPEAT;
    } else if (barStyle == u"light-heavy" && repeat.empty()) {
        type = BarLineType::END;
    } else if (barStyle == u"heavy-light" && repeat.empty()) {
        type = BarLineType::REVERSE_END;
    } else if (barStyle == u"regular") {
        type = BarLineType::NORMAL;
    } else if (barStyle == u"dashed") {
        type = BarLineType::BROKEN;
    } else if (barStyle == u"dotted") {
        type = BarLineType::DOTTED;
    } else if (barStyle == u"light-light") {
        type = BarLineType::DOUBLE;
        /*
        } else if (barStyle == "heavy-light") {
         ;
         */
    } else if (barStyle == u"heavy-heavy") {
        type = BarLineType::DOUBLE_HEAVY;
    } else if (barStyle == u"heavy") {
        type = BarLineType::HEAVY;
    } else if (barStyle == u"none") {
        visible = false;
    } else if (barStyle.empty()) {
        if (repeat == u"backward") {
            type = BarLineType::END_REPEAT;
        } else if (repeat == u"forward") {
            type = BarLineType::START_REPEAT;
        } else {
            return false;
        }
    } else if ((barStyle == u"tick") || (barStyle == u"short")) {
        // handled later (as normal barline with different parameters)
    } else {
        LOGD("unsupported bar type <%s>", muPrintable(barStyle)); // TODO
        return false;
    }

    return true;
}

//---------------------------------------------------------
//   barline
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/barline node.
 */

/*
 Following barline types are automatically generated by MuseScore in an EndBarLine segment at the end of a measure:
 - normal (excluding tick and short)
 - start repeat
 - end-start repeat
 - end repeat
 - final (at the end of the score only)
 The other barline types can also be in an EndBarLine segment at the end of a measure, but are NOT generated
 Mid-measure barlines are in a BarLine segment and are NOT generated
 Following barline types can only be at the end of a measure:
 - start repeat
 - end-start repeat
 - end repeat
 - final
Regular barlines should not be added at the start or end of a measure, as that could lead to inconsistent behaviour.
 */

void MusicXmlParserPass2::barline(const String &partId, Measure *measure, const Fraction &tick) {
    AsciiStringView loc = e_.asciiAttribute("location");
    if (loc.empty()) {
        loc = "right";
    }
    // Place barline in correct place
    Fraction locTick = tick;
    if (loc == "left") {
        locTick = measure->tick();
    } else if (loc == "right") {
        locTick = measure->endTick();
    }

    const String codaLabel = e_.attribute("coda");
    const String segnoLabel = e_.attribute("segno");

    String barStyle;
    Color barlineColor;
    String endingNumber;
    String endingType;
    Color endingColor;
    String endingText;
    String repeat;
    bool printEnding = true;

    while (e_.readNextStartElement()) {
        if (e_.name() == "bar-style") {
            barlineColor = Color::fromString(e_.asciiAttribute("color").ascii());
            barStyle = e_.readText();
        } else if (e_.name() == "segno") {
            const Color segnoColor = Color::fromString(e_.asciiAttribute("color").ascii());
            const AsciiStringView segnoSymbol = e_.asciiAttribute("smufl");
            Measure *markedMeasure = (loc == "right") ? measure->nextMeasure() : measure;
            if (markedMeasure == nullptr) {
                logger_->logError(u"coda or segno marker cannot be placed at the end of the score", &e_);
                e_.skipCurrentElement();
                continue;
            }
            Marker *m = Factory::createMarker(markedMeasure);
            m->setMarkerType(MarkerType::SEGNO);
            if (!segnoSymbol.empty()) {
                m->setXmlText(u"<sym>" + String::fromAscii(segnoSymbol.ascii()) + u"</sym>");
            }
            if (!segnoLabel.empty()) {
                m->setLabel(segnoLabel);
            }
            if (segnoColor.isValid()) {
                colorItem(m, segnoColor);
            }
            addElemOffset(m, 0, u"above", markedMeasure, markedMeasure->tick());
            e_.skipCurrentElement();
        } else if (e_.name() == "coda") {
            const Color codaColor = Color::fromString(e_.asciiAttribute("color").ascii());
            const AsciiStringView codaSymbol = e_.asciiAttribute("smufl");
            Measure *markedMeasure = (loc == "right") ? measure->nextMeasure() : measure;
            if (markedMeasure == nullptr) {
                logger_->logError(u"coda or segno marker cannot be placed at the end of the score", &e_);
                e_.skipCurrentElement();
                continue;
            }
            Marker *m = Factory::createMarker(markedMeasure);
            m->setMarkerType(MarkerType::CODA);
            if (!codaSymbol.empty()) {
                m->setXmlText(u"<sym>" + String::fromAscii(codaSymbol.ascii()) + u"</sym>");
            }
            if (!codaLabel.empty()) {
                m->setLabel(codaLabel);
            }
            if (codaColor.isValid()) {
                colorItem(m, codaColor);
            }
            addElemOffset(m, 0, u"above", markedMeasure, markedMeasure->tick());
            e_.skipCurrentElement();
        } else if (e_.name() == "fermata") {
            const Color fermataColor = Color::fromString(e_.asciiAttribute("color").ascii());
            const String fermataType = e_.attribute("type");
            SegmentType st = SegmentType::BarLine;
            if (locTick == measure->endTick()) {
                st = SegmentType::EndBarLine;
            } else if (locTick == measure->tick()) {
                st = SegmentType::BeginBarLine;
            }
            Segment *const segment = measure->getSegment(st, locTick);
            const track_idx_t track = pass1_.trackForPart(partId);
            Fermata *fermata = Factory::createFermata(segment);
            fermata->setSymId(convertFermataToSymId(e_.readText()));
            fermata->setTrack(track);
            segment->add(fermata);
            colorItem(fermata, fermataColor);
            if (fermataType == u"inverted") {
                fermata->setPlacement(PlacementV::BELOW);
                fermata->resetProperty(Pid::OFFSET);
            } else if (fermataType.empty()) {
                fermata->setPlacement(fermata->propertyDefault(Pid::PLACEMENT).value<PlacementV>());
            }

            // Terminate tempo lines
            const InferredTempoLineStack &lines = getInferredTempoLine();
            terminateInferredLine(std::vector<TextLineBase *>(lines.begin(), lines.end()), locTick, track,
                                  configuration()->inferTextType());
        } else if (e_.name() == "ending") {
            endingNumber = e_.attribute("number");
            endingType = e_.attribute("type");
            endingColor = Color::fromString(e_.asciiAttribute("color").ascii());
            printEnding = e_.asciiAttribute("print-object") != "no";
            endingText = e_.readText();
        } else if (e_.name() == "repeat") {
            repeat = e_.attribute("direction");
            int count = e_.attribute("times").toInt();
            if (!count) {
                count = 2;
            }
            measure->setRepeatCount(count);
            bool repeatBarTips = !e_.attribute("winged").empty() && e_.attribute("winged") != "none";
            score_->undoChangeStyleVal(Sid::repeatBarTips, repeatBarTips);
            e_.skipCurrentElement();
        } else {
            skipLogCurrElem();
        }
    }

    BarLineType type = BarLineType::NORMAL;
    bool visible = true;
    if (determineBarLineType(barStyle, repeat, type, visible, pass1_.exporterSoftware())) {
        const track_idx_t track = pass1_.trackForPart(partId);
        if (type & BarLineType::START_REPEAT) {
            // combine start_repeat flag with current state initialized during measure parsing
            measure->setRepeatStart(true);
        } else if (type & BarLineType::END_REPEAT) {
            // combine end_repeat flag with current state initialized during measure parsing
            measure->setRepeatEnd(true);
        } else {
            if (barStyle != "regular" || barlineColor.isValid() || loc == "middle") {
                // Add barline to the first voice of every staff in the part,
                // and span every barline except the last
                const Part *part = pass1_.getPart(partId);
                staff_idx_t nstaves = part->nstaves();
                for (staff_idx_t i = 0; i < nstaves; ++i) {
                    const Staff *staff = part->staff(i);
                    const bool spanStaff = nstaves > 1 ? i < nstaves - 1 : staff->barLineSpan();
                    track_idx_t currentTrack = track + (i * VOICES);
                    auto b = createBarline(measure->score(), currentTrack, type, visible, barStyle, spanStaff);
                    colorItem(b.get(), barlineColor);
                    addBarlineToMeasure(measure, locTick, std::move(b));
                }
            }
        }
    }

    doEnding(partId, measure, endingNumber, endingType, endingColor, endingText, printEnding);
}

//---------------------------------------------------------
//   findRedundantVolta
//---------------------------------------------------------
static Volta *findRedundantVolta(const track_idx_t track, const Measure *measure) {
    auto spanners = measure->score()->spannerMap().findOverlapping(measure->tick().ticks(), measure->endTick().ticks());
    for (auto spanner : spanners) {
        if (spanner.value->isVolta() && track2staff(spanner.value->track()) != track2staff(track)) {
            return toVolta(spanner.value);
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   doEnding
//---------------------------------------------------------

void MusicXmlParserPass2::doEnding(const String &partId, Measure *measure, const String &number, const String &type, const Color color,
                                   const String &text, const bool print) {
    if (!(number.empty() && type.empty())) {
        if (number.empty()) {
            logger_->logError(u"empty ending number", &e_);
        } else if (type.empty()) {
            logger_->logError(u"empty ending type", &e_);
        } else {
            StringList sl = number.split(u',', SkipEmptyParts);
            std::vector<int> iEndingNumbers;
            bool unsupported = false;
            for (const String &s : sl) {
                int iEndingNumber = s.toInt();
                if (iEndingNumber <= 0) {
                    unsupported = true;
                    break;
                }
                iEndingNumbers.push_back(iEndingNumber);
            }

            if (unsupported) {
                logger_->logError(String(u"unsupported ending number '%1'").arg(number), &e_);
            } else {
                // Ignore if it is hidden and redundant
                Volta *redundantVolta = findRedundantVolta(pass1_.trackForPart(partId), measure);
                if (!print && redundantVolta) {
                    logger_->logDebugInfo(u"Ignoring redundant hidden Volta", &e_);
                } else if (type == u"start") {
                    Volta *volta = Factory::createVolta(score_->dummy());
                    volta->setTrack(pass1_.trackForPart(partId));
                    volta->setText(text.empty() ? number : text);
                    // LVIFIX TODO also support endings "1 - 3"
                    volta->endings().clear();
                    muse::join(volta->endings(), iEndingNumbers);
                    volta->setTick(measure->tick());
                    score_->addElement(volta);
                    lastVolta_ = volta;
                    volta->setVisible(print);
                    colorItem(volta, color);
                } else if (type == u"stop") {
                    if (lastVolta_) {
                        lastVolta_->setVoltaType(Volta::Type::CLOSED);
                        lastVolta_->setTick2(measure->tick() + measure->ticks());
                        // Assume print-object was handled at the start
                        lastVolta_ = 0;
                    } else if (!redundantVolta) {
                        logger_->logError(u"ending stop without start", &e_);
                    }
                } else if (type == u"discontinue") {
                    if (lastVolta_) {
                        lastVolta_->setVoltaType(Volta::Type::OPEN);
                        lastVolta_->setTick2(measure->tick() + measure->ticks());
                        // Assume print-object was handled at the start
                        lastVolta_ = 0;
                    } else if (!redundantVolta) {
                        logger_->logError(u"ending discontinue without start", &e_);
                    }
                } else {
                    logger_->logError(String(u"unsupported ending type '%1'").arg(type), &e_);
                }

                // Delete any hidden redundant voltas before
                while (redundantVolta && !redundantVolta->visible()) {
                    score_->removeElement(redundantVolta);
                    delete redundantVolta;
                    redundantVolta = findRedundantVolta(pass1_.trackForPart(partId), measure);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   addSymToSig
//---------------------------------------------------------

/**
 Add a symbol defined as key-step \a step , -alter \a alter and -accidental \a accid to \a sig.
 */

static void addSymToSig(KeySigEvent &sig, const String &step, const String &alter, const String &accid, const String &smufl) {
    //LOGD("addSymToSig(step '%s' alt '%s' acc '%s')",
    //       muPrintable(step), muPrintable(alter), muPrintable(accid));

    SymId id = musicXmlString2accSymId(accid, smufl);

    if (id == SymId::noSym) {
        bool ok;
        double d;
        d = alter.toDouble(&ok);
        AccidentalType accTpAlter = ok ? microtonalGuess(d) : AccidentalType::NONE;
        String s = accidentalType2MusicXmlString(accTpAlter);
        if (s == u"other") {
            s = accidentalType2SmuflMusicXmlString(accTpAlter);
        }
        id = musicXmlString2accSymId(s);
    }

    if (step.size() == 1 && id != SymId::noSym) {
        const String table = u"CDEFGAB";
        if (table.contains(step)) {
            CustDef cd;
            cd.degree = int(table.indexOf(step));
            cd.sym = id;
            sig.customKeyDefs().push_back(cd);
            sig.setCustom(true);
        }
    }
}

//---------------------------------------------------------
//   addKey
//---------------------------------------------------------

/**
 Add a KeySigEvent to the score.
 */

static void addKey(const KeySigEvent key, const Color keyColor, const bool printObj, Score *score, Measure *measure,
                   const staff_idx_t staffIdx, const Fraction &tick) {
    Key oldkey = score->staff(staffIdx)->key(tick);
    // TODO only if different custom key ?
    if (oldkey != key.key() || key.custom() || key.isAtonal()) {
        // new key differs from key in effect at this tick
        Segment *s = measure->getSegment(SegmentType::KeySig, tick);
        KeySig *keysig = Factory::createKeySig(s);
        keysig->setTrack(staffIdx * VOICES);
        keysig->setKeySigEvent(key);
        keysig->setVisible(printObj);
        colorItem(keysig, keyColor);
        s->add(keysig);
        //currKeySig->setKeySigEvent(key);
    }
}

//---------------------------------------------------------
//   flushAlteredTone
//---------------------------------------------------------

/**
 If a valid key-step, -alter, -accidental combination has been read,
 convert it to a key symbol and add to the key.
 Clear key-step, -alter, -accidental.
 */

static void flushAlteredTone(KeySigEvent &kse, String &step, String &alt, String &acc, String &smufl) {
    //LOGD("flushAlteredTone(step '%s' alt '%s' acc '%s')",
    //       muPrintable(step), muPrintable(alt), muPrintable(acc));

    if (step.empty() && alt.empty() && acc.empty()) {
        return; // nothing to do
    }
    // step and alt are required, but also accept step and acc
    if (!step.empty() && (!alt.empty() || !acc.empty())) {
        addSymToSig(kse, step, alt, acc, smufl);
    } else {
        LOGD("flushAlteredTone invalid combination of step '%s' alt '%s' acc '%s')", muPrintable(step), muPrintable(alt),
             muPrintable(acc)); // TODO
    }

    // clean up
    step.clear();
    alt.clear();
    acc.clear();
}

//---------------------------------------------------------
//   key
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/key node.
 */

// TODO: check currKeySig handling

void MusicXmlParserPass2::key(const String &partId, Measure *measure, const Fraction &tick) {
    String strKeyno = e_.attribute("number");
    int keyno = -1; // assume no number (see below)
    if (!strKeyno.empty()) {
        keyno = pass1_.getMusicXmlPart(partId).staffNumberToIndex(strKeyno.toInt());
        if (keyno < 0) {
            // conversion error (-1), assume staff 0
            logger_->logError(String(u"invalid key number '%1'").arg(strKeyno), &e_);
            keyno = 0;
        }
    }
    const bool printObject = e_.asciiAttribute("print-object") != "no";
    const Color keyColor = Color::fromString(e_.asciiAttribute("color").ascii());

    // for custom keys, a single altered tone is described by
    // key-step (required),  key-alter (required) and key-accidental (optional)
    // none, one or more altered tone may be present
    // a simple state machine is required to detect them
    KeySigEvent key;
    String keyStep;
    String keyAlter;
    String keyAccidental;
    String smufl;

    while (e_.readNextStartElement()) {
        if (e_.name() == "fifths") {
            Key tKey = Key(e_.readInt());
            Key cKey = tKey;
            Interval v = pass1_.getPart(partId)->instrument()->transpose();
            if (!v.isZero() && !score_->style().styleB(Sid::concertPitch)) {
                cKey = Transpose::transposeKey(tKey, v);
                // if there are more than 6 accidentals in transposing key, it cannot be PreferSharpFlat::AUTO
                Part *part = pass1_.getPart(partId);
                if ((tKey > 6 || tKey < -6) && part->preferSharpFlat() == PreferSharpFlat::AUTO) {
                    part->setPreferSharpFlat(PreferSharpFlat::NONE);
                }
            }
            key.setConcertKey(cKey);
            key.setKey(tKey);
        } else if (e_.name() == "mode") {
            key.setMode(TConv::fromXml(e_.readText().toAscii().constChar(), KeyMode::UNKNOWN));
        } else if (e_.name() == "cancel") {
            skipLogCurrElem(); // TODO ??
        } else if (e_.name() == "key-step") {
            flushAlteredTone(key, keyStep, keyAlter, keyAccidental, smufl);
            keyStep = e_.readText();
        } else if (e_.name() == "key-alter") {
            keyAlter = e_.readText().trimmed();
        } else if (e_.name() == "key-accidental") {
            smufl = e_.attribute("smufl");
            keyAccidental = e_.readText();
        } else {
            skipLogCurrElem();
        }
    }
    flushAlteredTone(key, keyStep, keyAlter, keyAccidental, smufl);

    size_t nstaves = pass1_.getPart(partId)->nstaves();
    staff_idx_t staffIdx = pass1_.trackForPart(partId) / VOICES;
    if (keyno == -1) {
        // apply key to all staves in the part
        for (staff_idx_t i = 0; i < nstaves; ++i) {
            addKey(key, keyColor, printObject, score_, measure, staffIdx + i, tick);
        }
    } else if (keyno < static_cast<int>(nstaves)) {
        addKey(key, keyColor, printObject, score_, measure, staffIdx + keyno, tick);
    }
}

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/clef node.
 */

void MusicXmlParserPass2::clef(const String &partId, Measure *measure, const Fraction &tick) {
    ClefType clef = ClefType::G;
    StaffTypes st = StaffTypes::STANDARD;

    String c;
    int i = 0;
    int line = -1;

    const String strClefno = e_.attribute("number");
    const bool afterBarline = e_.asciiAttribute("after-barline") == "yes";
    const bool printObject = e_.asciiAttribute("print-object") != "no";
    const Color clefColor = Color::fromString(e_.asciiAttribute("color").ascii());

    while (e_.readNextStartElement()) {
        if (e_.name() == "sign") {
            c = e_.readText();
        } else if (e_.name() == "line") {
            line = e_.readInt();
        } else if (e_.name() == "clef-octave-change") {
            i = e_.readInt();
            if (i && !(c == "F" || c == "G" || c == "C")) {
                LOGD("clef-octave-change only implemented for F and G key"); // TODO
            }
        } else {
            skipLogCurrElem();
        }
    }

    //some software (Primus) don't include line and assume some default
    // it's permitted by MusicXML 2.0 XSD
    if (line == -1) {
        if (c == u"G") {
            line = 2;
        } else if (c == u"F") {
            line = 4;
        } else if (c == u"C") {
            line = 3;
        }
    }

    if (c == u"G" && i == 0 && line == 2) {
        clef = ClefType::G;
    } else if (c == u"G" && i == 1 && line == 2) {
        clef = ClefType::G8_VA;
    } else if (c == u"G" && i == 2 && line == 2) {
        clef = ClefType::G15_MA;
    } else if (c == u"G" && i == -1 && line == 2) {
        clef = ClefType::G8_VB;
    } else if (c == u"G" && i == -2 && line == 2) {
        clef = ClefType::G15_MB;
    } else if (c == u"G" && i == 0 && line == 1) {
        clef = ClefType::G_1;
    } else if (c == u"F" && i == 0 && line == 3) {
        clef = ClefType::F_B;
    } else if (c == u"F" && i == 0 && line == 4) {
        clef = ClefType::F;
    } else if (c == u"F" && i == 1 && line == 4) {
        clef = ClefType::F_8VA;
    } else if (c == u"F" && i == 2 && line == 4) {
        clef = ClefType::F_15MA;
    } else if (c == u"F" && i == -1 && line == 4) {
        clef = ClefType::F8_VB;
    } else if (c == u"F" && i == -2 && line == 4) {
        clef = ClefType::F15_MB;
    } else if (c == u"F" && i == 0 && line == 5) {
        clef = ClefType::F_C;
    } else if (c == u"C") {
        if (line == 5) {
            clef = ClefType::C5;
        } else if (line == 4) {
            if (i == -1) {
                clef = ClefType::C4_8VB;
            } else {
                clef = ClefType::C4;
            }
        } else if (line == 3) {
            clef = ClefType::C3;
        } else if (line == 2) {
            clef = ClefType::C2;
        } else if (line == 1) {
            clef = ClefType::C1;
        }
    } else if (c == u"percussion") {
        clef = ClefType::PERC;
        if (hasDrumset_) {
            st = StaffTypes::PERC_DEFAULT;
        }
    } else if (c == u"TAB") {
        clef = ClefType::TAB;
        st = StaffTypes::TAB_DEFAULT;
    } else {
        LOGD("clef: unknown clef <sign=%s line=%d oct ch=%d>", muPrintable(c), line, i); // TODO
    }

    Part *part = pass1_.getPart(partId);
    IF_ASSERT_FAILED(part) { return; }

    // TODO: check error handling for
    // - single staff
    // - multi-staff with same clef
    int clefno = 0; // default
    if (!strClefno.empty()) {
        clefno = pass1_.getMusicXmlPart(partId).staffNumberToIndex(strClefno.toInt());
    }
    if (clefno < 0 || clefno >= int(part->nstaves())) {
        // conversion error (0) or other issue, assume staff 1
        // Also for Cubase 6.5.5 which generates clef number="2" in a single staff part
        // Same fix is required in pass 1 and pass 2
        logger_->logError(String(u"invalid clef number '%1'").arg(strClefno), &e_);
        clefno = 0;
    }

    Segment *s;
    ClefToBarlinePosition position = ClefToBarlinePosition::AUTO;
    // check if the clef change needs to be in the previous measure
    if (!afterBarline && (tick == measure->tick()) && measure->prevMeasure()) {
        s = measure->prevMeasure()->getSegment(SegmentType::Clef, tick);
        if (measure->prevMeasure()->repeatEnd()) {
            position = ClefToBarlinePosition::BEFORE;
        }
    } else {
        s = measure->getSegment(tick.isNotZero() ? SegmentType::Clef : SegmentType::HeaderClef, tick);
        if (measure->prevMeasure() && !measure->prevMeasure()->repeatEnd()) {
            position = ClefToBarlinePosition::AFTER;
        }
    }

    Clef *clefs = Factory::createClef(s);
    clefs->setClefType(clef);
    clefs->setVisible(printObject);
    clefs->setClefToBarlinePosition(position);
    colorItem(clefs, clefColor);
    track_idx_t track = pass1_.trackForPart(partId) + clefno * VOICES;
    clefs->setTrack(track);
    s->add(clefs);

    // set the correct staff type
    // note that clef handling should probably done in pass1
    staff_idx_t staffIdx = score_->staffIdx(part) + clefno;
    int lines = score_->staff(staffIdx)->lines(Fraction(0, 1));
    if (tick.isZero()) { // changing staff type not supported (yet ?)
        score_->staff(staffIdx)->setStaffType(tick, *StaffType::preset(st));
        score_->staff(staffIdx)->setLines(tick, lines); // preserve previously set staff lines
        score_->staff(staffIdx)->setBarLineTo(0);       // default
    }
}

//---------------------------------------------------------
//   determineTimeSig
//---------------------------------------------------------

/**
 Determine the time signature based on \a beats, \a beatType and \a timeSymbol.
 Sets return parameters \a st, \a bts, \a btp.
 Return true if OK, false on error.
 */

// TODO: share between pass 1 and pass 2

static bool determineTimeSig(const String &beats, const String &beatType, const String &timeSymbol, TimeSigType &st, int &bts, int &btp) {
    // initialize
    st = TimeSigType::NORMAL;
    bts = 0; // the beats (max 4 separated by "+") as integer
    btp = 0; // beat-type as integer
    // determine if timesig is valid
    if (timeSymbol == u"cut") {
        st = TimeSigType::ALLA_BREVE;
    } else if (timeSymbol == u"common") {
        st = TimeSigType::FOUR_FOUR;
    } else if (timeSymbol == u"single-number") {
        // let pass
    } else if (!timeSymbol.empty() && timeSymbol != u"normal") {
        LOGD("determineTimeSig: time symbol <%s> not recognized", muPrintable(timeSymbol)); // TODO
        return false;
    }

    btp = beatType.toInt();
    StringList list = beats.split(u'+');
    for (size_t i = 0; i < list.size(); i++) {
        bts += list.at(i).toInt();
    }

    // determine if bts and btp are valid
    if (bts <= 0 || btp <= 0) {
        LOGD("determineTimeSig: beats=%s and/or beat-type=%s not recognized", muPrintable(beats), muPrintable(beatType)); // TODO
        return false;
    }

    return true;
}

//---------------------------------------------------------
//   time
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/time node.
 */

void MusicXmlParserPass2::time(const String &partId, Measure *measure, const Fraction &tick) {
    String beats;
    String beatType;
    String timeSymbol = e_.attribute("symbol");
    bool printObject = e_.asciiAttribute("print-object") != "no";
    const Color timeColor = Color::fromString(e_.asciiAttribute("color").ascii());

    while (e_.readNextStartElement()) {
        if (e_.name() == "beats") {
            beats = e_.readText();
        } else if (e_.name() == "beat-type") {
            beatType = e_.readText();
        } else {
            skipLogCurrElem();
        }
    }

    if (!beats.empty() && !beatType.empty()) {
        // determine if timesig is valid
        TimeSigType st = TimeSigType::NORMAL;
        int bts = 0; // total beats as integer (beats may contain multiple numbers, separated by "+")
        int btp = 0; // beat-type as integer
        if (determineTimeSig(beats, beatType, timeSymbol, st, bts, btp)) {
            timeSigDura_ = Fraction(bts, btp);
            Fraction fractionTSig = Fraction(bts, btp);
            for (size_t i = 0; i < pass1_.getPart(partId)->nstaves(); ++i) {
                Segment *s = measure->getSegment(SegmentType::TimeSig, tick);
                TimeSig *timesig = Factory::createTimeSig(s);
                timesig->setVisible(printObject);
                colorItem(timesig, timeColor);
                track_idx_t track = pass1_.trackForPart(partId) + i * VOICES;
                timesig->setTrack(track);
                timesig->setSig(fractionTSig, st);
                // handle simple compound and single time signatures
                if (beats.contains(Char(u'+'))) {
                    timesig->setNumeratorString(beats);
                    timesig->setDenominatorString(beatType);
                } else if (timeSymbol == u"single-number") {
                    timesig->setNumeratorString(beats);
                }
                s->add(timesig);
            }
        }
    }

    if (printObject && score_->style().styleV(Sid::timeSigPlacement) == TimeSigPlacement::ACROSS_STAVES) {
        track_idx_t track = pass1_.trackForPart(partId);
        Staff *st = score_->staff(track2staff(track));
        if (!st->isSystemObjectStaff() && st->idx() != 0) {
            score_->addSystemObjectStaff(st);
        }
    }
}

//---------------------------------------------------------
//   divisions
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/divisions node.
 */

void MusicXmlParserPass2::divisions() {
    divs_ = e_.readInt();
    if (!(divs_ > 0)) {
        logger_->logError(u"illegal divisions", &e_);
    }
}

//---------------------------------------------------------
//   isWholeMeasureRest
//---------------------------------------------------------

/**
 * Determine whole measure rest.
 */

// By convention, whole measure rests do not have a "type" element
// As of MusicXML 3.0, this can be indicated by an attribute "measure",
// but for backwards compatibility the "old" convention still has to be supported.
// Also verify the rest fits exactly in the measure, as some programs
// (e.g. Cakewalk SONAR X2 Studio [Version: 19.0.0.306]) leave out
// the type for all rests.
// Sibelius calls all whole-measure rests "whole", even if the duration != 4/4

static bool isWholeMeasureRest(const String &type, const Fraction restDuration, const Fraction measureDuration) {
    if (!restDuration.isValid() || !measureDuration.isValid()) {
        return false;
    }

    return (type.empty() && restDuration == measureDuration) || (type == u"whole" && restDuration == measureDuration);
}

//---------------------------------------------------------
//   determineDuration
//---------------------------------------------------------

/**
 * Determine duration for a note or rest.
 * This includes whole measure rest detection.
 */

static TDuration determineDuration(const bool isRest, const bool measureRest, const String &type, const int dots,
                                   const Fraction chordRestDuration, const Fraction measureDuration) {
    //LOGD("determineDuration %s, type <%s>, dots %d, duration %s, measure duration %s",
    //      isRest ? "rest" : "chord", muPrintable(durationType), dots, muPrintable(chordRestDuration.toString()), muPrintable(measureDuration.toString()));

    TDuration res;
    if (isRest && (measureRest || isWholeMeasureRest(type, chordRestDuration, measureDuration))) {
        res.setType(DurationType::V_MEASURE);
    } else if (type.empty()) {
        // If no type, set duration type based on duration.
        // Note that sometimes unusual duration (e.g. 261/256) are found.
        res.setVal(chordRestDuration.ticks());
    } else {
        ByteArray ba = type.toAscii();
        res.setType(TConv::fromXml(ba.constChar(), DurationType::V_INVALID));
        res.setDots(dots);
        if (!res.isValid()) {
            res.setType(DurationType::V_QUARTER); // default, TODO: use measureDuration ?
        }
    }

    //LOGD("-> duration %hhd, dots %d, ticks %s",
    //      res.type(), res.dots(), muPrintable(chordRestDuration.toString()));

    return res;
}

//---------------------------------------------------------
//   findOrCreateChord
//---------------------------------------------------------

/**
 * Find (or create if not found) the chord at \a tick and \a track.
 * Note: staff move is a note property in MusicXML, but chord property in MuseScore
 * This is simply ignored here, effectively using the last chords value.
 */

static Chord *findOrCreateChord(Score *, Measure *m, const Fraction &tick, const int track, const int move, const TDuration duration,
                                const Fraction dura, BeamMode bm, bool small) {
    //LOGD("findOrCreateChord tick %d track %d dur ticks %d ticks %s bm %hhd",
    //       tick, track, duration.ticks(), muPrintable(dura.print()), bm);
    Chord *c = m->findChord(tick, track);
    if (c == 0) {
        Segment *s = m->getSegment(SegmentType::ChordRest, tick);
        c = Factory::createChord(s);
        // better not to force beam end, as the beam palette does not support it
        if (bm == BeamMode::END) {
            c->setBeamMode(BeamMode::AUTO);
        } else {
            c->setBeamMode(bm);
        }
        c->setTrack(track);
        // Chord is initialized with the smallness of its first note.
        // If a non-small note is added later, this is handled in handleSmallness.
        c->setSmall(small);

        setChordRestDuration(c, duration, dura);
        s->add(c);
    }
    c->setStaffMove(move);
    return c;
}

//---------------------------------------------------------
//   graceNoteType
//---------------------------------------------------------

/**
 * convert duration and slash to grace note type
 */

NoteType graceNoteType(const TDuration duration, const bool slash) {
    NoteType nt = NoteType::APPOGGIATURA;
    if (slash) {
        nt = NoteType::ACCIACCATURA;
    } else {
        if (duration.type() == DurationType::V_QUARTER) {
            nt = NoteType::GRACE4;
        } else if (duration.type() == DurationType::V_16TH) {
            nt = NoteType::GRACE16;
        } else if (duration.type() == DurationType::V_32ND) {
            nt = NoteType::GRACE32;
        }
    }
    return nt;
}

//---------------------------------------------------------
//   createGraceChord
//---------------------------------------------------------

/**
 * Create a grace chord.
 */

static Chord *createGraceChord(Score *score, const int track, const TDuration duration, const bool slash, const bool small) {
    Chord *c = Factory::createChord(score->dummy());
    c->setNoteType(graceNoteType(duration, slash));
    c->setTrack(track);
    // Chord is initialized with the smallness of its first note.
    // If a non-small note is added later, this is handled in handleSmallness.
    c->setSmall(small);
    // note grace notes have no durations, use default fraction 0/1
    setChordRestDuration(c, duration, Fraction());
    return c;
}

//---------------------------------------------------------
//   handleDisplayStep
//---------------------------------------------------------

/**
 * convert display-step and display-octave to staff line
 */

static void handleDisplayStep(ChordRest *cr, int step, int octave, const Fraction &tick, double spatium) {
    if (0 <= step && step <= 6 && 0 <= octave && octave <= 9) {
        //LOGD("rest step=%d oct=%d", step, octave);
        ClefType clef = cr->staff()->clef(tick);
        int po = ClefInfo::pitchOffset(clef);
        //LOGD(" clef=%hhd po=%d step=%d", clef, po, step);
        int dp = 7 * (octave + 2) + step;
        //LOGD(" dp=%d po-dp=%d", dp, po-dp);
        cr->ryoffset() = (po - dp + 3) * spatium / 2;
    }
}

//---------------------------------------------------------
//   handleSmallness
//---------------------------------------------------------

static void handleSmallness(bool cueOrSmall, Note *note, Chord *c) {
    if (cueOrSmall) {
        note->setSmall(!c->isSmall()); // Avoid redundant smallness
    } else {
        note->setSmall(false);
        if (c->isSmall()) {
            // What was a small chord becomes small notes in a non-small chord
            c->setSmall(false);
            for (Note *otherNote : c->notes()) {
                if (note != otherNote) {
                    otherNote->setSmall(true);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   setNoteHead
//---------------------------------------------------------

/**
 Set the notehead parameters.
 */

static void setNoteHead(Note *note, const String &noteheadFilled) {
    if (noteheadFilled == u"no") {
        note->setHeadType(NoteHeadType::HEAD_HALF);
    } else if (noteheadFilled == u"yes") {
        note->setHeadType(NoteHeadType::HEAD_QUARTER);
    }
}

//---------------------------------------------------------
//   computeBeamMode
//---------------------------------------------------------

/**
 Calculate the beam mode based on the collected beamTypes.
 */

static BeamMode computeBeamMode(const std::map<int, String> &beamTypes) {
    // Start with uniquely-handled beam modes
    if (muse::value(beamTypes, 1) == u"continue" && muse::value(beamTypes, 2) == u"begin") {
        return BeamMode::BEGIN16;
    } else if (muse::value(beamTypes, 1) == u"continue" && muse::value(beamTypes, 2) == u"continue" &&
               muse::value(beamTypes, 3) == u"begin") {
        return BeamMode::BEGIN32;
    }
    // Generic beam modes are naive to all except the first beam
    else if (muse::value(beamTypes, 1) == u"begin") {
        return BeamMode::BEGIN;
    } else if (muse::value(beamTypes, 1) == u"continue") {
        return BeamMode::MID;
    } else if (muse::value(beamTypes, 1) == u"end") {
        return BeamMode::END;
    } else {
        // backward-hook, forward-hook, and other unknown combinations
        return BeamMode::AUTO;
    }
}

//---------------------------------------------------------
//   addFiguredBassElements
//---------------------------------------------------------

/**
 Add the figured bass elements.
 */

static void addFiguredBassElements(FiguredBassList &fbl, const Fraction noteStartTime, const int msTrack, const Fraction dura,
                                   Measure *measure) {
    if (!fbl.empty()) {
        Fraction sTick = noteStartTime; // starting tick
        for (FiguredBass *fb : fbl) {
            fb->setTrack(msTrack);
            // No duration tag defaults ticks() to 0; set to note value
            if (fb->ticks().isZero()) {
                fb->setTicks(dura);
            }
            // TODO: set correct onNote value
            Segment *s = measure->getSegment(SegmentType::ChordRest, sTick);
            s->add(fb);
            sTick += fb->ticks();
        }
        fbl.clear();
    }
}

//---------------------------------------------------------
//   addTremolo
//---------------------------------------------------------

static void addTremolo(ChordRest *cr, const int tremoloNr, const String &tremoloType, const String &smufl, const Color &color,
                       Chord *&tremStart, MusicXmlLogger *logger, const XmlStreamReader *const xmlreader, Fraction &timeMod) {
    if (!cr->isChord()) {
        return;
    }
    if (tremoloNr) {
        //LOGD("tremolo %d type '%s' ticks %d tremStart %p", tremoloNr, muPrintable(tremoloType), ticks, _tremStart);
        if (tremoloNr >= 1 && tremoloNr <= 6) {
            if (tremoloType.empty() || tremoloType == u"single") {
                TremoloType type = TremoloType::INVALID_TREMOLO;
                switch (tremoloNr) {
                    case 1: type = TremoloType::R8; break;
                    case 2: type = TremoloType::R16; break;
                    case 3: type = TremoloType::R32; break;
                    case 4: type = TremoloType::R64; break;
                    case 5: type = TremoloType::R128; break;
                    case 6: type = TremoloType::R256; break;
                }

                if (type != TremoloType::INVALID_TREMOLO) {
                    TremoloSingleChord *tremolo = Factory::createTremoloSingleChord(mu::engraving::toChord(cr));
                    tremolo->setTremoloType(type);
                    colorItem(tremolo, color);
                    cr->add(tremolo);
                }
            } else if (tremoloType == u"start") {
                if (tremStart) {
                    logger->logError(u"MusicXml::import: double tremolo start", xmlreader);
                }
                tremStart = toChord(cr);
                // timeMod takes into account also the factor 2 of a two-note tremolo
                if (timeMod.isValid() && ((timeMod.denominator() % 2) == 0)) {
                    timeMod.setDenominator(timeMod.denominator() / 2);
                }
            } else if (tremoloType == u"stop") {
                if (tremStart) {
                    TremoloType type = TremoloType::INVALID_TREMOLO;
                    switch (tremoloNr) {
                        case 1: type = TremoloType::C8; break;
                        case 2: type = TremoloType::C16; break;
                        case 3: type = TremoloType::C32; break;
                        case 4: type = TremoloType::C64; break;
                        case 5: type = TremoloType::C128; break;
                        case 6: type = TremoloType::C256; break;
                    }

                    if (type != TremoloType::INVALID_TREMOLO) {
                        TremoloTwoChord *tremolo = Factory::createTremoloTwoChord(mu::engraving::toChord(cr));
                        tremolo->setTremoloType(type);
                        colorItem(tremolo, color);
                        tremolo->setChords(tremStart, toChord(cr));
                        // fixup chord duration and type
                        const Fraction tremDur = cr->ticks() * Fraction(1, 2);
                        tremolo->chord1()->setDurationType(tremDur);
                        tremolo->chord1()->setTicks(tremDur);
                        tremolo->chord2()->setDurationType(tremDur);
                        tremolo->chord2()->setTicks(tremDur);
                        // add tremolo to first chord (only)
                        tremStart->add(tremolo);
                    }
                    // timeMod takes into account also the factor 2 of a two-note tremolo
                    if (timeMod.isValid() && ((timeMod.denominator() % 2) == 0)) {
                        timeMod.setDenominator(timeMod.denominator() / 2);
                    }
                } else {
                    logger->logError(u"MusicXml::import: double tremolo stop w/o start", xmlreader);
                }
                tremStart = nullptr;
            }
        } else {
            logger->logError(String(u"unknown tremolo type %1").arg(tremoloNr), xmlreader);
        }
    } else if (tremoloNr == 0 && (tremoloType == u"unmeasured" || tremoloType.empty() || smufl == u"buzzRoll")) {
        // Out of all the SMuFL unmeasured tremolos, we only support 'buzzRoll'
        TremoloSingleChord *tremolo = Factory::createTremoloSingleChord(mu::engraving::toChord(cr));
        tremolo->setTremoloType(TremoloType::BUZZ_ROLL);
        colorItem(tremolo, color);
        cr->add(tremolo);
    } else if (!smufl.empty() && smufl != u"buzzRoll") {
        logger->logError(String(u"MusicXml::import: only buzzRoll glyph is supported for unmeasured tremolos"), xmlreader);
    }
}

//---------------------------------------------------------
//   setPitch
//---------------------------------------------------------

// TODO: refactor: optimize parameters

static void setPitch(Note *note, const MusicXmlInstruments &instruments, const String &instrumentId, const MusicXmlNotePitch &mnp,
                     const int octaveShift, const Instrument *const instrument) {
    if (mnp.unpitched()) {
        if (hasDrumset(instruments) && muse::contains(instruments, instrumentId)) {
            // step and oct are display-step and ...-oct
            // get pitch from instrument definition in drumset instead
            int unpitched = instruments.at(instrumentId).unpitched;
            note->setPitch(std::clamp(unpitched, 0, 127));
            note->setTpcFromPitch();
        } else {
            //LOGD("disp step %d oct %d", displayStep, displayOctave);
            xmlSetPitch(note, mnp.displayStep(), 0, 0.0, mnp.displayOctave(), 0, instrument);
        }
    } else {
        xmlSetPitch(note, mnp.step(), mnp.alter(), mnp.tuning(), mnp.octave(), octaveShift, instrument);
    }
}

//---------------------------------------------------------
//   setDrumset
//---------------------------------------------------------

// set drumset information
// note that in MuseScore, the drumset contains defaults for notehead,
// line and stem direction, while a MusicXML file contains actuals.
// the MusicXML values for each note are simply copied to the defaults

static void setDrumset(Chord *c, MusicXmlParserPass1 &pass1, const String &partId, const String &instrumentId,
                       const Fraction &noteStartTime, const MusicXmlNotePitch &mnp, const DirectionV stemDir,
                       const NoteHeadGroup headGroup) {
    // determine staff line based on display-step / -octave and clef type
    const ClefType clef = c->staff()->clef(noteStartTime);
    const int po = ClefInfo::pitchOffset(clef);
    const int pitch = MusicXmlStepAltOct2Pitch(mnp.displayStep(), 0, mnp.displayOctave());
    int line = po - absStep(pitch);

    // correct for number of staff lines
    // see ExportMusicXml::unpitch2xml for explanation
    // TODO handle other # staff lines ?
    int staffLines = c->staff()->lines(Fraction(0, 1));
    if (staffLines == 1) {
        line -= 8;
    }
    if (staffLines == 3) {
        line -= 2;
    }

    // the drum palette cannot handle stem direction AUTO,
    // overrule if necessary
    DirectionV overruledStemDir = stemDir;
    if (stemDir == DirectionV::AUTO) {
        if (line > 4) {
            overruledStemDir = DirectionV::DOWN;
        } else {
            overruledStemDir = DirectionV::UP;
        }
    }
    // this should be done in pass 1, would make _pass1 const here
    pass1.setDrumsetDefault(partId, instrumentId, headGroup, line, overruledStemDir);
}

void MusicXmlParserPass2::xmlSetDrumsetPitch(Note *note, const Chord *chord, const Staff *staff, int step, int octave,
                                             NoteHeadGroup headGroup, DirectionV &stemDir, Instrument *instrument) {
    Drumset *ds = instrument->drumset();
    // get line
    // determine staff line based on display-step / -octave and clef type
    const ClefType clef = staff->clef(chord->tick());
    const int po = ClefInfo::pitchOffset(clef);
    const int pitch = MusicXmlStepAltOct2Pitch(step, 0, octave);
    int line = po - absStep(pitch);
    const int staffLines = staff->lines(chord->tick());
    if (staffLines == 1) {
        line -= 8;
    }
    if (staffLines == 3) {
        line -= 2;
    }

    const int firstDrum = ds->nextPitch(0);
    int curDrum = firstDrum;
    int newPitch = pitch;
    bool matchFound = false;
    do {
        if (ds->line(curDrum) == line) {
            if (ds->noteHead(curDrum) == headGroup) {
                newPitch = curDrum;
                matchFound = true;
                break;
            }
        }
        curDrum = ds->nextPitch(curDrum);
    } while (curDrum != firstDrum);

    // Find inferred instruments at this tick
    if (configuration()->inferTextType()) {
        InferredPercInstr instr = inferredPercInstr(chord->tick(), chord->track());
        if (instr.track != muse::nidx) {
            // Clear old instrument
            ds->drum(newPitch) = DrumInstrument();

            newPitch = instr.pitch;
            if (stemDir == DirectionV::AUTO) {
                stemDir = chord->voice() & 1 ? DirectionV::DOWN : DirectionV::UP;
            }
            ds->drum(newPitch) = DrumInstrument(instr.name, headGroup, line, stemDir, static_cast<int>(chord->voice()));
        }
    }

    // If there is no exact match add an entry to the drumkit with the XML line and notehead
    if (!matchFound) {
        // Create new instrument in drumkit
        if (stemDir == DirectionV::AUTO) {
            if (line > 4) {
                stemDir = DirectionV::DOWN;
            } else {
                stemDir = DirectionV::UP;
            }
        }

        ds->drum(newPitch) = DrumInstrument(u"drum", headGroup, line, stemDir, static_cast<int>(chord->voice()));
    } else if (stemDir == DirectionV::AUTO) {
        if (ds->voice(newPitch) != static_cast<int>(chord->voice())) {
            stemDir = chord->voice() & 1 ? DirectionV::DOWN : DirectionV::UP;
        } else {
            stemDir = ds->stemDirection(newPitch);
        }
    }

    note->setPitch(newPitch);
    note->setTpcFromPitch();
}

//---------------------------------------------------------
//   note
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note node.
 */

Note *MusicXmlParserPass2::note(const String &partId, Measure *measure, const Fraction sTime, const Fraction prevSTime,
                                Fraction &missingPrev, Fraction &dura, Fraction &missingCurr, String &currentVoice, GraceChordList &gcl,
                                size_t &gac, Beams &currBeams, FiguredBassList &fbl, int &alt, MusicXmlTupletStates &tupletStates,
                                Tuplets &tuplets, ArpeggioMap &arpMap, DelayedArpMap &delayedArps) {
    if (e_.asciiAttribute("print-spacing") == "no") {
        notePrintSpacingNo(dura);
        return 0;
    }

    bool chord = false;
    bool cue = false;
    bool isSmall = false;
    bool grace = false;
    bool rest = false;
    bool measureRest = false;
    int staff = 0;
    String type;
    String voice;
    DirectionV stemDir = DirectionV::AUTO;
    bool noStem = false;
    bool hasHead = true;
    NoteHeadGroup headGroup = NoteHeadGroup::HEAD_NORMAL;
    NoteHeadScheme headScheme = NoteHeadScheme::HEAD_AUTO;
    const Color noteColor = Color::fromString(e_.asciiAttribute("color").ascii());
    Color noteheadColor;
    Color stemColor;
    Color beamColor;
    bool noteheadParentheses = false;
    String noteheadFilled;
    // velocity as a percentage of the MIDI 1.0 default forte value of 90;
    // an explicit dynamics="0" means a silent note, which the score model can
    // only represent as velocity 1 (velocity 0 means "unset")
    const bool hasDynamics = e_.hasAttribute("dynamics");
    const int velocity = hasDynamics ? musicXmlDynamicVelocity(e_.attribute("dynamics"), 1, 1) : 1;
    bool graceSlash = false;
    double graceStealFollowing = -1.0;
    double graceStealPrevious = -1.0;
    bool printObject = e_.asciiAttribute("print-object") != "no";
    bool printLyric = (printObject && e_.asciiAttribute("print-lyric") != "no") || e_.asciiAttribute("print-lyric") == "yes";
    bool isSingleDrumset = false;
    BeamMode bm;
    std::map<int, String> beamTypes;
    String beamFan;
    String instrumentId;
    String tieType;
    MusicXmlParserLyric lyric{
        pass1_.getMusicXmlPart(partId).lyricNumberHandler(), e_, score_, *configuration(), logger_, pass1_.isVocalStaff(partId)};
    MusicXmlParserNotations notations{e_, score_, logger_, *this};

    MusicXmlNoteDuration mnd{divs_, logger_, &pass1_};
    MusicXmlNotePitch mnp{logger_};

    while (e_.readNextStartElement()) {
        if (mnp.readProperties(e_, score_)) {
            // element handled
        } else if (mnd.readProperties(e_)) {
            // element handled
        } else if (e_.name() == "accidental") {
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "beam") {
            beamColor = Color::fromString(e_.asciiAttribute("color").ascii());
            beamFan = e_.attribute("fan");
            beam(beamTypes);
        } else if (e_.name() == "chord") {
            chord = true;
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "cue") {
            cue = true;
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "grace") {
            grace = true;
            graceSlash = e_.asciiAttribute("slash") == "yes";
            graceStealFollowing = e_.doubleAttribute("steal-time-following", -1.0);
            graceStealPrevious = e_.doubleAttribute("steal-time-previous", -1.0);
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "instrument") {
            instrumentId = e_.attribute("id");
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "lyric") {
            // lyrics on grace notes not (yet) supported by MuseScore
            // add to main note instead
            lyric.parse(printLyric);
        } else if (e_.name() == "notations") {
            notations.parse();
            addError(notations.errors());
        } else if (e_.name() == "notehead") {
            noteheadColor = Color::fromString(e_.asciiAttribute("color").ascii());
            noteheadParentheses = e_.asciiAttribute("parentheses") == "yes";
            noteheadFilled = e_.attribute("filled");
            String noteheadValue = e_.readText();
            if (noteheadValue == "none") {
                hasHead = false;
            } else if (noteheadValue == "named" && pass1_.exporterSoftware() == MusicXmlExporterSoftware::NOTEFLIGHT) {
                headScheme = NoteHeadScheme::HEAD_PITCHNAME;
            } else {
                headGroup = convertNotehead(noteheadValue);
            }
        } else if (e_.name() == "notehead-text") {
            String noteheadText;
            while (e_.readNextStartElement()) {
                if (e_.name() == "display-text") {
                    noteheadText = e_.readText();
                } else if (e_.name() == "accidental-text") {
                    e_.skipCurrentElement();
                } else {
                    skipLogCurrElem();
                }
            }
            if (noteheadText.size() == 1) {
                const bool isGerman = noteheadText == u"H" || (noteheadText == u"B" && mnp.alter());
                headScheme = isGerman ? NoteHeadScheme::HEAD_PITCHNAME_GERMAN : NoteHeadScheme::HEAD_PITCHNAME;
            } else {
                const std::vector<String> names = {u"Do", u"Re", u"Mi", u"Fa", u"Sol", u"La", u"Si"};
                const bool isFixed = names.at(mnp.step()) == noteheadText;
                headScheme = isFixed ? NoteHeadScheme::HEAD_SOLFEGE_FIXED : NoteHeadScheme::HEAD_SOLFEGE;
            }
        } else if (e_.name() == "rest") {
            rest = true;
            measureRest = e_.asciiAttribute("measure") == "yes";
            mnp.displayStepOctave(e_);
        } else if (e_.name() == "staff") {
            bool ok = false;
            String strStaff = e_.readText();
            staff = pass1_.getMusicXmlPart(partId).staffNumberToIndex(strStaff.toInt(&ok));
            if (!ok) {
                // error already reported in pass 1
                staff = -1;
            }
        } else if (e_.name() == "stem") {
            stemColor = Color::fromString(e_.asciiAttribute("color").ascii());
            stem(stemDir, noStem);
        } else if (e_.name() == "tie") {
            tieType = e_.attribute("type");
            e_.skipCurrentElement();
        } else if (e_.name() == "type") {
            isSmall = e_.asciiAttribute("size") == "cue" || e_.asciiAttribute("size") == "grace-cue";
            type = e_.readText();
        } else if (e_.name() == "voice") {
            voice = e_.readText();
        } else {
            skipLogCurrElem();
        }
    }

    // Bug fix for Sibelius 7.1.3 which does not write <voice> for notes with <chord>
    if (!chord) {
        // remember voice
        currentVoice = voice;
    } else if (voice.empty()) {
        // use voice from last note w/o <chord>
        voice = currentVoice;
    }

    // Assume voice 1 if voice is empty (legal in a single voice part)
    if (voice.empty()) {
        voice = u"1";
    }

    // Define currBeam based on currentVoice to handle multi-voice beaming (and instantiate if not already)
    if (!muse::contains(currBeams, currentVoice)) {
        currBeams.insert({currentVoice, (Beam *) nullptr});
    }
    Beam *&currBeam = currBeams[currentVoice];

    bm = computeBeamMode(beamTypes);

    // check for timing error(s) and set dura
    // keep in this order as checkTiming() might change dura
    String errorStr = mnd.checkTiming(type, rest, grace);
    dura = mnd.duration();
    if (!errorStr.empty()) {
        logger_->logError(errorStr, &e_);
    }

    IF_ASSERT_FAILED(pass1_.getPart(partId)) { return nullptr; }

    // At this point all checks have been done, the note should be added
    // note: in case of error exit from here, the postponed <note> children
    // must still be skipped

    int msMove = 0;
    int msTrack = 0;
    int msVoice = 0;

    int voiceInt = pass1_.voiceToInt(voice);
    if (!pass1_.determineStaffMoveVoice(partId, staff, voiceInt, msMove, msTrack, msVoice)) {
        logger_->logDebugInfo(String(u"could not map staff %1 voice '%2'").arg(staff + 1).arg(voice), &e_);
        addError(checkAtEndElement(e_, u"note"));
        return 0;
    }

    // start time for note:
    // - sTime for non-chord / first chord note
    // - prevTime for others
    Fraction noteStartTime = chord ? prevSTime : sTime;
    Fraction timeMod = mnd.timeMod();

    // determine tuplet state, used twice (before and after note allocation)
    MusicXmlTupletFlags tupletAction;

    // handle tuplet state for the previous chord or rest
    if (!chord && !grace) {
        Tuplet *tuplet = tuplets[voice];
        MusicXmlTupletState &tupletState = tupletStates[voice];
        tupletAction = tupletState.determineTupletAction(mnd.duration(), timeMod, notations.tupletDesc().type, mnd.normalType(),
                                                         missingPrev, missingCurr);
        if (tupletAction & MusicXmlTupletFlag::STOP_PREVIOUS) {
            // tuplet start while already in tuplet
            if (missingPrev.isValid() && missingPrev > Fraction(0, 1)) {
                const int track = msTrack + msVoice;
                Rest *const extraRest =
                    addRest(score_, measure, noteStartTime, track, msMove, TDuration{missingPrev * tuplet->ratio()}, missingPrev);
                if (extraRest) {
                    extraRest->setTuplet(tuplet);
                    tuplet->add(extraRest);
                    noteStartTime += missingPrev;
                }
            }
            // recover by simply stopping the current tuplet first
            const int normalNotes = timeMod.numerator();
            handleTupletStop(tuplet, normalNotes);
        }
    }

    Chord *c{nullptr};
    ChordRest *cr{nullptr};
    Note *note{nullptr};

    TDuration duration = determineDuration(rest, measureRest, type, mnd.dots(), dura, measure->ticks());

    Part *part = pass1_.getPart(partId);
    Instrument *instrument = part->instrument(noteStartTime);
    const MusicXmlInstruments &instruments = pass1_.getInstruments(partId);
    isSingleDrumset = instrument->drumset() && instruments.size() == 1;
    // begin allocation
    if (rest) {
        if (!grace) {
            const int track = msTrack + msVoice;
            cr = addRest(score_, measure, noteStartTime, track, msMove, duration, dura);
        } else {
            LOGD("ignoring grace rest");
        }
    } else {
        if (!grace) {
            // regular note
            // if there is already a chord just add to it
            // else create a new one
            // this basically ignores <chord/> errors
            c = findOrCreateChord(score_, measure, noteStartTime, msTrack + msVoice, msMove, duration, dura, bm, isSmall || cue);
        } else {
            // grace note
            // TODO: check if explicit stem direction should also be set for grace notes
            // (the DOM parser does that, but seems to have no effect on the autotester)
            if (!chord || gcl.empty()) {
                c = createGraceChord(score_, msTrack + msVoice, duration, graceSlash, isSmall || cue);
                // TODO FIX
                // the setStaffMove() below results in identical behaviour as 2.0:
                // grace note will be at the wrong staff with the wrong pitch,
                // seems to use the line value calculated for the right staff
                // leaving it places the note at the wrong staff with the right pitch
                // this affects only grace notes where staff move differs from
                // the main note, e.g. DebuMandSample.xml first grace in part 2
                // c->setStaffMove(msMove);
                // END TODO
                gcl.push_back(c);
            } else {
                c = gcl.back();
            }
        }
        note = Factory::createNote(c);
        const staff_idx_t ottavaStaff = (msTrack - pass1_.trackForPart(partId)) / VOICES;
        const int octaveShift = pass1_.octaveShift(partId, ottavaStaff, noteStartTime);
        const Staff *st = c->staff();
        if (isSingleDrumset && mnp.unpitched() && instrumentId.empty()) {
            xmlSetDrumsetPitch(note, c, st, mnp.displayStep(), mnp.displayOctave(), headGroup, stemDir, instrument);
        } else {
            setPitch(note, instruments, instrumentId, mnp, octaveShift, instrument);
        }
        c->add(note);
        cr = c;
    }
    // end allocation

    if (rest) {
        const track_idx_t track = msTrack + msVoice;
        if (cr) {
            if (currBeam) {
                if (currBeam->track() == track) {
                    cr->setBeamMode(BeamMode::MID);
                    currBeam->add(cr);
                } else {
                    removeBeam(currBeam);
                }
            } else {
                cr->setBeamMode(BeamMode::NONE);
            }
            cr->setSmall(isSmall);
            colorItem(cr, noteColor);
            cr->setVisible(printObject);
            handleDisplayStep(cr, mnp.displayStep(), mnp.displayOctave(), noteStartTime, score_->style().spatium());
        }
    } else {
        handleSmallness(cue || isSmall, note, c);
        note->setPlay(!cue); // cue notes don't play
        note->setHeadGroup(headGroup);
        if (headScheme != NoteHeadScheme::HEAD_AUTO && !mnp.unpitched()) {
            note->setHeadScheme(headScheme);
        }
        colorItem(note, noteColor);
        Stem *stem = c->stem();
        if (!stem) {
            stem = Factory::createStem(c);
            colorItem(stem, stemColor);
            if (!stemColor.isValid()) {
                colorItem(stem, noteColor);
            }
            c->add(stem);
        }
        setNoteHead(note, noteheadFilled);
        if (noteheadParentheses) {
            note->setParenthesesMode(ParenthesesMode::BOTH);
        }
        colorItem(note, noteheadColor);
        note->setVisible(hasHead && printObject);
        stem->setVisible(printObject);

        if (!grace) {
            // regular note
            // handle beam
            if (!chord) {
                handleBeamAndStemDir(c, bm, stemDir, currBeam, pass1_.hasBeamingInfo(), beamColor, beamFan);
            }

            // append any grace chord after chord to the previous chord
            Chord *const prevChord = measure->findChord(prevSTime, msTrack + msVoice);
            if (prevChord && prevChord != c) {
                addGraceChordsAfter(prevChord, gcl, gac);
            }

            // append any grace chord
            addGraceChordsBefore(c, gcl);
        }

        if (mnd.calculatedDuration().isValid() && mnd.specifiedDuration().isValid() && mnd.calculatedDuration().isNotZero() &&
            mnd.calculatedDuration() != mnd.specifiedDuration()) {
            // convert duration into note length
            Fraction durationMult{(mnd.specifiedDuration() / mnd.calculatedDuration()).reduced()};
            durationMult = (1000 * durationMult).reduced();
            const int noteLen = durationMult.numerator() / durationMult.denominator();

            NoteEventList nel;
            NoteEvent ne;
            ne.setLen(noteLen);
            nel.push_back(ne);
            note->setPlayEvents(nel);
            if (c) {
                c->setPlayEventType(PlayEventType::User);
            }
        }

        if (hasDynamics) {
            note->setUserVelocity(velocity);
        }

        if (mnp.unpitched() && !isSingleDrumset) {
            setDrumset(c, pass1_, partId, instrumentId, noteStartTime, mnp, stemDir, headGroup);
        }

        // accidental handling
        //LOGD("note acc %p type %hhd acctype %hhd",
        //       acc, acc ? acc->accidentalType() : static_cast<mu::engraving::AccidentalType>(0), accType);
        Accidental *acc = mnp.acc();
        if (!acc && mnp.accType() != AccidentalType::NONE) {
            acc = Factory::createAccidental(score_->dummy());
            acc->setAccidentalType(mnp.accType());
        }

        if (acc) {
            acc->setVisible(printObject);
            note->add(acc);
            // save alter value for user accidental
            if (acc->accidentalType() != AccidentalType::NONE) {
                alt = mnp.alter();
            }
        }

        c->setNoStem(noStem);
    }

    // cr can be 0 here (if a rest cannot be added)
    // TODO: complete and cleanup handling this case
    if (cr) {
        cr->setVisible(printObject);
    }

    // handle notations
    if (cr) {
        notations.addToScore(cr, note, noteStartTime, slurs_, glissandi_, spanners_, trills_, ties_, unstartedTieNotes_, unendedTieNotes_,
                             arpMap, delayedArps);

        // if no tie added yet, convert the "tie" into "tied" and add it.
        if (note && !note->tieFor() && !note->tieBack() && !tieType.empty()) {
            Notation notation = Notation(u"tied");
            const String type2 = u"type";
            notation.addAttribute(type2, tieType);
            addTie(notation, note, cr->track(), ties_, unstartedTieNotes_, unendedTieNotes_, logger_, &e_, configuration()->importLayout());
        }
    }

    if (grace) {
        const bool afterByStealTime = graceStealPrevious >= 0.0 && c && c->noteType() != NoteType::ACCIACCATURA;
        if (graceStealFollowing < 0.0 && (afterByStealTime || notations.mustStopGraceAfter())) {
            // handle grace after state: remember current grace list size
            gac = gcl.size();
        }
    }

    // handle tremolo before handling tuplet (two note tremolos modify timeMod)
    if (cr && notations.hasTremolo()) {
        addTremolo(cr, notations.tremoloNr(), notations.tremoloType(), notations.tremoloSmufl(), notations.tremoloColor(), tremStart_,
                   logger_, &e_, timeMod);
    }

    // handle tuplet state for the current chord or rest
    if (cr) {
        if (!chord && !grace) {
            Tuplet *&tuplet = tuplets[voice];
            // do tuplet if valid time-modification is not 1/1 and is not 1/2 (tremolo)
            // TODO: check interaction tuplet and tremolo handling
            if (timeMod.isValid() && timeMod != Fraction(1, 1) && timeMod != Fraction(1, 2)) {
                const int actualNotes = timeMod.denominator();
                const int normalNotes = timeMod.numerator();
                if (tupletAction & MusicXmlTupletFlag::START_NEW) {
                    // create a new tuplet
                    handleTupletStart(cr, tuplet, actualNotes, normalNotes, notations.tupletDesc());
                }
                if (tuplet && tupletAction & MusicXmlTupletFlag::ADD_CHORD) {
                    cr->setTuplet(tuplet);
                    tuplet->add(cr);
                }
                if (tuplet && tupletAction & MusicXmlTupletFlag::STOP_CURRENT) {
                    if (missingCurr.isValid() && missingCurr > Fraction(0, 1)) {
                        LOGD("add missing %s to current tuplet", muPrintable(missingCurr.toString()));
                        const int track = msTrack + msVoice;
                        Rest *const extraRest = addRest(score_, measure, noteStartTime + dura, track, msMove,
                                                        TDuration{missingCurr * tuplet->ratio()}, missingCurr);
                        if (extraRest) {
                            extraRest->setTuplet(tuplet);
                            tuplet->add(extraRest);
                        }
                    }
                    handleTupletStop(tuplet, normalNotes);
                }
            } else if (tuplet) {
                // stop any still incomplete tuplet
                handleTupletStop(tuplet, 2);
            }
        }
    }

    // Add all lyrics from grace notes attached to this chord
    if (c && !c->graceNotes().empty() && !graceNoteLyrics_.empty()) {
        for (GraceNoteLyrics gnl : graceNoteLyrics_) {
            if (gnl.lyric) {
                addLyric(logger_, &e_, cr, gnl.lyric, gnl.no, extendedLyrics_);
                if (gnl.extend) {
                    extendedLyrics_.addLyric(gnl.lyric);
                }
            }
        }
        graceNoteLyrics_.clear();
    }

    if (cr) {
        addInferredStickings(cr, lyric.inferredStickings());
    }

    // add lyrics found by lyric
    if (cr && !grace) {
        // add lyrics and stop corresponding extends
        addLyrics(logger_, &e_, cr, lyric.numberedLyrics(), lyric.extendedLyrics(), extendedLyrics_);
        if (rest) {
            // stop all extends
            extendedLyrics_.setExtend(-1, cr->track(), cr->tick());
        }
    } else if (c && grace) {
        // Add grace note lyrics to main chord later
        addGraceNoteLyrics(lyric.numberedLyrics(), lyric.extendedLyrics(), graceNoteLyrics_);
    }

    // add figured bass element
    addFiguredBassElements(fbl, noteStartTime, msTrack, dura, measure);

    // convert to slash or rhythmic notation if needed
    // TODO in the case of slash notation, we assume that given notes do in fact correspond to slash beats
    if (c && measureStyleSlash_ != MusicXmlSlash::NONE) {
        c->setSlash(true, measureStyleSlash_ == MusicXmlSlash::SLASH);
    }

    // don't count chord or grace note duration
    // note that this does not check the MusicXML requirement that notes in a chord
    // cannot have a duration longer than the first note in the chord
    if (chord || grace) {
        dura.set(0, 1);
    }

    addError(checkAtEndElement(e_, u"note"));

    return note;
}

//---------------------------------------------------------
//   notePrintSpacingNo
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note node for a note with print-spacing="no".
 These are handled like a forward: only moving the time forward.
 */

void MusicXmlParserPass2::notePrintSpacingNo(Fraction &dura) {
    //_logger->logDebugTrace("MusicXmlParserPass1::notePrintSpacingNo", &_e);

    bool chord = false;
    bool grace = false;

    while (e_.readNextStartElement()) {
        if (e_.name() == "chord") {
            chord = true;
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "duration") {
            duration(dura);
        } else if (e_.name() == "grace") {
            grace = true;
            e_.skipCurrentElement(); // skip but don't log
        } else {
            e_.skipCurrentElement(); // skip but don't log
        }
    }

    // don't count chord or grace note duration
    // note that this does not check the MusicXML requirement that notes in a chord
    // cannot have a duration longer than the first note in the chord
    if (chord || grace) {
        dura.set(0, 1);
    }

    addError(checkAtEndElement(e_, u"note"));
}

//---------------------------------------------------------
//   duration
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/duration node.
 */

void MusicXmlParserPass2::duration(Fraction &dura) {
    dura.set(0, 0); // invalid unless set correctly
    const String elementText = e_.readText();
    if (elementText.toInt() > 0) {
        dura = pass1_.calcTicks(elementText.toInt(), divs_, &e_);
    } else {
        logger_->logError(String(u"illegal duration %1").arg(dura.toString()), &e_);
    }
    //LOGD("duration %s valid %d", muPrintable(dura.print()), dura.isValid());
}

static FiguredBassItem::Modifier MusicXml2Modifier(const String prefix) {
    if (prefix == u"sharp") {
        return FiguredBassItem::Modifier::SHARP;
    } else if (prefix == u"flat") {
        return FiguredBassItem::Modifier::FLAT;
    } else if (prefix == u"natural") {
        return FiguredBassItem::Modifier::NATURAL;
    } else if (prefix == u"double-sharp") {
        return FiguredBassItem::Modifier::DOUBLESHARP;
    } else if (prefix == u"flat-flat") {
        return FiguredBassItem::Modifier::DOUBLEFLAT;
    } else if (prefix == u"sharp-sharp") {
        return FiguredBassItem::Modifier::DOUBLESHARP;
    } else if (prefix == u"cross") {
        return FiguredBassItem::Modifier::CROSS;
    } else if (prefix == u"backslash") {
        return FiguredBassItem::Modifier::BACKSLASH;
    } else if (prefix == u"slash") {
        return FiguredBassItem::Modifier::SLASH;
    } else {
        return FiguredBassItem::Modifier::NONE;
    }
}

//---------------------------------------------------------
//   figure
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/harmony/figured-bass/figure node.
 Return the result as a FiguredBassItem.
 */

FiguredBassItem *MusicXmlParserPass2::figure(const int idx, const bool paren, FiguredBass *parent) {
    FiguredBassItem *fgi = parent->createItem(idx);

    // read the figure
    while (e_.readNextStartElement()) {
        if (e_.name() == "extend") {
            AsciiStringView type = e_.asciiAttribute("type");
            if (type == "start") {
                fgi->setContLine(FiguredBassItem::ContLine::EXTENDED);
            } else if (type == "continue") {
                fgi->setContLine(FiguredBassItem::ContLine::EXTENDED);
            } else if (type == "stop") {
                fgi->setContLine(FiguredBassItem::ContLine::SIMPLE);
            }
            e_.skipCurrentElement();
        } else if (e_.name() == "figure-number") {
            const Color color = Color::fromString(e_.asciiAttribute("color").ascii());
            String val = e_.readText();
            int iVal = val.toInt();
            // MusicXML spec states figure-number is a number
            // MuseScore can only handle single digit
            if (1 <= iVal && iVal <= 9) {
                fgi->setDigit(iVal);
                colorItem(fgi, color);
            } else {
                logger_->logError(String(u"incorrect figure-number '%1'").arg(val), &e_);
            }
        } else if (e_.name() == "prefix") {
            fgi->setPrefix(MusicXml2Modifier(e_.readText()));
        } else if (e_.name() == "suffix") {
            fgi->setSuffix(MusicXml2Modifier(e_.readText()));
        } else {
            skipLogCurrElem();
        }
    }

    // set parentheses
    if (paren) {
        // parenthesis open
        if (fgi->prefix() != FiguredBassItem::Modifier::NONE) {
            fgi->setParenth1(FiguredBassItem::Parenthesis::ROUNDOPEN); // before prefix
        } else if (fgi->digit() != FBIDigitNone) {
            fgi->setParenth2(FiguredBassItem::Parenthesis::ROUNDOPEN); // before digit
        } else if (fgi->suffix() != FiguredBassItem::Modifier::NONE) {
            fgi->setParenth3(FiguredBassItem::Parenthesis::ROUNDOPEN); // before suffix
        }
        // parenthesis close
        if (fgi->suffix() != FiguredBassItem::Modifier::NONE) {
            fgi->setParenth4(FiguredBassItem::Parenthesis::ROUNDCLOSED); // after suffix
        } else if (fgi->digit() != FBIDigitNone) {
            fgi->setParenth3(FiguredBassItem::Parenthesis::ROUNDCLOSED); // after digit
        } else if (fgi->prefix() != FiguredBassItem::Modifier::NONE) {
            fgi->setParenth2(FiguredBassItem::Parenthesis::ROUNDCLOSED); // after prefix
        }
    }

    return fgi;
}

//---------------------------------------------------------
//   figuredBass
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/harmony/figured-bass node.
 TODO check description:
 // Set the FiguredBass state based on the MusicXML <figured-bass> node de.
 // Note that onNote and ticks must be set by the MusicXML importer,
 // as the required context is not present in the items DOM tree.
 // Exception: if a <duration> element is present, tick can be set.
 Return the result as a FiguredBass if valid, non-empty figure(s) are found.
 Return 0 in case of error.
 */

FiguredBass *MusicXmlParserPass2::figuredBass() {
    FiguredBass *fb = Factory::createFiguredBass(score_->dummy());

    const bool parentheses = e_.asciiAttribute("parentheses") == "yes";
    const bool printObject = e_.asciiAttribute("print-object") != "no";
    const String placement = e_.attribute("placement");

    colorItem(fb, Color::fromString(e_.asciiAttribute("color").ascii()));
    fb->setVisible(printObject);

    String normalizedText;
    int idx = 0;
    while (e_.readNextStartElement()) {
        if (e_.name() == "duration") {
            Fraction dura;
            duration(dura);
            if (dura.isValid() && dura > Fraction(0, 1)) {
                fb->setTicks(dura);
            }
        } else if (e_.name() == "figure") {
            FiguredBassItem *pItem = figure(idx++, parentheses, fb);
            pItem->setTrack(0 /* TODO fb->track() */);
            pItem->setOwnershipParent(fb);
            fb->appendItem(pItem);
            // add item normalized text
            if (!normalizedText.empty()) {
                normalizedText.append('\n');
            }
            normalizedText.append(pItem->normalizedText());
        } else {
            skipLogCurrElem();
            delete fb;
            return 0;
        }
    }

    fb->setXmlText(normalizedText); // this is the text to show while editing

    fb->setPlacement(placement == "above" ? PlacementV::ABOVE : PlacementV::BELOW);
    fb->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    fb->resetProperty(Pid::OFFSET);

    if (normalizedText.empty()) {
        delete fb;
        return 0;
    }

    return fb;
}

//---------------------------------------------------------
//   frame
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/harmony/frame node.
 Return the result as a FretDiagram.
  Notes:
 - MusicXML's first-fret is a positive integer equivalent to MuseScore's FretDiagram::_fretOffset
 - it is one-based in MusicXML and zero-based in MuseScore
 - in MusicXML fret numbers are absolute, in MuseScore they are relative to the fretOffset,
   which affects both single strings and barres
 */

FretDiagram *MusicXmlParserPass2::frame() {
    FretDiagram *fd = Factory::createFretDiagram(score_->dummy());

    colorItem(fd, Color::fromString(e_.asciiAttribute("color").ascii()));

    // Format: fret: string
    std::map<int, int> bStarts;
    std::map<int, int> bEnds;

    while (e_.readNextStartElement()) {
        if (e_.name() == "first-fret") {
            bool ok{};
            int val = e_.readInt(&ok);
            if (ok && val > 0) {
                fd->setFretOffset(val - 1);
            } else {
                logger_->logError(String(u"FretDiagram::readMusicXml: illegal first-fret %1").arg(val), &e_);
            }
        } else if (e_.name() == "frame-frets") {
            int val = e_.readInt();
            if (val > 0) {
                fd->setProperty(Pid::FRET_FRETS, val);
                if (val != fd->propertyDefault(Pid::FRET_FRETS).toInt()) {
                    fd->setPropertyFlags(Pid::FRET_FRETS, PropertyFlags::UNSTYLED);
                }
            } else {
                logger_->logError(String(u"FretDiagram::readMusicXml: illegal frame-fret %1").arg(val), &e_);
            }
        } else if (e_.name() == "frame-note") {
            int fret = -1;
            int string = -1;
            int actualString = -1;
            while (e_.readNextStartElement()) {
                if (e_.name() == "fret") {
                    fret = e_.readInt();
                } else if (e_.name() == "string") {
                    string = e_.readInt();
                    actualString = fd->strings() - string;
                } else if (e_.name() == "barre") {
                    // Keep barres to be added later
                    String t = e_.attribute("type");
                    if (t == "start") {
                        bStarts[fret] = actualString;
                    } else if (t == "stop") {
                        bEnds[fret] = actualString;
                    } else {
                        logger_->logError(String(u"FretDiagram::readMusicXml: illegal frame-note barre type %1").arg(t), &e_);
                    }
                    skipLogCurrElem();
                } else {
                    skipLogCurrElem();
                }
            }
            logger_->logDebugInfo(String(u"FretDiagram::readMusicXml string %1 fret %2").arg(string).arg(fret), &e_);

            if (string > 0) {
                if (fret == 0) {
                    fd->setMarker(actualString, FretMarkerType::CIRCLE);
                } else if (fret > 0) {
                    if (fd->marker(actualString).mtype == FretMarkerType::CROSS) {
                        fd->setMarker(actualString, FretMarkerType::NONE);
                    }
                    fd->setDot(actualString, fret - fd->fretOffset(), true);
                }
            } else {
                logger_->logError(String(u"FretDiagram::readMusicXml: illegal frame-note string %1").arg(string), &e_);
            }
        } else if (e_.name() == "frame-strings") {
            int val = e_.readInt();
            if (val > 0) {
                fd->setStrings(val);
                for (int i = 0; i < val; ++i) {
                    // MXML Spec: any string without a dot or other marker has a closed string
                    // cross marker above it.
                    fd->setMarker(i, FretMarkerType::CROSS);
                }
            } else {
                logger_->logError(String(u"FretDiagram::readMusicXml: illegal frame-strings %1").arg(val), &e_);
            }
        } else {
            skipLogCurrElem();
        }
    }

    // Finally add barres
    for (auto const &i : bStarts) {
        int fret = i.first;
        int startString = i.second;

        if (bEnds.find(fret) == bEnds.end()) {
            continue;
        }

        int endString = bEnds[fret];
        fd->setBarre(startString, endString, fret - fd->fretOffset());
    }

    return fd;
}

//---------------------------------------------------------
//   harmony
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/harmony node.
 */

void MusicXmlParserPass2::harmony(const String &partId, Measure *measure, const Fraction &sTime, HarmonyMap &harmonyMap) {
    UNUSED(measure);
    track_idx_t track = pass1_.trackForPart(partId);

    Staff *staff = score_->staff(track2staff(track));
    Key key = staff ? staff->key(sTime) : Key::INVALID;

    const Color color = Color::fromString(e_.asciiAttribute("color").ascii());
    const String fontFamily = e_.attribute("font-family");
    const String placement = e_.attribute("placement");
    const bool printObject = e_.asciiAttribute("print-object") != "no";

    String kind, kindText, functionText, inversionText, symbols, parens;
    std::vector<HDegree> degreeList;

    FretDiagram *fd = nullptr;
    Harmony *ha = Factory::createHarmony(score_->dummy());
    HarmonyInfo *info = new HarmonyInfo(score_);
    Fraction offset;
    if (!placement.empty()) {
        ha->setPlacement(placement == "below" ? PlacementV::BELOW : PlacementV::ABOVE);
        ha->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
        ha->resetProperty(Pid::OFFSET);
    }
    while (e_.readNextStartElement()) {
        if (e_.name() == "root") {
            if (info->rootTpc() != Tpc::TPC_INVALID) {
                const ChordDescription *d = harmonyFromXml(info, score_, kind, kindText, symbols, parens, degreeList);
                info->setId(d->id);
                info->setTextName(d->names.front());
                ha->addChord(info);
                degreeList.clear();
                info = new HarmonyInfo(score_);
            }
            String step;
            int alter = 0;
            bool invalidRoot = false;
            while (e_.readNextStartElement()) {
                if (e_.name() == "root-step") {
                    // attributes: print-style
                    step = e_.readText();
                    if (e_.hasAttribute("text")) {
                        if (e_.attribute("text").empty()) {
                            invalidRoot = true;
                        }
                    }
                } else if (e_.name() == "root-alter") {
                    // attributes: print-object, print-style
                    //             location (left-right)
                    alter = e_.readText().trimmed().toInt();
                } else {
                    skipLogCurrElem();
                }
            }
            if (invalidRoot) {
                info->setRootTpc(Tpc::TPC_INVALID);
            } else {
                info->setRootTpc(step2tpc(step, AccidentalVal(alter)));
            }
        } else if (e_.name() == "function") {
            // deprecated in MusicXML 4.0
            // attributes: print-style
            info->setRootTpc(Tpc::TPC_INVALID);
            info->setBassTpc(Tpc::TPC_INVALID);
            functionText = e_.readText();
            ha->setHarmonyType(HarmonyType::ROMAN);
        } else if (e_.name() == "numeral") {
            info->setRootTpc(Tpc::TPC_INVALID);
            info->setBassTpc(Tpc::TPC_INVALID);
            while (e_.readNextStartElement()) {
                if (e_.name() == "numeral-root") {
                    functionText = e_.attribute("text");
                    const String numeralRoot = e_.readText();
                    if (functionText.isEmpty() || functionText.at(0).isDigit()) {
                        ha->setHarmonyType(HarmonyType::NASHVILLE);
                        setHarmonyRootTpcFromFunction(info, ha, numeralRoot, key);
                    } else {
                        ha->setHarmonyType(HarmonyType::ROMAN);
                    }
                } else if (e_.name() == "numeral-alter") {
                    const int alter = e_.readText().toInt();
                    switch (alter) {
                        case -1: setHarmonyRootTpcFromFunction(info, ha, u"b" + harmonyXmlFunction(info, ha, key), key); break;
                        case 1: setHarmonyRootTpcFromFunction(info, ha, u"#" + harmonyXmlFunction(info, ha, key), key); break;
                        default: break;
                    }
                } else {
                    skipLogCurrElem();
                }
            }
        } else if (e_.name() == "kind") {
            // attributes: use-symbols  yes-no
            //             text, stack-degrees, parentheses-degree, bracket-degrees,
            //             print-style, halign, valign
            kindText = e_.attribute("text");
            symbols = e_.attribute("use-symbols");
            parens = e_.attribute("parentheses-degrees");
            kind = e_.readText();
            if (kind == "none") {
                info->setRootTpc(Tpc::TPC_INVALID);
            }
        } else if (e_.name() == "inversion") {
            const int inversion = e_.readText().toInt();
            switch (inversion) {
                case 1: inversionText = u"6"; break;
                case 2: inversionText = u"64"; break;
                default: inversionText = u""; break;
            }
        } else if (e_.name() == "bass") {
            String step;
            int alter = 0;
            while (e_.readNextStartElement()) {
                if (e_.name() == "bass-step") {
                    // attributes: print-style
                    step = e_.readText();
                } else if (e_.name() == "bass-alter") {
                    // attributes: print-object, print-style
                    //             location (left-right)
                    alter = e_.readText().trimmed().toInt();
                } else {
                    skipLogCurrElem();
                }
            }
            info->setBassTpc(step2tpc(step, AccidentalVal(alter)));
        } else if (e_.name() == "degree") {
            int degreeValue = 0;
            int degreeAlter = 0;
            String degreeType;
            while (e_.readNextStartElement()) {
                if (e_.name() == "degree-value") {
                    degreeValue = e_.readInt();
                } else if (e_.name() == "degree-alter") {
                    degreeAlter = e_.readText().trimmed().toInt();
                } else if (e_.name() == "degree-type") {
                    degreeType = e_.readText();
                } else {
                    skipLogCurrElem();
                }
            }
            if (degreeValue <= 0 || degreeValue > 13 || degreeAlter < -2 || degreeAlter > 2 ||
                (degreeType != u"add" && degreeType != u"alter" && degreeType != u"subtract")) {
                logger_->logError(String(u"incorrect degree: degreeValue=%1 degreeAlter=%2 degreeType=%3")
                                      .arg(degreeValue)
                                      .arg(degreeAlter)
                                      .arg(degreeType),
                                  &e_);
            } else {
                if (degreeType == "add") {
                    degreeList.emplace_back(degreeValue, degreeAlter, HDegreeType::ADD);
                } else if (degreeType == "alter") {
                    degreeList.emplace_back(degreeValue, degreeAlter, HDegreeType::ALTER);
                } else if (degreeType == "subtract") {
                    degreeList.emplace_back(degreeValue, degreeAlter, HDegreeType::SUBTRACT);
                }
            }
        } else if (e_.name() == "frame") {
            fd = frame();
        } else if (e_.name() == "level") {
            skipLogCurrElem();
        } else if (e_.name() == "offset") {
            offset = pass1_.calcTicks(e_.readInt(), divs_, &e_);
            preventNegativeTick(sTime, offset, logger_);
        } else if (e_.name() == "staff") {
            size_t nstaves = pass1_.getPart(partId)->nstaves();
            String strStaff = e_.readText();
            int staffIdx = pass1_.getMusicXmlPart(partId).staffNumberToIndex(strStaff.toInt());
            if (staffIdx >= 0 && staffIdx < int(nstaves)) {
                track += staffIdx * VOICES;
            } else {
                logger_->logError(String(u"invalid staff %1").arg(strStaff), &e_);
            }
        } else {
            skipLogCurrElem();
        }
    }

    const ChordDescription *d = nullptr;
    if (info->rootTpc() != Tpc::TPC_INVALID || ha->harmonyType() == HarmonyType::NASHVILLE) {
        d = harmonyFromXml(info, score_, kind, kindText, symbols, parens, degreeList);
    }
    if (d) {
        info->setId(d->id);
        info->setTextName(d->names.front());
    } else {
        info->setId(-1);
        String textName = functionText + kindText + inversionText;
        info->setTextName(textName);
    }
    ha->addChord(info);

    ha->setVisible(printObject);
    if (placement == u"below") {
        ha->setPlacement(PlacementV::BELOW);
        ha->resetProperty(Pid::OFFSET);
    }
    colorItem(ha, color);
    if (!fontFamily.empty()) {
        ha->setFamily(fontFamily);
        ha->setPropertyFlags(Pid::FONT_FACE, PropertyFlags::UNSTYLED);
    }

    const HarmonyDesc newHarmonyDesc(track, ha, fd);
    bool insert = true;
    if (pass1_.sibOrDolet()) {
        const int ticks = (sTime + offset).ticks();
        for (auto itr = harmonyMap.begin(); itr != harmonyMap.end(); itr++) {
            if (itr->first != ticks) {
                continue;
            }
            HarmonyDesc &foundHarmonyDesc = itr->second;
            if (track2staff(foundHarmonyDesc.track_) == track2staff(track) && foundHarmonyDesc.harmony_ &&
                foundHarmonyDesc.descr() == info->descr()) {
                if (foundHarmonyDesc.harmony_ && foundHarmonyDesc.fretDiagramVisible() == newHarmonyDesc.fretDiagramVisible()) {
                    // Matching harmony with matching visibility of fret diagram.  No need to add
                    insert = false;
                } else if (fd && fd->visible() && !foundHarmonyDesc.fretDiagramVisible()) {
                    // Matching harmony without a fret diagram found at this tick, replace with this harmony and its fret diagram
                    foundHarmonyDesc.harmony_ = ha;
                    foundHarmonyDesc.fretDiagram_ = fd;
                    foundHarmonyDesc.track_ = track;
                    insert = false;
                }
            }
        }
    }

    if (insert) {
        // No harmony at this tick, add to the map
        harmonyMap.insert(std::pair<int, HarmonyDesc>((sTime + offset).ticks(), newHarmonyDesc));
    }
}

//---------------------------------------------------------
//   beam
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/beam node.
 Collects beamTypes, used in computeBeamMode.
 */

void MusicXmlParserPass2::beam(std::map<int, String> &beamTypes) {
    bool hasBeamNo;
    int beamNo = e_.asciiAttribute("number").toInt(&hasBeamNo);
    String s = e_.readText();

    beamTypes.insert({hasBeamNo ? beamNo : 1, s});
}

//---------------------------------------------------------
//   forward
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/forward node.
 */

void MusicXmlParserPass2::forward(Fraction &dura) {
    while (e_.readNextStartElement()) {
        if (e_.name() == "duration") {
            duration(dura);
        } else if (e_.name() == "staff") {
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "voice") {
            e_.skipCurrentElement(); // skip but don't log
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   backup
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/backup node.
 */

void MusicXmlParserPass2::backup(Fraction &dura) {
    while (e_.readNextStartElement()) {
        if (e_.name() == "duration") {
            duration(dura);
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   MusicXmlParserLyric
//---------------------------------------------------------

MusicXmlParserLyric::MusicXmlParserLyric(const LyricNumberHandler lyricNumberHandler, XmlStreamReader &e, Score *score,
                                         const IMusicXmlConfiguration &configuration, MusicXmlLogger *logger, bool isVoiceStaff)
    : lyricNumberHandler_(lyricNumberHandler), e_(e), score_(score), configuration_(configuration), logger_(logger),
      isVoiceStaff_(isVoiceStaff) {
    // nothing
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserLyric::skipLogCurrElem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skipCurrentElement();
}

void MusicXmlParserLyric::readElision(String &formattedText) {
    const String text = e_.readText();
    const String smufl = e_.attribute("smufl");
    if (!text.empty()) {
        formattedText += text;
    } else if (!smufl.empty()) {
        formattedText += u"<sym>" + smufl + u"</sym>";
    } else {
        formattedText += u"<sym>lyricsElision</sym>";
    }
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

void MusicXmlParserLyric::parse(bool visibility) {
    bool hasExtend = false;
    const String lyricNumber = e_.attribute("number");
    const Color lyricColor = Color::fromString(e_.asciiAttribute("color").ascii());
    const bool printLyric = visibility ? e_.asciiAttribute("print-object") != "no" : e_.asciiAttribute("print-object") == "yes";
    placement_ = e_.attribute("placement");
    double relX = e_.doubleAttribute("relative-x") * 0.1 * DPMM;
    relativeY_ = e_.doubleAttribute("relative-y") * -0.1 * DPMM;
    defaultY_ = e_.doubleAttribute("default-y") * -0.1 * DPMM;
    LyricsSyllabic syllabic = LyricsSyllabic::SINGLE;

    String extendType;
    String formattedText;

    while (e_.readNextStartElement()) {
        if (e_.name() == "elision") {
            readElision(formattedText);
        } else if (e_.name() == "extend") {
            hasExtend = true;
            extendType = e_.attribute("type");
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "syllabic") {
            String syll = e_.readText();
            if (syll == "single") {
                syllabic = LyricsSyllabic::SINGLE;
            } else if (syll == "begin") {
                syllabic = LyricsSyllabic::BEGIN;
            } else if (syll == "end") {
                syllabic = LyricsSyllabic::END;
            } else if (syll == "middle") {
                syllabic = LyricsSyllabic::MIDDLE;
            } else {
                LOGD("unknown syllabic %s", muPrintable(syll)); // TODO
            }
        } else if (e_.name() == "text") {
            formattedText += xmlpass2::nextPartOfFormattedString(e_, configuration_, *score_->engravingFonts());
        } else {
            skipLogCurrElem();
        }
    }

    // if no lyric read (e.g. only 'extend "type=stop"'), no further action required
    if (formattedText.empty()) {
        return;
    }

    const int lyricNo = lyricNumberHandler_.getLyricNo(lyricNumber);
    if (lyricNo < 0) {
        logger_->logError(u"invalid lyrics number (<0)", &e_);
        return;
    } else if (lyricNo > MAX_LYRICS) {
        logger_->logError(String(u"too much lyrics (>%1)").arg(MAX_LYRICS), &e_);
        return;
    } else if (muse::contains(numberedLyrics_, lyricNo)) {
        logger_->logError(String(u"duplicate lyrics number (%1)").arg(lyricNumber), &e_);
        return;
    }

    TextBase *item = nullptr;
    if (isLikelySticking(formattedText, syllabic, hasExtend)) {
        item = Factory::createSticking(score_->dummy());
    } else {
        item = Factory::createLyrics(score_->dummy());
    }

    //LOGD("formatted lyric '%s'", muPrintable(formattedText));
    item->setXmlText(formattedText);
    colorItem(item, lyricColor);
    item->setVisible(printLyric);

    item->setPlacement(placement() == "above" ? PlacementV::ABOVE : PlacementV::BELOW);
    item->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    item->resetProperty(Pid::OFFSET);

    if (!RealIsNull(relX)) {
        PointF offset = item->offset();
        offset.setX(relX);
        item->setOffset(offset);
        item->setPropertyFlags(Pid::OFFSET, PropertyFlags::UNSTYLED);
    }

    if (item->isLyrics()) {
        // Add lyric
        Lyrics *l = toLyrics(item);
        l->setSyllabic(syllabic);
        numberedLyrics_[lyricNo] = l;

        if (hasExtend && (extendType.empty() || extendType == "start") &&
            (l->syllabic() == LyricsSyllabic::SINGLE || l->syllabic() == LyricsSyllabic::END)) {
            extendedLyrics_.insert(l);
        }
    } else if (item->isSticking()) {
        // Add sticking
        Sticking *s = toSticking(item);
        inferredStickings_.push_back(s);
    }
}

String MusicXmlParserLyric::placement() const {
    if (placement_ == "" && hasTotalY()) {
        return totalY() < 0 ? u"above" : u"below";
    } else {
        return placement_;
    }
}

bool MusicXmlParserLyric::isLikelySticking(const String &text, const LyricsSyllabic syllabic, const bool hasExtend) {
    if (!configuration_.inferTextType()) {
        return false;
    }
    String plainWords = MScoreTextToMusicXml::toPlainText(text.simplified());
    static const std::wregex sticking(L"^[lrbLRB]+$");
    return plainWords.contains(sticking) && syllabic == LyricsSyllabic::SINGLE && !hasExtend && !isVoiceStaff_;
}

//---------------------------------------------------------
//   slur
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/slur node.
 */

void MusicXmlParserNotations::slur() {
    Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"notations");
    notation.setVisible(visible_);
    notations_.push_back(notation);

    // any grace note containing a slur stop means
    // last note of a grace after set has been found
    // -> remember slur stop
    if (notation.attribute(u"type") == u"stop") {
        slurStop_ = true;
    } else if (notation.attribute(u"type") == u"start") {
        slurStart_ = true;
    }

    e_.skipCurrentElement(); // skip but don't log
}

//---------------------------------------------------------
//   addSlur
//---------------------------------------------------------

static void addSlur(const Notation &notation, SlurStack &slurs, ChordRest *cr, Note *note, const Fraction &tick, MusicXmlLogger *logger,
                    const XmlStreamReader *const xmlreader, bool importLayout) {
    int slurNo = notation.attribute(u"number").toInt();
    if (slurNo > 0) {
        slurNo--;
    }
    const String slurType = notation.attribute(u"type");

    const track_idx_t track = cr->track();
    Score *score = cr->score();

    // PriMus Music-Notation by Columbussoft (build 10093) generates overlapping
    // slurs that do not have a number attribute to distinguish them.
    // The duplicates must be ignored, to prevent memory allocation issues,
    // which caused a MuseScore crash
    // Similar issues happen with Sibelius 7.1.3 (direct export)

    if (slurType == u"start") {
        if (slurs[slurNo].isStart()) {
            // slur start when slur already started: report error
            logger->logError(String(u"ignoring duplicate slur start"), xmlreader);
        } else if (slurs[slurNo].isStop()) {
            // slur start when slur already stopped: wrap up
            Slur *newSlur = slurs[slurNo].slur();
            slurs[slurNo] = SlurDesc();

            if (newSlur->endElement() == cr && note) {
                delete newSlur;

                // Slur starts & ends on same chord - add lv instead
                LaissezVib *lvTie = Factory::createLaissezVib(note);
                note->score()->undoAddElement(lvTie);
                return;
            }
            const Fraction tick2 = newSlur->endElement()->tick();
            if (tick2 < tick) {
                logger->logError(String(u"slur end is before slur start"), xmlreader);
                delete newSlur;
                return;
            }
            newSlur->setTrack(track);
            newSlur->setTick(tick);
            newSlur->setStartElement(cr);
            newSlur->setTick2(tick2);
            score->addElement(newSlur);
        } else {
            // slur start for new slur: init
            Slur *newSlur =
                notation.name() == "slur" ? Factory::createSlur(score->dummy()) : toSlur(Factory::createHammerOnPullOff(score->dummy()));
            const String lineType = notation.attribute(u"line-type");
            if (lineType == u"dashed") {
                newSlur->setStyleType(SlurStyleType::Dashed);
            } else if (lineType == u"dotted") {
                newSlur->setStyleType(SlurStyleType::Dotted);
            } else if (lineType == u"solid" || lineType.empty()) {
                newSlur->setStyleType(SlurStyleType::Solid);
            }
            newSlur->setVisible(notation.visible());
            colorItem(newSlur, Color::fromString(notation.attribute(u"color")));
            newSlur->setTick(tick);
            newSlur->setStartElement(cr);
            if (importLayout) {
                const String orientation = notation.attribute(u"orientation");
                const String placement = notation.attribute(u"placement");
                if (orientation == u"over" || placement == u"above") {
                    newSlur->setSlurDirection(DirectionV::UP);
                } else if (orientation == u"under" || placement == u"below") {
                    newSlur->setSlurDirection(DirectionV::DOWN);
                } else if (!orientation.empty() || !placement.empty()) {
                    logger->logError(String(u"unknown slur orientation/placement: %1/%2").arg(orientation).arg(placement), xmlreader);
                }
            }
            newSlur->setTrack(track);
            newSlur->setTrack2(track);
            slurs[slurNo].start(newSlur);
        }
    } else if (slurType == u"stop") {
        if (slurs[slurNo].isStart()) {
            // slur stop when slur already started: wrap up
            Slur *newSlur = slurs[slurNo].slur();
            slurs[slurNo] = SlurDesc();

            if (newSlur->startElement() == cr && note) {
                delete newSlur;

                // Slur starts & ends on same chord - add lv instead
                LaissezVib *lvTie = Factory::createLaissezVib(note);
                note->score()->undoAddElement(lvTie);
                return;
            }

            if (!(cr->isGrace())) {
                newSlur->setTick2(tick);
                newSlur->setTrack2(track);
            }
            newSlur->setEndElement(cr);
            score->addElement(newSlur);
        } else if (slurs[slurNo].isStop()) {
            // slur stop when slur already stopped: report error
            logger->logError(String(u"ignoring duplicate slur stop"), xmlreader);
        } else {
            // slur stop for new slur: init
            Slur *newSlur = Factory::createSlur(score->dummy());
            if (!(cr->isGrace())) {
                newSlur->setTrack2(track);
            }
            newSlur->setEndElement(cr);
            slurs[slurNo].stop(newSlur);
        }
    } else if (slurType == "continue") {
        // ignore
    } else {
        logger->logError(String(u"unknown slur type %1").arg(slurType), xmlreader);
    }
}

//---------------------------------------------------------
//   tied
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/tied node.
 */

void MusicXmlParserNotations::tied() {
    Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"notations");
    notation.setVisible(visible_);
    // Make sure "stops" get processed before "starts"
    if (notation.attribute(u"type") == u"stop") {
        notations_.insert(notations_.begin(), notation);
    } else {
        notations_.push_back(notation);
    }
    String tiedType = notation.attribute(u"type");
    if (tiedType != u"start" && tiedType != u"stop" && tiedType != u"let-ring") {
        logger_->logError(String(u"unknown tied type %1").arg(tiedType), &e_);
    }

    e_.skipCurrentElement(); // skip but don't log
}

//---------------------------------------------------------
//   dynamics
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/dynamics node.
 */

void MusicXmlParserNotations::dynamics() {
    // The existing notation importer does not apply the parent visibility to
    // note-attached dynamics; preserve that policy when grouping children.
    const MusicXmlDynamic dynamic = parseDynamicsContainer(e_, logger_);
    if (!dynamic.text.empty()) {
        dynamicsList_.push_back(dynamic);
    }
}

//---------------------------------------------------------
//   articulations
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/articulations node.
 Note that some notations attach to notes only in MuseScore,
 which means trying to attach them to a rest will crash,
 as in that case note is 0.
 */

void MusicXmlParserNotations::articulations() {
    while (e_.readNextStartElement()) {
        SymId id{SymId::noSym};
        if (convertArticulationToSymId(String::fromAscii(e_.name().ascii()), id)) {
            if (e_.name() == "detached-legato") {
                Notation artic = Notation::notationWithAttributes(u"tenuto", e_.attributes(), u"articulations", SymId::articTenutoAbove);
                artic.setVisible(visible_);
                notations_.push_back(artic);
                artic = Notation::notationWithAttributes(u"staccato", e_.attributes(), u"articulations", SymId::articStaccatoAbove);
                artic.setVisible(visible_);
                notations_.push_back(artic);
            } else {
                Notation artic =
                    Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"articulations", id);
                artic.setVisible(visible_);
                notations_.push_back(artic);
            }
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "breath-mark") {
            std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
            String value = e_.readText();
            engraving::SymId breath = SymId::noSym;
            if (value == "tick") {
                breath = SymId::breathMarkTick;
            } else if (value == "upbow") {
                breath = SymId::breathMarkUpbow;
            } else if (value == "salzedo") {
                breath = SymId::breathMarkSalzedo;
            } else {
                // Use comma as the default symbol
                breath = SymId::breathMarkComma;
            }
            Notation notation = Notation::notationWithAttributes(u"breath", attributes, u"articulations", breath);
            notation.setVisible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "caesura") {
            std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
            String value = e_.readText();
            engraving::SymId caesura = SymId::noSym;
            if (value == "curved") {
                caesura = SymId::caesuraCurved;
            } else if (value == "short") {
                caesura = SymId::caesuraShort;
            } else if (value == "thick") {
                caesura = SymId::caesuraThick;
            } else if (value == "single") {
                caesura = SymId::caesuraSingleStroke;
            } else { // Use as the default symbol
                caesura = SymId::caesura;
            }
            Notation notation = Notation::notationWithAttributes(u"breath", attributes, u"articulations", caesura);
            notation.setVisible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "doit" || e_.name() == "falloff" || e_.name() == "plop" || e_.name() == "scoop") {
            Notation artic = Notation::notationWithAttributes(u"chord-line", e_.attributes(), u"articulations");
            artic.setSubType(String::fromAscii(e_.name().ascii()));
            artic.setVisible(visible_);
            notations_.push_back(artic);
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "other-articulation") {
            const String smufl = e_.attribute("smufl");
            SymId sid = SymNames::symIdByName(smufl, SymId::noSym);
            Notation artic = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"articulations", sid);
            const String articText = e_.readText();
            artic.setText(articText);

            if (!smufl.empty() || !articText.empty()) {
                artic.setVisible(visible_);
                notations_.push_back(artic);
            }
        } else {
            skipLogCurrElem();
        }
    }
}

//---------------------------------------------------------
//   ornaments
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/ornaments node.
 */

void MusicXmlParserNotations::ornaments() {
    bool trillMark = false;
    // <trill-mark placement="above"/>
    while (e_.readNextStartElement()) {
        SymId id{SymId::noSym};
        if (convertArticulationToSymId(String::fromAscii(e_.name().ascii()), id)) {
            Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"ornaments", id);
            notation.setVisible(visible_);
            notations_.push_back(notation);
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "trill-mark") {
            trillMark = true;
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "wavy-line") {
            bool wavyLineTypeWasStart = (wavyLineType_ == "start");
            wavyLineType_ = e_.attribute("type");
            wavyLineNo_ = e_.intAttribute("number");
            if (wavyLineNo_ > 0) {
                wavyLineNo_--;
            }
            // any grace note containing a wavy-line stop means
            // last note of a grace after set has been found
            // remember wavy-line stop
            if (wavyLineType_ == u"stop") {
                wavyLineStop_ = true;
            }
            // check for start and stop on same note
            if (wavyLineTypeWasStart && wavyLineType_ == u"stop") {
                wavyLineType_ = u"startstop";
            }
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "tremolo") {
            hasTremolo_ = true;
            tremoloColor_ = Color::fromString(e_.attribute("color"));
            tremoloType_ = e_.attribute("type");
            tremoloSmufl_ = e_.attribute("smufl");
            tremoloNr_ = e_.readInt();
        } else if (e_.name() == "inverted-mordent" || e_.name() == "mordent") {
            mordentNormalOrInverted();
        } else if (e_.name() == "other-ornament") {
            Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"ornaments");
            notation.setVisible(visible_);
            notations_.push_back(notation);
            e_.skipCurrentElement(); // skip but don't log
        } else {
            skipLogCurrElem();
        }
    }

    // note that mscore wavy line already implicitly includes a trillsym
    // so don't add an additional one
    if (trillMark && wavyLineType_ != "start" && wavyLineType_ != "startstop") {
        Notation ornament = Notation::notationWithAttributes(u"trill-mark", e_.attributes(), u"ornaments", SymId::ornamentTrill);
        ornament.setVisible(visible_);
        notations_.push_back(ornament);
    }
}

//---------------------------------------------------------
//   technical
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/technical node.
 */

void MusicXmlParserNotations::technical() {
    while (e_.readNextStartElement()) {
        SymId id{SymId::noSym};
        const String smufl = e_.attribute("smufl");
        if (!smufl.empty()) {
            id = SymNames::symIdByName(smufl, SymId::noSym);
            Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"technical", id);
            notation.setVisible(visible_);
            notations_.push_back(notation);
            e_.skipCurrentElement();
        } else if (convertArticulationToSymId(String::fromAscii(e_.name().ascii()), id)) {
            Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"technical", id);
            notation.setVisible(visible_);
            notations_.push_back(notation);
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "fingering" || e_.name() == "fret" || e_.name() == "pluck" || e_.name() == "string" || e_.name() == "tap") {
            Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"technical");
            notation.setText(e_.readText());
            notation.setVisible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "harmonic") {
            harmonic();
        } else if (e_.name() == "handbell") {
            const std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
            convertArticulationToSymId(e_.readText(), id);
            Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), attributes, u"technical", id);
            notation.setVisible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "harmon-mute") {
            harmonMute();
        } else if (e_.name() == "hole") {
            hole();
        } else if (e_.name() == "tap") {
            id = (e_.attribute("hand") == u"left") ? SymId::guitarLeftHandTapping : SymId::guitarRightHandTapping;
            Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"technical");
            notation.setVisible(visible_);
            notations_.push_back(notation);
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "other-technical") {
            otherTechnical();
        } else {
            skipLogCurrElem();
        }
    }
}

void MusicXmlParserNotations::otherTechnical() {
    const Color color = Color::fromString(e_.attribute("color"));
    const String text = e_.readText();

    if (text == u"z") {
        // Buzz roll
        hasTremolo_ = true;
        tremoloNr_ = 0;
        tremoloType_ = u"unmeasured";
        tremoloColor_ = color;
    }
}

//---------------------------------------------------------
//   harmonic
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/technical/harmonic node.
 */

void MusicXmlParserNotations::harmonic() {
    Notation notation =
        Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"technical", SymId::stringsHarmonic);

    while (e_.readNextStartElement()) {
        String name = String::fromAscii(e_.name().ascii());
        if (name == "natural") {
            notation.setSubType(name);
            notation.setVisible(visible_);
            e_.skipCurrentElement();       // skip but don't log
        } else if (name == "artificial") { // TODO: add artificial harmonic when supported by MuseScore
            logger_->logError(String(u"unsupported harmonic type/pitch '%1'").arg(name), &e_);
            notation.setSymId(SymId::noSym);
            e_.skipCurrentElement();
        } else {
            e_.skipCurrentElement();
        }
    }

    notations_.push_back(notation);
}

//---------------------------------------------------------
//   harmonMute
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/technical/harmon-mute node.
 */

void MusicXmlParserNotations::harmonMute() {
    engraving::SymId mute = SymId::brassHarmonMuteClosed;
    const std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
    while (e_.readNextStartElement()) {
        String name = String::fromAscii(e_.name().ascii());
        if (name == "harmon-closed") {
            const String location = e_.attribute("location");
            String value = e_.readText();
            if (value == "yes") {
                mute = SymId::brassHarmonMuteClosed;
            } else if (value == "no") {
                mute = SymId::brassHarmonMuteStemOpen;
            } else if (value == "half") {
                if (location == "left") {
                    mute = SymId::brassHarmonMuteStemHalfLeft;
                } else if (location == "right") {
                    mute = SymId::brassHarmonMuteStemHalfRight;
                } else {
                    logger_->logError(String(u"unsupported harmon-closed location '%1'").arg(location), &e_);
                    mute = SymId::brassHarmonMuteStemHalfLeft;
                }
            }
        } else {
            e_.skipCurrentElement();
        }
    }
    Notation notation = Notation::notationWithAttributes(u"harmon-closed", attributes, u"technical", mute);
    notation.setVisible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   hole
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/technical/hole node.
 */

void MusicXmlParserNotations::hole() {
    engraving::SymId hole = SymId::noSym;
    const std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
    while (e_.readNextStartElement()) {
        if (e_.name() == "hole-closed") {
            const String location = e_.attribute("location");
            const String value = e_.readText();
            if (value == "yes") {
                hole = SymId::windClosedHole;
            } else if (value == "no") {
                hole = SymId::windOpenHole;
            } else if (value == "half") {
                if (location == "bottom") {
                    hole = SymId::windHalfClosedHole2;
                } else if (location == "right") {
                    hole = SymId::windHalfClosedHole1;
                } else {
                    logger_->logError(String(u"unsupported hole-closed location '%1'").arg(location), &e_);
                    hole = SymId::windHalfClosedHole3;
                }
            }
        } else {
            e_.skipCurrentElement();
        }
    }
    Notation notation = Notation::notationWithAttributes(u"hole-closed", attributes, u"technical", hole);
    notation.setVisible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   addTechnical
//---------------------------------------------------------

void MusicXmlParserNotations::addTechnical(const Notation &notation, Note *note) {
    const String placement = notation.attribute(u"placement");
    const String fontWeight = notation.attribute(u"font-weight");
    const double fontSize = notation.attribute(u"font-size").toDouble();
    const String fontStyle = notation.attribute(u"font-style");
    const String fontFamily = notation.attribute(u"font-family");
    const Color color = Color::fromString(notation.attribute(u"color"));
    if (notation.name() == u"fingering") {
        // TODO: distinguish between keyboards (style TextStyleName::FINGERING)
        // and (plucked) strings (style TextStyleName::LH_GUITAR_FINGERING)
        addTextToNote(e_.byteOffset(), notation.text(), placement, fontWeight, fontSize, fontStyle, fontFamily, notation.visible(), color,
                      TextStyleType::FINGERING, score_, note, pass2_.configuration()->needUseDefaultFont());
    } else if (notation.name() == u"fret") {
        int fret = notation.text().toInt();
        if (note) {
            if (note->staff()->isTabStaff(Fraction(0, 1))) {
                note->setFret(fret);
            }
        } else {
            logger_->logError(u"no note for fret", &e_);
        }
    } else if (notation.name() == "pluck") {
        addTextToNote(e_.byteOffset(), notation.text(), placement, fontWeight, fontSize, fontStyle, fontFamily, notation.visible(), color,
                      TextStyleType::RH_GUITAR_FINGERING, score_, note, pass2_.configuration()->needUseDefaultFont());
    } else if (notation.name() == "string") {
        if (note) {
            if (note->staff()->isTabStaff(Fraction(0, 1))) {
                note->setString(notation.text().toInt() - 1);
            } else {
                addTextToNote(e_.byteOffset(), notation.text(), placement, fontWeight, fontSize, fontStyle, fontFamily, notation.visible(),
                              color, TextStyleType::STRING_NUMBER, score_, note, pass2_.configuration()->needUseDefaultFont());
            }
        } else {
            logger_->logError(u"no note for string", &e_);
        }
    } else if (notation.name() == u"tap") {
        if (note) {
            addTapToChord(notation, note->chord());
        } else {
            logger_->logError(u"no note for tap", &e_);
        }
    }
}

void MusicXmlParserNotations::arpeggio() {
    arpeggioType_ = e_.attribute("direction");
    if (arpeggioType_.empty()) {
        arpeggioType_ = u"none";
    }
    arpeggioNo_ = e_.intAttribute("number");
    if (arpeggioNo_ == 0) {
        arpeggioNo_ = 1;
    }
    arpeggioColor_ = Color::fromString(e_.attribute("color"));
    e_.skipCurrentElement(); // skip but don't log
}

//---------------------------------------------------------
//   mordentNormalOrInverted
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/ornaments/mordent
 and /score-partwise/part/measure/note/notations/ornaments/inverted-mordent nodes.
 */

void MusicXmlParserNotations::mordentNormalOrInverted() {
    Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"ornaments");
    notation.setText(e_.readText());
    notation.setVisible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   glissandoSlide
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/glissando
 and /score-partwise/part/measure/note/notations/slide nodes.
 */

void MusicXmlParserNotations::glissandoSlide() {
    Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"notations");
    notation.setText(e_.readText());
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   addGlissandoSlide
//---------------------------------------------------------

static void addGlissandoSlide(const Notation &notation, Note *note, Glissando *glissandi[MAX_NUMBER_LEVEL][2], MusicXmlSpannerMap &spanners,
                              MusicXmlLogger *logger, const XmlStreamReader *const xmlreader) {
    int glissandoNumber = notation.attribute(u"number").toInt();
    if (glissandoNumber > 0) {
        glissandoNumber--;
    }
    const String glissandoType = notation.attribute(u"type");
    const bool glissandoTag = notation.name() != u"slide";
    const String lineType = notation.attribute(u"line-type");
    Glissando *&gliss = glissandi[glissandoNumber][glissandoTag];

    const Fraction tick = note->tick();
    const track_idx_t track = note->track();

    if (glissandoType == u"start") {
        const String glissandoText = notation.text();
        if (gliss) {
            logger->logError(String(u"overlapping glissando/slide number %1").arg(glissandoNumber + 1), xmlreader);
        } else if (!note) {
            logger->logError(String(u"no note for glissando/slide number %1 start").arg(glissandoNumber + 1), xmlreader);
        } else {
            gliss = Factory::createGlissando(note);
            gliss->setStartElement(note);
            gliss->setTick(tick);
            gliss->setTrack(track);
            gliss->setVisible(notation.visible());
            colorItem(gliss, Color::fromString(notation.attribute(u"color")));
            if (lineType == u"dashed") {
                gliss->setLineStyle(LineType::DASHED);
            } else if (lineType == u"dotted") {
                gliss->setLineStyle(LineType::DOTTED);
            } else if (lineType == u"solid" || lineType.empty()) {
                gliss->setLineStyle(LineType::SOLID);
            }
            gliss->setText(glissandoText);
            gliss->setGlissandoType(glissandoTag || (lineType == u"wavy") ? GlissandoType::WAVY : GlissandoType::STRAIGHT);
            const GlissandoStyle instrumentStyle = note->part()->instrument(tick)->glissandoStyle();
            if (instrumentStyle != gliss->glissandoStyle()) {
                gliss->setGlissandoStyle(instrumentStyle);
                // the instrument's style, not the score's: keep it when the score is saved
                gliss->setPropertyFlags(Pid::GLISS_STYLE, PropertyFlags::UNSTYLED);
            }
            spanners[gliss] = std::pair<int, int>(tick.ticks(), -1);
            // LOGD("glissando/slide=%p inserted at first tick %d", gliss, tick);
        }
    } else if (glissandoType == u"stop") {
        if (!gliss) {
            logger->logError(String(u"glissando/slide number %1 stop without start").arg(glissandoNumber + 1), xmlreader);
        } else if (!note) {
            logger->logError(String(u"no note for glissando/slide number %1 stop").arg(glissandoNumber + 1), xmlreader);
        } else if (gliss->tick() == tick) {
            // A gliss starting and ending on the same tick is a fall
            ChordLine *cl = Factory::createChordLine(note->chord());
            cl->setChordLineType(ChordLineType::FALL);
            cl->setWavy(gliss->glissandoType() == GlissandoType::WAVY ? true : false);
            cl->setStraight(true);
            cl->setOwnershipParent(note);
            note->chord()->add(cl);
            spanners.erase(gliss);
            delete gliss;
            gliss = nullptr;
        } else {
            // Remove glissandos longer than 16 beats
            if ((tick + note->chord()->ticks()) - gliss->tick() >= Fraction(4, 1)) {
                logger->logError(String(u"glissando/slide number %1 too long (%2)")
                                     .arg(glissandoNumber + 1, ((tick + note->chord()->ticks()) - gliss->tick()).ticks()),
                                 xmlreader);
                spanners.erase(gliss);
            } else {
                spanners[gliss].second = tick.ticks() + note->chord()->ticks().ticks();
                gliss->setEndElement(note);
                gliss->setTick2(tick);
                gliss->setTrack2(track);
                // LOGD("glissando/slide=%p second tick %d", gliss, tick);
            }
            gliss = nullptr;
        }
    } else {
        logger->logError(String(u"unknown glissando/slide type %1").arg(glissandoType), xmlreader);
    }
}

//---------------------------------------------------------
//   addArpeggio
//---------------------------------------------------------

static void addArpeggio(ChordRest *cr, String &arpeggioType, int arpeggioNo, Color arpeggioColor, ArpeggioMap &arpMap,
                        DelayedArpMap &delayedArps) {
    if (cr->isRest() && !arpeggioType.empty()) {
        // If the arpeggio is attached to a rest, store to add to the next available chord
        DelayedArpeggio delayedArp(arpeggioType, arpeggioNo);
        delayedArps.insert(std::pair<int, DelayedArpeggio>(cr->tick().ticks(), delayedArp));
    } else {
        // Retrieve stored arpeggio to add to this chord
        DelayedArpeggio delayedArp = muse::value(delayedArps, cr->tick().ticks(), DelayedArpeggio(u"", 0));
        if (!delayedArp.arpeggioType_.empty()) {
            arpeggioType = delayedArp.arpeggioType_;
            arpeggioNo = delayedArp.arpeggioNo_;
            delayedArps.erase(cr->tick().ticks());
        }
    }

    // If no current arpeggio with same number add new
    // If not, expand span
    // no support for arpeggio on rest
    const std::vector<MusicXmlArpeggioDesc> arps = muse::values(arpMap, cr->tick().ticks());
    Arpeggio *curArp = nullptr;
    for (const MusicXmlArpeggioDesc arp : arps) {
        if (arp.no == arpeggioNo) {
            curArp = arp.arp;
        }
    }

    if (curArp) {
        track_idx_t chordTrack = cr->track();
        track_idx_t arpTrack = curArp->track();
        track_idx_t span = cr->track() - curArp->track();
        if (chordTrack > arpTrack && span != 0) {
            curArp->setSpan(static_cast<int>(span + 1));
        }
    } else {
        if (!arpeggioType.empty() && cr->isChord()) {
            Arpeggio *arpeggio = Factory::createArpeggio(mu::engraving::toChord(cr));
            arpeggio->setArpeggioType(ArpeggioType::NORMAL);
            if (arpeggioType == "up") {
                arpeggio->setArpeggioType(ArpeggioType::UP);
            } else if (arpeggioType == "down") {
                arpeggio->setArpeggioType(ArpeggioType::DOWN);
            } else if (arpeggioType == "non-arpeggiate") {
                arpeggio->setArpeggioType(ArpeggioType::BRACKET);
            }
            colorItem(arpeggio, arpeggioColor);
            // there can be only one
            if (!(toChord(cr))->arpeggio()) {
                cr->add(arpeggio);

                MusicXmlArpeggioDesc arpDesc(arpeggio, arpeggioNo);
                arpMap.insert(std::pair<int, MusicXmlArpeggioDesc>(cr->tick().ticks(), arpDesc));
            }
        }
    }
}

//---------------------------------------------------------
//   addTie
//---------------------------------------------------------

static void addTie(const Notation &notation, Note *note, const track_idx_t track, MusicXmlTieMap &ties,
                   std::vector<Note *> &unstartedTieNotes, std::vector<Note *> &unendedTieNotes, MusicXmlLogger *logger,
                   const XmlStreamReader *const xmlreader, bool importLayout) {
    IF_ASSERT_FAILED(note) { return; }

    const String type = notation.attribute(u"type");
    const String orientation = notation.attribute(u"orientation");
    const String placement = notation.attribute(u"placement");
    const String lineType = notation.attribute(u"line-type");

    TieLocation loc = TieLocation(note->pitch(), note->track());

    if (type.empty()) {
        // ignore, nothing to do
    } else if (type == u"start") {
        if (Tie *activeTie = muse::value(ties, loc, nullptr)) {
            logger->logError(String(u"Tie already active"), xmlreader);
            unendedTieNotes.push_back(activeTie->startNote());
            ties.erase(loc);
        }
        ties[loc] = Factory::createTie(note);
        Tie *currTie = ties[loc];
        note->setTieFor(currTie);
        currTie->setStartNote(note);
        currTie->setTrack(track);
        currTie->setVisible(notation.visible());
        colorItem(currTie, Color::fromString(notation.attribute(u"color")));

        if (importLayout) {
            if (orientation == u"over" || placement == u"above") {
                currTie->setSlurDirection(DirectionV::UP);
            } else if (orientation == u"under" || placement == u"below") {
                currTie->setSlurDirection(DirectionV::DOWN);
            } else if (!orientation.empty() || !placement.empty()) {
                logger->logError(String(u"unknown tied orientation/placement: %1/%2").arg(orientation).arg(placement), xmlreader);
            }
        }

        if (lineType == u"dashed") {
            currTie->setStyleType(SlurStyleType::Dashed);
        } else if (lineType == u"dotted") {
            currTie->setStyleType(SlurStyleType::Dotted);
        } else if (lineType == u"solid" || lineType.empty()) {
            currTie->setStyleType(SlurStyleType::Solid);
        }
        currTie = nullptr;
    } else if (type == "stop") {
        if (Tie *currTie = muse::value(ties, loc, nullptr)) {
            const Note *startNote = currTie->startNote();
            const Chord *startChord = startNote ? startNote->chord() : nullptr;
            const Chord *endChord = note->chord();
            const Measure *startMeasure = startChord ? startChord->measure() : nullptr;
            if (startMeasure == endChord->measure() ||
                (startChord && startChord->tick() + startChord->measure()->ticks() >= endChord->tick())) {
                // only connect if they're in the same bar or no further than a full measure apart
                currTie->setEndNote(note);
                note->setTieBack(currTie);
            } else {
                logger->logError(String(u"Intervening note in voice"), xmlreader);
                unstartedTieNotes.push_back(note);
                unendedTieNotes.push_back(currTie->startNote());
            }
            ties.erase(loc);
        } else {
            unstartedTieNotes.push_back(note);
            logger->logError(String(u"Non-started tie terminated. No-op."), xmlreader);
        }
    } else if (type == "let-ring") {
        LaissezVib *lvTie = Factory::createLaissezVib(note);
        lvTie->setVisible(notation.visible());
        colorItem(lvTie, Color::fromString(notation.attribute(u"color")));

        if (importLayout) {
            if (orientation == u"over" || placement == u"above") {
                lvTie->setSlurDirection(DirectionV::UP);
            } else if (orientation == u"under" || placement == u"below") {
                lvTie->setSlurDirection(DirectionV::DOWN);
            } else if (!orientation.empty() || !placement.empty()) {
                logger->logError(String(u"unknown tied orientation/placement: %1/%2").arg(orientation).arg(placement), xmlreader);
            }
        }

        if (lineType == u"dashed") {
            lvTie->setStyleType(SlurStyleType::Dashed);
        } else if (lineType == u"dotted") {
            lvTie->setStyleType(SlurStyleType::Dotted);
        } else if (lineType == u"solid" || lineType.empty()) {
            lvTie->setStyleType(SlurStyleType::Solid);
        }

        note->score()->undoAddElement(lvTie);
    } else {
        logger->logError(String(u"unknown tied type %1").arg(type), xmlreader);
    }
}

//---------------------------------------------------------
//   addWavyLine
//---------------------------------------------------------

static void addWavyLine(ChordRest *cr, const Fraction &tick, const int wavyLineNo, const String &wavyLineType, MusicXmlSpannerMap &spanners,
                        TrillStack &trills, MusicXmlLogger *logger, const XmlStreamReader *const xmlreader) {
    if (!wavyLineType.empty()) {
        const Fraction ticks = cr->ticks();
        const track_idx_t track = cr->track();
        Trill *&trill = trills[wavyLineNo];
        if (wavyLineType == u"start" || wavyLineType == u"startstop") {
            if (trill) {
                logger->logError(String(u"overlapping wavy-line number %1").arg(wavyLineNo + 1), xmlreader);
            } else {
                trill = Factory::createTrill(cr->score()->dummy());
                trill->setTrack(track);
                trill->setTrack2(track);

                trill->setOrnament(Factory::createOrnament(cr));
                trill->ornament()->setAnchor(ArticulationAnchor::AUTO);

                if (wavyLineType == u"start") {
                    spanners[trill] = std::pair<int, int>(tick.ticks(), -1);
                    // LOGD("trill=%p inserted at first tick %d", trill, tick);
                }
                if (wavyLineType == u"startstop") {
                    spanners[trill] = std::pair<int, int>(tick.ticks(), tick.ticks() + ticks.ticks());
                    trill = nullptr;
                    // LOGD("trill=%p inserted at first tick %d second tick %d", trill, tick, tick);
                }
            }
        } else if (wavyLineType == u"stop") {
            if (!trill) {
                logger->logError(String(u"wavy-line number %1 stop without start").arg(wavyLineNo + 1), xmlreader);
            } else {
                spanners[trill].second = tick.ticks() + ticks.ticks();
                // LOGD("trill=%p second tick %d", trill, tick);
                trill = nullptr;
            }
        } else {
            logger->logError(String(u"unknown wavy-line type %1").arg(wavyLineType), xmlreader);
        }
    }
}

//---------------------------------------------------------
//   addBreath
//---------------------------------------------------------

static void addBreath(const Notation &notation, ChordRest *cr) {
    const SymId breath = notation.symId();
    const String placement = notation.attribute(u"placement");

    Segment *const seg = cr->measure()->getSegment(SegmentType::Breath, cr->tick() + cr->ticks());
    Breath *const b = Factory::createBreath(seg);
    // b->setTrack(trk + voice); TODO check next line
    b->setTrack(cr->track());
    b->setSymId(breath);
    b->setVisible(notation.visible());
    b->setPlacement(placement == u"below" ? PlacementV::BELOW : PlacementV::ABOVE);
    b->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    colorItem(b, Color::fromString(notation.attribute(u"color")));
    seg->add(b);
}

//---------------------------------------------------------
//   addChordLine
//---------------------------------------------------------

static void addChordLine(const Notation &notation, Note *note, MusicXmlLogger *logger, const XmlStreamReader *const xmlreader) {
    const String chordLineType = notation.subType();
    const String lineType = notation.attribute(u"line-type");
    const String lineShape = notation.attribute(u"line-shape");
    if (!chordLineType.empty()) {
        if (note) {
            ChordLine *const chordline = Factory::createChordLine(note->chord());
            if (chordLineType == u"falloff") {
                chordline->setChordLineType(ChordLineType::FALL);
            } else if (chordLineType == u"doit") {
                chordline->setChordLineType(ChordLineType::DOIT);
            } else if (chordLineType == u"plop") {
                chordline->setChordLineType(ChordLineType::PLOP);
            } else if (chordLineType == u"scoop") {
                chordline->setChordLineType(ChordLineType::SCOOP);
            }
            if (lineShape == u"straight") {
                chordline->setStraight(true);
            }
            if (lineType == u"wavy") {
                chordline->setWavy(true);
            } else if (!lineType.empty() && lineType != u"solid") {
                logger->logError(String(u"unsupported line-type: %1").arg(lineType), xmlreader);
            }
            chordline->setVisible(notation.visible());
            colorItem(chordline, Color::fromString(notation.attribute(u"color")));
            note->chord()->add(chordline);
        } else {
            logger->logError(String(u"no note for %1").arg(chordLineType), xmlreader);
        }
    }
}

//---------------------------------------------------------
//   notationWithAttributes
//---------------------------------------------------------

/**
 Helper function to create Notation with initial attributes.
 */

Notation Notation::notationWithAttributes(const String &name, const std::vector<XmlStreamReader::Attribute> &attributes,
                                          const String &parent, const SymId &symId) {
    Notation notation = Notation(name, parent, symId);
    for (const XmlStreamReader::Attribute &attr : attributes) {
        notation.addAttribute(String::fromAscii(attr.name.ascii()), attr.value);
    }
    return notation;
}

//---------------------------------------------------------
//   addAttribute
//---------------------------------------------------------

void Notation::addAttribute(const String &name, const String &value) { attributes_.emplace(name, value); }

//---------------------------------------------------------
//   attribute
//---------------------------------------------------------

String Notation::attribute(const String &name) const {
    const auto it = attributes_.find(name);
    return (it != attributes_.end()) ? it->second : String();
}

//---------------------------------------------------------
//   print
//---------------------------------------------------------

String Notation::print() const {
    String res = name_;

    for (auto const &pair : attributes_) {
        res += u" ";
        res += pair.first;
        res += u" ";
        res += pair.second;
    }

    if (!text_.empty()) {
        res += u" ";
        res += text_;
    }
    return res;
}

//---------------------------------------------------------
//   MusicXmlParserNotations
//---------------------------------------------------------

MusicXmlParserNotations::MusicXmlParserNotations(XmlStreamReader &e, Score *score, MusicXmlLogger *logger, MusicXmlParserPass2 &pass2)
    : e_(e), pass2_(pass2), score_(score), logger_(logger) {
    // nothing
}

//---------------------------------------------------------
//   addError
//---------------------------------------------------------

// notes:
// when the parser gets out of sync, only a single error stack is reported
// even when e.g. two incorrect notations are present
// line number will be added by pass 2

void MusicXmlParserNotations::addError(const String &error) {
    if (!error.empty()) {
        logger_->logError(error, &e_);
        errors_ += error;
    }
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserNotations::skipLogCurrElem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skipCurrentElement();
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

void MusicXmlParserNotations::parse() {
    visible_ = e_.asciiAttribute("print-object") != "no";

    while (e_.readNextStartElement()) {
        if (e_.name() == "arpeggiate") {
            arpeggio();
        } else if (e_.name() == "articulations") {
            articulations();
        } else if (e_.name() == "dynamics") {
            dynamics();
        } else if (e_.name() == "fermata") {
            fermata();
        } else if (e_.name() == "glissando") {
            glissandoSlide();
        } else if (e_.name() == "hammer-on") {
            slur();
        } else if (e_.name() == "non-arpeggiate") {
            arpeggioColor_ = Color::fromString(e_.attribute("color"));
            arpeggioType_ = u"non-arpeggiate";
            e_.skipCurrentElement(); // skip but don't log
        } else if (e_.name() == "ornaments") {
            ornaments();
        } else if (e_.name() == "pull-off") {
            slur();
        } else if (e_.name() == "slur") {
            slur();
        } else if (e_.name() == "slide") {
            glissandoSlide();
        } else if (e_.name() == "technical") {
            technical();
        } else if (e_.name() == "tied") {
            tied();
        } else if (e_.name() == "tuplet") {
            tuplet();
        } else if (e_.name() == "other-notation") {
            otherNotation();
        } else {
            skipLogCurrElem();
        }
    }

    /*
    for (const auto& notation : _notations) {
          LOGD("%s", muPrintable(notation.print()));
          }
     */

    addError(checkAtEndElement(e_, u"notations"));
}

//---------------------------------------------------------
//   addNotation
//---------------------------------------------------------

void MusicXmlParserNotations::addNotation(const Notation &notation, ChordRest *const cr, Note *const note) {
    if (notation.symId() != SymId::noSym) {
        if (notation.name() == u"breath") {
            addBreath(notation, cr);
        } else if (notation.name() == u"fermata") {
            addFermataToChord(notation, cr);

            // Terminate tempo line
            const InferredTempoLineStack &lines = pass2_.getInferredTempoLine();
            terminateInferredLine(std::vector<TextLineBase *>(lines.begin(), lines.end()), cr->tick(), cr->track(),
                                  pass2_.configuration()->inferTextType());
        } else if (notation.parent() == u"ornaments") {
            addTurnToChord(notation, cr);
        } else {
            addArticulationToChord(notation, cr);
        }
    } else if (notation.parent() == u"ornaments") {
        if (notation.name() == u"mordent" || notation.name() == u"inverted-mordent") {
            addMordentToChord(notation, cr);
        } else if (notation.name() == u"other-ornament") {
            addOtherOrnamentToChord(notation, cr);
        }
    } else if (notation.parent() == u"articulations") {
        if (note && notation.name() == u"chord-line") {
            addChordLine(notation, note, logger_, &e_);
        }
    } else {
        // LOGD("addNotation: notation has been skipped: %s %s", muPrintable(notation.name()), muPrintable(notation.parent()));
    }
}

//---------------------------------------------------------
//   addToScore
//---------------------------------------------------------

/**
 Add the notations found to the score.
 Note that some notations attach to notes only in MuseScore,
 which means trying to attach them to a rest will crash,
 as in that case note is a nullptr.
 */

void MusicXmlParserNotations::addToScore(ChordRest *const cr, Note *const note, const Fraction &tick, SlurStack &slurs,
                                         Glissando *glissandi[MAX_NUMBER_LEVEL][2], MusicXmlSpannerMap &spanners, TrillStack &trills,
                                         MusicXmlTieMap &ties, std::vector<Note *> &unstartedTieNotes, std::vector<Note *> &unendedTieNotes,
                                         ArpeggioMap &arpMap, DelayedArpMap &delayedArps) {
    addArpeggio(cr, arpeggioType_, arpeggioNo_, arpeggioColor_, arpMap, delayedArps);
    addWavyLine(cr, tick, wavyLineNo_, wavyLineType_, spanners, trills, logger_, &e_);

    for (const Notation &notation : notations_) {
        if (notation.symId() != SymId::noSym) {
            addNotation(notation, cr, note);
        } else if (notation.name() == "slur" || notation.name() == "hammer-on" || notation.name() == "pull-off") {
            addSlur(notation, slurs, cr, note, tick, logger_, &e_, pass2_.configuration()->importLayout());
        } else if (note && (notation.name() == "glissando" || notation.name() == "slide")) {
            addGlissandoSlide(notation, note, glissandi, spanners, logger_, &e_);
        } else if (note && notation.name() == "tied") {
            addTie(notation, note, cr->track(), ties, unstartedTieNotes, unendedTieNotes, logger_, &e_,
                   pass2_.configuration()->importLayout());
        } else if (note && notation.parent() == "technical") {
            addTechnical(notation, note);
        } else {
            addNotation(notation, cr, note);
        }
    }

    for (const MusicXmlDynamic &d : std::as_const(dynamicsList_)) {
        Dynamic *dynamic = createImportedDynamic(score_, d);
        pass2_.addElemOffset(dynamic, cr->track(), d.placement, cr->measure(), tick);
    }
}

//---------------------------------------------------------
//   stem
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/stem node.
 */

void MusicXmlParserPass2::stem(DirectionV &sd, bool &nost) {
    // defaults
    sd = DirectionV::AUTO;
    nost = false;

    String s = e_.readText();

    if (s == u"up") {
        sd = DirectionV::UP;
    } else if (s == u"down") {
        sd = DirectionV::DOWN;
    } else if (s == u"none") {
        nost = true;
    } else if (s == u"double") {
    } else {
        logger_->logError(String(u"unknown stem direction %1").arg(s), &e_);
    }
}

//---------------------------------------------------------
//   fermata
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/fermata node.
 Note: MusicXML common.mod: "An empty fermata element represents a normal fermata."
 */

void MusicXmlParserNotations::fermata() {
    Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"notations");
    const String fermataText = e_.readText();

    notation.setSymId(convertFermataToSymId(fermataText));
    notation.setText(fermataText);
    notation.setVisible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   tuplet
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/tuplet node.
 */

void MusicXmlParserNotations::tuplet() {
    const String tupletType = e_.attribute("type");
    const String tupletPlacement = e_.attribute("placement");
    const String tupletBracket = e_.attribute("bracket");
    const String tupletShowNumber = e_.attribute("show-number");

    // ignore possible children (currently not supported)
    e_.skipCurrentElement();

    if (tupletType == u"start") {
        tupletDesc_.type = MusicXmlStartStop::START;
    } else if (tupletType == u"stop") {
        tupletDesc_.type = MusicXmlStartStop::STOP;
    } else if (!tupletType.empty() && tupletType != u"start" && tupletType != u"stop") {
        logger_->logError(String(u"unknown tuplet type '%1'").arg(tupletType), &e_);
    }

    // set bracket, leave at default if unspecified
    if (tupletBracket == u"yes") {
        tupletDesc_.bracket = TupletBracketType::SHOW_BRACKET;
    } else if (tupletBracket == u"no") {
        tupletDesc_.bracket = TupletBracketType::SHOW_NO_BRACKET;
    }

    // set number, default is "actual" (=NumberType::SHOW_NUMBER)
    if (tupletShowNumber == u"both") {
        tupletDesc_.shownumber = TupletNumberType::SHOW_RELATION;
    } else if (tupletShowNumber == u"none") {
        tupletDesc_.shownumber = TupletNumberType::NO_TEXT;
    } else {
        tupletDesc_.shownumber = TupletNumberType::SHOW_NUMBER;
    }

    // set number and bracket placement
    if (tupletPlacement == u"above") {
        tupletDesc_.direction = DirectionV::UP;
    } else if (tupletPlacement == u"below") {
        tupletDesc_.direction = DirectionV::DOWN;
    } else if (tupletPlacement.empty()) {
        // ignore
    } else {
        logger_->logError(String(u"unknown tuplet placement: %1").arg(tupletPlacement), &e_);
    }
}

void MusicXmlParserNotations::otherNotation() {
    const String type = e_.attribute("type");
    const String smufl = e_.attribute("smufl");

    if (!smufl.empty()) {
        SymId id = SymNames::symIdByName(smufl, SymId::noSym);
        Notation notation = Notation::notationWithAttributes(String::fromAscii(e_.name().ascii()), e_.attributes(), u"notations", id);
        notation.setVisible(visible_);
        notations_.push_back(notation);
        e_.skipCurrentElement();
    }
}

//---------------------------------------------------------
//   MusicXmlParserDirection
//---------------------------------------------------------

/**
 MusicXmlParserDirection constructor.
 */

MusicXmlParserDirection::MusicXmlParserDirection(XmlStreamReader &e, Score *score, MusicXmlParserPass1 &pass1, MusicXmlParserPass2 &pass2,
                                                 MusicXmlLogger *logger)
    : e_(e), score_(score), pass1_(pass1), pass2_(pass2), logger_(logger), hasDefaultY_(false), defaultY_(0.0), hasRelativeY_(false),
      relativeY_(0.0), tpoMetro_(0), tpoSound_(0), offset_(0, 1) {
    // nothing
}

bool HarmonyDesc::fretDiagramVisible() const { return fretDiagram_ ? fretDiagram_->visible() : false; }

const ChordDescription *HarmonyDesc::descr() const {
    return harmony_ && !harmony_->chords().empty() ? harmony_->chords().front()->descr() : nullptr;
}
} // namespace mu::iex::musicxml
