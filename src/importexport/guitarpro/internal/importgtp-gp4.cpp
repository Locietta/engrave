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

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/capo.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/note.h"
#include "engraving/dom/notedot.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/stringdata.h"
#include "engraving/dom/stringtunings.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/editchord.h"
#include "engraving/types/symid.h"
#include "guitarprodrumset.h"
#include "importgtp.h"
#include "log.h"
#include "utils.h"

using namespace muse::io;
using namespace mu::engraving;

namespace mu::iex::guitarpro {
//---------------------------------------------------------
//   readMixChange
//---------------------------------------------------------

bool GuitarPro4::read_mix_change(Measure *measure) {
    /*char patch   =*/
    read_char();
    signed char volume = read_char();
    signed char pan = read_char();
    signed char chorus = read_char();
    signed char reverb = read_char();
    signed char phase = read_char();
    signed char tremolo = read_char();
    int temp = read_int();

    bool tempo_edited = false;

    if (volume >= 0) {
        read_char();
    }
    if (pan >= 0) {
        read_char();
    }
    if (chorus >= 0) {
        read_char();
    }
    if (reverb >= 0) {
        read_char();
    }
    if (phase >= 0) {
        read_char();
    }
    if (tremolo >= 0) {
        read_char();
    }
    if (temp >= 0) {
        if (temp != previous_tempo) {
            if (last_segment && last_segment->tick() != measure->tick()) {
                bool has_tempo_text = false;
                for (EngravingItem *e : last_segment->annotations()) {
                    has_tempo_text |= e->is_tempo_text();
                }
                if (!has_tempo_text) {
                    TempoText *tt = Factory::create_tempo_text(last_segment);
                    tt->set_tempo(double(temp) / 60.0f);
                    tt->set_visible(false);
                    tt->set_track(0);
                    last_segment->add(tt);
                }
            }
            previous_tempo = temp;
            set_tempo(temp, measure);
        }
        last_segment = nullptr;
        read_char();
        tempo_edited = true;
    }

    read_char(); // bitmask: what should be applied to all tracks
    return tempo_edited;
}

//---------------------------------------------------------
//   readBeatEffects
//---------------------------------------------------------

int GuitarPro4::read_beat_effects(int track, Segment *segment) {
    int effects = 0;
    uint8_t fx_bits1 = read_u_int8();
    uint8_t fx_bits2 = read_u_int8();
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_fade) {
        effects = 4; // fade in
    }
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_effect) {
        effects = read_u_int8(); // effect 1-tapping, 2-slapping, 3-popping
    }
    if (fx_bits2 & ::mu::iex::guitarpro::GuitarPro::beat_tremolo) {
        read_tremolo_bar(track, segment);
    }
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_arpeggio) {
        int strokeup = read_u_int8();   // up stroke length
        int strokedown = read_u_int8(); // down stroke length

        Arpeggio *a = Factory::create_arpeggio(score->dummy());
        if (strokeup > 0) {
            a->set_arpeggio_type(ArpeggioType::UP_STRAIGHT);
            if (strokeup < 7) {
                a->set_stretch(1.0 / std::pow(2, 6 - strokeup));
            }
        } else if (strokedown > 0) {
            a->set_arpeggio_type(ArpeggioType::DOWN_STRAIGHT);
            if (strokedown < 7) {
                a->set_stretch(1.0 / std::pow(2, 6 - strokedown));
            }
        } else {
            delete a;
            a = 0;
        }

        if (a) {
            ChordRest *cr = Factory::create_chord(segment);
            cr->set_track(track);
            cr->add(a);
            segment->add(cr);
        }
    }
    if (fx_bits2 & ::mu::iex::guitarpro::GuitarPro::beat_stroke_dir) {
        effects = read_u_int8(); // stroke pick direction
        effects += 4;            //1 or 2 for effects becomes 4 or 5
    }
    if (fx_bits1 & 0x01) { // GP3 column-wide vibrato
    }
    if (fx_bits1 & 0x2) { // GP3 column-wide wide vibrato (="tremolo" in GP3)
    }
    return effects;
}

//---------------------------------------------------------
//   readNote
//---------------------------------------------------------

GuitarPro::ReadNoteResult GuitarPro4::read_note(int string, int staff_idx, Note *note) {
    uint8_t note_bits = read_u_int8();

    //
    // noteBits:
    //   80 - Right hand or left hand fingering;
    //   40 - Accentuated note
    //   20 - Note type (rest, empty note, normal note);
    //   10 - note dynamic;
    //    8 - Presence of effects linked to the note;
    //    4 - Ghost note;
    //    2 - Dotted note;  ?
    //    1 - Time-independent duration

    if (note_bits & ::mu::iex::guitarpro::GuitarPro::beat_tremolo) {
        //note->setHeadGroup(NoteHeadGroup::HEAD_CROSS);
        note->set_ghost(true);
    }

    bool tie_note = false;
    uint8_t variant = 1;
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::beat_effect) { // 0x20
        variant = read_u_int8();
        if (variant == 1) { // normal note
        } else if (variant == 2) {
            /* guitar pro 4 bundles tied notes with slides in the representation
             * we take note when we see ties and do not create slides for these notes. */
            slides[staff_idx * ::mu::engraving::voices] = -2;
            tie_note = true;
        } else if (variant == 3) { // dead notes = ghost note
            note->set_head_group(NoteHeadGroup::HEAD_CROSS);
            note->set_dead_note(true);
        } else {
            LOGD("unknown note variant: %d", variant);
        }
    }

    if (note_bits & 0x1) {     // note != beat
        int a = read_u_int8(); // length
        int b = read_u_int8(); // t
        LOGD("          Time independent note len, len %d t %d", a, b);
    }
    if (note_bits & 0x2) { // note is dotted
        //readUInt8();
    }

    int &previous_dynamic = previous_dynamic_by_track[note->track()];

    // set dynamic information on note if different from previous note
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_dynamic) { // 0x10
        int d = read_char();
        if (previous_dynamic != d) {
            previous_dynamic = d;
            add_dynamic(note, d);
        }
    } else if (previous_dynamic != ::mu::iex::guitarpro::GuitarPro::default_dynamic) {
        previous_dynamic = ::mu::iex::guitarpro::GuitarPro::default_dynamic;
        add_dynamic(note, previous_dynamic);
    }

    int fret_number = -1;
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_fret) { // 0x20
        // TODO: special case if note is tied
        fret_number = read_u_int8();
    }

    // check if a note is supposed to be accented, and give it the sforzato type
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_sforzato) { // 0x40
        Articulation *art = Factory::create_articulation(note->score()->dummy());
        art->set_sym_id(SymId::ARTIC_ACCENT_ABOVE);
        if (!EditChord::toggle_articulation(note->score(), note, art)) {
            delete art;
        }
    }

    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_fingering) { // 0x80
        int left_finger = read_u_int8();
        int right_finger = read_u_int8();
        Fingering *fi = Factory::create_fingering(note);
        String finger;
        // if there is a valid left hand fingering
        if (left_finger < 5) {
            if (left_finger == 0) {
                finger = u"T";
            } else if (left_finger == 1) {
                finger = u"1";
            } else if (left_finger == 2) {
                finger = u"2";
            } else if (left_finger == 3) {
                finger = u"3";
            } else if (left_finger == 4) {
                finger = u"4";
            }
        } else {
            if (right_finger == 0) {
                finger = u"T";
            } else if (right_finger == 1) {
                finger = u"I";
            } else if (right_finger == 2) {
                finger = u"M";
            } else if (right_finger == 3) {
                finger = u"A";
            } else if (right_finger == 4) {
                finger = u"O";
            }
        }
        fi->set_plain_text(finger);
        note->add(fi);
        fi->reset();
    }

    ReadNoteResult read_result;
    uint8_t mod_mask2 = 0;
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::beat_effects_value) {
        uint8_t mod_mask1 = read_u_int8();
        mod_mask2 = read_u_int8();
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_bend) {
            read_bend(note);
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_hammer) {
            read_result.hammer_on_pull_off = true;
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_let_ring) {
            read_result.let_ring = true;
        }
        if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_vibrato) {
            read_result.vibrato = true;
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_grace) {
            int fret = read_u_int8();       // grace fret
            int dynamic = read_u_int8();    // grace dynamic
            int transition = read_u_int8(); // grace transition
            int duration = read_u_int8();   // grace duration

            int grace_len = Constants::division / 8;
            if (duration == 1) {
                grace_len = Constants::division / 8; //32nd
            } else if (duration == 2) {
                grace_len = Constants::division / 6; //24th
            } else if (duration == 3) {
                grace_len = Constants::division / 4; //16th
            }
            Note *gn = Factory::create_note(score->dummy());

            if (fret == 255) {
                gn->set_head_group(NoteHeadGroup::HEAD_CROSS);
                gn->set_ghost(true);
            }
            if (fret == 255) {
                fret = 0;
            }
            gn->set_fret(fret);
            gn->set_string(string);
            int grace_pitch = note->part()->instrument()->string_data()->get_pitch(string, fret, note->staff(), note->tick());
            gn->set_pitch(grace_pitch);
            gn->set_tpc_from_pitch();

            Chord *gc = Factory::create_chord(score->dummy());
            gc->set_track(note->chord()->track());
            gc->add(gn);
            gc->set_ownership_parent(note->chord());

            TDuration d;
            d.set_val(grace_len);
            if (grace_len == Constants::division / 6) {
                d.set_dots(1);
            }
            gc->set_duration_type(d);
            gc->set_ticks(d.fraction());
            gc->set_note_type(NoteType::ACCIACCATURA);
            gc->mutldata()->set_mag(note->chord()->staff()->staff_mag(Fraction(0, 1)) * score->style().style_d(Sid::GRACE_NOTE_MAG));
            note->chord()->add(gc);
            add_dynamic(gn, dynamic);

            if (transition == 0) {
                // no transition
            } else if (transition == 1) {
                //TODO: Add a 'slide' guitar effect when implemented
                //TODO-ws               note->setSlideNote(gn);
            } else if (transition == 2 && fret_number >= 0 && fret_number <= 255 && fret_number != gn->fret()) {
            } else if (transition == 3) {
                // TODO:
                //     major: replace with a 'hammer-on' guitar effect when implemented
                //     minor: make slurs for parts

                ChordRest *cr1 = to_chord(gc);
                ChordRest *cr2 = to_chord(note->chord());

                Slur *slur1 = Factory::create_slur(score->dummy());
                slur1->set_start_element(cr1);
                slur1->set_end_element(cr2);
                slur1->set_tick(cr1->tick());
                slur1->set_tick2(cr2->tick());
                slur1->set_track(cr1->track());
                slur1->set_track2(cr2->track());
                // this case specifies only two-note slurs, don't set a parent
                score->undo_add_element(slur1);
            }
        }
        if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_staccato) { // staccato
            Chord *chord = note->chord();
            Articulation *a = Factory::create_articulation(chord);
            a->set_sym_id(SymId::ARTIC_STACCATO_ABOVE);
            chord->add(a);
        }
        if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_palm_mute) {
            read_result.palm_mute = true;
        }

        if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_tremolo) { // tremolo picking length
            int tremolo_division = read_u_int8();
            Chord *chord = note->chord();
            TremoloType type = TremoloType::INVALID_TREMOLO;
            if (tremolo_division == 1) {
                type = TremoloType::R8;
            } else if (tremolo_division == 2) {
                type = TremoloType::R16;
            } else if (tremolo_division == 3) {
                type = TremoloType::R32;
            } else {
                LOGD("Unknown tremolo value");
            }

            if (type != TremoloType::INVALID_TREMOLO) {
                TremoloSingleChord *t = Factory::create_tremolo_single_chord(chord);
                t->set_tremolo_type(type);
                chord->add(t);
            }
        }
        if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_slide) {
            int slide_kind = read_u_int8();
            // if slide >= 4 then we are not dealing with legato slide nor shift slide
            if (slide_kind >= 3 || slide_kind == 254 || slide_kind == 255) {
                slide = slide_kind;
            } else {
                //slides[note->chord()->track()] = slideKind;
                slide_list.push_back(note);
            }
            if (slide_kind == 2) {
                read_result.slur = true;
            }
        }

        if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_trill) {
            read_u_int8();
            /*int period =*/read_u_int8(); // trill length
            read_result.trill = true;
        }
    }
    if (fret_number == -1) {
        LOGD("Note: no fret number, tie %d", tie_note);
    }
    Staff *staff = note->staff();
    if (fret_number == 255) {
        fret_number = 0;
        note->set_head_group(NoteHeadGroup::HEAD_CROSS);
        note->set_ghost(true);
    }
    // dead note represented as high numbers - fix to zero
    if (fret_number > 99 || fret_number == -1) {
        fret_number = 0;
    }
    int pitch = staff->part()->instrument()->string_data()->get_pitch(string, fret_number, staff, note->tick());
    note->set_fret(fret_number);
    note->set_string(string);
    note->set_pitch(std::min(pitch, mu::engraving::max_pitch));

    if (mod_mask2 & 0x10) {
        int type = read_u_int8(); // harmonic kind
        if (type == 1) {          //Natural
            note->set_harmonic(false);
        } else if (type == 3) { // Tapped
            read_result.harmonic_tap = true;
        } else if (type == 4) { //Pinch
            read_result.harmonic_pinch = true;
        } else if (type == 5) { //semi
            read_result.harmonic_semi = true;
        } else { //Artificial
            read_result.harmonic_artificial = true;
            int harmonic_fret = note->fret();
            harmonic_fret += type - 10;
            Note *harmonic_note = Factory::create_note(note->chord());
            note->chord()->add(harmonic_note);
            harmonic_note->set_fret(harmonic_fret);
            harmonic_note->set_string(note->string());
            harmonic_note->set_pitch(
                note->staff()->part()->instrument()->string_data()->get_pitch(note->string(), harmonic_fret, note->staff(), note->tick()));
            harmonic_note->set_tpc_from_pitch();
        }
    }

    if (tie_note) {
        staff_idx_t si = note->staff_idx();
        if (slurs[si]) {
            score->remove_spanner(slurs[si]);
            delete slurs[si];
            slurs[si] = 0;
        }
        bool found = false;
        Chord *chord = note->chord();
        Segment *segment = chord->segment()->prev1(SegmentType::CHORD_REST);
        track_idx_t track = note->track();
        std::vector<ChordRest *> chords;
        Note *true_note = 0;
        while (segment) {
            EngravingItem *e = segment->element(track);
            if (e) {
                if (e->is_chord()) {
                    Chord *chord2 = to_chord(e);
                    for (Note *note2 : chord2->notes()) {
                        if (note2->string() == string) {
                            if (chords.empty()) {
                                Tie *tie = Factory::create_tie(note2);
                                tie->set_end_note(note);
                                note2->add(tie);
                            }
                            note->set_fret(note2->fret());
                            note->set_pitch(note2->pitch());
                            true_note = note2;
                            found = true;
                            break;
                        }
                    }
                }
                if (found) {
                    break;
                } else if (e) {
                    chords.push_back(to_chord_rest(e));
                }
            }
            segment = segment->prev1(SegmentType::CHORD_REST);
        }
        if (true_note && chords.size()) {
            Note *end_note = note;
            for (unsigned int i = 0; i < chords.size(); ++i) {
                Chord *chord1 = nullptr;
                auto cr = chords.at(i);
                if (cr->is_chord()) {
                    chord1 = to_chord(cr);
                } else {
                    Rest *rest = to_rest(cr);
                    auto dur = rest->ticks();
                    auto dut = rest->duration_type();
                    auto seg = rest->segment();
                    seg->remove(rest);
                    auto tuplet = rest->tuplet();
                    if (tuplet) {
                        tuplet->remove(rest);
                    }
                    delete rest;
                    chord1 = Factory::create_chord(seg);
                    chord1->set_track(note->track());
                    chord1->set_ticks(dur);
                    chord1->set_duration_type(dut);
                    seg->add(chord1);
                    if (tuplet) {
                        tuplet->add(chord1);
                    }
                }

                Note *note2 = Factory::create_note(chord1);
                note2->set_string(true_note->string());
                note2->set_fret(true_note->fret());
                note2->set_pitch(true_note->pitch());
                note2->set_tpc_from_pitch();
                chord1->set_note_type(true_note->note_type());
                chord1->add(note2);
                Tie *tie = Factory::create_tie(note2);
                tie->set_end_note(end_note);
                end_note->set_harmonic(true_note->harmonic());
                end_note = note2;
                note2->add(tie);
            }
            Tie *tie = Factory::create_tie(true_note);
            tie->set_end_note(end_note);
            end_note->set_harmonic(true_note->harmonic());
            true_note->add(tie);
        }
        if (!found) {
            LOGD("tied note not found, pitch %d fret %d string %d", note->pitch(), note->fret(), note->string());
            read_result.slur = false;
            return read_result;
        }
    }

    return read_result;
}

//---------------------------------------------------------
//   readInfo
//---------------------------------------------------------

void GuitarPro4::read_info() {
    title = read_delphi_string();
    subtitle = read_delphi_string();
    artist = read_delphi_string();
    album = read_delphi_string();
    composer = read_delphi_string();
    String copyright = read_delphi_string();
    if (!copyright.is_empty()) {
        score->set_meta_tag(u"copyright", copyright);
    }

    read_delphi_string(); // transcriber
    read_delphi_string(); // instructions
    int n = read_int();
    for (int i = 0; i < n; ++i) {
        comments.append(read_delphi_string());
    }
}

//---------------------------------------------------------
//   convertGP4SlideNum
//---------------------------------------------------------

int GuitarPro4::convert_g_p4_slide_num(int sl) {
    switch (sl) {
        case 1: return ::mu::iex::guitarpro::GuitarPro::shift_slide;
        case 2: return ::mu::iex::guitarpro::GuitarPro::legato_slide;
        case 3: // slide out downwards
            return ::mu::iex::guitarpro::GuitarPro::slide_out_down;
        case 4: // slide out upwards
            return ::mu::iex::guitarpro::GuitarPro::slide_out_up;
        case 254: // slide in from above
            return ::mu::iex::guitarpro::GuitarPro::slide_in_above;
        case 255: // slide in from below
            return ::mu::iex::guitarpro::GuitarPro::slide_in_below;
    }
    return sl;
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool GuitarPro4::read(IODevice *io) {
    continious_elements_builder_ = std::make_unique<ContiniousElementsBuilder>(score);
    guitar_bend_importer_ = std::make_unique<GuitarBendImporter>(score);

    f = io;
    cur_pos = 30;

    read_info();
    read_u_int8(); // triplet feeling
    read_lyrics();

    int temp = read_int();
    key = read_int();
    /*int octave =*/read_u_int8(); // octave

    previous_tempo = -1;

    read_channels();
    measures = read_int();
    staves = read_int();
    if (measures == 0 || measures > 100000) {
        return false;
    }

    init_dynamics(staves);

    cur_dynam.resize(staves * ::mu::engraving::voices);
    for (auto &i : cur_dynam) {
        i = -1;
    }

    int tnumerator = 4;
    int tdenominator = 4;
    for (size_t i = 0; i < measures; ++i) {
        GpBar bar;
        uint8_t bar_bits = read_u_int8();
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_timesig_numerator) {
            tnumerator = read_u_int8();
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_timesig_denominator) {
            tdenominator = read_u_int8();
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_repeat_start) {
            bar.repeat_flags = bar.repeat_flags | mu::engraving::Repeat::START;
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_repeat_end) { // number of repeats
            bar.repeat_flags = bar.repeat_flags | mu::engraving::Repeat::END;
            bar.repeats = read_u_int8() + 1;
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_volta) { // a volta
            uint8_t volta_number = read_u_int8();
            while (volta_number > 0) {
                // volta information is represented as a binary number
                bar.volta.volta_type = ::mu::iex::guitarpro::gp_volta_binary;
                bar.volta.volta_info.push_back(volta_number & 1);
                volta_number >>= 1;
            }
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_marker) {
            bar.marker = read_delphi_string(); // new section?
            /*int color = */ read_int();       // color?
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_keysig) {
            int current_key = read_u_int8();
            /* key signatures are specified as
             * 1# = 1, 2# = 2, ..., 7# = 7
             * 1b = 255, 2b = 254, ... 7b = 249 */
            bar.keysig = current_key <= 7 ? current_key : -256 + current_key;
            read_u_int8(); // specifies major/minor mode
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_double_bar) {
            bar.bar_line = BarLineType::DOUBLE;
        }
        bar.timesig = Fraction(tnumerator, tdenominator);
        bars.push_back(bar);
    }

    //
    // create a part for every staff
    //
    for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
        Part *part = new Part(score);
        Staff *s = Factory::create_staff(part);

        score->append_staff(s);
        score->append_part(part);
    }

    create_measures();

    set_tempo(temp, score->first_measure());

    for (size_t i = 0; i < staves; ++i) {
        int tuning[::mu::iex::guitarpro::gp_max_string_number];

        uint8_t c = read_u_int8(); // simulations bitmask
        if (c & 0x2) {             // 12 stringed guitar
        }
        if (c & 0x4) { // banjo track
        }
        String name = read_pascal_string(40);
        int strings = read_int();
        if (strings <= 0 || strings > ::mu::iex::guitarpro::gp_max_string_number) {
            return false;
        }
        for (int j = 0; j < strings; ++j) {
            tuning[j] = read_int();
        }
        for (int j = strings; j < ::mu::iex::guitarpro::gp_max_string_number; ++j) {
            read_int();
        }
        int midi_port = read_int() - 1;
        int midi_channel = read_int() - 1;
        if (midi_channel < 0 || midi_channel >= static_cast<int>(channel_defaults.size())) {
            LOGE() << "midiChannel " << midi_channel << " out of range 0-" << channel_defaults.size();
            return false;
        }

        /*int midiChannel2 =*/read_int(); // - 1;
        int frets = read_int();

        int capo = read_int();
        /*int color        =*/read_int();

        std::vector<int> tuning2(strings);
        //int tuning2[strings];
        for (int k = 0; k < strings; ++k) {
            tuning2[strings - k - 1] = tuning[k];
        }
        bool use_flats = create_tuning_string(strings, &tuning2[0]);
        StringData string_data(frets, strings, &tuning2[0], use_flats);
        Part *part = score->staff(i)->part();
        Instrument *instr = part->instrument();
        instr->set_string_data(string_data);
        instr->set_single_note_dynamics(false);
        part->set_plain_long_name(name);

        int patch = channel_defaults[midi_channel].patch;

        //
        // determine clef
        //
        Staff *staff = score->staff(i);
        ClefType clef_id = ClefType::G;
        if (midi_channel == ::mu::iex::guitarpro::gp_default_percussion_channel) {
            clef_id = ClefType::PERC;
            StaffTypes type = StaffTypes::PERC_DEFAULT;
            if (auto it = drumset::perc_staff_lines_from_instrument.find(name.to_std_string());
                it != drumset::perc_staff_lines_from_instrument.end()) {
                drumset::init_guitar_pro_percussion_set(it->second);
                drumset::set_instrument_drumset(instr, it->second);
                switch (it->second.num_lines) {
                    case 1: type = StaffTypes::PERC_1LINE; break;
                    case 2: type = StaffTypes::PERC_2LINE; break;
                    case 3: type = StaffTypes::PERC_3LINE; break;
                    default: type = StaffTypes::PERC_DEFAULT; break;
                }
            } else {
                drumset::init_guitar_pro_drumset();
                instr->set_drumset(drumset::gp_drumset);
            }
            staff->set_staff_type(Fraction(0, 1), *StaffType::preset(type));
        } else {
            clef_id = default_clef(patch);
        }

        Measure *measure = score->first_measure();
        if (measure->tick() != Fraction(0, 1)) {
            Segment *segment = measure->get_segment(SegmentType::HEADER_CLEF, Fraction(0, 1));

            Clef *clef = Factory::create_clef(segment);
            clef->set_clef_type(clef_id);
            clef->set_track(i * ::mu::engraving::voices);
            segment->add(clef);
        } else {
            staff->set_default_clef_type(ClefTypeList(clef_id, clef_id));
        }

        if (capo > 0 && !engraving_configuration()->guitar_pro_import_experimental()) {
            Segment *s = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
            const size_t track = i * ::mu::engraving::voices;

            CapoParams params;
            params.active = true;
            params.transpose_mode = CapoParams::TransposeMode::TAB_ONLY;
            params.fret_position = capo;

            Capo *capo_el = Factory::create_capo(s);
            capo_el->set_track(track);
            capo_el->set_params(params);
            s->add(capo_el);

            staff->insert_capo_params({0, 1}, params, true);
        }

        InstrChannel *ch = instr->channel(0);
        if (midi_channel == ::mu::iex::guitarpro::gp_default_percussion_channel) {
            ch->set_program(0);
            ch->set_bank(128);
        } else {
            ch->set_program(patch);
            ch->set_bank(0);
        }
        ch->set_volume(channel_defaults[midi_channel].volume);
        ch->set_pan(channel_defaults[midi_channel].pan);
        ch->set_chorus(channel_defaults[midi_channel].chorus);
        ch->set_reverb(channel_defaults[midi_channel].reverb);
        staff->part()->set_midi_channel(midi_channel, midi_port);
        // missing: phase, tremolo
    }

    slurs.resize(staves, nullptr);
    tuple_kind.resize(staves);
    for (auto &i : tuple_kind) {
        i = 0;
    }

    Measure *measure = score->first_measure();
    bool mix_change = false;
    bool last_slur_add = false;
    for (size_t bar = 0; bar < measures; ++bar, measure = measure->next_measure()) {
        const GpBar &gpbar = bars[bar];
        if (!gpbar.marker.is_empty()) {
            Segment *segment = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
            RehearsalMark *s = new RehearsalMark(segment);
            s->set_plain_text(gpbar.marker.trimmed());
            s->set_track(0);
            segment->add(s);
        }

        std::vector<Tuplet *> tuplets(staves);
        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            tuplets[staff_idx] = 0;
        }

        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            Fraction measure_len = {0, 1};
            Fraction tick = measure->tick();
            int beats = read_int();
            track_idx_t track = staff_idx * ::mu::engraving::voices;

            for (int beat = 0; beat < beats; ++beat) {
                slide = -1;
                if (muse::contains(slides, static_cast<int>(track))) {
                    slide = muse::take(slides, static_cast<int>(track));
                }

                uint8_t beat_bits = read_u_int8();
                bool dotted = beat_bits & 0x1;
                int pause = -1;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_pause) {
                    pause = read_u_int8();
                }
                int len = read_char();
                int tuple = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_tuplet) {
                    tuple = read_int();
                }
                Segment *segment = measure->get_segment(SegmentType::CHORD_REST, tick);
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_chord) {
                    size_t num_strings = score->staff(staff_idx)->part()->instrument()->string_data()->strings();
                    int header = read_u_int8();
                    String name;
                    if ((header & 1) == 0) {
                        name = read_delphi_string();
                        read_chord(segment, static_cast<int>(staff_idx * ::mu::engraving::voices), static_cast<int>(num_strings), name,
                                   false);
                    } else {
                        skip(16);
                        name = read_pascal_string(21);
                        skip(4);
                        read_chord(segment, static_cast<int>(staff_idx * ::mu::engraving::voices), static_cast<int>(num_strings), name,
                                   true);
                        skip(32);
                    }
                }

                Lyrics *lyrics = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_lyrics) {
                    lyrics = Factory::create_lyrics(score->dummy());
                    auto str = read_delphi_string();
                    //TODO-ws                   str.erase(std::remove_if(str.begin(), str.end(), [](char c){return c == '_'; }), str.end());
                    lyrics->set_plain_text(str);
                }
                gp_lyrics.beat_counter_value++;
                if (gp_lyrics.beat_counter_value >= gp_lyrics.from_beat && static_cast<size_t>(gp_lyrics.lyric_track) == staff_idx + 1) {
                    size_t index = gp_lyrics.beat_counter_value - gp_lyrics.from_beat;
                    if (index < gp_lyrics.lyrics.size()) {
                        lyrics = Factory::create_lyrics(score->dummy());
                        lyrics->set_plain_text(gp_lyrics.lyrics[index]);
                    }
                }
                int beat_effects = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_effects_value) {
                    beat_effects = read_beat_effects(static_cast<int>(track), segment);
                }
                last_segment = segment;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_mix_change) {
                    read_mix_change(measure);
                    mix_change = true;
                }
                int strings = read_u_int8(); // used strings mask
                Fraction l = len2fraction(len);

                // Some beat effects could add a Chord before this
                ChordRest *cr = segment->cr(track);

                if (strings == 0) {
                    if (segment->cr(track)) {
                        segment->remove(segment->cr(track));
                        delete cr;
                        cr = 0;
                    }
                    cr = Factory::create_rest(segment);
                } else {
                    if (!segment->cr(track)) {
                        cr = Factory::create_chord(segment);
                    }
                }
                cr->set_ownership_parent(segment);
                cr->set_track(track);
                if (lyrics) {
                    cr->add(lyrics);
                }

                TDuration d(l);
                d.set_dots(dotted ? 1 : 0);

                if (dotted) {
                    l = l + (l * Fraction(1, 2));
                }
                if (tuple) {
                    Tuplet *tuplet = tuplets[staff_idx];
                    if ((tuplet == 0) || (tuplet->elements_duration() == tuplet->base_len().fraction() * tuplet->ratio().numerator())) {
                        tuplet = Factory::create_tuplet(measure);
                        tuplet->set_tick(tick);
                        tuplet->set_track(cr->track());
                        tuplets[staff_idx] = tuplet;
                        set_tuplet(tuplet, tuple);
                        tuplet->set_ownership_parent(measure);
                    }
                    tuplet->set_track(track);
                    tuplet->set_base_len(l);
                    tuplet->set_ticks(l * tuplet->ratio().denominator());
                    cr->set_tuplet(tuplet);
                    tuplet->add(cr);
                } else {
                    tuplets[staff_idx] = 0; // needed?
                }
                cr->set_ticks(l);
                if (cr->is_rest() && (pause == 0 || l >= measure->ticks())) {
                    cr->set_duration_type(DurationType::V_MEASURE);
                    cr->set_ticks(measure->ticks());
                } else {
                    cr->set_duration_type(d);
                }
                if (!segment->cr(track)) {
                    segment->add(cr);
                }
                Staff *staff = cr->staff();
                size_t num_strings = staff->part()->instrument()->string_data()->strings();
                bool has_slur = false;
                bool has_hammer_on_pull_off = false;
                bool has_let_ring = false;
                bool has_palm_mute = false;
                bool has_trill = false;
                bool has_vibrato_left_hand = false;
                bool has_vibrato_w_trem_bar = false;
                bool has_harmonic_artificial = false;
                bool has_harmonic_pinch = false;
                bool has_harmonic_tap = false;
                bool has_harmonic_semi = false;

                int dynam = -1;
                if (cr && cr->is_chord()) {
                    for (int i = 6; i >= 0; --i) {
                        if (strings & (1 << i) && ((6 - i) < static_cast<int>(num_strings))) {
                            Note *note = Factory::create_note(to_chord(cr));
                            // apply dotted notes to the note
                            if (dotted) {
                                // there is at most one dotted note in this guitar pro version
                                NoteDot *dot = Factory::create_note_dot(note);
                                dot->set_track(track); // needed to know the staff it belongs to (and detect tablature)
                                dot->set_visible(true);
                                note->add(dot);
                            }
                            to_chord(cr)->add(note);

                            ReadNoteResult read_result = read_note(6 - i, static_cast<int>(staff_idx), note);
                            has_slur = read_result.slur || has_slur;
                            has_hammer_on_pull_off = read_result.hammer_on_pull_off || has_hammer_on_pull_off;
                            has_let_ring = read_result.let_ring || has_let_ring;
                            has_palm_mute = read_result.palm_mute || has_palm_mute;
                            has_trill = read_result.trill || has_trill;
                            has_vibrato_left_hand = read_result.vibrato || has_vibrato_left_hand;
                            has_harmonic_artificial = read_result.harmonic_artificial || has_harmonic_artificial;
                            has_harmonic_pinch = read_result.harmonic_pinch || has_harmonic_pinch;
                            has_harmonic_tap = read_result.harmonic_tap || has_harmonic_tap;
                            has_harmonic_semi = read_result.harmonic_semi || has_harmonic_semi;
                            dynam = std::max(dynam, previous_dynamic_by_track[track]);
                            note->set_tpc_from_pitch();
                        }
                    }

                    bool has_vibrato_left_hand_on_beat = false;
                    bool has_vibrato_w_trem_bar_on_beat = false;
                    apply_beat_effects(to_chord(cr), beat_effects, has_vibrato_left_hand_on_beat, has_vibrato_w_trem_bar_on_beat);
                    has_vibrato_left_hand = has_vibrato_left_hand || has_vibrato_left_hand_on_beat;
                    has_vibrato_w_trem_bar = has_vibrato_w_trem_bar || has_vibrato_w_trem_bar_on_beat;
                    if (dynam != cur_dynam[track]) {
                        cur_dynam[track] = dynam;
                        add_dynamic(to_chord(cr)->notes().front(), dynam);
                    }
                }

                if (cr) {
                    add_let_ring(cr, has_let_ring);
                    add_palm_mute(cr, has_palm_mute);
                    add_trill(cr, has_trill);
                    add_hammer_on_pull_off(cr, has_hammer_on_pull_off);
                    add_vibrato_left_hand(cr, has_vibrato_left_hand);
                    add_vibrato_w_trem_bar(cr, has_vibrato_w_trem_bar);
                    add_harmonic_marks(cr, has_harmonic_artificial, has_harmonic_pinch, has_harmonic_tap, has_harmonic_semi);
                }

                // if we see that a tied note has been constructed do not create the tie
                if (slides[static_cast<int>(track)] == -2) {
                    slide = 0;
                    slides[static_cast<int>(track)] = -1;
                }
                bool slur_swap = true;
                if (slide != 2) {
                    if (has_slur && (slurs[staff_idx] == 0)) {
                        Slur *slur = Factory::create_slur(score->dummy());
                        slur->set_ownership_parent(0);
                        slur->set_track(track);
                        slur->set_track2(track);
                        slur->set_tick(cr->tick());
                        slur->set_tick2(cr->tick());
                        slur->set_start_element(cr);
                        slurs[staff_idx] = slur;
                        score->add_element(slur);
                    } else if (slurs[staff_idx] && !has_slur) {
                        // TODO: check slur
                        Slur *s = slurs[staff_idx];
                        slurs[staff_idx] = 0;
                        s->set_tick2(cr->tick());
                        s->set_track2(cr->track());
                        s->set_end_element(cr);
                        if (cr->is_chord()) {
                            last_slur_add = true;
                            slur_swap = false;
                        }
                        //TODO-ws                           cr->has_slur = true;
                    } else if (slurs[staff_idx] && has_slur) {
                    }
                }
                if (cr && (cr->is_chord()) && slide > 0) {
                    auto chord = to_chord(cr);
                    auto effect = convert_g_p4_slide_num(slide);
                    if (slide > 2) {
                        create_slide(convert_g_p4_slide_num(slide), cr, static_cast<int>(staff_idx));
                    }
                    if (slide < 3 || effect == ::mu::iex::guitarpro::GuitarPro::slide_out_up) {
                        Note *last = chord->up_note();
                        auto seg = chord->segment();
                        Measure *mes = seg->measure();
                        while ((seg = seg->prev()) || (mes = mes->prev_measure())) {
                            if (!seg) {
                                break; //seg = mes->last();
                            }
                            if (seg->segment_type() == SegmentType::CHORD_REST && seg->cr(chord->track()) &&
                                seg->cr(chord->track())->is_chord()) {
                                bool br = false;
                                Chord *cr1 = to_chord(seg->cr(chord->track()));
                                if (cr1) {
                                    for (auto n : cr1->notes()) {
                                        if (n->string() == last->string()) {
                                            Glissando *s = mu::engraving::Factory::create_glissando(n);
                                            s->set_start_element(n);
                                            s->set_tick(seg->tick());
                                            s->set_track(chord->track());
                                            s->set_glissando_type(GlissandoType::STRAIGHT);
                                            s->set_end_element(last);
                                            s->set_tick2(chord->segment()->tick());
                                            s->set_track2(chord->track());
                                            s->set_glissando_style(n->part()->instrument(n->tick())->glissando_style());
                                            score->add_element(s);
                                            if (slide == 2 || effect == ::mu::iex::guitarpro::GuitarPro::slide_out_up) {
                                                if (!last_slur_add) {
                                                    create_slur(true, chord->staff_idx(), cr1);
                                                    create_slur(false, chord->staff_idx(), chord);
                                                }
                                            }
                                            br = true;
                                            break;
                                        }
                                    }
                                }
                                if (br) {
                                    break;
                                }
                            }
                        }
                    }
                }
                if (slur_swap) {
                    last_slur_add = false;
                }
                rests_for_empty_beats(segment, measure, cr, l, static_cast<int>(track), tick);
                create_slur(has_slur, staff_idx, cr);
                tick += cr->actual_ticks();
                measure_len += cr->actual_ticks();
            }
            if (measure_len < measure->ticks()) {
                score->set_rest(tick, track, measure->ticks() - measure_len, false, nullptr, false);
                continious_elements_builder_->notify_uncompleted_measure();
            }
        }

        if (bar == 1 && !mix_change) {
            set_tempo(temp, score->first_measure());
        }
    }

    for (auto n : slide_list) {
        Segment *segment = n->chord()->segment();
        Measure *measure1 = segment->measure();
        int segment_counter = 0;
        while ((segment = segment->next1(SegmentType::CHORD_REST)) ||
               ((measure1 = measure1->next_measure()) && (segment = measure1->first()))) {
            if (!segment->is_chord_rest_type()) {
                continue;
            }
            bool br = false;
            ChordRest *cr = segment->cr(n->track());
            if (cr && cr->is_chord()) {
                Chord *c = to_chord(cr);
                ++segment_counter;
                if (segment_counter > 2) {
                    break;
                }
                for (Note *nt : c->notes()) {
                    if (nt->string() == n->string()) {
                        for (auto e : nt->el()) {
                            if (e->is_chord_line()) {
                                auto cl = to_chord_line(e);
                                if (cl->chord_line_type() == ChordLineType::PLOP || cl->chord_line_type() == ChordLineType::SCOOP) {
                                    br = true;
                                    break;
                                }
                            }
                        }
                        if (br) {
                            break;
                        }
                        Glissando *s = new Glissando(n);
                        s->set_start_element(n);
                        s->set_tick(n->chord()->segment()->tick());
                        s->set_track(n->track());
                        s->set_glissando_type(GlissandoType::STRAIGHT);
                        s->set_end_element(nt);
                        s->set_tick2(nt->chord()->segment()->tick());
                        s->set_track2(n->track());
                        score->add_element(s);
                        br = true;
                        break;
                    }
                }
                if (br) {
                    break;
                }
            }
        }
    }

    continious_elements_builder_->add_elements_to_score();
    guitar_bend_importer_->add_elements_to_score();
    add_tunings();
    utils::add_play_count_texts(score);

    return true;
}
} // namespace mu::iex::guitarpro
