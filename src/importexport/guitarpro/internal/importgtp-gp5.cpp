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

#include "containers.h"

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/bracket.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/note.h"
#include "engraving/dom/notedot.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/stringdata.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/types/symid.h"
#include "engraving/dom/stringtunings.h"
#include "engraving/dom/capo.h"

#include "engraving/editing/editchord.h"

#include "guitarprodrumset.h"
#include "utils.h"

#include "log.h"

using namespace muse::io;
using namespace mu::engraving;

namespace mu::iex::guitarpro {
static TremoloType tremolo_type(int division) {
    static const std::map<int, TremoloType> types{{1, TremoloType::R8}, {2, TremoloType::R16}, {3, TremoloType::R32}};

    auto it = types.find(division);
    if (it != types.end()) {
        return it->second;
    }

    LOGE() << "wrong tremolo type";
    return TremoloType::INVALID_TREMOLO;
}

static std::unordered_map<uint64_t, mu::engraving::StringData> string_datas;
//---------------------------------------------------------
//   readInfo
//---------------------------------------------------------

void GuitarPro5::read_info() {
    title = read_delphi_string();
    subtitle = read_delphi_string();
    artist = read_delphi_string();
    album = read_delphi_string();
    composer = read_delphi_string();
    poet = read_delphi_string();
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
//   readBeatEffects
//---------------------------------------------------------

int GuitarPro5::read_beat_effects(int track, Segment *segment) {
    int effects = 0;

    uint8_t fx_bits1 = read_u_int8();
    uint8_t fx_bits2 = read_u_int8();
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_fade) {
        effects = 4; // fade in
    }
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_effect) {
        int k = read_u_int8();
        effects = k + effects * 100; // &effects;
    }
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_vibrato_tremolo) {
        effects = 7 + effects * 100;
    }
    if (fx_bits2 & ::mu::iex::guitarpro::GuitarPro::beat_tremolo) {
        read_tremolo_bar(track, segment); // readBend();
    }
    if (fx_bits2 & ::mu::iex::guitarpro::GuitarPro::beat_rasgueado) {
        current_beat_has_rasgueado_ = true;
    }
    if (fx_bits1 & ::mu::iex::guitarpro::GuitarPro::beat_arpeggio) {
        int strokeup = read_u_int8();   // up stroke length
        int strokedown = read_u_int8(); // down stroke length

        Arpeggio *a = Factory::create_arpeggio(score->dummy());
        // representation is different in guitar pro 5 - the up/down order below is correct
        if (strokeup > 0) {
            a->set_arpeggio_type(ArpeggioType::DOWN_STRAIGHT);
            if (strokeup < 7) {
                a->set_stretch(1.0 / std::pow(2, 6 - strokeup));
            }
        } else if (strokedown > 0) {
            a->set_arpeggio_type(ArpeggioType::UP_STRAIGHT);
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
        effects = read_char(); // stroke pick direction
        effects += 4;          //1 or 2 for effects becomes 4 or 5
    }
    return effects;
}

//---------------------------------------------------------
//   readPageSetup
//---------------------------------------------------------

void GuitarPro5::read_page_setup() {
    skip(version > 500 ? 49 : 30);
    for (int i = 0; i < 11; ++i) {
        skip(4);
        read_byte_pascal_string();
    }
}

//---------------------------------------------------------
//   readBeat
//---------------------------------------------------------

Fraction GuitarPro5::read_beat(const Fraction &tick, int voice, Measure *measure, int staff_idx, Tuplet **tuplets, bool /*mixChange*/) {
    uint8_t beat_bits = read_u_int8();
    bool dotted = beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_dotted;
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

    current_beat_has_rasgueado_ = false;

    slide = -1;
    int track = staff_idx * ::mu::engraving::voices + voice;
    if (muse::contains(slides, track)) {
        slide = muse::take(slides, track);
    }

    int pause = -1;
    if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_pause) {
        pause = read_u_int8();
    }

    // readDuration
    int len = read_char();
    int tuple = 0;
    if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_tuplet) {
        tuple = read_int();
    }

    Segment *segment = measure->get_segment(SegmentType::CHORD_REST, tick);
    if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_chord) {
        size_t num_strings = score->staff(staff_idx)->part()->instrument()->string_data()->strings();
        skip(17);
        String name;
        {
            uint8_t chars_count = read_u_int8();
            char c[21];
            GuitarPro::read(c, 21);
            // Just in case
            if (chars_count <= 20) {
                c[chars_count] = '\0';
            } else {
                c[20] = '\0';
            }
            name = String::from_utf8(c);
        }
        skip(4);
        // no header to be read in the GP5 format - default to true.
        read_chord(segment, staff_idx * ::mu::engraving::voices, static_cast<int>(num_strings), name, true);
        skip(32);
    }
    Lyrics *lyrics = 0;
    String free_text;
    if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_lyrics) {
        /// it is not beat lirics, it is free text
        free_text = read_delphi_string();
    }

    int beat_effects = 0;
    if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_effects_value) {
        beat_effects = read_beat_effects(track, segment);
    }

    last_segment = segment;
    if (beat_bits & ::mu::iex::guitarpro::GuitarPro::beat_mix_change) {
        read_mix_change(measure);
    }

    int strings = read_u_int8(); // used strings mask

    Fraction l = len2fraction(len);

    // Some beat effects could add a Chord before this
    ChordRest *cr = segment->cr(track);
    if (voice != 0 && pause == 0 && strings == 0) {
        cr = 0;
    } else {
        if (strings == 0) {
            if (cr) {
                segment->remove(cr);
                delete cr;
                cr = 0;
            }
            cr = Factory::create_rest(segment);
        } else {
            if (!cr) {
                cr = Factory::create_chord(segment);
            }
        }
        cr->set_track(track);

        TDuration d(l);
        d.set_dots(dotted ? 1 : 0);

        if (dotted) {
            l = l + (l * Fraction(1, 2));
        }

        if (tuple) {
            int track2 = staff_idx * 2 + voice;
            Tuplet *tuplet = tuplets[track2];
            if ((tuplet == nullptr) || (tuplet->elements_duration() == tuplet->base_len().fraction() * tuplet->ratio().numerator())) {
                tuplet = Factory::create_tuplet(measure);
                tuplets[track2] = tuplet;
                set_tuplet(tuplet, tuple);
            }
            tuplet->set_track(cr->track());
            tuplet->set_base_len(l);
            tuplet->set_tick(tick);
            tuplet->set_ticks(l * tuplet->ratio().denominator());
            cr->set_tuplet(tuplet);
            tuplet->add(cr);
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

        Note *note_value = nullptr;
        std::vector<Note *> delnote;
        for (int i = 6; i >= 0; --i) {
            if (strings & (1 << i) && ((6 - i) < static_cast<int>(num_strings))) {
                Note *note = Factory::create_note(to_chord(cr));
                note_value = note;
                if (dotted) {
                    // there is at most one dotted note in this guitar pro version
                    NoteDot *dot = Factory::create_note_dot(note);
                    dot->set_track(track); // needed to know the staff it belongs to (and detect tablature)
                    dot->set_visible(true);
                    note->add(dot);
                }
                to_chord(cr)->add(note);

                ReadNoteResult read_result = read_note(6 - i, note);
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
                if (!slide_list.empty() && slide_list.back() == nullptr) {
                    slide_list.back() = note;
                    has_slur = true;
                }
                if (note->fret() == -20) {
                    delnote.push_back(note);
                } else {
                    note->set_tpc_from_pitch(Prefer::SHARPS);
                }
            }
        }
        if (!delnote.empty()) {
            Chord *chord = to_chord(cr);
            for (auto n : delnote) {
                chord->remove(n);
                delete n;
            }
            if (chord->notes().empty()) {
                if (chord->tuplet()) {
                    chord->tuplet()->remove(chord);
                }
                chord->segment()->remove(chord);
                delete chord;
                cr = nullptr;
            }
            delnote.clear();
        }
        if (cr && cr->is_chord()) {
            Chord *chord = to_chord(cr);
            chord->sort_notes();
            mu::iex::guitarpro::utils::create_ghost_note_paren_groups(chord);
        }
        create_slur(has_slur, staff_idx, cr);
        if (lyrics) {
            cr->add(lyrics);
        }
        if (free_text.size() && note_value) {
            add_text_to_note(free_text, note_value);
        }
    }

    int rr = read_char();
    if (cr && cr->is_chord()) {
        Chord *chord = to_chord(cr);

        bool has_vibrato_left_hand_on_beat = false;
        bool has_vibrato_w_trem_bar_on_beat = false;

        do {
            apply_beat_effects(chord, beat_effects % 100, has_vibrato_left_hand_on_beat, has_vibrato_w_trem_bar_on_beat);
            has_vibrato_left_hand = has_vibrato_left_hand || has_vibrato_left_hand_on_beat;
            has_vibrato_w_trem_bar = has_vibrato_w_trem_bar || has_vibrato_w_trem_bar_on_beat;
        } while (beat_effects /= 100);
        if (rr == ::mu::iex::guitarpro::GuitarPro::arpeggio_down) {
            chord->set_stem_direction(DirectionV::DOWN);
        } else if (rr == ::mu::iex::guitarpro::GuitarPro::arpeggio_up) {
            chord->set_stem_direction(DirectionV::UP);
        }
    }

    if (cr) {
        //  fixing gp5 bug with not storying let ring for tied notes
        if (has_let_ring) {
            let_ring_for_chords_.insert(cr);
        }

        add_let_ring(cr, has_let_ring);
        add_palm_mute(cr, has_palm_mute);
        add_trill(cr, has_trill);
        add_hammer_on_pull_off(cr, has_hammer_on_pull_off);
        add_rasgueado(cr, current_beat_has_rasgueado_);
        add_vibrato_left_hand(cr, has_vibrato_left_hand);
        add_vibrato_w_trem_bar(cr, has_vibrato_w_trem_bar);
        add_harmonic_marks(cr, has_harmonic_artificial, has_harmonic_pinch, has_harmonic_tap, has_harmonic_semi);
    }

    int r = read_char();
    if (r & 0x8) {
        int rrr = read_char();
        LOGD("  3beat read 0x%02x", rrr);
    }
    if (cr && cr->is_chord()) {
        if (to_chord(cr)->notes().size() == 0) {
            segment->remove(cr);
            delete cr;
            cr = 0;
        } else if (slide > 0) {
            create_slide(slide, cr, staff_idx);
        }
    }
    rests_for_empty_beats(segment, measure, cr, l, track, tick);
    return cr ? cr->actual_ticks() : measure->ticks();
}

//---------------------------------------------------------
//   readMeasure
//---------------------------------------------------------

void GuitarPro5::read_measure(Measure *measure, int staff_idx, Tuplet **tuplets, bool mix_change) {
    for (int voice = 0; voice < 2; ++voice) {
        Fraction measure_len = {0, 1};
        Fraction tick = measure->tick();
        int beats = read_int();
        if (beats > 100) {
            return;
        }
        for (int beat = 0; beat < beats; ++beat) {
            Fraction ticks = read_beat(tick, voice, measure, staff_idx, tuplets, mix_change);
            ++beat_counter_value;
            tick += ticks;
            measure_len += ticks;
        }
        if (measure_len < measure->ticks()) {
            score->set_rest(tick, staff_idx * ::mu::engraving::voices + voice, measure->ticks() - measure_len, false, nullptr, false);
            continious_elements_builder_->notify_uncompleted_measure();
        }
    }
}

//---------------------------------------------------------
//   readMixChange
//---------------------------------------------------------

bool GuitarPro5::read_mix_change(Measure *measure) {
    /*char patch   =*/
    read_char();
    skip(16);
    signed char volume = read_char();
    signed char pan = read_char();
    signed char chorus = read_char();
    signed char reverb = read_char();
    signed char phase = read_char();
    signed char tremolo = read_char();
    read_delphi_string(); // tempo name

    int temp = read_int();
    bool edited_tempo = false;
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
                    tt->set_tempo(BeatsPerSecond::from_bpm(temp));
                    tt->set_visible(false);
                    tt->set_track(0);
                    last_segment->add(tt);
                }
            }
            previous_tempo = temp;
            set_tempo(temp, measure);
            edited_tempo = true;
        }
        last_segment = nullptr;
        read_char();
        if (version > 500) {
            read_char();
        }
    }
    read_char();
    read_char();
    if (version > 500) {
        read_delphi_string();
        read_delphi_string();
    }
    return edited_tempo;
}

//---------------------------------------------------------
//   readTracks
//---------------------------------------------------------

bool GuitarPro5::read_tracks() {
    string_datas.clear();
    for (size_t i = 0; i < staves; ++i) {
        int tuning[::mu::iex::guitarpro::gp_max_string_number];
        Staff *staff = score->staff(i);
        Part *part = staff->part();

        uint8_t c = read_u_int8(); // simulations bitmask
        if (c & 0x2) {             // 12 stringed guitar
        }
        if (c & 0x4) { // banjo track
        }
        if (i == 0 || version == 500) {
            skip(1);
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
        /*int midiChannel2 =*/read_int(); // -1

        int frets = read_int();
        int capo = read_int();
        /*int color        =*/read_int();

        skip(version > 500 ? 49 : 44);
        if (version > 500) {
            //  british stack clean / amp tone
            read_delphi_string();
            read_delphi_string();
        }
        std::vector<int> tuning2(strings);
        //int tuning2[strings];
        for (int k = 0; k < strings; ++k) {
            tuning2[strings - k - 1] = tuning[k];
        }
        bool use_flats = create_tuning_string(strings, &tuning2[0]);
        StringData string_data(frets, strings, &tuning2[0], use_flats);
        Instrument *instr = part->instrument();
        instr->set_string_data(string_data);
        instr->set_single_note_dynamics(false);
        part->set_plain_long_name(name);
        string_datas.insert_or_assign(part->id().to_uint64(), string_data);

        //
        // determine clef
        //
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
    skip(version == 500 ? 2 : 1);

    return true;
}

//---------------------------------------------------------
//   readMeasures
//---------------------------------------------------------

void GuitarPro5::read_measures(int /*startingTempo*/) {
    Measure *measure = score->first_measure();
    bool mix_change = false;
    for (size_t bar = 0; bar < measures; ++bar, measure = measure->next_measure()) {
        const GpBar &gpbar = bars[bar];

        if (!gpbar.marker.is_empty()) {
            Segment *segment = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
            RehearsalMark *s = Factory::create_rehearsal_mark(segment);
            s->set_type(RehearsalMark::Type::ADDITIONAL);
            s->set_plain_text(gpbar.marker.trimmed());
            s->set_track(0);
            segment->add(s);
        }

        std::vector<Tuplet *> tuplets(staves * 2);
        //Tuplet* tuplets[staves * 2];     // two voices
        for (size_t track = 0; track < staves * 2; ++track) {
            tuplets[track] = 0;
        }

        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            beat_counter_value = 0;
            read_measure(measure, static_cast<int>(staff_idx), &tuplets[0], mix_change);
            if (!(((bar == (measures - 1)) && (staff_idx == (staves - 1))))) {
                /*int a = */
                read_char();
            }
        }
        if (bar == 1 && !mix_change) {
            set_tempo(tempo, score->first_measure());
        }
    }

    if (!gp_lyrics.segments.empty()) {
        size_t size = std::min(gp_lyrics.segments.size(), gp_lyrics.lyrics.size());
        for (size_t i = 0; i < size; ++i) {
            std::string str = gp_lyrics.lyrics[i].to_std_string();
            auto seg = gp_lyrics.segments[i];
            auto mes = seg->measure();
            while (str.size() && seg && seg->segment_type() == SegmentType::CHORD_REST) {
                auto cr = seg->cr(gp_lyrics.lyric_track);
                if (cr) {
                    if (str[0] != '-') {
                        Lyrics *lyr = Factory::create_lyrics(cr);

                        std::string text;
                        auto pos = str.find('-');
                        auto pos2 = str.find('\n');
                        if (pos2 < pos) {
                            pos = pos2;
                        }
                        if (pos != std::string::npos) {
                            const char *c = &str.c_str()[pos + 1];
                            if (*c == 0) {
                                pos = std::string::npos;
                                text = str;
                            } else {
                                text = str.substr(0, pos);
                                str = str.substr(pos + 1);
                            }
                        } else {
                            text = str;
                        }
                        if (pos == std::string::npos) {
                            str.resize(0);
                        }
                        lyr->set_plain_text(String::from_std_string(text));
                        cr->add(lyr);
                    } else {
                        str = str.substr(1);
                    }
                }
                seg = seg->next();
                if (!seg) {
                    mes = mes->next_measure();
                    if (!mes) {
                        break;
                    }
                    seg = mes->first();
                }
            }
        }
    } else if (gp_lyrics.lyric_track != 0) {
        size_t counter = 0;
        size_t index = 0;
        gp_lyrics.lyric_track -= 1;
        Measure *mes = score->first_measure();
        if (!mes) {
            return;
        }

        Segment *beg = mes->first();

        do {
            if (beg->segment_type() == SegmentType::CHORD_REST && beg->cr(gp_lyrics.lyric_track)) {
                ChordRest *cr = beg->cr(gp_lyrics.lyric_track);
                assert(cr);
                ++counter;
                if (!cr->is_chord()) {
                    continue;
                }

                bool is_tied = false;
                Chord *chord = to_chord(cr);
                for (Note *n : chord->notes()) {
                    const auto &tied_notes = n->tied_notes();

                    if (tied_notes.size() > 1 && tied_notes.front() != n) {
                        is_tied = true;
                        break;
                    }
                }

                if (is_tied) {
                    continue;
                }

                if (counter >= gp_lyrics.from_beat) {
                    if (gp_lyrics.lyric_pos.size() == index) {
                        gp_lyrics.lyric_pos.push_back(0);
                    }

                    String &lyrics_in_index = gp_lyrics.lyrics[index];
                    if (lyrics_in_index[0] != u'-') {
                        Lyrics *lyr = mu::engraving::Factory::create_lyrics(cr);

                        String text;
                        size_t pos = lyrics_in_index.index_of(u'-', gp_lyrics.lyric_pos[index]);
                        size_t pos2 = lyrics_in_index.index_of(u'\n', gp_lyrics.lyric_pos[index]);
                        if (pos2 < pos) {
                            pos = pos2;
                        }

                        if (pos != std::string::npos && (pos + 1 < lyrics_in_index.size())) {
                            const char16_t *c = &lyrics_in_index[pos + 1];
                            if (*c == 0) {
                                pos = std::string::npos;
                                text = lyrics_in_index;
                            } else {
                                text = lyrics_in_index.mid(gp_lyrics.lyric_pos[index], pos + 1 - gp_lyrics.lyric_pos[index]);
                                gp_lyrics.lyric_pos[index] = pos + 1;
                            }
                        } else {
                            text = lyrics_in_index.mid(gp_lyrics.lyric_pos[index]);
                        }

                        if (pos == std::string::npos) {
                            ++index;
                        }

                        lyr->set_plain_text(text);
                        cr->add(lyr);
                        if (index >= gp_lyrics.lyrics.size()) {
                            break;
                        }
                    } else if (lyrics_in_index.size() >= 2) {
                        lyrics_in_index = lyrics_in_index.mid(1);
                    }
                }
            }
        } while ((beg = beg->next()) ||
                 (mes->next() && mes->next()->is_measure() && (mes = to_measure(mes->next())) && (beg = mes->first())));
    }
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool GuitarPro5::read(IODevice *io) {
    continious_elements_builder_ = std::make_unique<ContiniousElementsBuilder>(score);
    guitar_bend_importer_ = std::make_unique<GuitarBendImporter>(score);

    f = io;

    read_info();
    read_lyrics();
    read_page_setup();

    previous_tempo = -1;

    tempo = read_int();
    if (version > 500) {
        skip(1);
    }

    key = read_int();
    /* int octave =*/read_char(); // octave

    read_channels();

    std::vector<unsigned int> articulations;
    articulations.resize(19);
    {
        unsigned int r;
        unsigned char x;
        for (int i = 0; i < 19; ++i) {
            x = read_u_int8();
            r = x;
            x = read_u_int8();
            r += x << 8;

            articulations[i] = r;
        }
    }

    //skip(42);
    skip(4);

    measures = read_int();
    staves = read_int();
    if (measures == 0 || measures > 100000) {
        return false;
    }
    init_dynamics(staves);

    for (size_t str = 0; str < 7; ++str) {
        for (size_t staff = 0; staff < staves; ++staff) {
            dead_end[{static_cast<int>(staff), static_cast<int>(str)}] = true;
        }
    }

    slurs.resize(staves, nullptr);

    int tnumerator = 4;
    int tdenominator = 4;
    for (size_t i = 0; i < measures; ++i) {
        if (i > 0) {
            skip(1);
        }
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
            bar.repeats = read_u_int8();
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_marker) {
            bar.marker = read_delphi_string(); // new section?
            /*int color =*/read_int();         // color?
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_volta) { // a volta
            uint8_t volta_number = read_u_int8();
            while (volta_number > 0) {
                // voltas are represented as flags
                bar.volta.volta_type = ::mu::iex::guitarpro::gp_volta_flags;
                bar.volta.volta_info.push_back(volta_number & 1);
                volta_number >>= 1;
            }
            bar.repeats = static_cast<int>(bar.volta.volta_info.size()) + 1;
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_keysig) {
            int current_key = read_u_int8();
            /* key signatures are specified as
             * 1# = 1, 2# = 2, ..., 7# = 7
             * 1b = 255, 2b = 254, ... 7b = 249 */
            bar.keysig = current_key <= 7 ? current_key : -256 + current_key;
            read_u_int8(); // specified major/minor mode
        }
        if (bar_bits & ::mu::iex::guitarpro::GuitarPro::score_double_bar) {
            bar.bar_line = BarLineType::DOUBLE;
        }
        if (bar_bits & 0x3) {
            skip(4);
        }
        if ((bar_bits & 0x10) == 0) {
            skip(1);
        }

        read_char(); // triple feel  (none, 8, 16)
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
    if (!read_tracks()) {
        return false;
    }
    read_measures(tempo);
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
                return false;
            }

            if (string_datas.find(p->id().to_uint64()) == string_datas.end()) {
                continue;
            }
            StringData sd = string_datas.at(p->id().to_uint64());

            std::vector<int> tuning_pitches(sd.strings());
            for (size_t i = 0; i < tuning_pitches.size(); ++i) {
                tuning_pitches[i] = sd.string_list().at(i).pitch + p->instrument()->transpose().chromatic;
            }

            if (utils::is_standard_tuning(p->instrument()->channel(0)->program(), tuning_pitches)) {
                continue;
            }

            StringTunings *tun = Factory::create_string_tunings(seg);
            tun->set_string_data(sd);
            tun->set_track(staff2track(s->idx()));
            seg->add(tun);

            auto tuning = utils::standard_tuning_for(p->instrument()->channel(0)->program(), (int) sd.strings());
            sd = StringData(sd.frets(), (int) tuning.size(), tuning.data());
            p->instrument()->set_string_data(sd);
        }
    }
    for (auto n : slide_list) {
        auto segment = n->chord()->segment();
        auto measure = segment->measure();
        while ((segment = segment->next1(SegmentType::CHORD_REST)) ||
               ((measure = measure->next_measure()) && (segment = measure->first()))) {
            if (segment->segment_type() != SegmentType::CHORD_REST) {
                continue;
            }
            bool br = false;
            ChordRest *cr = to_chord_rest(segment->cr(n->track()));
            if (cr && cr->is_chord()) {
                Chord *c = to_chord(cr);
                for (auto nt : c->notes()) {
                    if (nt->string() == n->string()) {
                        for (auto e : nt->el()) {
                            if (e->is_chord_line()) {
                                ChordLine *cl = to_chord_line(e);
                                if (cl->chord_line_type() == ChordLineType::PLOP || cl->chord_line_type() == ChordLineType::SCOOP) {
                                    br = true;
                                    break;
                                }
                            }
                        }
                        if (br) {
                            break;
                        }

                        glissando_note_pairs_.push_back({n, nt});
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

    std::map<int, int> counter;
    for (int i = 0; i < 19; ++i) {
        if (articulations[i] != 0xFFFF) {
            Measure *measure = to_measure(score->measure(articulations[i] - 1));
            if (i < 4) {
                Segment *segment = measure->get_segment(SegmentType::BAR_LINE, measure->tick());
                Symbol *sym = new Symbol(measure);
                if (i == 0) {
                    sym->set_sym(SymId::CODA);
                } else if (i == 1) {
                    sym->set_sym(SymId::CODA_SQUARE);
                } else if (i == 2) {
                    sym->set_sym(SymId::SEGNO);
                } else {
                    sym->set_sym(SymId::SEGNO_SERPENT2);
                }

                sym->set_track(0);
                segment->add(sym);
                auto iter = counter.find(articulations[i]);
                if (iter == counter.end()) {
                    counter[articulations[i]] = 1;
                } else {
                    counter[articulations[i]] += 1;
                }
            } else {
                Segment *s = measure->get_segment(SegmentType::KEY_SIG, measure->tick());
                StaffText *st = new StaffText(s);
                static constexpr char text[][22] = {"fine",           "Da Capo",         "D.C. al Coda",   "D.C. al Doppia Coda",
                                                    "D.C. al Fine",   "Da Segno",        "D.S. al Coda",   "D.S. al Doppia Coda",
                                                    "D.S. al Fine",   "Da Doppio Segno", "D.D.S. al Coda", "D.D.S. al Doppia Coda",
                                                    "D.D.S. al Fine", "Da Coda",         "Da Doppia Coda"};
                st->set_plain_text(String::from_ascii(text[i - 4]));
                st->set_track(0);
                auto iter = counter.find(articulations[i]);
                if (iter == counter.end()) {
                    counter[articulations[i]] = 1;
                } else {
                    counter[articulations[i]] += 1;
                }
                score->add_element(st);
            }
        }
    }

    continious_elements_builder_->add_elements_to_score();
    guitar_bend_importer_->add_elements_to_score();
    add_glissandos();
    utils::add_play_count_texts(score);

    return true;
}

//---------------------------------------------------------
//   readNoteEffects
//---------------------------------------------------------

GuitarPro::ReadNoteResult GuitarPro5::read_note_effects(Note *note) {
    ReadNoteResult result;
    uint8_t mod_mask1 = read_u_int8();
    uint8_t mod_mask2 = read_u_int8();
    std::vector<PitchValue> bend_data;
    Note *bend_parent = nullptr;

    if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_bend) {
        bend_data = read_bend_data_from_file();
        bend_parent = note;
    }
    if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_hammer) {
        result.hammer_on_pull_off = true;
    }
    if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_let_ring) {
        result.let_ring = true;
    }

    if (mod_mask1 & ::mu::iex::guitarpro::GuitarPro::effect_grace) {
        int fret = read_u_int8();       // grace fret
        /*int dynamic =*/read_u_int8(); // grace dynamic
        int transition = read_u_int8(); // grace transition
        int duration = read_u_int8();   // grace duration
        int gflags = read_u_int8();

        int grace_len = Constants::division / 8;
        if (duration == 2) {
            grace_len = Constants::division / 6; //24th
        } else if (duration == 3) {
            grace_len = Constants::division / 4; //16th
        }
        NoteType note_type = NoteType::ACCIACCATURA;

        if (gflags & ::mu::iex::guitarpro::GuitarPro::note_appogiatura) { //on beat
            note_type = NoteType::APPOGGIATURA;
        }

        int grace_pitch = note->staff()->part()->instrument()->string_data()->get_pitch(note->string(), fret, nullptr);

        auto gnote = score->set_grace_note(note->chord(), grace_pitch, note_type, grace_len);

        // gp5 not supports more than one grace note,
        // so it's always should be shown as eight note
        gnote->chord()->set_duration_type(Fraction{1, 8});

        gnote->set_string(note->string());
        gnote->set_fret(fret);
        if (transition == 0) {
            // no transition
        } else if (transition == 1) {
        } else if (transition == 3) {
            Slur *slur1 = Factory::create_slur(score->dummy());
            slur1->set_start_element(gnote->chord());
            slur1->set_end_element(note->chord());
            slur1->set_ownership_parent(0);
            slur1->set_track(note->staff_idx());
            slur1->set_track2(note->staff_idx());
            slur1->set_tick(gnote->chord()->tick());
            slur1->set_tick2(note->chord()->tick());
            score->add_element(slur1);
            //TODO: Add a 'slide' guitar effect when implemented
            //note->setSlideNote(gn);
        }
    }
    if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_staccato) {
        Chord *chord = note->chord();
        Articulation *a = Factory::create_articulation(chord);
        a->set_sym_id(SymId::ARTIC_STACCATO_ABOVE);
        bool add = true;
        for (auto a1 : chord->articulations()) {
            if (a1->sym_id() == SymId::ARTIC_STACCATO_ABOVE) {
                add = false;
                break;
            }
        }
        if (add) {
            chord->add(a);
        }
    }
    if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_palm_mute) {
        result.palm_mute = true;
    }

    if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_tremolo) { // tremolo picking length
        int tremolo_division = read_u_int8();
        if (tremolo_division >= 1 && tremolo_division <= 3) {
            TremoloType type = tremolo_type(tremolo_division);
            DO_ASSERT(!is_tremolo_two_chord(type));
            Chord *chord = note->chord();
            TremoloSingleChord *t = Factory::create_tremolo_single_chord(chord);
            t->set_tremolo_type(type);
            chord->add(t);
            tremolos_in_chords_[chord] = type;
        } else {
            LOGD("Unknown tremolo value");
        }
    }
    if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_slide) {
        int slide_kind = read_u_int8();
        ChordLine *sld = nullptr;
        ChordLineType slide_type = ChordLineType::NOTYPE;

        if (slide_kind & ::mu::iex::guitarpro::GuitarPro::slide_out_down) {
            slide_kind &= ~::mu::iex::guitarpro::GuitarPro::slide_out_down;
            sld = Factory::create_chord_line(score->dummy());
            slide_type = ChordLineType::FALL;
        }
        // slide out upwards (doit)
        if (slide_kind & ::mu::iex::guitarpro::GuitarPro::slide_out_up) {
            slide_kind &= ~::mu::iex::guitarpro::GuitarPro::slide_out_up;
            sld = Factory::create_chord_line(score->dummy());
            slide_type = ChordLineType::DOIT;
        }
        // slide in from below (plop)
        if (slide_kind & ::mu::iex::guitarpro::GuitarPro::slide_in_below) {
            slide_kind &= ~::mu::iex::guitarpro::GuitarPro::slide_in_below;
            sld = Factory::create_chord_line(score->dummy());
            slide_type = ChordLineType::PLOP;
        }
        // slide in from above (scoop)
        if (slide_kind & ::mu::iex::guitarpro::GuitarPro::slide_in_above) {
            slide_kind &= ~::mu::iex::guitarpro::GuitarPro::slide_in_above;
            sld = Factory::create_chord_line(score->dummy());
            slide_type = ChordLineType::SCOOP;
        }

        if (sld) {
            sld->set_chord_line_type(slide_type);
            sld->set_straight(true);
            note->chord()->add(sld);
            sld->set_note(note);
        }

        if (slide_kind & ::mu::iex::guitarpro::GuitarPro::legato_slide) {
            slide_kind &= ~::mu::iex::guitarpro::GuitarPro::legato_slide;
            slide_list.push_back(nullptr);
            create_slur(true, note->staff_idx(), note->chord());
        }

        if (slide_kind & ::mu::iex::guitarpro::GuitarPro::shift_slide) {
            slide_kind &= ~::mu::iex::guitarpro::GuitarPro::shift_slide;
            slide_list.push_back(note);
        }
    }

    if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_artificial_harmonic) {
        int type = read_char();
        if (type == ::mu::iex::guitarpro::GuitarPro::harmonic_mark_natural) {
            int fret = note->fret();

            note->set_harmonic(true);
            float harmonic_fret = natural_harmonic_from_fret(fret);
            int harmonic_overtone = utils::harmonic_overtone(note, harmonic_fret, type);
            note->set_display_fret(Note::DisplayFretOption::NATURAL_HARMONIC);
            note->set_harmonic_fret(harmonic_fret);
            auto staff = note->staff();
            int pitch = staff->part()->instrument()->string_data()->get_pitch(note->string(), harmonic_overtone, staff, note->tick());

            note->set_pitch(clamp_pitch(pitch));
            note->set_tpc_from_pitch(Prefer::SHARPS);
        } else if (type >= ::mu::iex::guitarpro::GuitarPro::harmonic_mark_artificial &&
                   type <= ::mu::iex::guitarpro::GuitarPro::harmonic_mark_semi) {
            int fret = (type == ::mu::iex::guitarpro::GuitarPro::harmonic_mark_tap ? (read_char() - note->fret()) : note->fret());
            float midi = note->pitch() + fret;
            int current_octave = floor(midi / 12);
            auto harm_note = (type == ::mu::iex::guitarpro::GuitarPro::harmonic_mark_artificial ? read_char() : 0);
            if (type == ::mu::iex::guitarpro::GuitarPro::harmonic_mark_artificial) {
                read_char();
            }

            auto octave = current_octave + (type == ::mu::iex::guitarpro::GuitarPro::harmonic_mark_artificial ? read_char() : 0);

            Note *harmonic_note = Factory::create_note(note->chord());

            harmonic_note->set_harmonic(true);
            harmonic_note->set_play(false);
            note->set_play(true);
            /// @note option to show or not additional harmonic fret in "<>" to be implemented
            ///harmonicNote->setDisplayFret(Note::DisplayFretOption::ArtificialHarmonic);
            ///note->setDisplayFret(Note::DisplayFretOption::Hide);
            harmonic_note->set_display_fret(Note::DisplayFretOption::HIDE);
            harmonic_notes_[note] = harmonic_note;

            Staff *staff = note->staff();
            float harmonic_fret = 0;
            switch (static_cast<int>(((octave * 12) + harm_note) - midi)) {
                case 4: harmonic_fret = 12; break;
                case 11: harmonic_fret = 7; break;
                case 16: harmonic_fret = 5; break;
                case 20: harmonic_fret = 4; break;
                case 23: harmonic_fret = 3.2f; break;
                case 25: harmonic_fret = 2.7f; break;
                case 28: harmonic_fret = 2.4f; break;
                default: harmonic_fret = 12; break;
            }

            int overtone_fret = utils::harmonic_overtone(note, harmonic_fret, type);
            harmonic_note->set_string(note->string());
            harmonic_note->set_fret(note->fret());
            harmonic_note->set_harmonic_fret(harmonic_fret + fret);

            int pitch = staff->part()->instrument()->string_data()->get_pitch(note->string(), overtone_fret, staff, note->tick());

            harmonic_note->set_pitch(clamp_pitch(pitch));
            harmonic_note->set_tpc_from_pitch(Prefer::SHARPS);
            note->set_harmonic_pitch_offset(harmonic_note->pitch() - note->pitch());
            note->chord()->add(harmonic_note);

            switch (type) {
                case ::mu::iex::guitarpro::GuitarPro::harmonic_mark_artificial: result.harmonic_artificial = true; break;
                case ::mu::iex::guitarpro::GuitarPro::harmonic_mark_pinch: result.harmonic_pinch = true; break;
                case ::mu::iex::guitarpro::GuitarPro::harmonic_mark_tap: result.harmonic_tap = true; break;
                case ::mu::iex::guitarpro::GuitarPro::harmonic_mark_semi: result.harmonic_semi = true; break;
            }
        }
    }

    if (bend_parent) {
        create_bend(bend_parent, bend_data);
    }

    if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_vibrato) {
        result.vibrato = true;
    }

    if (mod_mask2 & ::mu::iex::guitarpro::GuitarPro::effect_trill) {
        read_u_int8(); // trill fret

        int period = read_u_int8(); // trill length
        result.trill = true;

        switch (period) {
            case 1: // 16
                break;
            case 2: // 32
                break;
            case 3: // 64
                break;
            default: LOGD("unknown trill period %d", period); break;
        }
    }
    return result;
}

//---------------------------------------------------------
//   readNote
//---------------------------------------------------------

GuitarPro::ReadNoteResult GuitarPro5::read_note(int string, Note *note) {
    uint8_t note_bits = read_u_int8();
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

    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_ghost) {
        note->set_ghost(true);
    }

    bool tie_note = false;

    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_dead) {
        uint8_t note_type_value = read_u_int8();
        if (note_type_value == 1) {
        } //standard note
        else if (note_type_value == 2) {
            tie_note = true;
        } else if (note_type_value == 3) { // dead notes
            note->set_head_group(NoteHeadGroup::HEAD_CROSS);
            note->set_dead_note(true);
        } else {
            LOGD("unknown note type: %d", note_type_value);
        }
    }

    int &previous_dynamic = previous_dynamic_by_track[note->track()];

    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_dynamic) { // velocity
        int d = read_char();
        if (previous_dynamic != d) {
            previous_dynamic = d;
            add_dynamic(note, d);
        }
    } else if (previous_dynamic != ::mu::iex::guitarpro::GuitarPro::default_dynamic) {
        previous_dynamic = ::mu::iex::guitarpro::GuitarPro::default_dynamic;
        add_dynamic(note, previous_dynamic);
    }

    int fret_number = 0;
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_fret) {
        fret_number = read_char();
    }

    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_fingering) {
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

    if (note_bits & 0x1) { // Time independent duration
        skip(8);
    }

    // check if a note is supposed to be accented, and give it the marcato type
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_marcato) {
        Articulation *art = Factory::create_articulation(note->score()->dummy());
        art->set_sym_id(SymId::ARTIC_MARCATO_ABOVE);
        if (!EditChord::toggle_articulation(note->score(), note, art)) {
            delete art;
        }
    }
    // check if a note is supposed to be accented, and give it the sforzato type
    else if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_sforzato) {
        Articulation *art = Factory::create_articulation(note->score()->dummy());
        art->set_sym_id(SymId::ARTIC_ACCENT_ABOVE);
        note->add(art);
        if (!EditChord::toggle_articulation(note->score(), note, art)) {
            delete art;
        }
    }

    read_u_int8(); //skip

    Staff *staff = note->staff();
    if (fret_number == 255 || fret_number < 0) {
        fret_number = 0;
        note->set_head_group(NoteHeadGroup::HEAD_CROSS);
        note->set_dead_note(true);
    }
    int pitch = staff->part()->instrument()->string_data()->get_pitch(string, fret_number, staff, note->tick());
    note->set_fret(fret_number);
    note->set_string(string);
    note->set_pitch(pitch);

    // This function uses string and fret number, so it should be set before this
    ReadNoteResult result;
    if (note_bits & ::mu::iex::guitarpro::GuitarPro::note_slur) {
        result = read_note_effects(note);
    }

    if (tie_note) {
        auto staff_idx = note->staff_idx();
        if (dead_end[{static_cast<int>(staff_idx), string}]) {
            note->set_fret(::mu::engraving::invalid_fret_index);
            result.slur = false;
            return result;
        }
        if (slurs[staff_idx]) {
            score->remove_spanner(slurs[staff_idx]);
            delete slurs[staff_idx];
            slurs[staff_idx] = 0;
        }
        bool found = false;
        Chord *chord = note->chord();
        Segment *segment = chord->segment()->prev1(SegmentType::CHORD_REST);
        track_idx_t track = note->track();
        std::vector<ChordRest *> chords;
        Note *true_note = nullptr;
        while (segment) {
            EngravingItem *e = segment->element(track);
            if (e && e->is_chord()) {
                Chord *chord2 = to_chord(e);
                for (Note *note2 : chord2->notes()) {
                    if (note2->string() == string) {
                        if (chords.empty()) {
                            Tie *tie = Factory::create_tie(note2);
                            tie->set_end_note(note);
                            note2->add(tie);
                        }

                        //  fixing gp5 bug with not storying let ring for tied notes
                        if (let_ring_for_chords_.find(chord2) != let_ring_for_chords_.end()) {
                            result.let_ring = true;
                            muse::remove(let_ring_for_chords_, chord2);
                        }

                        if (harmonic_notes_.find(note) != harmonic_notes_.end() && harmonic_notes_.find(note2) != harmonic_notes_.end()) {
                            Note *start_harmonic_note = harmonic_notes_.at(note2);
                            Note *end_harmonic_note = harmonic_notes_.at(note);

                            Tie *tie_harmonic = Factory::create_tie(start_harmonic_note);
                            tie_harmonic->set_end_note(end_harmonic_note);
                            start_harmonic_note->add(tie_harmonic);

                            muse::remove(harmonic_notes_, start_harmonic_note);
                            muse::remove(harmonic_notes_, end_harmonic_note);
                        }

                        note->set_fret(note2->fret());
                        note->set_pitch(note2->pitch());
                        true_note = note2;
                        if (tremolos_in_chords_.find(chord2) != tremolos_in_chords_.end()) {
                            TremoloType type = tremolos_in_chords_.at(chord2);
                            DO_ASSERT(!is_tremolo_two_chord(type));
                            TremoloSingleChord *t = Factory::create_tremolo_single_chord(chord);
                            t->set_tremolo_type(type);
                            chord->add(t);
                            muse::remove(tremolos_in_chords_, chord2);
                            tremolos_in_chords_[chord] = type;
                        }

                        found = true;
                        break;
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

        if (true_note && !chords.empty()) {
            Note *end_note = note;
            for (unsigned int i = 0; i < chords.size(); ++i) {
                Chord *chord1 = nullptr;
                ChordRest *cr = chords.at(i);
                if (cr->is_chord()) {
                    chord1 = to_chord(cr);
                } else {
                    Rest *rest = to_rest(cr);
                    Fraction dur = rest->ticks();
                    TDuration dut = rest->duration_type();
                    Segment *seg = rest->segment();
                    seg->remove(rest);
                    Tuplet *tuplet = rest->tuplet();
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
                note2->set_tpc_from_pitch(Prefer::SHARPS);
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
            note->set_fret(::mu::engraving::invalid_fret_index);
            dead_end[{static_cast<int>(staff_idx), string}] = true;
            LOGD("tied note not found, pitch %d fret %d string %d", note->pitch(), note->fret(), note->string());
            result.slur = false;
            return result;
        }
    }
    dead_end[{static_cast<int>(note->staff_idx()), string}] = false;
    return result;
}

float GuitarPro5::natural_harmonic_from_fret(int fret) {
    switch (fret) {
        case 2: return 2.4f;
        case 3: return 3.2f;
        case 4:
        case 5:
        case 7:
        case 9:
        case 12:
        case 16:
        case 17:
        case 19:
        case 24: return fret;
        case 8: return 8.2f;
        case 10: return 9.6f;
        case 14: return 14.7f;
        case 15: return 14.7f;
        case 21: return 21.7f;
        case 22: return 21.7f;
        default: return 12.0f;
    }
}

void GuitarPro5::add_glissandos() {
    for (auto &[start_note, end_note_value] : glissando_note_pairs_) {
        Note *current_start = start_note;
        if (start_note->bend_for()) {
            Note *bend_note = start_note;
            GuitarBend *bend = bend_note->bend_for();

            while (bend) {
                bend_note = bend->end_note();
                IF_ASSERT_FAILED(bend_note) {
                    LOGE() << "glissando start note may be incorrect";
                    break;
                }

                if (!bend_note->chord()->is_grace_after()) {
                    break;
                }

                current_start = bend_note;
                bend = bend_note->bend_for();
            }
        }

        Glissando *gliss = mu::engraving::Factory::create_glissando(current_start);
        gliss->set_start_element(current_start);
        gliss->set_tick(current_start->tick());
        gliss->set_track(current_start->track());
        gliss->set_glissando_type(GlissandoType::STRAIGHT);
        gliss->set_glissando_shift(true);
        gliss->set_end_element(end_note_value);
        gliss->set_tick2(end_note_value->tick());
        gliss->set_track2(end_note_value->track());
        score->add_element(gliss);

        std::vector<std::pair<Slur *, Note *>> moved_slurs;
        for (Spanner *spanner : start_note->chord()->starting_spanners()) {
            if (spanner && spanner->is_slur()) {
                Slur *slur = to_slur(spanner);
                if (slur->end_element() == end_note_value->chord()) {
                    gliss->set_glissando_shift(false);
                    slur->set_connected_element(mu::engraving::Slur::ConnectedElement::GLISSANDO);
                    moved_slurs.push_back({slur, current_start});
                    break;
                }
            }
        }

        for (auto &[slur, note] : moved_slurs) {
            slur->set_start_element(note->chord());
            slur->set_tick(note->tick());
            slur->set_track(note->track());
        }
    }
}
} // namespace mu::iex::guitarpro
