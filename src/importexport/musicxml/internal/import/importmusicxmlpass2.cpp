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

#include "importmusicxmlpass2.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "../shared/musicxmlfonthandler.h"
#include "../shared/musicxmlsupport.h"
#include "../shared/musicxmltypes.h"
#include "containers.h"
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
#include "engraving/dom/hairpin.h"
#include "engraving/dom/hammeronpulloff.h"
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
#include "engraving/internal/engravingfont.h"
#include "engraving/internal/engravingfontsprovider.h"
#include "engraving/rendering/iscorerenderer.h"
#include "engraving/rendering/score/tlayout.h"
#include "engraving/types/symnames.h"
#include "engraving/types/typesconv.h"
#include "importexport/musicxml/internal/musicxmlconfiguration.h"
#include "importmusicxmllogger.h"
#include "importmusicxmlnoteduration.h"
#include "importmusicxmlnotepitch.h"
#include "importmusicxmlpass1.h"
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

constexpr int max_lyrics = 16;

//---------------------------------------------------------
//   function declarations
//---------------------------------------------------------

static void add_tie(const Notation &notation, Note *note, const track_idx_t track, MusicXmlTieMap &tie,
                    std::vector<Note *> &unstarted_tie_notes, std::vector<Note *> &unended_tie_notes, MusicXmlLogger *logger,
                    const XmlStreamReader *const xmlreader, bool import_layout);

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

void MusicXmlLyricsExtend::add_lyric(Lyrics *const lyric) { lyrics_.insert(lyric); }

//---------------------------------------------------------
//   lastChordTicks
//---------------------------------------------------------

// find the duration of the chord starting at or after s and ending at tick

static Fraction last_chord_ticks(const Segment *s, const Fraction &tick, const track_idx_t track) {
    while (s && s->tick() < tick) {
        for (EngravingItem *el : s->elist()) {
            if (el && el->is_chord_rest() && el->track() == track) {
                ChordRest *cr = to_chord_rest(el);
                if (cr->tick() + cr->actual_ticks() == tick) {
                    return cr->actual_ticks();
                }
            }
        }
        s = s->next_cr(muse::nidx, true);
    }
    return Fraction(0, 1);
}

static bool spanner_exists(const std::vector<MusicXmlSpannerDesc> &spanners, int number, engraving::ElementType element_type) {
    for (const MusicXmlSpannerDesc &desc : spanners) {
        if (desc.nr == number && desc.tp == element_type) {
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

void MusicXmlLyricsExtend::set_extend(const int verse, const track_idx_t track, const Fraction &tick,
                                      const Lyrics *prev_added_lyrics = nullptr) {
    std::vector<Lyrics *> list;
    for (Lyrics *l : lyrics_) {
        const ChordRest *par = l->chord_rest();
        if (par) {
            // no = -1: stop all extends on this track
            // otherwise, stop all extends in the stave with the same no and placement
            if ((verse == -1 && par->track() == track) || (l->verse() == verse && track2staff(par->track()) == track2staff(track) &&
                                                           prev_added_lyrics && prev_added_lyrics->placement() == l->placement())) {
                Fraction lct = last_chord_ticks(l->segment(), tick, track);
                if (lct > Fraction(0, 1)) {
                    // set lyric tick to the total length from the lyric note
                    // plus all notes covered by the melisma minus the last note length
                    l->set_ticks(tick - par->tick() - lct);
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

static int music_xml_step_alt_oct2_pitch(int step, int alter, int octave) {
    //                       c  d  e  f  g  a   b
    static int table[7] = {0, 2, 4, 5, 7, 9, 11};
    if (step < 0 || step > 6) {
        LOGD("MusicXmlStepAltOct2Pitch: illegal step %d", step);
        return -1;
    }
    int pitch = table[step] + alter + (octave + 1) * 12;

    if (!pitch_is_valid(pitch)) {
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

static void xml_set_pitch(Note *n, int step, int alter, double tuning, int octave, const int octave_shift, const Instrument *const instr) {
    //LOGD("xmlSetPitch(n=%p, step=%d, alter=%d, octave=%d, octaveShift=%d)",
    //       n, step, alter, octave, octaveShift);

    //const Staff* staff = n->score()->staff(track / VOICES);
    //const Instrument* instr = staff->part()->instr();

    const Interval intval = instr->transpose();

    //LOGD("  staff=%p instr=%p dia=%d chro=%d",
    //       staff, instr, static_cast<int>(intval.diatonic), static_cast<int>(intval.chromatic));

    int pitch = music_xml_step_alt_oct2_pitch(step, alter, octave);
    pitch += intval.chromatic;  // assume not in concert pitch
    pitch += 12 * octave_shift; // correct for octave shift
    // ensure sane values
    pitch = std::clamp(pitch, 0, 127);

    int tpc2 = step2tpc(step, AccidentalVal(alter));
    int tpc1 = Transpose::transpose_tpc(tpc2, intval, true);
    n->set_pitch(pitch, tpc1, tpc2);
    n->set_tuning(tuning);
    //LOGD("  pitch=%d tpc1=%d tpc2=%d", n->pitch(), n->tpc1(), n->tpc2());
}

//---------------------------------------------------------
//   fillGap
//---------------------------------------------------------

/**
 Fill one gap (tstart - tend) in this track in this measure with rest(s).
 */

static void fill_gap(Measure *measure, track_idx_t track, const Fraction &tstart, const Fraction &tend) {
    Fraction ctick = tstart;
    Fraction rest_len = tend - tstart;
    // LOGD("\nfillGIFV     fillGap(measure %p track %d tstart %d tend %d) restLen %d len",
    //        measure, track, tstart, tend, restLen);
    // note: as Constants::division (#ticks in a quarter note) equals 480
    // Constants::division / 64 (#ticks in a 256th note) equals 7.5 but is rounded down to 7
    while (rest_len > Fraction(1, 256)) {
        Fraction len = rest_len;
        TDuration d(DurationType::V_INVALID);
        if (measure->ticks() == rest_len) {
            d.set_type(DurationType::V_MEASURE);
        } else {
            d.set_val(len.ticks());
        }
        Segment *s = measure->get_segment(SegmentType::CHORD_REST, tstart);
        Rest *rest = Factory::create_rest(s, d);
        rest->set_ticks(len);
        rest->set_track(track);
        rest->set_visible(false);
        s->add(rest);
        len = rest->global_ticks();
        // LOGD(" %d", len);
        ctick += len;
        rest_len -= len;
    }
}

//---------------------------------------------------------
//   fillGapsInFirstVoices
//---------------------------------------------------------

/**
 Fill gaps in first voice of every staff in this measure for this part with rest(s).
 */

static void fill_gaps_in_first_voices(Measure *measure, Part *part) {
    IF_ASSERT_FAILED(measure) { return; }
    IF_ASSERT_FAILED(part) { return; }

    Fraction meas_tick = measure->tick();
    Fraction meas_len = measure->ticks();
    Fraction next_meas_tick = meas_tick + meas_len;
    staff_idx_t staff_idx = part->score()->staff_idx(part);
    /*
     LOGD("fillGIFV measure %p part %p idx %d nstaves %d tick %d - %d (len %d)",
     measure, part, staffIdx, part->nstaves(),
     measTick, nextMeasTick, measLen);
     */
    for (staff_idx_t st = 0; st < part->nstaves(); ++st) {
        track_idx_t track = (staff_idx + st) * ::mu::engraving::voices;
        Fraction end_of_last_cr = meas_tick;
        for (Segment *s = measure->first(); s; s = s->next()) {
            // LOGD("fillGIFV   segment %p tp %s", s, s->subTypeName());
            EngravingItem *el = s->element(track);
            if (el) {
                // LOGD(" el[%d] %p", track, el);
                if (s->is_chord_rest_type()) {
                    ChordRest *cr = to_chord_rest(el);
                    Fraction cr_tick = cr->tick();
                    Fraction cr_len = cr->global_ticks();
                    Fraction next_cr_tick = cr_tick + cr_len;
                    /*
                     LOGD(" chord/rest tick %d - %d (len %d)",
                     crTick, nextCrTick, crLen);
                     */
                    if (cr_tick > end_of_last_cr) {
                        /*
                         LOGD(" GAP: track %d tick %d - %d",
                         track, endOfLastCR, crTick);
                         */
                        fill_gap(measure, track, end_of_last_cr, cr_tick);
                    }
                    end_of_last_cr = next_cr_tick;
                }
            }
        }
        if (next_meas_tick > end_of_last_cr) {
            /*
             LOGD("fillGIFV   measure end GAP: track %d tick %d - %d",
             track, endOfLastCR, nextMeasTick);
             */
            fill_gap(measure, track, end_of_last_cr, next_meas_tick);
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

static bool has_drumset(const MusicXmlInstruments &instruments) {
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

static void init_drumset(Drumset *drumset, const MusicXmlInstruments &instruments) {
    drumset->clear();

    for (const auto &ii : instruments) {
        // debug: also dump the drumset for this part
        //LOGD("initDrumset: instrument: %s %s", muPrintable(ii.key()), muPrintable(ii.value().toString()));
        int unpitched = ii.second.unpitched;
        if (0 <= unpitched && unpitched <= 127) {
            drumset->drum(ii.second.unpitched) =
                DrumInstrument(ii.second.name, ii.second.notehead, ii.second.line, ii.second.stem_direction);
        }
    }
}

//---------------------------------------------------------
//   setStaffTypePercussion
//---------------------------------------------------------

/**
 Set staff type to percussion
 */

static void set_staff_type_percussion(Part *part, Drumset *drumset) {
    for (staff_idx_t j = 0; j < part->nstaves(); ++j) {
        if (part->staff(j)->lines(Fraction(0, 1)) == 5 && !part->staff(j)->is_drum_staff(Fraction(0, 1))) {
            part->staff(j)->set_staff_type(Fraction(0, 1), *StaffType::preset(StaffTypes::PERC_DEFAULT));
        }
    }
    // set drumset for instrument
    part->instrument()->set_drumset(drumset);
    part->instrument()->channel(0)->set_bank(128);
}

static std::pair<String, String> process_instr_name(const String &name) {
    // Make corrections so instruments match our names, then separate transpositions and remove unneeded info
    String n = name;
    n.replace(u"♭", u"b").replace(u"♯", u"#");
    n.replace(u"Sax", u"Saxophone");
    n.replace(u"Bari", u"Baritone");
    std::pair<String, String> ret;
    // Find transposition information
    static const std::regex transposition_regex("(^|(?:\\s|\\u00A0))([ABCDEF][b#]?)((?:\\s|\\u00A0)|$)");
    // Remove: ' in', part numbers '1, 2' which may be bracketed, any text after a slash
    static const std::regex remove_regex("(?:\\s|\u00A0)in|(\\(?[0-9]([,](?:\\s|\u00A0)?)?)\\)?|\\.|(\\/.*)");

    const StringList results = n.search(transposition_regex, {2});
    if (!results.empty()) {
        ret.second = convert_pitch_string_flats_and_sharps_to_unicode(results.front());
        n.remove(transposition_regex);
    }
    n.remove(remove_regex);
    ret.first = n.simplified();

    return ret;
}

//---------------------------------------------------------
//   createInstrument
//---------------------------------------------------------

/**
 Create an Instrument based on the information in \a mxmlInstr.
 */

static Instrument create_instrument(const MusicXmlInstrument &mxml_instr, const Interval interval) {
    // The interval may not match the instrument's default traitName.
    const auto &[track_name, trait_name] = process_instr_name(mxml_instr.name);
    Trait trait;
    trait.name = trait_name;

    // Initialize instrument with data from MusicXML.
    Instrument instr;
    InstrChannel *channel = instr.channel(0);
    instr.set_music_xml_id(mxml_instr.sound);
    instr.set_track_name(track_name);
    instr.set_long_name(track_name);
    instr.set_short_name(track_name);
    instr.set_trait(trait);
    instr.set_transpose(interval);
    channel->set_bank(0);
    channel->set_program(mxml_instr.midi_program);

    // Is it similar to one of our built-in instruments?
    if (const InstrumentTemplate *templ = combined_template_search(instr)) {
        instr = Instrument::from_template(templ); // Re-initialize with built-in data.
        channel = instr.channel(0);
    } else {
        // set articulations to default (global articulations)
        instr.set_articulation(midi_articulations);
        // set default program
        channel->set_program(mxml_instr.midi_program < 0 ? 0 : mxml_instr.midi_program);
    }

    // (Re-)apply values from MusicXML.
    instr.set_track_name(mxml_instr.name);
    instr.set_transpose(interval);
    channel->set_pan(mxml_instr.midi_pan);
    channel->set_volume(mxml_instr.midi_volume);

    return instr;
}

//---------------------------------------------------------
//   updatePartWithInstrument
//---------------------------------------------------------

static void update_part_with_instrument(Part *const part, const MusicXmlInstrument &mxml_instr, const Interval interval,
                                        const bool has_drumset = false) {
    Instrument instr = create_instrument(mxml_instr, interval);
    if (has_drumset) {
        instr.channel(0)->set_bank(128);
    }
    part->set_instrument(instr);
    if (mxml_instr.midi_channel >= 0) {
        part->set_midi_channel(mxml_instr.midi_channel, mxml_instr.midi_port);
    }
    // note: setMidiProgram() does more than simply setting the MIDI program
    if (mxml_instr.midi_program >= 0) {
        part->set_midi_program(mxml_instr.midi_program);
    }
}

//---------------------------------------------------------
//   createInstrumentChange
//---------------------------------------------------------

/**
 Create an InstrumentChange based on the information in \a mxmlInstr.
 */

static InstrumentChange *create_instrument_change(Score *score, const MusicXmlInstrument &mxml_instr, const Interval interval,
                                                  const track_idx_t track, const Instrument *cur_instr) {
    const Instrument instr = create_instrument(mxml_instr, interval);

    if (cur_instr->id() == instr.id() && instr.long_name() == cur_instr->long_name() && instr.short_name() == cur_instr->short_name()) {
        return nullptr;
    }

    InstrumentChange *instr_change = Factory::create_instrument_change(score->dummy(), instr);
    instr_change->set_track(track);

    // for text use instrument name (if known) else use "Instrument change"
    const String text = mxml_instr.name;
    instr_change->set_xml_text(text.empty() ? "Instrument change" : text.to_utf8().const_char());
    instr_change->set_visible(false);

    return instr_change;
}

//---------------------------------------------------------
//   updatePartWithInstrumentChange
//---------------------------------------------------------

static void update_part_with_instrument_change(Part *const part, const MusicXmlInstrument &mxml_instr, const Interval interval,
                                               Segment *const segment, const track_idx_t track, const Fraction tick) {
    const Instrument *cur_instr = part->instrument(tick);
    InstrumentChange *const ic = create_instrument_change(part->score(), mxml_instr, interval, track, cur_instr);

    if (!ic) {
        return;
    }

    segment->add(ic); // note: includes part::setInstrument(instr);

    // setMidiChannel() depends on setInstrument() already been done
    if (mxml_instr.midi_channel >= 0) {
        part->set_midi_channel(mxml_instr.midi_channel, mxml_instr.midi_port, tick);
    }
}

static void set_percussion_instrument(MusicXmlInstrument &mxml_instr, const String &part_name) {
    // If there are multiple unpitched instruments we should set the part's instrument to drumkit or percussion
    if (part_name.contains(u"drumset", CaseSensitivity::CASE_INSENSITIVE) ||
        part_name.contains(u"drumkit", CaseSensitivity::CASE_INSENSITIVE)) {
        mxml_instr = MusicXmlInstrument();
        mxml_instr.name = u"Drum Kit";
    } else if (part_name.contains(u"percussion", CaseSensitivity::CASE_INSENSITIVE)) {
        mxml_instr = MusicXmlInstrument();
        mxml_instr.name = u"Percussion";
        mxml_instr.sound = u"drum.group.set";
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

static void set_part_instruments(MusicXmlLogger *logger, const XmlStreamReader *xmlreader, Part *part, const String &part_id,
                                 const Score *score, const MusicXmlInstrList &instr_list, const MusicXmlIntervalList &interv_list,
                                 const MusicXmlInstruments &instruments, const String &part_name) {
    if (instruments.empty()) {
        // no instrument details found, create a default instrument
        //LOGD("no instrument details");
        update_part_with_instrument(part, {}, interv_list.interval({0, 1}));
        return;
    }

    if (has_drumset(instruments)) {
        // do not create multiple instruments for a drum part
        //LOGD("hasDrumset");
        MusicXmlInstrument mxml_instr = instruments.begin()->second;
        if (instruments.size() > 1) {
            set_percussion_instrument(mxml_instr, part_name);
        }
        update_part_with_instrument(part, mxml_instr, {}, true);
        return;
    }

    if (instr_list.empty()) {
        // instrument details found, but no instrument ids found
        // -> only a single instrument is playing in the part
        //LOGD("single instrument");
        MusicXmlInstrument mxml_instr = instruments.begin()->second;
        update_part_with_instrument(part, mxml_instr, interv_list.interval({0, 1}));
        return;
    }

    // either a single instrument is playing, or forwards / rests resulted in gaps in the instrument map
    // (and thus multiple entries)
    //LOGD("possibly multiple instruments");
    String prev_instr_id;
    for (auto it = instr_list.cbegin(); it != instr_list.cend(); ++it) {
        Fraction tick = (*it).first;
        if (it == instr_list.cbegin()) {
            prev_instr_id = (*it).second; // first instrument id
            MusicXmlInstrument mxml_instr = muse::value(instruments, prev_instr_id);
            update_part_with_instrument(part, mxml_instr, interv_list.interval(tick));
        } else {
            String instr_id = (*it).second;
            bool must_insert = instr_id != prev_instr_id;
            /*
             LOGD("tick %s previd %s id %s mustInsert %d",
             muPrintable(tick.print()),
             muPrintable(prevInstrId),
             muPrintable(instrId),
             mustInsert);
             */
            if (must_insert) {
                const staff_idx_t staff = score->staff_idx(part);
                const track_idx_t track = staff * ::mu::engraving::voices;
                //LOGD("instrument change: tick %s (%d) track %d instr '%s'",
                //       muPrintable(tick.print()), tick.ticks(), track, muPrintable(instrId));

                Measure *const m = score->tick2measure(tick);
                Segment *const segment = m->get_segment(SegmentType::CHORD_REST, tick);

                if (!segment) {
                    logger->log_error(String(u"segment for instrument change at tick %1 not found").arg(tick.ticks()), xmlreader);
                } else if (!muse::contains(instruments, instr_id)) {
                    logger->log_error(
                        String(u"changed instrument '%1' at tick %2 not found in part '%3'").arg(instr_id).arg(tick.ticks()).arg(part_id),
                        xmlreader);
                } else {
                    MusicXmlInstrument mxml_instr = muse::value(instruments, instr_id);
                    update_part_with_instrument_change(part, mxml_instr, interv_list.interval(tick), segment, track, tick);
                }
            }
            prev_instr_id = instr_id;
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
static String text2syms(const String &t, const EngravingFontsProvider &fonts, bool is_dolet = false) {
    //QTime time;
    //time.start();

    // first create a map from symbol (Unicode) text to symId
    // note that this takes about 1 msec on a Core i5,
    // caching does not gain much

    EngravingFontPtr sf = fonts.fallback_font();
    std::map<String, SymId> map;
    size_t max_string_size = 0; // maximum string size found

    for (int i = int(SymId::NO_SYM); i < int(SymId::LAST_SYM); ++i) {
        SymId id((SymId(i)));
        String string = sf->to_string(id);
        // insert all syms except space to prevent matching all regular spaces
        if (id != SymId::SPACE) {
            map.insert({string, id});
        }
        if (string.size() > max_string_size) {
            max_string_size = string.size();
        }
    }

    if (is_dolet) {
        map.insert({u"$", SymId::SEGNO});
        map.insert({u"Ø", SymId::CODA});
    }

    //LOGD("text2syms map count %d maxsz %d filling time elapsed: %d ms",
    //       map.size(), maxStringSize, time.elapsed());

    // then look for matches
    String in = t;
    String res;

    while (!in.empty()) {
        // try to find the largest match possible
        int max_match = int(std::min(in.size(), max_string_size));
        AsciiStringView sym;
        while (max_match > 0) {
            String to_be_matched = in.left(max_match);
            if (muse::contains(map, to_be_matched)) {
                sym = SymNames::name_for_sym_id(map.at(to_be_matched));
                break;
            }
            max_match--;
        }
        if (max_match > 0) {
            // found a match, add sym to res and remove match from string in
            res += u"<sym>";
            res += String::from_ascii(sym.ascii());
            res += u"</sym>";
            in.remove(0, max_match);
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
static String next_part_of_formatted_string(XmlStreamReader &e, const MusicXmlConfiguration &configuration,
                                            const EngravingFontsProvider &fonts, bool is_dolet = false) {
    //String lang       = e.attribute(String("xml:lang"), "it");
    String font_weight = e.attribute("font-weight");
    String font_size = e.attribute("font-size");
    String font_style = e.attribute("font-style");
    String underline = e.attribute("underline");
    String strike = e.attribute("line-through");
    String font_family = e.attribute("font-family");
    // TODO: color, enclosure, yoffset in only part of the text, ...

    String txt = e.read_text();
    // replace HTML entities
    txt = String::decode_xml_entities(txt);
    String syms = xmlpass2::text2syms(txt, fonts, is_dolet);

    String importedtext;

    if (!font_size.empty()) {
        bool ok = true;
        float size = font_size.to_float(&ok);
        if (ok) {
            importedtext += String(u"<font size=\"%1\"/>").arg(size);
        }
    }

    bool need_use_default_font = configuration.need_use_default_font();
    if (!font_family.empty() && txt == syms && !need_use_default_font) {
        // add font family only if no <sym> replacement made
        importedtext += String(u"<font face=\"%1\"/>").arg(font_family);
    }
    if (font_weight == u"bold") {
        importedtext += u"<b>";
    }
    if (font_style == u"italic") {
        importedtext += u"<i>";
    }
    if (!underline.empty()) {
        bool ok = true;
        int lines = underline.to_int(&ok);
        if (ok && (lines > 0)) { // 1, 2, or 3 underlines are imported as single underline
            importedtext += u"<u>";
        } else {
            underline.clear();
        }
    }
    if (!strike.empty()) {
        bool ok = true;
        int lines = strike.to_int(&ok);
        if (ok && (lines > 0)) { // 1, 2, or 3 strikes are imported as single strike
            importedtext += u"<s>";
        } else {
            strike.clear();
        }
    }
    if (txt == syms) {
        txt.replace(String(u"\r"), String()); // convert Windows line break \r\n -> \n
        importedtext += txt.to_xml_escaped();
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
    if (font_style == u"italic") {
        importedtext += u"</i>";
    }
    if (font_weight == u"bold") {
        importedtext += u"</b>";
    }
    //LOGD("importedtext '%s'", muPrintable(importedtext));
    return importedtext;
}
} // namespace xmlpass2

//---------------------------------------------------------
//   colorItem
//---------------------------------------------------------

static void color_item(EngravingItem *item, const Color color) {
    if (color.is_valid()) {
        item->set_property(Pid::COLOR, color);
        item->set_property_flags(Pid::COLOR, PropertyFlags::UNSTYLED);
    }
}

// Convert a MusicXML percentage without a locale change or an unsafe cast.
static int music_xml_dynamic_velocity(const String &value, int minimum, int fallback) {
    const std::string text = value.to_std_string();
    const auto is_xml_space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    const char *first = text.data();
    const char *last = first + text.size();
    while (first != last && is_xml_space(*first)) ++first;
    while (first != last && is_xml_space(*(last - 1))) --last;
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
        int64_t integer_digits = 0;
        int64_t digit_index = 0;
        int64_t first_nonzero = -1;
        bool after_point = false;
        const char *p = first;
        for (; p != last && *p != 'e' && *p != 'E'; ++p) {
            if (*p == '.') {
                after_point = true;
            } else {
                if (!after_point) ++integer_digits;
                if (*p != '0' && first_nonzero < 0) first_nonzero = digit_index;
                ++digit_index;
            }
        }
        if (first_nonzero < 0) return minimum;
        constexpr int64_t limit = INT64_MAX / 4;
        int64_t exponent = 0;
        bool exponent_negative = false;
        if (p != last) {
            ++p;
            if (p != last && (*p == '+' || *p == '-')) exponent_negative = *p++ == '-';
            for (; p != last; ++p) {
                const int digit = *p - '0';
                if (exponent > (limit - digit) / 10)
                    exponent = limit;
                else
                    exponent = exponent * 10 + digit;
            }
        }
        if (exponent_negative) exponent = -exponent;
        const int64_t order = integer_digits - first_nonzero - 1 + exponent;
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
static String read_dynamics_text(XmlStreamReader &e) {
    String text;
    while (!e.at_end()) {
        e.read_next();
        if (e.is_end_element()) {
            break;
        }
        if (e.is_characters()) {
            text += e.text();
        } else if (e.is_start_element()) {
            e.skip_current_element();
        }
    }
    return text;
}

static String dynamic_letters_to_xml(const String &text) {
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
static void append_dynamic_literal(MusicXmlDynamic &result, const String &value) {
    std::vector<std::pair<size_t, size_t>> spans;
    std::vector<std::pair<String, size_t>> open_tags;
    size_t scan = 0;
    while (scan < value.size()) {
        const size_t begin = value.index_of(u'<', scan);
        const size_t end = begin == muse::nidx ? muse::nidx : value.index_of(u'>', begin + 1);
        if (end == muse::nidx) {
            break;
        }
        spans.emplace_back(begin, end + 1);
        size_t name_begin = begin + 1;
        const bool closing = name_begin < end && value.at(name_begin).unicode() == u'/';
        if (closing) {
            ++name_begin;
        }
        size_t name_end = name_begin;
        while (name_end < end) {
            const char16_t c = value.at(name_end).unicode();
            if (!((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9') || c == u'_' || c == u'-' ||
                  c == u':')) {
                break;
            }
            ++name_end;
        }
        if (name_end > name_begin) {
            const String name = value.mid(name_begin, name_end - name_begin);
            if (closing && !open_tags.empty() && open_tags.back().first == name) {
                spans.emplace_back(open_tags.back().second, end + 1);
                open_tags.pop_back();
            } else if (!closing && value.at(end - 1).unicode() != u'/') {
                open_tags.emplace_back(name, begin);
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
        result.text += TextBase::plain_to_xml_text(value.mid(cursor, begin - cursor));
        const String literal = TextBase::plain_to_xml_text(value.mid(begin, end - begin));
        result.protected_literal_ranges.emplace_back(result.text.size(), literal.size());
        result.text += literal;
        cursor = end;
    }
    result.text += TextBase::plain_to_xml_text(value.mid(cursor));
}

// Keep literal text distinct from markup generated from the symbol table.
static MusicXmlDynamic parse_dynamics_container(XmlStreamReader &e, MusicXmlLogger *logger) {
    MusicXmlDynamic result;
    result.color = Color::from_string(e.attribute("color"));
    result.placement = e.attribute("placement");
    result.enclosure = e.attribute("enclosure");
    while (e.read_next_start_element()) {
        if (e.name() == "other-dynamics") {
            const String smufl = e.attribute("smufl");
            const String value = read_dynamics_text(e);
            const SymId id = SymNames::sym_id_by_name(smufl);
            // symIdByName(String) converts to ASCII and then uses a C string.
            // Require an exact round trip to reject lossy/truncated names too.
            const String canonical = id == SymId::NO_SYM ? String() : String::from_ascii(SymNames::name_for_sym_id(id).ascii());
            // The generated standard block ends before engine/font extensions.
            if (!smufl.empty() && id != SymId::NO_SYM && id < SymId::NOTEHEAD_DOUBLE_WHOLE_ALT && canonical == smufl) {
                result.text += u"<sym>" + canonical + u"</sym>";
            } else {
                if (!smufl.empty() && logger) {
                    logger->log_error(String(u"Unsupported dynamics SMuFL name '%1'; using literal text").arg(smufl), &e);
                }
                const String symbols = dynamic_letters_to_xml(value);
                if (smufl.empty() && !symbols.empty()) {
                    result.text += symbols;
                } else {
                    append_dynamic_literal(result, value);
                }
            }
        } else {
            const String name = String::from_ascii(e.name().ascii());
            const String symbols = dynamic_letters_to_xml(name);
            // Upstream retains unrecognized tag names as literal dynamics text.
            result.text += symbols.empty() ? TextBase::plain_to_xml_text(name) : symbols;
            e.skip_current_element();
        }
    }
    return result;
}

// Pick one marker prefix absent from the input with a single scan, so many
// protected spans do not cause repeated whole-string searches.
static String dynamic_literal_marker_prefix(const String &text) {
    const std::string input = text.to_std_string();
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

static Dynamic *create_imported_dynamic(Score *score, const MusicXmlDynamic &entry) {
    Dynamic *dynamic = Factory::create_dynamic(score->dummy());
    // The dynamic classifier also recognizes plain words. Hide markup-looking
    // literals from it temporarily, while still classifying genuine symbols and
    // ordinary prose in the rest of the container.
    String classification_text;
    std::vector<std::pair<String, String>> protected_literals;
    const String marker_prefix = entry.protected_literal_ranges.empty() ? String() : dynamic_literal_marker_prefix(entry.text);
    size_t cursor = 0;
    for (size_t i = 0; i < entry.protected_literal_ranges.size(); ++i) {
        const auto [start, length] = entry.protected_literal_ranges[i];
        classification_text += entry.text.mid(cursor, start - cursor);
        const String marker = marker_prefix + String::number(i) + u"~";
        classification_text += marker;
        protected_literals.emplace_back(marker, entry.text.mid(start, length));
        cursor = start + length;
    }
    classification_text += entry.text.mid(cursor);
    dynamic->set_dynamic_type(classification_text);
    if (!protected_literals.empty()) {
        const String text = dynamic->xml_text();
        String restored;
        restored.reserve(entry.text.size());
        size_t position = 0;
        bool valid = true;
        for (const auto &[marker, literal] : protected_literals) {
            const size_t next = text.index_of(marker, position);
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
        dynamic->set_xml_text(valid ? restored : entry.text);
    }
    color_item(dynamic, entry.color);
    if (entry.enclosure == u"circle") {
        dynamic->set_frame_type(FrameType::CIRCLE);
        dynamic->set_property_flags(Pid::FRAME_TYPE, PropertyFlags::UNSTYLED);
    } else if (entry.enclosure == u"none") {
        dynamic->set_frame_type(FrameType::NO_FRAME);
        dynamic->set_property_flags(Pid::FRAME_TYPE, PropertyFlags::UNSTYLED);
    } else if (entry.enclosure == u"rectangle") {
        dynamic->set_frame_type(FrameType::RECTANGLE);
        dynamic->set_frame_round(0_sp);
        dynamic->set_property_flags(Pid::FRAME_TYPE, PropertyFlags::UNSTYLED);
        dynamic->set_property_flags(Pid::FRAME_ROUND, PropertyFlags::UNSTYLED);
    }
    return dynamic;
}

//---------------------------------------------------------
//   addLyric
//---------------------------------------------------------

/**
 Add a single lyric to the score or delete it (if number too high)
 */

static void add_lyric(MusicXmlLogger *logger, const XmlStreamReader *const xmlreader, ChordRest *cr, Lyrics *l, int lyric_no,
                      MusicXmlLyricsExtend &extended_lyrics) {
    if (lyric_no > ::mu::iex::musicxml::max_lyrics) {
        logger->log_error(String(u"too much lyrics (>%1)").arg(::mu::iex::musicxml::max_lyrics), xmlreader);
        delete l;
    } else {
        l->set_verse(lyric_no);
        l->init_text_style_type(l->is_even() ? TextStyleType::LYRICS_EVEN : TextStyleType::LYRICS_ODD, /*preserveDifferent*/ true);
        cr->add(l);
        extended_lyrics.set_extend(lyric_no, cr->track(), cr->tick(), l);
    }
}

//---------------------------------------------------------
//   addLyrics
//---------------------------------------------------------

/**
 Add a notes lyrics to the score
 */

static void add_lyrics(MusicXmlLogger *logger, const XmlStreamReader *const xmlreader, ChordRest *cr,
                       const std::map<int, Lyrics *> &numbrd_lyrics, const std::set<Lyrics *> &ext_lyrics,
                       MusicXmlLyricsExtend &extended_lyrics) {
    for (const auto &[lyric_no, lyric] : numbrd_lyrics) {
        add_lyric(logger, xmlreader, cr, lyric, lyric_no, extended_lyrics);
        if (muse::contains(ext_lyrics, lyric)) {
            extended_lyrics.add_lyric(lyric);
        }
    }
}

static void add_grace_note_lyrics(const std::map<int, Lyrics *> &numbered_lyrics, std::set<Lyrics *> extended_lyrics,
                                  std::vector<GraceNoteLyrics> &gn_lyrics) {
    for (const auto &[lyric_no, lyric] : numbered_lyrics) {
        if (lyric) {
            bool extend = muse::contains(extended_lyrics, lyric);
            const GraceNoteLyrics gnl = GraceNoteLyrics(lyric, extend, lyric_no);
            gn_lyrics.push_back(gnl);
        }
    }
}

static void add_inferred_stickings(ChordRest *cr, const std::vector<Sticking *> &numbered_stickings) {
    for (Sticking *sticking : numbered_stickings) {
        sticking->set_ownership_parent(cr->segment());
        sticking->set_track(cr->track());
        cr->score()->add_element(sticking);
    }
}

//---------------------------------------------------------
//   addElemOffset
//---------------------------------------------------------

void MusicXmlParserPass2::add_elem_offset(engraving::EngravingItem *el, engraving::track_idx_t track, const muse::String &placement,
                                          engraving::Measure *measure, const engraving::Fraction &tick) {
    if (!measure) {
        return;
    }
    Fraction el_tick = tick;

    if (!placement.empty()) {
        if (el->has_voice_assignment_properties()) {
            el->set_property(Pid::DIRECTION, placement == u"above" ? DirectionV::UP : DirectionV::DOWN);
            el->set_property_flags(Pid::DIRECTION, PropertyFlags::UNSTYLED);
        } else {
            el->set_placement(placement == u"above" ? PlacementV::ABOVE : PlacementV::BELOW);
            el->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
            if (!el->is_sticking()) {
                el->reset_property(Pid::OFFSET);
            }
        }
    }
    const Fraction &end_tick = measure->score()->end_tick();
    if (el_tick > end_tick) {
        el_tick = end_tick;
    }

    el->set_track(el->is_tempo_text() ? 0 : track); // TempoText must be in track 0
    Segment *s = measure->get_segment(SegmentType::CHORD_REST, el_tick);

    if (el->is_sticking()) {
        if (el->property_flags(Pid::OFFSET) == PropertyFlags::UNSTYLED) {
            const EngravingItem *item = s->element(el->track());
            const Chord *chord = item && item->is_chord() ? to_chord(item) : nullptr;
            const bool has_grace_notes = chord && !chord->grace_notes().empty();

            if (!has_grace_notes) {
                el->reset_property(Pid::OFFSET);
            }
        }
    }

    if (el->system_flag()) {
        const bool finale = pass1_.exporter_software() == MusicXmlExporterSoftware::FINALE;
        Score *score = measure->score();
        Staff *st = score->staff(track2staff(track));

        if (!st->is_system_object_staff() && st->idx() != 0) {
            if (finale) {
                delete el;
                return;
            }
            score->add_system_object_staff(st);
        }

        bool found = false;
        for (EngravingItem *existing_el : muse::values(system_elements(), el_tick.ticks())) {
            if (el->type() == existing_el->type()) {
                if (el->is_text_base()) {
                    TextBase *el_text = to_text_base(el);
                    TextBase *existing_text = to_text_base(existing_el);
                    if (el_text->xml_text() == existing_text->xml_text()) {
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
            if (el->is_marker() || el->is_jump()) {
                el->set_ownership_parent(measure);
            } else {
                el->set_ownership_parent(s);
            }
            add_system_element(el, el_tick);
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

static Fraction calculate_tuplet_duration(const Tuplet *const t) {
    Fraction res;

    for (DurationElement *de : t->elements()) {
        if (de->is_chord_rest()) {
            const ChordRest *cr = to_chord_rest(de);
            const Fraction fraction = cr->ticks(); // TODO : take care of nested tuplets
            if (fraction.is_valid()) {
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

static TDuration determine_tuplet_base_len(const Tuplet *const t) {
    Fraction tuplet_fraction;
    Fraction tuplet_full_duration;
    MusicXmlTupletState::determine_tuplet_fraction_and_full_duration(calculate_tuplet_duration(t), tuplet_fraction, tuplet_full_duration);

    Fraction base_len = tuplet_full_duration * Fraction(1, t->ratio().denominator());
    /*
    LOGD("tupletFraction %s tupletFullDuration %s ratio %s baseLen %s",
           muPrintable(tupletFraction.toString()),
           muPrintable(tupletFullDuration.toString()),
           muPrintable(t->ratio().toString()),
           muPrintable(baseLen.toString())
           );
     */

    return TDuration(base_len);
}

//---------------------------------------------------------
//   handleTupletStart
//---------------------------------------------------------

static void handle_tuplet_start(const ChordRest *const cr, Tuplet *&tuplet, const int actual_notes, const int normal_notes,
                                const MusicXmlTupletDesc &tuplet_desc) {
    tuplet = new Tuplet(cr->measure());
    tuplet->set_track(cr->track());
    tuplet->set_ratio(Fraction(actual_notes, normal_notes));
    tuplet->set_tick(cr->tick());
    tuplet->set_bracket_type(tuplet_desc.bracket);
    tuplet->set_number_type(tuplet_desc.shownumber);
    tuplet->set_direction(tuplet_desc.direction);
}

//---------------------------------------------------------
//   handleTupletStop
//---------------------------------------------------------

static void handle_tuplet_stop(Tuplet *&tuplet, const int normal_notes) {
    // allow handleTupletStop to be called w/o error of no tuplet active
    if (!tuplet) {
        return;
    }

    // set baselen
    TDuration td = determine_tuplet_base_len(tuplet);
    tuplet->set_base_len(td);
    Fraction f(normal_notes, td.fraction().denominator());
    f.reduce();
    if (!f.is_valid()) {
        LOGD("MusicXml::import: tuplet stop but note values too small");
        tuplet = nullptr;
        return;
    }
    tuplet->set_ticks(f);
    // TODO determine usefulness of following check
    int total_duration = 0;
    int ticks_per_note = f.ticks() / tuplet->ratio().numerator();
    bool ticks_correct = true;
    for (DurationElement *de : tuplet->elements()) {
        if (de->is_chord_rest()) {
            int global_ticks = de->global_ticks().ticks();
            if (global_ticks != ticks_per_note) {
                ticks_correct = false;
            }
            total_duration += global_ticks;
        }
    }
    if (total_duration != f.ticks()) {
        LOGD("MusicXml::import: tuplet stop but bad duration"); // TODO
    }
    if (!ticks_correct) {
        LOGD("MusicXml::import: tuplet stop but uneven note ticks"); // TODO
    }
    tuplet = 0;
}

//---------------------------------------------------------
//   setElementPropertyFlags
//---------------------------------------------------------

static void set_element_property_flags(EngravingObject *element, const Pid property_id, const String value1,
                                       const String value2 = String()) {
    if (value1.empty()) { // Set as an implicit value
        element->set_property_flags(property_id, PropertyFlags::STYLED);
    } else if (!value1.empty() || !value2.empty()) { // Set as an explicit value
        element->set_property_flags(property_id, PropertyFlags::UNSTYLED);
    }
}

//---------------------------------------------------------
//   addArticulationToChord
//---------------------------------------------------------

static void add_articulation_to_chord(const Notation &notation, ChordRest *cr) {
    const SymId artic_sym = notation.sym_id();
    const String dir = notation.attribute(u"type");
    const String place = notation.attribute(u"placement");
    Articulation *na = Factory::create_articulation(cr);
    if (artic_sym != SymId::NO_SYM) {
        na->set_sym_id(artic_sym);
    }
    if (!notation.text().empty()) {
        na->set_text_type(TConv::from_xml(notation.text().to_ascii().const_char(), ArticulationTextType::NO_TEXT));
    }
    na->set_visible(notation.visible());
    color_item(na, Color::from_string(notation.attribute(u"color")));

    if (dir == "up" || dir == "down") {
        na->set_up(dir == "up");
        na->set_property_flags(Pid::DIRECTION, PropertyFlags::UNSTYLED);
    }

    // when setting anchor, assume type up/down without explicit placement
    // implies placement above/below
    if (place == "above" || (dir == "up" && place.empty())) {
        na->set_anchor(ArticulationAnchor::TOP);
        na->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    } else if (place == "below" || (dir == "down" && place.empty())) {
        na->set_anchor(ArticulationAnchor::BOTTOM);
        na->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    }

    if (cr->is_chord() && to_chord(cr)->has_articulation(na)) {
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

static void add_fermata_to_chord(const Notation &notation, ChordRest *cr) {
    const SymId artic_sym = notation.sym_id();
    const String direction = notation.attribute(u"type");
    Segment *seg = cr->segment();
    Fermata *fermata = Factory::create_fermata(parent_or_dummy(seg, cr->score()->dummy()));
    fermata->set_sym_id_and_time_stretch(artic_sym);
    fermata->set_track(cr->track());
    fermata->set_visible(notation.visible());
    color_item(fermata, Color::from_string(notation.attribute(u"color")));
    if (!direction.empty()) {
        fermata->set_placement(direction == "inverted" ? PlacementV::BELOW : PlacementV::ABOVE);
        fermata->reset_property(Pid::OFFSET);
    } else {
        fermata->set_placement(fermata->property_default(Pid::PLACEMENT).value<PlacementV>());
    }
    set_element_property_flags(fermata, Pid::PLACEMENT, direction);
    if (!seg) {
        assert(cr->is_grace());
        cr->add_fermata(fermata); // store for later move to segment
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

static void add_tap_to_chord(const Notation &notation, ChordRest *cr) {
    const String place = notation.attribute(u"placement");
    const TappingHand hand = notation.attribute(u"hand") == u"right" ? TappingHand::RIGHT : TappingHand::LEFT;

    Tapping *tap = Factory::create_tapping(cr);
    tap->set_track(cr->track());
    tap->set_hand(hand);
    if (place == u"above") {
        tap->set_anchor(ArticulationAnchor::TOP);
        tap->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    } else if (place == u"below") {
        tap->set_anchor(ArticulationAnchor::BOTTOM);
        tap->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
    } else {
        tap->set_anchor(ArticulationAnchor::AUTO);
    }
    color_item(tap, Color::from_string(notation.attribute(u"color")));
    cr->add(tap);
}

//---------------------------------------------------------
//   addMordentToChord
//---------------------------------------------------------

/**
 Add Mordent to Chord.
 */

static void add_mordent_to_chord(const Notation &notation, ChordRest *cr) {
    const String name = notation.name();
    const String attr_long = notation.attribute(u"long");
    const String attr_appr = notation.attribute(u"approach");
    const String attr_dep = notation.attribute(u"departure");
    SymId artic_sym = SymId::NO_SYM; // legal but impossible ArticulationType value here indicating "not found"
    if (name == "inverted-mordent") {
        if ((attr_long.empty() || attr_long == "no") && attr_appr.empty() && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_SHORT_TRILL;
        } else if (attr_long == "yes" && attr_appr.empty() && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_TREMBLEMENT;
        } else if (attr_long == "yes" && attr_appr == "below" && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_UP_PRALL;
        } else if (attr_long == "yes" && attr_appr == "above" && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_PRECOMP_MORDENT_UPPER_PREFIX;
        } else if (attr_long == "yes" && attr_appr.empty() && attr_dep == "below") {
            artic_sym = SymId::ORNAMENT_PRALL_DOWN;
        } else if (attr_long == "yes" && attr_appr.empty() && attr_dep == "above") {
            artic_sym = SymId::ORNAMENT_PRALL_UP;
        }
    } else if (name == "mordent") {
        if ((attr_long.empty() || attr_long == "no") && attr_appr.empty() && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_MORDENT;
        } else if (attr_long == "yes" && attr_appr.empty() && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_PRALL_MORDENT;
        } else if (attr_long == "yes" && attr_appr == "below" && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_UP_MORDENT;
        } else if (attr_long == "yes" && attr_appr == "above" && attr_dep.empty()) {
            artic_sym = SymId::ORNAMENT_DOWN_MORDENT;
        }
    }
    if (artic_sym != SymId::NO_SYM) {
        const String place = notation.attribute(u"placement");
        Ornament *mordent = Factory::create_ornament(cr);
        mordent->set_sym_id(artic_sym);
        if (place == u"above") {
            mordent->set_anchor(ArticulationAnchor::TOP);
        } else if (place == u"below") {
            mordent->set_anchor(ArticulationAnchor::BOTTOM);
        } else {
            mordent->set_anchor(ArticulationAnchor::AUTO);
        }
        mordent->set_visible(notation.visible());
        color_item(mordent, Color::from_string(notation.attribute(u"color")));
        cr->add(mordent);
    } else {
        LOGD("unknown ornament: name '%s' long '%s' approach '%s' departure '%s'", muPrintable(name), muPrintable(attr_long),
             muPrintable(attr_appr), muPrintable(attr_dep)); // TODO
    }
}

//---------------------------------------------------------
//   addTurnToChord
//---------------------------------------------------------

/**
 Add Turn to Chord.
 */

static void add_turn_to_chord(const Notation &notation, ChordRest *cr) {
    SymId turn_sym = notation.sym_id();
    const String place = notation.attribute(u"placement");
    if (notation.attribute(u"slash") == "yes") {
        // TODO: currently this is the only available SMuFL turn with a slash
        turn_sym = SymId::ORNAMENT_TURN_SLASH;
    }
    Ornament *turn = Factory::create_ornament(cr);
    turn->set_sym_id(turn_sym);
    if (place == u"above") {
        turn->set_anchor(ArticulationAnchor::TOP);
    } else if (place == u"below") {
        turn->set_anchor(ArticulationAnchor::BOTTOM);
    } else {
        turn->set_anchor(ArticulationAnchor::AUTO);
    }
    turn->set_visible(notation.visible());
    color_item(turn, Color::from_string(notation.attribute(u"color")));
    cr->add(turn);
}

//---------------------------------------------------------
//   addOtherOrnamentToChord
//---------------------------------------------------------

/**
 Add Other Ornament to Chord.
 */

static void add_other_ornament_to_chord(const Notation &notation, ChordRest *cr) {
    const String name = notation.name();
    const String symname = notation.attribute(u"smufl");
    SymId sym = SymId::NO_SYM; // legal but impossible ArticulationType value here indicating "not found"
    sym = SymNames::sym_id_by_name(symname);

    if (sym != SymId::NO_SYM) {
        Ornament *ornam = Factory::create_ornament(cr);
        ornam->set_sym_id(sym);
        ornam->set_visible(notation.visible());
        color_item(ornam, Color::from_string(notation.attribute(u"color")));
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

static bool convert_articulation_to_sym_id(const String &mxml_name, SymId &id) {
    // map MusicXML notations name to MuseScore symbol
    static const std::map<String, SymId> map{// ornaments
                                             {u"delayed-turn", SymId::ORNAMENT_TURN},
                                             {u"inverted-turn", SymId::ORNAMENT_TURN_INVERTED},
                                             {u"vertical-turn", SymId::ORNAMENT_TURN_UP},
                                             {u"inverted-vertical-turn", SymId::ORNAMENT_TURN_UP_S},
                                             {u"turn", SymId::ORNAMENT_TURN},
                                             {u"shake", SymId::ORNAMENT_TREMBLEMENT_COUPERIN},
                                             {u"schleifer", SymId::ORNAMENT_PRECOMP_SLIDE},
                                             {u"haydn", SymId::ORNAMENT_HAYDN},
                                             // articulations
                                             {u"accent", SymId::ARTIC_ACCENT_ABOVE},
                                             {u"strong-accent", SymId::ARTIC_MARCATO_ABOVE},
                                             {u"staccato", SymId::ARTIC_STACCATO_ABOVE},
                                             {u"tenuto", SymId::ARTIC_TENUTO_ABOVE},
                                             {u"detached-legato", SymId::ARTIC_TENUTO_STACCATO_ABOVE},
                                             {u"staccatissimo", SymId::ARTIC_STACCATISSIMO_ABOVE},
                                             {u"spiccato", SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE},
                                             {u"stress", SymId::ARTIC_STRESS_ABOVE},
                                             {u"unstress", SymId::ARTIC_UNSTRESS_ABOVE},
                                             {u"soft-accent", SymId::ARTIC_SOFT_ACCENT_ABOVE},
                                             // technical
                                             {u"up-bow", SymId::STRINGS_UP_BOW},
                                             {u"down-bow", SymId::STRINGS_DOWN_BOW},
                                             {u"open-string", SymId::BRASS_MUTE_OPEN},
                                             {u"thumb-position", SymId::STRINGS_THUMB_POSITION},
                                             {u"double-tongue", SymId::DOUBLE_TONGUE_ABOVE},
                                             {u"triple-tongue", SymId::TRIPLE_TONGUE_ABOVE},
                                             {u"stopped", SymId::BRASS_MUTE_CLOSED},
                                             {u"snap-pizzicato", SymId::PLUCKED_SNAP_PIZZICATO_ABOVE},
                                             {u"heel", SymId::KEYBOARD_PEDAL_HEEL1},
                                             {u"toe", SymId::KEYBOARD_PEDAL_TOE2},
                                             {u"fingernails", SymId::PLUCKED_WITH_FINGERNAILS},
                                             {u"brass-bend", SymId::BRASS_BEND},
                                             {u"flip", SymId::BRASS_FLIP},
                                             {u"smear", SymId::BRASS_SMEAR},
                                             {u"open", SymId::BRASS_MUTE_OPEN},
                                             {u"half-muted", SymId::BRASS_MUTE_HALF_CLOSED}, // ignoring the smufl attribute
                                             {u"golpe", SymId::GUITAR_GOLPE},

                                             {u"belltree", SymId::HANDBELLS_BELLTREE},
                                             {u"damp", SymId::HANDBELLS_DAMP3},
                                             {u"echo", SymId::HANDBELLS_ECHO1},
                                             {u"gyro", SymId::HANDBELLS_GYRO},
                                             {u"hand martellato", SymId::HANDBELLS_HAND_MARTELLATO},
                                             {u"mallet lift", SymId::HANDBELLS_MALLET_LFT},
                                             {u"mallet table", SymId::HANDBELLS_MALLET_BELL_ON_TABLE},
                                             {u"martellato", SymId::HANDBELLS_MARTELLATO},
                                             {u"martellato lift", SymId::HANDBELLS_MARTELLATO_LIFT},
                                             {u"muted martellato", SymId::HANDBELLS_MUTED_MARTELLATO},
                                             {u"pluck lift", SymId::HANDBELLS_PLUCK_LIFT},
                                             {u"swing", SymId::HANDBELLS_SWING}};

    auto it = map.find(mxml_name);
    if (it != map.end()) {
        id = it->second;
        return true;
    } else {
        id = SymId::NO_SYM;
        return false;
    }
}

//---------------------------------------------------------
//   convertFermataToSymId
//---------------------------------------------------------

/**
 Convert a MusicXML fermata name to a MuseScore fermata.
 */

static SymId convert_fermata_to_sym_id(const String &mxml_name) {
    // map MusicXML fermata name to MuseScore symbol
    static const std::map<String, SymId> map{
        {u"normal", SymId::FERMATA_ABOVE},
        {u"angled", SymId::FERMATA_SHORT_ABOVE},
        {u"square", SymId::FERMATA_LONG_ABOVE},
        {u"double-angled", SymId::FERMATA_VERY_SHORT_ABOVE},
        {u"double-square", SymId::FERMATA_VERY_LONG_ABOVE},
        {u"double-dot", SymId::FERMATA_LONG_HENZE_ABOVE},
        {u"half-curve", SymId::FERMATA_SHORT_HENZE_ABOVE},
        {u"curlew", SymId::CURLEW_SIGN},
    };

    auto it = map.find(mxml_name);
    if (it != map.end()) {
        return it->second;
    }

    return SymId::FERMATA_ABOVE;
}

//---------------------------------------------------------
//   convertNotehead
//---------------------------------------------------------

/**
 Convert a MusicXML notehead name to a MuseScore headgroup.
 */

static NoteHeadGroup convert_notehead(String mxml_name) {
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

    auto it = map.find(mxml_name);
    if (it != map.end()) {
        return it->second;
    } else {
        LOGD("unknown notehead %s", muPrintable(mxml_name)); // TODO
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

static void add_text_to_note(int64_t byte_offset, String txt, String placement, String font_weight, double font_size, String font_style,
                             String font_family, bool visible, Color color, TextStyleType sub_type, const Score *, Note *note,
                             bool need_use_default_font) {
    if (note) {
        if (!txt.empty()) {
            TextBase *t = Factory::create_fingering(note, sub_type);
            t->set_plain_text(txt);
            if (!font_family.empty() && !need_use_default_font) {
                t->set_family(font_family);
                t->set_property_flags(Pid::FONT_FACE, PropertyFlags::UNSTYLED);
            }
            if (std::isnormal(font_size) && font_size > 0.0) {
                t->set_size(font_size);
                t->set_property_flags(Pid::FONT_SIZE, PropertyFlags::UNSTYLED);
            }
            if (!font_weight.empty()) {
                t->set_bold(font_weight == u"bold");
                t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
            if (!font_style.empty()) {
                t->set_italic(font_style == u"italic");
                t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
            if (!placement.empty()) {
                t->set_placement(placement == u"below" ? PlacementV::BELOW : PlacementV::ABOVE);
                t->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
                t->reset_property(Pid::OFFSET);
            }
            t->set_visible(visible);
            color_item(t, color);
            note->add(t);
        }
    } else {
        LOGD("%s", muPrintable(String(u"Error at byte offset %1: no note for text").arg(byte_offset))); // TODO
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

static void set_s_line_placement(SLine *sli, const String &placement) {
    if (placement == u"above" || placement == u"below") {
        if (sli->has_voice_assignment_properties()) {
            sli->set_property(Pid::DIRECTION, placement == u"above" ? DirectionV::UP : DirectionV::DOWN);
            sli->set_property_flags(Pid::DIRECTION, PropertyFlags::UNSTYLED);
        } else {
            sli->set_placement(placement == u"above" ? PlacementV::ABOVE : PlacementV::BELOW);
            sli->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
            sli->reset_property(Pid::OFFSET);
        }
    }
}

//---------------------------------------------------------
//   handleSpannerStart
//---------------------------------------------------------

// note that in case of overlapping spanners, handleSpannerStart is called for every spanner
// as spanners std::map allows only one value per key, this does not hurt at all

static void handle_spanner_start(SLine *new_sp, track_idx_t track, String &placement, const Fraction &tick, MusicXmlSpannerMap &spanners) {
    new_sp->set_track(track);
    set_s_line_placement(new_sp, placement);
    spanners[new_sp] = std::pair<int, int>(tick.ticks(), -1);
}

//---------------------------------------------------------
//   handleSpannerStop
//---------------------------------------------------------

static void handle_spanner_stop(SLine *cur_sp, track_idx_t track2, const Fraction &tick, MusicXmlSpannerMap &spanners) {
    //LOGD("handleSpannerStop(sp %p, track2 %d, tick %s (%d))", cur_sp, track2, muPrintable(tick.print()), tick.ticks());
    if (!cur_sp) {
        return;
    }

    cur_sp->set_track2(track2);
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

void MusicXmlParserPass2::add_error(const String &error) {
    if (!error.empty()) {
        logger_->log_error(error, &e_);
        errors_ += error_string_with_location(e_.byte_offset(), error) + u'\n';
    }
}

//---------------------------------------------------------
//   setChordRestDuration
//---------------------------------------------------------

/**
 * Set \a cr duration
 */

static void set_chord_rest_duration(ChordRest *cr, TDuration duration, const Fraction dura) {
    if (duration.is_measure()) {
        cr->set_duration_type(duration);
        cr->set_ticks(dura);
    } else {
        cr->set_duration_type(duration);
        cr->set_ticks(cr->duration_type().fraction());
    }
}

//---------------------------------------------------------
//   addRest
//---------------------------------------------------------

/**
 * Add a rest to the score
 * TODO: display step handling
 */

static Rest *add_rest(Score *, Measure *m, const Fraction &tick, const track_idx_t track, const int move, const TDuration duration,
                      const Fraction dura) {
    Segment *s = m->get_segment(SegmentType::CHORD_REST, tick);
    // Sibelius might export two rests at the same place, ignore the 2nd one
    // <?DoletSibelius Two NoteRests in same voice at same position may be an error?>
    // Same issue may result from trying to import incomplete tuplets
    if (s->element(track)) {
        LOGD("cannot add rest at tick %s (%d) track %zu: element already present", muPrintable(tick.to_string()), tick.ticks(),
             track); // TODO
        return nullptr;
    }

    Rest *cr = Factory::create_rest(s);
    set_chord_rest_duration(cr, duration, dura);
    cr->set_track(track);
    cr->set_staff_move(move);
    s->add(cr);
    return cr;
}

//---------------------------------------------------------
//   resetTuplets
//---------------------------------------------------------

static void reset_tuplets(Tuplets &tuplets) {
    for (auto &pair : tuplets) {
        Tuplet *tuplet = pair.second;
        if (tuplet) {
            const Fraction actual_duration = tuplet->elements_duration() / tuplet->ratio();
            const Fraction missing_duration = MusicXmlTupletState::missing_tuplet_duration(actual_duration);
            LOGD("tuplet %p not stopped at end of measure, tick %s duration %s missing %s", tuplet, muPrintable(tuplet->tick().to_string()),
                 muPrintable(actual_duration.to_string()), muPrintable(missing_duration.to_string()));
            if (actual_duration > Fraction(0, 1) && missing_duration > Fraction(0, 1)) {
                LOGD("add missing %s to previous tuplet", muPrintable(missing_duration.to_string()));
                const DurationElement *first_element = tuplet->elements().at(0);
                // appended the rest to the current end of the tuplet (firstElement->tick() + actualDuration)
                Rest *const extra_rest =
                    add_rest(first_element->score(), first_element->measure(), first_element->tick() + actual_duration,
                             first_element->track(), 0, TDuration{missing_duration * tuplet->ratio()}, missing_duration);
                if (extra_rest) {
                    extra_rest->set_tuplet(tuplet);
                    tuplet->add(extra_rest);
                }
            }
            const int normal_notes = tuplet->ratio().denominator();
            handle_tuplet_stop(tuplet, normal_notes);
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

void MusicXmlParserPass2::init_part_state(const String &part_id) {
    time_sig_dura_ = Fraction(0, 0); // invalid
    ties_.clear();
    unstarted_tie_notes_.clear();
    unended_tie_notes_.clear();
    last_volta_ = 0;
    has_drumset_ = false;
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        slurs_[i] = SlurDesc();
    }
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        trills_[i] = 0;
    }
    for (auto &i : glissandi_) {
        i[0] = i[1] = 0;
    }
    pedal_continue_ = 0;
    harmony_ = 0;
    trem_start_ = 0;
    fig_bass_ = 0;
    delayed_ottava_ = 0;
    multi_measure_rest_count_ = -1;
    measure_style_slash_ = MusicXmlSlash::NONE;
    extended_lyrics_.init();
    grace_note_lyrics_.clear();
    inferred_hairpins_.clear();
    inferred_tempo_lines_.clear();
    inferred_perc_.clear();

    nstaves_ = pass1_.get_part(part_id)->nstaves();
    measure_repeat_num_measures_.assign(nstaves_, 0);
    measure_repeat_count_.assign(nstaves_, 0);
}

//---------------------------------------------------------
//   findIncompleteSpannersInStack
//---------------------------------------------------------

static void find_incomplete_spanners_in_stack(const String &spanner_type, SpannerStack &stack, SpannerSet &res) {
    for (MusicXmlExtendedSpannerDesc &desc : stack) {
        if (desc.sp) {
            LOGD("%s not terminated at end of part", muPrintable(spanner_type));
            res.insert(desc.sp);
            desc = {};
        }
    }
}

//---------------------------------------------------------
//   findIncompleteSpannersAtPartEnd
//---------------------------------------------------------

SpannerSet MusicXmlParserPass2::find_incomplete_spanners_at_part_end() {
    SpannerSet res;
    find_incomplete_spanners_in_stack(u"bracket", brackets_, res);
    find_incomplete_spanners_in_stack(u"wedge", hairpins_, res);
    find_incomplete_spanners_in_stack(u"octave-shift", ottavas_, res);
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

static void cleanup_unterminated_tie(Tie *tie, const Score *score, bool fix_for_cross_staff = false) {
    Note *unterminated_tie_note = tie->start_note();
    const Chord *unterminated_chord = unterminated_tie_note->chord();

    // Dolet 6 doesn't export cross staff information
    // If a tie is unterminated, try to find a candidate to tie it to on a different track/stave
    if (fix_for_cross_staff) {
        const Segment *next_seg = score->tick2left_segment(unterminated_chord->tick() + unterminated_chord->ticks());
        if (next_seg) {
            const Part *part = unterminated_tie_note->part();
            const TrackRange track_range = part->track_range();
            for (track_idx_t track = track_range.start_track; track <= track_range.end_track; ++track) {
                const EngravingItem *el = next_seg->element(track);
                if (el && el->is_chord()) {
                    Note *matching_note = to_chord(el)->find_note(unterminated_tie_note->pitch());
                    if (matching_note && matching_note->tpc() == unterminated_tie_note->tpc()) {
                        tie->set_end_note(matching_note);
                        matching_note->set_tie_back(tie);
                        return;
                    }
                }
            }
        }
    }

    // Delete unterminated ties pending fully featured l.v. ties & ties over repeats
    unterminated_tie_note->remove(tie);
    delete tie;

    // Add Laissez Vibrer instead
    LaissezVib *lv_tie = Factory::create_laissez_vib(unterminated_tie_note);
    unterminated_tie_note->score()->undo_add_element(lv_tie);
}

//---------------------------------------------------------
//   isLikelyIncorrectPartName
//---------------------------------------------------------
/**
 Sibelius exports part names of the form "P#" rather than
 specifying print-object="no". This finds those.
 */

static bool is_likely_incorrect_part_name(const String &part_name) {
    static const std::wregex re(L"^P[0-9]+$");
    return part_name.contains(re);
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

void MusicXmlParserPass2::set_multi_measure_rest_count(int count) { multi_measure_rest_count_ = count; }

//---------------------------------------------------------
//   getAndDecMultiMeasureRestCount
//---------------------------------------------------------

/**
 Return current multi-measure rest counter.
 Decrement counter if possible (not beyond -1).
 */

int MusicXmlParserPass2::get_and_dec_multi_measure_rest_count() {
    int res = multi_measure_rest_count_;
    if (multi_measure_rest_count_ >= 0) {
        multi_measure_rest_count_--;
    }
    return res;
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserDirection::skip_log_curr_elem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skip_current_element();
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserPass2::skip_log_curr_elem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skip_current_element();
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Parse MusicXML in \a device and extract pass 2 data.
 */

Err MusicXmlParserPass2::parse(const ByteArray &data) {
    //LOGD("MusicXmlParserPass2::parse()");
    e_.set_data(data);
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
    while (e_.read_next_start_element()) {
        if (e_.name() == "score-partwise") {
            found = true;
            score_partwise();
        } else {
            logger_->log_error(u"this is not a MusicXML score-partwise file", &e_);
            e_.skip_current_element();
            return Err::FILE_BAD_FORMAT;
        }
    }

    if (!found) {
        logger_->log_error(u"this is not a MusicXML score-partwise file", &e_);
        return Err::FILE_BAD_FORMAT;
    }

    return Err::NO_ERROR;
}

//---------------------------------------------------------
//   createBarline
//---------------------------------------------------------

/*
 * Create a barline of the specified type.
 */

static std::unique_ptr<BarLine> create_barline(const Score *score, const track_idx_t track, const BarLineType type, const bool visible,
                                               const String &bar_style, const bool span_staff) {
    std::unique_ptr<BarLine> barline(Factory::create_bar_line(score->dummy()));
    barline->set_track(track);
    barline->set_bar_line_type(type);
    barline->set_span_staff(span_staff);
    barline->set_visible(visible);
    if (bar_style == u"tick") {
        barline->set_span_from(::mu::engraving::barline_span_tic_k1_from);
        barline->set_span_to(::mu::engraving::barline_span_tic_k1_to);
    } else if (bar_style == u"short") {
        barline->set_span_from(::mu::engraving::barline_span_shor_t1_from);
        barline->set_span_to(::mu::engraving::barline_span_shor_t1_to);
    }
    return barline;
}

//---------------------------------------------------------
//   addBarlineToMeasure
//---------------------------------------------------------

/*
 * Add barline to the measure at tick.
 */

static void add_barline_to_measure(Measure *measure, const Fraction tick, std::unique_ptr<BarLine> barline) {
    SegmentType st = SegmentType::BAR_LINE;
    if (tick == measure->end_tick()) {
        st = SegmentType::END_BAR_LINE;
    } else if (tick == measure->tick()) {
        st = SegmentType::BEGIN_BAR_LINE;
    }
    Segment *const segment = measure->get_segment(st, tick);
    segment->add(barline.release());
}

//---------------------------------------------------------
//   scorePartwise
//---------------------------------------------------------

/**
 Parse the MusicXML top-level (XPath /score-partwise) node.
 */

void MusicXmlParserPass2::score_partwise() {
    while (e_.read_next_start_element()) {
        if (e_.name() == "part") {
            part();
        } else if (e_.name() == "part-list") {
            part_list();
        } else {
            skip_log_curr_elem();
        }
    }
    // set last measure barline to normal or MuseScore will generate light-heavy EndBarline
    // this creates non-generated barlines spanning only the current instrument
    // BarLine::_spanStaff is set using the default in Staff::_barLineSpan
    Measure *const lm = score_->last_measure();
    if (lm && lm->end_bar_line_type() & BarLineType::NORMAL) {
        for (staff_idx_t staffidx = 0; staffidx < score_->nstaves(); ++staffidx) {
            const Staff *staff = score_->staff(staffidx);
            auto b = create_barline(score_, staffidx * ::mu::engraving::voices, BarLineType::NORMAL, true, u"", staff->bar_line_span());
            add_barline_to_measure(lm, lm->end_tick(), std::move(b));
        }
    }
    add_error(check_at_end_element(e_, u"score-partwise"));

    for (const auto &[_, sys_el] : sys_elements_) {
        score_->undo_add_element(sys_el);

        // Remove potential duplicated text for text and text lines
        const bool el_is_text = (sys_el->is_text_base() || sys_el->is_text_line_base()) && !sys_el->is_marker() && !sys_el->is_jump();
        if (!el_is_text) {
            continue;
        }
        const Score *score = sys_el->score();
        const Segment *seg = score ? score->tick2segment(sys_el->tick(), false, SegmentType::CHORD_REST) : nullptr;
        if (!seg) {
            continue;
        }
        const String sys_el_text = sys_el->is_text_base() ? to_text_base(sys_el)->plain_text() : to_text_line_base(sys_el)->begin_text();
        std::vector<EngravingItem *> annotations = seg->annotations();
        for (EngravingItem *existing_el : annotations) {
            const bool both_text = (existing_el->is_text_base() || existing_el->is_text_line_base()) && el_is_text;
            if (existing_el && existing_el != sys_el && both_text) {
                const String existing_text =
                    existing_el->is_text_base() ? to_text_base(existing_el)->plain_text() : to_text_line_base(existing_el)->begin_text();
                const bool text_matches = existing_text == sys_el_text;
                const bool placement_matches = existing_el->placement() == sys_el->placement();
                const bool staff_matches = existing_el->staff_idx() == sys_el->staff_idx();
                if (text_matches && (!existing_el->system_flag() || staff_matches) && placement_matches) {
                    score_->remove_element(existing_el);
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

void MusicXmlParserPass2::part_list() {
    while (e_.read_next_start_element()) {
        if (e_.name() == "score-part") {
            score_part();
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   scorePart
//---------------------------------------------------------

// Parse the /score-partwise/part-list/score-part node.
// TODO: nothing required for pass 2 ?

void MusicXmlParserPass2::score_part() {
    while (e_.read_next_start_element()) {
        if (e_.name() == "midi-instrument") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "score-instrument") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "part-name") {
            e_.skip_current_element(); // skip but don't log
        } else {
            skip_log_curr_elem();
        }
    }
}

static void create_time_tick(const Score *score, const Fraction &tick, const staff_idx_t staff_idx) {
    Measure *const measure = score->tick2measure(tick);
    if (!measure->find_segment(SegmentType::TIME_TICK | SegmentType::CHORD_REST, tick)) {
        EditTimeTickAnchors::create_time_tick_anchor(measure, tick - measure->tick(), staff_idx);
    }
}

static String replace_part_name_accidentals(const String &part_name) {
    String name = part_name;
    static const std::wregex re(L"((^|\\s|\\u00A0)[ABCDEF][b#\\u00AF]($|\\s|\\u00A0))");
    StringList res = name.search(re, {1}, SplitBehavior::SKIP_EMPTY_PARTS);
    if (!res.empty()) {
        String transp = res.at(0).replace(u"b", u"♭").replace(u"¯", u"♭").replace(u"#", u"♯");
        name.replace(re, transp);
        return name;
    }
    return part_name;
}

//---------------------------------------------------------
//   part
//---------------------------------------------------------

/**
 Parse the /score-partwise/part node.
 */

void MusicXmlParserPass2::part() {
    const String id = e_.attribute("id");

    if (!pass1_.has_part(id)) {
        logger_->log_error(String(u"MusicXmlParserPass2::part cannot find part '%1'").arg(id), &e_);
        skip_log_curr_elem();
        return;
    }

    init_part_state(id);

    const MusicXmlInstruments &instruments = pass1_.get_instruments(id);
    has_drumset_ = has_drumset(instruments);

    // set the part name
    Part *part = pass1_.get_part(id);
    MusicXmlPart mxml_part = pass1_.get_music_xml_part(id);
    String part_name = mxml_part.get_name();
    set_part_instruments(logger_, &e_, part, id, score_, pass1_.get_instr_list(id), pass1_.get_intervals(id), instruments, part_name);
    part_name = replace_part_name_accidentals(part_name);
    const bool inconsistent_visibility = mxml_part.get_print_name() != mxml_part.get_print_abbr();
    if (!is_likely_incorrect_part_name(part_name)) {
        part->set_long_name_all(part_name);
    } else {
        part->set_long_name_all(u"");
    }
    if (!mxml_part.get_abbr().empty()) {
        part->set_plain_short_name_all(mxml_part.get_abbr());
        if (inconsistent_visibility) {
            // TODO: improve logic to set visibility of part names
            part->set_long_name_all(u"");
        }
    } else {
        part->set_plain_short_name_all(u"");
    }

    if (pass1_.nparts() == 1 && mxml_part.get_print_name() && mxml_part.get_print_abbr()) {
        score_->style().set(Sid::HIDE_INSTRUMENT_NAME_IF_ONE_INSTRUMENT, false);
    }

#ifdef DEBUG_VOICE_MAPPER
    VoiceList voicelist = _pass1.get_voice_list(id);
    // debug: print voice mapper contents
    LOGD("voiceMapperStats: part '%s'", muPrintable(id));
    for (std::map<String, mu::engraving::VoiceDesc>::const_iterator i = voicelist.cbegin(); i != voicelist.cend(); ++i) {
        LOGD("voiceMapperStats: voice %s staff data %s", muPrintable(i.key()), muPrintable(i.value().to_string()));
    }
#endif

    // read the measures
    size_t nr = 0;       // current measure sequence number (always increments by one for each measure)
    measure_number_ = 0; // written measure number (doesn't always increment by 1)
    while (e_.read_next_start_element()) {
        if (e_.name() == "measure") {
            Fraction t = pass1_.get_measure_start(nr);
            if (t.is_valid()) {
                measure(id, t);
            } else {
                logger_->log_error(String(u"no valid start time for measure %1").arg(nr + 1), &e_);
                e_.skip_current_element();
            }
            ++nr;
        } else {
            skip_log_curr_elem();
        }
    }

    // stop all remaining extends for this part and add remaining ottava if present
    Measure *lm = part->score()->last_measure();
    if (lm) {
        track_idx_t strack = pass1_.track_for_part(id);
        track_idx_t etrack = strack + part->nstaves() * ::mu::engraving::voices;
        Fraction last_tick = lm->end_tick();
        for (track_idx_t trk = strack; trk < etrack; trk++) {
            extended_lyrics_.set_extend(-1, trk, last_tick);
        }
        if (delayed_ottava_ && delayed_ottava_->tick2() < last_tick) {
            handle_spanner_stop(delayed_ottava_, delayed_ottava_->track2(), last_tick, spanners_);
            delayed_ottava_ = nullptr;
        }
    }

    if (configuration()->infer_text_type()) {
        for (Hairpin *hp : inferred_hairpins_) {
            hp->score()->add_element(hp);
        }
        for (GradualTempoChange *line : inferred_tempo_lines_) {
            add_system_element(line, line->tick());
        }
    }

    const SpannerSet incomplete_spanners = find_incomplete_spanners_at_part_end();
    //LOGD("spanner list:");
    auto i = spanners_.cbegin();
    while (i != spanners_.cend()) {
        SLine *const sp = i->first;
        Fraction tick1 = Fraction::from_ticks(i->second.first);
        Fraction tick2 = Fraction::from_ticks(i->second.second);
        //LOGD("spanner %p tp %d isHairpin %d tick1 %s tick2 %s track1 %d track2 %d start %p end %p",
        //       sp, sp->type(), sp->isHairpin(), muPrintable(tick1.toString()), muPrintable(tick2.toString()),
        //       sp->track(), sp->track2(), sp->startElement(), sp->endElement());
        if (incomplete_spanners.find(sp) == incomplete_spanners.end()) {
            // complete spanner found
            // Create time tick for hairpin
            if (sp->is_hairpin()) {
                create_time_tick(score_, tick1, track2staff(sp->track()));
            }
            sp->set_tick(tick1);
            sp->set_tick2(tick2);
            sp->score()->add_element(sp);
        } else {
            // incomplete spanner -> cleanup
            delete sp;
        }
        ++i;
    }
    spanners_.clear();

    // Clean up any remaining ties
    MusicXmlTieMap tie_map = ties_;
    for (auto &tie : tie_map) {
        if (tie.second) {
            unended_tie_notes_.push_back(tie.second->start_note());
            ties_.erase(tie.first);
        }
    }
    // Find ties between different voices which may have been missed
    for (Note *start_note : unended_tie_notes_) {
        Tie *unended_tie = start_note->tie_for();
        if (!unended_tie) {
            continue;
        }
        const Chord *start_chord = start_note->chord();
        const Measure *start_measure = start_chord ? start_chord->measure() : nullptr;
        for (Note *end_note : unstarted_tie_notes_) {
            if (end_note->tie_back()) {
                continue;
            }
            const Chord *end_chord = end_note->chord();
            if (start_note->pitch() == end_note->pitch() && start_chord->tick() < end_chord->tick() &&
                (start_measure == end_chord->measure() || start_chord->tick() + start_chord->actual_ticks() == end_chord->tick())) {
                unended_tie->set_end_note(end_note);
                end_note->set_tie_back(unended_tie);
            }
        }

        if (!unended_tie->end_note()) {
            // Tie started with no matching end tags
            // Find a note of the same pitch in the same voice immediately following the start chord
            Segment *next_seg = start_chord ? start_chord->segment() : nullptr;
            while (next_seg && next_seg->tick() < start_chord->tick() + start_chord->ticks()) {
                next_seg = next_seg->next_cr(start_chord->track(), true);
            }
            EngravingItem *next_el = next_seg ? next_seg->element(start_note->track()) : nullptr;
            Chord *next_chord = next_el && next_el->is_chord() ? to_chord(next_el) : nullptr;
            Note *matching_note = next_chord ? next_chord->find_note(start_note->pitch()) : nullptr;

            if (matching_note && matching_note->tpc() == start_note->tpc() && matching_note != start_note) {
                unended_tie->set_end_note(matching_note);
                matching_note->set_tie_back(unended_tie);
            } else if (start_chord) {
                // try other voices in the stave
                const Part *p = start_chord->part();
                const TrackRange track_range = p->track_range();
                for (track_idx_t track = track_range.start_track; track < track_range.end_track + ::mu::engraving::voices; track++) {
                    next_el = next_seg ? next_seg->element(track) : nullptr;
                    next_chord = next_el && next_el->is_chord() ? to_chord(next_el) : nullptr;
                    if (next_chord && next_chord->v_staff_idx() != start_chord->v_staff_idx()) {
                        continue;
                    }
                    matching_note = next_chord ? next_chord->find_note(start_note->pitch()) : nullptr;
                    if (matching_note && matching_note->tpc() == start_note->tpc() && matching_note != start_note) {
                        unended_tie->set_end_note(matching_note);
                        matching_note->set_tie_back(unended_tie);
                    }
                }
            }
        }

        if (!unended_tie->end_note()) {
            cleanup_unterminated_tie(unended_tie, score_, pass1_.exporter_software() == MusicXmlExporterSoftware::DOLET6);
        }
    }

    ties_.clear();
    unstarted_tie_notes_.clear();
    unended_tie_notes_.clear();

    if (has_drumset_) {
        Drumset *drumset = new Drumset;
        const MusicXmlInstruments &instruments_after_pass2 = pass1_.get_instruments(id);
        init_drumset(drumset, instruments_after_pass2);
        // set staff type to percussion if incorrectly imported as pitched staff
        // Note: part has been read, staff type already set based on clef type and staff-details
        // but may be incorrect for a percussion staff that does not use a percussion clef
        set_staff_type_percussion(part, drumset);
    }

    bool show_part = false;
    for (Staff *staff : part->staves()) {
        if (staff->visible()) {
            show_part = true;
            break;
        }
    }

    part->set_show(show_part);

    add_error(check_at_end_element(e_, u"part"));
}

//---------------------------------------------------------
//   findMeasure
//---------------------------------------------------------

/**
 In Score \a score find the measure starting at \a tick.
 */

static Measure *find_measure(const Score *score, const Fraction &tick) {
    for (Measure *m = score->first_measure(); m; m = m->next_measure()) {
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

static void remove_beam(Beam *&beam) {
    for (size_t i = 0; i < beam->elements().size(); ++i) {
        beam->elements().at(i)->set_beam_mode(BeamMode::NONE);
    }
    delete beam;
    beam = nullptr;
}

//---------------------------------------------------------
//   handleBeamAndStemDir
//---------------------------------------------------------

static void handle_beam_and_stem_dir(ChordRest *cr, const BeamMode bm, const DirectionV sd, Beam *&beam, bool has_beaming_info,
                                     Color beam_color, const String fan) {
    if (!cr) {
        return;
    }
    // create a new beam
    if (bm == BeamMode::BEGIN) {
        // if currently in a beam, delete it
        if (beam) {
            LOGD("handleBeamAndStemDir() new beam, removing previous incomplete beam %p", beam);
            remove_beam(beam);
        }
        // create a new beam
        beam = Factory::create_beam(cr->score());
        beam->set_track(cr->track());
        beam->set_direction(sd);
        color_item(beam, beam_color);
        if (!fan.empty() && fan != u"none") {
            beam->set_as_feathered(fan == u"rit");
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
            remove_beam(beam);
        } else if (bm == BeamMode::NONE) {
            LOGD("handleBeamAndStemDir() in beam, bm BeamMode::NONE -> abort beam");
            // reset beam mode for all elements and remove the beam
            remove_beam(beam);
        } else if (!(bm == BeamMode::BEGIN || bm == BeamMode::MID || bm == BeamMode::END || bm == BeamMode::BEGIN16 ||
                     bm == BeamMode::BEGIN32)) {
            LOGD("handleBeamAndStemDir() in beam, bm %d -> abort beam", static_cast<int>(bm));
            // reset beam mode for all elements and remove the beam
            remove_beam(beam);
        } else {
            // actually add cr to the beam
            beam->add(cr);
        }
    }
    // if no beam, set stem direction on chord itself
    if (!beam) {
        to_chord(cr)->set_stem_direction(sd);
        // set beam to none if score has beaming information and note can get beam, otherwise
        // set to auto
        bool can_get_beam = (cr->duration_type().type() >= DurationType::V_EIGHTH && cr->duration_type().type() <= DurationType::V_1024TH);
        if (has_beaming_info && can_get_beam) {
            cr->set_beam_mode(BeamMode::NONE);
        } else {
            cr->set_beam_mode(BeamMode::AUTO);
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

static void mark_user_accidentals(const staff_idx_t first_staff, const size_t staves, const Key key, const Measure *measure,
                                  const std::map<Note *, int> &alter_map) {
    std::map<int, bool> acc_tmp;

    AccidentalState curr_acc;
    curr_acc.init(key);
    SegmentType st = SegmentType::CHORD_REST;
    for (mu::engraving::Segment *segment = measure->first(st); segment; segment = segment->next(st)) {
        for (track_idx_t track = 0; track < staves * ::mu::engraving::voices; ++track) {
            EngravingItem *e = segment->element(first_staff * ::mu::engraving::voices + track);
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(e);
            for (Note *nt : chord->notes()) {
                if (muse::contains(alter_map, nt)) {
                    int alter = alter_map.at(nt);
                    int ln = abs_step(nt->tpc(), nt->pitch());
                    bool error = false;
                    AccidentalVal curr_acc_val = curr_acc.accidental_val(ln, error);
                    if (error) {
                        continue;
                    }
                    if ((alter == -1 && curr_acc_val == AccidentalVal::FLAT &&
                         nt->accidental()->accidental_type() == AccidentalType::FLAT && !muse::value(acc_tmp, ln, false)) ||
                        (alter == 0 && curr_acc_val == AccidentalVal::NATURAL &&
                         nt->accidental()->accidental_type() == AccidentalType::NATURAL && !muse::value(acc_tmp, ln, false)) ||
                        (alter == 1 && curr_acc_val == AccidentalVal::SHARP &&
                         nt->accidental()->accidental_type() == AccidentalType::SHARP && !muse::value(acc_tmp, ln, false))) {
                        nt->accidental()->set_role(AccidentalRole::USER);
                    } else if (Accidental::is_microtonal(nt->accidental()->accidental_type()) &&
                               nt->accidental()->accidental_type() < AccidentalType::END) {
                        // microtonal accidental
                        nt->accidental()->set_role(AccidentalRole::USER);
                        acc_tmp.insert({ln, false});
                    } else {
                        acc_tmp.insert({ln, true});
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

static void coerce_grace_cue(Chord *main_chord, Chord *grace_chord) {
    if (main_chord->is_small()) {
        grace_chord->set_small(true);
    }
    bool any_plays = false;
    for (const Note *n : main_chord->notes()) {
        any_plays |= n->play();
    }
    if (!any_plays) {
        for (Note *gn : grace_chord->notes()) {
            gn->set_play(false);
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

static void add_grace_chords_after(Chord *c, GraceChordList &gcl, size_t &gac) {
    if (!c) {
        return;
    }

    while (gac > 0) {
        if (gcl.size() > 0) {
            Chord *grace_chord = muse::take_first(gcl);
            std::vector<EngravingItem *> el = grace_chord->el(); // copy, because modified during loop
            for (EngravingItem *e : el) {
                if (e->is_fermata()) {
                    e->set_ownership_parent(c->segment());
                    c->segment()->add(e);
                    grace_chord->remove_fermata(to_fermata(e));
                }
            }
            grace_chord->to_grace_after();
            c->add(grace_chord); // TODO check if same voice ?
            coerce_grace_cue(c, grace_chord);
            LOGD("addGraceChordsAfter chord %p grace after chord %p", c, grace_chord);
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

static void add_grace_chords_before(Chord *c, GraceChordList &gcl) {
    for (int i = static_cast<int>(gcl.size()) - 1; i >= 0; i--) {
        Chord *gc = gcl.at(i);
        std::vector<EngravingItem *> el = gc->el(); // copy, because modified during loop
        for (EngravingItem *e : el) {
            if (e->is_fermata()) {
                e->set_ownership_parent(c->segment());
                c->segment()->add(e);
                gc->remove_fermata(to_fermata(e));
            }
        }
        c->add(gc); // TODO check if same voice ?
        coerce_grace_cue(c, gc);
    }
    gcl.clear();
}

static bool can_add_tempo_text(const Score *const score, const int tick) {
    const Measure *measure = score->tick2measure(Fraction::from_ticks(tick));
    const Segment *segment = measure ? measure->find_segment(SegmentType::CHORD_REST, Fraction::from_ticks(tick)) : nullptr;
    if (!segment) {
        return true;
    }

    for (const EngravingItem *e : segment->annotations()) {
        if (e->is_tempo_text() && to_tempo_text(e)->tempo() != Constants::default_tempo) {
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

void MusicXmlParserPass2::measure(const String &part_id, const Fraction time) {
    // "measure numbers" don't have to be actual numbers in MusicXML
    bool is_numeric_measure_number = false;
    int parsed_measure_number = 0;

    AsciiStringView number_a = e_.ascii_attribute("number");
    if (!number_a.empty()) {
        if (number_a.at(0).ascii() == 'X') {
            is_numeric_measure_number = false;
        } else {
            parsed_measure_number = number_a.to_int(&is_numeric_measure_number);
        }
    }

    //LOGD("measure %d start", parsedMeasureNumber);

    Measure *measure = find_measure(score_, time);
    if (!measure) {
        logger_->log_error(String(u"measure at tick %1 not found!").arg(time.ticks()), &e_);
        skip_log_curr_elem();
        return;
    }

    if (e_.ascii_attribute("implicit") == "yes") {
        // Implicit measure: expect measure number to be unchanged.
        measure->set_exclude_from_numbering(true);
    } else {
        // Normal measure: expect number to have increased by one.
        ++measure_number_;
    }

    if (is_numeric_measure_number) {
        // Actual measure number may differ from expected value.
        measure->set_measure_number_offset(parsed_measure_number - measure_number_);
        measure_number_ = parsed_measure_number;
    }

    // set measure's RepeatFlag to none because musicXML is allowing single measure repeat and no ordering in repeat start and end barlines
    measure->set_repeat_start(false);
    measure->set_repeat_end(false);

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

    Fraction m_time;                            // current time stamp within measure
    Fraction prev_time;                         // time stamp within measure previous chord
    Chord *prev_chord = 0;                      // previous chord
    Fraction m_dura;                            // current total measure duration
    GraceChordList gcl;                         // grace chords collected sofar
    size_t gac = 0;                             // grace after count in the grace chord list
    Beams beams;                                // Current beam for each voice in the current part
    String cv = u"1";                           // current voice for chords, default is 1
    FiguredBassList fbl;                        // List of figured bass elements under a single note
    MusicXmlTupletStates tuplet_states;         // Tuplet state for each voice in the current part
    Tuplets tuplets;                            // Current tuplet for each voice in the current part
    DelayedDirectionsList delayed_directions;   // Directions to be added to score *after* collecting all and sorting
    InferredFingeringsList inferred_fingerings; // Directions to be reinterpreted as Fingerings
    ArpeggioMap arp_map;
    DelayedArpMap delayed_arps;
    HarmonyMap delayed_harmony;
    bool measure_has_coda = false;
    String tempo_string; // helper for Dorico imports

    // collect candidates for courtesy accidentals to work out at measure end
    std::map<Note *, int> alter_map;

    while (e_.read_next_start_element()) {
        if (e_.name() == "attributes") {
            attributes(part_id, measure, time + m_time);
        } else if (e_.name() == "direction") {
            MusicXmlParserDirection dir(e_, score_, pass1_, *this, logger_);
            if (!tempo_string.empty() && pass1_.exporter_software() == MusicXmlExporterSoftware::DORICO) {
                dir.set_bpm(tempo_string.to_double());
                tempo_string.clear();
            }
            dir.direction(part_id, measure, time + m_time, spanners_, delayed_directions, inferred_fingerings, delayed_harmony,
                          measure_has_coda, segnos_);
        } else if (e_.name() == "figured-bass") {
            FiguredBass *fb = figured_bass();
            if (fb) {
                fbl.push_back(fb);
            }
        } else if (e_.name() == "harmony") {
            harmony(part_id, measure, time + m_time, delayed_harmony);
        } else if (e_.name() == "note") {
            if (!tempo_string.empty()) {
                // sound tempo="..."
                // create an invisible default TempoText
                // to prevent duplicates, only if none is present yet
                Fraction tick = time + m_time;

                if (can_add_tempo_text(score_, tick.ticks())) {
                    double tpo = tempo_string.to_double() / 60;
                    TempoText *t = Factory::create_tempo_text(score_->dummy());
                    t->set_xml_text(
                        String(u"%1 = %2").arg(TempoText::duration2tempo_text_string(TDuration(DurationType::V_QUARTER)), tempo_string));
                    t->set_visible(false);
                    t->set_tempo(tpo);
                    t->set_follow_text(true);

                    add_elem_offset(t, pass1_.track_for_part(part_id), u"above", measure, tick);
                }
                tempo_string.clear();
            }

            // Correct delayed ottava tick
            if (delayed_ottava_ && delayed_ottava_->tick2() < time + m_time) {
                handle_spanner_stop(delayed_ottava_, delayed_ottava_->track2(), time + m_time, spanners_);
                delayed_ottava_ = nullptr;
            }
            Fraction missing_prev;
            Fraction dura;
            Fraction missing_curr;
            int alt = -10; // any number outside range of xml-tag "alter"
            // note: chord and grace note handling done in note()
            // dura > 0 if valid rest or first note of chord found
            Note *n = note(part_id, measure, time + m_time, time + prev_time, missing_prev, dura, missing_curr, cv, gcl, gac, beams, fbl,
                           alt, tuplet_states, tuplets, arp_map, delayed_arps);
            if (n && !n->chord()->is_grace()) {
                prev_chord = n->chord(); // remember last non-grace chord
            }
            if (n && n->accidental() && n->accidental()->accidental_type() != AccidentalType::NONE) {
                alter_map.insert({n, alt});
            }
            if (missing_prev.is_valid()) {
                m_time += missing_prev;
            }
            if (dura.is_valid() && dura > Fraction(0, 1)) {
                prev_time = m_time; // save time stamp last chord created
                m_time += dura;
                if (m_time > m_dura) {
                    m_dura = m_time;
                }
            }
            if (missing_curr.is_valid()) {
                m_time += missing_curr;
            }
            //LOGD("added note %p chord %p gac %d", n, n ? n->chord() : 0, gac);
        } else if (e_.name() == "forward") {
            Fraction dura;
            forward(dura);
            if (dura.is_valid()) {
                m_time += dura;
                if (m_time > m_dura) {
                    m_dura = m_time;
                }
            }
        } else if (e_.name() == "backup") {
            Fraction dura;
            backup(dura);
            if (dura.is_valid()) {
                if (dura <= m_time) {
                    m_time -= dura;
                } else {
                    logger_->log_error(u"backup beyond measure start", &e_);
                    m_time.set(0, 1);
                }
                // check if the tick position is smaller than the minimum division resolution
                // (possibly caused by rounding errors) and in that case set position to 0
                if (m_time.is_not_zero() && (divs_ > 0) && (m_time < Fraction(1, 4 * divs_))) {
                    logger_->log_error(u"backup to a fractional tick smaller than the minimum division", &e_);
                    m_time.set(0, 1);
                }
            }
        } else if (e_.name() == "sound") {
            tempo_string = e_.attribute("tempo");
            e_.skip_current_element();
        } else if (e_.name() == "barline") {
            barline(part_id, measure, time + m_time);
        } else if (e_.name() == "print") {
            if (score_->parts()[0] == pass1_.get_part(part_id)) {
                // only process for first part
                while (e_.read_next_start_element()) {
                    if (e_.name() == "page-layout") {
                        e_.skip_current_element(); // skip but don't log
                    } else if (e_.name() == "system-layout") {
                        e_.skip_current_element(); // skip but don't log
                    } else if (e_.name() == "staff-layout") {
                        e_.skip_current_element(); // skip but don't log
                    } else if (e_.name() == "measure-layout") {
                        measure_layout(measure);
                    } else {
                        skip_log_curr_elem();
                    }
                }
            } else {
                e_.skip_current_element();
            }
        } else {
            skip_log_curr_elem();
        }

        /*
         LOGD("mTime %s (%s) mDura %s (%s)",
         muPrintable(mTime.print()),
         muPrintable(mTime.reduced().print()),
         muPrintable(mDura.print()),
         muPrintable(mDura.reduced().print()));
         */
        m_dura.reduce();
        m_time.reduce();
    }

    // convert remaining grace chords to grace after
    gac = gcl.size();
    add_grace_chords_after(prev_chord, gcl, gac);

    // prevent tuplets from crossing measure boundaries
    reset_tuplets(tuplets);

    // fill possible gaps in voice 1
    Part *part = pass1_.get_part(part_id); // should not fail, we only get here if the part exists
    fill_gaps_in_first_voices(measure, part);

    // Prevent any beams from extending into the next measure
    for (auto &[_, beam] : beams) {
        if (beam) {
            remove_beam(beam);
        }
    }

    // Sort and add inferred fingerings
    std::sort(inferred_fingerings.begin(), inferred_fingerings.end(),
              // Lambda: sort by absolute value of totalY
              [](const MusicXmlInferredFingering *a, const MusicXmlInferredFingering *b) -> bool {
                  return std::abs(a->total_y()) < std::abs(b->total_y());
              });
    for (MusicXmlInferredFingering *inferred_fingering : inferred_fingerings) {
        if (!inferred_fingering->find_and_add_to_notes(measure)) {
            // Could not find notes to add to; print as direction
            delayed_directions.push_back(inferred_fingering->to_delayed_direction());
        }
        delete inferred_fingering;
    }

    for (auto &del_harmony : delayed_harmony) {
        HarmonyDesc harmony_desc = del_harmony.second;
        Fraction tick = Fraction::from_ticks(del_harmony.first);
        if (FretDiagram *fd = harmony_desc.fret_diagram_) {
            fd->set_track(harmony_desc.track_);
            Segment *s = measure->get_segment(SegmentType::CHORD_REST, tick);
            if (Harmony *harmony = harmony_desc.harmony_) {
                harmony->set_property(Pid::ALIGN, Align(AlignH::HCENTER, AlignV::TOP));
                harmony->set_track(harmony_desc.track_);
                fd->add(harmony);
            }
            s->add(fd);
        } else if (Harmony *harmony = harmony_desc.harmony_) {
            harmony->set_track(harmony_desc.track_);
            Segment *s = measure->get_segment(SegmentType::CHORD_REST, tick);
            s->add(harmony);
        }
    }

    // Sort and add delayed directions
    std::sort(delayed_directions.begin(), delayed_directions.end(),
              // Lambda: sort by absolute value of totalY
              [](const MusicXmlDelayedDirectionElement *a, const MusicXmlDelayedDirectionElement *b) -> bool {
                  return std::abs(a->total_y()) < std::abs(b->total_y());
              });
    for (MusicXmlDelayedDirectionElement *direction : delayed_directions) {
        add_elem_offset(direction->element(), direction->track(), direction->placement(), direction->measure(), direction->tick());
    }

    // TODO:
    // - how to handle _timeSigDura.isZero (shouldn't happen ?)
    // - how to handle unmetered music
    if (time_sig_dura_.is_valid() && !time_sig_dura_.is_zero()) {
        measure->set_timesig(time_sig_dura_);
    }

    // mark superfluous accidentals as user accidentals
    const staff_idx_t score_rel_staff = score_->staff_idx(part);
    const Key key = score_->staff(score_rel_staff)->key_sig_event(time).key();
    mark_user_accidentals(score_rel_staff, part->nstaves(), key, measure, alter_map);

    // multi-measure rest handling
    if (get_and_dec_multi_measure_rest_count() == 0) {
        // measure is first measure after a multi-measure rest
        measure->set_break_multi_measure_rest(true);
    }

    set_measure_repeats(score_rel_staff, measure);

    add_error(check_at_end_element(e_, u"measure"));
}

//---------------------------------------------------------
//   setMeasureRepeats
//---------------------------------------------------------

/**
 Measure repeat handling, based on values set in measureStyle().
 */

void MusicXmlParserPass2::set_measure_repeats(const staff_idx_t score_rel_staff, Measure *measure) {
    for (staff_idx_t i = 0; i < nstaves_; ++i) {
        staff_idx_t staff_idx = score_rel_staff + i;
        track_idx_t track = staff2track(staff_idx);
        if (measure_repeat_num_measures_[i]) {
            // delete anything already added to measure
            score_->make_gap(measure->first(SegmentType::CHORD_REST), track, measure->stretched_len(score_->staff(staff_idx)), 0);

            if (measure_repeat_count_[i] == measure_repeat_num_measures_[i]) {
                // starting a new one, not continuing a multi-measure group
                measure_repeat_count_[i] = 1;
            } else {
                // continue building measure repeat group
                ++measure_repeat_count_[i];
            }

            if (((measure_repeat_num_measures_[i] % 2) && (measure_repeat_count_[i] - 1 == measure_repeat_num_measures_[i] / 2)) ||
                (!(measure_repeat_num_measures_[i] % 2) && (measure_repeat_count_[i] == measure_repeat_num_measures_[i] / 2))) {
                // MeasureRepeat element goes in center measure of group if odd-numbered,
                // or last measure of first half of group if even-numbered
                Transaction &tx = score_->transaction_manager()->current_or_dummy_transaction();
                EditMeasureRepeat::add_measure_repeat(tx, score_, measure->tick(), track, measure_repeat_num_measures_[i]);
            } else {
                // measures that are part of group but do not contain the element have undisplayed whole rests
                score_->add_rest(measure->tick(), track, TDuration(DurationType::V_MEASURE), 0);
            }
        } else {
            // measureStyle() hit a "stop" element most recently
            measure_repeat_count_[i] = 0;
        }
        measure->set_measure_repeat_count(measure_repeat_count_[i], staff_idx);
    }
}

//---------------------------------------------------------
//   measureLayout
//---------------------------------------------------------

void MusicXmlParserPass2::measure_layout(Measure *measure) {
    while (e_.read_next_start_element()) {
        if (e_.name() == "measure-distance") {
            const Spatium val(e_.read_double() / 10.0);
            if (!measure->prev() || !measure->prev()->is_h_box()) {
                MeasureBase *gap = score_->insert_box(ElementType::HBOX, measure);
                to_h_box(gap)->set_box_width(val);
            }
        } else {
            skip_log_curr_elem();
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

void MusicXmlParserPass2::attributes(const String &part_id, Measure *measure, const Fraction &tick) {
    while (e_.read_next_start_element()) {
        if (e_.name() == "clef") {
            clef(part_id, measure, tick);
        } else if (e_.name() == "divisions") {
            divisions();
        } else if (e_.name() == "key") {
            key(part_id, measure, tick);
        } else if (e_.name() == "measure-style") {
            measure_style(measure);
        } else if (e_.name() == "staff-details") {
            staff_details(part_id, measure);
        } else if (e_.name() == "time") {
            time(part_id, measure, tick);
        } else if (e_.name() == "transpose") {
            e_.skip_current_element(); // skip but don't log
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   setStaffLines
//---------------------------------------------------------

/**
 Set stafflines and barline span for a single staff
 */

static void set_staff_lines(Score *score, staff_idx_t staff_idx, int stafflines) {
    score->staff(staff_idx)->set_lines(Fraction(0, 1), stafflines);
    score->staff(staff_idx)->set_bar_line_to(0); // default
}

//---------------------------------------------------------
//   staffDetails
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/staff-details node.
 */

void MusicXmlParserPass2::staff_details(const String &part_id, Measure *measure) {
    //logDebugTrace("MusicXmlParserPass2::staffDetails");

    Part *part = pass1_.get_part(part_id);
    IF_ASSERT_FAILED(part) { return; }
    size_t staves = part->nstaves();

    String str_number = e_.attribute("number");
    int n = 0; // default
    if (!str_number.empty()) {
        n = pass1_.get_music_xml_part(part_id).staff_number_to_index(str_number.to_int());
        if (n < 0 || n >= int(staves)) {
            logger_->log_error(String(u"invalid staff-details number %1 (may be hidden)").arg(str_number), &e_);
            n = 0;
        }
    }

    staff_idx_t staff_idx = score_->staff_idx(part) + n;

    StringData string_data;
    AsciiStringView visible = e_.ascii_attribute("print-object");
    AsciiStringView spacing = e_.ascii_attribute("print-spacing");
    if (visible == "no") {
        // EITHER:
        //  1) this indicates an empty staff that is hidden
        //  2) this indicates a cutaway measure. if it is a cutaway measure then print-spacing will be yes
        if (spacing == "yes") {
            measure->set_staff_visible(staff_idx, false);
        } else if (measure && !measure->has_voices(staff_idx) && measure->is_only_rests(staff_idx * ::mu::engraving::voices)) {
            // measures with print-object="no" are generally exported by exporters such as dolet when empty staves are hidden.
            // for this reason, if we see print-object="no" (and no print-spacing), we can assume that this indicates we should set
            // the hide empty staves style.
            score_->style().set(Sid::HIDE_EMPTY_STAVES, true);
            score_->style().set(Sid::DONT_HIDE_STAVES_IN_FIRST_SYSTEM, false);
        } else {
            // this doesn't apply to a measure, so we'll assume the entire staff has to be hidden.
            score_->staff(staff_idx)->set_visible(false);
        }
    } else if (visible == "yes" || visible.empty()) {
        if (measure) {
            score_->staff(staff_idx)->set_visible(true);
            measure->set_staff_visible(staff_idx, true);
        }
    } else {
        logger_->log_error(String(u"print-object should be \"yes\" or \"no\""));
    }

    int staff_lines = 0;
    while (e_.read_next_start_element()) {
        if (e_.name() == "staff-lines") {
            // save staff lines for later
            staff_lines = e_.read_int();
            // for a TAB staff also resize the string table and init with zeroes
            if (0 < staff_lines) {
                string_data.string_list() = std::vector<InstrString>(staff_lines);
            } else {
                logger_->log_error(String(u"illegal staff-lines %1").arg(staff_lines), &e_);
            }
        } else if (e_.name() == "line-detail") {
            const Color color = Color::from_string(e_.attribute("color"));
            if (color.is_valid()) {
                score_->staff(staff_idx)->staff_type(Fraction(0, 1))->set_color(color);
            }
            if (e_.attribute("print-object") == u"no") {
                score_->staff(staff_idx)->staff_type(Fraction(0, 1))->set_invisible(true);
            }
            e_.skip_current_element();
        } else if (e_.name() == "staff-tuning") {
            staff_tuning(&string_data);
        } else if (e_.name() == "staff-size") {
            const double scaling = e_.double_attribute("scaling", 100.0);
            const Spatium val(e_.read_double() / scaling);
            score_->staff(staff_idx)->set_property(Pid::MAG, scaling / 100.0);
            score_->staff(staff_idx)->set_property(Pid::LINE_DISTANCE, val);
        } else {
            skip_log_curr_elem();
        }
    }

    if (staff_lines > 0) {
        set_staff_lines(score_, staff_idx, staff_lines);
    }

    Instrument *i = part->instrument();
    if (score_->staff(staff_idx)->is_tab_staff(Fraction(0, 1))) {
        if (i->string_data()->frets() == 0) {
            string_data.set_frets(25);
        } else {
            string_data.set_frets(i->string_data()->frets());
        }
        if (string_data.strings() > 0) {
            i->set_string_data(string_data);
        }
    } else if (string_data.strings() > 0) {
        logger_->log_error(u"trying to change string data for non-TAB staff (not supported)", &e_);
    }
}

//---------------------------------------------------------
//   staffTuning
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/staff-details/staff-tuning node.
 */

void MusicXmlParserPass2::staff_tuning(StringData *t) {
    //logDebugTrace("MusicXmlParserPass2::staffTuning");

    // ignore <staff-tuning> if not a TAB staff
    if (!t) {
        logger_->log_error(u"<staff-tuning> on non-TAB staff", &e_);
        skip_log_curr_elem();
        return;
    }

    int line = e_.int_attribute("line");
    int step = 0;
    int alter = 0;
    int octave = 0;
    while (e_.read_next_start_element()) {
        if (e_.name() == "tuning-alter") {
            alter = e_.read_text().trimmed().to_int();
        } else if (e_.name() == "tuning-octave") {
            octave = e_.read_int();
        } else if (e_.name() == "tuning-step") {
            String str_step = e_.read_text();
            int pos = static_cast<int>(String(u"CDEFGAB").index_of(str_step));
            if (str_step.size() == 1 && pos >= 0 && pos < 7) {
                step = pos;
            } else {
                logger_->log_error(String(u"invalid step '%1'").arg(str_step), &e_);
            }
        } else {
            skip_log_curr_elem();
        }
    }

    if (0 < line && line <= static_cast<int>(t->string_list().size())) {
        int pitch = music_xml_step_alt_oct2_pitch(step, alter, octave);
        if (pitch >= 0) {
            t->string_list()[line - 1].pitch = pitch;
        } else {
            logger_->log_error(String(u"invalid string %1 tuning step/alter/oct %2/%3/%4").arg(line).arg(step).arg(alter).arg(octave), &e_);
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

void MusicXmlParserPass2::measure_style(Measure *measure) {
    AsciiStringView staff_number_string = e_.ascii_attribute("number");

    // by default, apply to all staves in part
    int start_staff = 0;
    int end_staff = static_cast<int>(nstaves_) - 1;

    // but if a staff number was specified in the measure-style tag, use that instead
    if (!staff_number_string.empty()) {
        int staff_number = staff_number_string.to_int();
        if (staff_number < 1 || staff_number > static_cast<int>(nstaves_)) {
            logger_->log_error(String(u"measure-style staff number can only be int from 1 to _nstaves."));
        }
        --staff_number; // convert to 0-based
        end_staff = start_staff = staff_number;
    }

    while (e_.read_next_start_element()) {
        if (e_.name() == "multiple-rest") {
            int multiple_rest = e_.read_int();
            if (multiple_rest > 1) {
                multi_measure_rest_count_ = multiple_rest;
                score_->style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
                measure->set_break_multi_measure_rest(true);
            } else {
                logger_->log_error(String(u"multiple-rest %1 not supported").arg(multiple_rest), &e_);
            }
        } else if (e_.name() == "measure-repeat") {
            String start_stop = e_.attribute("type");
            // note: possible "slashes" attribute is either redundant with numMeasures or not supported by MuseScore, so ignored either way
            if (start_stop == u"start") {
                int num_measures = e_.read_int();
                for (int i = start_staff; i <= end_staff; i++) {
                    measure_repeat_num_measures_[i] = num_measures;
                    measure_repeat_count_[i] =
                        num_measures; // measure repeat(s) haven't actually started yet in current measure, so this is a lie,
                                      // but if we pretend it's true then everything is set for the next measure to restart
                }
            } else { // "stop"
                for (int i = start_staff; i <= end_staff; i++) {
                    measure_repeat_num_measures_[i] = 0;
                }
                e_.skip_current_element(); // since not reading any text inside stop tag, we are done with this element
            }
        } else if (e_.name() == "slash") {
            String type = e_.attribute("type");
            String stems = e_.attribute("use-stems");
            measure_style_slash_ =
                type == u"start" ? (stems == u"yes" ? MusicXmlSlash::RHYTHM : MusicXmlSlash::SLASH) : MusicXmlSlash::NONE;
            e_.skip_current_element();
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   preventNegativeTick
//---------------------------------------------------------
/**
  Prevent an offset that would result in a negative tick (set offset to -tick instead, resulting in a tick of 0)
 */

static void prevent_negative_tick(const Fraction &tick, Fraction &offset, MusicXmlLogger *logger) {
    if (tick + offset < Fraction(0, 1)) {
        logger->log_error(String(u"illegal offset %1 at tick %2").arg(offset.ticks()).arg(tick.ticks()));
        offset = -tick;
    }
}

String MusicXmlParserDirection::placement() const {
    if (placement_.empty() && has_total_y()) {
        return total_y() < 0 ? u"above" : u"below";
    } else {
        return placement_;
    }
}

static GradualTempoChangeType get_tempo_change_type_from_string(String txt) {
    txt = txt.to_lower();
    if (txt.contains(u"accel")) {
        return GradualTempoChangeType::ACCELERANDO;
    } else if (txt.contains(u"allarg")) {
        return GradualTempoChangeType::ALLARGANDO;
    } else if (txt == u"calando") {
        return GradualTempoChangeType::CALANDO;
    } else if (txt.contains(u"lent")) {
        return GradualTempoChangeType::LENTANDO;
    } else if (txt == u"morendo") {
        return GradualTempoChangeType::MORENDO;
    } else if (txt == u"precipitando") {
        return GradualTempoChangeType::PRECIPITANDO;
    } else if (txt.contains(u"rall")) {
        return GradualTempoChangeType::RALLENTANDO;
    } else if (txt.contains(u"rit")) {
        return GradualTempoChangeType::RITARDANDO;
    } else if (txt.contains(u"smorz")) {
        return GradualTempoChangeType::SMORZANDO;
    } else if (txt == u"stringendo") {
        return GradualTempoChangeType::STRINGENDO;
    } else {
        return GradualTempoChangeType::UNDEFINED;
    }
}

static void terminate_inferred_line(const std::vector<TextLineBase *> lines, const Fraction &tick, const track_idx_t track,
                                    bool infer_text_type) {
    // Check staff and end any lines which are waiting
    if (infer_text_type) {
        // To avoid extending lines which aren't intended to be terminated by their end markings,
        // only extend lines to markings within 24 quarter notes
        static const Fraction max_inferred_line_len = Fraction(24, 4);
        for (TextLineBase *line : lines) {
            Fraction diff = tick - line->tick();
            if (line && line->staff_idx() == track2staff(track) && line->ticks() == Fraction(0, 1) && diff <= max_inferred_line_len) {
                line->set_track2(track);
                line->set_tick2(tick);
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

void MusicXmlParserDirection::direction(const String &part_id, Measure *measure, const Fraction &tick, MusicXmlSpannerMap &spanners,
                                        DelayedDirectionsList &delayed_directions, InferredFingeringsList &inferred_fingerings,
                                        HarmonyMap &harmony_map, bool &measure_has_coda, SegnoStack &segnos) {
    //LOGD("direction tick %s", muPrintable(tick.print()));

    placement_ = e_.attribute("placement");
    track_ = pass1_.track_for_part(part_id);
    bool is_vocal_staff = pass1_.is_vocal_staff(part_id);
    bool is_percussion_staff = pass1_.is_percussion_staff(part_id);
    bool is_expression_text = false;
    bool delay_ottava = pass1_.exporter_software() == MusicXmlExporterSoftware::SIBELIUS;
    system_direction_ = e_.attribute("system") == "only-top";
    //LOGD("direction track %d", track);
    std::vector<MusicXmlSpannerDesc> starts;
    std::vector<MusicXmlSpannerDesc> stops;
    bool is_dynamic_range = false;
    bool tempo_text_added = false;

    // note: file order is direction-type first, then staff
    // this means staff is still unknown when direction-type is handled
    // easiest solution is to put spanners on a stop and start list
    // and handle these after the while loop

    // note that placement is a <direction> attribute (which is currently supported by MS)
    // but the <direction-type> children also have formatting attributes
    // (currently NOT supported by MS, at least not for spanners when <direction> children)

    while (e_.read_next_start_element()) {
        if (e_.name() == "direction-type") {
            direction_type(starts, stops);
        } else if (e_.name() == "offset") {
            offset_ = pass1_.calc_ticks(e_.read_int(), pass2_.divs(), &e_);
            prevent_negative_tick(tick, offset_, logger_);
        } else if (e_.name() == "sound") {
            sound();
        } else if (e_.name() == "staff") {
            String str_staff = e_.read_text();
            int staff = pass1_.get_music_xml_part(part_id).staff_number_to_index(str_staff.to_int());
            if (staff >= 0) {
                track_ += staff * ::mu::engraving::voices;
            }
        } else {
            skip_log_curr_elem();
        }
    }

    handle_repeats(measure, tick + offset_, measure_has_coda, segnos, delayed_directions);
    handle_nmi_cmi(measure, tick + offset_, delayed_directions);
    handle_fraction();
    handle_chord_sym(tick + offset_, harmony_map);
    handle_drum_instrument(is_percussion_staff, tick + offset_);

    // fix for Sibelius 7.1.3 (direct export) which creates metronomes without <sound tempo="..."/>:
    // if necessary, use the value calculated by metronome()
    // note: no floating point comparisons with 0 ...
    if (tpo_sound_ < 0.1 && tpo_metro_ > 0.1) {
        tpo_sound_ = tpo_metro_;
    }

    //LOGD("words '%s' rehearsal '%s' metro '%s' tpo %g",
    //       muPrintable(_wordsText), muPrintable(_rehearsalText), muPrintable(_metroText), _tpoSound);

    // create text if any text was found
    if (is_lyric_bracket()) {
        return;
    } else if (is_likely_credit(tick)) {
        Text *inferred_text = add_text_to_header(TextStyleType::COMPOSER);
        if (inferred_text) {
            pass1_.set_has_inferred_header_text(true);
            hide_redundant_header_text(inferred_text, {u"lyricist", u"composer", u"poet"});
        }
    } else if (is_likely_subtitle(tick)) {
        Text *inferred_text = add_text_to_header(TextStyleType::SUBTITLE);
        if (inferred_text) {
            pass1_.set_has_inferred_header_text(true);
            if (score_->meta_tag(u"source").is_empty()) {
                score_->set_meta_tag(u"source", inferred_text->plain_text());
            }
            hide_redundant_header_text(inferred_text, {u"source"});
        }
    } else if (is_likely_legally_downloaded(tick)) {
        // Ignore (TBD: print to footer?)
        return;
    } else if (is_likely_tempo_text(track_)) {
        TempoText *tt = Factory::create_tempo_text(score_->dummy());
        tt->set_xml_text(words_text_ + metro_text_);
        if (tpo_sound_ > 0 && can_add_tempo_text(score_, tick.ticks())) {
            double tpo = tpo_sound_ / 60;
            tt->set_tempo(tpo);
            if (tt->plain_text().contains('=')) {
                tt->set_follow_text(true);
            }
        }
        tt->set_visible(visible_);

        pass2_.add_elem_offset(tt, track_, placement(), measure, tick + offset_);
        tempo_text_added = true;
    } else if (is_likely_tempo_line(track_)) {
        String simplified_text = MScoreTextToMusicXml::to_plain_text(words_text_).simplified();
        words_text_.clear();

        GradualTempoChangeType gtc = get_tempo_change_type_from_string(simplified_text);

        GradualTempoChange *tempo_line = Factory::create_gradual_tempo_change(score_->dummy());
        tempo_line->set_tempo_change_type(gtc);
        tempo_line->set_begin_text(simplified_text);
        tempo_line->set_continue_text(u"");
        inferred_tempo_line_start_ = tempo_line;
    } else if (is_likely_sticking()) {
        Sticking *sticking = Factory::create_sticking(score_->dummy());
        sticking->set_xml_text(words_text_);
        if (!real_is_null(relative_x_)) {
            PointF offset = sticking->offset();
            offset.set_x(relative_x_);
            sticking->set_offset(offset);
            sticking->set_property_flags(Pid::OFFSET, PropertyFlags::UNSTYLED);
        }

        // Add element to score later, after collecting all the others and sorting by default-y
        // This allows default-y to be at least respected by the order of elements
        // We also need to check whether the chord at the current tick has a grace note. Directions can appear
        // before the note they apply to, so this may not have been read yet
        MusicXmlDelayedDirectionElement *delayed_direction =
            new MusicXmlDelayedDirectionElement(total_y(), sticking, track_, placement(), measure, tick + offset_);
        delayed_directions.push_back(delayed_direction);
    } else if (is_likely_dynamic_range()) {
        is_dynamic_range = true;
    } else if (!words_text_.empty() || !rehearsal_text_.empty() || !metro_text_.empty()) {
        TextBase *t = 0;
        if (tpo_sound_ > 0.1) {
            if (can_add_tempo_text(score_, tick.ticks())) {
                tpo_sound_ /= 60;
                t = Factory::create_tempo_text(score_->dummy());
                String raw_words_text = words_text_;
                static const std::regex re("(<.*?>)");
                raw_words_text.remove(re);
                String sep = !metro_text_.empty() && !raw_words_text.empty() && raw_words_text.back() != ' ' ? u" " : String();
                t->set_xml_text(words_text_ + sep + metro_text_);
                ((TempoText *) t)->set_tempo(tpo_sound_);
                if (t->plain_text().contains('=')) {
                    ((TempoText *) t)->set_follow_text(true);
                }
                tempo_text_added = true;
            }
        } else {
            if (!words_text_.empty() || !metro_text_.empty()) {
                PlayingTechniqueType technique = PlayingTechniqueType::UNDEFINED;
                if (!play_.empty()) {
                    technique = TConv::from_xml(play_.to_ascii().const_char(), PlayingTechniqueType::UNDEFINED);
                    play_.clear();
                } else {
                    technique = get_playing_technique();
                }
                is_expression_text = words_text_.contains(u"<i>") && metro_text_.empty() && placement() == u"below";
                if (is_expression_text) {
                    t = Factory::create_expression(score_->dummy());
                } else if (system_direction_) {
                    t = Factory::create_system_text(score_->dummy());
                } else if (technique != PlayingTechniqueType::UNDEFINED) {
                    t = Factory::create_play_tech_annotation(score_->dummy(), technique, TextStyleType::STAFF);
                } else {
                    t = Factory::create_staff_text(score_->dummy());
                }
                t->set_xml_text(words_text_ + metro_text_);
            } else {
                t = Factory::create_rehearsal_mark(score_->dummy());
                if (!rehearsal_text_.contains(u"<b>")) {
                    rehearsal_text_ = u"<b></b>" + rehearsal_text_; // explicitly turn bold off
                }
                t->set_xml_text(rehearsal_text_);
                if (!has_default_y_) {
                    t->set_placement(PlacementV::ABOVE); // crude way to force placement TODO improve ?
                    t->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
                    t->reset_property(Pid::OFFSET);
                }
            }
        }

        if (t) {
            if (enclosure_ == "circle") {
                t->set_frame_type(FrameType::CIRCLE);
            } else if (enclosure_ == "none") {
                t->set_frame_type(FrameType::NO_FRAME);
            } else if (enclosure_ == "rectangle") {
                t->set_frame_type(FrameType::RECTANGLE);
                t->set_frame_round(0_sp);
            }

            color_item(t, color_);

            if (configuration()->import_layout()) {
                if (justify_ == u"right") {
                    t->set_align(AlignH::RIGHT);
                } else if (justify_ == u"center") {
                    t->set_align(AlignH::HCENTER);
                } else {
                    t->set_align(AlignH::LEFT);
                }
            }

            t->set_visible(visible_);

            if (swing_.second != 0) {
                to_staff_text_base(t)->set_swing(true);
                to_staff_text_base(t)->set_swing_parameters(
                    swing_.first, swing_.first ? swing_.second : to_staff_text_base(t)->style().style_i(Sid::SWING_RATIO));
                swing_.second = 0;
            }

            String words_placement = placement_;
            // Case-based defaults
            if (words_placement.empty()) {
                if (is_vocal_staff) {
                    words_placement = u"above";
                } else if (is_expression_text) {
                    words_placement = u"below";
                }
            }

            String fingering_str = words_text_;
            static const std::regex xml_formating("(<.*?>)");
            fingering_str.remove(xml_formating).remove(u'\u00A0');
            if (is_likely_fingering(fingering_str)) {
                logger_->log_debug_info(String(u"Inferring fingering: %1").arg(fingering_str));
                t->set_xml_text(fingering_str);
                MusicXmlInferredFingering *inferred_fingering = new MusicXmlInferredFingering(
                    total_y(), t, fingering_str, track_, placement(), measure, tick + offset_, configuration()->need_use_default_font());
                inferred_fingerings.push_back(inferred_fingering);
            } else {
                if (has_total_y()) {
                    // Add element to score later, after collecting all the others and sorting by default-y
                    // This allows default-y to be at least respected by the order of elements
                    MusicXmlDelayedDirectionElement *delayed_direction =
                        new MusicXmlDelayedDirectionElement(total_y(), t, track_, placement(), measure, tick + offset_);
                    delayed_directions.push_back(delayed_direction);
                } else {
                    pass2_.add_elem_offset(t, track_, placement(), measure, tick + offset_);
                }
            }
        }
    } else if (tpo_sound_ > 0) {
        // direction without text but with sound tempo="..."
        // create an invisible default TempoText

        if (can_add_tempo_text(score_, tick.ticks())) {
            double tpo = tpo_sound_ / 60;
            TempoText *t = Factory::create_tempo_text(score_->dummy());
            t->set_xml_text(
                String(u"%1 = %2").arg(TempoText::duration2tempo_text_string(TDuration(DurationType::V_QUARTER))).arg(tpo_sound_));
            t->set_visible(false);
            t->set_tempo(tpo);
            t->set_follow_text(true);

            pass2_.add_elem_offset(t, track_, placement(), measure, tick + offset_);
            tempo_text_added = true;
        }
    }

    if (tempo_text_added) {
        const InferredTempoLineStack &lines = pass2_.get_inferred_tempo_line();
        terminate_inferred_line(std::vector<TextLineBase *>(lines.begin(), lines.end()), tick + offset_, track_,
                                configuration()->infer_text_type());
    }
    add_inferred_tempo_line(tick + offset_);

    Dynamic *first_dyn = nullptr;

    // do dynamics
    for (auto it = dynamics_list_.begin(); it != dynamics_list_.end(); ++it) {
        Dynamic *dynamic = create_imported_dynamic(score_, *it);
        // Preserve the existing direction visibility policy.
        dynamic->set_visible(visible_);

        if (is_dynamic_range) {
            if (it == dynamics_list_.begin()) {
                first_dyn = dynamic;
            } else if (it == dynamics_list_.end() - 1 && first_dyn) {
                // append hyphen and this dynamic to first
                first_dyn->set_xml_text(first_dyn->xml_text() + u"<sym>dynamicCombinedSeparatorHyphen</sym>" + dynamic->xml_text());
                delete dynamic;
                continue;
            }
        }

        if (!dyna_velocity_.empty()) {
            dynamic->set_velocity(music_xml_dynamic_velocity(dyna_velocity_, 0, 0));
        }

        String dynamics_placement = it->placement;
        // Case-based defaults
        if (dynamics_placement.empty()) {
            dynamics_placement = is_vocal_staff ? u"above" : u"below";
        }

        const InferredHairpinsStack &hairpins = pass2_.get_inferred_hairpins();
        terminate_inferred_line(std::vector<TextLineBase *>(hairpins.begin(), hairpins.end()), tick + offset_, track_,
                                configuration()->infer_text_type());

        // Add element to score later, after collecting all the others and sorting by default-y
        // This allows default-y to be at least respected by the order of elements
        MusicXmlDelayedDirectionElement *delayed_direction = new MusicXmlDelayedDirectionElement(
            it->has_total_y ? it->total_y : 100, dynamic, track_, dynamics_placement, measure, tick + offset_);
        delayed_directions.push_back(delayed_direction);
    }

    add_inferred_hairpin(tick + offset_, is_vocal_staff);

    // handle the elems
    for (EngravingItem *elem : elems_) {
        // TODO (?) if (_hasDefaultY) elem->setYoff(_defaultY);
        if (has_total_y()) {
            MusicXmlDelayedDirectionElement *delayed_direction =
                new MusicXmlDelayedDirectionElement(total_y(), elem, track_, placement(), measure, tick + offset_);
            delayed_directions.push_back(delayed_direction);
        } else {
            pass2_.add_elem_offset(elem, track_, placement(), measure, tick + offset_);
        }
    }

    // handle the spanner stops first
    for (MusicXmlSpannerDesc desc : stops) {
        MusicXmlExtendedSpannerDesc &spdesc = pass2_.get_spanner({desc.tp, desc.nr});
        if (spdesc.is_stopped) {
            logger_->log_error(u"spanner already stopped", &e_);
            delete desc.sp;
        } else {
            if (spdesc.is_started) {
                // Adjustments to ottavas by the offset value are unwanted
                const Fraction sp_tick = spdesc.sp && spdesc.sp->is_ottava() ? tick : tick + offset_;
                if (spdesc.sp && spdesc.sp->is_ottava() && delay_ottava) {
                    // Sibelius writes ottava ends 1 note too early
                    pass2_.set_delayed_ottava(spdesc.sp);
                    pass2_.delayed_ottava()->set_track2(track_);
                    pass2_.delayed_ottava()->set_tick2(sp_tick);
                    // need to set tick again later
                    pass2_.clear_spanner(desc);
                } else {
                    handle_spanner_stop(spdesc.sp, track_, sp_tick, spanners);
                    pass2_.clear_spanner(desc);
                }
            } else {
                spdesc.sp = desc.sp;
                const Fraction sp_tick = spdesc.sp && spdesc.sp->is_ottava() ? tick : tick + offset_;
                spdesc.tick2 = sp_tick;
                spdesc.track2 = track_;
                spdesc.is_stopped = true;
            }
        }
    }

    // then handle the spanner starts
    // TBD handle offset ?
    for (MusicXmlSpannerDesc desc : starts) {
        MusicXmlExtendedSpannerDesc &spdesc = pass2_.get_spanner({desc.tp, desc.nr});
        if (spdesc.is_started) {
            logger_->log_error(u"spanner already started", &e_);
            delete desc.sp;
        } else {
            String spanner_placement = placement();
            // Case-based defaults
            if (spanner_placement.empty()) {
                if (desc.sp->is_hairpin()) {
                    spanner_placement = is_vocal_staff ? u"above" : u"below";
                } else {
                    spanner_placement = total_y() < 0 ? u"above" : u"below";
                }
            }
            desc.sp->set_visible(visible_);
            if (spdesc.is_stopped) {
                pass2_.add_spanner(desc);
                // handleSpannerStart and handleSpannerStop must be called in order
                // due to allocation of elements in the map
                handle_spanner_start(desc.sp, track_, spanner_placement, tick + offset_, spanners);
                handle_spanner_stop(spdesc.sp, spdesc.track2, spdesc.tick2, spanners);
                pass2_.clear_spanner(desc);
            } else {
                pass2_.add_spanner(desc);
                handle_spanner_start(desc.sp, track_, spanner_placement, tick + offset_, spanners);
                spdesc.is_started = true;
            }
        }
    }
}

//---------------------------------------------------------
//   isLikelyCredit
//---------------------------------------------------------

bool MusicXmlParserDirection::is_likely_credit(const Fraction &tick) const {
    if (!configuration()->infer_text_type()) {
        return false;
    }
    return (tick + offset_ < Fraction(5, 1)) // Only early in the piece
           && rehearsal_text_.empty() && metro_text_.empty() && tpo_sound_ < 0.1 && is_likely_credit_text(words_text_, false);
}

//---------------------------------------------------------
//   isLyricBracket
//    Dolet exports lyric brackets as staff text,
//    which we ought not render.
//---------------------------------------------------------

bool MusicXmlParserDirection::is_lyric_bracket() const {
    if (words_text_.empty()) {
        return false;
    }

    if (!(words_text_.front() == u'}' || words_text_.back() == u'{')) {
        return false;
    }

    return rehearsal_text_.empty() && metro_text_.empty() && dynamics_list_.empty() && tpo_sound_ < 0.1;
}

bool MusicXmlParserDirection::is_likely_subtitle(const Fraction &tick) const {
    if (!configuration()->infer_text_type()) {
        return false;
    }
    return (tick + offset_ < Fraction(5, 1)) // Only early in the piece
           && rehearsal_text_.empty() && metro_text_.empty() && tpo_sound_ < 0.1 && is_likely_subtitle_text(words_text_, false);
}

bool MusicXmlParserDirection::is_likely_legally_downloaded(const Fraction &tick) const {
    if (!configuration()->infer_text_type()) {
        return false;
    }
    return (tick + offset_ < Fraction(5, 1)) // Only early in the piece
           && rehearsal_text_.empty() && metro_text_.empty() && tpo_sound_ < 0.1 &&
           words_text_.contains(std::wregex(L"This music has been legally downloaded\\.(?:\\s|\\u00A0)Do not photocopy\\."));
}

bool MusicXmlParserDirection::is_likely_tempo_text(const track_idx_t track) const {
    if (!configuration()->infer_text_type() || words_text_.contains(u"<i>") || words_text_.contains(u"“") || words_text_.contains(u"”") ||
        placement() == u"below" || track2staff(track) || words_text_.empty() || swing_.second) {
        return false;
    }

    const String plain_text = MScoreTextToMusicXml::to_plain_text(words_text_.simplified());
    static const std::array<String, 29> tempo_strs = {
        u"adag",  u"alleg",  u"andant", u"a tempo",  u"ballad", u"brisk",        u"determination", u"dolce",      u"expressive", u"fast",
        u"free",  u"gently", u"grave",  u"larg",     u"lento",  u"stesso tempo", u"lively",        u"maestoso",   u"moderat",    u"mosso",
        u"prest", u"rubato", u"slow",   u"straight", u"tango",  u"tempo i",      u"tenderly",      u"triumphant", u"vivace"};

    for (const String &str : tempo_strs) {
        if (plain_text.contains(str, CaseSensitivity::CASE_INSENSITIVE)) {
            return true;
        }
    }
    return false;
}

void MusicXmlParserDirection::handle_fraction() {
    if (!configuration()->infer_text_type()) {
        return;
    }

    String raw_words_text = words_text_;
    static const std::regex re("(<.*?>)");
    raw_words_text.remove(re);
    raw_words_text = raw_words_text.simplified();
    //                         UTF-16 encoding: 0x00BC, 0x00BD, 0x00BE, 0x2150, 0x2151, 0x2152,  etc...
    static const std::array<String, 18> fracs{u"1/4", u"1/2", u"3/4", u"1/7", u"1/9", u"1/10", u"1/3", u"2/3", u"1/5",
                                              u"2/5", u"3/5", u"4/5", u"1/6", u"5/6", u"1/8",  u"3/8", u"5/8", u"7/8"};

    for (size_t n = 0; n < fracs.size(); n++) {
        if (raw_words_text.contains(fracs.at(n))) {
            size_t p = n <= 2 ? 0x00BC + n : 0x2150 + n - 3;
            raw_words_text.replace(fracs.at(n), String(char16_t(p)));
            words_text_ = raw_words_text;
            return;
        }
    }
}

bool MusicXmlParserDirection::is_likely_tempo_line(const track_idx_t track) const {
    if (!configuration()->infer_text_type() || words_text_.contains(u"“") || words_text_.contains(u"”") || placement() == u"below" ||
        track2staff(track) != 0 || words_text_.empty()) {
        return false;
    }

    const String plain_text = MScoreTextToMusicXml::to_plain_text(words_text_.simplified());
    static const std::wregex re(L"(^|(?:\\s|\u00A0))(accel|faster|mosso|rit|rubato|slower|stretto)", std::regex_constants::icase);

    return plain_text.contains(re);
}

Text *MusicXmlParserDirection::add_text_to_header(const TextStyleType text_style_type) {
    Text *t = Factory::create_text(score_->dummy(), text_style_type);
    t->set_xml_text(words_text_.trimmed());
    t->renderer()->layout_text1(t);
    MeasureBase *const first_measure = score_->measures()->first();
    VBox *vbox = first_measure->is_v_box() ? to_v_box(first_measure) :
                                             MusicXmlParserPass1::create_and_add_v_box_for_credit_words(score_, Fraction(0, 1));
    double spatium = score_->style().style_d(Sid::SPATIUM);
    vbox->set_box_height(vbox->box_height() + Spatium(t->height() / spatium / 2)); // add some height
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

void MusicXmlParserDirection::hide_redundant_header_text(const Text *inferred_text, const std::vector<String> meta_tags) {
    if (!inferred_text->parent()->is_v_box()) {
        return;
    }

    for (EngravingItem *e : to_v_box(inferred_text->parent())->el()) {
        if (e == inferred_text || !e->is_text()) {
            continue;
        }

        Text *t = to_text(e);
        for (const String &meta_tag : meta_tags) {
            if (t->plain_text() == score_->meta_tag(meta_tag)) {
                t->set_visible(false);
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

void MusicXmlParserDirection::direction_type(std::vector<MusicXmlSpannerDesc> &starts, std::vector<MusicXmlSpannerDesc> &stops) {
    while (e_.read_next_start_element()) {
        default_y_ = e_.ascii_attribute("default-y").to_double(&has_default_y_) * -0.1;
        relative_x_ = e_.double_attribute("relative-x") / 10 * score_->style().spatium();
        visible_ = e_.ascii_attribute("print-object") != "no"; // only available for "metronome" and "other-direction"
        const String number = e_.attribute("number");
        int n = 0;
        if (!number.empty()) {
            n = number.to_int();
            if (n <= 0) {
                logger_->log_error(String(u"invalid number %1").arg(number), &e_);
            } else {
                n--; // make zero-based
            }
        }
        const String type = e_.attribute("type");
        color_ = Color::from_string(e_.ascii_attribute("color").ascii());
        justify_ = e_.attribute("justify");
        if (e_.name() == "metronome") {
            metro_text_ = metronome(tpo_metro_);
        } else if (e_.name() == "words") {
            enclosure_ = e_.attribute("enclosure");
            font_family_ = e_.attribute("font-family");
            String next_part = xmlpass2::next_part_of_formatted_string(e_, *configuration(), *score_->engraving_fonts(), pass1_.dolet());

            text_to_dynamic(next_part);
            text_to_cresc_line(next_part);
            handle_tempo(next_part);
            words_text_ += next_part;
        } else if (e_.name() == "rehearsal") {
            enclosure_ = e_.attribute("enclosure");
            if (enclosure_.empty()) {
                enclosure_ = u"square"; // note different default
            }
            rehearsal_text_ += xmlpass2::next_part_of_formatted_string(e_, *configuration(), *score_->engraving_fonts(), pass1_.dolet());
        } else if (e_.name() == "harp-pedals") {
            harp_pedal();
        } else if (e_.name() == "pedal") {
            pedal(type, n, starts, stops);
        } else if (e_.name() == "octave-shift") {
            octave_shift(type, n, starts, stops);
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
                words_text_ += u"<sym>" + smufl + u"</sym>";
            } else {
                words_text_ += u"<sym>coda</sym>";
            }
            e_.skip_current_element();
        } else if (e_.name() == "segno") {
            const String smufl = e_.attribute("smufl");
            if (!smufl.empty()) {
                words_text_ += u"<sym>" + smufl + u"</sym>";
            } else {
                words_text_ += u"<sym>segno</sym>";
            }
            e_.skip_current_element();
        } else if (e_.name() == "eyeglasses") {
            words_text_ += u"<sym>miscEyeglasses</sym>";
            e_.skip_current_element();
        } else if (e_.name() == "symbol") {
            const String smufl = e_.read_text();
            if (!smufl.empty()) {
                words_text_ += u"<sym>" + smufl + u"</sym>";
            }
        } else if (e_.name() == "string-mute") {
            if (type == u"on") {
                words_text_ += u"<sym>stringsMuteOn</sym>";
            } else if (type == u"off") {
                words_text_ += u"<sym>stringsMuteOff</sym>";
            }
            e_.skip_current_element();
        } else if (e_.name() == "other-direction") {
            other_direction();
        } else {
            skip_log_curr_elem();
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
    snd_coda_ = e_.attribute("coda");
    snd_dacapo_ = e_.attribute("dacapo");
    snd_dalsegno_ = e_.attribute("dalsegno");
    snd_fine_ = e_.attribute("fine");
    snd_segno_ = e_.attribute("segno");
    snd_to_coda_ = e_.attribute("tocoda");
    tpo_sound_ = e_.double_attribute("tempo");
    dyna_velocity_ = e_.attribute("dynamics");

    const String pizz = e_.attribute("pizzicato");
    if (pizz == u"yes") {
        play_ = u"pizzicato";
    } else if (pizz == u"no") {
        play_ = u"natural";
    }

    while (e_.read_next_start_element()) {
        if (e_.name() == "play") {
            play();
        } else if (e_.name() == "swing") {
            swing();
        } else {
            skip_log_curr_elem();
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
    while (e_.read_next_start_element()) {
        if (e_.name() == "mute") {
            const String muted = e_.read_text();
            play_ = (muted == u"off") ? u"open" : u"mute";
        } else if (e_.name() == "other-play") {
            play_ = e_.attribute("type");
            e_.skip_current_element();
        } else {
            skip_log_curr_elem();
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
    int swing_numerator = 1;
    int swing_denominator = 1;
    int swing_unit = 0;
    while (e_.read_next_start_element()) {
        if (e_.name() == "straight") {
            // unused
            e_.skip_current_element();
        } else if (e_.name() == "first") {
            swing_denominator = e_.read_text().to_int();
        } else if (e_.name() == "second") {
            swing_numerator = e_.read_text().to_int();
        } else if (e_.name() == "swing-type") {
            const String swing_type = e_.read_text();
            if (swing_type == u"eighth") {
                swing_unit = Constants::division / 2;
            } else if (swing_type == u"16th") {
                swing_unit = Constants::division / 4;
            }
        } else if (e_.name() == "swing-style") {
            // unused
            e_.skip_current_element();
        } else {
            skip_log_curr_elem();
        }
    }
    swing_.first = swing_unit;
    swing_.second = (swing_numerator * 100) / swing_denominator;
}

//---------------------------------------------------------
//   dynamics
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/dynamics node.
 */

void MusicXmlParserDirection::dynamics() {
    MusicXmlDynamic dynamic = parse_dynamics_container(e_, logger_);
    // MusicXML dynamics placement is ignored in directions; the parent wins.
    dynamic.placement = placement();
    dynamic.has_total_y = has_total_y();
    dynamic.total_y = total_y();
    if (!dynamic.text.empty()) {
        dynamics_list_.push_back(dynamic);
    }
}

//---------------------------------------------------------
//   harpPedal
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/hair-pedal node.
 */

void MusicXmlParserDirection::harp_pedal() {
    const std::vector<String> pedal_steps = {u"D", u"C", u"B", u"E", u"F", u"G", u"A"};
    const Color color = Color::from_string(e_.attribute("color"));

    HarpPedalDiagram *hpd = Factory::create_harp_pedal_diagram(score_->dummy());
    while (e_.read_next_start_element()) {
        int step_index = 0;
        PedalPosition pedpos = PedalPosition::UNSET;
        while (e_.read_next_start_element()) {
            if (e_.name() == "pedal-step") {
                step_index =
                    std::distance(std::begin(pedal_steps), std::find(std::begin(pedal_steps), std::end(pedal_steps), e_.read_text()));
            } else if (e_.name() == "pedal-alter") {
                pedpos = static_cast<PedalPosition>(e_.read_text().trimmed().to_int() + 1);
            } else {
                skip_log_curr_elem();
            }
        }
        hpd->set_pedal(static_cast<HarpStringType>(step_index), pedpos);
        color_item(hpd, color);
    }
    elems_.push_back(hpd);
}

void MusicXmlParserDirection::other_direction() {
    // <other-direction> element is used to define any <direction> symbols not yet in the MusicXML format
    const String smufl = e_.attribute("smufl");
    const Color color = Color::from_string(e_.attribute("color"));

    // Read smufl symbol
    if (!smufl.empty()) {
        SymId id = SymNames::sym_id_by_name(smufl, SymId::NO_SYM);
        if (id != SymId::NO_SYM) {
            Symbol *smufl_sym = Factory::create_symbol(score_->dummy());
            smufl_sym->set_sym(id);
            color_item(smufl_sym, color);
            elems_.push_back(smufl_sym);
        }
        e_.skip_current_element();
    } else {
        // TODO: Multiple sets of maps for exporters other than Dolet 6/Sibelius
        // TODO: Add more symbols from Sibelius
        std::map<String, String> other_direction_strings;
        if (pass1_.dolet()) {
            other_direction_strings = {
                {String(u"To Coda"), String(u"To Coda")},
                {String(u"Segno"), String(u"<sym>segno</sym>")},
                {String(u"CODA"), String(u"CODA")},
            };
        }
        static const std::map<String, SymId> other_direction_syms = {{String(u"Rhythm dot"), SymId::AUGMENTATION_DOT},
                                                                     {String(u"Whole rest"), SymId::REST_WHOLE},
                                                                     {String(u"l.v. down"), SymId::ARTIC_LAISSEZ_VIBRER_BELOW},
                                                                     {String(u"8vb"), SymId::OTTAVA_BASSA_VB},
                                                                     {String(u"Treble clef"), SymId::G_CLEF},
                                                                     {String(u"Bass clef"), SymId::F_CLEF},
                                                                     {String(u"Caesura"), SymId::CAESURA},
                                                                     {String(u"Thick caesura"), SymId::CAESURA_THICK}};
        String t = e_.read_text();
        String val = muse::value(other_direction_strings, t);
        if (!val.empty()) {
            words_text_ += val;
        } else {
            SymId sym = muse::value(other_direction_syms, t);
            Symbol *smufl_sym = Factory::create_symbol(score_->dummy());
            smufl_sym->set_sym(sym);
            color_item(smufl_sym, color);
            elems_.push_back(smufl_sym);
        }
    }
}

static const String get_marker_id(const String &words) {
    // Helper function to set the marker ID string
    // eg. extract 'II' from 'To Coda II'
    static const std::regex number("(i{0,3})");
    const StringList nums = words.search(number, {1}, SplitBehavior::SKIP_EMPTY_PARTS);
    if (!nums.empty()) {
        return nums.at(0);
    }
    return String();
}

String MusicXmlParserDirection::match_repeat(const String &plain_words) const {
    static const std::wregex da_capo(L"^(d\\.? ?|da )(c\\.?|capo)$");
    static const std::wregex da_capo_al_fine(L"^(d\\.? ?|da )(c\\.? ?|capo )al fine$");
    static const std::wregex da_capo_al_coda(L"^(d\\.? ?|da )(c\\.? ?|capo )al coda ?(i{0,3})?$");
    static const std::wregex dal_segno(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno) ?(i{0,3})?$");
    static const std::wregex dal_segno_al_fine(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})? ?(\\(.*\\))? al fine$");
    static const std::wregex dal_segno_al_coda(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})? ?(\\(.*\\))? al coda ?(i{0,3})?$");
    static const std::wregex fine(L"^fine$");
    static const std::wregex segno(L"^segno( segno)? ?(i{0,3})?$");
    static const std::wregex to_coda(L"^to coda( coda)? ?(i{0,3})?$");
    static const std::wregex coda(L"^coda( coda)? ?(i{0,3})?$");

    if (plain_words.contains(da_capo)) {
        return u"daCapo";
    }
    if (plain_words.contains(da_capo_al_fine)) {
        return u"daCapoAlFine";
    }
    if (plain_words.contains(da_capo_al_coda)) {
        return u"daCapoAlCoda";
    }
    if (plain_words.contains(dal_segno)) {
        return u"dalSegno";
    }
    if (plain_words.contains(dal_segno_al_fine)) {
        return u"dalSegnoAlFine";
    }
    if (plain_words.contains(dal_segno_al_coda)) {
        return u"dalSegnoAlCoda";
    }
    if (plain_words.contains(segno)) {
        return u"segno";
    }
    if (plain_words.contains(fine)) {
        return u"fine";
    }
    if (plain_words.contains(to_coda)) {
        return u"toCoda";
    }
    if (plain_words.contains(coda)) {
        return u"coda";
    }
    return String();
}

static void add_symbols_to_coda(String &words_string, const String &coda_id) {
    // Append correct number of symbols to coda text
    if (!words_string.contains(u"<sym>coda</sym>")) {
        String end = u"\n";
        if (coda_id.size() > 0) {
            end.append(coda_id);
            end.replace(u"i", u"<sym>coda</sym>");
        } else {
            end.append(u"<sym>coda</sym>");
        }
        words_string.append(end);
    }
}

//---------------------------------------------------------
//   isLikelyFingering
//---------------------------------------------------------

bool MusicXmlParserDirection::is_likely_fingering(const String &fingering_str) const {
    if (!configuration()->infer_text_type()) {
        return false;
    }

    // One or more newline-separated digits, possibly lead or trailed by whitespace
    static const std::wregex re(L"^(?:\\s|\\u00A0)*[-–]?(?:\\s|\\u00A0)?[0-5pimac](?:[-–][0-5pimac])?(?:\n[-–]?(?:\\s|\\u00A0)?[0-5pimac](?"
                                L":[-–][0-5pimac])?)*(?:\\s|\\u00A0)*$");
    return fingering_str.contains(re) && rehearsal_text_.empty() && metro_text_.empty() && tpo_sound_ < 0.1;
}

//---------------------------------------------------------
//   MusicXmlInferredFingering
//---------------------------------------------------------

MusicXmlInferredFingering::MusicXmlInferredFingering(double total_y, EngravingItem *element, String &text, track_idx_t track,
                                                     String placement, Measure *measure, Fraction tick, bool need_use_default_font)
    : total_y_(total_y), element_(element), text_(text), track_(track), placement_(placement), measure_(measure), tick_(tick),
      need_use_default_font_(need_use_default_font) {
    fingerings_ = text_.simplified().split(u" ");
}

//---------------------------------------------------------
//   roundTick
//---------------------------------------------------------
/**
 Round tick to multiple of gcd of measure
 */

void MusicXmlInferredFingering::round_tick(Measure *measure) {
    measure->compute_ticks();
    int gcd_ticks = Fraction(1, 1).ticks();
    for (auto &s : measure->segments()) {
        if (s.is_chord_rest_type()) {
            gcd_ticks = std::gcd(gcd_ticks, s.ticks().ticks());
        }
    }
    if (!gcd_ticks || gcd_ticks == Fraction(1, 1).ticks() || !(tick_.ticks() % gcd_ticks)) {
        return;
    }
    int rounded_tick = std::round(static_cast<double>(tick_.ticks()) / static_cast<double>(gcd_ticks)) * (gcd_ticks);
    tick_ = Fraction::from_ticks(rounded_tick);
}

//---------------------------------------------------------
//   findAndAddToNotes
//---------------------------------------------------------
/**
 Attempts to find an eligible collection of notes to add inferred
 fingerings to. Adds notes and returns true if successful, else no-op
 and returns false.
 */
bool MusicXmlInferredFingering::find_and_add_to_notes(Measure *measure) {
    round_tick(measure);
    std::vector<Note *> collected_notes;
    for (track_idx_t track = track_; track < track_ + 4; ++track) {
        Chord *candidate_chord = measure->find_chord(tick(), track);
        if (candidate_chord) {
            if (candidate_chord->notes().size() >= fingerings().size()) {
                add_to_notes(candidate_chord->notes());
                return true;
            } else {
                collected_notes.insert(collected_notes.begin(), candidate_chord->notes().begin(), candidate_chord->notes().end());
                if (collected_notes.size() >= fingerings().size()) {
                    add_to_notes(collected_notes);
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
void MusicXmlInferredFingering::add_to_notes(std::vector<Note *> &notes) const {
    assert(notes.size() >= fingerings_.size());
    for (size_t i = 0; i < fingerings_.size(); ++i) {
        // Fingerings in reverse order
        add_text_to_note(-1, fingerings_[fingerings_.size() - 1 - i], placement_, u"", -1, u"", u"", true, Color::black,
                         TextStyleType::FINGERING, notes[i]->score(), notes[i], need_use_default_font_);
    }
}

//---------------------------------------------------------
//   toDelayedDirection
//---------------------------------------------------------

MusicXmlDelayedDirectionElement *MusicXmlInferredFingering::to_delayed_direction() {
    MusicXmlDelayedDirectionElement *dd = new MusicXmlDelayedDirectionElement(total_y_, element_, track_, placement_, measure_, tick_);
    return dd;
}

//---------------------------------------------------------
//   textToDynamic
//---------------------------------------------------------
/**
 Attempts to convert text to dynamic text. No-op if unable.
 */
void MusicXmlParserDirection::text_to_dynamic(String &text) {
    if (!configuration()->infer_text_type()) {
        return;
    }
    String simplified_text = MScoreTextToMusicXml::to_plain_text(text).simplified();
    // Correct finale's incorrect dynamic export
    if (pass1_.exporter_software() == MusicXmlExporterSoftware::FINALE) {
        static const std::map<String, String> finale_dynamic_subs = {{u"π", u"pp"},  {u"P", u"mp"},  {u"F", u"mf"},
                                                                     {u"ƒ", u"ff"},  {u"Ï", u"fff"}, {u"S", u"sf"},
                                                                     {u"ß", u"sfz"}, {u"Z", u"fz"},  {u"Í", u"fp"}};
        for (const auto &sub : finale_dynamic_subs) {
            if (simplified_text == sub.first) {
                simplified_text = sub.second;
            }
        }
    }
    // We don't want to count a single 'm', 'r', 's' or 'z' as a whole dynamic
    static const std::wregex single_char_dynamic = std::wregex(L"^[mrsz]$");
    // try to find a dynamic - xml representation or
    // if found add to dynamics list and set text to blank string
    if (!simplified_text.contains(single_char_dynamic) && TConv::dynamic_valid(simplified_text.to_std_string())) {
        DynamicType dt = TConv::from_xml(simplified_text.to_std_string(), DynamicType::OTHER);
        if (dt != DynamicType::OTHER) {
            dyna_velocity_ = String::number(round(Dynamic::dynamic_velocity(dt) / 0.9));
            MusicXmlDynamic dynamic;
            dynamic.text = Dynamic::dynamic_text(dt);
            dynamic.placement = placement();
            dynamic.enclosure = enclosure_;
            dynamic.has_total_y = has_total_y();
            dynamic.total_y = total_y();
            dynamics_list_.push_back(dynamic);
            text.clear();
        }
    }
}

void MusicXmlParserDirection::text_to_cresc_line(String &text) {
    if (!configuration()->infer_text_type()) {
        return;
    }
    String simplified_text = MScoreTextToMusicXml::to_plain_text(text).simplified();
    bool cresc = simplified_text.contains(u"cresc");
    bool dim = simplified_text.contains(u"dim");
    if (!cresc && !dim) {
        return;
    }

    // Create line
    text.clear();
    Hairpin *line = Factory::create_hairpin(score_->dummy());

    line->set_hairpin_type(cresc ? HairpinType::CRESC_LINE : HairpinType::DIM_LINE);
    line->set_begin_text(simplified_text);
    line->set_continue_text(u"");
    line->set_property(Pid::LINE_VISIBLE, false);
    inferred_hairpin_start_ = line;
}

void MusicXmlParserDirection::add_inferred_hairpin(const Fraction &tick, const bool is_vocal_staff) {
    if (!configuration()->infer_text_type() || !inferred_hairpin_start_) {
        return;
    }

    inferred_hairpin_start_->set_track(track_);
    inferred_hairpin_start_->set_tick(tick + offset_);

    String spanner_placement = placement_;
    if (placement_.empty()) {
        spanner_placement = is_vocal_staff ? u"above" : u"below";
    }
    set_s_line_placement(inferred_hairpin_start_, spanner_placement);

    pass2_.add_inferred_hairpin(inferred_hairpin_start_);
}

void MusicXmlParserDirection::add_inferred_tempo_line(const Fraction &tick) {
    if (!configuration()->infer_text_type() || !inferred_tempo_line_start_) {
        return;
    }

    inferred_tempo_line_start_->set_track(track_);
    inferred_tempo_line_start_->set_tick(tick + offset_);

    String spanner_placement = placement();

    set_s_line_placement(inferred_tempo_line_start_, spanner_placement);

    pass2_.add_inferred_tempo_line(inferred_tempo_line_start_);
}

static String find_detached_repeat_number(const Measure *measure, const track_idx_t track, const Fraction tick, const String &placement,
                                          DelayedDirectionsList &directions) {
    static const std::wregex number(L"^(i{1,3})$");
    DelayedDirectionsList::iterator it;

    for (it = directions.begin(); it != directions.end(); ++it) {
        MusicXmlDelayedDirectionElement d = **it;
        const EngravingItem *el = d.element();
        if (d.track() == track && d.tick() == tick && d.placement() == placement && el->is_text_base()) {
            const String text = to_text_base(el)->plain_text();
            if (text.to_lower().contains(number)) {
                it = directions.erase(it);
                return u" " + text;
            }
        }
    }

    Segment *s = measure->find_segment(SegmentType::CHORD_REST, tick);
    if (s) {
        for (EngravingItem *el : s->annotations()) {
            if (el->tick() == tick && el->track() == track && el->place_above() == (placement == u"above") && el->is_text_base()) {
                const String text = to_text_base(el)->plain_text();
                if (text.to_lower().contains(number)) {
                    s->remove_annotation(el);
                    return u" " + text;
                }
            }
        }
    }

    return String();
}

static String count_segno(const String &plain_words) {
    // Count how many 's' in D.S. or D.S.S...
    std::wsmatch matches;
    const std::wstring str = plain_words.to_std_w_string();
    static const std::wregex dal_segno_al_coda(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})?(\\(.*\\))? al coda ?(i{0,3})?$");
    if (std::regex_search(str, matches, dal_segno_al_coda)) {
        const String segno_group = String::from_std_w_string(matches.str(2)).to_lower();
        const int count = segno_group.count('s');
        return String::from_std_string(std::string(count, 'i'));
    }
    return String();
}

//---------------------------------------------------------
//   handleRepeats
//---------------------------------------------------------

void MusicXmlParserDirection::handle_repeats(Measure *measure, const Fraction tick, bool &measure_has_coda, SegnoStack &segnos,
                                             DelayedDirectionsList &delayed_directions) {
    // Try to recognize the various repeats
    // The explicit repeat attributes of the <sound> element are always honoured;
    // only the purely text-based fallback depends on the inferTextType preference
    String repeat;
    const String plain_words = MScoreTextToMusicXml::to_plain_text(words_text_.to_lower().simplified());
    const String words_repeat = match_repeat(plain_words);
    if (!snd_coda_.empty()) {
        repeat = u"coda";
    } else if (!snd_dacapo_.empty()) {
        // the accompanying words may refine the jump type (e.g. "D.C. al Fine")
        repeat = words_repeat.starts_with(u"daCapo") ? words_repeat : u"daCapo";
    } else if (!snd_dalsegno_.empty()) {
        repeat = words_repeat.starts_with(u"dalSegno") ? words_repeat : u"dalSegno";
    } else if (!snd_fine_.empty()) {
        repeat = u"fine";
    } else if (!snd_segno_.empty()) {
        repeat = u"segno";
    } else if (!snd_to_coda_.empty()) {
        repeat = u"toCoda";
    } else if (configuration()->infer_text_type()) {
        repeat = words_repeat;
    }
    // Check if repeat number has become detached
    if (repeat == u"coda" || repeat == u"segno") {
        words_text_ += find_detached_repeat_number(measure, track_, tick, placement(), delayed_directions);
    }

    // Check if coda is numbered by checking numerals after 'coda' or 'segno'
    if (repeat == u"daCapoAlCoda" || repeat == u"toCoda" || repeat == u"coda") {
        coda_id_ = get_marker_id(plain_words);
    } else if (repeat == u"dalSegnoAlFine" || repeat == u"segno" || repeat == u"dalSegno") {
        segno_id_ = get_marker_id(plain_words);
    } else if (repeat == u"dalSegnoAlCoda") {
        // Find numerals eg D.S I al Coda II
        std::wsmatch matches;
        std::wstring str = plain_words.to_std_w_string();
        static const std::wregex dal_segno_al_coda(L"^(d\\.? ?|d[ae]l )(s\\.?s?\\.?|segno\\.?) ?(i{0,3})?(\\(.*\\))? al coda ?(i{0,3})?$");
        if (std::regex_search(str, matches, dal_segno_al_coda)) {
            segno_id_ = String::from_std_w_string(matches.str(3));
            coda_id_ = String::from_std_w_string(matches.str(5));
        }
    }

    if ((repeat == u"dalSegno" || repeat == u"dalSegnoAlCoda") && segno_id_.empty()) {
        // If no numeral, count how many 's' in D.S. or D.S.S...
        segno_id_ = count_segno(plain_words);
    }

    if (!repeat.empty()) {
        TextBase *tb = nullptr;
        if (coda_id_ == u"i") {
            coda_id_.clear();
        }
        if (segno_id_ == u"i") {
            segno_id_.clear();
        }
        words_text_ = TextBase::un_escape(words_text_);
        if ((repeat == u"coda" || repeat == u"toCoda") && measure_has_coda) {
            // If there is already a coda in the bar, discard this marker
            words_text_.clear();
            return;
        } else if ((tb = find_jump(repeat)) || (tb = find_marker(repeat))) {
            if (tb->is_marker()) {
                if (repeat == u"segno") {
                    if (Marker *prev_segno = muse::value(segnos, tick.ticks(), nullptr)) {
                        // If there is another segno at this tick, combine their text and increase the label
                        // eg. two segno symbols will have the label 'segnoii'
                        if (prev_segno->xml_text().contains(u"<sym>segno</sym>") && words_text_.contains(u"<sym>segno</sym>")) {
                            prev_segno->set_xml_text(prev_segno->xml_text() + u" " + words_text_);
                            words_text_.clear();
                            const int segno_count = prev_segno->xml_text().count(u"<sym>segno</sym>");
                            const String label = String::from_std_string(std::string(segno_count, 'i'));
                            prev_segno->set_label(prev_segno->label() + label);

                            return;
                        }
                    } else {
                        segnos.insert(std::pair<int, Marker *>(tick.ticks(), to_marker(tb)));
                    }
                } else {
                    if (repeat == u"coda" || repeat == u"toCoda") {
                        measure_has_coda = true;
                        add_symbols_to_coda(words_text_, coda_id_);
                    }
                }
            }

            tb->set_track(track_);
            if (!words_text_.empty()) {
                tb->set_xml_text(words_text_);
                words_text_.clear();
            } else {
                tb->set_visible(false);
            }
            // Sometimes Jumps and Markers are exported on the incorrect side
            // of the barline (i.e. end of mm. 29 vs. beginning of mm. 30).
            // This fixes that.
            bool closer_to_left = tick - measure->tick() < measure->end_tick() - tick;
            if (tb->text_style_type() == TextStyleType::REPEAT_RIGHT && closer_to_left && measure->prev_measure()) {
                measure = measure->prev_measure();
            } else if (tb->text_style_type() == TextStyleType::REPEAT_LEFT && !closer_to_left && measure->next_measure()) {
                measure = measure->next_measure();
            }
            // Temporary solution to indent codas - add a horizontal frame at start of system or midway through
            MeasureBase *prev_measure_base = measure->prev();
            bool hbox = prev_measure_base && prev_measure_base->is_h_box();
            if (tb->is_marker() && to_marker(tb)->marker_type() == MarkerType::CODA && !hbox) {
                MeasureBase *gap = score_->insert_box(ElementType::HBOX, measure);
                to_h_box(gap)->set_box_width(10_sp);
            }
            tb->set_visible(visible_);
            measure->add(tb);
        }
    }
}

Marker *MusicXmlParserDirection::find_marker(const String &repeat) const {
    Marker *m = nullptr;
    if (repeat == u"segno") {
        m = Factory::create_marker(score_->dummy());
        // note: Marker::read() also contains code to set text style based on type
        // avoid duplicated code
        // apparently this MUST be after setTextStyle
        m->set_marker_type(MarkerType::SEGNO);
        m->set_label(m->property_default(Pid::LABEL).value<String>() + segno_id_);
    } else if (repeat == u"coda") {
        m = Factory::create_marker(score_->dummy());
        m->set_marker_type(MarkerType::CODA);
        m->set_label(m->property_default(Pid::LABEL).value<String>() + coda_id_);
    } else if (repeat == u"fine") {
        m = Factory::create_marker(score_->dummy(), TextStyleType::REPEAT_RIGHT);
        m->set_marker_type(MarkerType::FINE);
        m->reset_property(Pid::LABEL);
    } else if (repeat == u"toCoda") {
        m = Factory::create_marker(score_->dummy(), TextStyleType::REPEAT_RIGHT);
        m->set_marker_type(MarkerType::TOCODA);
        m->set_label(m->property_default(Pid::LABEL).value<String>() + coda_id_);
    }
    return m;
}

Jump *MusicXmlParserDirection::find_jump(const String &repeat) const {
    Jump *jp = nullptr;
    if (repeat == u"daCapo") {
        jp = Factory::create_jump(score_->dummy());
        jp->set_jump_type(JumpType::DC);
    } else if (repeat == u"daCapoAlCoda") {
        jp = Factory::create_jump(score_->dummy());
        jp->set_jump_type(JumpType::DC_AL_CODA);
        jp->set_play_until(jp->play_until() + coda_id_);
        jp->set_continue_at(jp->continue_at() + coda_id_);
    } else if (repeat == u"daCapoAlFine") {
        jp = Factory::create_jump(score_->dummy());
        jp->set_jump_type(JumpType::DC_AL_FINE);
    } else if (repeat == u"dalSegno") {
        jp = Factory::create_jump(score_->dummy());
        jp->set_jump_type(JumpType::DS);
        jp->set_jump_to(jp->jump_to() + segno_id_);
    } else if (repeat == u"dalSegnoAlCoda") {
        jp = Factory::create_jump(score_->dummy());
        jp->set_jump_type(JumpType::DS_AL_CODA);
        jp->set_jump_to(jp->jump_to() + segno_id_);
        jp->set_play_until(jp->play_until() + coda_id_);
        jp->set_continue_at(jp->continue_at() + coda_id_);
    } else if (repeat == u"dalSegnoAlFine") {
        jp = Factory::create_jump(score_->dummy());
        jp->set_jump_type(JumpType::DS_AL_FINE);
        jp->set_jump_to(jp->jump_to() + segno_id_);
    }
    return jp;
}

//---------------------------------------------------------
//   handleNmiCmi
//    Dolet strangely exports N.C. chord symbols as the
//    text direction "NmiCmi".
//---------------------------------------------------------

void MusicXmlParserDirection::handle_nmi_cmi(Measure *measure, const Fraction &tick, DelayedDirectionsList &delayed_directions) {
    if (!configuration()->infer_text_type()) {
        return;
    }
    if (!words_text_.contains(u"NmiCmi")) {
        return;
    }
    Harmony *ha = new Harmony(score_->dummy());
    HarmonyInfo *info = new HarmonyInfo(score_);
    info->set_root_tpc(Tpc::TPC_INVALID);
    info->set_id(-1);
    info->set_text_name(u"N.C.");
    ha->set_track(track_);
    ha->add_chord(info);
    MusicXmlDelayedDirectionElement *delayed_direction =
        new MusicXmlDelayedDirectionElement(total_y(), ha, track_, u"above", measure, tick);
    delayed_directions.push_back(delayed_direction);
    words_text_.replace(u"NmiCmi", u"N.C.");
}

void MusicXmlParserDirection::handle_chord_sym(const Fraction &tick, HarmonyMap &harmony_map) {
    if (!configuration()->infer_text_type() || placement() == "below") {
        return;
    }

    static const std::wregex re(L"^([abcdefg])(([#b♯♭])\3?)?(maj|min|m)?[769]?((add[#b♯♭]?(9|11|))|(sus[24]?))?(\\(.*\\))?$");
    String plain_words = words_text_.simplified().to_lower();
    if (!plain_words.contains(re)) {
        return;
    }

    Harmony *ha = Factory::create_harmony(score_->dummy());
    ha->set_harmony(words_text_);
    ha->set_track(track_);
    ha->set_placement(placement() == u"above" ? PlacementV::ABOVE : PlacementV::BELOW);
    ha->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    ha->reset_property(Pid::OFFSET);
    ha->set_visible(visible_);
    const HarmonyInfo *info = ha->chords().empty() ? nullptr : ha->chords().front();
    const ChordDescription *desc = info ? info->descr() : nullptr;
    HarmonyDesc new_harmony_desc(track_, ha, nullptr);

    const int ticks = tick.ticks();
    bool insert = true;
    for (auto itr = harmony_map.begin(); itr != harmony_map.end(); itr++) {
        if (itr->first != ticks) {
            continue;
        }
        HarmonyDesc &found_harmony_desc = itr->second;

        // Don't insert if there is a matching chord symbol
        // This symbol doesn't have a fret diagram, so no need to check that here
        if (track2staff(found_harmony_desc.track_) == track2staff(track_) && found_harmony_desc.descr() == desc) {
            insert = false;
        }
    }

    if (insert) {
        harmony_map.insert(std::pair<int, HarmonyDesc>(ticks, new_harmony_desc));
    }
    words_text_.clear();
}

void MusicXmlParserDirection::handle_tempo(String &words_string) {
    if (!configuration()->infer_text_type()) {
        return;
    }

    // Map Sibelius' representation of note types to their SMuFL counterparts and duration types
    static const MetronomeTextMap sibelius_syms = {{u"y", {u"<sym>metNote32ndUp</sym>", DurationType::V_32ND}},
                                                   {u"x", {u"<sym>metNote16thUp</sym>", DurationType::V_16TH}},
                                                   {u"e", {u"<sym>metNote8thUp</sym>", DurationType::V_EIGHTH}},
                                                   {u"q", {u"<sym>metNoteQuarterUp</sym>", DurationType::V_QUARTER}},
                                                   {u"h", {u"<sym>metNoteHalfUp</sym>", DurationType::V_HALF}},
                                                   {u"w", {u"<sym>metNoteWhole</sym>", DurationType::V_WHOLE}},
                                                   {u"V", {u"<sym>metNoteDoubleWholeSquare</sym>", DurationType::V_BREVE}},
                                                   {u"W", {u"<sym>metNoteDoubleWhole</sym>", DurationType::V_BREVE}}};

    static const MetronomeTextMap met_times_syms = {
        {u"Œ", {u"<sym>metNoteQuarterUp</sym>", DurationType::V_QUARTER}},
        {u"Ó", {u"<sym>metNoteHalfUp</sym>", DurationType::V_HALF}},
    };

    MetronomeTextMap text_map;
    if (pass1_.sib_or_dolet()) {
        text_map = sibelius_syms;
    } else if (font_family_ == u"MetTimes Plain") {
        text_map = met_times_syms;
    }
    if (text_map.empty()) {
        return;
    }
    String substitutions;
    for (auto &entry : text_map) {
        substitutions.append(entry.first);
    }

    // Pick up any tempo markings which may have been exported as <words>
    // eg. andante (q = c. 90)
    // Sibelius uses a symbol font with the characters 'yxeqhVwW' each drawn as a different duration
    // which we need to map to SMuFL syms
    String plain_words = MScoreTextToMusicXml::to_plain_text(words_string.simplified());

    static const std::wregex tempo(L".*([" + substitutions.to_std_w_string() + L"])(\\.?)(?:\\s|\\u00A0)*=[^0-9]*([0-9]+).*");
    StringList tempo_matches = plain_words.search(tempo, {1, 2, 3}, SplitBehavior::SKIP_EMPTY_PARTS);

    // Not a tempo
    if (tempo_matches.size() < 2) {
        return;
    }

    const String dur = tempo_matches.at(0);
    const bool dot = tempo_matches.size() == 3;
    const String val = tempo_matches.at(dot ? 2 : 1);

    const String dot_str = dot ? u"<sym>space</sym><sym>metAugmentationDot</sym>" : u"";

    static const std::wregex replace(L"(.*)[" + substitutions.to_std_w_string() + L"]\\.?((?:\\s|\\u00A0)*=[^0-9]*[0-9]+.*)");
    const String new_str = u"$1" + text_map.at(dur).first + dot_str + u"$2";
    plain_words.replace(replace, new_str);
    words_string = plain_words;

    if (!val.empty() && !dur.empty()) {
        bool ok;
        double d = val.to_double(&ok);
        TDuration duration = TDuration(text_map.at(dur).second);
        duration.set_dots(dot);

        if (ok && duration.is_valid()) {
            // convert fraction to beats per minute
            tpo_metro_ = 4 * duration.fraction().numerator() * d / duration.fraction().denominator();
        }
    }
}

bool MusicXmlParserDirection::is_likely_sticking() {
    if (!configuration()->infer_text_type()) {
        return false;
    }

    String plain_words = MScoreTextToMusicXml::to_plain_text(words_text_.simplified());
    static const std::wregex sticking(L"^[lrbLRB]+$");
    return plain_words.contains(sticking) && rehearsal_text_.empty() && metro_text_.empty() && tpo_sound_ < 0.1;
}

bool MusicXmlParserDirection::is_likely_dynamic_range() const {
    if (!configuration()->infer_text_type()) {
        return false;
    }

    String raw_words_text = words_text_.simplified();
    static const std::regex re("(<.*?>)");
    raw_words_text.remove(re);

    // If there are two dynamics and a hyphen in a single direction node, this is most likely something like "mp-f"
    return (raw_words_text.simplified() == u"-" || raw_words_text.simplified() == u"—") && dynamics_list_.size() == 2;
}

PlayingTechniqueType MusicXmlParserDirection::get_playing_technique() const {
    if (!configuration()->infer_text_type()) {
        return PlayingTechniqueType::UNDEFINED;
    }

    String plain_words = MScoreTextToMusicXml::to_plain_text(words_text_.simplified()).to_lower();
    plain_words.remove(std::wregex(L"[\\.,;:]+")); // remove punctuation

    // Ordered longest-first so specific phrases match before their substrings
    // (e.g. "without mute" before "mute", "con sord" before "ord")
    static const std::vector<std::pair<muse::String, PlayingTechniqueType>> text_to_play_technique_type{
        {u"palm mute", PlayingTechniqueType::UNDEFINED}, {u"palmmute", PlayingTechniqueType::UNDEFINED},
        {u"without mute", PlayingTechniqueType::OPEN},   {u"distortion", PlayingTechniqueType::DISTORTION},
        {u"senza sord", PlayingTechniqueType::OPEN},     {u"harmonics", PlayingTechniqueType::HARMONICS},
        {u"overdrive", PlayingTechniqueType::OVERDRIVE}, {u"sul pont", PlayingTechniqueType::SUL_PONTICELLO},
        {u"sul tast", PlayingTechniqueType::SUL_TASTO},  {u"con sord", PlayingTechniqueType::MUTE},
        {u"detache", PlayingTechniqueType::DETACHE},     {u"martele", PlayingTechniqueType::MARTELE},
        {u"natural", PlayingTechniqueType::NATURAL},     {u"no mute", PlayingTechniqueType::OPEN},
        {u"unmuted", PlayingTechniqueType::OPEN},        {u"vibrato", PlayingTechniqueType::VIBRATO},
        {u"legato", PlayingTechniqueType::LEGATO},       {u"normal", PlayingTechniqueType::NATURAL},
        {u"legno", PlayingTechniqueType::COL_LEGNO},     {u"open", PlayingTechniqueType::OPEN},
        {u"arco", PlayingTechniqueType::NATURAL},        {u"jazz", PlayingTechniqueType::JAZZ_TONE},
        {u"mute", PlayingTechniqueType::MUTE},           {u"pizz", PlayingTechniqueType::PIZZICATO},
        {u"sord", PlayingTechniqueType::MUTE},           {u"trem", PlayingTechniqueType::TREMOLO},
        {u"ord", PlayingTechniqueType::NATURAL},
    };

    for (const auto &technique : text_to_play_technique_type) {
        if (plain_words.contains(technique.first)) {
            return technique.second;
        }
    }

    return PlayingTechniqueType::UNDEFINED;
}

void MusicXmlParserDirection::handle_drum_instrument(bool is_perc, Fraction tick) const {
    if (!configuration()->infer_text_type() || words_text_.empty() || !rehearsal_text_.empty() || !metro_text_.empty() ||
        tpo_sound_ > 0.1 || !is_perc) {
        return;
    }

    String plain_words = words_text_;

    static const std::regex to("to ", std::regex_constants::icase);
    static const std::regex brackets("\\(.*\\)");
    plain_words.remove(to);
    plain_words.remove(brackets);

    plain_words = MScoreTextToMusicXml::to_plain_text(plain_words.simplified());

    const InstrumentTemplate *it = search_template_for_instr_name_list({plain_words}, true, false);

    // Ignore marching percussion, as these won't map correctly to the standard drumkit
    if (it && it->family_id() != u"batterie") {
        int pitch = it->use_drumset && it->drumset ? it->drumset->next_pitch(0) : 0;

        // Text under the staff will be for voice 2 and 4 even if it's in voice 1
        track_idx_t track = placement() == u"below" && !(track_ & 1) ? track_ + 1 : track_;

        InferredPercInstr instr = InferredPercInstr(pitch, track, it->id, tick);
        pass2_.add_inferred_perc_instr(instr);
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
    const AsciiStringView line_end = e_.ascii_attribute("line-end");
    const AsciiStringView line_type = e_.ascii_attribute("line-type");
    const AsciiStringView end_length = e_.ascii_attribute("end-length");
    const bool is_wavy = line_type == "wavy";
    const ElementType element_type = is_wavy ? ElementType::TRILL : ElementType::TEXTLINE;
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.get_spanner({element_type, number});
    if (type == "start") {
        if (spanner_exists(starts, number, element_type)) {
            logger_->log_error(String(u"%1 with number %2 already started").arg(TConv::user_name(element_type).translated()).arg(number),
                               &e_);
            e_.skip_current_element();
            return;
        }
        SLine *sline = spdesc.is_stopped ? spdesc.sp : 0;
        if ((sline && sline->is_trill()) || (!sline && is_wavy)) {
            if (!sline) {
                sline = Factory::create_trill(score_->dummy());
            }
            Trill *trill = to_trill(sline);
            trill->set_trill_type(TrillType::PRALLPRALL_LINE);

            if (!line_end.empty() && line_end != "none") {
                logger_->log_error(String(u"line-end not supported for line-type \"wavy\""));
            }
        } else if ((sline && sline->is_text_line()) || (!sline && !is_wavy)) {
            if (!sline) {
                sline = new TextLine(score_->dummy());
                sline->set_system_flag(system_direction_);
            }
            TextLine *text_line = to_text_line(sline);
            // if (placement.empty()) placement = "above";  // TODO ? set default

            if (!end_length.empty()) {
                double length = end_length.to_double();
                text_line->set_begin_hook_height(Spatium(line_end == "both" ? length / 20 : length / 10));
                text_line->set_property_flags(Pid::BEGIN_HOOK_HEIGHT, PropertyFlags::UNSTYLED);
            }
            if (line_end == "up") {
                text_line->set_begin_hook_type(HookType::HOOK_90);
            } else if (line_end == "down") {
                text_line->set_begin_hook_type(HookType::HOOK_90);
                text_line->set_begin_hook_height(-1 * text_line->begin_hook_height());
            } else if (line_end == "both") {
                text_line->set_begin_hook_type(HookType::HOOK_90T);
            } else if (line_end == "arrow") {
                text_line->set_begin_hook_type(HookType::ARROW_FILLED);
            } else if (line_end == "none") {
                text_line->set_begin_hook_type(HookType::NONE);
            }

            // hack: combine with a previous words element
            if (!words_text_.empty()) {
                // TextLine supports only limited formatting, remove all (compatible with 1.3)
                text_line->set_begin_text(MScoreTextToMusicXml::to_plain_text(words_text_));
                words_text_.clear();
            }

            if (line_type == "solid") {
                text_line->set_line_style(LineType::SOLID);
                text_line->set_property_flags(Pid::LINE_STYLE, PropertyFlags::UNSTYLED);
            } else if (line_type == "dashed") {
                text_line->set_line_style(LineType::DASHED);
                text_line->set_property_flags(Pid::LINE_STYLE, PropertyFlags::UNSTYLED);
            } else if (line_type == "dotted") {
                text_line->set_line_style(LineType::DOTTED);
                text_line->set_property_flags(Pid::LINE_STYLE, PropertyFlags::UNSTYLED);
            } else if (line_type != "wavy") {
                logger_->log_error(String(u"unsupported line-type: %1").arg(String::from_ascii(line_type.ascii())), &e_);
            }
            color_item(text_line, Color::from_string(e_.ascii_attribute("color").ascii()));
        }

        starts.push_back(MusicXmlSpannerDesc(sline, element_type, number));
    } else if (type == "stop") {
        if (spanner_exists(stops, number, element_type)) {
            logger_->log_error(String(u"%1 with number %2 already stopped").arg(TConv::user_name(element_type).translated()).arg(number),
                               &e_);
            e_.skip_current_element();
            return;
        }
        SLine *sline = spdesc.is_started ? spdesc.sp : 0;
        if ((sline && sline->is_trill()) || (!sline && is_wavy)) {
            if (!sline) {
                sline = new Trill(score_->dummy());
            }
            if (!line_end.empty() && line_end != "none") {
                logger_->log_error(String(u"line-end not supported for line-type \"wavy\""));
            }
        } else if ((sline && sline->is_text_line()) || (!sline && !is_wavy)) {
            if (!sline) {
                sline = new TextLine(score_->dummy());
            }
            TextLine *text_line = to_text_line(sline);

            if (!end_length.empty()) {
                double length = end_length.to_double();
                text_line->set_end_hook_height(Spatium(line_end == "both" ? length / 20 : length / 10));
                text_line->set_property_flags(Pid::END_HOOK_HEIGHT, PropertyFlags::UNSTYLED);
            }
            if (line_end == "up") {
                text_line->set_end_hook_type(HookType::HOOK_90);
            } else if (line_end == "down") {
                text_line->set_end_hook_type(HookType::HOOK_90);
                text_line->set_end_hook_height(-1 * text_line->end_hook_height());
            } else if (line_end == "both") {
                text_line->set_end_hook_type(HookType::HOOK_90T);
            } else if (line_end == "arrow") {
                text_line->set_end_hook_type(HookType::ARROW_FILLED);
            } else if (line_end == "none") {
                text_line->set_end_hook_type(HookType::NONE);
            }
        }

        stops.push_back(MusicXmlSpannerDesc(sline, element_type, number));
    }
    e_.skip_current_element();
}

//---------------------------------------------------------
//   dashes
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/dashes node.
 */

void MusicXmlParserDirection::dashes(const String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                                     std::vector<MusicXmlSpannerDesc> &stops) {
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.get_spanner({ElementType::HAIRPIN, number});
    if (type == u"start") {
        if (spanner_exists(starts, number, ElementType::HAIRPIN)) {
            logger_->log_error(String(u"hairpin with number %1 already started").arg(number), &e_);
            e_.skip_current_element();
            return;
        }
        TextLineBase *b = spdesc.is_stopped ? to_text_line_base(spdesc.sp) : Factory::create_text_line(score_->dummy());
        // if (placement.empty()) placement = "above";  // TODO ? set default

        // hack: combine with a previous words element
        if (!words_text_.empty()) {
            // TextLine supports only limited formatting, remove all (compatible with 1.3)
            String simplified_text = MScoreTextToMusicXml::to_plain_text(words_text_).simplified();
            if (is_likely_tempo_line(track_)) {
                b = Factory::create_gradual_tempo_change(score_->dummy());
                GradualTempoChangeType gtc = get_tempo_change_type_from_string(simplified_text);
                to_gradual_tempo_change(b)->set_tempo_change_type(gtc);
            }
            b->set_begin_text(simplified_text);
            words_text_.clear();
        }

        b->set_begin_hook_type(HookType::NONE);
        b->set_end_hook_type(HookType::NONE);
        b->set_line_style(LineType::DASHED);
        // TODO brackets and dashes now share the same storage
        // because they both use ElementType::TEXTLINE
        // use MusicXML specific type instead
        starts.push_back(MusicXmlSpannerDesc(b, ElementType::TEXTLINE, number));
    } else if (type == u"stop") {
        if (spanner_exists(stops, number, ElementType::HAIRPIN)) {
            logger_->log_error(String(u"hairpin with number %1 already stopped").arg(number), &e_);
            e_.skip_current_element();
            return;
        }
        TextLineBase *b = spdesc.is_started ? to_text_line_base(spdesc.sp) : Factory::create_text_line(score_->dummy());
        stops.push_back(MusicXmlSpannerDesc(b, ElementType::TEXTLINE, number));
    }
    e_.skip_current_element();
}

//---------------------------------------------------------
//   octaveShift
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/octave-shift node.
 */

void MusicXmlParserDirection::octave_shift(const String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                                           std::vector<MusicXmlSpannerDesc> &stops) {
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.get_spanner({ElementType::OTTAVA, number});
    if (type == u"up" || type == u"down") {
        if (spanner_exists(starts, number, ElementType::OTTAVA)) {
            logger_->log_error(String(u"ottava with number %1 already started").arg(number), &e_);
            e_.skip_current_element();
            return;
        }
        int ottavasize = e_.int_attribute("size");
        if (!(ottavasize == 8 || ottavasize == 15 || ottavasize == 22)) {
            logger_->log_error(String(u"unknown octave-shift size %1").arg(ottavasize), &e_);
        } else {
            Ottava *o = spdesc.is_stopped ? to_ottava(spdesc.sp) : Factory::create_ottava(score_->dummy());

            if (type == u"down") {
                placement_ = placement_.empty() ? u"above" : placement_;
                if (ottavasize == 8) {
                    o->set_ottava_type(OttavaType::OTTAVA_8VA);
                } else if (ottavasize == 15) {
                    o->set_ottava_type(OttavaType::OTTAVA_15MA);
                } else if (ottavasize == 22) {
                    o->set_ottava_type(OttavaType::OTTAVA_22MA);
                }
            } else if (type == u"up") {
                placement_ = placement_.empty() ? u"below" : placement_;
                if (ottavasize == 8) {
                    o->set_ottava_type(OttavaType::OTTAVA_8VB);
                } else if (ottavasize == 15) {
                    o->set_ottava_type(OttavaType::OTTAVA_15MB);
                } else if (ottavasize == 22) {
                    o->set_ottava_type(OttavaType::OTTAVA_22MB);
                }
            }

            color_item(o, Color::from_string(e_.ascii_attribute("color").ascii()));

            starts.push_back(MusicXmlSpannerDesc(o, ElementType::OTTAVA, number));
        }
    } else if (type == u"stop") {
        if (spanner_exists(stops, number, ElementType::OTTAVA)) {
            logger_->log_error(String(u"ottava with number %1 already stopped").arg(number), &e_);
            e_.skip_current_element();
            return;
        }
        Ottava *o = spdesc.is_started ? to_ottava(spdesc.sp) : Factory::create_ottava(score_->dummy());
        stops.push_back(MusicXmlSpannerDesc(o, ElementType::OTTAVA, number));
    }
    e_.skip_current_element();
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
    AsciiStringView line = e_.ascii_attribute("line");
    String sign = e_.attribute("sign");
    const Color color = Color::from_string(e_.ascii_attribute("color").ascii());

    // We have found that many exporters omit "sign" even when one is originally present,
    // therefore we will default to "yes", even though this is technically against the spec.
    bool override_default_sign = true; // TODO: set this flag based on the exporting software
    if (sign.empty()) {
        if (line != "yes" || (override_default_sign && type == "start")) {
            sign = u"yes"; // MusicXML 2.0 compatibility
        } else if (line == "yes") {
            sign = u"no"; // MusicXML 2.0 compatibility
        }
    }
    MusicXmlExtendedSpannerDesc &spdesc = pass2_.get_spanner({ElementType::PEDAL, number});
    if (type == u"start" || type == u"resume" || type == u"sostenuto") {
        if (spdesc.is_started && !spdesc.is_stopped) {
            // Previous pedal unterminated
            // if previous pedal was a change, create a new change instead of a new pedal start
            if (to_pedal(spdesc.sp)->begin_hook_type() == HookType::HOOK_45) {
                Pedal *p = Factory::create_pedal(score_->dummy());
                p->set_begin_hook_type(HookType::HOOK_45);
                p->set_end_hook_type(HookType::HOOK_90);
                if (line == "yes") {
                    p->set_line_visible(true);
                } else {
                    p->set_line_visible(false);
                }
                if (sign == u"no") {
                    p->set_begin_text(u"");
                    p->set_continue_text(u"");
                    p->set_end_text(u"");
                }
                color_item(p, color);
                starts.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
                e_.skip_current_element();

                return;
            } else {
                // likely an unrecorded "discontinue", so delete the line.
                pass2_.delete_handled_spanner(spdesc.sp);
                spdesc.is_started = false;
            }
        }
        Pedal *p = spdesc.is_stopped ? to_pedal(spdesc.sp) : Factory::create_pedal(score_->dummy());
        if (line == "yes") {
            p->set_line_visible(true);
        } else {
            p->set_line_visible(false);
        }
        if (!p->line_visible() || sign == u"yes") {
            p->set_begin_text(score_->style().style_st(Sid::PEDAL_TEXT));
            p->set_continue_text(score_->style().style_st(Sid::PEDAL_CONTINUE_TEXT));
            if (type == "sostenuto") {
                p->set_begin_text(u"<sym>keyboardPedalSost</sym>");
                p->set_continue_text(u"<sym>keyboardPedalParensLeft</sym><sym>keyboardPedalSost</sym><sym>keyboardPedalParensRight</sym>");
            }
        } else {
            p->set_begin_text(u"");
            p->set_continue_text(u"");
            p->set_begin_hook_type(type == "resume" ? HookType::NONE : HookType::HOOK_90);
        }
        p->set_end_hook_type(HookType::NONE);
        color_item(p, color);
        starts.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
    } else if (type == u"stop" || type == u"discontinue") {
        Pedal *p = spdesc.is_started ? to_pedal(spdesc.sp) : Factory::create_pedal(score_->dummy());
        if (line == "yes") {
            p->set_line_visible(true);
        } else if (line == "no") {
            p->set_line_visible(false);
        }
        if ((!p->line_visible() || sign == u"yes") && p->end_hook_type() == HookType::NONE) {
            p->set_end_text(u"<sym>keyboardPedalUp</sym>");
        } else {
            p->set_end_hook_type(type == "discontinue" ? HookType::NONE : HookType::HOOK_90);
        }
        stops.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
    } else if (type == "change") {
        // pedal change is implemented as two separate pedals
        // first stop the first one
        if (spdesc.is_started && !spdesc.is_stopped) {
            Pedal *p = to_pedal(spdesc.sp);
            p->set_end_hook_type(HookType::HOOK_45);
            if (line == "yes") {
                p->set_line_visible(true);
            } else if (line == "no") {
                p->set_line_visible(false);
            }
            stops.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
        } else {
            logger_->log_error(String(u"\"change\" type pedal created without existing pedal"), &e_);
        }
        // then start a new one
        Pedal *p = Factory::create_pedal(score_->dummy());
        p->set_begin_hook_type(HookType::HOOK_45);
        p->set_end_hook_type(HookType::HOOK_90);
        if (line == "yes") {
            p->set_line_visible(true);
        } else {
            p->set_line_visible(false);
        }
        if (sign == u"no") {
            p->set_begin_text(u"");
            p->set_continue_text(u"");
        }
        color_item(p, color);
        starts.push_back(MusicXmlSpannerDesc(p, ElementType::PEDAL, number));
    } else if (type == "continue") {
        // ignore
    } else {
        LOGD("unknown pedal type %s", muPrintable(type));
    }

    e_.skip_current_element();
}

//---------------------------------------------------------
//   wedge
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type/wedge node.
 */

void MusicXmlParserDirection::wedge(const String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                                    std::vector<MusicXmlSpannerDesc> &stops) {
    AsciiStringView niente = e_.ascii_attribute("niente");
    const MusicXmlExtendedSpannerDesc &spdesc = pass2_.get_spanner({ElementType::HAIRPIN, number});
    if (type == "crescendo" || type == "diminuendo") {
        if (spanner_exists(starts, number, ElementType::HAIRPIN)) {
            logger_->log_error(String(u"hairpin with number %1 already started").arg(number), &e_);
            e_.skip_current_element();
            return;
        }
        Hairpin *h = spdesc.is_stopped ? to_hairpin(spdesc.sp) : Factory::create_hairpin(score_->dummy());
        h->set_hairpin_type(type == "crescendo" ? HairpinType::CRESC_HAIRPIN : HairpinType::DIM_HAIRPIN);
        if (niente == "yes") {
            h->set_hairpin_circled_tip(true);
        }
        color_item(h, Color::from_string(e_.ascii_attribute("color").ascii()));
        AsciiStringView line_type = e_.ascii_attribute("line-type");
        if (line_type == "dashed") {
            h->set_line_style(LineType::DASHED);
        } else if (line_type == "dotted") {
            h->set_line_style(LineType::DOTTED);
        }
        const String spread = e_.attribute("spread");
        if (!spread.empty() && configuration()->import_layout()) {
            const Spatium val(spread.to_double() / 10.0);
            h->set_hairpin_height(val);
        }
        starts.push_back(MusicXmlSpannerDesc(h, ElementType::HAIRPIN, number));
    } else if (type == "stop") {
        if (spanner_exists(stops, number, ElementType::HAIRPIN)) {
            logger_->log_error(String(u"hairpin with number %1 already stopped").arg(number), &e_);
            e_.skip_current_element();
            return;
        }
        Hairpin *h = spdesc.is_started ? to_hairpin(spdesc.sp) : Factory::create_hairpin(score_->dummy());
        if (niente == "yes") {
            h->set_hairpin_circled_tip(true);
        }
        stops.push_back(MusicXmlSpannerDesc(h, ElementType::HAIRPIN, number));
    }
    e_.skip_current_element();
}

//---------------------------------------------------------
//   toString
//---------------------------------------------------------

String MusicXmlExtendedSpannerDesc::to_string() const {
    String sp_str = sp ? String::from_std_string(sp->eid().to_std_string()) : u"null";
    return String(u"sp %1 tp %2 tick2 %3 track2 %4 %5 %6")
        .arg(sp_str, tick2.to_string())
        .arg(static_cast<int>(track2))
        .arg(is_started ? u"started" : u"", is_stopped ? u"stopped" : u"");
}

//---------------------------------------------------------
//   addSpanner
//---------------------------------------------------------

void MusicXmlParserPass2::add_spanner(const MusicXmlSpannerDesc &d) {
    MusicXmlExtendedSpannerDesc &spdesc = get_spanner(d);
    spdesc.sp = d.sp;
}

//---------------------------------------------------------
//   getSpanner
//---------------------------------------------------------

MusicXmlExtendedSpannerDesc &MusicXmlParserPass2::get_spanner(const MusicXmlSpannerDesc &d) {
    if (d.tp == ElementType::HAIRPIN && 0 <= d.nr && d.nr < ::mu::iex::musicxml::max_number_level) {
        return hairpins_[d.nr];
    } else if (d.tp == ElementType::OTTAVA && 0 <= d.nr && d.nr < ::mu::iex::musicxml::max_number_level) {
        return ottavas_[d.nr];
    } else if (d.tp == ElementType::PEDAL && 0 == d.nr) {
        return pedal_;
    } else if ((d.tp == ElementType::TEXTLINE || d.tp == ElementType::TRILL) && 0 <= d.nr && d.nr < ::mu::iex::musicxml::max_number_level) {
        return brackets_[d.nr];
    }
    logger_->log_error(String(u"invalid number %1").arg(d.nr + 1), &e_);
    return dummy_new_music_xml_spanner_desc_;
}

//---------------------------------------------------------
//   clearSpanner
//---------------------------------------------------------

void MusicXmlParserPass2::clear_spanner(const MusicXmlSpannerDesc &d) {
    MusicXmlExtendedSpannerDesc &spdesc = get_spanner(d);
    spdesc = {};
}

//---------------------------------------------------------
//   deleteHandledSpanner
//---------------------------------------------------------
/**
 Delete a spanner that's already been added to _spanners.
 This is used to remove pedal markings that are never stopped
 */

void MusicXmlParserPass2::delete_handled_spanner(SLine *const &spanner) {
    muse::remove(spanners_, spanner);
    delete spanner;
}

InferredPercInstr MusicXmlParserPass2::inferred_perc_instr(const Fraction &tick, const track_idx_t track_idx) {
    InferredPercInstr instr = InferredPercInstr();

    for (InferredPercList::iterator iter = inferred_perc_.begin(); iter != inferred_perc_.end();) {
        if (iter->tick == tick && iter->track == track_idx) {
            instr = *iter;
            iter = inferred_perc_.erase(iter);
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
    String tempo_text;
    String per_minute;
    bool parenth = e_.ascii_attribute("parentheses") == "yes";

    if (parenth) {
        tempo_text += u"(";
    }

    TDuration dur1;
    TDuration dur2;

    while (e_.read_next_start_element()) {
        if (e_.name() == "metronome-note" || e_.name() == "metronome-relation") {
            skip_log_curr_elem();
            continue;
        }
        String txt = e_.read_text();
        if (e_.name() == "beat-unit") {
            // set first dur that is still invalid
            ByteArray ba = txt.to_ascii();
            if (!dur1.is_valid()) {
                dur1.set_type(TConv::from_xml(ba.const_char(), DurationType::V_INVALID));
            } else if (!dur2.is_valid()) {
                dur2.set_type(TConv::from_xml(ba.const_char(), DurationType::V_INVALID));
            }
        } else if (e_.name() == "beat-unit-dot") {
            if (dur2.is_valid()) {
                dur2.set_dots(1);
            } else if (dur1.is_valid()) {
                dur1.set_dots(1);
            }
        } else if (e_.name() == "per-minute") {
            per_minute = txt;
        } else {
            skip_log_curr_elem();
        }
    }

    if (dur1.is_valid()) {
        tempo_text += TempoText::duration2tempo_text_string(dur1);
    }
    if (dur2.is_valid()) {
        tempo_text += u" = ";
        tempo_text += TempoText::duration2tempo_text_string(dur2);
    } else if (!per_minute.empty()) {
        tempo_text += u" = ";
        tempo_text += per_minute;
    }
    if (dur1.is_valid() && !dur2.is_valid() && !per_minute.empty()) {
        bool ok;
        double d = per_minute.to_double(&ok);
        if (ok) {
            // convert fraction to beats per minute
            r = 4 * dur1.fraction().numerator() * d / dur1.fraction().denominator();
        }
    }

    if (parenth) {
        tempo_text += u")";
    }

    return tempo_text;
}

//---------------------------------------------------------
//   determineBarLineType
//---------------------------------------------------------

static bool determine_bar_line_type(const String &bar_style, const String &repeat, BarLineType &type, bool &visible,
                                    const MusicXmlExporterSoftware &exporter) {
    // set defaults
    type = BarLineType::NORMAL;
    visible = true;

    if (bar_style == u"light-heavy" && repeat == u"backward") {
        type = BarLineType::END_REPEAT;
    } else if (bar_style == u"heavy-light" && repeat == u"forward") {
        type = BarLineType::START_REPEAT;
    } else if (bar_style == u"light-heavy" && repeat == u"forward" && exporter == MusicXmlExporterSoftware::NOTEFLIGHT) {
        type = BarLineType::START_REPEAT;
    } else if (bar_style == u"light-heavy" && repeat.empty()) {
        type = BarLineType::END;
    } else if (bar_style == u"heavy-light" && repeat.empty()) {
        type = BarLineType::REVERSE_END;
    } else if (bar_style == u"regular") {
        type = BarLineType::NORMAL;
    } else if (bar_style == u"dashed") {
        type = BarLineType::BROKEN;
    } else if (bar_style == u"dotted") {
        type = BarLineType::DOTTED;
    } else if (bar_style == u"light-light") {
        type = BarLineType::DOUBLE;
        /*
        } else if (barStyle == "heavy-light") {
         ;
         */
    } else if (bar_style == u"heavy-heavy") {
        type = BarLineType::DOUBLE_HEAVY;
    } else if (bar_style == u"heavy") {
        type = BarLineType::HEAVY;
    } else if (bar_style == u"none") {
        visible = false;
    } else if (bar_style.empty()) {
        if (repeat == u"backward") {
            type = BarLineType::END_REPEAT;
        } else if (repeat == u"forward") {
            type = BarLineType::START_REPEAT;
        } else {
            return false;
        }
    } else if ((bar_style == u"tick") || (bar_style == u"short")) {
        // handled later (as normal barline with different parameters)
    } else {
        LOGD("unsupported bar type <%s>", muPrintable(bar_style)); // TODO
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

void MusicXmlParserPass2::barline(const String &part_id, Measure *measure, const Fraction &tick) {
    AsciiStringView loc = e_.ascii_attribute("location");
    if (loc.empty()) {
        loc = "right";
    }
    // Place barline in correct place
    Fraction loc_tick = tick;
    if (loc == "left") {
        loc_tick = measure->tick();
    } else if (loc == "right") {
        loc_tick = measure->end_tick();
    }

    const String coda_label = e_.attribute("coda");
    const String segno_label = e_.attribute("segno");

    String bar_style;
    Color barline_color;
    String ending_number;
    String ending_type;
    Color ending_color;
    String ending_text;
    String repeat;
    bool print_ending = true;

    while (e_.read_next_start_element()) {
        if (e_.name() == "bar-style") {
            barline_color = Color::from_string(e_.ascii_attribute("color").ascii());
            bar_style = e_.read_text();
        } else if (e_.name() == "segno") {
            const Color segno_color = Color::from_string(e_.ascii_attribute("color").ascii());
            const AsciiStringView segno_symbol = e_.ascii_attribute("smufl");
            Measure *marked_measure = (loc == "right") ? measure->next_measure() : measure;
            if (marked_measure == nullptr) {
                logger_->log_error(u"coda or segno marker cannot be placed at the end of the score", &e_);
                e_.skip_current_element();
                continue;
            }
            Marker *m = Factory::create_marker(marked_measure);
            m->set_marker_type(MarkerType::SEGNO);
            if (!segno_symbol.empty()) {
                m->set_xml_text(u"<sym>" + String::from_ascii(segno_symbol.ascii()) + u"</sym>");
            }
            if (!segno_label.empty()) {
                m->set_label(segno_label);
            }
            if (segno_color.is_valid()) {
                color_item(m, segno_color);
            }
            add_elem_offset(m, 0, u"above", marked_measure, marked_measure->tick());
            e_.skip_current_element();
        } else if (e_.name() == "coda") {
            const Color coda_color = Color::from_string(e_.ascii_attribute("color").ascii());
            const AsciiStringView coda_symbol = e_.ascii_attribute("smufl");
            Measure *marked_measure = (loc == "right") ? measure->next_measure() : measure;
            if (marked_measure == nullptr) {
                logger_->log_error(u"coda or segno marker cannot be placed at the end of the score", &e_);
                e_.skip_current_element();
                continue;
            }
            Marker *m = Factory::create_marker(marked_measure);
            m->set_marker_type(MarkerType::CODA);
            if (!coda_symbol.empty()) {
                m->set_xml_text(u"<sym>" + String::from_ascii(coda_symbol.ascii()) + u"</sym>");
            }
            if (!coda_label.empty()) {
                m->set_label(coda_label);
            }
            if (coda_color.is_valid()) {
                color_item(m, coda_color);
            }
            add_elem_offset(m, 0, u"above", marked_measure, marked_measure->tick());
            e_.skip_current_element();
        } else if (e_.name() == "fermata") {
            const Color fermata_color = Color::from_string(e_.ascii_attribute("color").ascii());
            const String fermata_type = e_.attribute("type");
            SegmentType st = SegmentType::BAR_LINE;
            if (loc_tick == measure->end_tick()) {
                st = SegmentType::END_BAR_LINE;
            } else if (loc_tick == measure->tick()) {
                st = SegmentType::BEGIN_BAR_LINE;
            }
            Segment *const segment = measure->get_segment(st, loc_tick);
            const track_idx_t track = pass1_.track_for_part(part_id);
            Fermata *fermata = Factory::create_fermata(segment);
            fermata->set_sym_id(convert_fermata_to_sym_id(e_.read_text()));
            fermata->set_track(track);
            segment->add(fermata);
            color_item(fermata, fermata_color);
            if (fermata_type == u"inverted") {
                fermata->set_placement(PlacementV::BELOW);
                fermata->reset_property(Pid::OFFSET);
            } else if (fermata_type.empty()) {
                fermata->set_placement(fermata->property_default(Pid::PLACEMENT).value<PlacementV>());
            }

            // Terminate tempo lines
            const InferredTempoLineStack &lines = get_inferred_tempo_line();
            terminate_inferred_line(std::vector<TextLineBase *>(lines.begin(), lines.end()), loc_tick, track,
                                    configuration()->infer_text_type());
        } else if (e_.name() == "ending") {
            ending_number = e_.attribute("number");
            ending_type = e_.attribute("type");
            ending_color = Color::from_string(e_.ascii_attribute("color").ascii());
            print_ending = e_.ascii_attribute("print-object") != "no";
            ending_text = e_.read_text();
        } else if (e_.name() == "repeat") {
            repeat = e_.attribute("direction");
            int count = e_.attribute("times").to_int();
            if (!count) {
                count = 2;
            }
            measure->set_repeat_count(count);
            bool repeat_bar_tips = !e_.attribute("winged").empty() && e_.attribute("winged") != "none";
            score_->undo_change_style_val(Sid::REPEAT_BAR_TIPS, repeat_bar_tips);
            e_.skip_current_element();
        } else {
            skip_log_curr_elem();
        }
    }

    BarLineType type = BarLineType::NORMAL;
    bool visible = true;
    if (determine_bar_line_type(bar_style, repeat, type, visible, pass1_.exporter_software())) {
        const track_idx_t track = pass1_.track_for_part(part_id);
        if (type & BarLineType::START_REPEAT) {
            // combine start_repeat flag with current state initialized during measure parsing
            measure->set_repeat_start(true);
        } else if (type & BarLineType::END_REPEAT) {
            // combine end_repeat flag with current state initialized during measure parsing
            measure->set_repeat_end(true);
        } else {
            if (bar_style != "regular" || barline_color.is_valid() || loc == "middle") {
                // Add barline to the first voice of every staff in the part,
                // and span every barline except the last
                const Part *part = pass1_.get_part(part_id);
                staff_idx_t nstaves = part->nstaves();
                for (staff_idx_t i = 0; i < nstaves; ++i) {
                    const Staff *staff = part->staff(i);
                    const bool span_staff = nstaves > 1 ? i < nstaves - 1 : staff->bar_line_span();
                    track_idx_t current_track = track + (i * ::mu::engraving::voices);
                    auto b = create_barline(measure->score(), current_track, type, visible, bar_style, span_staff);
                    color_item(b.get(), barline_color);
                    add_barline_to_measure(measure, loc_tick, std::move(b));
                }
            }
        }
    }

    do_ending(part_id, measure, ending_number, ending_type, ending_color, ending_text, print_ending);
}

//---------------------------------------------------------
//   findRedundantVolta
//---------------------------------------------------------
static Volta *find_redundant_volta(const track_idx_t track, const Measure *measure) {
    auto spanners = measure->score()->spanner_map().find_overlapping(measure->tick().ticks(), measure->end_tick().ticks());
    for (auto spanner : spanners) {
        if (spanner.value->is_volta() && track2staff(spanner.value->track()) != track2staff(track)) {
            return to_volta(spanner.value);
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   doEnding
//---------------------------------------------------------

void MusicXmlParserPass2::do_ending(const String &part_id, Measure *measure, const String &number, const String &type, const Color color,
                                    const String &text, const bool print) {
    if (!(number.empty() && type.empty())) {
        if (number.empty()) {
            logger_->log_error(u"empty ending number", &e_);
        } else if (type.empty()) {
            logger_->log_error(u"empty ending type", &e_);
        } else {
            StringList sl = number.split(u',', SKIP_EMPTY_PARTS);
            std::vector<int> i_ending_numbers;
            bool unsupported = false;
            for (const String &s : sl) {
                int i_ending_number = s.to_int();
                if (i_ending_number <= 0) {
                    unsupported = true;
                    break;
                }
                i_ending_numbers.push_back(i_ending_number);
            }

            if (unsupported) {
                logger_->log_error(String(u"unsupported ending number '%1'").arg(number), &e_);
            } else {
                // Ignore if it is hidden and redundant
                Volta *redundant_volta = find_redundant_volta(pass1_.track_for_part(part_id), measure);
                if (!print && redundant_volta) {
                    logger_->log_debug_info(u"Ignoring redundant hidden Volta", &e_);
                } else if (type == u"start") {
                    Volta *volta = Factory::create_volta(score_->dummy());
                    volta->set_track(pass1_.track_for_part(part_id));
                    volta->set_text(text.empty() ? number : text);
                    // LVIFIX TODO also support endings "1 - 3"
                    volta->endings().clear();
                    muse::join(volta->endings(), i_ending_numbers);
                    volta->set_tick(measure->tick());
                    score_->add_element(volta);
                    last_volta_ = volta;
                    volta->set_visible(print);
                    color_item(volta, color);
                } else if (type == u"stop") {
                    if (last_volta_) {
                        last_volta_->set_volta_type(Volta::Type::CLOSED);
                        last_volta_->set_tick2(measure->tick() + measure->ticks());
                        // Assume print-object was handled at the start
                        last_volta_ = 0;
                    } else if (!redundant_volta) {
                        logger_->log_error(u"ending stop without start", &e_);
                    }
                } else if (type == u"discontinue") {
                    if (last_volta_) {
                        last_volta_->set_volta_type(Volta::Type::OPEN);
                        last_volta_->set_tick2(measure->tick() + measure->ticks());
                        // Assume print-object was handled at the start
                        last_volta_ = 0;
                    } else if (!redundant_volta) {
                        logger_->log_error(u"ending discontinue without start", &e_);
                    }
                } else {
                    logger_->log_error(String(u"unsupported ending type '%1'").arg(type), &e_);
                }

                // Delete any hidden redundant voltas before
                while (redundant_volta && !redundant_volta->visible()) {
                    score_->remove_element(redundant_volta);
                    delete redundant_volta;
                    redundant_volta = find_redundant_volta(pass1_.track_for_part(part_id), measure);
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

static void add_sym_to_sig(KeySigEvent &sig, const String &step, const String &alter, const String &accid, const String &smufl) {
    //LOGD("addSymToSig(step '%s' alt '%s' acc '%s')",
    //       muPrintable(step), muPrintable(alter), muPrintable(accid));

    SymId id = music_xml_string2acc_sym_id(accid, smufl);

    if (id == SymId::NO_SYM) {
        bool ok;
        double d;
        d = alter.to_double(&ok);
        AccidentalType acc_tp_alter = ok ? microtonal_guess(d) : AccidentalType::NONE;
        String s = accidental_type2_music_xml_string(acc_tp_alter);
        if (s == u"other") {
            s = accidental_type2_smufl_music_xml_string(acc_tp_alter);
        }
        id = music_xml_string2acc_sym_id(s);
    }

    if (step.size() == 1 && id != SymId::NO_SYM) {
        const String table = u"CDEFGAB";
        if (table.contains(step)) {
            CustDef cd;
            cd.degree = int(table.index_of(step));
            cd.sym = id;
            sig.custom_key_defs().push_back(cd);
            sig.set_custom(true);
        }
    }
}

//---------------------------------------------------------
//   addKey
//---------------------------------------------------------

/**
 Add a KeySigEvent to the score.
 */

static void add_key(const KeySigEvent key, const Color key_color, const bool print_obj, Score *score, Measure *measure,
                    const staff_idx_t staff_idx, const Fraction &tick) {
    Key oldkey = score->staff(staff_idx)->key(tick);
    // TODO only if different custom key ?
    if (oldkey != key.key() || key.custom() || key.is_atonal()) {
        // new key differs from key in effect at this tick
        Segment *s = measure->get_segment(SegmentType::KEY_SIG, tick);
        KeySig *keysig = Factory::create_key_sig(s);
        keysig->set_track(staff_idx * ::mu::engraving::voices);
        keysig->set_key_sig_event(key);
        keysig->set_visible(print_obj);
        color_item(keysig, key_color);
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

static void flush_altered_tone(KeySigEvent &kse, String &step, String &alt, String &acc, String &smufl) {
    //LOGD("flushAlteredTone(step '%s' alt '%s' acc '%s')",
    //       muPrintable(step), muPrintable(alt), muPrintable(acc));

    if (step.empty() && alt.empty() && acc.empty()) {
        return; // nothing to do
    }
    // step and alt are required, but also accept step and acc
    if (!step.empty() && (!alt.empty() || !acc.empty())) {
        add_sym_to_sig(kse, step, alt, acc, smufl);
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

void MusicXmlParserPass2::key(const String &part_id, Measure *measure, const Fraction &tick) {
    String str_keyno = e_.attribute("number");
    int keyno = -1; // assume no number (see below)
    if (!str_keyno.empty()) {
        keyno = pass1_.get_music_xml_part(part_id).staff_number_to_index(str_keyno.to_int());
        if (keyno < 0) {
            // conversion error (-1), assume staff 0
            logger_->log_error(String(u"invalid key number '%1'").arg(str_keyno), &e_);
            keyno = 0;
        }
    }
    const bool print_object = e_.ascii_attribute("print-object") != "no";
    const Color key_color = Color::from_string(e_.ascii_attribute("color").ascii());

    // for custom keys, a single altered tone is described by
    // key-step (required),  key-alter (required) and key-accidental (optional)
    // none, one or more altered tone may be present
    // a simple state machine is required to detect them
    KeySigEvent key;
    String key_step;
    String key_alter;
    String key_accidental;
    String smufl;

    while (e_.read_next_start_element()) {
        if (e_.name() == "fifths") {
            Key t_key = Key(e_.read_int());
            Key c_key = t_key;
            Interval v = pass1_.get_part(part_id)->instrument()->transpose();
            if (!v.is_zero() && !score_->style().style_b(Sid::CONCERT_PITCH)) {
                c_key = Transpose::transpose_key(t_key, v);
                // if there are more than 6 accidentals in transposing key, it cannot be PreferSharpFlat::AUTO
                Part *part = pass1_.get_part(part_id);
                if ((t_key > 6 || t_key < -6) && part->prefer_sharp_flat() == PreferSharpFlat::AUTO) {
                    part->set_prefer_sharp_flat(PreferSharpFlat::NONE);
                }
            }
            key.set_concert_key(c_key);
            key.set_key(t_key);
        } else if (e_.name() == "mode") {
            key.set_mode(TConv::from_xml(e_.read_text().to_ascii().const_char(), KeyMode::UNKNOWN));
        } else if (e_.name() == "cancel") {
            skip_log_curr_elem(); // TODO ??
        } else if (e_.name() == "key-step") {
            flush_altered_tone(key, key_step, key_alter, key_accidental, smufl);
            key_step = e_.read_text();
        } else if (e_.name() == "key-alter") {
            key_alter = e_.read_text().trimmed();
        } else if (e_.name() == "key-accidental") {
            smufl = e_.attribute("smufl");
            key_accidental = e_.read_text();
        } else {
            skip_log_curr_elem();
        }
    }
    flush_altered_tone(key, key_step, key_alter, key_accidental, smufl);

    size_t nstaves = pass1_.get_part(part_id)->nstaves();
    staff_idx_t staff_idx = pass1_.track_for_part(part_id) / ::mu::engraving::voices;
    if (keyno == -1) {
        // apply key to all staves in the part
        for (staff_idx_t i = 0; i < nstaves; ++i) {
            add_key(key, key_color, print_object, score_, measure, staff_idx + i, tick);
        }
    } else if (keyno < static_cast<int>(nstaves)) {
        add_key(key, key_color, print_object, score_, measure, staff_idx + keyno, tick);
    }
}

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/clef node.
 */

void MusicXmlParserPass2::clef(const String &part_id, Measure *measure, const Fraction &tick) {
    ClefType clef = ClefType::G;
    StaffTypes st = StaffTypes::STANDARD;

    String c;
    int i = 0;
    int line = -1;

    const String str_clefno = e_.attribute("number");
    const bool after_barline = e_.ascii_attribute("after-barline") == "yes";
    const bool print_object = e_.ascii_attribute("print-object") != "no";
    const Color clef_color = Color::from_string(e_.ascii_attribute("color").ascii());

    while (e_.read_next_start_element()) {
        if (e_.name() == "sign") {
            c = e_.read_text();
        } else if (e_.name() == "line") {
            line = e_.read_int();
        } else if (e_.name() == "clef-octave-change") {
            i = e_.read_int();
            if (i && !(c == "F" || c == "G" || c == "C")) {
                LOGD("clef-octave-change only implemented for F and G key"); // TODO
            }
        } else {
            skip_log_curr_elem();
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
        if (has_drumset_) {
            st = StaffTypes::PERC_DEFAULT;
        }
    } else if (c == u"TAB") {
        clef = ClefType::TAB;
        st = StaffTypes::TAB_DEFAULT;
    } else {
        LOGD("clef: unknown clef <sign=%s line=%d oct ch=%d>", muPrintable(c), line, i); // TODO
    }

    Part *part = pass1_.get_part(part_id);
    IF_ASSERT_FAILED(part) { return; }

    // TODO: check error handling for
    // - single staff
    // - multi-staff with same clef
    int clefno = 0; // default
    if (!str_clefno.empty()) {
        clefno = pass1_.get_music_xml_part(part_id).staff_number_to_index(str_clefno.to_int());
    }
    if (clefno < 0 || clefno >= int(part->nstaves())) {
        // conversion error (0) or other issue, assume staff 1
        // Also for Cubase 6.5.5 which generates clef number="2" in a single staff part
        // Same fix is required in pass 1 and pass 2
        logger_->log_error(String(u"invalid clef number '%1'").arg(str_clefno), &e_);
        clefno = 0;
    }

    Segment *s;
    ClefToBarlinePosition position = ClefToBarlinePosition::AUTO;
    // check if the clef change needs to be in the previous measure
    if (!after_barline && (tick == measure->tick()) && measure->prev_measure()) {
        s = measure->prev_measure()->get_segment(SegmentType::CLEF, tick);
        if (measure->prev_measure()->repeat_end()) {
            position = ClefToBarlinePosition::BEFORE;
        }
    } else {
        s = measure->get_segment(tick.is_not_zero() ? SegmentType::CLEF : SegmentType::HEADER_CLEF, tick);
        if (measure->prev_measure() && !measure->prev_measure()->repeat_end()) {
            position = ClefToBarlinePosition::AFTER;
        }
    }

    Clef *clefs = Factory::create_clef(s);
    clefs->set_clef_type(clef);
    clefs->set_visible(print_object);
    clefs->set_clef_to_barline_position(position);
    color_item(clefs, clef_color);
    track_idx_t track = pass1_.track_for_part(part_id) + clefno * ::mu::engraving::voices;
    clefs->set_track(track);
    s->add(clefs);

    // set the correct staff type
    // note that clef handling should probably done in pass1
    staff_idx_t staff_idx = score_->staff_idx(part) + clefno;
    int lines = score_->staff(staff_idx)->lines(Fraction(0, 1));
    if (tick.is_zero()) { // changing staff type not supported (yet ?)
        score_->staff(staff_idx)->set_staff_type(tick, *StaffType::preset(st));
        score_->staff(staff_idx)->set_lines(tick, lines); // preserve previously set staff lines
        score_->staff(staff_idx)->set_bar_line_to(0);     // default
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

static bool determine_time_sig(const String &beats, const String &beat_type, const String &time_symbol, TimeSigType &st, int &bts,
                               int &btp) {
    // initialize
    st = TimeSigType::NORMAL;
    bts = 0; // the beats (max 4 separated by "+") as integer
    btp = 0; // beat-type as integer
    // determine if timesig is valid
    if (time_symbol == u"cut") {
        st = TimeSigType::ALLA_BREVE;
    } else if (time_symbol == u"common") {
        st = TimeSigType::FOUR_FOUR;
    } else if (time_symbol == u"single-number") {
        // let pass
    } else if (!time_symbol.empty() && time_symbol != u"normal") {
        LOGD("determineTimeSig: time symbol <%s> not recognized", muPrintable(time_symbol)); // TODO
        return false;
    }

    btp = beat_type.to_int();
    StringList list = beats.split(u'+');
    for (size_t i = 0; i < list.size(); i++) {
        bts += list.at(i).to_int();
    }

    // determine if bts and btp are valid
    if (bts <= 0 || btp <= 0) {
        LOGD("determineTimeSig: beats=%s and/or beat-type=%s not recognized", muPrintable(beats), muPrintable(beat_type)); // TODO
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

void MusicXmlParserPass2::time(const String &part_id, Measure *measure, const Fraction &tick) {
    String beats;
    String beat_type;
    String time_symbol = e_.attribute("symbol");
    bool print_object = e_.ascii_attribute("print-object") != "no";
    const Color time_color = Color::from_string(e_.ascii_attribute("color").ascii());

    while (e_.read_next_start_element()) {
        if (e_.name() == "beats") {
            beats = e_.read_text();
        } else if (e_.name() == "beat-type") {
            beat_type = e_.read_text();
        } else {
            skip_log_curr_elem();
        }
    }

    if (!beats.empty() && !beat_type.empty()) {
        // determine if timesig is valid
        TimeSigType st = TimeSigType::NORMAL;
        int bts = 0; // total beats as integer (beats may contain multiple numbers, separated by "+")
        int btp = 0; // beat-type as integer
        if (determine_time_sig(beats, beat_type, time_symbol, st, bts, btp)) {
            time_sig_dura_ = Fraction(bts, btp);
            Fraction fraction_t_sig = Fraction(bts, btp);
            for (size_t i = 0; i < pass1_.get_part(part_id)->nstaves(); ++i) {
                Segment *s = measure->get_segment(SegmentType::TIME_SIG, tick);
                TimeSig *timesig = Factory::create_time_sig(s);
                timesig->set_visible(print_object);
                color_item(timesig, time_color);
                track_idx_t track = pass1_.track_for_part(part_id) + i * ::mu::engraving::voices;
                timesig->set_track(track);
                timesig->set_sig(fraction_t_sig, st);
                // handle simple compound and single time signatures
                if (beats.contains(Char(u'+'))) {
                    timesig->set_numerator_string(beats);
                    timesig->set_denominator_string(beat_type);
                } else if (time_symbol == u"single-number") {
                    timesig->set_numerator_string(beats);
                }
                s->add(timesig);
            }
        }
    }

    if (print_object && score_->style().style_v(Sid::TIME_SIG_PLACEMENT) == TimeSigPlacement::ACROSS_STAVES) {
        track_idx_t track = pass1_.track_for_part(part_id);
        Staff *st = score_->staff(track2staff(track));
        if (!st->is_system_object_staff() && st->idx() != 0) {
            score_->add_system_object_staff(st);
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
    divs_ = e_.read_int();
    if (!(divs_ > 0)) {
        logger_->log_error(u"illegal divisions", &e_);
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

static bool is_whole_measure_rest(const String &type, const Fraction rest_duration, const Fraction measure_duration) {
    if (!rest_duration.is_valid() || !measure_duration.is_valid()) {
        return false;
    }

    return (type.empty() && rest_duration == measure_duration) || (type == u"whole" && rest_duration == measure_duration);
}

//---------------------------------------------------------
//   determineDuration
//---------------------------------------------------------

/**
 * Determine duration for a note or rest.
 * This includes whole measure rest detection.
 */

static TDuration determine_duration(const bool is_rest, const bool measure_rest, const String &type, const int dots,
                                    const Fraction chord_rest_duration, const Fraction measure_duration) {
    //LOGD("determineDuration %s, type <%s>, dots %d, duration %s, measure duration %s",
    //      isRest ? "rest" : "chord", muPrintable(durationType), dots, muPrintable(chordRestDuration.toString()), muPrintable(measureDuration.toString()));

    TDuration res;
    if (is_rest && (measure_rest || is_whole_measure_rest(type, chord_rest_duration, measure_duration))) {
        res.set_type(DurationType::V_MEASURE);
    } else if (type.empty()) {
        // If no type, set duration type based on duration.
        // Note that sometimes unusual duration (e.g. 261/256) are found.
        res.set_val(chord_rest_duration.ticks());
    } else {
        ByteArray ba = type.to_ascii();
        res.set_type(TConv::from_xml(ba.const_char(), DurationType::V_INVALID));
        res.set_dots(dots);
        if (!res.is_valid()) {
            res.set_type(DurationType::V_QUARTER); // default, TODO: use measureDuration ?
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

static Chord *find_or_create_chord(Score *, Measure *m, const Fraction &tick, const int track, const int move, const TDuration duration,
                                   const Fraction dura, BeamMode bm, bool small) {
    //LOGD("findOrCreateChord tick %d track %d dur ticks %d ticks %s bm %hhd",
    //       tick, track, duration.ticks(), muPrintable(dura.print()), bm);
    Chord *c = m->find_chord(tick, track);
    if (c == 0) {
        Segment *s = m->get_segment(SegmentType::CHORD_REST, tick);
        c = Factory::create_chord(s);
        // better not to force beam end, as the beam palette does not support it
        if (bm == BeamMode::END) {
            c->set_beam_mode(BeamMode::AUTO);
        } else {
            c->set_beam_mode(bm);
        }
        c->set_track(track);
        // Chord is initialized with the smallness of its first note.
        // If a non-small note is added later, this is handled in handleSmallness.
        c->set_small(small);

        set_chord_rest_duration(c, duration, dura);
        s->add(c);
    }
    c->set_staff_move(move);
    return c;
}

//---------------------------------------------------------
//   graceNoteType
//---------------------------------------------------------

/**
 * convert duration and slash to grace note type
 */

NoteType grace_note_type(const TDuration duration, const bool slash) {
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

static Chord *create_grace_chord(Score *score, const int track, const TDuration duration, const bool slash, const bool small) {
    Chord *c = Factory::create_chord(score->dummy());
    c->set_note_type(grace_note_type(duration, slash));
    c->set_track(track);
    // Chord is initialized with the smallness of its first note.
    // If a non-small note is added later, this is handled in handleSmallness.
    c->set_small(small);
    // note grace notes have no durations, use default fraction 0/1
    set_chord_rest_duration(c, duration, Fraction());
    return c;
}

//---------------------------------------------------------
//   handleDisplayStep
//---------------------------------------------------------

/**
 * convert display-step and display-octave to staff line
 */

static void handle_display_step(ChordRest *cr, int step, int octave, const Fraction &tick, double spatium) {
    if (0 <= step && step <= 6 && 0 <= octave && octave <= 9) {
        //LOGD("rest step=%d oct=%d", step, octave);
        ClefType clef = cr->staff()->clef(tick);
        int po = ClefInfo::pitch_offset(clef);
        //LOGD(" clef=%hhd po=%d step=%d", clef, po, step);
        int dp = 7 * (octave + 2) + step;
        //LOGD(" dp=%d po-dp=%d", dp, po-dp);
        cr->ryoffset() = (po - dp + 3) * spatium / 2;
    }
}

//---------------------------------------------------------
//   handleSmallness
//---------------------------------------------------------

static void handle_smallness(bool cue_or_small, Note *note, Chord *c) {
    if (cue_or_small) {
        note->set_small(!c->is_small()); // Avoid redundant smallness
    } else {
        note->set_small(false);
        if (c->is_small()) {
            // What was a small chord becomes small notes in a non-small chord
            c->set_small(false);
            for (Note *other_note : c->notes()) {
                if (note != other_note) {
                    other_note->set_small(true);
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

static void set_note_head(Note *note, const String &notehead_filled) {
    if (notehead_filled == u"no") {
        note->set_head_type(NoteHeadType::HEAD_HALF);
    } else if (notehead_filled == u"yes") {
        note->set_head_type(NoteHeadType::HEAD_QUARTER);
    }
}

//---------------------------------------------------------
//   computeBeamMode
//---------------------------------------------------------

/**
 Calculate the beam mode based on the collected beamTypes.
 */

static BeamMode compute_beam_mode(const std::map<int, String> &beam_types) {
    // Start with uniquely-handled beam modes
    if (muse::value(beam_types, 1) == u"continue" && muse::value(beam_types, 2) == u"begin") {
        return BeamMode::BEGIN16;
    } else if (muse::value(beam_types, 1) == u"continue" && muse::value(beam_types, 2) == u"continue" &&
               muse::value(beam_types, 3) == u"begin") {
        return BeamMode::BEGIN32;
    }
    // Generic beam modes are naive to all except the first beam
    else if (muse::value(beam_types, 1) == u"begin") {
        return BeamMode::BEGIN;
    } else if (muse::value(beam_types, 1) == u"continue") {
        return BeamMode::MID;
    } else if (muse::value(beam_types, 1) == u"end") {
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

static void add_figured_bass_elements(FiguredBassList &fbl, const Fraction note_start_time, const int ms_track, const Fraction dura,
                                      Measure *measure) {
    if (!fbl.empty()) {
        Fraction s_tick = note_start_time; // starting tick
        for (FiguredBass *fb : fbl) {
            fb->set_track(ms_track);
            // No duration tag defaults ticks() to 0; set to note value
            if (fb->ticks().is_zero()) {
                fb->set_ticks(dura);
            }
            // TODO: set correct onNote value
            Segment *s = measure->get_segment(SegmentType::CHORD_REST, s_tick);
            s->add(fb);
            s_tick += fb->ticks();
        }
        fbl.clear();
    }
}

//---------------------------------------------------------
//   addTremolo
//---------------------------------------------------------

static void add_tremolo(ChordRest *cr, const int tremolo_nr, const String &tremolo_type, const String &smufl, const Color &color,
                        Chord *&trem_start, MusicXmlLogger *logger, const XmlStreamReader *const xmlreader, Fraction &time_mod) {
    if (!cr->is_chord()) {
        return;
    }
    if (tremolo_nr) {
        //LOGD("tremolo %d type '%s' ticks %d tremStart %p", tremoloNr, muPrintable(tremoloType), ticks, _tremStart);
        if (tremolo_nr >= 1 && tremolo_nr <= 6) {
            if (tremolo_type.empty() || tremolo_type == u"single") {
                TremoloType type = TremoloType::INVALID_TREMOLO;
                switch (tremolo_nr) {
                    case 1: type = TremoloType::R8; break;
                    case 2: type = TremoloType::R16; break;
                    case 3: type = TremoloType::R32; break;
                    case 4: type = TremoloType::R64; break;
                    case 5: type = TremoloType::R128; break;
                    case 6: type = TremoloType::R256; break;
                }

                if (type != TremoloType::INVALID_TREMOLO) {
                    TremoloSingleChord *tremolo = Factory::create_tremolo_single_chord(mu::engraving::to_chord(cr));
                    tremolo->set_tremolo_type(type);
                    color_item(tremolo, color);
                    cr->add(tremolo);
                }
            } else if (tremolo_type == u"start") {
                if (trem_start) {
                    logger->log_error(u"MusicXml::import: double tremolo start", xmlreader);
                }
                trem_start = to_chord(cr);
                // timeMod takes into account also the factor 2 of a two-note tremolo
                if (time_mod.is_valid() && ((time_mod.denominator() % 2) == 0)) {
                    time_mod.set_denominator(time_mod.denominator() / 2);
                }
            } else if (tremolo_type == u"stop") {
                if (trem_start) {
                    TremoloType type = TremoloType::INVALID_TREMOLO;
                    switch (tremolo_nr) {
                        case 1: type = TremoloType::C8; break;
                        case 2: type = TremoloType::C16; break;
                        case 3: type = TremoloType::C32; break;
                        case 4: type = TremoloType::C64; break;
                        case 5: type = TremoloType::C128; break;
                        case 6: type = TremoloType::C256; break;
                    }

                    if (type != TremoloType::INVALID_TREMOLO) {
                        TremoloTwoChord *tremolo = Factory::create_tremolo_two_chord(mu::engraving::to_chord(cr));
                        tremolo->set_tremolo_type(type);
                        color_item(tremolo, color);
                        tremolo->set_chords(trem_start, to_chord(cr));
                        // fixup chord duration and type
                        const Fraction trem_dur = cr->ticks() * Fraction(1, 2);
                        tremolo->chord1()->set_duration_type(trem_dur);
                        tremolo->chord1()->set_ticks(trem_dur);
                        tremolo->chord2()->set_duration_type(trem_dur);
                        tremolo->chord2()->set_ticks(trem_dur);
                        // add tremolo to first chord (only)
                        trem_start->add(tremolo);
                    }
                    // timeMod takes into account also the factor 2 of a two-note tremolo
                    if (time_mod.is_valid() && ((time_mod.denominator() % 2) == 0)) {
                        time_mod.set_denominator(time_mod.denominator() / 2);
                    }
                } else {
                    logger->log_error(u"MusicXml::import: double tremolo stop w/o start", xmlreader);
                }
                trem_start = nullptr;
            }
        } else {
            logger->log_error(String(u"unknown tremolo type %1").arg(tremolo_nr), xmlreader);
        }
    } else if (tremolo_nr == 0 && (tremolo_type == u"unmeasured" || tremolo_type.empty() || smufl == u"buzzRoll")) {
        // Out of all the SMuFL unmeasured tremolos, we only support 'buzzRoll'
        TremoloSingleChord *tremolo = Factory::create_tremolo_single_chord(mu::engraving::to_chord(cr));
        tremolo->set_tremolo_type(TremoloType::BUZZ_ROLL);
        color_item(tremolo, color);
        cr->add(tremolo);
    } else if (!smufl.empty() && smufl != u"buzzRoll") {
        logger->log_error(String(u"MusicXml::import: only buzzRoll glyph is supported for unmeasured tremolos"), xmlreader);
    }
}

//---------------------------------------------------------
//   setPitch
//---------------------------------------------------------

// TODO: refactor: optimize parameters

static void set_pitch(Note *note, const MusicXmlInstruments &instruments, const String &instrument_id, const MusicXmlNotePitch &mnp,
                      const int octave_shift, const Instrument *const instrument) {
    if (mnp.unpitched()) {
        if (has_drumset(instruments) && muse::contains(instruments, instrument_id)) {
            // step and oct are display-step and ...-oct
            // get pitch from instrument definition in drumset instead
            int unpitched = instruments.at(instrument_id).unpitched;
            note->set_pitch(std::clamp(unpitched, 0, 127));
            note->set_tpc_from_pitch();
        } else {
            //LOGD("disp step %d oct %d", displayStep, displayOctave);
            xml_set_pitch(note, mnp.display_step(), 0, 0.0, mnp.display_octave(), 0, instrument);
        }
    } else {
        xml_set_pitch(note, mnp.step(), mnp.alter(), mnp.tuning(), mnp.octave(), octave_shift, instrument);
    }
}

//---------------------------------------------------------
//   setDrumset
//---------------------------------------------------------

// set drumset information
// note that in MuseScore, the drumset contains defaults for notehead,
// line and stem direction, while a MusicXML file contains actuals.
// the MusicXML values for each note are simply copied to the defaults

static void set_drumset(Chord *c, MusicXmlParserPass1 &pass1, const String &part_id, const String &instrument_id,
                        const Fraction &note_start_time, const MusicXmlNotePitch &mnp, const DirectionV stem_dir,
                        const NoteHeadGroup head_group) {
    // determine staff line based on display-step / -octave and clef type
    const ClefType clef = c->staff()->clef(note_start_time);
    const int po = ClefInfo::pitch_offset(clef);
    const int pitch = music_xml_step_alt_oct2_pitch(mnp.display_step(), 0, mnp.display_octave());
    int line = po - abs_step(pitch);

    // correct for number of staff lines
    // see ExportMusicXml::unpitch2xml for explanation
    // TODO handle other # staff lines ?
    int staff_lines = c->staff()->lines(Fraction(0, 1));
    if (staff_lines == 1) {
        line -= 8;
    }
    if (staff_lines == 3) {
        line -= 2;
    }

    // the drum palette cannot handle stem direction AUTO,
    // overrule if necessary
    DirectionV overruled_stem_dir = stem_dir;
    if (stem_dir == DirectionV::AUTO) {
        if (line > 4) {
            overruled_stem_dir = DirectionV::DOWN;
        } else {
            overruled_stem_dir = DirectionV::UP;
        }
    }
    // this should be done in pass 1, would make _pass1 const here
    pass1.set_drumset_default(part_id, instrument_id, head_group, line, overruled_stem_dir);
}

void MusicXmlParserPass2::xml_set_drumset_pitch(Note *note, const Chord *chord, const Staff *staff, int step, int octave,
                                                NoteHeadGroup head_group, DirectionV &stem_dir, Instrument *instrument) {
    Drumset *ds = instrument->drumset();
    // get line
    // determine staff line based on display-step / -octave and clef type
    const ClefType clef = staff->clef(chord->tick());
    const int po = ClefInfo::pitch_offset(clef);
    const int pitch = music_xml_step_alt_oct2_pitch(step, 0, octave);
    int line = po - abs_step(pitch);
    const int staff_lines = staff->lines(chord->tick());
    if (staff_lines == 1) {
        line -= 8;
    }
    if (staff_lines == 3) {
        line -= 2;
    }

    const int first_drum = ds->next_pitch(0);
    int cur_drum = first_drum;
    int new_pitch = pitch;
    bool match_found = false;
    do {
        if (ds->line(cur_drum) == line) {
            if (ds->note_head(cur_drum) == head_group) {
                new_pitch = cur_drum;
                match_found = true;
                break;
            }
        }
        cur_drum = ds->next_pitch(cur_drum);
    } while (cur_drum != first_drum);

    // Find inferred instruments at this tick
    if (configuration()->infer_text_type()) {
        InferredPercInstr instr = inferred_perc_instr(chord->tick(), chord->track());
        if (instr.track != muse::nidx) {
            // Clear old instrument
            ds->drum(new_pitch) = DrumInstrument();

            new_pitch = instr.pitch;
            if (stem_dir == DirectionV::AUTO) {
                stem_dir = chord->voice() & 1 ? DirectionV::DOWN : DirectionV::UP;
            }
            ds->drum(new_pitch) = DrumInstrument(instr.name, head_group, line, stem_dir, static_cast<int>(chord->voice()));
        }
    }

    // If there is no exact match add an entry to the drumkit with the XML line and notehead
    if (!match_found) {
        // Create new instrument in drumkit
        if (stem_dir == DirectionV::AUTO) {
            if (line > 4) {
                stem_dir = DirectionV::DOWN;
            } else {
                stem_dir = DirectionV::UP;
            }
        }

        ds->drum(new_pitch) = DrumInstrument(u"drum", head_group, line, stem_dir, static_cast<int>(chord->voice()));
    } else if (stem_dir == DirectionV::AUTO) {
        if (ds->voice(new_pitch) != static_cast<int>(chord->voice())) {
            stem_dir = chord->voice() & 1 ? DirectionV::DOWN : DirectionV::UP;
        } else {
            stem_dir = ds->stem_direction(new_pitch);
        }
    }

    note->set_pitch(new_pitch);
    note->set_tpc_from_pitch();
}

//---------------------------------------------------------
//   note
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note node.
 */

Note *MusicXmlParserPass2::note(const String &part_id, Measure *measure, const Fraction s_time, const Fraction prev_s_time,
                                Fraction &missing_prev, Fraction &dura, Fraction &missing_curr, String &current_voice, GraceChordList &gcl,
                                size_t &gac, Beams &curr_beams, FiguredBassList &fbl, int &alt, MusicXmlTupletStates &tuplet_states,
                                Tuplets &tuplets, ArpeggioMap &arp_map, DelayedArpMap &delayed_arps) {
    if (e_.ascii_attribute("print-spacing") == "no") {
        note_print_spacing_no(dura);
        return 0;
    }

    bool chord = false;
    bool cue = false;
    bool is_small = false;
    bool grace = false;
    bool rest = false;
    bool measure_rest = false;
    int staff = 0;
    String type;
    String voice;
    DirectionV stem_dir = DirectionV::AUTO;
    bool no_stem = false;
    bool has_head = true;
    NoteHeadGroup head_group = NoteHeadGroup::HEAD_NORMAL;
    NoteHeadScheme head_scheme = NoteHeadScheme::HEAD_AUTO;
    const Color note_color = Color::from_string(e_.ascii_attribute("color").ascii());
    Color notehead_color;
    Color stem_color;
    Color beam_color;
    bool notehead_parentheses = false;
    String notehead_filled;
    // velocity as a percentage of the MIDI 1.0 default forte value of 90;
    // an explicit dynamics="0" means a silent note, which the score model can
    // only represent as velocity 1 (velocity 0 means "unset")
    const bool has_dynamics = e_.has_attribute("dynamics");
    const int velocity = has_dynamics ? music_xml_dynamic_velocity(e_.attribute("dynamics"), 1, 1) : 1;
    bool grace_slash = false;
    double grace_steal_following = -1.0;
    double grace_steal_previous = -1.0;
    bool print_object = e_.ascii_attribute("print-object") != "no";
    bool print_lyric = (print_object && e_.ascii_attribute("print-lyric") != "no") || e_.ascii_attribute("print-lyric") == "yes";
    bool is_single_drumset = false;
    BeamMode bm;
    std::map<int, String> beam_types;
    String beam_fan;
    String instrument_id;
    String tie_type;
    MusicXmlParserLyric lyric{
        pass1_.get_music_xml_part(part_id).lyric_number_handler(), e_, score_, *configuration(), logger_, pass1_.is_vocal_staff(part_id)};
    MusicXmlParserNotations notations{e_, score_, logger_, *this};

    MusicXmlNoteDuration mnd{divs_, logger_, &pass1_};
    MusicXmlNotePitch mnp{logger_};

    while (e_.read_next_start_element()) {
        if (mnp.read_properties(e_, score_)) {
            // element handled
        } else if (mnd.read_properties(e_)) {
            // element handled
        } else if (e_.name() == "accidental") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "beam") {
            beam_color = Color::from_string(e_.ascii_attribute("color").ascii());
            beam_fan = e_.attribute("fan");
            beam(beam_types);
        } else if (e_.name() == "chord") {
            chord = true;
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "cue") {
            cue = true;
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "grace") {
            grace = true;
            grace_slash = e_.ascii_attribute("slash") == "yes";
            grace_steal_following = e_.double_attribute("steal-time-following", -1.0);
            grace_steal_previous = e_.double_attribute("steal-time-previous", -1.0);
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "instrument") {
            instrument_id = e_.attribute("id");
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "lyric") {
            // lyrics on grace notes not (yet) supported by MuseScore
            // add to main note instead
            lyric.parse(print_lyric);
        } else if (e_.name() == "notations") {
            notations.parse();
            add_error(notations.errors());
        } else if (e_.name() == "notehead") {
            notehead_color = Color::from_string(e_.ascii_attribute("color").ascii());
            notehead_parentheses = e_.ascii_attribute("parentheses") == "yes";
            notehead_filled = e_.attribute("filled");
            String notehead_value = e_.read_text();
            if (notehead_value == "none") {
                has_head = false;
            } else if (notehead_value == "named" && pass1_.exporter_software() == MusicXmlExporterSoftware::NOTEFLIGHT) {
                head_scheme = NoteHeadScheme::HEAD_PITCHNAME;
            } else {
                head_group = convert_notehead(notehead_value);
            }
        } else if (e_.name() == "notehead-text") {
            String notehead_text;
            while (e_.read_next_start_element()) {
                if (e_.name() == "display-text") {
                    notehead_text = e_.read_text();
                } else if (e_.name() == "accidental-text") {
                    e_.skip_current_element();
                } else {
                    skip_log_curr_elem();
                }
            }
            if (notehead_text.size() == 1) {
                const bool is_german = notehead_text == u"H" || (notehead_text == u"B" && mnp.alter());
                head_scheme = is_german ? NoteHeadScheme::HEAD_PITCHNAME_GERMAN : NoteHeadScheme::HEAD_PITCHNAME;
            } else {
                const std::vector<String> names = {u"Do", u"Re", u"Mi", u"Fa", u"Sol", u"La", u"Si"};
                const bool is_fixed = names.at(mnp.step()) == notehead_text;
                head_scheme = is_fixed ? NoteHeadScheme::HEAD_SOLFEGE_FIXED : NoteHeadScheme::HEAD_SOLFEGE;
            }
        } else if (e_.name() == "rest") {
            rest = true;
            measure_rest = e_.ascii_attribute("measure") == "yes";
            mnp.display_step_octave(e_);
        } else if (e_.name() == "staff") {
            bool ok = false;
            String str_staff = e_.read_text();
            staff = pass1_.get_music_xml_part(part_id).staff_number_to_index(str_staff.to_int(&ok));
            if (!ok) {
                // error already reported in pass 1
                staff = -1;
            }
        } else if (e_.name() == "stem") {
            stem_color = Color::from_string(e_.ascii_attribute("color").ascii());
            stem(stem_dir, no_stem);
        } else if (e_.name() == "tie") {
            tie_type = e_.attribute("type");
            e_.skip_current_element();
        } else if (e_.name() == "type") {
            is_small = e_.ascii_attribute("size") == "cue" || e_.ascii_attribute("size") == "grace-cue";
            type = e_.read_text();
        } else if (e_.name() == "voice") {
            voice = e_.read_text();
        } else {
            skip_log_curr_elem();
        }
    }

    // Bug fix for Sibelius 7.1.3 which does not write <voice> for notes with <chord>
    if (!chord) {
        // remember voice
        current_voice = voice;
    } else if (voice.empty()) {
        // use voice from last note w/o <chord>
        voice = current_voice;
    }

    // Assume voice 1 if voice is empty (legal in a single voice part)
    if (voice.empty()) {
        voice = u"1";
    }

    // Define currBeam based on currentVoice to handle multi-voice beaming (and instantiate if not already)
    if (!muse::contains(curr_beams, current_voice)) {
        curr_beams.insert({current_voice, (Beam *) nullptr});
    }
    Beam *&curr_beam = curr_beams[current_voice];

    bm = compute_beam_mode(beam_types);

    // check for timing error(s) and set dura
    // keep in this order as checkTiming() might change dura
    String error_str = mnd.check_timing(type, rest, grace);
    dura = mnd.duration();
    if (!error_str.empty()) {
        logger_->log_error(error_str, &e_);
    }

    IF_ASSERT_FAILED(pass1_.get_part(part_id)) { return nullptr; }

    // At this point all checks have been done, the note should be added
    // note: in case of error exit from here, the postponed <note> children
    // must still be skipped

    int ms_move = 0;
    int ms_track = 0;
    int ms_voice = 0;

    int voice_int = pass1_.voice_to_int(voice);
    if (!pass1_.determine_staff_move_voice(part_id, staff, voice_int, ms_move, ms_track, ms_voice)) {
        logger_->log_debug_info(String(u"could not map staff %1 voice '%2'").arg(staff + 1).arg(voice), &e_);
        add_error(check_at_end_element(e_, u"note"));
        return 0;
    }

    // start time for note:
    // - sTime for non-chord / first chord note
    // - prevTime for others
    Fraction note_start_time = chord ? prev_s_time : s_time;
    Fraction time_mod = mnd.time_mod();

    // determine tuplet state, used twice (before and after note allocation)
    MusicXmlTupletFlags tuplet_action;

    // handle tuplet state for the previous chord or rest
    if (!chord && !grace) {
        Tuplet *tuplet = tuplets[voice];
        MusicXmlTupletState &tuplet_state = tuplet_states[voice];
        tuplet_action = tuplet_state.determine_tuplet_action(mnd.duration(), time_mod, notations.tuplet_desc().type, mnd.normal_type(),
                                                             missing_prev, missing_curr);
        if (tuplet_action & MusicXmlTupletFlag::STOP_PREVIOUS) {
            // tuplet start while already in tuplet
            if (missing_prev.is_valid() && missing_prev > Fraction(0, 1)) {
                const int track = ms_track + ms_voice;
                Rest *const extra_rest =
                    add_rest(score_, measure, note_start_time, track, ms_move, TDuration{missing_prev * tuplet->ratio()}, missing_prev);
                if (extra_rest) {
                    extra_rest->set_tuplet(tuplet);
                    tuplet->add(extra_rest);
                    note_start_time += missing_prev;
                }
            }
            // recover by simply stopping the current tuplet first
            const int normal_notes = time_mod.numerator();
            handle_tuplet_stop(tuplet, normal_notes);
        }
    }

    Chord *c{nullptr};
    ChordRest *cr{nullptr};
    Note *note{nullptr};

    TDuration duration = determine_duration(rest, measure_rest, type, mnd.dots(), dura, measure->ticks());

    Part *part = pass1_.get_part(part_id);
    Instrument *instrument = part->instrument(note_start_time);
    const MusicXmlInstruments &instruments = pass1_.get_instruments(part_id);
    is_single_drumset = instrument->drumset() && instruments.size() == 1;
    // begin allocation
    if (rest) {
        if (!grace) {
            const int track = ms_track + ms_voice;
            cr = add_rest(score_, measure, note_start_time, track, ms_move, duration, dura);
        } else {
            LOGD("ignoring grace rest");
        }
    } else {
        if (!grace) {
            // regular note
            // if there is already a chord just add to it
            // else create a new one
            // this basically ignores <chord/> errors
            c = find_or_create_chord(score_, measure, note_start_time, ms_track + ms_voice, ms_move, duration, dura, bm, is_small || cue);
        } else {
            // grace note
            // TODO: check if explicit stem direction should also be set for grace notes
            // (the DOM parser does that, but seems to have no effect on the autotester)
            if (!chord || gcl.empty()) {
                c = create_grace_chord(score_, ms_track + ms_voice, duration, grace_slash, is_small || cue);
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
        note = Factory::create_note(c);
        const staff_idx_t ottava_staff = (ms_track - pass1_.track_for_part(part_id)) / ::mu::engraving::voices;
        const int octave_shift = pass1_.octave_shift(part_id, ottava_staff, note_start_time);
        const Staff *st = c->staff();
        if (is_single_drumset && mnp.unpitched() && instrument_id.empty()) {
            xml_set_drumset_pitch(note, c, st, mnp.display_step(), mnp.display_octave(), head_group, stem_dir, instrument);
        } else {
            set_pitch(note, instruments, instrument_id, mnp, octave_shift, instrument);
        }
        c->add(note);
        cr = c;
    }
    // end allocation

    if (rest) {
        const track_idx_t track = ms_track + ms_voice;
        if (cr) {
            if (curr_beam) {
                if (curr_beam->track() == track) {
                    cr->set_beam_mode(BeamMode::MID);
                    curr_beam->add(cr);
                } else {
                    remove_beam(curr_beam);
                }
            } else {
                cr->set_beam_mode(BeamMode::NONE);
            }
            cr->set_small(is_small);
            color_item(cr, note_color);
            cr->set_visible(print_object);
            handle_display_step(cr, mnp.display_step(), mnp.display_octave(), note_start_time, score_->style().spatium());
        }
    } else {
        handle_smallness(cue || is_small, note, c);
        note->set_play(!cue); // cue notes don't play
        note->set_head_group(head_group);
        if (head_scheme != NoteHeadScheme::HEAD_AUTO && !mnp.unpitched()) {
            note->set_head_scheme(head_scheme);
        }
        color_item(note, note_color);
        Stem *stem = c->stem();
        if (!stem) {
            stem = Factory::create_stem(c);
            color_item(stem, stem_color);
            if (!stem_color.is_valid()) {
                color_item(stem, note_color);
            }
            c->add(stem);
        }
        set_note_head(note, notehead_filled);
        if (notehead_parentheses) {
            note->set_parentheses_mode(ParenthesesMode::BOTH);
        }
        color_item(note, notehead_color);
        note->set_visible(has_head && print_object);
        stem->set_visible(print_object);

        if (!grace) {
            // regular note
            // handle beam
            if (!chord) {
                handle_beam_and_stem_dir(c, bm, stem_dir, curr_beam, pass1_.has_beaming_info(), beam_color, beam_fan);
            }

            // append any grace chord after chord to the previous chord
            Chord *const prev_chord = measure->find_chord(prev_s_time, ms_track + ms_voice);
            if (prev_chord && prev_chord != c) {
                add_grace_chords_after(prev_chord, gcl, gac);
            }

            // append any grace chord
            add_grace_chords_before(c, gcl);
        }

        if (mnd.calculated_duration().is_valid() && mnd.specified_duration().is_valid() && mnd.calculated_duration().is_not_zero() &&
            mnd.calculated_duration() != mnd.specified_duration()) {
            // convert duration into note length
            Fraction duration_mult{(mnd.specified_duration() / mnd.calculated_duration()).reduced()};
            duration_mult = (1000 * duration_mult).reduced();
            const int note_len = duration_mult.numerator() / duration_mult.denominator();

            NoteEventList nel;
            NoteEvent ne;
            ne.set_len(note_len);
            nel.push_back(ne);
            note->set_play_events(nel);
            if (c) {
                c->set_play_event_type(PlayEventType::USER);
            }
        }

        if (has_dynamics) {
            note->set_user_velocity(velocity);
        }

        if (mnp.unpitched() && !is_single_drumset) {
            set_drumset(c, pass1_, part_id, instrument_id, note_start_time, mnp, stem_dir, head_group);
        }

        // accidental handling
        //LOGD("note acc %p type %hhd acctype %hhd",
        //       acc, acc ? acc->accidentalType() : static_cast<mu::engraving::AccidentalType>(0), accType);
        Accidental *acc = mnp.acc();
        if (!acc && mnp.acc_type() != AccidentalType::NONE) {
            acc = Factory::create_accidental(score_->dummy());
            acc->set_accidental_type(mnp.acc_type());
        }

        if (acc) {
            acc->set_visible(print_object);
            note->add(acc);
            // save alter value for user accidental
            if (acc->accidental_type() != AccidentalType::NONE) {
                alt = mnp.alter();
            }
        }

        c->set_no_stem(no_stem);
    }

    // cr can be 0 here (if a rest cannot be added)
    // TODO: complete and cleanup handling this case
    if (cr) {
        cr->set_visible(print_object);
    }

    // handle notations
    if (cr) {
        notations.add_to_score(cr, note, note_start_time, slurs_, glissandi_, spanners_, trills_, ties_, unstarted_tie_notes_,
                               unended_tie_notes_, arp_map, delayed_arps);

        // if no tie added yet, convert the "tie" into "tied" and add it.
        if (note && !note->tie_for() && !note->tie_back() && !tie_type.empty()) {
            Notation notation = Notation(u"tied");
            const String type2 = u"type";
            notation.add_attribute(type2, tie_type);
            add_tie(notation, note, cr->track(), ties_, unstarted_tie_notes_, unended_tie_notes_, logger_, &e_,
                    configuration()->import_layout());
        }
    }

    if (grace) {
        const bool after_by_steal_time = grace_steal_previous >= 0.0 && c && c->note_type() != NoteType::ACCIACCATURA;
        if (grace_steal_following < 0.0 && (after_by_steal_time || notations.must_stop_grace_after())) {
            // handle grace after state: remember current grace list size
            gac = gcl.size();
        }
    }

    // handle tremolo before handling tuplet (two note tremolos modify timeMod)
    if (cr && notations.has_tremolo()) {
        add_tremolo(cr, notations.tremolo_nr(), notations.tremolo_type(), notations.tremolo_smufl(), notations.tremolo_color(), trem_start_,
                    logger_, &e_, time_mod);
    }

    // handle tuplet state for the current chord or rest
    if (cr) {
        if (!chord && !grace) {
            Tuplet *&tuplet = tuplets[voice];
            // do tuplet if valid time-modification is not 1/1 and is not 1/2 (tremolo)
            // TODO: check interaction tuplet and tremolo handling
            if (time_mod.is_valid() && time_mod != Fraction(1, 1) && time_mod != Fraction(1, 2)) {
                const int actual_notes = time_mod.denominator();
                const int normal_notes = time_mod.numerator();
                if (tuplet_action & MusicXmlTupletFlag::START_NEW) {
                    // create a new tuplet
                    handle_tuplet_start(cr, tuplet, actual_notes, normal_notes, notations.tuplet_desc());
                }
                if (tuplet && tuplet_action & MusicXmlTupletFlag::ADD_CHORD) {
                    cr->set_tuplet(tuplet);
                    tuplet->add(cr);
                }
                if (tuplet && tuplet_action & MusicXmlTupletFlag::STOP_CURRENT) {
                    if (missing_curr.is_valid() && missing_curr > Fraction(0, 1)) {
                        LOGD("add missing %s to current tuplet", muPrintable(missing_curr.to_string()));
                        const int track = ms_track + ms_voice;
                        Rest *const extra_rest = add_rest(score_, measure, note_start_time + dura, track, ms_move,
                                                          TDuration{missing_curr * tuplet->ratio()}, missing_curr);
                        if (extra_rest) {
                            extra_rest->set_tuplet(tuplet);
                            tuplet->add(extra_rest);
                        }
                    }
                    handle_tuplet_stop(tuplet, normal_notes);
                }
            } else if (tuplet) {
                // stop any still incomplete tuplet
                handle_tuplet_stop(tuplet, 2);
            }
        }
    }

    // Add all lyrics from grace notes attached to this chord
    if (c && !c->grace_notes().empty() && !grace_note_lyrics_.empty()) {
        for (GraceNoteLyrics gnl : grace_note_lyrics_) {
            if (gnl.lyric) {
                add_lyric(logger_, &e_, cr, gnl.lyric, gnl.no, extended_lyrics_);
                if (gnl.extend) {
                    extended_lyrics_.add_lyric(gnl.lyric);
                }
            }
        }
        grace_note_lyrics_.clear();
    }

    if (cr) {
        add_inferred_stickings(cr, lyric.inferred_stickings());
    }

    // add lyrics found by lyric
    if (cr && !grace) {
        // add lyrics and stop corresponding extends
        add_lyrics(logger_, &e_, cr, lyric.numbered_lyrics(), lyric.extended_lyrics(), extended_lyrics_);
        if (rest) {
            // stop all extends
            extended_lyrics_.set_extend(-1, cr->track(), cr->tick());
        }
    } else if (c && grace) {
        // Add grace note lyrics to main chord later
        add_grace_note_lyrics(lyric.numbered_lyrics(), lyric.extended_lyrics(), grace_note_lyrics_);
    }

    // add figured bass element
    add_figured_bass_elements(fbl, note_start_time, ms_track, dura, measure);

    // convert to slash or rhythmic notation if needed
    // TODO in the case of slash notation, we assume that given notes do in fact correspond to slash beats
    if (c && measure_style_slash_ != MusicXmlSlash::NONE) {
        c->set_slash(true, measure_style_slash_ == MusicXmlSlash::SLASH);
    }

    // don't count chord or grace note duration
    // note that this does not check the MusicXML requirement that notes in a chord
    // cannot have a duration longer than the first note in the chord
    if (chord || grace) {
        dura.set(0, 1);
    }

    add_error(check_at_end_element(e_, u"note"));

    return note;
}

//---------------------------------------------------------
//   notePrintSpacingNo
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note node for a note with print-spacing="no".
 These are handled like a forward: only moving the time forward.
 */

void MusicXmlParserPass2::note_print_spacing_no(Fraction &dura) {
    //_logger->logDebugTrace("MusicXmlParserPass1::notePrintSpacingNo", &_e);

    bool chord = false;
    bool grace = false;

    while (e_.read_next_start_element()) {
        if (e_.name() == "chord") {
            chord = true;
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "duration") {
            duration(dura);
        } else if (e_.name() == "grace") {
            grace = true;
            e_.skip_current_element(); // skip but don't log
        } else {
            e_.skip_current_element(); // skip but don't log
        }
    }

    // don't count chord or grace note duration
    // note that this does not check the MusicXML requirement that notes in a chord
    // cannot have a duration longer than the first note in the chord
    if (chord || grace) {
        dura.set(0, 1);
    }

    add_error(check_at_end_element(e_, u"note"));
}

//---------------------------------------------------------
//   duration
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/duration node.
 */

void MusicXmlParserPass2::duration(Fraction &dura) {
    dura.set(0, 0); // invalid unless set correctly
    const String element_text = e_.read_text();
    if (element_text.to_int() > 0) {
        dura = pass1_.calc_ticks(element_text.to_int(), divs_, &e_);
    } else {
        logger_->log_error(String(u"illegal duration %1").arg(dura.to_string()), &e_);
    }
    //LOGD("duration %s valid %d", muPrintable(dura.print()), dura.isValid());
}

static FiguredBassItem::Modifier music_xml2_modifier(const String prefix) {
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
    FiguredBassItem *fgi = parent->create_item(idx);

    // read the figure
    while (e_.read_next_start_element()) {
        if (e_.name() == "extend") {
            AsciiStringView type = e_.ascii_attribute("type");
            if (type == "start") {
                fgi->set_cont_line(FiguredBassItem::ContLine::EXTENDED);
            } else if (type == "continue") {
                fgi->set_cont_line(FiguredBassItem::ContLine::EXTENDED);
            } else if (type == "stop") {
                fgi->set_cont_line(FiguredBassItem::ContLine::SIMPLE);
            }
            e_.skip_current_element();
        } else if (e_.name() == "figure-number") {
            const Color color = Color::from_string(e_.ascii_attribute("color").ascii());
            String val = e_.read_text();
            int i_val = val.to_int();
            // MusicXML spec states figure-number is a number
            // MuseScore can only handle single digit
            if (1 <= i_val && i_val <= 9) {
                fgi->set_digit(i_val);
                color_item(fgi, color);
            } else {
                logger_->log_error(String(u"incorrect figure-number '%1'").arg(val), &e_);
            }
        } else if (e_.name() == "prefix") {
            fgi->set_prefix(music_xml2_modifier(e_.read_text()));
        } else if (e_.name() == "suffix") {
            fgi->set_suffix(music_xml2_modifier(e_.read_text()));
        } else {
            skip_log_curr_elem();
        }
    }

    // set parentheses
    if (paren) {
        // parenthesis open
        if (fgi->prefix() != FiguredBassItem::Modifier::NONE) {
            fgi->set_parenth1(FiguredBassItem::Parenthesis::ROUNDOPEN); // before prefix
        } else if (fgi->digit() != ::mu::engraving::fbi_digit_none) {
            fgi->set_parenth2(FiguredBassItem::Parenthesis::ROUNDOPEN); // before digit
        } else if (fgi->suffix() != FiguredBassItem::Modifier::NONE) {
            fgi->set_parenth3(FiguredBassItem::Parenthesis::ROUNDOPEN); // before suffix
        }
        // parenthesis close
        if (fgi->suffix() != FiguredBassItem::Modifier::NONE) {
            fgi->set_parenth4(FiguredBassItem::Parenthesis::ROUNDCLOSED); // after suffix
        } else if (fgi->digit() != ::mu::engraving::fbi_digit_none) {
            fgi->set_parenth3(FiguredBassItem::Parenthesis::ROUNDCLOSED); // after digit
        } else if (fgi->prefix() != FiguredBassItem::Modifier::NONE) {
            fgi->set_parenth2(FiguredBassItem::Parenthesis::ROUNDCLOSED); // after prefix
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

FiguredBass *MusicXmlParserPass2::figured_bass() {
    FiguredBass *fb = Factory::create_figured_bass(score_->dummy());

    const bool parentheses = e_.ascii_attribute("parentheses") == "yes";
    const bool print_object = e_.ascii_attribute("print-object") != "no";
    const String placement = e_.attribute("placement");

    color_item(fb, Color::from_string(e_.ascii_attribute("color").ascii()));
    fb->set_visible(print_object);

    String normalized_text;
    int idx = 0;
    while (e_.read_next_start_element()) {
        if (e_.name() == "duration") {
            Fraction dura;
            duration(dura);
            if (dura.is_valid() && dura > Fraction(0, 1)) {
                fb->set_ticks(dura);
            }
        } else if (e_.name() == "figure") {
            FiguredBassItem *p_item = figure(idx++, parentheses, fb);
            p_item->set_track(0 /* TODO fb->track() */);
            p_item->set_ownership_parent(fb);
            fb->append_item(p_item);
            // add item normalized text
            if (!normalized_text.empty()) {
                normalized_text.append('\n');
            }
            normalized_text.append(p_item->normalized_text());
        } else {
            skip_log_curr_elem();
            delete fb;
            return 0;
        }
    }

    fb->set_xml_text(normalized_text); // this is the text to show while editing

    fb->set_placement(placement == "above" ? PlacementV::ABOVE : PlacementV::BELOW);
    fb->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    fb->reset_property(Pid::OFFSET);

    if (normalized_text.empty()) {
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
    FretDiagram *fd = Factory::create_fret_diagram(score_->dummy());

    color_item(fd, Color::from_string(e_.ascii_attribute("color").ascii()));

    // Format: fret: string
    std::map<int, int> b_starts;
    std::map<int, int> b_ends;

    while (e_.read_next_start_element()) {
        if (e_.name() == "first-fret") {
            bool ok{};
            int val = e_.read_int(&ok);
            if (ok && val > 0) {
                fd->set_fret_offset(val - 1);
            } else {
                logger_->log_error(String(u"FretDiagram::readMusicXml: illegal first-fret %1").arg(val), &e_);
            }
        } else if (e_.name() == "frame-frets") {
            int val = e_.read_int();
            if (val > 0) {
                fd->set_property(Pid::FRET_FRETS, val);
                if (val != fd->property_default(Pid::FRET_FRETS).to_int()) {
                    fd->set_property_flags(Pid::FRET_FRETS, PropertyFlags::UNSTYLED);
                }
            } else {
                logger_->log_error(String(u"FretDiagram::readMusicXml: illegal frame-fret %1").arg(val), &e_);
            }
        } else if (e_.name() == "frame-note") {
            int fret = -1;
            int string = -1;
            int actual_string = -1;
            while (e_.read_next_start_element()) {
                if (e_.name() == "fret") {
                    fret = e_.read_int();
                } else if (e_.name() == "string") {
                    string = e_.read_int();
                    actual_string = fd->strings() - string;
                } else if (e_.name() == "barre") {
                    // Keep barres to be added later
                    String t = e_.attribute("type");
                    if (t == "start") {
                        b_starts[fret] = actual_string;
                    } else if (t == "stop") {
                        b_ends[fret] = actual_string;
                    } else {
                        logger_->log_error(String(u"FretDiagram::readMusicXml: illegal frame-note barre type %1").arg(t), &e_);
                    }
                    skip_log_curr_elem();
                } else {
                    skip_log_curr_elem();
                }
            }
            logger_->log_debug_info(String(u"FretDiagram::readMusicXml string %1 fret %2").arg(string).arg(fret), &e_);

            if (string > 0) {
                if (fret == 0) {
                    fd->set_marker(actual_string, FretMarkerType::CIRCLE);
                } else if (fret > 0) {
                    if (fd->marker(actual_string).mtype == FretMarkerType::CROSS) {
                        fd->set_marker(actual_string, FretMarkerType::NONE);
                    }
                    fd->set_dot(actual_string, fret - fd->fret_offset(), true);
                }
            } else {
                logger_->log_error(String(u"FretDiagram::readMusicXml: illegal frame-note string %1").arg(string), &e_);
            }
        } else if (e_.name() == "frame-strings") {
            int val = e_.read_int();
            if (val > 0) {
                fd->set_strings(val);
                for (int i = 0; i < val; ++i) {
                    // MXML Spec: any string without a dot or other marker has a closed string
                    // cross marker above it.
                    fd->set_marker(i, FretMarkerType::CROSS);
                }
            } else {
                logger_->log_error(String(u"FretDiagram::readMusicXml: illegal frame-strings %1").arg(val), &e_);
            }
        } else {
            skip_log_curr_elem();
        }
    }

    // Finally add barres
    for (auto const &i : b_starts) {
        int fret = i.first;
        int start_string = i.second;

        if (b_ends.find(fret) == b_ends.end()) {
            continue;
        }

        int end_string = b_ends[fret];
        fd->set_barre(start_string, end_string, fret - fd->fret_offset());
    }

    return fd;
}

//---------------------------------------------------------
//   harmony
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/harmony node.
 */

void MusicXmlParserPass2::harmony(const String &part_id, [[maybe_unused]] Measure *measure, const Fraction &s_time,
                                  HarmonyMap &harmony_map) {
    track_idx_t track = pass1_.track_for_part(part_id);

    Staff *staff = score_->staff(track2staff(track));
    Key key = staff ? staff->key(s_time) : Key::INVALID;

    const Color color = Color::from_string(e_.ascii_attribute("color").ascii());
    const String font_family = e_.attribute("font-family");
    const String placement = e_.attribute("placement");
    const bool print_object = e_.ascii_attribute("print-object") != "no";

    String kind, kind_text, function_text, inversion_text, symbols, parens;
    std::vector<HDegree> degree_list;

    FretDiagram *fd = nullptr;
    Harmony *ha = Factory::create_harmony(score_->dummy());
    HarmonyInfo *info = new HarmonyInfo(score_);
    Fraction offset;
    if (!placement.empty()) {
        ha->set_placement(placement == "below" ? PlacementV::BELOW : PlacementV::ABOVE);
        ha->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
        ha->reset_property(Pid::OFFSET);
    }
    while (e_.read_next_start_element()) {
        if (e_.name() == "root") {
            if (info->root_tpc() != Tpc::TPC_INVALID) {
                const ChordDescription *d = harmony_from_xml(info, score_, kind, kind_text, symbols, parens, degree_list);
                info->set_id(d->id);
                info->set_text_name(d->names.front());
                ha->add_chord(info);
                degree_list.clear();
                info = new HarmonyInfo(score_);
            }
            String step;
            int alter = 0;
            bool invalid_root = false;
            while (e_.read_next_start_element()) {
                if (e_.name() == "root-step") {
                    // attributes: print-style
                    step = e_.read_text();
                    if (e_.has_attribute("text")) {
                        if (e_.attribute("text").empty()) {
                            invalid_root = true;
                        }
                    }
                } else if (e_.name() == "root-alter") {
                    // attributes: print-object, print-style
                    //             location (left-right)
                    alter = e_.read_text().trimmed().to_int();
                } else {
                    skip_log_curr_elem();
                }
            }
            if (invalid_root) {
                info->set_root_tpc(Tpc::TPC_INVALID);
            } else {
                info->set_root_tpc(step2tpc(step, AccidentalVal(alter)));
            }
        } else if (e_.name() == "function") {
            // deprecated in MusicXML 4.0
            // attributes: print-style
            info->set_root_tpc(Tpc::TPC_INVALID);
            info->set_bass_tpc(Tpc::TPC_INVALID);
            function_text = e_.read_text();
            ha->set_harmony_type(HarmonyType::ROMAN);
        } else if (e_.name() == "numeral") {
            info->set_root_tpc(Tpc::TPC_INVALID);
            info->set_bass_tpc(Tpc::TPC_INVALID);
            while (e_.read_next_start_element()) {
                if (e_.name() == "numeral-root") {
                    function_text = e_.attribute("text");
                    const String numeral_root = e_.read_text();
                    if (function_text.is_empty() || function_text.at(0).is_digit()) {
                        ha->set_harmony_type(HarmonyType::NASHVILLE);
                        set_harmony_root_tpc_from_function(info, ha, numeral_root, key);
                    } else {
                        ha->set_harmony_type(HarmonyType::ROMAN);
                    }
                } else if (e_.name() == "numeral-alter") {
                    const int alter = e_.read_text().to_int();
                    switch (alter) {
                        case -1: set_harmony_root_tpc_from_function(info, ha, u"b" + harmony_xml_function(info, ha, key), key); break;
                        case 1: set_harmony_root_tpc_from_function(info, ha, u"#" + harmony_xml_function(info, ha, key), key); break;
                        default: break;
                    }
                } else {
                    skip_log_curr_elem();
                }
            }
        } else if (e_.name() == "kind") {
            // attributes: use-symbols  yes-no
            //             text, stack-degrees, parentheses-degree, bracket-degrees,
            //             print-style, halign, valign
            kind_text = e_.attribute("text");
            symbols = e_.attribute("use-symbols");
            parens = e_.attribute("parentheses-degrees");
            kind = e_.read_text();
            if (kind == "none") {
                info->set_root_tpc(Tpc::TPC_INVALID);
            }
        } else if (e_.name() == "inversion") {
            const int inversion = e_.read_text().to_int();
            switch (inversion) {
                case 1: inversion_text = u"6"; break;
                case 2: inversion_text = u"64"; break;
                default: inversion_text = u""; break;
            }
        } else if (e_.name() == "bass") {
            String step;
            int alter = 0;
            while (e_.read_next_start_element()) {
                if (e_.name() == "bass-step") {
                    // attributes: print-style
                    step = e_.read_text();
                } else if (e_.name() == "bass-alter") {
                    // attributes: print-object, print-style
                    //             location (left-right)
                    alter = e_.read_text().trimmed().to_int();
                } else {
                    skip_log_curr_elem();
                }
            }
            info->set_bass_tpc(step2tpc(step, AccidentalVal(alter)));
        } else if (e_.name() == "degree") {
            int degree_value = 0;
            int degree_alter = 0;
            String degree_type;
            while (e_.read_next_start_element()) {
                if (e_.name() == "degree-value") {
                    degree_value = e_.read_int();
                } else if (e_.name() == "degree-alter") {
                    degree_alter = e_.read_text().trimmed().to_int();
                } else if (e_.name() == "degree-type") {
                    degree_type = e_.read_text();
                } else {
                    skip_log_curr_elem();
                }
            }
            if (degree_value <= 0 || degree_value > 13 || degree_alter < -2 || degree_alter > 2 ||
                (degree_type != u"add" && degree_type != u"alter" && degree_type != u"subtract")) {
                logger_->log_error(String(u"incorrect degree: degreeValue=%1 degreeAlter=%2 degreeType=%3")
                                       .arg(degree_value)
                                       .arg(degree_alter)
                                       .arg(degree_type),
                                   &e_);
            } else {
                if (degree_type == "add") {
                    degree_list.emplace_back(degree_value, degree_alter, HDegreeType::ADD);
                } else if (degree_type == "alter") {
                    degree_list.emplace_back(degree_value, degree_alter, HDegreeType::ALTER);
                } else if (degree_type == "subtract") {
                    degree_list.emplace_back(degree_value, degree_alter, HDegreeType::SUBTRACT);
                }
            }
        } else if (e_.name() == "frame") {
            fd = frame();
        } else if (e_.name() == "level") {
            skip_log_curr_elem();
        } else if (e_.name() == "offset") {
            offset = pass1_.calc_ticks(e_.read_int(), divs_, &e_);
            prevent_negative_tick(s_time, offset, logger_);
        } else if (e_.name() == "staff") {
            size_t nstaves = pass1_.get_part(part_id)->nstaves();
            String str_staff = e_.read_text();
            int staff_idx = pass1_.get_music_xml_part(part_id).staff_number_to_index(str_staff.to_int());
            if (staff_idx >= 0 && staff_idx < int(nstaves)) {
                track += staff_idx * ::mu::engraving::voices;
            } else {
                logger_->log_error(String(u"invalid staff %1").arg(str_staff), &e_);
            }
        } else {
            skip_log_curr_elem();
        }
    }

    const ChordDescription *d = nullptr;
    if (info->root_tpc() != Tpc::TPC_INVALID || ha->harmony_type() == HarmonyType::NASHVILLE) {
        d = harmony_from_xml(info, score_, kind, kind_text, symbols, parens, degree_list);
    }
    if (d) {
        info->set_id(d->id);
        info->set_text_name(d->names.front());
    } else {
        info->set_id(-1);
        String text_name = function_text + kind_text + inversion_text;
        info->set_text_name(text_name);
    }
    ha->add_chord(info);

    ha->set_visible(print_object);
    if (placement == u"below") {
        ha->set_placement(PlacementV::BELOW);
        ha->reset_property(Pid::OFFSET);
    }
    color_item(ha, color);
    if (!font_family.empty()) {
        ha->set_family(font_family);
        ha->set_property_flags(Pid::FONT_FACE, PropertyFlags::UNSTYLED);
    }

    const HarmonyDesc new_harmony_desc(track, ha, fd);
    bool insert = true;
    if (pass1_.sib_or_dolet()) {
        const int ticks = (s_time + offset).ticks();
        for (auto itr = harmony_map.begin(); itr != harmony_map.end(); itr++) {
            if (itr->first != ticks) {
                continue;
            }
            HarmonyDesc &found_harmony_desc = itr->second;
            if (track2staff(found_harmony_desc.track_) == track2staff(track) && found_harmony_desc.harmony_ &&
                found_harmony_desc.descr() == info->descr()) {
                if (found_harmony_desc.harmony_ && found_harmony_desc.fret_diagram_visible() == new_harmony_desc.fret_diagram_visible()) {
                    // Matching harmony with matching visibility of fret diagram.  No need to add
                    insert = false;
                } else if (fd && fd->visible() && !found_harmony_desc.fret_diagram_visible()) {
                    // Matching harmony without a fret diagram found at this tick, replace with this harmony and its fret diagram
                    found_harmony_desc.harmony_ = ha;
                    found_harmony_desc.fret_diagram_ = fd;
                    found_harmony_desc.track_ = track;
                    insert = false;
                }
            }
        }
    }

    if (insert) {
        // No harmony at this tick, add to the map
        harmony_map.insert(std::pair<int, HarmonyDesc>((s_time + offset).ticks(), new_harmony_desc));
    }
}

//---------------------------------------------------------
//   beam
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/beam node.
 Collects beamTypes, used in computeBeamMode.
 */

void MusicXmlParserPass2::beam(std::map<int, String> &beam_types) {
    bool has_beam_no;
    int beam_no = e_.ascii_attribute("number").to_int(&has_beam_no);
    String s = e_.read_text();

    beam_types.insert({has_beam_no ? beam_no : 1, s});
}

//---------------------------------------------------------
//   forward
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/forward node.
 */

void MusicXmlParserPass2::forward(Fraction &dura) {
    while (e_.read_next_start_element()) {
        if (e_.name() == "duration") {
            duration(dura);
        } else if (e_.name() == "staff") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "voice") {
            e_.skip_current_element(); // skip but don't log
        } else {
            skip_log_curr_elem();
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
    while (e_.read_next_start_element()) {
        if (e_.name() == "duration") {
            duration(dura);
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   MusicXmlParserLyric
//---------------------------------------------------------

MusicXmlParserLyric::MusicXmlParserLyric(const LyricNumberHandler lyric_number_handler, XmlStreamReader &e, Score *score,
                                         const MusicXmlConfiguration &configuration, MusicXmlLogger *logger, bool is_voice_staff)
    : lyric_number_handler_(lyric_number_handler), e_(e), score_(score), configuration_(configuration), logger_(logger),
      is_voice_staff_(is_voice_staff) {
    // nothing
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserLyric::skip_log_curr_elem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skip_current_element();
}

void MusicXmlParserLyric::read_elision(String &formatted_text) {
    const String text = e_.read_text();
    const String smufl = e_.attribute("smufl");
    if (!text.empty()) {
        formatted_text += text;
    } else if (!smufl.empty()) {
        formatted_text += u"<sym>" + smufl + u"</sym>";
    } else {
        formatted_text += u"<sym>lyricsElision</sym>";
    }
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

void MusicXmlParserLyric::parse(bool visibility) {
    bool has_extend = false;
    const String lyric_number = e_.attribute("number");
    const Color lyric_color = Color::from_string(e_.ascii_attribute("color").ascii());
    const bool print_lyric = visibility ? e_.ascii_attribute("print-object") != "no" : e_.ascii_attribute("print-object") == "yes";
    placement_ = e_.attribute("placement");
    double rel_x = e_.double_attribute("relative-x") * 0.1 * ::mu::engraving::dpmm;
    relative_y_ = e_.double_attribute("relative-y") * -0.1 * ::mu::engraving::dpmm;
    default_y_ = e_.double_attribute("default-y") * -0.1 * ::mu::engraving::dpmm;
    LyricsSyllabic syllabic = LyricsSyllabic::SINGLE;

    String extend_type;
    String formatted_text;

    while (e_.read_next_start_element()) {
        if (e_.name() == "elision") {
            read_elision(formatted_text);
        } else if (e_.name() == "extend") {
            has_extend = true;
            extend_type = e_.attribute("type");
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "syllabic") {
            String syll = e_.read_text();
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
            formatted_text += xmlpass2::next_part_of_formatted_string(e_, configuration_, *score_->engraving_fonts());
        } else {
            skip_log_curr_elem();
        }
    }

    // if no lyric read (e.g. only 'extend "type=stop"'), no further action required
    if (formatted_text.empty()) {
        return;
    }

    const int lyric_no = lyric_number_handler_.get_lyric_no(lyric_number);
    if (lyric_no < 0) {
        logger_->log_error(u"invalid lyrics number (<0)", &e_);
        return;
    } else if (lyric_no > ::mu::iex::musicxml::max_lyrics) {
        logger_->log_error(String(u"too much lyrics (>%1)").arg(::mu::iex::musicxml::max_lyrics), &e_);
        return;
    } else if (muse::contains(numbered_lyrics_, lyric_no)) {
        logger_->log_error(String(u"duplicate lyrics number (%1)").arg(lyric_number), &e_);
        return;
    }

    TextBase *item = nullptr;
    if (is_likely_sticking(formatted_text, syllabic, has_extend)) {
        item = Factory::create_sticking(score_->dummy());
    } else {
        item = Factory::create_lyrics(score_->dummy());
    }

    //LOGD("formatted lyric '%s'", muPrintable(formattedText));
    item->set_xml_text(formatted_text);
    color_item(item, lyric_color);
    item->set_visible(print_lyric);

    item->set_placement(placement() == "above" ? PlacementV::ABOVE : PlacementV::BELOW);
    item->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    item->reset_property(Pid::OFFSET);

    if (!real_is_null(rel_x)) {
        PointF offset = item->offset();
        offset.set_x(rel_x);
        item->set_offset(offset);
        item->set_property_flags(Pid::OFFSET, PropertyFlags::UNSTYLED);
    }

    if (item->is_lyrics()) {
        // Add lyric
        Lyrics *l = to_lyrics(item);
        l->set_syllabic(syllabic);
        numbered_lyrics_[lyric_no] = l;

        if (has_extend && (extend_type.empty() || extend_type == "start") &&
            (l->syllabic() == LyricsSyllabic::SINGLE || l->syllabic() == LyricsSyllabic::END)) {
            extended_lyrics_.insert(l);
        }
    } else if (item->is_sticking()) {
        // Add sticking
        Sticking *s = to_sticking(item);
        inferred_stickings_.push_back(s);
    }
}

String MusicXmlParserLyric::placement() const {
    if (placement_ == "" && has_total_y()) {
        return total_y() < 0 ? u"above" : u"below";
    } else {
        return placement_;
    }
}

bool MusicXmlParserLyric::is_likely_sticking(const String &text, const LyricsSyllabic syllabic, const bool has_extend) {
    if (!configuration_.infer_text_type()) {
        return false;
    }
    String plain_words = MScoreTextToMusicXml::to_plain_text(text.simplified());
    static const std::wregex sticking(L"^[lrbLRB]+$");
    return plain_words.contains(sticking) && syllabic == LyricsSyllabic::SINGLE && !has_extend && !is_voice_staff_;
}

//---------------------------------------------------------
//   slur
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/slur node.
 */

void MusicXmlParserNotations::slur() {
    Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"notations");
    notation.set_visible(visible_);
    notations_.push_back(notation);

    // any grace note containing a slur stop means
    // last note of a grace after set has been found
    // -> remember slur stop
    if (notation.attribute(u"type") == u"stop") {
        slur_stop_ = true;
    } else if (notation.attribute(u"type") == u"start") {
        slur_start_ = true;
    }

    e_.skip_current_element(); // skip but don't log
}

//---------------------------------------------------------
//   addSlur
//---------------------------------------------------------

static void add_slur(const Notation &notation, SlurStack &slurs, ChordRest *cr, Note *note, const Fraction &tick, MusicXmlLogger *logger,
                     const XmlStreamReader *const xmlreader, bool import_layout) {
    int slur_no = notation.attribute(u"number").to_int();
    if (slur_no > 0) {
        slur_no--;
    }
    const String slur_type = notation.attribute(u"type");

    const track_idx_t track = cr->track();
    Score *score = cr->score();

    // PriMus Music-Notation by Columbussoft (build 10093) generates overlapping
    // slurs that do not have a number attribute to distinguish them.
    // The duplicates must be ignored, to prevent memory allocation issues,
    // which caused a MuseScore crash
    // Similar issues happen with Sibelius 7.1.3 (direct export)

    if (slur_type == u"start") {
        if (slurs[slur_no].is_start()) {
            // slur start when slur already started: report error
            logger->log_error(String(u"ignoring duplicate slur start"), xmlreader);
        } else if (slurs[slur_no].is_stop()) {
            // slur start when slur already stopped: wrap up
            Slur *new_slur = slurs[slur_no].slur();
            slurs[slur_no] = SlurDesc();

            if (new_slur->end_element() == cr && note) {
                delete new_slur;

                // Slur starts & ends on same chord - add lv instead
                LaissezVib *lv_tie = Factory::create_laissez_vib(note);
                note->score()->undo_add_element(lv_tie);
                return;
            }
            const Fraction tick2 = new_slur->end_element()->tick();
            if (tick2 < tick) {
                logger->log_error(String(u"slur end is before slur start"), xmlreader);
                delete new_slur;
                return;
            }
            new_slur->set_track(track);
            new_slur->set_tick(tick);
            new_slur->set_start_element(cr);
            new_slur->set_tick2(tick2);
            score->add_element(new_slur);
        } else {
            // slur start for new slur: init
            Slur *new_slur = notation.name() == "slur" ? Factory::create_slur(score->dummy()) :
                                                         to_slur(Factory::create_hammer_on_pull_off(score->dummy()));
            const String line_type = notation.attribute(u"line-type");
            if (line_type == u"dashed") {
                new_slur->set_style_type(SlurStyleType::DASHED);
            } else if (line_type == u"dotted") {
                new_slur->set_style_type(SlurStyleType::DOTTED);
            } else if (line_type == u"solid" || line_type.empty()) {
                new_slur->set_style_type(SlurStyleType::SOLID);
            }
            new_slur->set_visible(notation.visible());
            color_item(new_slur, Color::from_string(notation.attribute(u"color")));
            new_slur->set_tick(tick);
            new_slur->set_start_element(cr);
            if (import_layout) {
                const String orientation = notation.attribute(u"orientation");
                const String placement = notation.attribute(u"placement");
                if (orientation == u"over" || placement == u"above") {
                    new_slur->set_slur_direction(DirectionV::UP);
                } else if (orientation == u"under" || placement == u"below") {
                    new_slur->set_slur_direction(DirectionV::DOWN);
                } else if (!orientation.empty() || !placement.empty()) {
                    logger->log_error(String(u"unknown slur orientation/placement: %1/%2").arg(orientation).arg(placement), xmlreader);
                }
            }
            new_slur->set_track(track);
            new_slur->set_track2(track);
            slurs[slur_no].start(new_slur);
        }
    } else if (slur_type == u"stop") {
        if (slurs[slur_no].is_start()) {
            // slur stop when slur already started: wrap up
            Slur *new_slur = slurs[slur_no].slur();
            slurs[slur_no] = SlurDesc();

            if (new_slur->start_element() == cr && note) {
                delete new_slur;

                // Slur starts & ends on same chord - add lv instead
                LaissezVib *lv_tie = Factory::create_laissez_vib(note);
                note->score()->undo_add_element(lv_tie);
                return;
            }

            if (!(cr->is_grace())) {
                new_slur->set_tick2(tick);
                new_slur->set_track2(track);
            }
            new_slur->set_end_element(cr);
            score->add_element(new_slur);
        } else if (slurs[slur_no].is_stop()) {
            // slur stop when slur already stopped: report error
            logger->log_error(String(u"ignoring duplicate slur stop"), xmlreader);
        } else {
            // slur stop for new slur: init
            Slur *new_slur = Factory::create_slur(score->dummy());
            if (!(cr->is_grace())) {
                new_slur->set_track2(track);
            }
            new_slur->set_end_element(cr);
            slurs[slur_no].stop(new_slur);
        }
    } else if (slur_type == "continue") {
        // ignore
    } else {
        logger->log_error(String(u"unknown slur type %1").arg(slur_type), xmlreader);
    }
}

//---------------------------------------------------------
//   tied
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/tied node.
 */

void MusicXmlParserNotations::tied() {
    Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"notations");
    notation.set_visible(visible_);
    // Make sure "stops" get processed before "starts"
    if (notation.attribute(u"type") == u"stop") {
        notations_.insert(notations_.begin(), notation);
    } else {
        notations_.push_back(notation);
    }
    String tied_type = notation.attribute(u"type");
    if (tied_type != u"start" && tied_type != u"stop" && tied_type != u"let-ring") {
        logger_->log_error(String(u"unknown tied type %1").arg(tied_type), &e_);
    }

    e_.skip_current_element(); // skip but don't log
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
    const MusicXmlDynamic dynamic = parse_dynamics_container(e_, logger_);
    if (!dynamic.text.empty()) {
        dynamics_list_.push_back(dynamic);
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
    while (e_.read_next_start_element()) {
        SymId id{SymId::NO_SYM};
        if (convert_articulation_to_sym_id(String::from_ascii(e_.name().ascii()), id)) {
            if (e_.name() == "detached-legato") {
                Notation artic =
                    Notation::notation_with_attributes(u"tenuto", e_.attributes(), u"articulations", SymId::ARTIC_TENUTO_ABOVE);
                artic.set_visible(visible_);
                notations_.push_back(artic);
                artic = Notation::notation_with_attributes(u"staccato", e_.attributes(), u"articulations", SymId::ARTIC_STACCATO_ABOVE);
                artic.set_visible(visible_);
                notations_.push_back(artic);
            } else {
                Notation artic =
                    Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"articulations", id);
                artic.set_visible(visible_);
                notations_.push_back(artic);
            }
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "breath-mark") {
            std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
            String value = e_.read_text();
            engraving::SymId breath = SymId::NO_SYM;
            if (value == "tick") {
                breath = SymId::BREATH_MARK_TICK;
            } else if (value == "upbow") {
                breath = SymId::BREATH_MARK_UPBOW;
            } else if (value == "salzedo") {
                breath = SymId::BREATH_MARK_SALZEDO;
            } else {
                // Use comma as the default symbol
                breath = SymId::BREATH_MARK_COMMA;
            }
            Notation notation = Notation::notation_with_attributes(u"breath", attributes, u"articulations", breath);
            notation.set_visible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "caesura") {
            std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
            String value = e_.read_text();
            engraving::SymId caesura = SymId::NO_SYM;
            if (value == "curved") {
                caesura = SymId::CAESURA_CURVED;
            } else if (value == "short") {
                caesura = SymId::CAESURA_SHORT;
            } else if (value == "thick") {
                caesura = SymId::CAESURA_THICK;
            } else if (value == "single") {
                caesura = SymId::CAESURA_SINGLE_STROKE;
            } else { // Use as the default symbol
                caesura = SymId::CAESURA;
            }
            Notation notation = Notation::notation_with_attributes(u"breath", attributes, u"articulations", caesura);
            notation.set_visible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "doit" || e_.name() == "falloff" || e_.name() == "plop" || e_.name() == "scoop") {
            Notation artic = Notation::notation_with_attributes(u"chord-line", e_.attributes(), u"articulations");
            artic.set_sub_type(String::from_ascii(e_.name().ascii()));
            artic.set_visible(visible_);
            notations_.push_back(artic);
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "other-articulation") {
            const String smufl = e_.attribute("smufl");
            SymId sid = SymNames::sym_id_by_name(smufl, SymId::NO_SYM);
            Notation artic =
                Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"articulations", sid);
            const String artic_text = e_.read_text();
            artic.set_text(artic_text);

            if (!smufl.empty() || !artic_text.empty()) {
                artic.set_visible(visible_);
                notations_.push_back(artic);
            }
        } else {
            skip_log_curr_elem();
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
    bool trill_mark = false;
    // <trill-mark placement="above"/>
    while (e_.read_next_start_element()) {
        SymId id{SymId::NO_SYM};
        if (convert_articulation_to_sym_id(String::from_ascii(e_.name().ascii()), id)) {
            Notation notation =
                Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"ornaments", id);
            notation.set_visible(visible_);
            notations_.push_back(notation);
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "trill-mark") {
            trill_mark = true;
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "wavy-line") {
            bool wavy_line_type_was_start = (wavy_line_type_ == "start");
            wavy_line_type_ = e_.attribute("type");
            wavy_line_no_ = e_.int_attribute("number");
            if (wavy_line_no_ > 0) {
                wavy_line_no_--;
            }
            // any grace note containing a wavy-line stop means
            // last note of a grace after set has been found
            // remember wavy-line stop
            if (wavy_line_type_ == u"stop") {
                wavy_line_stop_ = true;
            }
            // check for start and stop on same note
            if (wavy_line_type_was_start && wavy_line_type_ == u"stop") {
                wavy_line_type_ = u"startstop";
            }
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "tremolo") {
            has_tremolo_ = true;
            tremolo_color_ = Color::from_string(e_.attribute("color"));
            tremolo_type_ = e_.attribute("type");
            tremolo_smufl_ = e_.attribute("smufl");
            tremolo_nr_ = e_.read_int();
        } else if (e_.name() == "inverted-mordent" || e_.name() == "mordent") {
            mordent_normal_or_inverted();
        } else if (e_.name() == "other-ornament") {
            Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"ornaments");
            notation.set_visible(visible_);
            notations_.push_back(notation);
            e_.skip_current_element(); // skip but don't log
        } else {
            skip_log_curr_elem();
        }
    }

    // note that mscore wavy line already implicitly includes a trillsym
    // so don't add an additional one
    if (trill_mark && wavy_line_type_ != "start" && wavy_line_type_ != "startstop") {
        Notation ornament = Notation::notation_with_attributes(u"trill-mark", e_.attributes(), u"ornaments", SymId::ORNAMENT_TRILL);
        ornament.set_visible(visible_);
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
    while (e_.read_next_start_element()) {
        SymId id{SymId::NO_SYM};
        const String smufl = e_.attribute("smufl");
        if (!smufl.empty()) {
            id = SymNames::sym_id_by_name(smufl, SymId::NO_SYM);
            Notation notation =
                Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"technical", id);
            notation.set_visible(visible_);
            notations_.push_back(notation);
            e_.skip_current_element();
        } else if (convert_articulation_to_sym_id(String::from_ascii(e_.name().ascii()), id)) {
            Notation notation =
                Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"technical", id);
            notation.set_visible(visible_);
            notations_.push_back(notation);
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "fingering" || e_.name() == "fret" || e_.name() == "pluck" || e_.name() == "string" || e_.name() == "tap") {
            Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"technical");
            notation.set_text(e_.read_text());
            notation.set_visible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "harmonic") {
            harmonic();
        } else if (e_.name() == "handbell") {
            const std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
            convert_articulation_to_sym_id(e_.read_text(), id);
            Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), attributes, u"technical", id);
            notation.set_visible(visible_);
            notations_.push_back(notation);
        } else if (e_.name() == "harmon-mute") {
            harmon_mute();
        } else if (e_.name() == "hole") {
            hole();
        } else if (e_.name() == "tap") {
            id = (e_.attribute("hand") == u"left") ? SymId::GUITAR_LEFT_HAND_TAPPING : SymId::GUITAR_RIGHT_HAND_TAPPING;
            Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"technical");
            notation.set_visible(visible_);
            notations_.push_back(notation);
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "other-technical") {
            other_technical();
        } else {
            skip_log_curr_elem();
        }
    }
}

void MusicXmlParserNotations::other_technical() {
    const Color color = Color::from_string(e_.attribute("color"));
    const String text = e_.read_text();

    if (text == u"z") {
        // Buzz roll
        has_tremolo_ = true;
        tremolo_nr_ = 0;
        tremolo_type_ = u"unmeasured";
        tremolo_color_ = color;
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
        Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"technical", SymId::STRINGS_HARMONIC);

    while (e_.read_next_start_element()) {
        String name = String::from_ascii(e_.name().ascii());
        if (name == "natural") {
            notation.set_sub_type(name);
            notation.set_visible(visible_);
            e_.skip_current_element();     // skip but don't log
        } else if (name == "artificial") { // TODO: add artificial harmonic when supported by MuseScore
            logger_->log_error(String(u"unsupported harmonic type/pitch '%1'").arg(name), &e_);
            notation.set_sym_id(SymId::NO_SYM);
            e_.skip_current_element();
        } else {
            e_.skip_current_element();
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

void MusicXmlParserNotations::harmon_mute() {
    engraving::SymId mute = SymId::BRASS_HARMON_MUTE_CLOSED;
    const std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
    while (e_.read_next_start_element()) {
        String name = String::from_ascii(e_.name().ascii());
        if (name == "harmon-closed") {
            const String location = e_.attribute("location");
            String value = e_.read_text();
            if (value == "yes") {
                mute = SymId::BRASS_HARMON_MUTE_CLOSED;
            } else if (value == "no") {
                mute = SymId::BRASS_HARMON_MUTE_STEM_OPEN;
            } else if (value == "half") {
                if (location == "left") {
                    mute = SymId::BRASS_HARMON_MUTE_STEM_HALF_LEFT;
                } else if (location == "right") {
                    mute = SymId::BRASS_HARMON_MUTE_STEM_HALF_RIGHT;
                } else {
                    logger_->log_error(String(u"unsupported harmon-closed location '%1'").arg(location), &e_);
                    mute = SymId::BRASS_HARMON_MUTE_STEM_HALF_LEFT;
                }
            }
        } else {
            e_.skip_current_element();
        }
    }
    Notation notation = Notation::notation_with_attributes(u"harmon-closed", attributes, u"technical", mute);
    notation.set_visible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   hole
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/technical/hole node.
 */

void MusicXmlParserNotations::hole() {
    engraving::SymId hole = SymId::NO_SYM;
    const std::vector<XmlStreamReader::Attribute> attributes = e_.attributes();
    while (e_.read_next_start_element()) {
        if (e_.name() == "hole-closed") {
            const String location = e_.attribute("location");
            const String value = e_.read_text();
            if (value == "yes") {
                hole = SymId::WIND_CLOSED_HOLE;
            } else if (value == "no") {
                hole = SymId::WIND_OPEN_HOLE;
            } else if (value == "half") {
                if (location == "bottom") {
                    hole = SymId::WIND_HALF_CLOSED_HOLE2;
                } else if (location == "right") {
                    hole = SymId::WIND_HALF_CLOSED_HOLE1;
                } else {
                    logger_->log_error(String(u"unsupported hole-closed location '%1'").arg(location), &e_);
                    hole = SymId::WIND_HALF_CLOSED_HOLE3;
                }
            }
        } else {
            e_.skip_current_element();
        }
    }
    Notation notation = Notation::notation_with_attributes(u"hole-closed", attributes, u"technical", hole);
    notation.set_visible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   addTechnical
//---------------------------------------------------------

void MusicXmlParserNotations::add_technical(const Notation &notation, Note *note) {
    const String placement = notation.attribute(u"placement");
    const String font_weight = notation.attribute(u"font-weight");
    const double font_size = notation.attribute(u"font-size").to_double();
    const String font_style = notation.attribute(u"font-style");
    const String font_family = notation.attribute(u"font-family");
    const Color color = Color::from_string(notation.attribute(u"color"));
    if (notation.name() == u"fingering") {
        // TODO: distinguish between keyboards (style TextStyleName::FINGERING)
        // and (plucked) strings (style TextStyleName::LH_GUITAR_FINGERING)
        add_text_to_note(e_.byte_offset(), notation.text(), placement, font_weight, font_size, font_style, font_family, notation.visible(),
                         color, TextStyleType::FINGERING, score_, note, pass2_.configuration()->need_use_default_font());
    } else if (notation.name() == u"fret") {
        int fret = notation.text().to_int();
        if (note) {
            if (note->staff()->is_tab_staff(Fraction(0, 1))) {
                note->set_fret(fret);
            }
        } else {
            logger_->log_error(u"no note for fret", &e_);
        }
    } else if (notation.name() == "pluck") {
        add_text_to_note(e_.byte_offset(), notation.text(), placement, font_weight, font_size, font_style, font_family, notation.visible(),
                         color, TextStyleType::RH_GUITAR_FINGERING, score_, note, pass2_.configuration()->need_use_default_font());
    } else if (notation.name() == "string") {
        if (note) {
            if (note->staff()->is_tab_staff(Fraction(0, 1))) {
                note->set_string(notation.text().to_int() - 1);
            } else {
                add_text_to_note(e_.byte_offset(), notation.text(), placement, font_weight, font_size, font_style, font_family,
                                 notation.visible(), color, TextStyleType::STRING_NUMBER, score_, note,
                                 pass2_.configuration()->need_use_default_font());
            }
        } else {
            logger_->log_error(u"no note for string", &e_);
        }
    } else if (notation.name() == u"tap") {
        if (note) {
            add_tap_to_chord(notation, note->chord());
        } else {
            logger_->log_error(u"no note for tap", &e_);
        }
    }
}

void MusicXmlParserNotations::arpeggio() {
    arpeggio_type_ = e_.attribute("direction");
    if (arpeggio_type_.empty()) {
        arpeggio_type_ = u"none";
    }
    arpeggio_no_ = e_.int_attribute("number");
    if (arpeggio_no_ == 0) {
        arpeggio_no_ = 1;
    }
    arpeggio_color_ = Color::from_string(e_.attribute("color"));
    e_.skip_current_element(); // skip but don't log
}

//---------------------------------------------------------
//   mordentNormalOrInverted
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/ornaments/mordent
 and /score-partwise/part/measure/note/notations/ornaments/inverted-mordent nodes.
 */

void MusicXmlParserNotations::mordent_normal_or_inverted() {
    Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"ornaments");
    notation.set_text(e_.read_text());
    notation.set_visible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   glissandoSlide
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/glissando
 and /score-partwise/part/measure/note/notations/slide nodes.
 */

void MusicXmlParserNotations::glissando_slide() {
    Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"notations");
    notation.set_text(e_.read_text());
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   addGlissandoSlide
//---------------------------------------------------------

static void add_glissando_slide(const Notation &notation, Note *note, Glissando *glissandi[::mu::iex::musicxml::max_number_level][2],
                                MusicXmlSpannerMap &spanners, MusicXmlLogger *logger, const XmlStreamReader *const xmlreader) {
    int glissando_number = notation.attribute(u"number").to_int();
    if (glissando_number > 0) {
        glissando_number--;
    }
    const String glissando_type = notation.attribute(u"type");
    const bool glissando_tag = notation.name() != u"slide";
    const String line_type = notation.attribute(u"line-type");
    Glissando *&gliss = glissandi[glissando_number][glissando_tag];

    const Fraction tick = note->tick();
    const track_idx_t track = note->track();

    if (glissando_type == u"start") {
        const String glissando_text = notation.text();
        if (gliss) {
            logger->log_error(String(u"overlapping glissando/slide number %1").arg(glissando_number + 1), xmlreader);
        } else if (!note) {
            logger->log_error(String(u"no note for glissando/slide number %1 start").arg(glissando_number + 1), xmlreader);
        } else {
            gliss = Factory::create_glissando(note);
            gliss->set_start_element(note);
            gliss->set_tick(tick);
            gliss->set_track(track);
            gliss->set_visible(notation.visible());
            color_item(gliss, Color::from_string(notation.attribute(u"color")));
            if (line_type == u"dashed") {
                gliss->set_line_style(LineType::DASHED);
            } else if (line_type == u"dotted") {
                gliss->set_line_style(LineType::DOTTED);
            } else if (line_type == u"solid" || line_type.empty()) {
                gliss->set_line_style(LineType::SOLID);
            }
            gliss->set_text(glissando_text);
            gliss->set_glissando_type(glissando_tag || (line_type == u"wavy") ? GlissandoType::WAVY : GlissandoType::STRAIGHT);
            const GlissandoStyle instrument_style = note->part()->instrument(tick)->glissando_style();
            if (instrument_style != gliss->glissando_style()) {
                gliss->set_glissando_style(instrument_style);
                // the instrument's style, not the score's: keep it when the score is saved
                gliss->set_property_flags(Pid::GLISS_STYLE, PropertyFlags::UNSTYLED);
            }
            spanners[gliss] = std::pair<int, int>(tick.ticks(), -1);
            // LOGD("glissando/slide=%p inserted at first tick %d", gliss, tick);
        }
    } else if (glissando_type == u"stop") {
        if (!gliss) {
            logger->log_error(String(u"glissando/slide number %1 stop without start").arg(glissando_number + 1), xmlreader);
        } else if (!note) {
            logger->log_error(String(u"no note for glissando/slide number %1 stop").arg(glissando_number + 1), xmlreader);
        } else if (gliss->tick() == tick) {
            // A gliss starting and ending on the same tick is a fall
            ChordLine *cl = Factory::create_chord_line(note->chord());
            cl->set_chord_line_type(ChordLineType::FALL);
            cl->set_wavy(gliss->glissando_type() == GlissandoType::WAVY ? true : false);
            cl->set_straight(true);
            cl->set_ownership_parent(note);
            note->chord()->add(cl);
            spanners.erase(gliss);
            delete gliss;
            gliss = nullptr;
        } else {
            // Remove glissandos longer than 16 beats
            if ((tick + note->chord()->ticks()) - gliss->tick() >= Fraction(4, 1)) {
                logger->log_error(String(u"glissando/slide number %1 too long (%2)")
                                      .arg(glissando_number + 1, ((tick + note->chord()->ticks()) - gliss->tick()).ticks()),
                                  xmlreader);
                spanners.erase(gliss);
            } else {
                spanners[gliss].second = tick.ticks() + note->chord()->ticks().ticks();
                gliss->set_end_element(note);
                gliss->set_tick2(tick);
                gliss->set_track2(track);
                // LOGD("glissando/slide=%p second tick %d", gliss, tick);
            }
            gliss = nullptr;
        }
    } else {
        logger->log_error(String(u"unknown glissando/slide type %1").arg(glissando_type), xmlreader);
    }
}

//---------------------------------------------------------
//   addArpeggio
//---------------------------------------------------------

static void add_arpeggio(ChordRest *cr, String &arpeggio_type, int arpeggio_no, Color arpeggio_color, ArpeggioMap &arp_map,
                         DelayedArpMap &delayed_arps) {
    if (cr->is_rest() && !arpeggio_type.empty()) {
        // If the arpeggio is attached to a rest, store to add to the next available chord
        DelayedArpeggio delayed_arp(arpeggio_type, arpeggio_no);
        delayed_arps.insert(std::pair<int, DelayedArpeggio>(cr->tick().ticks(), delayed_arp));
    } else {
        // Retrieve stored arpeggio to add to this chord
        DelayedArpeggio delayed_arp = muse::value(delayed_arps, cr->tick().ticks(), DelayedArpeggio(u"", 0));
        if (!delayed_arp.arpeggio_type_.empty()) {
            arpeggio_type = delayed_arp.arpeggio_type_;
            arpeggio_no = delayed_arp.arpeggio_no_;
            delayed_arps.erase(cr->tick().ticks());
        }
    }

    // If no current arpeggio with same number add new
    // If not, expand span
    // no support for arpeggio on rest
    const std::vector<MusicXmlArpeggioDesc> arps = muse::values(arp_map, cr->tick().ticks());
    Arpeggio *cur_arp = nullptr;
    for (const MusicXmlArpeggioDesc arp : arps) {
        if (arp.no == arpeggio_no) {
            cur_arp = arp.arp;
        }
    }

    if (cur_arp) {
        track_idx_t chord_track = cr->track();
        track_idx_t arp_track = cur_arp->track();
        track_idx_t span = cr->track() - cur_arp->track();
        if (chord_track > arp_track && span != 0) {
            cur_arp->set_span(static_cast<int>(span + 1));
        }
    } else {
        if (!arpeggio_type.empty() && cr->is_chord()) {
            Arpeggio *arpeggio = Factory::create_arpeggio(mu::engraving::to_chord(cr));
            arpeggio->set_arpeggio_type(ArpeggioType::NORMAL);
            if (arpeggio_type == "up") {
                arpeggio->set_arpeggio_type(ArpeggioType::UP);
            } else if (arpeggio_type == "down") {
                arpeggio->set_arpeggio_type(ArpeggioType::DOWN);
            } else if (arpeggio_type == "non-arpeggiate") {
                arpeggio->set_arpeggio_type(ArpeggioType::BRACKET);
            }
            color_item(arpeggio, arpeggio_color);
            // there can be only one
            if (!(to_chord(cr))->arpeggio()) {
                cr->add(arpeggio);

                MusicXmlArpeggioDesc arp_desc(arpeggio, arpeggio_no);
                arp_map.insert(std::pair<int, MusicXmlArpeggioDesc>(cr->tick().ticks(), arp_desc));
            }
        }
    }
}

//---------------------------------------------------------
//   addTie
//---------------------------------------------------------

static void add_tie(const Notation &notation, Note *note, const track_idx_t track, MusicXmlTieMap &ties,
                    std::vector<Note *> &unstarted_tie_notes, std::vector<Note *> &unended_tie_notes, MusicXmlLogger *logger,
                    const XmlStreamReader *const xmlreader, bool import_layout) {
    IF_ASSERT_FAILED(note) { return; }

    const String type = notation.attribute(u"type");
    const String orientation = notation.attribute(u"orientation");
    const String placement = notation.attribute(u"placement");
    const String line_type = notation.attribute(u"line-type");

    TieLocation loc = TieLocation(note->pitch(), note->track());

    if (type.empty()) {
        // ignore, nothing to do
    } else if (type == u"start") {
        if (Tie *active_tie = muse::value(ties, loc, nullptr)) {
            logger->log_error(String(u"Tie already active"), xmlreader);
            unended_tie_notes.push_back(active_tie->start_note());
            ties.erase(loc);
        }
        ties[loc] = Factory::create_tie(note);
        Tie *curr_tie = ties[loc];
        note->set_tie_for(curr_tie);
        curr_tie->set_start_note(note);
        curr_tie->set_track(track);
        curr_tie->set_visible(notation.visible());
        color_item(curr_tie, Color::from_string(notation.attribute(u"color")));

        if (import_layout) {
            if (orientation == u"over" || placement == u"above") {
                curr_tie->set_slur_direction(DirectionV::UP);
            } else if (orientation == u"under" || placement == u"below") {
                curr_tie->set_slur_direction(DirectionV::DOWN);
            } else if (!orientation.empty() || !placement.empty()) {
                logger->log_error(String(u"unknown tied orientation/placement: %1/%2").arg(orientation).arg(placement), xmlreader);
            }
        }

        if (line_type == u"dashed") {
            curr_tie->set_style_type(SlurStyleType::DASHED);
        } else if (line_type == u"dotted") {
            curr_tie->set_style_type(SlurStyleType::DOTTED);
        } else if (line_type == u"solid" || line_type.empty()) {
            curr_tie->set_style_type(SlurStyleType::SOLID);
        }
        curr_tie = nullptr;
    } else if (type == "stop") {
        if (Tie *curr_tie = muse::value(ties, loc, nullptr)) {
            const Note *start_note = curr_tie->start_note();
            const Chord *start_chord = start_note ? start_note->chord() : nullptr;
            const Chord *end_chord = note->chord();
            const Measure *start_measure = start_chord ? start_chord->measure() : nullptr;
            if (start_measure == end_chord->measure() ||
                (start_chord && start_chord->tick() + start_chord->measure()->ticks() >= end_chord->tick())) {
                // only connect if they're in the same bar or no further than a full measure apart
                curr_tie->set_end_note(note);
                note->set_tie_back(curr_tie);
            } else {
                logger->log_error(String(u"Intervening note in voice"), xmlreader);
                unstarted_tie_notes.push_back(note);
                unended_tie_notes.push_back(curr_tie->start_note());
            }
            ties.erase(loc);
        } else {
            unstarted_tie_notes.push_back(note);
            logger->log_error(String(u"Non-started tie terminated. No-op."), xmlreader);
        }
    } else if (type == "let-ring") {
        LaissezVib *lv_tie = Factory::create_laissez_vib(note);
        lv_tie->set_visible(notation.visible());
        color_item(lv_tie, Color::from_string(notation.attribute(u"color")));

        if (import_layout) {
            if (orientation == u"over" || placement == u"above") {
                lv_tie->set_slur_direction(DirectionV::UP);
            } else if (orientation == u"under" || placement == u"below") {
                lv_tie->set_slur_direction(DirectionV::DOWN);
            } else if (!orientation.empty() || !placement.empty()) {
                logger->log_error(String(u"unknown tied orientation/placement: %1/%2").arg(orientation).arg(placement), xmlreader);
            }
        }

        if (line_type == u"dashed") {
            lv_tie->set_style_type(SlurStyleType::DASHED);
        } else if (line_type == u"dotted") {
            lv_tie->set_style_type(SlurStyleType::DOTTED);
        } else if (line_type == u"solid" || line_type.empty()) {
            lv_tie->set_style_type(SlurStyleType::SOLID);
        }

        note->score()->undo_add_element(lv_tie);
    } else {
        logger->log_error(String(u"unknown tied type %1").arg(type), xmlreader);
    }
}

//---------------------------------------------------------
//   addWavyLine
//---------------------------------------------------------

static void add_wavy_line(ChordRest *cr, const Fraction &tick, const int wavy_line_no, const String &wavy_line_type,
                          MusicXmlSpannerMap &spanners, TrillStack &trills, MusicXmlLogger *logger,
                          const XmlStreamReader *const xmlreader) {
    if (!wavy_line_type.empty()) {
        const Fraction ticks = cr->ticks();
        const track_idx_t track = cr->track();
        Trill *&trill = trills[wavy_line_no];
        if (wavy_line_type == u"start" || wavy_line_type == u"startstop") {
            if (trill) {
                logger->log_error(String(u"overlapping wavy-line number %1").arg(wavy_line_no + 1), xmlreader);
            } else {
                trill = Factory::create_trill(cr->score()->dummy());
                trill->set_track(track);
                trill->set_track2(track);

                trill->set_ornament(Factory::create_ornament(cr));
                trill->ornament()->set_anchor(ArticulationAnchor::AUTO);

                if (wavy_line_type == u"start") {
                    spanners[trill] = std::pair<int, int>(tick.ticks(), -1);
                    // LOGD("trill=%p inserted at first tick %d", trill, tick);
                }
                if (wavy_line_type == u"startstop") {
                    spanners[trill] = std::pair<int, int>(tick.ticks(), tick.ticks() + ticks.ticks());
                    trill = nullptr;
                    // LOGD("trill=%p inserted at first tick %d second tick %d", trill, tick, tick);
                }
            }
        } else if (wavy_line_type == u"stop") {
            if (!trill) {
                logger->log_error(String(u"wavy-line number %1 stop without start").arg(wavy_line_no + 1), xmlreader);
            } else {
                spanners[trill].second = tick.ticks() + ticks.ticks();
                // LOGD("trill=%p second tick %d", trill, tick);
                trill = nullptr;
            }
        } else {
            logger->log_error(String(u"unknown wavy-line type %1").arg(wavy_line_type), xmlreader);
        }
    }
}

//---------------------------------------------------------
//   addBreath
//---------------------------------------------------------

static void add_breath(const Notation &notation, ChordRest *cr) {
    const SymId breath = notation.sym_id();
    const String placement = notation.attribute(u"placement");

    Segment *const seg = cr->measure()->get_segment(SegmentType::BREATH, cr->tick() + cr->ticks());
    Breath *const b = Factory::create_breath(seg);
    // b->setTrack(trk + voice); TODO check next line
    b->set_track(cr->track());
    b->set_sym_id(breath);
    b->set_visible(notation.visible());
    b->set_placement(placement == u"below" ? PlacementV::BELOW : PlacementV::ABOVE);
    b->set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    color_item(b, Color::from_string(notation.attribute(u"color")));
    seg->add(b);
}

//---------------------------------------------------------
//   addChordLine
//---------------------------------------------------------

static void add_chord_line(const Notation &notation, Note *note, MusicXmlLogger *logger, const XmlStreamReader *const xmlreader) {
    const String chord_line_type = notation.sub_type();
    const String line_type = notation.attribute(u"line-type");
    const String line_shape = notation.attribute(u"line-shape");
    if (!chord_line_type.empty()) {
        if (note) {
            ChordLine *const chordline = Factory::create_chord_line(note->chord());
            if (chord_line_type == u"falloff") {
                chordline->set_chord_line_type(ChordLineType::FALL);
            } else if (chord_line_type == u"doit") {
                chordline->set_chord_line_type(ChordLineType::DOIT);
            } else if (chord_line_type == u"plop") {
                chordline->set_chord_line_type(ChordLineType::PLOP);
            } else if (chord_line_type == u"scoop") {
                chordline->set_chord_line_type(ChordLineType::SCOOP);
            }
            if (line_shape == u"straight") {
                chordline->set_straight(true);
            }
            if (line_type == u"wavy") {
                chordline->set_wavy(true);
            } else if (!line_type.empty() && line_type != u"solid") {
                logger->log_error(String(u"unsupported line-type: %1").arg(line_type), xmlreader);
            }
            chordline->set_visible(notation.visible());
            color_item(chordline, Color::from_string(notation.attribute(u"color")));
            note->chord()->add(chordline);
        } else {
            logger->log_error(String(u"no note for %1").arg(chord_line_type), xmlreader);
        }
    }
}

//---------------------------------------------------------
//   notationWithAttributes
//---------------------------------------------------------

/**
 Helper function to create Notation with initial attributes.
 */

Notation Notation::notation_with_attributes(const String &name, const std::vector<XmlStreamReader::Attribute> &attributes,
                                            const String &parent, const SymId &sym_id) {
    Notation notation = Notation(name, parent, sym_id);
    for (const XmlStreamReader::Attribute &attr : attributes) {
        notation.add_attribute(String::from_ascii(attr.name.ascii()), attr.value);
    }
    return notation;
}

//---------------------------------------------------------
//   addAttribute
//---------------------------------------------------------

void Notation::add_attribute(const String &name, const String &value) { attributes_.emplace(name, value); }

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

void MusicXmlParserNotations::add_error(const String &error) {
    if (!error.empty()) {
        logger_->log_error(error, &e_);
        errors_ += error;
    }
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserNotations::skip_log_curr_elem() {
    //_logger->logDebugInfo(String("skipping '%1'").arg(_e.name().toString()), &_e);
    e_.skip_current_element();
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

void MusicXmlParserNotations::parse() {
    visible_ = e_.ascii_attribute("print-object") != "no";

    while (e_.read_next_start_element()) {
        if (e_.name() == "arpeggiate") {
            arpeggio();
        } else if (e_.name() == "articulations") {
            articulations();
        } else if (e_.name() == "dynamics") {
            dynamics();
        } else if (e_.name() == "fermata") {
            fermata();
        } else if (e_.name() == "glissando") {
            glissando_slide();
        } else if (e_.name() == "hammer-on") {
            slur();
        } else if (e_.name() == "non-arpeggiate") {
            arpeggio_color_ = Color::from_string(e_.attribute("color"));
            arpeggio_type_ = u"non-arpeggiate";
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "ornaments") {
            ornaments();
        } else if (e_.name() == "pull-off") {
            slur();
        } else if (e_.name() == "slur") {
            slur();
        } else if (e_.name() == "slide") {
            glissando_slide();
        } else if (e_.name() == "technical") {
            technical();
        } else if (e_.name() == "tied") {
            tied();
        } else if (e_.name() == "tuplet") {
            tuplet();
        } else if (e_.name() == "other-notation") {
            other_notation();
        } else {
            skip_log_curr_elem();
        }
    }

    /*
    for (const auto& notation : _notations) {
          LOGD("%s", muPrintable(notation.print()));
          }
     */

    add_error(check_at_end_element(e_, u"notations"));
}

//---------------------------------------------------------
//   addNotation
//---------------------------------------------------------

void MusicXmlParserNotations::add_notation(const Notation &notation, ChordRest *const cr, Note *const note) {
    if (notation.sym_id() != SymId::NO_SYM) {
        if (notation.name() == u"breath") {
            add_breath(notation, cr);
        } else if (notation.name() == u"fermata") {
            add_fermata_to_chord(notation, cr);

            // Terminate tempo line
            const InferredTempoLineStack &lines = pass2_.get_inferred_tempo_line();
            terminate_inferred_line(std::vector<TextLineBase *>(lines.begin(), lines.end()), cr->tick(), cr->track(),
                                    pass2_.configuration()->infer_text_type());
        } else if (notation.parent() == u"ornaments") {
            add_turn_to_chord(notation, cr);
        } else {
            add_articulation_to_chord(notation, cr);
        }
    } else if (notation.parent() == u"ornaments") {
        if (notation.name() == u"mordent" || notation.name() == u"inverted-mordent") {
            add_mordent_to_chord(notation, cr);
        } else if (notation.name() == u"other-ornament") {
            add_other_ornament_to_chord(notation, cr);
        }
    } else if (notation.parent() == u"articulations") {
        if (note && notation.name() == u"chord-line") {
            add_chord_line(notation, note, logger_, &e_);
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

void MusicXmlParserNotations::add_to_score(ChordRest *const cr, Note *const note, const Fraction &tick, SlurStack &slurs,
                                           Glissando *glissandi[::mu::iex::musicxml::max_number_level][2], MusicXmlSpannerMap &spanners,
                                           TrillStack &trills, MusicXmlTieMap &ties, std::vector<Note *> &unstarted_tie_notes,
                                           std::vector<Note *> &unended_tie_notes, ArpeggioMap &arp_map, DelayedArpMap &delayed_arps) {
    add_arpeggio(cr, arpeggio_type_, arpeggio_no_, arpeggio_color_, arp_map, delayed_arps);
    add_wavy_line(cr, tick, wavy_line_no_, wavy_line_type_, spanners, trills, logger_, &e_);

    for (const Notation &notation : notations_) {
        if (notation.sym_id() != SymId::NO_SYM) {
            add_notation(notation, cr, note);
        } else if (notation.name() == "slur" || notation.name() == "hammer-on" || notation.name() == "pull-off") {
            add_slur(notation, slurs, cr, note, tick, logger_, &e_, pass2_.configuration()->import_layout());
        } else if (note && (notation.name() == "glissando" || notation.name() == "slide")) {
            add_glissando_slide(notation, note, glissandi, spanners, logger_, &e_);
        } else if (note && notation.name() == "tied") {
            add_tie(notation, note, cr->track(), ties, unstarted_tie_notes, unended_tie_notes, logger_, &e_,
                    pass2_.configuration()->import_layout());
        } else if (note && notation.parent() == "technical") {
            add_technical(notation, note);
        } else {
            add_notation(notation, cr, note);
        }
    }

    for (const MusicXmlDynamic &d : std::as_const(dynamics_list_)) {
        Dynamic *dynamic = create_imported_dynamic(score_, d);
        pass2_.add_elem_offset(dynamic, cr->track(), d.placement, cr->measure(), tick);
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

    String s = e_.read_text();

    if (s == u"up") {
        sd = DirectionV::UP;
    } else if (s == u"down") {
        sd = DirectionV::DOWN;
    } else if (s == u"none") {
        nost = true;
    } else if (s == u"double") {
    } else {
        logger_->log_error(String(u"unknown stem direction %1").arg(s), &e_);
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
    Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"notations");
    const String fermata_text = e_.read_text();

    notation.set_sym_id(convert_fermata_to_sym_id(fermata_text));
    notation.set_text(fermata_text);
    notation.set_visible(visible_);
    notations_.push_back(notation);
}

//---------------------------------------------------------
//   tuplet
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations/tuplet node.
 */

void MusicXmlParserNotations::tuplet() {
    const String tuplet_type = e_.attribute("type");
    const String tuplet_placement = e_.attribute("placement");
    const String tuplet_bracket = e_.attribute("bracket");
    const String tuplet_show_number = e_.attribute("show-number");

    // ignore possible children (currently not supported)
    e_.skip_current_element();

    if (tuplet_type == u"start") {
        tuplet_desc_.type = MusicXmlStartStop::START;
    } else if (tuplet_type == u"stop") {
        tuplet_desc_.type = MusicXmlStartStop::STOP;
    } else if (!tuplet_type.empty() && tuplet_type != u"start" && tuplet_type != u"stop") {
        logger_->log_error(String(u"unknown tuplet type '%1'").arg(tuplet_type), &e_);
    }

    // set bracket, leave at default if unspecified
    if (tuplet_bracket == u"yes") {
        tuplet_desc_.bracket = TupletBracketType::SHOW_BRACKET;
    } else if (tuplet_bracket == u"no") {
        tuplet_desc_.bracket = TupletBracketType::SHOW_NO_BRACKET;
    }

    // set number, default is "actual" (=NumberType::SHOW_NUMBER)
    if (tuplet_show_number == u"both") {
        tuplet_desc_.shownumber = TupletNumberType::SHOW_RELATION;
    } else if (tuplet_show_number == u"none") {
        tuplet_desc_.shownumber = TupletNumberType::NO_TEXT;
    } else {
        tuplet_desc_.shownumber = TupletNumberType::SHOW_NUMBER;
    }

    // set number and bracket placement
    if (tuplet_placement == u"above") {
        tuplet_desc_.direction = DirectionV::UP;
    } else if (tuplet_placement == u"below") {
        tuplet_desc_.direction = DirectionV::DOWN;
    } else if (tuplet_placement.empty()) {
        // ignore
    } else {
        logger_->log_error(String(u"unknown tuplet placement: %1").arg(tuplet_placement), &e_);
    }
}

void MusicXmlParserNotations::other_notation() {
    const String type = e_.attribute("type");
    const String smufl = e_.attribute("smufl");

    if (!smufl.empty()) {
        SymId id = SymNames::sym_id_by_name(smufl, SymId::NO_SYM);
        Notation notation = Notation::notation_with_attributes(String::from_ascii(e_.name().ascii()), e_.attributes(), u"notations", id);
        notation.set_visible(visible_);
        notations_.push_back(notation);
        e_.skip_current_element();
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
    : e_(e), score_(score), pass1_(pass1), pass2_(pass2), logger_(logger), has_default_y_(false), default_y_(0.0), has_relative_y_(false),
      relative_y_(0.0), tpo_metro_(0), tpo_sound_(0), offset_(0, 1) {
    // nothing
}

bool HarmonyDesc::fret_diagram_visible() const { return fret_diagram_ ? fret_diagram_->visible() : false; }

const ChordDescription *HarmonyDesc::descr() const {
    return harmony_ && !harmony_->chords().empty() ? harmony_->chords().front()->descr() : nullptr;
}
} // namespace mu::iex::musicxml
