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

#include "importgtp.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/bend.h"
#include "engraving/dom/box.h"
#include "engraving/dom/bracket.h"
#include "engraving/dom/capo.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrchange.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/keysig.h"
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
#include "engraving/dom/tapping.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/editing/editchord.h"
#include "engraving/rw/xmlwriter.h"
#include "engraving/types/symid.h"
#include "importptb.h"
#include "internal/guitarprodrumset.h"
#include "log.h"
#include "utils.h"

using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::iex::guitarpro {
//---------------------------------------------------------
//   errmsg
//---------------------------------------------------------

const char *const GuitarPro::errmsg[] = {
    "no error",
    "unknown file format",
    "unexpected end of file",
    "bad number of strings",
};

#ifdef _MSC_VER
#pragma optimize("", off)
#endif

//---------------------------------------------------------
//   GuitarPro
//---------------------------------------------------------

const std::shared_ptr<IEngravingConfiguration> &GuitarPro::engraving_configuration() const { return score->configuration(); }

GuitarPro::GuitarPro(MasterScore *s, int v) {
    score = s;
    version = v;
    volta_sequence = 1;
    tempo = -1;
}

//---------------------------------------------------------
//   skip
//---------------------------------------------------------

void GuitarPro::skip(int64_t len) {
    if (len < 0) {
        throw std::runtime_error("Invalid negative skip in Guitar Pro file");
    }
    const size_t pos = f->pos();
    if (static_cast<uint64_t>(len) > f->size() - pos) {
        throw std::runtime_error("Unexpected end of Guitar Pro file");
    }
    if (!f->seek(pos + static_cast<size_t>(len))) {
        throw std::runtime_error("Failed to skip Guitar Pro data");
    }
}

//---------------------------------------------------------
//   createTuningString
//---------------------------------------------------------
static const std::unordered_map<uint64_t, std::string> flat_presets = {
    // 63 58 54 49 44 39
    {0x3f3a36312c27, {"Eb Ab Db Gb Bb Eb"}},
    // 60 55 51 46 41 36
    {0x3c37332e2924, {"C F Bb Eb G C"}},
    // 63 58 54 49 44 37
    {0x3f3a36312c25, {"Db Ab Db Gb Bb Eb"}},
};

bool GuitarPro::create_tuning_string(int strings, int tuning[]) {
    bool use_flats = false;
    const char *tune[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    std::vector<int> pitch;
    uint64_t k = 0;
    for (int i = 0; i < strings; ++i) {
        pitch.push_back(tuning[i]);
        k |= (uint64_t) tuning[i] << 8 * i;
    }
    if (auto preset = ::mu::iex::guitarpro::flat_presets.find(k); preset != ::mu::iex::guitarpro::flat_presets.end()) {
        tunings.push_back(preset->second);
        use_flats = true;
        return use_flats;
    }
    std::string t;
    for (auto i : pitch) {
        t += tune[i % 12];
        t += " ";
    }
    tunings.push_back(t);
    return use_flats;
}

void GuitarPro::init_dynamics(size_t staves_num) {
    if (staves_num == 0 || staves_num > ::mu::iex::guitarpro::gp_max_track_number * 2) {
        throw std::runtime_error("Invalid Guitar Pro staff count");
    }
    previous_dynamic_by_track.resize(staves_num * mu::engraving::voices);
    std::fill(previous_dynamic_by_track.begin(), previous_dynamic_by_track.end(), ::mu::iex::guitarpro::GuitarPro::invalid_dynamic);
}

//---------------------------------------------------------
//    read
//---------------------------------------------------------

void GuitarPro::read(void *p, int64_t len) {
    if (len == 0) {
        return;
    }
    if (len < 0) {
        throw std::runtime_error("Invalid negative read length in Guitar Pro file");
    }
    int64_t rv = static_cast<int64_t>(f->read((uint8_t *) p, static_cast<size_t>(len)));
    if (rv != len) {
        throw std::runtime_error("Unexpected end of Guitar Pro file");
    }
    cur_pos += len;
}

//---------------------------------------------------------
//   readChar
//---------------------------------------------------------

int GuitarPro::read_char() {
    signed char c;
    read(&c, 1);
    return c;
}

//---------------------------------------------------------
//   readUInt8
//---------------------------------------------------------

uint8_t GuitarPro::read_u_int8() {
    uint8_t c;
    read(&c, 1);
    return c;
}

//---------------------------------------------------------
//   readPascalString
//---------------------------------------------------------

String GuitarPro::read_pascal_string(int n) {
    uint8_t l = read_u_int8();
    if (static_cast<size_t>(l) > f->size() - f->pos()) {
        throw std::runtime_error("Unexpected end of Guitar Pro string");
    }
    std::vector<char> s(static_cast<size_t>(l) + 1);
    read(&s[0], l);
    s[l] = 0;
    if (n - l > 0) {
        skip(n - l);
    }
    std::string_view sw(&s[0], l);
    if (UtfCodec::is_valid_utf8(sw)) {
        return String::from_utf8(&s[0]);
    }
    return String::from_ascii(&s[0], l);
}

//---------------------------------------------------------
//   readWordPascalString
//---------------------------------------------------------

String GuitarPro::read_word_pascal_string() {
    int l = read_int();
    if (l < 0 || static_cast<size_t>(l) > f->size() - f->pos()) {
        throw std::runtime_error("Invalid Guitar Pro string length");
    }
    std::vector<char> c(static_cast<size_t>(l) + 1);
    read(&c[0], l);
    c[l] = 0;
    std::string_view sw(&c[0], l);
    if (UtfCodec::is_valid_utf8(sw)) {
        return String::from_utf8(&c[0]);
    }
    return String::from_ascii(&c[0], l);
}

//---------------------------------------------------------
//   readBytePascalString
//---------------------------------------------------------

String GuitarPro::read_byte_pascal_string() {
    int l = read_u_int8();
    if (static_cast<size_t>(l) > f->size() - f->pos()) {
        throw std::runtime_error("Unexpected end of Guitar Pro string");
    }
    std::vector<char> c(static_cast<size_t>(l) + 1);
    read(&c[0], l);
    c[l] = 0;
    std::string_view sw(&c[0], l);
    if (UtfCodec::is_valid_utf8(sw)) {
        return String::from_utf8(&c[0]);
    }
    return String::from_ascii(&c[0], l);
}

//---------------------------------------------------------
//   readDelphiString
//---------------------------------------------------------

String GuitarPro::read_delphi_string() {
    int maxl = read_int();
    int l = read_u_int8();
    if (maxl < 0) {
        throw std::runtime_error("Invalid Guitar Pro string length");
    }
    if (maxl != l + 1 && maxl > 255) {
        l = maxl - 1;
    }
    if (l < 0 || static_cast<size_t>(l) > f->size() - f->pos()) {
        throw std::runtime_error("Invalid Guitar Pro string length");
    }
    std::vector<char> c(static_cast<size_t>(l) + 1);
    read(&c[0], l);
    c[l] = 0;
    return String::from_ascii(&c[0]);
}

//---------------------------------------------------------
//   readInt
//---------------------------------------------------------

int GuitarPro::read_int() {
    uint8_t x;
    read(&x, 1);
    uint32_t r = x;
    read(&x, 1);
    r |= static_cast<uint32_t>(x) << 8;
    read(&x, 1);
    r |= static_cast<uint32_t>(x) << 16;
    read(&x, 1);
    r |= static_cast<uint32_t>(x) << 24;
    return static_cast<int>(r);
}

//---------------------------------------------------------
//   addPalmMate
//---------------------------------------------------------

void GuitarPro::add_palm_mute(ChordRest *cr, bool has_palm_mute) {
    continious_elements_builder_->build_continious_element(cr, ElementType::PALM_MUTE, ContiniousElementsBuilder::ImportType::PALM_MUTE,
                                                           has_palm_mute);
}

//---------------------------------------------------------
//   addLetRing
//---------------------------------------------------------

void GuitarPro::add_let_ring(ChordRest *cr, bool has_let_ring) {
    continious_elements_builder_->build_continious_element(cr, ElementType::LET_RING, ContiniousElementsBuilder::ImportType::LET_RING,
                                                           has_let_ring);
}

//---------------------------------------------------------
//   addTrill
//---------------------------------------------------------

void GuitarPro::add_trill(ChordRest *cr, bool has_trill) {
    continious_elements_builder_->build_continious_element(cr, ElementType::TRILL, ContiniousElementsBuilder::ImportType::TRILL, has_trill);
}

//---------------------------------------------------------
//   addHammerOnPullOff
//---------------------------------------------------------

void GuitarPro::add_hammer_on_pull_off(ChordRest *cr, bool has_hammer_on_pull_off) {
    continious_elements_builder_->build_continious_element(
        cr, ElementType::HAMMER_ON_PULL_OFF, ContiniousElementsBuilder::ImportType::HAMMER_ON_PULL_OFF, has_hammer_on_pull_off);
}

//---------------------------------------------------------
//   addRasgueado
//---------------------------------------------------------

void GuitarPro::add_rasgueado(ChordRest *cr, bool has_rasgueado) {
    continious_elements_builder_->build_continious_element(cr, ElementType::RASGUEADO, ContiniousElementsBuilder::ImportType::RASGUEADO,
                                                           has_rasgueado);
}

//---------------------------------------------------------
//   addVibratoLeftHand
//---------------------------------------------------------

void GuitarPro::add_vibrato_left_hand(ChordRest *cr, bool has_vibrato_left_hand) {
    continious_elements_builder_->build_continious_element(
        cr, ElementType::VIBRATO, ContiniousElementsBuilder::ImportType::VIBRATO_LEFT_HAND_SLIGHT, has_vibrato_left_hand,
        ContiniousElementsBuilder::VibratoSubType::LEFT_HAND_SLIGHT);
}

//---------------------------------------------------------
//   addVibratoWTremBar
//---------------------------------------------------------

void GuitarPro::add_vibrato_w_trem_bar(ChordRest *cr, bool has_vibrato_w_trem_bar) {
    continious_elements_builder_->build_continious_element(
        cr, ElementType::VIBRATO, ContiniousElementsBuilder::ImportType::VIBRATO_W_TREM_BAR_SLIGHT, has_vibrato_w_trem_bar,
        ContiniousElementsBuilder::VibratoSubType::W_TREM_BAR_SLIGHT);
}

//---------------------------------------------------------
//   addHarmonicMarks
//---------------------------------------------------------

void GuitarPro::add_harmonic_marks(ChordRest *cr, bool has_harmonic_artificial, bool has_harmonic_pinch, bool has_harmonic_tap,
                                   bool has_harmonic_semi) {
    continious_elements_builder_->build_continious_element(
        cr, ElementType::HARMONIC_MARK, ContiniousElementsBuilder::ImportType::HARMONIC_ARTIFICIAL, has_harmonic_artificial,
        ContiniousElementsBuilder::HarmonicMarkSubType::ARTIFICIAL);

    continious_elements_builder_->build_continious_element(cr, ElementType::HARMONIC_MARK,
                                                           ContiniousElementsBuilder::ImportType::HARMONIC_PINCH, has_harmonic_pinch,
                                                           ContiniousElementsBuilder::HarmonicMarkSubType::PINCH);

    continious_elements_builder_->build_continious_element(cr, ElementType::HARMONIC_MARK,
                                                           ContiniousElementsBuilder::ImportType::HARMONIC_TAP, has_harmonic_tap,
                                                           ContiniousElementsBuilder::HarmonicMarkSubType::TAP);

    continious_elements_builder_->build_continious_element(cr, ElementType::HARMONIC_MARK,
                                                           ContiniousElementsBuilder::ImportType::HARMONIC_SEMI, has_harmonic_semi,
                                                           ContiniousElementsBuilder::HarmonicMarkSubType::SEMI);
}

//---------------------------------------------------------
//   addTap
//---------------------------------------------------------

void GuitarPro::add_tap(Chord *chord) {
    Tapping *tapping = Factory::create_tapping(chord);
    tapping->set_track(chord->track());
    tapping->set_hand(TappingHand::RIGHT);
    chord->add(tapping);
}

//---------------------------------------------------------
//   addSlap
//---------------------------------------------------------

void GuitarPro::add_slap(Note *note) { add_text_articulation(note, ArticulationTextType::SLAP); }

//---------------------------------------------------------
//   addPop
//---------------------------------------------------------

void GuitarPro::add_pop(Note *note) { add_text_articulation(note, ArticulationTextType::POP); }

void GuitarPro::add_text_articulation(Note *note, ArticulationTextType type) {
    if (type == ArticulationTextType::NO_TEXT) {
        return;
    }

    if (Chord *ch = to_chord(note->parent())) {
        Articulation *art = mu::engraving::Factory::create_articulation(ch);
        art->set_text_type(type);
        ch->add(art);
    }
}

//---------------------------------------------------------
//   addTextToNote
//---------------------------------------------------------

void GuitarPro::add_text_to_note(String string, Note *note) {
    Segment *segment = note->chord()->segment();
    StaffText *text = Factory::create_staff_text(segment);

    if (!string.is_empty()) {
        bool use_harmony = string[string.size() - 1] == '\\';
        if (use_harmony) {
            string.chop(1);
        }
    }
    text->set_plain_text(string);
    text->set_track(note->chord()->track());
    segment->add(text);
}

void GuitarPro::setup_tuplet_style(Tuplet *tuplet) {
    bool real;
    switch (tuplet->ratio().numerator()) {
        case 2: real = (tuplet->ratio().denominator() == 3); break;
        case 3:
        case 4: real = (tuplet->ratio().denominator() == 2); break;
        case 5:
        case 6:
        case 7: real = (tuplet->ratio().denominator() == 4); break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 13: real = (tuplet->ratio().denominator() == 8); break;
        default: real = false;
    }
    if (!real) {
        tuplet->set_number_type(TupletNumberType::SHOW_RELATION);
        tuplet->set_property_flags(Pid::NUMBER_TYPE, PropertyFlags::UNSTYLED);
    }
}

//---------------------------------------------------------
//   setTuplet
//---------------------------------------------------------

void GuitarPro::set_tuplet(Tuplet *tuplet, int tuple) {
    switch (tuple) {
        case 3: tuplet->set_ratio(Fraction(3, 2)); break;
        case 5: tuplet->set_ratio(Fraction(5, 4)); break;
        case 6: tuplet->set_ratio(Fraction(6, 4)); break;
        case 7: tuplet->set_ratio(Fraction(7, 4)); break;
        case 9: tuplet->set_ratio(Fraction(9, 8)); break;
        case 10: tuplet->set_ratio(Fraction(10, 8)); break;
        case 11: tuplet->set_ratio(Fraction(11, 8)); break;
        case 12: tuplet->set_ratio(Fraction(12, 8)); break;
        case 13: tuplet->set_ratio(Fraction(13, 8)); break;
        default: ASSERT_X(String(u"unsupported tuplet %d\n").arg(tuple));
    }
}

//---------------------------------------------------------
//   addDynamic
//---------------------------------------------------------

void GuitarPro::add_dynamic(Note *note, int d) {
    // guitar pro only allows their users to go from ppp to fff
    static const std::array<String, 9> map_dyn{u"f", u"ppp", u"pp", u"p", u"mp", u"mf", u"f", u"ff", u"fff"};
    if (d < 0 || static_cast<size_t>(d) >= map_dyn.size()) {
        return;
    }
    if (!note->chord()) {
        LOGD() << "addDynamics: No chord associated with this note";
        return;
    }
    Segment *s = nullptr;
    if (note->chord()->is_grace()) {
        Chord *parent = to_chord(note->chord()->parent());
        s = parent->segment();
    } else {
        s = note->chord()->segment();
    }
    if (!s->find_annotation(ElementType::DYNAMIC, note->staff_idx() * ::mu::engraving::voices,
                            note->staff_idx() * ::mu::engraving::voices + ::mu::engraving::voices - 1)) {
        Dynamic *dyn = new Dynamic(s);
        dyn->set_dynamic_type(map_dyn[d]);
        dyn->set_track(note->track());
        s->add(dyn);
    }
}

//---------------------------------------------------------
//   readVolta
//---------------------------------------------------------

void GuitarPro::read_volta(GPVolta *gp_volta, Measure *m) {
    /* Volta information is at most eight bits
     * signifying which numbers should appear in the
     * volta. A single bit 1 represents we should show
     * 1, 100 represents 3, 10000 represents 6, 10101
     * represents 1,3,5 etc. */
    if (gp_volta->volta_info.size() != 0) {
        // we have volta information - set up a volta
        mu::engraving::Volta *volta = new mu::engraving::Volta(score->dummy());
        volta->endings().clear();
        String volta_text_string = u"";
        // initialise count to 1 as the first bit processed with represent first time volta
        int count = 0;
        int binary_number = 0;
        // iterate through the volta information and determine the decimal numbers for voltas
        auto iter = gp_volta->volta_info.begin();
        while (iter != gp_volta->volta_info.end()) {
            switch (gp_volta->volta_type) {
                case ::mu::iex::guitarpro::gp_volta_flags:
                    count++;
                    if (*iter == 1) { // we want this number to be displayed in the volta
                        if (volta_text_string.empty()) {
                            volta_text_string += String::number(count);
                        } else {
                            volta_text_string += u',' + String::number(count);
                        }
                        // add the decimal number to the endings field of voltas as well as the text
                        volta->endings().push_back(count);
                    }
                    ++iter;
                    break;
                case ::mu::iex::guitarpro::gp_volta_binary:
                    // find the binary number in decimal
                    if (*iter == 1) {
                        binary_number += pow(2, count);
                    }
                    ++iter;
                    if (iter == gp_volta->volta_info.end()) {
                        // display all numbers in the volta from voltaSequence to the decimal
                        while (volta_sequence <= binary_number) {
                            if (volta_text_string.empty()) {
                                volta_text_string = String::number(volta_sequence);
                            } else {
                                volta_text_string += u',' + String::number(volta_sequence);
                            }
                            volta->endings().push_back(volta_sequence);
                            volta_sequence++;
                        }
                    }
                    count++;
                    break;
            }
        }
        volta->set_text(XmlWriter::xml_string(volta_text_string));
        volta->set_tick(m->tick());
        volta->set_tick2(m->end_tick());
        score->add_element(volta);
    }
}

std::vector<PitchValue> GuitarPro::read_bend_data_from_file() {
    std::vector<PitchValue> bend_data;

    read_u_int8();                 // icon
    /*int amplitude =*/read_int(); // shown amplitude
    int num_points = read_int();   // the number of points in the bend

    for (int i = 0; i < num_points; ++i) {
        int bend_time = read_int();
        int bend_pitch = read_int();
        int bend_vibrato = read_u_int8();
        bend_data.emplace_back(bend_time, bend_pitch, bend_vibrato);
    }

    return bend_data;
}

void GuitarPro::create_bend(Note *note, std::vector<PitchValue> &bend_data) {
    if (bend_data.size() < 2) {
        return;
    }

    /// not adding "hold" on 0 pitch
    int max_pitch = (std::max_element(bend_data.begin(), bend_data.end(), [](const PitchValue &l, const PitchValue &r) {
                        return l.pitch < r.pitch;
                    }))->pitch;

    if (max_pitch == 0) {
        return;
    }

    guitar_bend_importer_->collect_bend(note, bend_data);
}

//---------------------------------------------------------
//   readBend
//    bend graph
//---------------------------------------------------------

void GuitarPro::read_bend(Note *note) {
    std::vector<PitchValue> bend_data = read_bend_data_from_file();
    create_bend(note, bend_data);
}

//---------------------------------------------------------
//   readLyrics
//---------------------------------------------------------

void GuitarPro::read_lyrics() {
    gp_lyrics.lyric_track = static_cast<size_t>(read_int()); // lyric track
    gp_lyrics.from_beat = static_cast<size_t>(read_int());
    gp_lyrics.beat_counter_value = 0;

    String lyrics = read_word_pascal_string();
    lyrics.replace(u'\n', u' ');
    lyrics.replace(u'\r', u' ');
    auto sl = lyrics.split(u' ', KEEP_EMPTY_PARTS);
    for (auto &str : sl) {
        gp_lyrics.lyrics.push_back(str);
    }

    for (int i = 0; i < 4; ++i) {
        read_int();
        read_word_pascal_string();
    }
}

//---------------------------------------------------------
//   createSlide
//---------------------------------------------------------

void GuitarPro::create_slide(int sl, ChordRest *cr, int staff_idx, Note *note) {
    UNUSED(note);
    // shift / legato slide
    if (sl == ::mu::iex::guitarpro::GuitarPro::shift_slide || sl == ::mu::iex::guitarpro::GuitarPro::legato_slide) {
        Glissando *s = mu::engraving::Factory::create_glissando(cr);
        //s->setXmlText("");
        s->set_glissando_type(GlissandoType::STRAIGHT);
        cr->add(s);
        Segment *prev_seg = cr->segment()->prev1(SegmentType::CHORD_REST);
        EngravingItem *prev_elem = prev_seg->element(staff_idx);
        if (prev_elem) {
            if (prev_elem->is_chord()) {
                Chord *prev_chord = to_chord(prev_elem);
                /** TODO we should not just take the top note here
                * but the /correct/ note need to check whether GP
                * supports multi-note gliss. I think it can in modern
                * versions */
                s->set_start_element(prev_chord->up_note());
                s->set_tick(prev_seg->tick());
                s->set_track(staff_idx);
                s->set_ownership_parent(prev_chord->up_note());
                s->set_text(u"");
                s->set_glissando_type(GlissandoType::STRAIGHT);
                s->set_glissando_shift(sl == ::mu::iex::guitarpro::GuitarPro::shift_slide);
                s->set_glissando_style(s->part()->instrument(s->tick())->glissando_style());

                if (sl == ::mu::iex::guitarpro::GuitarPro::legato_slide) {
                    create_slur(true, staff_idx, prev_chord);
                }
            }
        }

        Chord *chord = (Chord *) cr;
        /* TODO again here, we should not just set the up note but the
         * /correct/ note need to check whether GP supports
         * multi-note gliss. I think it can in modern versions */
        s->set_end_element(chord->up_note());
        s->set_tick2(chord->segment()->tick());
        s->set_track2(staff_idx);
        score->add_element(s);
        if (sl == ::mu::iex::guitarpro::GuitarPro::legato_slide) {
            create_slur(false, staff_idx, cr);
        }
    }
    // slide out downwards (fall)
    if (sl & ::mu::iex::guitarpro::GuitarPro::slide_out_down) {
        ChordLine *sld = Factory::create_chord_line(mu::engraving::to_chord(cr));
        sld->set_chord_line_type(ChordLineType::FALL);
        cr->add(sld);
    }
    // slide out upwards (doit)
    if (sl & ::mu::iex::guitarpro::GuitarPro::slide_out_up) {
        ChordLine *slu = Factory::create_chord_line(mu::engraving::to_chord(cr));
        slu->set_chord_line_type(ChordLineType::DOIT);
        cr->add(slu);
    }
    // slide in from below (plop)
    if (sl & ::mu::iex::guitarpro::GuitarPro::slide_in_below) {
        ChordLine *slb = Factory::create_chord_line(mu::engraving::to_chord(cr));
        slb->set_chord_line_type(ChordLineType::PLOP);
        cr->add(slb);
    }
    // slide in from above (scoop)
    if (sl & ::mu::iex::guitarpro::GuitarPro::slide_in_above) {
        ChordLine *sla = Factory::create_chord_line(mu::engraving::to_chord(cr));
        sla->set_chord_line_type(ChordLineType::SCOOP);
        cr->add(sla);
    }
}

//---------------------------------------------------------
//   readChannels
//---------------------------------------------------------

void GuitarPro::read_channels() {
    for (int i = 0; i < ::mu::iex::guitarpro::gp_max_track_number * 2; ++i) {
        channel_defaults[i].patch = read_int();
        channel_defaults[i].volume = read_u_int8() * 8 - 1;
        channel_defaults[i].pan = read_u_int8() * 8 - 1;
        channel_defaults[i].chorus = read_u_int8() * 8 - 1;
        channel_defaults[i].reverb = read_u_int8() * 8 - 1;
        channel_defaults[i].phase = read_u_int8() * 8 - 1;
        channel_defaults[i].tremolo = read_u_int8() * 8 - 1;

        // defaults of 255, or any value above 127, are set to 0 (Musescore range is 0-127)
        if (channel_defaults[i].patch > 127) {
            channel_defaults[i].patch = 0;
        }
        if (channel_defaults[i].volume > 127) {
            channel_defaults[i].volume = 0;
        }
        if (channel_defaults[i].pan > 127) {
            channel_defaults[i].pan = 0;
        }
        if (channel_defaults[i].chorus > 127) {
            channel_defaults[i].chorus = 0;
        }
        if (channel_defaults[i].reverb > 127) {
            channel_defaults[i].reverb = 0;
        }
        if (channel_defaults[i].phase > 127) {
            channel_defaults[i].phase = 0;
        }
        if (channel_defaults[i].tremolo > 127) {
            channel_defaults[i].tremolo = 0;
        }

        // skip over blank information included for backwards compatibility with 3.0
        skip(2);
    }
}

//---------------------------------------------------------
//   len2fraction
//---------------------------------------------------------

Fraction GuitarPro::len2fraction(int len) {
    Fraction l;
    switch (len) {
        case -2: l.set(1, 1); break;
        case -1: l.set(1, 2); break;
        case 0: l.set(1, 4); break;
        case 1: l.set(1, 8); break;
        case 2: l.set(1, 16); break;
        case 3: l.set(1, 32); break;
        case 4: l.set(1, 64); break;
        case 5: l.set(1, 128); break;
        // set to len - in some cases we get whacky numbers for this (40, 28...)
        default: l.set(1, len);
    }
    return l;
}

//---------------------------------------------------------
//   readMixChange
//---------------------------------------------------------

bool GuitarPro::read_mix_change(Measure *measure) {
    /*char patch   =*/
    read_char();
    signed char volume = read_char();
    signed char pan = read_char();
    signed char chorus = read_char();
    signed char reverb = read_char();
    signed char phase = read_char();
    signed char tremolo = read_char();
    int temp = read_int();

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
            previous_tempo = temp;
            set_tempo(temp, measure);
        }
        read_char();
    }
    return true;
}

//---------------------------------------------------------
//   createMeasures
//---------------------------------------------------------

void GuitarPro::create_measures() {
    Fraction tick = Fraction(0, 1);
    Fraction ts;
    LOGD("measures %zu bars.size %zu", measures, bars.size());

    //      for (int i = 0; i < measures; ++i) {
    for (size_t i = 0; i < bars.size(); ++i) { // ?? (ws)
        Fraction nts = bars[i].timesig;
        Measure *m = Factory::create_measure(score);
        m->set_tick(tick);
        m->set_timesig(nts);
        m->set_ticks(nts);

        if (i == 0 || ts.numerator() != nts.numerator() || ts.denominator() != nts.denominator()) {
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                const Staff *staff = score->staff(staff_idx);
                const StaffType *staff_type = staff->staff_type(Fraction(0, 1)); // at tick 0
                if (staff_type->gen_timesig()) {
                    Segment *s = m->get_segment(SegmentType::TIME_SIG, tick);
                    TimeSig *t = Factory::create_time_sig(s);
                    t->set_track(staff_idx * ::mu::engraving::voices);
                    t->set_sig(nts);
                    s->add(t);
                }
            }
        }
        if (i == 0 || (bars[i].keysig != ::mu::iex::guitarpro::gp_invalid_keysig)) {
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                int keysig = bars[i].keysig != ::mu::iex::guitarpro::gp_invalid_keysig ? bars[i].keysig : key;
                if (tick.is_zero() || (int) score->staff(staff_idx)->key(tick) != (int) Key(keysig)) {
                    Segment *s = m->get_segment(SegmentType::KEY_SIG, tick);
                    KeySig *t = Factory::create_key_sig(s);
                    t->set_key(Key(keysig));
                    t->set_track(staff_idx * ::mu::engraving::voices);
                    s->add(t);
                }
            }
        }
        read_volta(&bars[i].volta, m);
        m->set_repeat_end(bars[i].repeat_flags == mu::engraving::Repeat::END);
        m->set_repeat_start(bars[i].repeat_flags == mu::engraving::Repeat::START);
        m->set_repeat_jump(bars[i].repeat_flags == mu::engraving::Repeat::JUMP);
        //            m->setRepeatFlags(bars[i].repeatFlags);
        m->set_repeat_count(bars[i].repeats); // supported in gp5

        if (bars[i].repeat_flags == (mu::engraving::Repeat::START | mu::engraving::Repeat::END)) {
            m->set_repeat_end(true);
            m->set_repeat_start(true);
            volta_sequence = 1;
        }
        // reset the volta sequence if we have an opening repeat
        if (bars[i].repeat_flags == mu::engraving::Repeat::START) {
            volta_sequence = 1;
        }
        // otherwise, if we see an end repeat symbol, only reset if the bar after it does not contain a volta
        else if (bars[i].repeat_flags == mu::engraving::Repeat::END && i < bars.size() - 1) {
            if (bars[i + 1].volta.volta_info.size() == 0) {
                volta_sequence = 1; // reset  the volta count
            }
        }

        score->measures()->append(m);
        tick += nts;
        ts = nts;
    }
}

//---------------------------------------------------------
//   applyBeatEffects
//---------------------------------------------------------

void GuitarPro::apply_beat_effects(Chord *chord, int beat_effect, bool &has_vibrato_left_hand, bool &has_vibrato_w_trem_bar) {
    /* slap/pop implemented as text until SMuFL has
     * specifications and we can add them to fonts. Note that
     * slap/pop are just added to the top note in the chord,
     * technically these can be applied to individual notes on the
     * UI, but Guitar Pro has no way to express that on the
     * score. To get the same result, we should just add the marking
     * to above the top note.
     */
    if (beat_effect == 1) {
        if (version > 300) {
            add_tap(chord);
        } else {
            has_vibrato_left_hand = true;
        }
    } else if (beat_effect == 2) {
        add_slap(chord->up_note());
    } else if (beat_effect == 3) {
        add_pop(chord->up_note());
    } else if (beat_effect == 4) {
        if (version >= 400) {
            Articulation *a = Factory::create_articulation(chord);
            a->set_sym_id(SymId::GUITAR_FADE_IN);
            a->set_anchor(ArticulationAnchor::TOP);
            a->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
            chord->add(a);
        }
        //TODO-ws       else for (auto n : chord->notes())
        //          n->setHarmonic(true);
    } else if (beat_effect == 5) {
        Articulation *a = Factory::create_articulation(chord);
        a->set_sym_id(SymId::STRINGS_UP_BOW);
        chord->add(a);
    } else if (beat_effect == 6) {
        Articulation *art = Factory::create_articulation(chord);
        art->set_sym_id(SymId::STRINGS_DOWN_BOW);
        chord->add(art);
    } else if (beat_effect == 7) {
        has_vibrato_w_trem_bar = true;
    }
}

#ifdef _MSC_VER
#pragma optimize("", on)
#endif

//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool GuitarPro1::read(IODevice *io) {
    continious_elements_builder_ = std::make_unique<ContiniousElementsBuilder>(score);
    guitar_bend_importer_ = std::make_unique<GuitarBendImporter>(score);

    f = io;
    cur_pos = 30;

    title = read_delphi_string();
    artist = read_delphi_string();
    read_delphi_string();

    int temp = read_int();
    /*uint8_t num =*/read_u_int8(); // Shuffle rhythm feel

    // int octave = 0;
    key = 0;
    if (version > 102) {
        key = read_int(); // key
    }
    staves = version > 102 ? 8 : 1;
    slurs.resize(staves, nullptr);

    //
    // create a part for every staff
    //
    for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
        Part *part = new Part(score);
        Staff *s = Factory::create_staff(part);

        score->append_staff(s);
        score->append_part(part);
    }

    for (size_t i = 0; i < staves; ++i) {
        int tuning[::mu::iex::guitarpro::gp_max_string_number];

        int strings = version > 101 ? read_int() : 6;
        if (strings <= 0 || strings > ::mu::iex::guitarpro::gp_max_string_number) {
            return false;
        }

        for (int j = 0; j < strings; ++j) {
            tuning[j] = read_int();
        }
        std::vector<int> tuning2(strings);
        for (int k = 0; k < strings; ++k) {
            tuning2[strings - k - 1] = tuning[k];
        }

        int frets = 32; // TODO
        bool use_flats = create_tuning_string(strings, &tuning2[0]);
        StringData string_data(frets, strings, &tuning2[0], use_flats);
        Part *part = score->staff(i)->part();
        Instrument *instr = part->instrument();
        instr->set_string_data(string_data);
        instr->set_single_note_dynamics(false);
    }

    measures = read_int();
    if (measures == 0 || measures > 100000) {
        return false;
    }

    Fraction ts;
    Fraction tick = {0, 1};
    for (size_t i = 0; i < measures; ++i) {
        Fraction nts = bars[i].timesig;
        Measure *m = Factory::create_measure(score);
        m->set_tick(tick);
        m->set_timesig(nts);
        m->set_ticks(nts);

        if (i == 0 || ts != nts) {
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                Segment *s = m->get_segment(SegmentType::TIME_SIG, tick);
                TimeSig *t = Factory::create_time_sig(s);
                t->set_track(staff_idx * ::mu::engraving::voices);
                t->set_sig(nts);
                s->add(t);
            }
        }

        score->measures()->append(m);
        tick += nts;
        ts = nts;
    }

    previous_tempo = temp;
    Measure *measure = score->first_measure();
    bool mix_change = false;
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
            track_idx_t track = staff_idx * ::mu::engraving::voices;
            Fraction fraction = measure->tick();
            int beats = read_int();
            for (int beat = 0; beat < beats; ++beat) {
                //                        int pause = 0;
                uint8_t beat_bits = read_u_int8();
                bool dotted = beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_dotted;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_pause) {
                    /*pause =*/
                    read_u_int8();
                }
                int len = read_char();
                int tuple = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_tuplet) {
                    tuple = read_int();
                }
                Segment *segment = measure->get_segment(SegmentType::CHORD_REST, fraction);
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_chord) {
                    int num_strings = static_cast<int>(score->staff(staff_idx)->part()->instrument()->string_data()->strings());
                    int header = read_u_int8();
                    String name;
                    if ((header & 1) == 0) {
                        name = read_delphi_string();
                        read_chord(segment, static_cast<int>(track), num_strings, name, false);
                    } else {
                        skip(25);
                        name = read_pascal_string(34);
                        read_chord(segment, static_cast<int>(track), num_strings, name, true);
                        skip(36);
                    }
                }
                Lyrics *lyrics = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_lyrics) {
                    lyrics = Factory::create_lyrics(score->dummy());
                    lyrics->set_plain_text(read_delphi_string());
                }
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_effects_value) {
                    read_beat_effects(static_cast<int>(track), segment);
                }

                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_mix_change) {
                    read_mix_change(measure);
                    mix_change = true;
                }

                int strings = read_u_int8(); // used strings mask

                Fraction l = len2fraction(len);
                ChordRest *cr;
                if (strings) {
                    cr = Factory::create_chord(score->dummy());
                } else {
                    cr = Factory::create_rest(score->dummy());
                }
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
                        tuplet->set_tick(fraction);
                        tuplet->set_track(cr->track());
                        tuplets[staff_idx] = tuplet;
                        set_tuplet(tuplet, tuple);
                    }
                    tuplet->set_track(track);
                    tuplet->set_base_len(l);
                    tuplet->set_ticks(l * tuplet->ratio().denominator());
                    cr->set_tuplet(tuplet);
                    tuplet->add(cr); //TODOxxx
                }

                cr->set_ticks(l);
                cr->set_duration_type(d);
                segment->add(cr);
                Staff *staff = cr->staff();
                int num_strings = static_cast<int>(staff->part()->instrument()->string_data()->strings());
                ReadNoteResult read_result;
                bool has_let_ring = false;
                bool has_palm_mute = false;
                for (int i = 6; i >= 0; --i) {
                    if (strings & (1 << i) && ((6 - i) < num_strings)) {
                        Note *note = Factory::create_note(to_chord(cr));
                        to_chord(cr)->add(note);
                        read_result = read_note(6 - i, note);
                        has_let_ring = read_result.let_ring || has_let_ring;
                        has_palm_mute = read_result.palm_mute || has_palm_mute;
                        note->set_tpc_from_pitch();
                    }
                }
                rests_for_empty_beats(segment, measure, cr, l, static_cast<int>(track), fraction);
                fraction += cr->actual_ticks();
                measure_len += cr->actual_ticks();
                add_let_ring(cr, has_let_ring);
                add_palm_mute(cr, has_palm_mute);
            }
            if (measure_len < measure->ticks()) {
                score->set_rest(fraction, track, measure->ticks() - measure_len, false, nullptr, false);
                continious_elements_builder_->notify_uncompleted_measure();
            }
        }
        if (bar == 1 && !mix_change) {
            set_tempo(temp, score->first_measure());
        }
    }

    continious_elements_builder_->add_elements_to_score();
    add_tunings();
    return true;
}

//---------------------------------------------------------
//   setTempo
//---------------------------------------------------------

void GuitarPro::set_tempo(int new_tempo, Measure *measure) {
    if (!measure || measure == last_measure) {
        return;
    }

    last_measure = measure;

    if (last_tempo != new_tempo) {
        Segment *segment = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
        TempoText *tt = Factory::create_tempo_text(segment);
        tt->set_tempo(BeatsPerSecond::from_bpm(new_tempo));
        tt->set_xml_text(String(u"<sym>metNoteQuarterUp</sym> = %1").arg(new_tempo));
        tt->set_track(0);
        segment->add(tt);
        tempo = new_tempo;
        last_tempo = new_tempo;
    }
}

//---------------------------------------------------------
//   readChord
//---------------------------------------------------------

void GuitarPro::read_chord(Segment *seg, int track, int num_strings, String name, bool gp_header) {
    int first_fret = read_int();
    if (first_fret || gp_header) {
        /// showing chord instead of fret diagrams
        if (true) {
            StaffText *staff_text = Factory::create_staff_text(seg);
            staff_text->set_track(track);
            staff_text->set_plain_text(name);
            seg->add(staff_text);
            skip(4 * (gp_header ? 7 : 6));
        } else {
            FretDiagram *fret = Factory::create_fret_diagram(seg);
            fret->set_track(track);
            fret->set_strings(num_strings);
            fret->set_fret_offset(first_fret - 1);
            for (int i = 0; i < (gp_header ? 7 : 6); ++i) {
                int current_fret = read_int();
                // read the frets and add them to the fretboard
                // subtract 1 extra from numStrings as we count from 0
                if (i > num_strings - 1) {
                } else if (current_fret > 0) {
                    fret->set_dot(num_strings - 1 - i, current_fret - first_fret + 1, true);
                } else if (current_fret == 0) {
                    fret->set_dot(num_strings - 1 - i, 0, true);
                    fret->set_marker(num_strings - 1 - i, FretMarkerType::CIRCLE);
                } else if (current_fret == -1) {
                    fret->set_dot(num_strings - 1 - i, 0, true);
                    fret->set_marker(num_strings - 1 - i, FretMarkerType::CROSS);
                }
            }
            seg->add(fret);
            if (!name.is_empty()) {
                Harmony *harmony = new Harmony(seg);
                harmony->set_harmony(name);
                harmony->set_track(track);
                fret->add(harmony);
            }
        }
    } else if (!name.is_empty()) {
        Harmony *harmony = new Harmony(seg);
        harmony->set_harmony(name);
        harmony->set_track(track);
        seg->add(harmony);
    }
}

//---------------------------------------------------------
//   restsForEmptyBeats
//---------------------------------------------------------

void GuitarPro::rests_for_empty_beats(Segment *seg, Measure *measure, ChordRest *cr, Fraction &l, int track, const Fraction &tick) {
    /* this can happen as Guitar Pro versions 5 and below allows
     * users to create empty segments. Here, we create rests and
     * make them invisible so users get the same visual if they are
     * at a valid tick of the score. */
    if (seg->empty()) {
        if (tick < measure->end_tick()) {
            cr = Factory::create_rest(seg);
            cr->set_track(track);
            TDuration d(l);
            cr->set_duration_type(d);
            cr->set_visible(false);
            seg->add(cr);
        } else {
            measure->remove(seg);
        }
    }
}

//---------------------------------------------------------
//   createSlur
//---------------------------------------------------------

void GuitarPro::create_slur(bool has_slur, staff_idx_t staff_idx, ChordRest *cr) {
    if (has_slur && (slurs[staff_idx] == 0)) {
        Slur *slur = Factory::create_slur(score->dummy());
        slur->set_track(cr->track());
        slur->set_track2(cr->track());
        slur->set_tick(cr->tick());
        slur->set_tick2(cr->tick());
        slur->set_start_element(cr);
        slur->set_end_element(cr);
        slurs[staff_idx] = slur;
        score->add_element(slur);
    } else if (slurs[staff_idx] && !has_slur) {
        Slur *s = slurs[staff_idx];
        slurs[staff_idx] = 0;
        s->set_end_element(cr);
        s->set_tick2(cr->tick());
        s->set_track2(cr->track());
    }
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool GuitarPro2::read(IODevice *io) {
    continious_elements_builder_ = std::make_unique<ContiniousElementsBuilder>(score);
    guitar_bend_importer_ = std::make_unique<GuitarBendImporter>(score);

    f = io;
    cur_pos = 30;

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

    /*uint8_t num =*/read_u_int8(); // Shuffle rhythm feel

    int temp = read_int();

    // int octave = 0;
    /*int key =*/read_int(); // key

    for (int i = 0; i < ::mu::iex::guitarpro::gp_max_track_number * 2; ++i) {
        channel_defaults[i].patch = read_int();
        channel_defaults[i].volume = read_u_int8() * 8 - 1;
        channel_defaults[i].pan = read_u_int8() * 8 - 1;
        channel_defaults[i].chorus = read_u_int8() * 8 - 1;
        channel_defaults[i].reverb = read_u_int8() * 8 - 1;
        channel_defaults[i].phase = read_u_int8() * 8 - 1;
        channel_defaults[i].tremolo = read_u_int8() * 8 - 1;
        read_u_int8(); // padding
        read_u_int8();
    }
    measures = read_int();
    staves = read_int();
    if (measures == 0 || measures > 100000) {
        return false;
    }
    init_dynamics(staves);

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
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_repeat_end) {
            bar.repeat_flags = bar.repeat_flags | mu::engraving::Repeat::END;
            bar.repeats = read_u_int8() + 1;
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_volta) {
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
            /*int color =*/read_int();         // color?
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_keysig) {
            bar.keysig = read_u_int8();
            /*uint8_t c    =*/read_u_int8(); // minor
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

    Fraction ts;
    Fraction tick = {0, 1};
    for (size_t i = 0; i < measures; ++i) {
        Fraction nts = bars[i].timesig;
        Measure *m = Factory::create_measure(score);
        m->set_tick(tick);
        m->set_timesig(nts);
        m->set_ticks(nts);

        if (i == 0 || ts != nts) {
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                Segment *s = m->get_segment(SegmentType::TIME_SIG, tick);
                TimeSig *t = Factory::create_time_sig(s);
                t->set_track(staff_idx * ::mu::engraving::voices);
                t->set_sig(nts);
                s->add(t);
            }
        }

        score->measures()->append(m);
        tick += nts;
        ts = nts;
    }

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
        /*int midiPort     =*/read_int(); //  - 1;
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

        //
        // determine clef
        //
        Staff *staff = score->staff(i);
        int patch = channel_defaults[midi_channel].patch;
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
        Segment *segment = measure->get_segment(SegmentType::HEADER_CLEF, Fraction(0, 1));
        Clef *clef = Factory::create_clef(segment);
        clef->set_clef_type(clef_id);
        clef->set_track(i * ::mu::engraving::voices);
        segment->add(clef);

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
        if (midi_channel == int(StaffTypes::PERC_DEFAULT)) {
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
        // missing: phase, tremolo
    }

    previous_tempo = temp;
    Measure *measure = score->first_measure();
    bool mix_change = false;
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
        // Tuplet* tuplets[staves];
        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            tuplets[staff_idx] = 0;
        }

        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            Fraction measure_len = {0, 1};
            track_idx_t track = staff_idx * ::mu::engraving::voices;
            Fraction fraction = measure->tick();
            int beats = read_int();
            for (int beat = 0; beat < beats; ++beat) {
                //                        int pause = 0;
                uint8_t beat_bits = read_u_int8();
                bool dotted = beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_dotted;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_pause) {
                    /*pause =*/
                    read_u_int8();
                }
                int len = read_char();
                int tuple = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_tuplet) {
                    tuple = read_int();
                }
                Segment *segment = measure->get_segment(SegmentType::CHORD_REST, fraction);
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_chord) {
                    int num_strings = static_cast<int>(score->staff(staff_idx)->part()->instrument()->string_data()->strings());
                    int header = read_u_int8();
                    String name;
                    if ((header & 1) == 0) {
                        name = read_delphi_string();
                        read_chord(segment, static_cast<int>(track), num_strings, name, false);
                    } else {
                        skip(25);
                        name = read_pascal_string(34);
                        read_chord(segment, static_cast<int>(track), num_strings, name, true);
                        skip(36);
                    }
                }
                Lyrics *lyrics = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_lyrics) {
                    String txt = read_delphi_string();
                    lyrics = Factory::create_lyrics(score->dummy());
                    lyrics->set_plain_text(txt);
                }
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_effects_value) {
                    read_beat_effects(static_cast<int>(track), segment);
                }

                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_mix_change) {
                    read_mix_change(measure);
                    mix_change = true;
                }

                int strings = read_u_int8(); // used strings mask

                Fraction l = len2fraction(len);
                ChordRest *cr;
                if (strings) {
                    cr = Factory::create_chord(segment);
                } else {
                    cr = Factory::create_rest(segment);
                }
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
                        tuplet->set_tick(fraction);
                        tuplet->set_track(cr->track());
                        tuplets[staff_idx] = tuplet;
                        set_tuplet(tuplet, tuple);
                    }
                    tuplet->set_track(track);
                    tuplet->set_base_len(l);
                    tuplet->set_ticks(l * tuplet->ratio().denominator());
                    cr->set_tuplet(tuplet);
                    tuplet->add(cr);
                }

                cr->set_ticks(l);
                cr->set_duration_type(d);
                segment->add(cr);
                Staff *staff = cr->staff();
                int num_strings = static_cast<int>(staff->part()->instrument()->string_data()->strings());
                ReadNoteResult read_result;
                bool has_let_ring = false;
                bool has_palm_mute = false;
                for (int i = 6; i >= 0; --i) {
                    if (strings & (1 << i) && ((6 - i) < num_strings)) {
                        Note *note = Factory::create_note(to_chord(cr));
                        to_chord(cr)->add(note);
                        read_result = read_note(6 - i, note);
                        has_let_ring = read_result.let_ring || has_let_ring;
                        has_palm_mute = read_result.palm_mute || has_palm_mute;
                        note->set_tpc_from_pitch();
                    }
                }
                rests_for_empty_beats(segment, measure, cr, l, static_cast<int>(track), fraction);
                fraction += cr->actual_ticks();
                measure_len += cr->actual_ticks();
                add_let_ring(cr, has_let_ring);
                add_palm_mute(cr, has_palm_mute);
            }
            if (measure_len < measure->ticks()) {
                score->set_rest(fraction, track, measure->ticks() - measure_len, false, nullptr, false);
                continious_elements_builder_->notify_uncompleted_measure();
            }
        }
        if (bar == 1 && !mix_change) {
            set_tempo(temp, score->first_measure());
        }
    }

    continious_elements_builder_->add_elements_to_score();
    add_tunings();
    return true;
}

//---------------------------------------------------------
//   readNote
//---------------------------------------------------------

GuitarPro::ReadNoteResult GuitarPro1::read_note(int string, Note *note) {
    ReadNoteResult read_result;
    uint8_t note_bits = read_u_int8();
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_ghost) {
        if (version == 300) {
            note->set_ghost(true);
        } else {
            note->set_head_group(NoteHeadGroup::HEAD_CROSS);
            note->set_ghost(true);
        }
    }

    bool tie_note = false;
    uint8_t variant = 1;
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_dead) {
        variant = read_u_int8();
        if (variant == 1) { // normal note
        } else if (variant == 2) {
            tie_note = true;
        } else if (variant == 3) { // dead notes
            note->set_head_group(NoteHeadGroup::HEAD_CROSS);
            note->set_dead_note(true);
        } else {
            LOGD("unknown note variant: %d", variant);
        }
    }

    //
    // noteBits:
    //    7 - Right hand or left hand fingering;
    //    6 - Accentuated note
    //    5 - Note type (rest, empty note, normal note);
    //    4 - note dynamic;
    //    3 - Presence of effects linked to the note;
    //    2 - Ghost note;
    //    1 - Dotted note;  ?
    //    0 - Time-independent duration

    if (note_bits & 0x1) {     // note != beat
        int a = read_u_int8(); // length
        int b = read_u_int8(); // t
        LOGD("Time independent note len, len %d t %d", a, b);
    }
    if (note_bits & 0x2) { // note is dotted
        //readUInt8();
    }

    int &previous_dynamic = previous_dynamic_by_track[note->track()];

    // set dynamic information on note if different from previous note
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_dynamic) {
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
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_fret) {
        fret_number = read_u_int8();
    }

    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_fingering) { // fingering
        int a = read_u_int8();
        int b = read_u_int8();
        LOGD("Fingering=========%d %d", a, b);
    }
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::beat_effects_value) {
        uint8_t mod_mask1 = read_u_int8();
        uint8_t mod_mask2 = 0;
        if (version >= 400) {
            mod_mask2 = read_u_int8();
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_bend) {
            read_bend(note);
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_grace) {
            // GP3 grace note
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
            int grace_pitch = note->staff()->part()->instrument()->string_data()->get_pitch(string, fret, note->staff(), note->tick());
            gn->set_pitch(grace_pitch);
            gn->set_tpc_from_pitch();

            Chord *gc = nullptr;
            if (note->chord()->grace_notes().size()) {
                gc = note->chord()->grace_notes().front();
            }
            if (!gc) {
                gc = Factory::create_chord(score->dummy());
                TDuration d;
                d.set_val(grace_len);
                if (grace_len == Constants::division / 6) {
                    d.set_dots(1);
                }
                gc->set_duration_type(d);
                gc->set_ticks(d.fraction());
                gc->set_note_type(NoteType::ACCIACCATURA);
                gc->mutldata()->set_mag(note->chord()->staff()->staff_mag(Fraction(0, 1)) * score->style().style_d(Sid::GRACE_NOTE_MAG));
                note->chord()->add(gc); // sets parent + track
                add_dynamic(gn, dynamic);
            }

            gc->add(gn);

            if (transition == 0) {
                // no transition
            } else if (transition == 1) {
                //note->setSlideNote(gn);
                Glissando *glis = new Glissando(gn);
                glis->set_glissando_type(GlissandoType::STRAIGHT);
                gn->chord()->add(glis);
                glis->set_start_element(gn);
                glis->set_tick(gn->chord()->tick());
                glis->set_track(gn->track());
                glis->set_end_element(note);
                glis->set_tick2(note->chord()->tick());
                glis->set_track2(note->track());
                score->add_element(glis);
                //HammerOn here??? Maybe version...

                Slur *slur1 = Factory::create_slur(score->dummy());
                slur1->set_start_element(gc);
                slur1->set_end_element(note->chord());
                slur1->set_tick(gc->tick());
                slur1->set_tick2(note->chord()->tick());
                slur1->set_track(gc->track());
                slur1->set_track2(note->track());
                score->add_element(slur1);

                //TODO: Add a 'slide' guitar effect when implemented
            } else if (transition == 2 && fret_number >= 0 && fret_number <= 255 && fret_number != gn->fret()) {
                /*QList<PitchValue> points;
                points.append(PitchValue(0,0, false));
                points.append(PitchValue(60,(fretNumber-gn->fret())*100, false));

                Bend* b = Factory::createBend(note->score());
                b->setPoints(points);
                b->setTrack(gn->track());
                gn->add(b);*/
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
                score->add_element(slur1);
            }
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_hammer) { // hammer on / pull off
            read_result.slur = true;
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_let_ring) { // let ring
            read_result.let_ring = true;
        }
        if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_slide_old) {
            slide_list.push_back(note);
        }

        if (version >= 400) {
            if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_staccato) {}
            if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_palm_mute) {
                read_result.palm_mute = true;
            }
            if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_tremolo) {
                read_u_int8();
            }
            if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_artificial_harmonic) {
                /*int type =*/
                read_u_int8();
                //TODO-ws           if (type == 1 || type == 4 || type == 5)
                //                    note->setHarmonic(true);
            }
            if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_trill) {
                //TODO-ws                        note->setTrillFret(readUInt8());      // trill fret
                read_u_int8(); // trill length
            }
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

    /* it's possible to specify extraordinarily high pitches by
    specifying fret numbers that don't exist. This is an issue that
    comes from tuxguitar. Just set to maximum pitch. GP6 actually
    sets the fret number to 0 also, so that's what I've opted to do
    here. */
    if (pitch > ::mu::iex::guitarpro::GuitarPro::max_pitch) {
        fret_number = 0;
        pitch = ::mu::iex::guitarpro::GuitarPro::max_pitch;
    }

    note->set_fret(fret_number);
    note->set_string(string);
    note->set_pitch(pitch);

    if (tie_note) {
        bool found = false;
        Chord *chord = note->chord();
        Segment *segment = chord->segment()->prev1(SegmentType::CHORD_REST);
        track_idx_t track = note->track();
        std::vector<Chord *> chords;
        Note *true_note = nullptr;
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
                            found = true;
                            true_note = note2;
                            break;
                        }
                    }
                    if (!found) {
                        chords.push_back(chord2);
                    }
                }
                if (found) {
                    break;
                }
            }
            segment = segment->prev1(SegmentType::CHORD_REST);
        }

        if (chords.size() && true_note) {
            Note *end_note = note;
            for (unsigned int i = 0; i < chords.size(); ++i) {
                Note *note2 = Factory::create_note(chords[i]);
                note2->set_string(true_note->string());
                note2->set_fret(true_note->fret());
                note2->set_pitch(true_note->pitch());
                note2->set_tpc_from_pitch();
                chords[i]->add(note2);
                Tie *tie = Factory::create_tie(note2);
                tie->set_end_note(end_note);
                end_note = note2;
                note2->add(tie);
            }
            Tie *tie = Factory::create_tie(true_note);
            tie->set_end_note(end_note);
            true_note->add(tie);
        }
    }

    return read_result;
}

//---------------------------------------------------------
//   readBeatEffects
//---------------------------------------------------------

int GuitarPro1::read_beat_effects(int, Segment *) {
    uint8_t fx_bits1 = read_u_int8();
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_effect) {
        uint8_t num = read_u_int8();
        switch (num) {
            case 0: // tremolo bar
                read_int();
                break;
            default: read_int(); break;
        }
    }
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_arpeggio) {
        read_u_int8(); // down stroke length
        read_u_int8(); // up stroke length
    }
    return 0;
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool GuitarPro3::read(IODevice *io) {
    continious_elements_builder_ = std::make_unique<ContiniousElementsBuilder>(score);
    guitar_bend_importer_ = std::make_unique<GuitarBendImporter>(score);

    f = io;
    cur_pos = 30;

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
    for (int i = 0, n = read_int(); i < n; ++i) {
        comments.append(read_delphi_string());
    }

    /*uint8_t num =*/read_u_int8(); // Shuffle rhythm feel

    int temp = read_int();

    // int octave = 0;
    key = read_int(); // key

    for (int i = 0; i < ::mu::iex::guitarpro::gp_max_track_number * 2; ++i) {
        channel_defaults[i].patch = read_int();
        channel_defaults[i].volume = read_u_int8() * 8 - 1;
        channel_defaults[i].pan = read_u_int8() * 8 - 1;
        channel_defaults[i].chorus = read_u_int8() * 8 - 1;
        channel_defaults[i].reverb = read_u_int8() * 8 - 1;
        channel_defaults[i].phase = read_u_int8() * 8 - 1;
        channel_defaults[i].tremolo = read_u_int8() * 8 - 1;
        read_u_int8(); // padding
        read_u_int8();
    }
    measures = read_int();
    staves = read_int();
    if (measures == 0 || measures > 100000) {
        return false;
    }
    init_dynamics(staves);

    slurs.resize(staves, nullptr);

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
            // voltas are represented as a binary number
            bar.volta.volta_type = ::mu::iex::guitarpro::gp_volta_binary;
            while (volta_number > 0) {
                bar.volta.volta_info.push_back(volta_number & 1);
                volta_number >>= 1;
            }
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_marker) {
            bar.marker = read_delphi_string(); // new section?
            /*int color =*/read_int();         // color?
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

    Fraction ts;
    Fraction tick = {0, 1};
    for (size_t i = 0; i < measures; ++i) {
        Fraction nts = bars[i].timesig;
        Measure *m = Factory::create_measure(score);
        m->set_tick(tick);
        m->set_timesig(nts);
        m->set_ticks(nts);

        if (i == 0 || ts != nts) {
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                Segment *s = m->get_segment(SegmentType::TIME_SIG, tick);
                TimeSig *t = Factory::create_time_sig(s);
                t->set_track(staff_idx * ::mu::engraving::voices);
                t->set_sig(nts);
                s->add(t);
            }
        }
        if (i == 0 && key) {
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                Segment *s = m->get_segment(SegmentType::KEY_SIG, tick);
                KeySig *t = Factory::create_key_sig(s);
                t->set_key(Key(key));
                t->set_track(staff_idx * ::mu::engraving::voices);
                s->add(t);
            }
        }

        read_volta(&bars[i].volta, m);
        m->set_repeat_end(bars[i].repeat_flags == mu::engraving::Repeat::END);
        m->set_repeat_start(bars[i].repeat_flags == mu::engraving::Repeat::START);
        m->set_repeat_jump(bars[i].repeat_flags == mu::engraving::Repeat::JUMP);
        //            m->setRepeatFlags(bars[i].repeatFlags);
        m->set_repeat_count(bars[i].repeats);

        // reset the volta sequence if we have an opening repeat
        if (bars[i].repeat_flags == mu::engraving::Repeat::START) {
            volta_sequence = 1;
        }
        // otherwise, if we see an end repeat symbol, only reset if the bar after it does not contain a volta
        else if (bars[i].repeat_flags == mu::engraving::Repeat::END && i < bars.size() - 1) {
            if (bars[i + 1].volta.volta_info.size() == 0) {
                volta_sequence = 1;
            }
        }

        score->measures()->append(m);
        tick += nts;
        ts = nts;
    }

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
        //
        // determine clef
        //
        Staff *staff = score->staff(i);
        int patch = channel_defaults[midi_channel].patch;
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
        Segment *segment = measure->get_segment(SegmentType::HEADER_CLEF, Fraction(0, 1));
        Clef *clef = Factory::create_clef(segment);
        clef->set_clef_type(clef_id);
        clef->set_track(i * ::mu::engraving::voices);
        segment->add(clef);

        if (capo > 0) {
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

    previous_tempo = temp;
    Measure *measure = score->first_measure();
    bool mix_change = false;
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
            track_idx_t track = staff_idx * ::mu::engraving::voices;
            Fraction fraction = measure->tick();
            int beats = read_int();
            if (beats > 200) {
                return false;
            }
            for (int beat = 0; beat < beats; ++beat) {
                bool has_slur = false;
                bool has_let_ring = false;
                bool has_palm_mute = false;
                bool has_vibrato_left_hand = false;
                bool has_vibrato_w_trem_bar = false;

                //                        int pause = 0;
                uint8_t beat_bits = read_u_int8();
                bool dotted = beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_dotted;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_pause) {
                    /*pause =*/
                    read_u_int8();
                }

                slide = -1;
                if (muse::contains(slides, static_cast<int>(track))) {
                    slide = muse::take(slides, static_cast<int>(track));
                }

                int len = read_char();
                int tuple = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_tuplet) {
                    tuple = read_int();
                }

                Segment *segment = measure->get_segment(SegmentType::CHORD_REST, fraction);
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_chord) {
                    int num_strings = static_cast<int>(score->staff(staff_idx)->part()->instrument()->string_data()->strings());
                    int header = read_u_int8();
                    String name;
                    if ((header & 1) == 0) {
                        name = read_delphi_string();
                        read_chord(segment, static_cast<int>(track), num_strings, name, false);
                    } else {
                        skip(25);
                        name = read_pascal_string(34);
                        read_chord(segment, static_cast<int>(track), num_strings, name, false);
                        skip(36);
                    }
                }
                Lyrics *lyrics = 0;
                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_lyrics) {
                    String txt = read_delphi_string();
                    Lyrics *lyrics2 = Factory::create_lyrics(score->dummy());
                    lyrics2->set_plain_text(txt);
                }
                int beat_effects = 0;

                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_effects_value) {
                    beat_effects = read_beat_effects(static_cast<int>(track), segment);
                }
                bool vibrato = beat_effects & 0x1 || beat_effects & 0x2;

                if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_mix_change) {
                    read_mix_change(measure);
                    mix_change = true;
                }

                int strings = read_u_int8(); // used strings mask

                Fraction l = len2fraction(len);

                // Some beat effects could add a Chord before this
                ChordRest *cr = segment->cr(track);
                // if (!pause || strings)
                if (strings) {
                    if (!segment->cr(track)) {
                        cr = Factory::create_chord(segment);
                    }
                } else {
                    if (segment->cr(track)) {
                        segment->remove(segment->cr(track));
                        delete cr;
                        cr = 0;
                    }
                    cr = Factory::create_rest(segment);
                }

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
                        tuplet->set_tick(fraction);
                        tuplet->set_track(cr->track());
                        tuplets[staff_idx] = tuplet;
                        set_tuplet(tuplet, tuple);
                    }
                    tuplet->set_track(track);
                    tuplet->set_base_len(l);
                    tuplet->set_ticks(l * tuplet->ratio().denominator());
                    cr->set_tuplet(tuplet);
                    tuplet->add(cr);
                }

                cr->set_ticks(l);

                if (cr->is_rest() && l >= measure->ticks()) {
                    cr->set_duration_type(DurationType::V_MEASURE);
                    cr->set_ticks(measure->ticks());
                } else {
                    cr->set_duration_type(d);
                }

                if (!segment->cr(track)) {
                    segment->add(cr);
                }

                Staff *staff = cr->staff();
                int num_strings = static_cast<int>(staff->part()->instrument()->string_data()->strings());
                for (int i = 6; i >= 0; --i) {
                    if (strings & (1 << i) && ((6 - i) < num_strings)) {
                        Note *note = Factory::create_note(to_chord(cr));
                        to_chord(cr)->add(note);
                        if (vibrato) {
                            has_vibrato_left_hand = true;
                        }
                        if (dotted) {
                            NoteDot *dot = Factory::create_note_dot(note);
                            // there is at most one dotted note in this guitar pro version - set 0 index
                            dot->set_track(track); // needed to know the staff it belongs to (and detect tablature)
                            dot->set_visible(true);
                            note->add(dot);
                        }
                        ReadNoteResult read_result = read_note(6 - i, note);
                        has_slur = read_result.slur || has_slur;
                        has_let_ring = read_result.let_ring || has_let_ring;
                        has_palm_mute = read_result.palm_mute || has_palm_mute;
                        has_vibrato_left_hand = read_result.vibrato || has_vibrato_left_hand;
                        note->set_tpc_from_pitch();
                    }
                }
                if (cr && cr->is_chord() && to_chord(cr)->notes().empty()) {
                    if (segment->cr(track)) {
                        segment->remove(cr);
                    }
                    delete cr;
                    cr = Factory::create_rest(segment);
                    cr->set_ticks(l);
                    cr->set_track(track);
                    cr->set_duration_type(d);
                    segment->add(cr);
                }
                create_slur(has_slur, staff_idx, cr);

                if (cr && (cr->is_chord())) {
                    if (beat_effects >= 200) {
                        beat_effects -= 200;
                        Articulation *art = Factory::create_articulation(cr);
                        art->set_sym_id(SymId::GUITAR_FADE_OUT);
                        art->set_anchor(ArticulationAnchor::TOP);
                        art->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
                        if (!EditChord::toggle_articulation(score, cr, art)) {
                            delete art;
                        }
                    }

                    bool has_vibrato_left_hand_on_beat = false;
                    bool has_vibrato_w_trem_bar_on_beat = false;
                    apply_beat_effects(to_chord(cr), beat_effects, has_vibrato_left_hand_on_beat, has_vibrato_w_trem_bar_on_beat);
                    has_vibrato_left_hand = has_vibrato_left_hand || has_vibrato_left_hand_on_beat;
                    has_vibrato_w_trem_bar = has_vibrato_w_trem_bar || has_vibrato_w_trem_bar_on_beat;
                    if (slide > 0) {
                        create_slide(slide, cr, static_cast<int>(staff_idx));
                    }
                }

                if (cr) {
                    add_let_ring(cr, has_let_ring);
                    add_palm_mute(cr, has_palm_mute);
                    add_vibrato_left_hand(cr, has_vibrato_left_hand);
                    add_vibrato_w_trem_bar(cr, has_vibrato_w_trem_bar);
                }

                rests_for_empty_beats(segment, measure, cr, l, static_cast<int>(track), fraction);
                fraction += cr->actual_ticks();
                measure_len += cr->actual_ticks();
            }
            if (measure_len < measure->ticks()) {
                score->set_rest(fraction, track, measure->ticks() - measure_len, false, nullptr, false);
                continious_elements_builder_->notify_uncompleted_measure();
            }
            bool remove_rests = true;
            int counter = 0;
            Rest *last_rest = nullptr;
            for (auto seg = measure->first(); seg; seg = seg->next()) {
                if (seg->segment_type() == SegmentType::CHORD_REST) {
                    auto cr = seg->cr(track);
                    if (cr && cr->is_chord()) {
                        remove_rests = false;
                        break;
                    } else if (cr) {
                        ++counter;
                        last_rest = to_rest(cr);
                    }
                }
            }
            if (remove_rests && counter < 2) {
                remove_rests = false;
                if (counter == 1) {
                    last_rest->set_ticks(measure->timesig());
                    last_rest->set_duration_type(DurationType::V_MEASURE);
                }
            }
            if (remove_rests) {
                auto seg = measure->first();
                while (seg && seg != measure->last()) {
                    if (seg->segment_type() == SegmentType::CHORD_REST) {
                        auto cr = seg->cr(track);
                        if (cr) {
                            seg->remove(cr);
                            delete cr;
                        }
                    }
                    seg = seg->next();
                }
                auto cr = Factory::create_rest(seg);
                cr->set_ticks(measure->timesig());
                cr->set_duration_type(DurationType::V_MEASURE);
                cr->set_track(track);
                seg->add(cr);
            }
        }
        if (bar == 1 && !mix_change) {
            set_tempo(temp, score->first_measure());
        }
    }
    for (auto n : slide_list) {
        auto segment = n->chord()->segment();
        auto measure1 = segment->measure();
        while ((segment = segment->next1(SegmentType::CHORD_REST)) ||
               ((measure1 = measure1->next_measure()) && (segment = measure1->first()))) {
            // bool br = false;
            auto crest = segment->cr(n->track());
            if (!crest) {
                continue;
            }
            if (crest->type() == mu::engraving::ElementType::REST) {
                break;
            }
            auto cr = to_chord(crest);
            if (!cr) {
                continue;
            }
            if (cr->grace_notes().size()) {
                cr = cr->grace_notes().front();
            }
            if (cr) {
                for (auto nt : cr->notes()) {
                    if (nt->string() == n->string()) {
                        // auto mg = nt->magS();
                        Glissando *s = new Glissando(n);
                        s->set_start_element(n);
                        s->set_tick(n->chord()->segment()->tick());
                        s->set_track(n->track());
                        s->set_glissando_type(GlissandoType::STRAIGHT);
                        s->set_end_element(nt);
                        s->set_tick2(cr->segment()->tick());
                        s->set_track2(n->track());
                        score->add_element(s);
                        break;
                    }
                }
            }
            break;
        }
    }

    continious_elements_builder_->add_elements_to_score();
    guitar_bend_importer_->add_elements_to_score();
    add_tunings();

    return true;
}

//---------------------------------------------------------
//   readBeatEffects
//---------------------------------------------------------

int GuitarPro3::read_beat_effects(int track, Segment *segment) {
    int effects = 0;
    uint8_t fx_bits = read_u_int8();

    if (fx_bits & ::mu::iex::guitarpro::GuitarPro::beat_effect) {
        effects = read_u_int8(); // effect 1-tapping, 2-slapping, 3-popping
        read_int();              // we don't need this integer
    }

    if (fx_bits & ::mu::iex::guitarpro::GuitarPro::beat_arpeggio) {
        int strokeup = read_u_int8();   // up stroke length
        int strokedown = read_u_int8(); // down stroke length

        Arpeggio *a = Factory::create_arpeggio(score->dummy());
        if (strokeup > 0) {
            a->set_arpeggio_type(ArpeggioType::UP_STRAIGHT);
        } else if (strokedown > 0) {
            a->set_arpeggio_type(ArpeggioType::DOWN_STRAIGHT);
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
    if (fx_bits & ::mu::iex::guitarpro::GuitarPro::beat_tremolo) {}
    if (fx_bits & ::mu::iex::guitarpro::GuitarPro::beat_fade) {
        effects += 200;
    }
    if (fx_bits & ::mu::iex::guitarpro::GuitarPro::beat_dotted) {}
    if (fx_bits & ::mu::iex::guitarpro::GuitarPro::beat_chord) {}
    if (effects == 0) {
        return fx_bits;
    }
    return effects;
}

//---------------------------------------------------------
//   readTremoloBar
//---------------------------------------------------------

void GuitarPro::read_tremolo_bar(int /*track*/, Segment * /*segment*/) {
    /*int a1 =*/
    read_char();
    /*int a2 =*/read_char();
    /*int a3 =*/read_char();
    /*int a4 =*/read_char();
    /*int a5 =*/read_char();
    int n = read_int();
    std::vector<PitchValue> points;
    for (int i = 0; i < n; ++i) {
        int time = read_int();
        int pitch = read_int();
        int vibrato = read_u_int8();
        points.push_back(PitchValue(time, pitch, vibrato));
    }
}

void GuitarPro::add_tunings() {
    const Measure *m = score->first_measure();

    // NOTE: GP doesn't support multiple tunings on one part
    // We're safe to just take the very first chord rest segment
    // and check if it has any non-standard tuning
    const Fraction &frac{0, 1};

    for (auto p : score->parts()) {
        for (auto s : p->staves()) {
            if (!s->is_primary_staff() || p->instrument()->use_drumset()) {
                continue;
            }

            Segment *seg = m->find_segment(SegmentType::CHORD_REST, frac);

            IF_ASSERT_FAILED(seg) {
                LOGE() << "First measure MUST has a chord rest segment after import";
                break;
            }

            const StringData sd = *p->instrument()->string_data();
            std::vector<int> tuning(sd.strings());
            for (size_t i = 0; i < tuning.size(); ++i) {
                tuning[i] = sd.string_list().at(i).pitch + p->instrument()->transpose().chromatic;
            }

            if (utils::is_standard_tuning(p->instrument()->channel(0)->program(), tuning)) {
                continue;
            }

            StringTunings *tun = Factory::create_string_tunings(seg);
            tun->set_string_data(sd);
            tun->set_track(staff2track(s->idx()));
            seg->add(tun);
            // Instrument string data should be set to the standard
            tuning = utils::standard_tuning_for(p->instrument()->channel(0)->program(), (int) sd.strings());
            StringData instrument_sd(sd.frets(), (int) tuning.size(), tuning.data());
            p->instrument()->set_string_data(instrument_sd);
        }
    }
}

//---------------------------------------------------------
//   addMetaInfo
//---------------------------------------------------------

static void add_meta_info(MasterScore *score, GuitarPro *gp, bool experimental) {
    std::vector<String> field_names = {gp->title, gp->subtitle, gp->artist, gp->album, gp->composer};

    bool create_title_field =
        std::any_of(field_names.begin(), field_names.end(), [](const String &field_name) { return !field_name.is_empty(); });

    if (!create_title_field && !experimental) {
        return;
    }

    MeasureBase *m = nullptr;
    if (!score->measures()->first()) {
        m = Factory::create_title_v_box(score);
        score->measures()->append(m);
    } else {
        m = score->measures()->first();
        if (!m->is_v_box()) {
            MeasureBase *mb = Factory::create_title_v_box(score);
            score->add_measure(mb, m);
            m = mb;
        }
    }
    if (!gp->title.is_empty() || experimental) {
        Text *s = Factory::create_text(m, TextStyleType::TITLE);
        s->set_plain_text(gp->title);
        m->add(s);
    }
    if (!gp->subtitle.is_empty() || !gp->artist.is_empty() || !gp->album.is_empty() || experimental) {
        Text *s = Factory::create_text(m, TextStyleType::SUBTITLE);
        String str;
        if (!gp->subtitle.is_empty()) {
            str.append(gp->subtitle);
        }
        if (!gp->artist.is_empty()) {
            if (!str.is_empty()) {
                str.append(u'\n');
            }
            str.append(gp->artist);
        }
        if (!gp->album.is_empty()) {
            if (!str.is_empty()) {
                str.append(u'\n');
            }
            str.append(gp->album);
        }
        s->set_plain_text(str);
        m->add(s);
    }
    if (!gp->composer.is_empty()) {
        Text *s = Factory::create_text(m, TextStyleType::COMPOSER);
        s->set_plain_text(gp->composer);
        m->add(s);
    }

    if (!gp->poet.is_empty() || experimental) {
        Text *s = Factory::create_text(m, TextStyleType::LYRICIST);
        if (!gp->poet.is_empty()) {
            s->set_plain_text(muse::mtrc("iex_guitarpro", "Words by %1").arg(gp->poet));
        }

        m->add(s);
    }
}

//---------------------------------------------------------
//   importGTP
//---------------------------------------------------------

Err import_gtp(MasterScore *score, muse::io::IODevice *io, bool experimental) {
    if (!score || !io) {
        return Err::FILE_BAD_FORMAT;
    }
    if (!io->open(IODevice::READ_ONLY)) {
        return Err::FILE_OPEN_ERROR;
    }

    score->check_chord_list();

    if (!io->seek(0)) {
        return Err::FILE_BAD_FORMAT;
    }
    char header[5] = {};
    if (io->read((uint8_t *) (header), 4) != 4) {
        return Err::FILE_BAD_FORMAT;
    }
    header[4] = 0;
    if (!io->seek(0)) {
        return Err::FILE_BAD_FORMAT;
    }
    if (strcmp(header, "ptab") == 0) {
        PowerTab ptb(io, score);
        return ptb.read();
    }

    std::unique_ptr<GuitarPro> gp;
    bool read_result = false;
    bool is_version_above6 = false;
    try {
        // check to see if we are dealing with a GP file via the extension
        if (strcmp(header, "PK\x3\x4") == 0) {
            gp = std::make_unique<GuitarPro7>(score);
            read_result = gp->read(io);
            is_version_above6 = true;
        }
        // check to see if we are dealing with a GPX file via the extension
        else if (strcmp(header, "BCFZ") == 0) {
            gp = std::make_unique<GuitarPro6>(score);
            drumset::init_guitar_pro_drumset();
            read_result = gp->read(io);
            is_version_above6 = true;
        }
        // otherwise it's an older version - check the header
        else if (strcmp(&header[1], "FIC") == 0) {
            uint8_t l = 0;
            if (io->read((uint8_t *) &l, 1) != 1) {
                return Err::FILE_BAD_FORMAT;
            }
            char ss[31] = {};
            if (io->read((uint8_t *) (ss), 30) != 30) {
                return Err::FILE_BAD_FORMAT;
            }
            if (l >= sizeof(ss)) {
                LOGD("invalid gtp version string length %u", static_cast<unsigned>(l));
                return Err::FILE_BAD_FORMAT;
            }
            ss[l] = 0;
            String s = String::from_utf8(ss);
            if (s.starts_with(u"FICHIER GUITAR PRO ")) {
                s = s.mid(20);
            } else if (s.starts_with(u"FICHIER GUITARE PRO ")) {
                s = s.mid(21);
            } else {
                LOGD("unknown gtp format <%s>", ss);
                return Err::FILE_BAD_FORMAT;
            }
            int a = s.left(1).to_int();
            int b = s.mid(2).to_int();
            int version = a * 100 + b;
            if (a == 1) {
                gp = std::make_unique<GuitarPro1>(score, version);
            } else if (a == 2) {
                gp = std::make_unique<GuitarPro2>(score, version);
            } else if (a == 3) {
                gp = std::make_unique<GuitarPro3>(score, version);
            } else if (a == 4) {
                gp = std::make_unique<GuitarPro4>(score, version);
            } else if (a == 5) {
                gp = std::make_unique<GuitarPro5>(score, version);
            } else {
                LOGD("unknown gtp format %d", version);
                return Err::FILE_BAD_FORMAT;
            }
            read_result = gp->read(io);
        } else {
            return Err::FILE_BAD_FORMAT;
        }

        if (!read_result) {
            LOGD("guitar pro import error====");
            return Err::FILE_BAD_FORMAT;
        }
        gp->set_tempo(0, 0);

        if (!is_version_above6) {
            add_meta_info(score, gp.get(), experimental);
        }

        int idx = 0;

        for (Measure *m1 = score->first_measure(); m1; m1 = m1->next_measure(), ++idx) {
            if (idx >= gp->bars.size()) {
                break;
            }
            const GpBar &bar = gp->bars[idx];
            if (bar.bar_line != BarLineType::NORMAL && bar.bar_line != BarLineType::END_REPEAT &&
                bar.bar_line != BarLineType::START_REPEAT && bar.bar_line != BarLineType::END_START_REPEAT) {
                m1->set_end_bar_line_type(bar.bar_line, 0);
            }
        }
        if (score->last_measure() && score->last_measure()->end_bar_line_type() != BarLineType::NORMAL) {
            score->last_measure()->set_end_bar_line_type(BarLineType::END, false);
        }

        for (Part *part : score->parts()) {
            for (const auto &pair : part->instruments()) {
                pair.second->update_instrument_id();
            }
        }

        score->update_ticks_and_time_sig_map();
        for (Part *p : score->parts()) {
            p->update_harmony_channels(false);
        }

        return Err::NO_ERROR;
    } catch (const std::exception &e) {
        LOGD("guitar pro import failed: %s", e.what());
        return Err::FILE_BAD_FORMAT;
    }
}
} // namespace mu::iex::guitarpro
