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

#include "engraving/dom/box.h"
#include "engraving/dom/bracketitem.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/page.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/sig.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/text.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/utils.h"
#include "engraving/editing/editstaffbrackets.h"
#include "engraving/engravingerrors.h"
#include "engraving/rendering/iscorerenderer.h"
#include "engraving/rendering/score/tlayout.h"

#include "engraving/style/style.h"
#include "engraving/style/textstyle.h"

#include "engraving/types/symnames.h"

#include "importmusicxmllogger.h"
#include "importmusicxmlnoteduration.h"
#include "importmusicxmlpass1.h"
#include "../shared/musicxmltypes.h"
#include "../shared/musicxmlsupport.h"

#include "importexport/musicxml/imusicxmlconfiguration.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   NoteList
//---------------------------------------------------------

/**
 List of note start/stop times in a voice in all staves.
*/

struct NoteList {
    NoteList();
    void add_note(const int start_tick, const int end_tick, const size_t staff);
    void dump(const int &voice) const;
    bool staves_overlap(const int staff1, const int staff2) const;
    bool any_staff_overlaps() const;
private:
    std::vector<StartStopList> staff_note_lists; // The note start/stop times in all staves
    bool notes_overlap(const StartStop &n1, const StartStop &n2) const;
};

//---------------------------------------------------------
//   VoiceOverlapDetector
//---------------------------------------------------------

/**
 Detect overlap in a voice, which is when a voice has two or more notes
 active at the same time. In theory this should not happen, as voices
 only move forward in time, but Sibelius 7 reuses voice numbers in multi-
 staff parts, which leads to overlap.

 Current implementation does not detect voice overlap within a staff,
 but only between staves.
*/

struct VoiceOverlapDetector {
    VoiceOverlapDetector();
    void add_note(const int start_tick, const int end_tick, const int &voice, const int staff);
    void dump() const;
    void new_measure();
    bool staves_overlap(const int &voice) const;
private:
    std::map<int, NoteList> note_lists; // The notelists for all the voices
};

NoteList::NoteList() {
    staff_note_lists.reserve(::mu::engraving::max_staves);
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        staff_note_lists.push_back(StartStopList());
    }
}

void NoteList::add_note(const int start_tick, const int end_tick, const size_t staff) {
    if (staff < staff_note_lists.size()) {
        staff_note_lists[staff].push_back(StartStop(start_tick, end_tick));
    }
}

void NoteList::dump(const int &voice) const {
    // dump contents
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        printf("voice %d staff %d:", voice, i);
        for (size_t j = 0; j < staff_note_lists.at(i).size(); ++j) {
            printf(" %d-%d", staff_note_lists.at(i).at(j).first, staff_note_lists.at(i).at(j).second);
        }
        printf("\n");
    }
    // show overlap
    printf("overlap voice %d:", voice);
    for (int i = 0; i < ::mu::engraving::max_staves - 1; ++i) {
        for (int j = i + 1; j < ::mu::engraving::max_staves; ++j) {
            staves_overlap(i, j);
        }
    }
    printf("\n");
}

/**
 Determine if notes n1 and n2 overlap.
 This is NOT the case if
 - n1 starts when or after n2 stops
 - or n2 starts when or after n1 stops
 */

bool NoteList::notes_overlap(const StartStop &n1, const StartStop &n2) const { return !(n1.first >= n2.second || n1.second <= n2.first); }

/**
 Determine if any note in staff1 and staff2 overlaps.
 */

bool NoteList::staves_overlap(const int staff1, const int staff2) const {
    for (size_t i = 0; i < staff_note_lists.at(staff1).size(); ++i) {
        for (size_t j = 0; j < staff_note_lists.at(staff2).size(); ++j) {
            if (notes_overlap(staff_note_lists.at(staff1).at(i), staff_note_lists.at(staff2).at(j))) {
                //printf(" %d-%d", staff1, staff2);
                return true;
            }
        }
    }
    return false;
}

/**
 Determine if any note in any staff overlaps.
 */

bool NoteList::any_staff_overlaps() const {
    for (int i = 0; i < ::mu::engraving::max_staves - 1; ++i) {
        for (int j = i + 1; j < ::mu::engraving::max_staves; ++j) {
            if (staves_overlap(i, j)) {
                return true;
            }
        }
    }
    return false;
}

VoiceOverlapDetector::VoiceOverlapDetector() {
    // LOGD("VoiceOverlapDetector::VoiceOverlapDetector(staves %d)", MAX_STAVES);
}

void VoiceOverlapDetector::add_note(const int start_tick, const int end_tick, const int &voice, const int staff) {
    // if necessary, create the note list for voice
    if (!muse::contains(note_lists, voice)) {
        note_lists.insert({voice, NoteList()});
    }
    note_lists[voice].add_note(start_tick, end_tick, staff);
}

void VoiceOverlapDetector::dump() const {
    // LOGD("VoiceOverlapDetector::dump()");
    for (auto &p : note_lists) {
        p.second.dump(p.first);
    }
}

void VoiceOverlapDetector::new_measure() {
    // LOGD("VoiceOverlapDetector::newMeasure()");
    note_lists.clear();
}

bool VoiceOverlapDetector::staves_overlap(const int &voice) const {
    if (muse::contains(note_lists, voice)) {
        return note_lists.at(voice).any_staff_overlaps();
    } else {
        return false;
    }
}

//---------------------------------------------------------
//   allocateStaves
//---------------------------------------------------------

/**
 Allocate MuseScore staff to MusicXML voices.
 For each staff, allocate at most VOICES voices to the staff.
 */

// for regular (non-overlapping) voices:
// 1) assign voice to a staff (allocateStaves)
// 2) assign voice numbers (allocateVoices)
// due to cross-staving, it is not a priori clear to which staff
// a voice has to be assigned
// allocate ordered by number of chordrests in the MusicXML voice
//
// for overlapping voices:
// 1) assign voice to staves it is found in (allocateStaves)
// 2) assign voice numbers (allocateVoices)

static void allocate_staves(VoiceList &vc_lst) {
    // initialize
    int voices_allocated[::mu::engraving::max_staves]; // number of voices allocated on each staff
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        voices_allocated[i] = 0;
    }

    // handle regular (non-overlapping) voices
    // note: outer loop executed vcLst.size() times, as each inner loop handles exactly one item
    for (size_t i = 0; i < vc_lst.size(); ++i) {
        // find the regular voice containing the highest number of chords and rests that has not been handled yet
        int max = 0;
        int key = -1;
        for (VoiceList::const_iterator j = vc_lst.cbegin(); j != vc_lst.cend(); ++j) {
            if (!j->second.overlaps() && j->second.number_chord_rests() > max && j->second.staff() == -1) {
                max = j->second.number_chord_rests();
                key = j->first;
            }
        }
        if (key > 0) {
            int pref_st = muse::value(vc_lst, key).preferred_staff();
            if (voices_allocated[pref_st] < static_cast<int>(::mu::engraving::voices)) {
                vc_lst[key].set_staff(pref_st);
                voices_allocated[pref_st]++;
            } else {
                // out of voices: mark as used but not allocated
                vc_lst[key].set_staff(-2);
            }
        }
    }

    // handle overlapping voices
    // for every staff allocate remaining voices (if space allows)
    // the ones with the highest number of chords and rests get allocated first
    for (int h = 0; h < ::mu::engraving::max_staves; ++h) {
        // note: middle loop executed vcLst.size() times, as each inner loop handles exactly one item
        for (size_t i = 0; i < vc_lst.size(); ++i) {
            // find the overlapping voice containing the highest number of chords and rests that has not been handled yet
            int max = 0;
            int key = -1;
            for (VoiceList::const_iterator j = vc_lst.cbegin(); j != vc_lst.cend(); ++j) {
                if (j->second.overlaps() && j->second.number_chord_rests(h) > max && j->second.staff_alloc(h) == -1) {
                    max = j->second.number_chord_rests(h);
                    key = j->first;
                }
            }
            if (key > 0) {
                int pref_st = h;
                if (voices_allocated[pref_st] < static_cast<int>(::mu::engraving::voices)) {
                    vc_lst[key].set_staff_alloc(pref_st, 1);
                    voices_allocated[pref_st]++;
                } else {
                    // out of voices: mark as used but not allocated
                    vc_lst[key].set_staff_alloc(pref_st, -2);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   allocateVoices
//---------------------------------------------------------

/**
 Allocate MuseScore voice to MusicXML voices.
 For each staff, the voices are number 1, 2, 3, 4
 in the same order they are numbered in the MusicXML file.
 */

static void allocate_voices(VoiceList &vc_lst) {
    int next_voice[::mu::engraving::max_staves]; // number of voices allocated on each staff
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        next_voice[i] = 0;
    }
    // handle regular (non-overlapping) voices
    // a voice is allocated on one specific staff
    for (VoiceList::const_iterator i = vc_lst.cbegin(); i != vc_lst.cend(); ++i) {
        int staff = i->second.staff();
        int key = i->first;
        if (staff >= 0) {
            vc_lst[key].set_voice(next_voice[staff]);
            next_voice[staff]++;
        }
    }
    // handle overlapping voices
    // each voice may be in every staff
    for (VoiceList::const_iterator i = vc_lst.cbegin(); i != vc_lst.cend(); ++i) {
        for (int j = 0; j < ::mu::engraving::max_staves; ++j) {
            int staff_alloc = i->second.staff_alloc(j);
            int key = i->first;
            if (staff_alloc >= 0) {
                vc_lst[key].set_voice(j, next_voice[j]);
                next_voice[j]++;
            }
        }
    }
}

//---------------------------------------------------------
//   copyOverlapData
//---------------------------------------------------------

/**
 Copy the overlap data from the overlap detector to the voice list.
 */

static void copy_overlap_data(VoiceOverlapDetector &vod, VoiceList &vc_lst) {
    for (VoiceList::const_iterator i = vc_lst.cbegin(); i != vc_lst.cend(); ++i) {
        int key = i->first;
        if (vod.staves_overlap(key)) {
            vc_lst[key].set_overlap(true);
        }
    }
}

//---------------------------------------------------------
//   MusicXmlParserPass1
//---------------------------------------------------------

MusicXmlParserPass1::MusicXmlParserPass1(Score *score, const IMusicXmlConfiguration &configuration, MusicXmlLogger *logger)
    : divs_(0), score_(score), configuration_(configuration), logger_(logger), has_beaming_info_(false) {
    // nothing
}

//---------------------------------------------------------
//   addError
//---------------------------------------------------------

void MusicXmlParserPass1::add_error(const String &error) {
    if (!error.empty()) {
        logger_->log_error(error, &e_);
        errors_ += error_string_with_location(e_.byte_offset(), error) + '\n';
    }
}

void MusicXmlParserPass1::set_exporter_software(String &exporter) {
    if (exporter.contains(u"sibelius")) {
        if (exporter.contains(u"dolet 6")) {
            exporter_software_ = MusicXmlExporterSoftware::DOLET6;
        } else if (exporter.contains(u"dolet 8")) {
            exporter_software_ = MusicXmlExporterSoftware::DOLET8;
        } else {
            exporter_software_ = MusicXmlExporterSoftware::SIBELIUS;
        }
    } else if (exporter.contains(u"dorico")) {
        exporter_software_ = MusicXmlExporterSoftware::DORICO;
    } else if (exporter.contains(u"finale")) {
        exporter_software_ = MusicXmlExporterSoftware::FINALE;
    } else if (exporter.contains(u"noteflight")) {
        exporter_software_ = MusicXmlExporterSoftware::NOTEFLIGHT;
    }
}

void MusicXmlParserPass1::set_exporter_styles() {
    // If exporters have different default styles to MuseScore, assume they are being used here
    if (sib_or_dolet()) {
        score_->style().set(Sid::FRET_FRETS, 5);
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

void MusicXmlParserPass1::init_part_state(const String & /* partId */) {
    time_sig_dura_ = Fraction(0, 0); // invalid
    octave_shifts_.clear();
}

//---------------------------------------------------------
//   determineMeasureLength
//---------------------------------------------------------

/**
 Determine the length in ticks of each measure in all parts.
 Return false on error.
 */

bool MusicXmlParserPass1::determine_measure_length(std::vector<Fraction> &ml) const {
    ml.clear();

    // determine number of measures: max number of measures in any part
    size_t n_measures = 0;
    for (const auto &[_, part] : parts_) {
        if (part.n_measures() > n_measures) {
            n_measures = part.n_measures();
        }
    }

    // determine max length of a specific measure in all parts
    for (size_t i = 0; i < n_measures; ++i) {
        Fraction max_meas_dur;
        for (const auto &[_, part] : parts_) {
            if (i < part.n_measures()) {
                Fraction meas_dur_part_j = part.measure_duration(i);
                if (meas_dur_part_j > max_meas_dur) {
                    max_meas_dur = meas_dur_part_j;
                }
            }
        }
        //LOGD("determineMeasureLength() measure %d %s (%d)", i, muPrintable(maxMeasDur.print()), maxMeasDur.ticks());
        ml.push_back(max_meas_dur);
    }
    return true;
}

//---------------------------------------------------------
//   getVoiceList
//---------------------------------------------------------

/**
 Get the VoiceList for part \a id.
 Return an empty VoiceList on error.
 */

VoiceList MusicXmlParserPass1::get_voice_list(const String &id) const {
    if (muse::contains(parts_, id)) {
        return parts_.at(id).voicelist;
    }
    return VoiceList();
}

//---------------------------------------------------------
//   getInstrList
//---------------------------------------------------------

/**
 Get the MusicXmlInstrList for part \a id.
 Return an empty MusicXmlInstrList on error.
 */

MusicXmlInstrList MusicXmlParserPass1::get_instr_list(const String &id) const {
    if (muse::contains(parts_, id)) {
        return parts_.at(id).instr_list;
    }
    return MusicXmlInstrList();
}

//---------------------------------------------------------
//   getIntervals
//---------------------------------------------------------

/**
 Get the MusicXmlIntervalList for part \a id.
 Return an empty MusicXmlIntervalList on error.
 */

MusicXmlIntervalList MusicXmlParserPass1::get_intervals(const String &id) const {
    if (muse::contains(parts_, id)) {
        return parts_.at(id).intervals;
    }
    return MusicXmlIntervalList();
}

//---------------------------------------------------------
//   determineMeasureLength
//---------------------------------------------------------

/**
 Set default notehead, line and stem direction
 for instrument \a instrId in part \a id.
 Called from pass 2, notehead, line and stemDirection are not read in pass 1.
 */

void MusicXmlParserPass1::set_drumset_default(const String &id, const String &instr_id, const NoteHeadGroup hg, const int line,
                                              const DirectionV sd) {
    if (muse::contains(instruments_, id) && muse::contains(instruments_.at(id), instr_id)) {
        instruments_[id][instr_id].notehead = hg;
        instruments_[id][instr_id].line = line;
        instruments_[id][instr_id].stem_direction = sd;
    }
}

//---------------------------------------------------------
//   determineStaffMoveVoice
//---------------------------------------------------------

/**
 For part \a id, determine MuseScore (ms) staffmove, track and voice from MusicXML (mx) staff and voice
 MusicXML staff is 0 for the first staff, 1 for the second.
 Note: track is the first track of the ms staff in the score, add ms voice for elements in a voice
 Return true if OK, false on error
 TODO: finalize
 */

bool MusicXmlParserPass1::determine_staff_move_voice(const String &id, const int mx_staff, const int &mx_voice, int &ms_move, int &ms_track,
                                                     int &ms_voice) const {
    VoiceList voicelist = get_voice_list(id);
    ms_move = 0;  // TODO
    ms_track = 0; // TODO
    ms_voice = 0; // TODO

    // MusicXML voices are counted for all staves of an
    // instrument. They are not limited. In mscore voices are associated
    // with a staff. Every staff can have at most VOICES voices.

    // The following lines map MusicXML voices to mscore voices.
    // If a voice crosses two staves, this is expressed with the
    // "move" parameter in mscore.

    // MusicXML voices are unique within a part, but not across parts.

    //LOGD("voice mapper before: voice='%s' staff=%d", muPrintable(mxVoice), mxStaff);
    int s; // staff mapped by voice mapper
    int v; // voice mapped by voice mapper
    if (muse::value(voicelist, mx_voice).overlaps()) {
        // for overlapping voices, the staff does not change
        // and the voice is mapped and staff-dependent
        s = mx_staff;
        v = muse::value(voicelist, mx_voice).voice(s);
    } else {
        // for non-overlapping voices, both staff and voice are
        // set by the voice mapper
        s = muse::value(voicelist, mx_voice).staff();
        v = muse::value(voicelist, mx_voice).voice();
    }

    //LOGD("voice mapper mapped: s=%d v=%d", s, v);
    if (s < 0 || v < 0) {
        LOGD("too many voices (staff=%d voice='%d' -> s=%d v=%d)", mx_staff + 1, mx_voice, s, v);
        return false;
    }

    ms_move = mx_staff - s;
    ms_voice = v;

    // make score-relative instead on part-relative
    Part *part = muse::value(part_map_, id);
    IF_ASSERT_FAILED(part) { return false; }
    staff_idx_t score_rel_staff = score_->staff_idx(part); // zero-based number of parts first staff in the score
    ms_track = static_cast<int>((score_rel_staff + s) * ::mu::engraving::voices);

    //LOGD("voice mapper after: scoreRelStaff=%d partRelStaff=%d msMove=%d msTrack=%d msVoice=%d",
    //       scoreRelStaff, s, msMove, msTrack, msVoice);
    // note: relStaff is the staff number relative to the parts first staff
    //       voice is the voice number in the staff

    return true;
}

//---------------------------------------------------------
//   hasPart
//---------------------------------------------------------

/**
 Check if part \a id is found.
 */

bool MusicXmlParserPass1::has_part(const String &id) const { return muse::contains(parts_, id); }

//---------------------------------------------------------
//   trackForPart
//---------------------------------------------------------

/**
 Return the (score relative) track number for the first staff of part \a id.
 */

track_idx_t MusicXmlParserPass1::track_for_part(const String &id) const {
    Part *part = muse::value(part_map_, id);
    IF_ASSERT_FAILED(part) { return muse::nidx; }
    staff_idx_t score_rel_staff = score_->staff_idx(part); // zero-based number of parts first staff in the score
    return score_rel_staff * ::mu::engraving::voices;
}

//---------------------------------------------------------
//   getMeasureStart
//---------------------------------------------------------

/**
 Return the measure start time for measure \a i.
 */

Fraction MusicXmlParserPass1::get_measure_start(const size_t i) const {
    if (i < measure_start_.size()) {
        return measure_start_.at(i);
    } else {
        return Fraction(0, 0); // invalid
    }
}

//---------------------------------------------------------
//   octaveShift
//---------------------------------------------------------

/**
 Return the octave shift for part \a id in \a staff at \a f.
 */

int MusicXmlParserPass1::octave_shift(const String &id, const staff_idx_t staff, const Fraction &f) const {
    if (muse::contains(parts_, id)) {
        return parts_.at(id).octave_shift(staff, f);
    }

    return 0;
}

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXmlParserPass1::skip_log_curr_elem() {
    logger_->log_debug_info(String(u"skipping '%1'").arg(String::from_ascii(e_.name().ascii())), &e_);
    e_.skip_current_element();
}

//---------------------------------------------------------
//   addBreak
//---------------------------------------------------------

static void add_break(Score *const, MeasureBase *const mb, const LayoutBreakType type) {
    LayoutBreak *lb = Factory::create_layout_break(mb);
    lb->set_layout_break_type(type);
    lb->set_track(0);
    mb->add(lb);
}

//---------------------------------------------------------
//   addBreakToPreviousMeasureBase
//---------------------------------------------------------

static void add_break_to_previous_measure_base(Score *const score, MeasureBase *const mb, const LayoutBreakType type, bool import_breaks) {
    MeasureBase *const pm = mb->prev();
    if (pm && import_breaks) {
        add_break(score, pm, type);
    }
}

//---------------------------------------------------------
//   addText
//---------------------------------------------------------

/**
 Add text \a strTxt to VBox \a vbx using Tid \a stl.
 */

static void add_text(VBox *vbx, Score *, const String &str_txt, const TextStyleType stl) {
    if (!str_txt.is_empty()) {
        Text *text = Factory::create_text(vbx, stl);
        text->set_xml_text(str_txt.trimmed());
        vbx->add(text);
    }
}

static bool override_text_style_for_composer(const String &credit_string) {
    // HACK: check if the string is likely to contain composer credit, so the proper text style can be applied.
    // TODO: introduce a flag to decide if we want to do this or not.
    static const std::wregex re(L"\\s*((Words|Music|Lyrics).*)*by\\s+([A-Z][a-zA-Zö'’-]+\\s[A-Z][a-zA-Zös'’-]+.*)+");
    return credit_string.contains(re);
}

//---------------------------------------------------------
//   addText2
//---------------------------------------------------------

static void scale_title(Score *score, Text *text);

/**
 Add text \a strTxt to VBox \a vbx using Tid \a stl.
 Also sets Align and Yoff.
 */

static void add_text2(VBox *vbx, Score *score, const String &str_txt, const TextStyleType stl, const Align align) {
    if (stl != TextStyleType::COMPOSER && override_text_style_for_composer(str_txt)) {
        // HACK: in some Dolet 8 files the composer is written as a subtitle, which leads to stupid formatting.
        // This overrides the formatting and introduces proper composer text
        Text *text = Factory::create_text(vbx, TextStyleType::COMPOSER);
        text->set_xml_text(str_txt.trimmed());
        text->set_property_flags(Pid::OFFSET, PropertyFlags::UNSTYLED);
        vbx->add(text);
    } else if (!str_txt.is_empty()) {
        Text *text = Factory::create_text(vbx, stl);
        text->set_xml_text(str_txt.trimmed());
        text->set_align(align);
        text->set_property_flags(Pid::ALIGN, PropertyFlags::UNSTYLED);
        text->set_property_flags(Pid::OFFSET, PropertyFlags::UNSTYLED);
        vbx->add(text);
        if (stl == TextStyleType::TITLE) {
            scale_title(score, text);
        }
    }
}

//---------------------------------------------------------
//   findYMinYMaxInWords
//---------------------------------------------------------

static void find_y_min_y_max_in_words(const std::vector<const CreditWords *> &words, int &miny, int &maxy) {
    miny = 0;
    maxy = 0;

    if (words.empty()) {
        return;
    }

    miny = words.at(0)->default_y;
    maxy = words.at(0)->default_y;
    for (const CreditWords *w : words) {
        if (w->default_y < miny) {
            miny = w->default_y;
        }
        if (w->default_y > maxy) {
            maxy = w->default_y;
        }
    }
}

//---------------------------------------------------------
//   alignForCreditWords
//---------------------------------------------------------

static Align align_for_credit_words(const CreditWords *const w, const int page_width, const TextStyleType tid) {
    Align align = AlignH::LEFT;
    if (w->default_x > (page_width / 3)) {
        if (w->default_x < (2 * page_width / 3)) {
            align = AlignH::HCENTER;
        } else {
            align = AlignH::RIGHT;
        }
    }
    if (tid == TextStyleType::COMPOSER) {
        align.vertical = AlignV::BOTTOM;
    }
    return align;
}

//---------------------------------------------------------
//   creditWordTypeToTid
//---------------------------------------------------------

static TextStyleType credit_word_type_to_tid(const String &type) {
    if (type == u"composer") {
        return TextStyleType::COMPOSER;
    } else if (type == u"lyricist") {
        return TextStyleType::LYRICIST;
    }
    /*
    else if (type == "page number")
          return TextStyleName::;
    else if (type == "rights")
          return TextStyleName::;
     */
    else if (type == u"subtitle") {
        return TextStyleType::SUBTITLE;
    } else if (type == u"title") {
        return TextStyleType::TITLE;
    } else {
        return TextStyleType::DEFAULT;
    }
}

//---------------------------------------------------------
//   creditWordTypeGuess
//---------------------------------------------------------

static TextStyleType credit_word_type_guess(const CreditWords *const word, std::vector<const CreditWords *> &words, const int page_width) {
    const double pw1 = page_width / 3;
    const double pw2 = page_width * 2 / 3;
    const double defx = word->default_x;
    // composer is in the right column
    if (pw2 < defx) {
        // found composer
        return TextStyleType::COMPOSER;
    }
    // poet is in the left column
    else if (defx < pw1) {
        // found poet/lyricist
        return TextStyleType::LYRICIST;
    }
    // title is in the middle column
    else {
        // if another word in the middle column has a larger font size, this word is not the title
        for (const CreditWords *w : words) {
            if (w == word) {
                continue; // it's me
            }
            if (w->default_x < pw1 || pw2 < w->default_x) {
                continue; // it's not in the middle column
            }
            if (word->font_size < w->font_size) {
                return TextStyleType::SUBTITLE; // word does not have the largest font size, assume subtitle
            }
        }
        return TextStyleType::TITLE; // no better title candidate found
    }
}

//---------------------------------------------------------
//   tidForCreditWords
//---------------------------------------------------------

static TextStyleType tid_for_credit_words(const CreditWords *const word, std::vector<const CreditWords *> &words, const int page_width) {
    const TextStyleType tid = credit_word_type_to_tid(word->type);
    if (tid != TextStyleType::DEFAULT) {
        // type recognized, done
        return tid;
    } else {
        // type not recognized, guess
        return credit_word_type_guess(word, words, page_width);
    }
}

//---------------------------------------------------------
//   createAndAddVBoxForCreditWords
//---------------------------------------------------------

VBox *MusicXmlParserPass1::create_and_add_v_box_for_credit_words(Score *score, Fraction tick) {
    VBox *vbox = Factory::create_title_v_box(score);
    vbox->set_tick(tick);
    score->measures()->append(vbox);
    return vbox;
}

//---------------------------------------------------------
//   mustAddWordToVbox
//---------------------------------------------------------

// determine if specific types of credit words must be added: do not add copyright and page number,
// as these typically conflict with MuseScore's style and/or layout

static bool must_add_word_to_vbox(const String &credit_type) { return credit_type != u"rights" && credit_type != u"page number"; }

//---------------------------------------------------------
//   isLikelySubtitleText
//---------------------------------------------------------

bool is_likely_subtitle_text(const String &text, const bool case_insensitive = true) {
    std::regex::flag_type case_option = case_insensitive ? std::regex::icase : std::regex::ECMAScript;
    return text.trimmed().contains(std::wregex(L"^[Ff]rom\\s+(?!$)", case_option)) ||
           text.trimmed().contains(std::wregex(L"^Theme from\\s+(?!$)", case_option)) ||
           text.trimmed().contains(std::wregex(L"(((Op\\.?\\s?\\d+)|(No\\.?\\s?\\d+))\\s?)+", case_option)) ||
           text.trimmed().contains(std::wregex(L"\\(.*[Ff]rom\\s.*\\)", case_option));
}

//---------------------------------------------------------
//   isLikelyCreditText
//---------------------------------------------------------

bool is_likely_credit_text(const String &text, const bool case_insensitive = true) {
    std::regex::flag_type case_option = case_insensitive ? std::regex::icase : std::regex::ECMAScript;
    return text.trimmed().contains(
               std::wregex(L"^((Words|Music|Lyrics|Composed),?(\\sand|\\s&amp;|\\s&)?\\s)*[Bb]y\\s+(?!$)", case_option)) ||
           text.trimmed().contains(std::wregex(L"^(Traditional|Trad\\.)", case_option));
}

static bool is_likely_rights_text(const String &text) {
    return text.contains(u"all rights reserved", CaseSensitivity::CASE_INSENSITIVE) || text.contains(u"\u00A9");
}

//---------------------------------------------------------
//   inferSubTitleFromTitle
//---------------------------------------------------------

// Extracts a likely subtitle from the title string
// Returns the inferred subtitle

static void infer_from_title(String &title, String &inferred_subtitle, String &inferred_credits) {
    StringList subtitle_lines;
    StringList credit_lines;
    StringList title_lines = title.split(std::regex("\\n"));
    size_t nr_of_title_lines = title_lines.size();
    if (nr_of_title_lines == 1) {
        return;
    }
    for (size_t i = nr_of_title_lines; i > 0; --i) {
        String line = title_lines[i - 1];
        if (is_likely_credit_text(line, true)) {
            credit_lines.insert(0, line);
            title_lines.erase(title_lines.begin() + i - 1);
        } else if (is_likely_subtitle_text(line, true)) {
            subtitle_lines.insert(0, line);
            title_lines.erase(title_lines.begin() + i - 1);
        }
    }
    title = title_lines.join(u"\n");
    inferred_subtitle = subtitle_lines.join(u"\n");
    inferred_credits = credit_lines.join(u"\n");
}

static void resize_title_box(VBox *vbox) {
    double calculated_v_box_height = 0;
    ElementList elist = vbox->el();
    Score *score = vbox->score();
    for (EngravingItem *e : elist) {
        score->renderer()->layout_item(e);
    }

    const double padding = vbox->size_is_spatium_dependent() ? vbox->style().spatium() : vbox->style().default_spatium();

    for (EngravingItem *e : elist) {
        if (e->is_text()) {
            Text *txt = to_text(e);
            Text::LayoutData *txt_ld = txt->mutldata();

            LD_CONDITION(txt_ld->is_set_bbox());

            RectF bbox = txt_ld->bbox();
            bbox.move_top(0.0);
            txt_ld->set_bbox(bbox);
            calculated_v_box_height += txt_ld->bbox().height() + padding;
        }
    }

    const auto height = Spatium::from_absolute(calculated_v_box_height, vbox->spatium());
    if (height > vbox->property_default(Pid::BOX_HEIGHT).value<Spatium>()) {
        vbox->undo_change_property(Pid::BOX_HEIGHT, height);
    }
}

//---------------------------------------------------------
//   addCreditWords
//---------------------------------------------------------

static VBox *add_credit_words(Score *score, const CreditWordsList &cr_words, const Size &page_size, Fraction tick,
                              const bool is_sibelius_score) {
    VBox *vbox = nullptr;
    const bool top = tick.is_zero();

    std::vector<const CreditWords *> header_words;
    std::vector<const CreditWords *> footer_words;
    for (const CreditWords *w : cr_words) {
        if (w->page == 1) {
            if (w->default_y > (page_size.height() / 2)) {
                header_words.push_back(w);
            } else {
                footer_words.push_back(w);
            }
        }
    }

    std::vector<const CreditWords *> words;
    // if there are more credit words in the footer than in header,
    // swap header and footer, assuming this will result in a vertical
    // frame with the title on top of the page.
    // Sibelius (direct export) typically exports no header
    // and puts the title etc. in the footer
    const bool do_swap = footer_words.size() > header_words.size() && is_sibelius_score;
    if (top) {
        words = do_swap ? footer_words : header_words;
    } else {
        words = do_swap ? header_words : footer_words;
    }

    int miny = 0;
    int maxy = 0;
    find_y_min_y_max_in_words(words, miny, maxy);

    for (const CreditWords *w : words) {
        if (must_add_word_to_vbox(w->type)) {
            const TextStyleType tid = top ? tid_for_credit_words(w, words, page_size.width()) : TextStyleType::DEFAULT;
            const Align align = align_for_credit_words(w, page_size.width(), tid);
            if (!vbox) {
                Fraction v_box_tick = top ? Fraction(0, 1) : tick;
                vbox = MusicXmlParserPass1::create_and_add_v_box_for_credit_words(score, v_box_tick);
            }
            add_text2(vbox, score, w->words, tid, align);
        } else if (w->type == u"rights" && score->meta_tag(u"copyright").empty()) {
            // Add rights to footer, not a vbox
            static const std::regex tag_re("(<.*?>)");
            String rights = w->words;
            rights.remove(tag_re);
            score->set_meta_tag(u"copyright", rights);
        }
    }

    if (vbox && !MScore::test_mode) {
        // Correct size
        resize_title_box(vbox);
    }

    return vbox;
}

//---------------------------------------------------------
//   createMeasuresAndVboxes
//---------------------------------------------------------

void MusicXmlParserPass1::create_default_header(Score *score) {
    String str_title;
    String str_sub_title;
    String inferred_str_sub_title;
    String inferred_str_composer;
    String str_composer;
    String str_lyricist;
    String str_translator;

    if (!(score->meta_tag(u"movementTitle").is_empty() && score->meta_tag(u"workTitle").is_empty())) {
        str_title = score->meta_tag(u"movementTitle");
        if (str_title.is_empty()) {
            str_title = score->meta_tag(u"workTitle");
        }
        infer_from_title(str_title, inferred_str_sub_title, inferred_str_composer);
    }
    if (!(score->meta_tag(u"movementNumber").is_empty() && score->meta_tag(u"workNumber").is_empty())) {
        str_sub_title = score->meta_tag(u"movementNumber");
        if (str_sub_title.is_empty()) {
            str_sub_title = score->meta_tag(u"workNumber");
        }
    }
    if (!inferred_str_sub_title.is_empty()) {
        str_sub_title = inferred_str_sub_title;
        has_inferred_header_text_ = true;
    }
    String meta_composer = score->meta_tag(u"composer");
    String meta_lyricist = score->meta_tag(u"lyricist");
    String meta_translator = score->meta_tag(u"translator");
    if (!meta_composer.is_empty()) {
        str_composer = meta_composer;
    }
    if (!inferred_str_composer.is_empty()) {
        str_composer = inferred_str_composer;
        has_inferred_header_text_ = true;
    }
    if (meta_lyricist.is_empty()) {
        meta_lyricist = score->meta_tag(u"poet");
    }
    if (!meta_lyricist.is_empty()) {
        str_lyricist = meta_lyricist;
    }
    if (!meta_translator.is_empty()) {
        str_translator = meta_translator;
    }

    VBox *const vbox = MusicXmlParserPass1::create_and_add_v_box_for_credit_words(score, Fraction(0, 1));
    vbox->set_exclude_from_other_parts(false);
    add_text(vbox, score, str_title.to_xml_escaped(), TextStyleType::TITLE);
    add_text(vbox, score, str_sub_title.to_xml_escaped(), TextStyleType::SUBTITLE);
    add_text(vbox, score, str_composer.to_xml_escaped(), TextStyleType::COMPOSER);
    add_text(vbox, score, str_lyricist.to_xml_escaped(), TextStyleType::LYRICIST);
    add_text(vbox, score, str_translator.to_xml_escaped(), TextStyleType::TRANSLATOR);
}

//---------------------------------------------------------
//   createMeasuresAndVboxes
//---------------------------------------------------------

/**
 Create required measures with correct number, start tick and length for Score \a score.
 */

void MusicXmlParserPass1::create_measures_and_vboxes(Score *score, const std::vector<Fraction> &ml, const std::vector<Fraction> &ms,
                                                     const std::set<int> &system_start_measure_nrs,
                                                     const std::set<int> &page_start_measure_nrs, const CreditWordsList &cr_words,
                                                     const Size &page_size) {
    if (cr_words.empty()) {
        create_default_header(score);
    }

    int page_nr = 0;
    for (size_t i = 0; i < ml.size(); ++i) {
        VBox *vbox = nullptr;

        // add a header vbox if the this measure is the first in the score or the first on a new page
        if (page_start_measure_nrs.count(int(i)) || i == 0) {
            ++page_nr;

            if (page_nr == 1) {
                vbox = add_credit_words(score, cr_words, page_size, Fraction(0, 0), sib_or_dolet());
                if (i == 0 && vbox) {
                    vbox->set_exclude_from_other_parts(false);
                }
            }
        }

        // create and add the measure
        Measure *measure = Factory::create_measure(score);
        measure->set_tick(ms.at(i));
        measure->set_ticks(ml.at(i));
        measure->set_measure_number(int(i));
        score->measures()->append(measure);

        // add break to previous measure or vbox
        MeasureBase *mb = vbox;
        if (!mb) {
            mb = measure;
        }
        if (page_start_measure_nrs.count(int(i))) {
            add_break_to_previous_measure_base(score, mb, LayoutBreakType::PAGE, configuration()->import_breaks());
        } else if (system_start_measure_nrs.count(int(i))) {
            add_break_to_previous_measure_base(score, mb, LayoutBreakType::LINE, configuration()->import_breaks());
        }

        // add a footer vbox if the next measure is on a new page or end of score has been reached
        if ((page_start_measure_nrs.count(int(i + 1)) || i == (ml.size() - 1)) && page_nr == 1) {
            add_credit_words(score, cr_words, page_size, measure->end_tick(), sib_or_dolet());
        }
    }
}

bool MusicXmlParserPass1::sib_or_dolet() const { return exporter_software_ == MusicXmlExporterSoftware::SIBELIUS || dolet(); }

bool MusicXmlParserPass1::dolet() const {
    return exporter_software_ == MusicXmlExporterSoftware::DOLET6 || exporter_software_ == MusicXmlExporterSoftware::DOLET8;
}

//---------------------------------------------------------
//   determineMeasureStart
//---------------------------------------------------------

/**
 Determine the start ticks of each measure
 i.e. the sum of all previous measures length
 or start tick measure equals start tick previous measure plus length previous measure
 */

static void determine_measure_start(const std::vector<Fraction> &ml, std::vector<Fraction> &ms) {
    ms.resize(ml.size());
    if (!(ms.size() > 0)) {
        return; // no parts read
    }
    // first measure starts at t = 0
    ms[0] = Fraction(0, 1);
    // all others start at start time previous measure plus length previous measure
    for (size_t i = 1; i < ml.size(); i++) {
        ms[i] = ms.at(i - 1) + ml.at(i - 1);
    }
}

//---------------------------------------------------------
//   fixupSigmap
//---------------------------------------------------------

/**
 To enable error handling in pass2, ensure sigmap contains a valid entry at tick = 0.
 Required by TimeSigMap::tickValues(), called (indirectly) by Segment::add().
 */

static void fixup_sigmap(MusicXmlLogger *logger, Score *score, const std::vector<Fraction> &measure_length) {
    auto it = score->sigmap()->find(0);

    if (it == score->sigmap()->end()) {
        // no valid timesig at tick = 0
        logger->log_debug_info(u"no valid time signature at tick = 0");
        // use length of first measure instead time signature.
        // if there is no first measure, we probably don't care,
        // but set a default anyway.
        Fraction tsig = measure_length.empty() ? Fraction(4, 4) : measure_length.at(0);
        score->sigmap()->add(0, tsig);
    }
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Parse MusicXML in \a device and extract pass 1 data.
 */

Err MusicXmlParserPass1::parse(const ByteArray &data) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::parse device");
    parts_.clear();
    e_.set_data(data);
    Err res = parse();
    if (res != Err::NO_ERROR) {
        return res;
    }

    // Determine the start tick of each measure in the part
    determine_measure_length(measure_length_);
    determine_measure_start(measure_length_, measure_start_);
    // Fixup timesig at tick = 0 if necessary
    fixup_sigmap(logger_, score_, measure_length_);
    // Create the measures
    create_measures_and_vboxes(score_, measure_length_, measure_start_, system_start_measure_nrs_, page_start_measure_nrs_, credits_,
                               page_size_);

    return res;
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Start the parsing process, after verifying the top-level node is score-partwise
 */

Err MusicXmlParserPass1::parse() {
    logger_->log_debug_trace(u"MusicXmlParserPass1::parse");

    bool found = false;
    while (e_.read_next_start_element()) {
        if (e_.name() == "score-partwise") {
            found = true;
            score_partwise();
        } else {
            logger_->log_error(
                String(u"this is not a MusicXML score-partwise file (top-level node '%1')").arg(String::from_ascii(e_.name().ascii())),
                &e_);
            e_.skip_current_element();
            return Err::FILE_BAD_FORMAT;
        }
    }

    if (!found) {
        logger_->log_error(u"this is not a MusicXML score-partwise file, node <score-partwise> not found", &e_);
        if (!e_.error_string().is_empty()) {
            errors_ += error_string_with_location(e_.byte_offset(), e_.error_string()) + '\n';
        }
        return Err::FILE_BAD_FORMAT;
    }

    return Err::NO_ERROR;
}

//---------------------------------------------------------
//   allStaffGroupsIdentical
//---------------------------------------------------------

/**
 Return true if all staves in Part \a p have the same staff group
 */

static bool all_staff_groups_identical(Part const *const p) {
    for (size_t i = 1; i < p->nstaves(); ++i) {
        if (p->staff(0)->const_staff_type(Fraction(0, 1))->group() != p->staff(i)->const_staff_type(Fraction(0, 1))->group()) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   isRedundantBracket
//---------------------------------------------------------

/**
 Return true if there is already an existing bracket
 with the same type and span.
 This prevents double brackets, which are sometimes exported
 by Dolet.
 */

static bool is_redundant_bracket(Staff const *const staff, const BracketType bracket_type, const size_t span) {
    for (const BracketItem *bracket : staff->score()->brackets(staff->idx())) {
        if (bracket->bracket_type() == bracket_type && bracket->bracket_span() == span) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   scorePartwise
//---------------------------------------------------------

/**
 Parse the MusicXML top-level (XPath /score-partwise) node.
 */

void MusicXmlParserPass1::score_partwise() {
    logger_->log_debug_trace(u"MusicXmlParserPass1::scorePartwise", &e_);

    MusicXmlPartGroupList part_group_list;

    while (e_.read_next_start_element()) {
        if (e_.name() == "part") {
            part();
        } else if (e_.name() == "part-list") {
            part_list(part_group_list);
        } else if (e_.name() == "work") {
            while (e_.read_next_start_element()) {
                if (e_.name() == "work-number") {
                    score_->set_meta_tag(u"workNumber", e_.read_text());
                } else if (e_.name() == "work-title") {
                    score_->set_meta_tag(u"workTitle", e_.read_text());
                } else {
                    skip_log_curr_elem();
                }
            }
        } else if (e_.name() == "identification") {
            identification();
        } else if (e_.name() == "defaults") {
            defaults();
        } else if (e_.name() == "movement-number") {
            score_->set_meta_tag(u"movementNumber", e_.read_text());
        } else if (e_.name() == "movement-title") {
            score_->set_meta_tag(u"movementTitle", e_.read_text());
        } else if (e_.name() == "credit") {
            credit(credits_);
        } else {
            skip_log_curr_elem();
        }
    }

    // add brackets where required

    /*
     LOGD("partGroupList");
     for (size_t i = 0; i < partGroupList.size(); i++) {
     MusicXmlPartGroup* pg = partGroupList[i];
     LOGD("part-group span %d start %d type %hhd barlinespan %d",
     pg->span, pg->start, pg->type, pg->barlineSpan);
     }
     */

    // set of (typically multi-staff) parts containing one or more explicit brackets
    // spanning only that part: these won't get an implicit brace later
    // e.g. a two-staff piano part with an explicit brace
    std::set<const Part *> part_set;

    // handle the explicit brackets
    const std::vector<Part *> &il = score_->parts();
    for (size_t i = 0; i < part_group_list.size(); i++) {
        MusicXmlPartGroup *pg = part_group_list[i];
        // determine span in staves
        // and span all barlines except last if applicable
        size_t staves_span = 0;
        for (int j = 0; j < pg->span; j++) {
            Part *spanned_part = il.at(pg->start + j);
            staves_span += spanned_part->nstaves();

            if (!pg->barline_span) {
                continue;
            }

            for (Staff *spanned_staff : spanned_part->staves()) {
                if ((j == pg->span - 1) && (spanned_staff == spanned_part->staves().back())) {
                    // Very last staff of group,
                    continue;
                }

                spanned_staff->set_bar_line_span(true);
            }
        }
        // add bracket and set the span
        // TODO: use group-symbol default-x to determine horizontal order of brackets
        Staff *staff = il.at(pg->start)->staff(0);
        if (pg->type != BracketType::NO_BRACKET && !is_redundant_bracket(staff, pg->type, staves_span)) {
            EditStaffBrackets::set_bracket_type(score_, staff->idx(), pg->column, pg->type);
            EditStaffBrackets::set_bracket_span(score_, staff->idx(), pg->column, staves_span);
            if ((score_->brackets(staff->idx()).size() > pg->column) && pg->color.is_valid()) {
                BracketItem *bracket_item = score_->brackets(staff->idx()).at(pg->column);
                bracket_item->set_color(pg->color);
            }
            // add part to set (skip implicit bracket later)
            if (pg->span == 1) {
                part_set.insert(il.at(pg->start));
            }
        }

        if (!staff->is_system_object_staff() && exporter_software() == MusicXmlExporterSoftware::FINALE &&
            configuration()->infer_text_type()) {
            score_->add_system_object_staff(staff);
        }
    }

    // handle the implicit brackets:
    // multi-staff parts w/o explicit brackets get a brace
    for (const Part *p : il) {
        if (p->nstaves() > 1 && !muse::contains(part_set, p)) {
            Staff *staff = p->staff(0);
            const size_t column = score_->bracket_levels(staff->idx()) + 1;
            EditStaffBrackets::set_bracket_type(score_, staff->idx(), column, BracketType::BRACE);
            EditStaffBrackets::set_bracket_span(score_, staff->idx(), column, p->nstaves());
            if (all_staff_groups_identical(p)) {
                // span only if the same types
                for (Staff *spanned_staff : p->staves()) {
                    if (spanned_staff != p->staves().back()) { // not last staff
                        spanned_staff->set_bar_line_span(true);
                    }
                }
            }
        }
    }
    add_error(check_at_end_element(e_, u"score-partwise"));
}

//---------------------------------------------------------
//   identification
//---------------------------------------------------------

/**
 Parse the /score-partwise/identification node:
 read the metadata.
 */

void MusicXmlParserPass1::identification() {
    logger_->log_debug_trace(u"MusicXmlParserPass1::identification", &e_);

    while (e_.read_next_start_element()) {
        if (e_.name() == "creator") {
            // type is an arbitrary label
            String str_type = e_.attribute("type");
            score_->set_meta_tag(str_type, e_.read_text());
        } else if (e_.name() == "rights") {
            score_->set_meta_tag(u"copyright", e_.read_text());
        } else if (e_.name() == "encoding") {
            // TODO
            while (e_.read_next_start_element()) {
                if (e_.name() == "encoder") {
                    score_->set_meta_tag(u"encoder", e_.read_text());
                } else if (e_.name() == "software") {
                    String exporter_string = e_.read_text().to_lower();
                    set_exporter_software(exporter_string);
                    set_exporter_styles();
                } else if (e_.name() == "supports" && e_.ascii_attribute("element") == "beam" && e_.ascii_attribute("type") == "yes") {
                    has_beaming_info_ = true;
                    e_.skip_current_element();
                } else {
                    e_.skip_current_element();
                }
            }
            // _score->setMetaTag("encoding", _e.readText()); works with DOM but not with pull parser
            // temporarily fake the encoding tag (compliant with DOM parser) to help the autotester
            if (MScore::debug_mode) {
                score_->set_meta_tag(u"encoding", u"MuseScore 0.7.02007-09-10");
            }
        } else if (e_.name() == "source") {
            score_->set_meta_tag(u"source", e_.read_text());
        } else if (e_.name() == "miscellaneous") {
            // store all miscellaneous information
            while (e_.read_next_start_element()) {
                if (e_.name() == "miscellaneous-field") {
                    const String name = e_.attribute("name");
                    score_->set_meta_tag(name, e_.read_text());
                } else {
                    e_.skip_current_element();
                }
            }
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   text2syms
//---------------------------------------------------------

/**
 Convert SMuFL code points to MuseScore <sym>...</sym>
 */

static String text2syms(const String &t, const IEngravingFontsProvider &fonts) {
    //QTime time;
    //time.start();

    // first create a map from symbol (Unicode) text to symId
    // note that this takes about 1 msec on a Core i5,
    // caching does not gain much

    IEngravingFontPtr sf = fonts.fallback_font();
    std::map<String, SymId> map;
    size_t max_string_size = 0; // maximum string size found

    for (int i = int(SymId::NO_SYM); i < int(SymId::LAST_SYM); ++i) {
        SymId id = SymId(i);
        String string = sf->to_string(id);
        // insert all syms except space to prevent matching all regular spaces
        if (id != SymId::SPACE) {
            map.insert({string, id});
        }
        if (string.size() > max_string_size) {
            max_string_size = string.size();
        }
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

//---------------------------------------------------------
//   nextPartOfFormattedString
//---------------------------------------------------------

// TODO: probably should be shared between pass 1 and 2

/**
 Read the next part of a MusicXML formatted string and convert to MuseScore internal encoding.
 */

static String next_part_of_formatted_string(XmlStreamReader &e, const IMusicXmlConfiguration &configuration,
                                            const IEngravingFontsProvider &fonts) {
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
    String syms = text2syms(txt, fonts);
    if (override_text_style_for_composer(syms)) {
        return syms;
    }

    String importedtext;

    if (!font_size.is_empty()) {
        bool ok = true;
        float size = font_size.to_float(&ok);
        if (ok) {
            importedtext += String(u"<font size=\"%1\"/>").arg(size);
        }
    }

    bool need_use_default_font = configuration.need_use_default_font();
    if (!font_family.is_empty() && txt == syms && !need_use_default_font) {
        // add font family only if no <sym> replacement made
        importedtext += String(u"<font face=\"%1\"/>").arg(font_family);
    }
    if (font_weight == u"bold") {
        importedtext += u"<b>";
    }
    if (font_style == u"italic") {
        importedtext += u"<i>";
    }
    if (!underline.is_empty()) {
        bool ok = true;
        int lines = underline.to_int(&ok);
        if (ok && (lines > 0)) { // 1, 2, or 3 underlines are imported as single underline
            importedtext += u"<u>";
        } else {
            underline.clear();
        }
    }
    if (!strike.is_empty()) {
        bool ok = true;
        int lines = strike.to_int(&ok);
        if (ok && (lines > 0)) { // 1, 2, or 3 strikes are imported as single strike
            importedtext += u"<s>";
        } else {
            underline.clear();
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

//---------------------------------------------------------
//   credit
//---------------------------------------------------------

/**
 Parse the /score-partwise/credit node:
 read the credits for later handling by doCredits().
 */

void MusicXmlParserPass1::credit(CreditWordsList &credits) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::credit", &e_);

    const int page = e_.int_attribute("page"); // ignoring errors implies incorrect conversion defaults to the first page
    // multiple credit-words elements may be present,
    // which are appended
    // use the position info from the first one
    // font information is ignored, credits will be styled
    bool credit_words_read = false;
    double defaultx = 0;
    double defaulty = 0;
    double font_size = 0;
    String justify;
    String halign;
    String valign;
    StringList crtypes;
    String crwords;
    bool has_rights = false;
    while (e_.read_next_start_element()) {
        if (e_.name() == "credit-words") {
            // IMPORT_LAYOUT
            if (!credit_words_read) {
                defaultx = e_.double_attribute("default-x");
                defaulty = e_.double_attribute("default-y");
                font_size = e_.double_attribute("font-size");
                justify = e_.attribute("justify");
                halign = e_.attribute("halign");
                valign = e_.attribute("valign");
                credit_words_read = true;
            }
            crwords += next_part_of_formatted_string(e_, *configuration(), *score_->engraving_fonts());
        } else if (e_.name() == "credit-type") {
            // multiple credit-type elements may be present, supported by
            // e.g. Finale v26.3 for Mac.
            String type = e_.read_text();
            crtypes.push_back(type);
            has_rights = has_rights || type == u"rights";
        } else {
            skip_log_curr_elem();
        }
    }
    if (!has_rights && is_likely_rights_text(crwords)) {
        crtypes.push_back(u"rights");
    }
    if (!crwords.empty()) {
        // as the meaning of multiple credit-types is undocumented,
        // use credit-type only if exactly one was found
        String crtype = (crtypes.size() == 1) ? crtypes.at(0) : String();
        CreditWords *cw = new CreditWords(page, crtype, defaultx, defaulty, font_size, justify, halign, valign, crwords);
        credits.push_back(cw);
    }
}

//---------------------------------------------------------
//   isTitleFrameStyle
//---------------------------------------------------------

/**
 Determine if tid is a style type used in a title frame
 */

static bool is_title_frame_style(const TextStyleType tid) {
    return tid == TextStyleType::TITLE || tid == TextStyleType::SUBTITLE || tid == TextStyleType::COMPOSER ||
           tid == TextStyleType::LYRICIST;
}

//---------------------------------------------------------
//   isHarpDiagramStyle
//---------------------------------------------------------

/**
 Determine if tid is a style type used in a harp pedal diagram
 */

static bool is_harp_pedal_style(const TextStyleType tid) {
    return tid == TextStyleType::HARP_PEDAL_DIAGRAM || tid == TextStyleType::HARP_PEDAL_TEXT_DIAGRAM;
}

//---------------------------------------------------------
//   updateStyles
//---------------------------------------------------------

/**
 Update the style definitions to match the MusicXML word-font and lyric-font.
 */

static void update_styles(Score *score, const String &word_family, const String &word_size, const String &lyric_family,
                          const String &lyric_size, const IMusicXmlConfiguration &configuration) {
    const double dbl_word_size = word_size.to_double();   // note conversion error results in value 0.0
    const double dbl_lyric_size = lyric_size.to_double(); // but avoid comparing (double) floating point number with exact value later
    const double epsilon = 0.001;                         // use epsilon instead

    bool need_use_default_font = configuration.need_use_default_font();

    // loop over all text styles (except the empty, always hidden, first one)
    // set all text styles to the MusicXML defaults
    for (const TextStyleType tid : all_text_styles()) {
        // The MusicXML specification does not specify to which kinds of text
        // the word-font setting applies. Setting all sizes to the size specified
        // gives bad results, so a selection is made:
        // Only apply word-font style when "Apply default typeface" is unchecked
        // exclude lyrics odd and even lines (handled separately),
        // Roman numeral analysis and harp pedal diagrams (special case, leave untouched)
        // and text types used in the title frame
        // Some further tweaking may still be required.
        if (word_family.empty()) {
            break; // If no font is specified, use default styles
        }

        if (tid == TextStyleType::LYRICS_ODD || tid == TextStyleType::LYRICS_EVEN || tid == TextStyleType::FRET_DIAGRAM_FINGERING ||
            tid == TextStyleType::FRET_DIAGRAM_FRET_NUMBER || tid == TextStyleType::HARMONY_ROMAN ||
            tid == TextStyleType::TAB_FRET_NUMBER) {
            continue;
        }

        bool need_use_default_size = configuration.need_use_default_font() || tid == TextStyleType::HAMMER_ON_PULL_OFF ||
                                     is_title_frame_style(tid) || is_harp_pedal_style(tid);

        const TextStyle *ts = text_style(tid);
        for (const TextStyleProperty &a : *ts) {
            if (a.pid == Pid::FONT_FACE && !need_use_default_font) {
                score->style().set(a.sid, word_family);
            } else if (a.pid == Pid::FONT_SIZE && dbl_word_size > epsilon && !need_use_default_size) {
                score->style().set(a.sid, dbl_word_size);
            }
        }
    }

    // handle lyrics odd and even lines separately
    if (!lyric_family.empty()) {
        if (!need_use_default_font) {
            score->style().set(Sid::LYRICS_ODD_FONT_FACE, lyric_family);
            score->style().set(Sid::LYRICS_EVEN_FONT_FACE, lyric_family);
        }
        if (dbl_lyric_size > epsilon) {
            score->style().set(Sid::LYRICS_ODD_FONT_SIZE, dbl_lyric_size);
            score->style().set(Sid::LYRICS_EVEN_FONT_SIZE, dbl_lyric_size);
        }
    }
}

//---------------------------------------------------------
//   setPageFormat
//---------------------------------------------------------

static void set_page_format(Score *score, const PageFormat &pf) {
    score->style().set(Sid::PAGE_WIDTH, pf.size.width());
    score->style().set(Sid::PAGE_HEIGHT, pf.size.height());
    score->style().set(Sid::PAGE_PRINTABLE_WIDTH, pf.printable_width);
    score->style().set(Sid::PAGE_EVEN_LEFT_MARGIN, pf.even_left_margin);
    score->style().set(Sid::PAGE_ODD_LEFT_MARGIN, pf.odd_left_margin);
    score->style().set(Sid::PAGE_EVEN_TOP_MARGIN, pf.even_top_margin);
    score->style().set(Sid::PAGE_EVEN_BOTTOM_MARGIN, pf.even_bottom_margin);
    score->style().set(Sid::PAGE_ODD_TOP_MARGIN, pf.odd_top_margin);
    score->style().set(Sid::PAGE_ODD_BOTTOM_MARGIN, pf.odd_bottom_margin);
    score->style().set(Sid::PAGE_TWOSIDED, pf.twosided);
}

static double scale_text(const String &str, const Sid font_face_sid, const double font_size, const Score *score) {
    // Scale text to fit within margins
    const MStyle style = score->style();
    const String font_face = style.style_v(font_face_sid).value<String>();
    Font font(font_face, Font::Type::UNKNOWN);
    font.set_point_size_f(font_size);
    const FontMetrics fm(font);

    const double page_printable_width = style.style_v(Sid::PAGE_PRINTABLE_WIDTH).value<double>() * ::mu::engraving::dpi;

    double longest_line = 0.0;
    for (const String &line : str.split(u"\n")) {
        const double text_width = fm.bounding_rect(line).width();
        longest_line = std::max(longest_line, text_width);
    }

    return page_printable_width / longest_line;
}

static void scale_copyright_text(Score *score) {
    String copyright = score->meta_tag(u"copyright");
    if (copyright.empty()) {
        return;
    }

    const double font_size = score->style().style_v(Sid::COPYRIGHT_FONT_SIZE).value<double>();
    const double size_ratio = scale_text(copyright, Sid::COPYRIGHT_FONT_FACE, font_size, score);

    if (size_ratio < 1) {
        const double new_size = floor(font_size * size_ratio * 10) / 10;
        score->style().set(Sid::COPYRIGHT_FONT_SIZE, new_size);
    }
}

static void scale_title(Score *score, Text *t) {
    String title = score->meta_tag(u"workTitle");
    if (title.empty()) {
        return;
    }

    const double font_size = score->style().style_v(Sid::TITLE_FONT_SIZE).value<double>();
    const double size_ratio = scale_text(title, Sid::TITLE_FONT_FACE, font_size, score);

    if (size_ratio < 1) {
        const double new_size = floor(font_size * size_ratio * 10) / 10;
        // Need to layout text to generate fragments before changing its size
        TLayout::layout_text(t, t->mutldata());
        t->set_property(Pid::FONT_SIZE, new_size);
        t->set_property_flags(Pid::FONT_SIZE, PropertyFlags::UNSTYLED);
    }
}

//---------------------------------------------------------
//   defaults
//---------------------------------------------------------

/**
 Parse the /score-partwise/defaults node:
 read the general score layout settings.
 */

void MusicXmlParserPass1::defaults() {
    //_logger->logDebugTrace("MusicXmlParserPass1::defaults", &_e);

    double millimeter = score_->style().spatium() / 10.0;
    double tenths = 1.0;
    String lyric_font_family;
    String lyric_font_size;
    String word_font_family;
    String word_font_size;

    bool is_import_layout = configuration()->import_layout();

    while (e_.read_next_start_element()) {
        if (e_.name() == "scaling") {
            while (e_.read_next_start_element()) {
                if (e_.name() == "millimeters") {
                    millimeter = e_.read_double();
                } else if (e_.name() == "tenths") {
                    tenths = e_.read_double();
                } else {
                    skip_log_curr_elem();
                }
            }
            double spatium_value = ::mu::engraving::dpmm * (millimeter * 10.0 / tenths);
            if (is_import_layout) {
                score_->style().set_spatium(spatium_value);
            }
        } else if (e_.name() == "concert-score") {
            score_->style().set(Sid::CONCERT_PITCH, true);
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "page-layout") {
            PageFormat pf;
            page_layout(pf, millimeter / (tenths * ::mu::engraving::inch));
            if (is_import_layout) {
                set_page_format(score_, pf);
            }
        } else if (e_.name() == "system-layout") {
            while (e_.read_next_start_element()) {
                if (e_.name() == "system-margins") {
                    e_.skip_current_element(); // skip but don't log
                } else if (e_.name() == "system-distance") {
                    const Spatium val(e_.read_double() / 10.0);
                    if (is_import_layout) {
                        score_->style().set(Sid::MIN_SYSTEM_DISTANCE, val);
                        //LOGD("system distance %f", val.val());
                    }
                } else if (e_.name() == "top-system-distance") {
                    e_.skip_current_element(); // skip but don't log
                } else if (e_.name() == "system-dividers") {
                    while (e_.read_next_start_element()) {
                        if (e_.name() == "left-divider") {
                            score_->style().set(Sid::DIVIDER_LEFT, (e_.ascii_attribute("print-object") != "no"));
                            if (is_import_layout) {
                                score_->style().set(Sid::DIVIDER_LEFT_X, e_.double_attribute("relative-x") / 10.0);
                                score_->style().set(Sid::DIVIDER_LEFT_Y, e_.double_attribute("relative-y") / 10.0);
                            }
                            e_.skip_current_element();
                        } else if (e_.name() == "right-divider") {
                            score_->style().set(Sid::DIVIDER_RIGHT, (e_.ascii_attribute("print-object") != "no"));
                            if (is_import_layout) {
                                score_->style().set(Sid::DIVIDER_RIGHT_X, e_.double_attribute("relative-x") / 10.0);
                                score_->style().set(Sid::DIVIDER_RIGHT_Y, e_.double_attribute("relative-y") / 10.0);
                            }
                            e_.skip_current_element();
                        } else {
                            skip_log_curr_elem();
                        }
                    }
                } else {
                    skip_log_curr_elem();
                }
            }
        } else if (e_.name() == "staff-layout") {
            while (e_.read_next_start_element()) {
                if (e_.name() == "staff-distance") {
                    const Spatium val(e_.read_double() / 10.0);
                    if (is_import_layout) {
                        score_->style().set(Sid::STAFF_DISTANCE, val);
                    }
                } else {
                    skip_log_curr_elem();
                }
            }
        } else if (e_.name() == "appearance") {
            while (e_.read_next_start_element()) {
                const String type = e_.attribute("type");
                if (e_.name() == "line-width") {
                    const double val = e_.read_double();
                    if (is_import_layout) {
                        set_style(type, val);
                    }
                } else if (e_.name() == "note-size") {
                    const double val = e_.read_double();
                    if (is_import_layout) {
                        set_style(type, val);
                    }
                } else if (e_.name() == "distance") {
                    e_.skip_current_element(); // skip but don't log
                } else if (e_.name() == "glyph") {
                    e_.skip_current_element(); // skip but don't log
                } else if (e_.name() == "other-appearance") {
                    e_.skip_current_element(); // skip but don't log
                } else {
                    skip_log_curr_elem();
                }
            }
        } else if (e_.name() == "music-font") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "word-font") {
            word_font_family = e_.attribute("font-family");
            word_font_size = e_.attribute("font-size");
            e_.skip_current_element();
        } else if (e_.name() == "lyric-font") {
            lyric_font_family = e_.attribute("font-family");
            lyric_font_size = e_.attribute("font-size");
            e_.skip_current_element();
        } else if (e_.name() == "lyric-language") {
            e_.skip_current_element(); // skip but don't log
        } else {
            skip_log_curr_elem();
        }
    }

    /*
    LOGD("word font family '%s' size '%s' lyric font family '%s' size '%s'",
           muPrintable(wordFontFamily), muPrintable(wordFontSize),
           muPrintable(lyricFontFamily), muPrintable(lyricFontSize));
    */
    lyric_font_family = lyric_font_family.empty() ? word_font_family : lyric_font_family;
    update_styles(score_, word_font_family, word_font_size, lyric_font_family, lyric_font_size, *configuration());

    scale_copyright_text(score_);
}

//---------------------------------------------------------
//   setStyle
//---------------------------------------------------------

void MusicXmlParserPass1::set_style(const String &type, const double val) {
    if (type == u"light barline") {
        score_->style().set(Sid::BAR_WIDTH, Spatium(val / 10));
    } else if (type == u"heavy barline") {
        score_->style().set(Sid::END_BAR_WIDTH, Spatium(val / 10));
    } else if (type == u"beam") {
        score_->style().set(Sid::BEAM_WIDTH, Spatium(val / 10));
    } else if (type == u"bracket") {
        score_->style().set(Sid::BRACKET_WIDTH, Spatium(val / 10));
    } else if (type == u"dashes") {
        score_->style().set(Sid::LYRICS_DASH_LINE_THICKNESS, Spatium(val / 10));
    } else if (type == u"enclosure") {
        score_->style().set(Sid::STAFF_TEXT_FRAME_WIDTH, Spatium(val / 10));
    } else if (type == u"ending") {
        score_->style().set(Sid::VOLTA_LINE_WIDTH, Spatium(val / 10));
    } else if (type == u"extend") {
        score_->style().set(Sid::LYRICS_LINE_THICKNESS, Spatium(val / 10));
    } else if (type == u"leger") {
        score_->style().set(Sid::LEDGER_LINE_WIDTH, Spatium(val / 10));
    } else if (type == u"pedal") {
        score_->style().set(Sid::PEDAL_LINE_WIDTH, Spatium(val / 10));
    } else if (type == "octave shift") {
        score_->style().set(Sid::OTTAVA_LINE_WIDTH, Spatium(val / 10));
    } else if (type == u"staff") {
        score_->style().set(Sid::STAFF_LINE_WIDTH, Spatium(val / 10));
    } else if (type == u"stem") {
        score_->style().set(Sid::STEM_WIDTH, Spatium(val / 10));
    } else if (type == u"tuplet bracket") {
        score_->style().set(Sid::TUPLET_BRACKET_WIDTH, Spatium(val / 10));
    } else if (type == u"wedge") {
        score_->style().set(Sid::HAIRPIN_LINE_WIDTH, Spatium(val / 10));
    } else if (type == u"slur middle") {
        score_->style().set(Sid::SLUR_MID_WIDTH, Spatium(val / 10));
    } else if (type == u"slur tip") {
        score_->style().set(Sid::SLUR_END_WIDTH, Spatium(val / 10));
    } else if (type == u"tie middle") {
        score_->style().set(Sid::TIE_MID_WIDTH, Spatium(val / 10));
    } else if (type == u"tie tip") {
        score_->style().set(Sid::TIE_END_WIDTH, Spatium(val / 10));
    } else if ((type == u"cue")) {
        score_->style().set(Sid::SMALL_NOTE_MAG, val / 100);
    } else if ((type == u"grace")) {
        score_->style().set(Sid::GRACE_NOTE_MAG, val / 100);
    } else if ((type == u"grace-cue")) {
        // not supported
    }
}

//---------------------------------------------------------
//   pageLayout
//---------------------------------------------------------

/**
 Parse the /score-partwise/defaults/page-layout node: read the page layout.
 Note that MuseScore does not support a separate value for left and right margins
 for odd and even pages. Only odd and even left margins are used, together  with
 the printable width, which is calculated from the left and right margins in the
 MusicXML file.
 */

void MusicXmlParserPass1::page_layout(PageFormat &pf, const double conversion) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::pageLayout", &e_);

    double odd_right_margin = 0.0;
    double even_right_margin = 0.0;
    SizeF size;

    while (e_.read_next_start_element()) {
        if (e_.name() == "page-margins") {
            String type = e_.attribute("type");
            if (type.empty()) {
                type = u"both";
            }
            double lm = 0.0, rm = 0.0, tm = 0.0, bm = 0.0;
            while (e_.read_next_start_element()) {
                if (e_.name() == "left-margin") {
                    lm = e_.read_double() * conversion;
                } else if (e_.name() == "right-margin") {
                    rm = e_.read_double() * conversion;
                } else if (e_.name() == "top-margin") {
                    tm = e_.read_double() * conversion;
                } else if (e_.name() == "bottom-margin") {
                    bm = e_.read_double() * conversion;
                } else {
                    skip_log_curr_elem();
                }
            }
            pf.twosided = type == "odd" || type == "even";
            if (type == "odd" || type == "both") {
                pf.odd_left_margin = lm;
                odd_right_margin = rm;
                pf.odd_top_margin = tm;
                pf.odd_bottom_margin = bm;
            }
            if (type == "even" || type == "both") {
                pf.even_left_margin = lm;
                even_right_margin = rm;
                pf.even_top_margin = tm;
                pf.even_bottom_margin = bm;
            }
        } else if (e_.name() == "page-height") {
            const double val = e_.read_double();
            size.set_height(val * conversion);
            // set pageHeight and pageWidth for use by doCredits()
            page_size_.set_height(static_cast<int>(val + 0.5));
        } else if (e_.name() == "page-width") {
            const double val = e_.read_double();
            size.set_width(val * conversion);
            // set pageHeight and pageWidth for use by doCredits()
            page_size_.set_width(static_cast<int>(val + 0.5));
        } else {
            skip_log_curr_elem();
        }
    }
    pf.size = size;
    double w1 = size.width() - pf.odd_left_margin - odd_right_margin;
    double w2 = size.width() - pf.even_left_margin - even_right_margin;
    pf.printable_width = std::max(w1, w2); // silently adjust right margins
}

//---------------------------------------------------------
//   partList
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list:
 create the parts and for each part set id and name.
 Also handle the part-groups.
 */

void MusicXmlParserPass1::part_list(MusicXmlPartGroupList &part_group_list) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::partList", &e_);

    int score_parts = 0; // number of score-parts read sofar
    MusicXmlPartGroupMap part_groups;
    String cur_part_group_name;

    while (e_.read_next_start_element()) {
        if (e_.name() == "part-group") {
            part_group(score_parts, part_group_list, part_groups, cur_part_group_name);
        } else if (e_.name() == "score-part") {
            score_part(cur_part_group_name);
            score_parts++;
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   createPart
//---------------------------------------------------------

/**
 Create the part, set its \a id and insert it in PartMap \a pm.
 Part name (if any) will be set later.
 */

static void create_part(Score *score, const String &id, PartMap &pm) {
    Part *part = new Part(score);
    pm.insert({id, part});
    score->append_part(part);
    Staff *staff = Factory::create_staff(part);
    score->append_staff(staff);
}

//---------------------------------------------------------
//   partGroupStart
//---------------------------------------------------------

typedef std::map<int, MusicXmlPartGroup *> MusicXmlPartGroupMap;

/**
 Store part-group start with number \a n, first part \a p and symbol / \a s in the partGroups
 map \a pgs for later reference, as at this time insufficient information is available to be able
 to generate the brackets.
 */

static void part_group_start(MusicXmlPartGroupMap &pgs, int n, int p, const String &s, bool barline_span, Color color) {
    //LOGD("partGroupStart number=%d part=%d symbol=%s", n, p, muPrintable(s));

    if (pgs.count(n) > 0) {
        LOGD("part-group number=%d already active", n);
        return;
    }

    BracketType bracket_type = BracketType::NO_BRACKET;
    if (s.empty()) {
        // ignore (handle as NO_BRACKET)
    } else if (s == u"none") {
        // already set to NO_BRACKET
    } else if (s == u"brace") {
        bracket_type = BracketType::BRACE;
    } else if (s == u"bracket") {
        bracket_type = BracketType::NORMAL;
    } else if (s == u"line") {
        bracket_type = BracketType::LINE;
    } else if (s == u"square") {
        bracket_type = BracketType::SQUARE;
    } else {
        LOGD("part-group symbol=%s not supported", muPrintable(s));
        return;
    }

    MusicXmlPartGroup *pg = new MusicXmlPartGroup;
    pg->span = 0;
    pg->start = p;
    pg->barline_span = barline_span, pg->type = bracket_type;
    if (color.is_valid()) {
        pg->color = color;
    }
    pg->column = static_cast<size_t>(n);
    pgs[n] = pg;
}

//---------------------------------------------------------
//   partGroupStop
//---------------------------------------------------------

/**
 Handle part-group stop with number \a n and part \a p.

 For part group n, the start part, span (in parts) and type are now known.
 To generate brackets, the span in staves must also be known.
 */

static void part_group_stop(MusicXmlPartGroupMap &pgs, int n, int p, MusicXmlPartGroupList &pgl) {
    if (pgs.count(n) == 0) {
        LOGD("part-group number=%d not active", n);
        return;
    }

    pgs[n]->span = p - pgs[n]->start;
    //LOGD("partgroupstop number=%d start=%d span=%d type=%hhd",
    //       n, pgs[n]->start, pgs[n]->span, pgs[n]->type);
    pgl.push_back(pgs[n]);
    pgs.erase(n);
}

//---------------------------------------------------------
//   partGroup
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/part-group node.
 */

void MusicXmlParserPass1::part_group(const int score_parts, MusicXmlPartGroupList &part_group_list, MusicXmlPartGroupMap &part_groups,
                                     String &cur_part_group_name) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::partGroup", &e_);
    bool barline_span = true;
    int number = e_.int_attribute("number");
    if (number > 0) {
        number--;
    }
    String symbol;
    String type = e_.attribute("type");
    Color symbol_color;

    while (e_.read_next_start_element()) {
        if (e_.name() == "group-name") {
            cur_part_group_name = e_.read_text();
        } else if (e_.name() == "group-abbreviation") {
            symbol = e_.read_text();
        } else if (e_.name() == "group-symbol") {
            symbol_color = Color::from_string(e_.attribute("color"));
            symbol = e_.read_text();
        } else if (e_.name() == "group-barline") {
            if (e_.read_text() == "no") {
                barline_span = false;
            }
        } else if (e_.name() == "group-time") {
            score_->style().set(Sid::TIME_SIG_PLACEMENT, TimeSigPlacement::ACROSS_STAVES);
            e_.skip_current_element(); // skip but don't log
        } else {
            skip_log_curr_elem();
        }
    }

    if (type == "start") {
        part_group_start(part_groups, number, score_parts, symbol, barline_span, symbol_color);
    } else if (type == "stop") {
        part_group_stop(part_groups, number, score_parts, part_group_list);
    } else {
        logger_->log_error(String(u"part-group type '%1' not supported").arg(type), &e_);
    }
}

//---------------------------------------------------------
//   scorePart
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/score-part node:
 create the part and sets id and name.
 Note that a part is created even if no part-name is present
 which is invalid MusicXML but is (sometimes ?) generated by NWC2MusicXml.
 */

void MusicXmlParserPass1::score_part(const String &cur_part_group_name) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::scorePart", &e_);
    String id = e_.attribute("id").trimmed();

    if (muse::contains(parts_, id)) {
        logger_->log_error(String(u"duplicate part id '%1'").arg(id), &e_);
        skip_log_curr_elem();
        return;
    } else {
        parts_.insert({id, MusicXmlPart(id)});
        instruments_.insert({id, MusicXmlInstruments()});
        create_part(score_, id, part_map_);
    }

    while (e_.read_next_start_element()) {
        if (e_.name() == "part-name") {
            // EngravingItem part-name contains the displayed (full) part name
            // It is displayed by default, but can be suppressed (print-object=”no”)
            // As of MusicXML 3.0, formatting is deprecated, with part-name in plain text
            // and the formatted version in the part-name-display element
            parts_[id].set_print_name(e_.ascii_attribute("print-object") != "no");
            String name = e_.read_text();
            parts_[id].set_name(name);
        } else if (e_.name() == "part-name-display") {
            parts_[id].set_print_name(e_.ascii_attribute("print-object") != "no");
            String name;
            while (e_.read_next_start_element()) {
                if (e_.name() == "display-text") {
                    name += e_.read_text();
                } else if (e_.name() == "accidental-text") {
                    name += music_xml_accidental_text_to_char(e_.read_text());
                } else {
                    skip_log_curr_elem();
                }
            }
            if (!name.empty()) {
                parts_[id].set_name(name);
            }
        } else if (e_.name() == "part-abbreviation") {
            // EngravingItem part-name contains the displayed (abbreviated) part name
            // It is displayed by default, but can be suppressed (print-object=”no”)
            // As of MusicXML 3.0, formatting is deprecated, with part-name in plain text
            // and the formatted version in the part-abbreviation-display element
            parts_[id].set_print_abbr(e_.ascii_attribute("print-object") != "no");
            String name = e_.read_text();
            parts_[id].set_abbr(name);
        } else if (e_.name() == "part-abbreviation-display") {
            parts_[id].set_print_abbr(e_.ascii_attribute("print-object") != "no");
            String name;
            while (e_.read_next_start_element()) {
                if (e_.name() == "display-text") {
                    name += e_.read_text();
                } else if (e_.name() == "accidental-text") {
                    name += music_xml_accidental_text_to_char(e_.read_text());
                } else {
                    skip_log_curr_elem();
                }
            }
            if (!name.empty()) {
                parts_[id].set_abbr(name);
            }
        } else if (e_.name() == "group") {
            // TODO
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "score-instrument") {
            score_instrument(id, cur_part_group_name);
        } else if (e_.name() == "player") {
            // unsupported
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "midi-device") {
            if (!e_.has_attribute("port")) {
                e_.read_text(); // empty string
                continue;
            }
            String instr_id = e_.attribute("id");
            String port = e_.attribute("port");
            // If instrId is missing, the device assignment affects all
            // score-instrument elements in the score-part
            if (instr_id.is_empty()) {
                for (auto it = instruments_[id].cbegin(); it != instruments_[id].cend(); ++it) {
                    instruments_[id][it->first].midi_port = port.to_int() - 1;
                }
            } else if (muse::contains(instruments_.at(id), instr_id)) {
                instruments_[id][instr_id].midi_port = port.to_int() - 1;
            }

            e_.read_text(); // empty string
        } else if (e_.name() == "midi-instrument") {
            midi_instrument(id);
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   scoreInstrument
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/score-part/score-instrument node.
 */

void MusicXmlParserPass1::score_instrument(const String &part_id, const String &cur_part_group_name) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::scoreInstrument", &e_);
    String instr_id = e_.attribute("id");

    while (e_.read_next_start_element()) {
        if (e_.name() == "ensemble") {
            skip_log_curr_elem();
        } else if (e_.name() == "instrument-name") {
            String instr_name = e_.read_text();
            /*
            LOGD("partId '%s' instrId '%s' instrName '%s'",
                   muPrintable(partId),
                   muPrintable(instrId),
                   muPrintable(instrName)
                   );
             */

            // Finale exports all instrument names as 'Grand Piano' - use part name
            if (exporter_software() == MusicXmlExporterSoftware::FINALE) {
                instr_name = parts_[part_id].get_name();
                if (instr_name.size() <= 1) {
                    instr_name = cur_part_group_name;
                }
            }
            instruments_[part_id].insert({instr_id, MusicXmlInstrument(instr_name)});
            // EngravingItem instrument-name is typically not displayed in the score,
            // but used only internally
            if (muse::contains(instruments_.at(part_id), instr_id)) {
                instruments_[part_id][instr_id].name = instr_name;
            }
        } else if (e_.name() == "instrument-sound") {
            String instr_sound = e_.read_text();
            if (muse::contains(instruments_.at(part_id), instr_id)) {
                instruments_[part_id][instr_id].sound = instr_sound;
            }
        } else if (e_.name() == "virtual-instrument") {
            while (e_.read_next_start_element()) {
                if (e_.name() == "virtual-library") {
                    String virtual_library = e_.read_text();
                    if (muse::contains(instruments_.at(part_id), instr_id)) {
                        instruments_[part_id][instr_id].virt_lib = virtual_library;
                    }
                } else if (e_.name() == "virtual-name") {
                    String virtual_name = e_.read_text();
                    if (muse::contains(instruments_.at(part_id), instr_id)) {
                        instruments_[part_id][instr_id].virt_name = virtual_name;
                    }
                } else {
                    skip_log_curr_elem();
                }
            }
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   midiInstrument
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/score-part/midi-instrument node.
 */

void MusicXmlParserPass1::midi_instrument(const String &part_id) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::midiInstrument", &e_);
    String instr_id = e_.attribute("id");

    while (e_.read_next_start_element()) {
        if (e_.name() == "midi-bank") {
            skip_log_curr_elem();
        } else if (e_.name() == "midi-channel") {
            int channel = e_.read_int();
            if (channel < 1) {
                logger_->log_error(String(u"incorrect midi-channel: %1").arg(channel), &e_);
                channel = 1;
            } else if (channel > 16) {
                logger_->log_error(String(u"incorrect midi-channel: %1").arg(channel), &e_);
                channel = 16;
            }
            if (muse::contains(instruments_.at(part_id), instr_id)) {
                instruments_[part_id][instr_id].midi_channel = channel - 1;
            }
        } else if (e_.name() == "midi-program") {
            int program = e_.read_int();
            // Bug fix for Cubase 6.5.5 which generates <midi-program>0</midi-program>
            // Check program number range
            if (program < 1) {
                logger_->log_error(String(u"incorrect midi-program: %1").arg(program), &e_);
                program = 1;
            } else if (program > 128) {
                logger_->log_error(String(u"incorrect midi-program: %1").arg(program), &e_);
                program = 128;
            }
            if (muse::contains(instruments_.at(part_id), instr_id)) {
                instruments_[part_id][instr_id].midi_program = program - 1;
            }
        } else if (e_.name() == "midi-unpitched") {
            if (muse::contains(instruments_.at(part_id), instr_id)) {
                instruments_[part_id][instr_id].unpitched = e_.read_int() - 1;
            }
        } else if (e_.name() == "volume") {
            double vol = e_.read_double();
            if (vol >= 0 && vol <= 100) {
                if (muse::contains(instruments_.at(part_id), instr_id)) {
                    instruments_[part_id][instr_id].midi_volume = static_cast<int>((vol / 100) * 127);
                }
            } else {
                logger_->log_error(String(u"incorrect midi-volume: %1").arg(vol), &e_);
            }
        } else if (e_.name() == "pan") {
            double pan = e_.read_double();
            if (pan >= -90 && pan <= 90) {
                if (muse::contains(instruments_.at(part_id), instr_id)) {
                    instruments_[part_id][instr_id].midi_pan = static_cast<int>(((pan + 90) / 180) * 127);
                }
            } else {
                logger_->log_error(String(u"incorrect midi-volume: %g1").arg(pan), &e_);
            }
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   setNumberOfStavesForPart
//---------------------------------------------------------

/**
 Set number of staves for part \a partId to the max value
 of the current value \a staves.
 Also handle HideMode.
 */

static void set_number_of_staves_for_part(Part *const part, const size_t staves) {
    IF_ASSERT_FAILED(part) { return; }

    size_t prevnstaves = part->nstaves();
    if (staves > part->nstaves()) {
        part->set_staves(static_cast<int>(staves));
    }
    if (staves != 0 && prevnstaves != 1 && prevnstaves != staves) {
        // A "staves" value different from the existing nstaves means
        // staves in a part will sometimes be hidden.
        // We can approximate this in the following way:
        part->set_hide_staves_when_individually_empty(true);
    }
}

//---------------------------------------------------------
//   part
//---------------------------------------------------------

/**
 Parse the /score-partwise/part node:
 read the parts data to determine measure timing and octave shifts.
 Assign voices and staves.
 */

void MusicXmlParserPass1::part() {
    logger_->log_debug_trace(u"MusicXmlParserPass1::part", &e_);
    const String id = e_.attribute("id").trimmed();

    if (!muse::contains(parts_, id)) {
        logger_->log_error(String(u"cannot find part '%1'").arg(id), &e_);
        skip_log_curr_elem();
        return;
    }

    init_part_state(id);

    VoiceOverlapDetector vod;
    Fraction time; // current time within part
    Fraction mdur; // measure duration

    int measure_nr = 0;
    while (e_.read_next_start_element()) {
        if (e_.name() == "measure") {
            measure(id, time, mdur, vod, measure_nr);
            time += mdur;
            ++measure_nr;
        } else {
            skip_log_curr_elem();
        }
    }

    // Bug fix for Cubase 6.5.5..9.5.10 which generate <staff>2</staff> in a single staff part
    set_number_of_staves_for_part(muse::value(part_map_, id), parts_[id].max_staff() + 1);
    // allocate MuseScore staff to MusicXML voices
    allocate_staves(parts_[id].voicelist);
    // allocate MuseScore voice to MusicXML voices
    allocate_voices(parts_[id].voicelist);
    // calculate the octave shifts
    parts_[id].calc_octave_shifts();
    // determine the lyric numbers for this part
    parts_[id].lyric_number_handler().determine_lyric_nos();
}

//---------------------------------------------------------
//   measureDurationAsFraction
//---------------------------------------------------------

/**
 Determine a suitable measure duration value given the time signature
 by setting the duration denominator to be greater than or equal
 to the time signature denominator
 */

static Fraction measure_duration_as_fraction(const Fraction length, const int tsigtype) {
    if (tsigtype <= 0) {
        // invalid tsigtype
        return length;
    }

    Fraction res = length;
    while (res.denominator() < tsigtype) {
        res.set_numerator(res.numerator() * 2);
        res.set_denominator(res.denominator() * 2);
    }
    return res;
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure node:
 read the measures data as required to determine measure timing, octave shifts
 and assign voices and staves.
 */

void MusicXmlParserPass1::measure(const String &part_id, const Fraction c_time, Fraction &mdur, VoiceOverlapDetector &vod,
                                  const int measure_nr) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::measure", &e_);
    String number = e_.attribute("number");

    Fraction m_time; // current time stamp within measure
    Fraction m_dura; // current total measure duration
    vod.new_measure();
    MusicXmlTupletStates tuplet_states;

    while (e_.read_next_start_element()) {
        if (e_.name() == "attributes") {
            attributes(part_id, c_time + m_time);
        } else if (e_.name() == "barline") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "note") {
            Fraction missing_prev;
            Fraction dura;
            Fraction missing_curr;
            // note: chord and grace note handling done in note()
            note(part_id, c_time + m_time, missing_prev, dura, missing_curr, vod, tuplet_states);
            if (missing_prev.is_valid()) {
                m_time += missing_prev;
            }
            if (dura.is_valid()) {
                m_time += dura;
            }
            if (missing_curr.is_valid()) {
                m_time += missing_curr;
            }
            if (m_time > m_dura) {
                m_dura = m_time;
            }
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
            }
        } else if (e_.name() == "direction") {
            direction(part_id, c_time + m_time);
        } else if (e_.name() == "harmony") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "print") {
            print(measure_nr);
        } else if (e_.name() == "sound") {
            e_.skip_current_element(); // skip but don't log
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
    }

    // debug vod
    // vod.dump();
    // copy overlap data from vod to voicelist
    copy_overlap_data(vod, parts_[part_id].voicelist);

    // measure duration fixups
    m_dura.reduce();

    // fix for PDFtoMusic Pro v1.3.0d Build BF4E and PlayScore / ReadScoreLib Version 3.11
    // which sometimes generate empty measures
    // if no valid length found and length according to time signature is known,
    // use length according to time signature
    if (m_dura.is_zero() && time_sig_dura_.is_valid() && time_sig_dura_ > Fraction(0, 1)) {
        m_dura = time_sig_dura_;
    }
    // if no valid length found and time signature is unknown, use default
    if (m_dura.is_zero() && !time_sig_dura_.is_valid()) {
        m_dura = Fraction(4, 4);
    }

    // if necessary, round up to an integral number of 1/128th,
    // to comply with MuseScores actual measure length constraints
    Fraction length = m_dura * Fraction(128, 1);
    Fraction corrected_length = m_dura;
    length.reduce();
    if (length.denominator() != 1) {
        Fraction round_down = Fraction(length.numerator() / length.denominator(), 128);
        Fraction round_up = Fraction(length.numerator() / length.denominator() + 1, 128);
        // mDura is not an integer multiple of 1/128;
        // first check if the duration is larger than an integer multiple of 1/128
        // by an amount smaller than the minimum division resolution
        // in that case, round down (rounding errors have possibly occurred),
        // otherwise, round up
        if ((divs_ > 0) && ((m_dura - round_down) < Fraction(1, 4 * divs_))) {
            logger_->log_error(String(u"rounding down measure duration %1 to %2").arg(m_dura.to_string(), round_down.to_string()), &e_);
            corrected_length = round_down;
        } else {
            logger_->log_error(String(u"rounding up measure duration %1 to %2").arg(m_dura.to_string(), round_up.to_string()), &e_);
            corrected_length = round_up;
        }
        m_dura = corrected_length;
    }

    // set measure duration to a suitable value given the time signature
    if (time_sig_dura_.is_valid() && time_sig_dura_ > Fraction(0, 1)) {
        int btp = time_sig_dura_.denominator();
        if (btp > 0) {
            m_dura = measure_duration_as_fraction(m_dura, btp);
        }
    }

    // set return value(s)
    mdur = m_dura;

    // set measure number and duration
    /*
    LOGD("part %s measure %s dura %s (%d)",
           muPrintable(partId), muPrintable(number), muPrintable(mdur.print()), mdur.ticks());
     */
    parts_[part_id].add_measure_number_and_duration(number, mdur);

    add_error(check_at_end_element(e_, u"measure"));
}

//---------------------------------------------------------
//   print
//---------------------------------------------------------

void MusicXmlParserPass1::print(const int measure_nr) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::print", &e_);

    const String new_page = e_.attribute("new-page");
    const String new_system = e_.attribute("new-system");
    if (new_page == u"yes") {
        page_start_measure_nrs_.insert(measure_nr);
    }
    if (new_system == u"yes") {
        system_start_measure_nrs_.insert(measure_nr);
    }

    e_.skip_current_element(); // skip but don't log
}

//---------------------------------------------------------
//   attributes
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes node.
 */

void MusicXmlParserPass1::attributes(const String &part_id, const Fraction c_time) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::attributes", &e_);

    int staves = 0;
    std::set<int> hidden_staves = {};

    while (e_.read_next_start_element()) {
        if (e_.name() == "clef") {
            clef(part_id);
        } else if (e_.name() == "divisions") {
            divisions();
        } else if (e_.name() == "key") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "instruments") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "staff-details") {
            if (e_.ascii_attribute("print-object") == "no") {
                hidden_staves.emplace(e_.int_attribute("number"));
            }
            e_.skip_current_element();
        } else if (e_.name() == "staves") {
            staves = e_.read_int();
        } else if (e_.name() == "time") {
            time(c_time);
        } else if (e_.name() == "transpose") {
            transpose(part_id, c_time);
        } else {
            skip_log_curr_elem();
        }
    }

    if (staves - static_cast<int>(hidden_staves.size()) > ::mu::engraving::max_staves) {
        logger_->log_error(u"staves exceed MAX_STAVES, even when discarding hidden staves", &e_);
        return;
    } else if (staves > ::mu::engraving::max_staves && static_cast<int>(hidden_staves.size()) > 0 &&
               parts_[part_id].staff_number_to_index().size() == 0) {
        logger_->log_error(u"staves exceed MAX_STAVES, but hidden staves can be discarded", &e_);
        // Some scores have parts with many staves (~10), but most are hidden
        // When this occurs, we can discard hidden staves
        // and store a std::map between staffNumber and staffIndex.
        int staff_number = 1;
        size_t staff_index = 0;
        for (; staff_number <= staves; ++staff_number) {
            if (hidden_staves.find(staff_number) != hidden_staves.end()) {
                logger_->log_error(String(u"removing hidden staff %1").arg(staff_number), &e_);
                continue;
            }
            parts_[part_id].insert_staff_number_to_index(staff_number, static_cast<int>(staff_index));
            ++staff_index;
        }
        DO_ASSERT(staff_index == parts_[part_id].staff_number_to_index().size());

        set_number_of_staves_for_part(muse::value(part_map_, part_id), staves - static_cast<int>(hidden_staves.size()));
    } else {
        // Otherwise, don't discard any staves
        // And set hidden staves to hide when empty
        // (MuseScore doesn't currently have a mechanism
        // for hiding non-empty staves, so this is an approximation
        // of the correct implementation)
        set_number_of_staves_for_part(muse::value(part_map_, part_id), staves);
        for (int hidden_staff : hidden_staves) {
            int hidden_staff_index = muse::value(parts_, part_id).staff_number_to_index(hidden_staff);
            if (hidden_staff_index >= 0) {
                muse::value(part_map_, part_id)->staff(hidden_staff_index)->set_hide_when_empty(engraving::AutoOnOff::ON);
            }
        }
    }
}

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/clef node.
 TODO: Store the clef type, to simplify staff type setting in pass 2.
 */

void MusicXmlParserPass1::clef(const String & /* partId */) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::clef", &e_);

    while (e_.read_next_start_element()) {
        if (e_.name() == "line") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "sign") {
            String sign = e_.read_text();
        } else {
            skip_log_curr_elem();
        }
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

static bool determine_time_sig(MusicXmlLogger *logger, const XmlStreamReader *const xmlreader, const String &beats, const String &beat_type,
                               const String &time_symbol, TimeSigType &st, int &bts, int &btp) {
    // initialize
    st = TimeSigType::NORMAL;
    bts = 0; // the beats (max 4 separated by "+") as integer
    btp = 0; // beat-type as integer
    // determine if timesig is valid
    if (time_symbol == "cut") {
        st = TimeSigType::ALLA_BREVE;
    } else if (time_symbol == "common") {
        st = TimeSigType::FOUR_FOUR;
    } else if (!time_symbol.is_empty() && time_symbol != "normal") {
        logger->log_error(String(u"time symbol '%1' not recognized").arg(time_symbol), xmlreader);
        return false;
    }

    btp = beat_type.to_int();
    StringList list = beats.split(u'+');
    for (size_t i = 0; i < list.size(); i++) {
        bts += list.at(i).to_int();
    }

    // determine if bts and btp are valid
    if (bts <= 0 || btp <= 0) {
        logger->log_error(String(u"beats=%1 and/or beat-type=%2 not recognized").arg(beats, beat_type), xmlreader);
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

void MusicXmlParserPass1::time(const Fraction c_time) {
    String beats;
    String beat_type;
    String time_symbol = e_.attribute("symbol");

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
        if (determine_time_sig(logger_, &e_, beats, beat_type, time_symbol, st, bts, btp)) {
            time_sig_dura_ = Fraction(bts, btp);
            score_->sigmap()->add(c_time.ticks(), time_sig_dura_);
        }
    }
}

//---------------------------------------------------------
//   transpose
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/transpose node.
 */

void MusicXmlParserPass1::transpose(const String &part_id, const Fraction &tick) {
    Interval interval;
    while (e_.read_next_start_element()) {
        int i = e_.read_int();
        if (e_.name() == "diatonic") {
            interval.diatonic = i;
        } else if (e_.name() == "chromatic") {
            interval.chromatic = i;
        } else if (e_.name() == "octave-change") {
            interval.diatonic += i * 7;
            interval.chromatic += i * 12;
        } else {
            skip_log_curr_elem();
        }
    }

    if (parts_[part_id].intervals.count(tick) == 0) {
        if (!interval.diatonic && interval.chromatic) {
            interval.diatonic = Interval::chromatic2diatonic(interval.chromatic);
        }
        parts_[part_id].intervals[tick] = interval;
    } else {
        LOGD("duplicate transpose at tick %s", muPrintable(tick.to_string()));
    }
}

//---------------------------------------------------------
//   divisions
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/divisions node.
 */

void MusicXmlParserPass1::divisions() {
    divs_ = e_.read_int();
    if (!(divs_ > 0)) {
        logger_->log_error(u"illegal divisions", &e_);
    }
}

//---------------------------------------------------------
//   direction
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction node
 to be able to handle octave-shifts, as these must be interpreted
 in musical order instead of in MusicXML file order.
 */

void MusicXmlParserPass1::direction(const String &part_id, const Fraction &c_time) {
    // note: file order is direction-type first, then staff
    // this means staff is still unknown when direction-type is handled

    std::vector<MusicXmlOctaveShiftDesc> starts;
    std::vector<MusicXmlOctaveShiftDesc> stops;
    int staff = 0;

    while (e_.read_next_start_element()) {
        if (e_.name() == "direction-type") {
            direction_type(c_time, starts, stops);
        } else if (e_.name() == "staff") {
            int nstaves = static_cast<int>(get_part(part_id)->nstaves());
            String str_staff = e_.read_text();
            staff = parts_[part_id].staff_number_to_index(str_staff.to_int());
            if (0 <= staff && staff < nstaves) {
                //LOGD("direction staff %d", staff + 1);
            } else {
                logger_->log_error(String(u"invalid staff %1").arg(str_staff), &e_);
                staff = 0;
            }
        } else {
            e_.skip_current_element();
        }
    }

    // handle the stops first
    for (const MusicXmlOctaveShiftDesc &desc : stops) {
        if (muse::contains(octave_shifts_, static_cast<int>(desc.num))) {
            MusicXmlOctaveShiftDesc prev_desc = octave_shifts_.at(desc.num);
            if (prev_desc.tp == MusicXmlOctaveShiftDesc::Type::UP || prev_desc.tp == MusicXmlOctaveShiftDesc::Type::DOWN) {
                // a complete pair
                parts_[part_id].add_octave_shift(staff, prev_desc.size, prev_desc.time);
                parts_[part_id].add_octave_shift(staff, -prev_desc.size, desc.time);
            } else {
                logger_->log_error(u"double octave-shift stop", &e_);
            }
            muse::remove(octave_shifts_, desc.num);
        } else {
            octave_shifts_.insert({desc.num, desc});
        }
    }

    // then handle the starts
    for (const MusicXmlOctaveShiftDesc &desc : starts) {
        if (muse::contains(octave_shifts_, static_cast<int>(desc.num))) {
            MusicXmlOctaveShiftDesc prev_desc = octave_shifts_.at(desc.num);
            if (prev_desc.tp == MusicXmlOctaveShiftDesc::Type::STOP) {
                // a complete pair
                parts_[part_id].add_octave_shift(staff, desc.size, desc.time);
                parts_[part_id].add_octave_shift(staff, -desc.size, prev_desc.time);
            } else {
                logger_->log_error(u"double octave-shift start", &e_);
            }
            muse::remove(octave_shifts_, desc.num);
        } else {
            octave_shifts_.insert({desc.num, desc});
        }
    }
}

//---------------------------------------------------------
//   directionType
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type node.
 */

void MusicXmlParserPass1::direction_type(const Fraction c_time, std::vector<MusicXmlOctaveShiftDesc> &starts,
                                         std::vector<MusicXmlOctaveShiftDesc> &stops) {
    while (e_.read_next_start_element()) {
        if (e_.name() == "octave-shift") {
            String number = e_.attribute("number");
            int n = 0;
            if (!number.empty()) {
                n = number.to_int();
                if (n <= 0) {
                    logger_->log_error(String(u"invalid number %1").arg(number), &e_);
                } else {
                    n--; // make zero-based
                }
            }

            if (0 <= n && n < ::mu::iex::musicxml::max_number_level) {
                short size = static_cast<short>(e_.int_attribute("size"));
                String type = e_.attribute("type");
                //LOGD("octave-shift type '%s' size %d number %d", muPrintable(type), size, n);
                MusicXmlOctaveShiftDesc os_desc;
                handle_octave_shift(c_time, type, size, os_desc);
                os_desc.num = n;
                if (os_desc.tp == MusicXmlOctaveShiftDesc::Type::UP || os_desc.tp == MusicXmlOctaveShiftDesc::Type::DOWN) {
                    starts.push_back(os_desc);
                } else if (os_desc.tp == MusicXmlOctaveShiftDesc::Type::STOP) {
                    stops.push_back(os_desc);
                }
            } else {
                logger_->log_error(String(u"invalid octave-shift number %1").arg(number), &e_);
            }
            e_.skip_current_element();
        } else {
            e_.skip_current_element();
        }
    }
}

//---------------------------------------------------------
//   handleOctaveShift
//---------------------------------------------------------

void MusicXmlParserPass1::handle_octave_shift(const Fraction &c_time, const String &type, short size, MusicXmlOctaveShiftDesc &desc) {
    MusicXmlOctaveShiftDesc::Type tp = MusicXmlOctaveShiftDesc::Type::NONE;
    short sz = 0;

    switch (size) {
        case 22: sz = 3; break;
        case 15: sz = 2; break;
        case 8:
        default: sz = 1;
    }

    if (!c_time.is_valid() || c_time < Fraction(0, 1)) {
        logger_->log_error(u"invalid current time", &e_);
    }

    if (type == u"up") {
        tp = MusicXmlOctaveShiftDesc::Type::UP;
    } else if (type == u"down") {
        tp = MusicXmlOctaveShiftDesc::Type::DOWN;
        sz *= -1;
    } else if (type == u"stop") {
        tp = MusicXmlOctaveShiftDesc::Type::STOP;
    } else {
        logger_->log_error(String(u"invalid octave-shift type '%1'").arg(type), &e_);
        return;
    }

    desc = MusicXmlOctaveShiftDesc(tp, sz, c_time);
}

//---------------------------------------------------------
//   notations
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations node.
 */

void MusicXmlParserPass1::notations(MusicXmlStartStop &tuplet_start_stop) {
    //_logger->logDebugTrace("MusicXmlParserPass1::note", &_e);

    while (e_.read_next_start_element()) {
        if (e_.name() == "tuplet") {
            String tuplet_type = e_.attribute("type");

            // ignore possible children (currently not supported)
            e_.skip_current_element();

            if (tuplet_type == u"start") {
                tuplet_start_stop = MusicXmlStartStop::START;
            } else if (tuplet_type == u"stop") {
                tuplet_start_stop = MusicXmlStartStop::STOP;
            } else if (!tuplet_type.empty() && tuplet_type != u"start" && tuplet_type != u"stop") {
                logger_->log_error(String(u"unknown tuplet type '%1'").arg(tuplet_type), &e_);
            }
        } else {
            e_.skip_current_element(); // skip but don't log
        }
    }
}

//---------------------------------------------------------
//   voiceToInt
//---------------------------------------------------------
static int voice_str_index(const String &v) {
    if (v.empty()) {
        return 0;
    }
    return v.at(0).unicode();
}

int MusicXmlParserPass1::voice_to_int(const String &voice) {
    bool ok;
    int voice_int = voice.to_int(&ok);
    if (voice.empty()) {
        voice_int = 1;
    } else if (!ok) {
        voice_int = voice_str_index(voice); // Handle the rare but techincally in-spec case of a non-int voice
    }
    return voice_int;
}

//---------------------------------------------------------
//   note
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note node.
 */

void MusicXmlParserPass1::note(const String &part_id, const Fraction &s_time, Fraction &missing_prev, Fraction &dura,
                               Fraction &missing_curr, VoiceOverlapDetector &vod, MusicXmlTupletStates &tuplet_states) {
    //_logger->logDebugTrace("MusicXmlParserPass1::note", &_e);

    if (e_.ascii_attribute("print-spacing") == "no") {
        note_print_spacing_no(dura);
        return;
    }

    //float alter = 0;
    bool chord = false;
    bool grace = false;
    //int octave = -1;
    bool b_rest = false;
    int staff = 0;
    //int step = 0;
    String type;
    String voice = u"1";
    String instr_id;
    MusicXmlStartStop tuplet_start_stop{MusicXmlStartStop::NONE};

    MusicXmlNoteDuration mnd(divs_, logger_, this);

    while (e_.read_next_start_element()) {
        if (mnd.read_properties(e_)) {
            // element handled
        } else if (e_.name() == "accidental") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "beam") {
            has_beaming_info_ = true;
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "chord") {
            chord = true;
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "cue") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "grace") {
            grace = true;
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "instrument") {
            instr_id = e_.attribute("id");
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "lyric") {
            const String number = e_.attribute("number");
            parts_[part_id].lyric_number_handler().add_number(number);
            parts_[part_id].has_lyrics(true);
            e_.skip_current_element();
        } else if (e_.name() == "notations") {
            notations(tuplet_start_stop);
        } else if (e_.name() == "notehead") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "pitch") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "rest") {
            b_rest = true;
            rest();
        } else if (e_.name() == "staff") {
            bool ok = false;
            String str_staff = e_.read_text();
            staff = parts_[part_id].staff_number_to_index(str_staff.to_int(&ok));
            parts_[part_id].set_max_staff(staff);
            Part *part = muse::value(part_map_, part_id);
            IF_ASSERT_FAILED(part) { continue; }
            if (!ok || staff < 0 || staff >= int(part->nstaves())) {
                logger_->log_error(String(u"illegal or hidden staff '%1'").arg(str_staff), &e_);
            }
        } else if (e_.name() == "stem") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "tie") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "type") {
            type = e_.read_text();
        } else if (e_.name() == "unpitched") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "voice") {
            voice = e_.read_text();
        } else {
            skip_log_curr_elem();
        }
    }

    // multi-instrument handling
    String prev_instr_id = parts_[part_id].instr_list.instrument(s_time);
    bool must_insert = instr_id != prev_instr_id;
    /*
    LOGD("tick %s (%d) staff %d voice '%s' previnst='%s' instrument '%s' mustInsert %d",
           muPrintable(sTime.print()),
           sTime.ticks(),
           staff + 1,
           muPrintable(voice),
           muPrintable(prevInstrId),
           muPrintable(instrId),
           mustInsert
           );
    */
    if (must_insert) {
        parts_[part_id].instr_list.set_instrument(instr_id, s_time);
    }

    // check for timing error(s) and set dura
    // keep in this order as checkTiming() might change dura
    String error_str = mnd.check_timing(type, b_rest, grace);
    dura = mnd.duration();
    if (!error_str.empty()) {
        logger_->log_error(error_str, &e_);
    }

    // don't count chord or grace note duration
    // note that this does not check the MusicXML requirement that notes in a chord
    // cannot have a duration longer than the first note in the chord
    missing_prev.set(0, 1);
    if (chord || grace) {
        dura.set(0, 1);
    }

    if (!chord && !grace) {
        // do tuplet
        Fraction time_mod = mnd.time_mod();
        MusicXmlTupletState &tuplet_state = tuplet_states[voice];
        tuplet_state.determine_tuplet_action(mnd.duration(), time_mod, tuplet_start_stop, mnd.normal_type(), missing_prev, missing_curr);
    }

    // store result
    if (dura.is_valid() && dura > Fraction(0, 1)) {
        // count the chords
        int voice_int = voice_to_int(voice);
        if (!muse::contains(muse::value(parts_, part_id).voicelist, voice_int)) {
            VoiceDesc vs;
            parts_[part_id].voicelist.insert({voice_int, vs});
        }
        parts_[part_id].voicelist[voice_int].incr_chord_rests(staff);
        // determine note length for voiceInt overlap detection
        vod.add_note((s_time + missing_prev).ticks(), (s_time + missing_prev + dura).ticks(), voice_int, staff);
    }

    add_error(check_at_end_element(e_, u"note"));
}

//---------------------------------------------------------
//   notePrintSpacingNo
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note node for a note with print-spacing="no".
 These are handled like a forward: only moving the time forward.
 */

void MusicXmlParserPass1::note_print_spacing_no(Fraction &dura) {
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
}

//---------------------------------------------------------
//   calcTicks
//---------------------------------------------------------

Fraction MusicXmlParserPass1::calc_ticks(const int &int_ticks, const int &input_divisions, const XmlStreamReader *xml_reader) {
    Fraction dura(0, 1); // invalid unless set correctly

    if (input_divisions > 0) {
        dura.set(int_ticks, 4 * input_divisions);
        dura.reduce(); // prevent overflow in later Fraction operations

        // Correct for previously adjusted durations
        // This is necessary when certain tuplets are
        // followed by a <backup> element.
        // There are two strategies:
        // 1. Use a lookup table of previous adjustments
        // 2. Check if within maxDiff of a seenDenominator
        if (muse::contains(adjusted_durations_, dura)) {
            dura = adjusted_durations_.at(dura);
        } else if (dura.reduced().denominator() > 64) {
            for (int seen_denominator : seen_denominators_) {
                int seen_denominator_ticks = Fraction(1, seen_denominator).ticks();
                if (std::abs(dura.ticks() % seen_denominator_ticks) <= max_diff_) {
                    Fraction rounded_dura = Fraction(std::round(dura.ticks() / double(seen_denominator_ticks)), seen_denominator);
                    rounded_dura.reduce();
                    logger_->log_error(String(u"calculated duration (%1) assumed to be a rounding error by proximity to (%2)")
                                           .arg(dura.to_string(), rounded_dura.to_string()));
                    insert_adjusted_duration(dura, rounded_dura);
                    dura = rounded_dura;
                    break;
                }
            }
        }
    } else {
        logger_->log_error(u"illegal or uninitialized divisions", xml_reader);
    }
    //qDebug("duration %s valid %d", muPrintable(dura.print()), dura.isValid());

    return dura;
}

//---------------------------------------------------------
//   duration
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/duration node.
 */

void MusicXmlParserPass1::duration(Fraction &dura, muse::XmlStreamReader &e) {
    DO_ASSERT(e.is_start_element() && e.name() == "duration");
    logger_->log_debug_trace(u"MusicXmlParserPass1::duration", &e);

    dura.set(0, 0); // invalid unless set correctly
    int int_dura = e.read_int();
    dura = calc_ticks(int_dura);
}

//---------------------------------------------------------
//   forward
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/forward node.
 */

void MusicXmlParserPass1::forward(Fraction &dura) {
    //_logger->logDebugTrace("MusicXmlParserPass1::forward", &_e);

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

void MusicXmlParserPass1::backup(Fraction &dura) {
    //_logger->logDebugTrace("MusicXmlParserPass1::backup", &_e);

    while (e_.read_next_start_element()) {
        if (e_.name() == "duration") {
            duration(dura);
        } else {
            skip_log_curr_elem();
        }
    }
}

//---------------------------------------------------------
//   timeModification
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/time-modification node.
 */

void MusicXmlParserPass1::time_modification(Fraction &time_mod) {
    //_logger->logDebugTrace("MusicXmlParserPass1::timeModification", &_e);

    int int_actual = 0;
    int int_normal = 0;
    String str_actual;
    String str_normal;

    while (e_.read_next_start_element()) {
        if (e_.name() == "actual-notes") {
            str_actual = e_.read_text();
        } else if (e_.name() == "normal-notes") {
            str_normal = e_.read_text();
        } else {
            skip_log_curr_elem();
        }
    }

    int_actual = str_actual.to_int();
    int_normal = str_normal.to_int();
    if (int_actual > 0 && int_normal > 0) {
        time_mod.set(int_normal, int_actual);
    } else {
        time_mod.set(1, 1);
        logger_->log_error(String(u"illegal time-modification: actual-notes %1 normal-notes %2").arg(str_actual, str_normal), &e_);
    }
}

//---------------------------------------------------------
//   rest
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/rest node.
 */

void MusicXmlParserPass1::rest() {
    //_logger->logDebugTrace("MusicXmlParserPass1::rest", &_e);

    while (e_.read_next_start_element()) {
        if (e_.name() == "display-octave") {
            e_.skip_current_element(); // skip but don't log
        } else if (e_.name() == "display-step") {
            e_.skip_current_element(); // skip but don't log
        } else {
            skip_log_curr_elem();
        }
    }
}
} // namespace mu::iex::musicxml
