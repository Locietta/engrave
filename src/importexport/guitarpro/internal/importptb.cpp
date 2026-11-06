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
#include "importptb.h"

#include <cmath>
#include <stdexcept>

#include "assert.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/bend.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/palmmute.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/types/symid.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
bool PowerTab::read_boolean() { return read_u_int8() != 0; }

uint8_t PowerTab::read_u_int8() {
    uint8_t byte;
    if (file->read((uint8_t *) &byte, 1) != 1) {
        throw std::runtime_error("Unexpected end of PowerTab file");
    }
    return byte;
}

unsigned short PowerTab::read_short() {
    uint16_t val;
    if (file->read((uint8_t *) &val, 2) != 2) {
        throw std::runtime_error("Unexpected end of PowerTab file");
    }
    return val;
}

char PowerTab::read_char() {
    char byte;
    if (file->read((uint8_t *) &byte, 1) != 1) {
        throw std::runtime_error("Unexpected end of PowerTab file");
    }
    return byte;
}

int PowerTab::read_int() {
    int32_t val;
    if (file->read((uint8_t *) &val, 4) != 4) {
        throw std::runtime_error("Unexpected end of PowerTab file");
    }
    return val;
}

std::string PowerTab::read_string(int length) {
    if (length == 0) {
        LOGE() << "reading string of length zero";
        return std::string();
    }

    if (length == -1) {
        length = read_u_int8();
        if (length == 0xFF) {
            length = read_short();
        }
    }
    if (length < 0 || static_cast<size_t>(length) > file->size() - file->pos()) {
        throw std::runtime_error("Invalid PowerTab string length");
    }

    std::string s(static_cast<size_t>(length), '\0');
    if (length > 0 && file->read((uint8_t *) s.data(), static_cast<size_t>(length)) != static_cast<size_t>(length)) {
        throw std::runtime_error("Unexpected end of PowerTab file");
    }
    return s;
}

bool PowerTab::read_version() {
    std::string version = read_string(4);
    version += std::string("-") + std::to_string(read_short());
    return version == "ptab-4";
}

void PowerTab::skip(int len /* = 1 */) {
    for (int i = 0; i < len; ++i) {
        read_char();
    }
}

void PowerTab::read_song_info(PtSongInfo &info) {
    auto classification = read_char();
    info.classification = classification;
    if (classification == 0) {
        skip(1);
        info.name = read_string();
        info.interpret = read_string();

        auto release_type = read_char();

        switch (release_type) {
            case 0: {
                /*auto albumType =*/
                read_char();
                info.album = read_string();
                info.year = read_short();
                info.liverecording = read_char() != 0;
                break;
            }
            case 1: {
                info.album = read_string();
                info.liverecording = read_char() != 0;
                break;
            }
            case 2: {
                info.album = read_string();
                info.day = read_short();
                info.month = read_short();
                info.year = read_short();
                break;
            }
            case 3: {
                break;
            }
            default: throw std::runtime_error("Invalid PowerTab release type");
        }

        if (read_char() == 0) {
            info.author = read_string();
            info.lyricist = read_string();
        }

        info.arranger = read_string();
        info.guitar_transcriber = read_string();
        info.bass_transcriber = read_string();
        info.copyright = read_string();

        info.lyrics = read_string();

        info.guitar_instructions = read_string();
        info.bass_instructions = read_string();
    } else if (classification == 1) {
        info.name = read_string();
        info.album = read_string();
        info.style = read_short();
        info.level = read_u_int8();
        info.author = read_string();
        info.instructions = read_string();
        info.copyright = read_string();
    }
}

int PowerTab::read_header_items() {
    int item_count = read_short();
    if (item_count != 0) {
        int header = read_short();
        if (header == 0xFFFF) {
            if (read_short() != 1) {
                return -1;
            }
            auto str = read_string(read_short()); //section title
        }
    }
    return item_count;
}

void PowerTab::read_track_info(PtTrack &info) {
    TrackInfo tr;

    tr.number = read_u_int8();
    tr.name = read_string();
    tr.instrument = read_u_int8();
    tr.volume = read_u_int8();
    tr.balance = read_u_int8();
    tr.reverb = read_u_int8();
    tr.chorus = read_u_int8();
    tr.tremolo = read_u_int8();
    tr.phaser = read_u_int8();
    tr.capo = read_u_int8();
    tr.tuning_name = read_string();
    tr.offset = read_u_int8();

    int ln = read_u_int8();
    for (int i = 0; i < ln; ++i) {
        tr.strings.push_back(read_u_int8());
    }
    info.infos.push_back(tr);
}

void PowerTab::read_chord(PtTrack &info) {
    PtChord ch;
    ch.key = read_short(); //chord key
    ch.formula = read_u_int8();
    ch.modification = read_short(); //Chord Modification
    ch.extra = read_u_int8();
    ch.top_fret = read_u_int8();
    auto string_count = read_u_int8();
    for (int i = 0; i < string_count; ++i) {
        ch.frets.push_back(read_u_int8()); // fret
    }
    if (info.diagram_map.find({{ch.key, ch.formula, ch.modification}}) == info.diagram_map.end()) {
        info.diagram_map[{{ch.key, ch.formula, ch.modification}}] = ch;
    } else {
        auto a1 = info.diagram_map[{{ch.key, ch.formula, ch.modification}}];
        auto a2 = ch;

        //??            int k = 1;
    }
}

void PowerTab::read_font_settings() {
    read_string();  //font name
    read_int();     // point size
    read_int();     // weight
    read_boolean(); //italic
    read_boolean(); //underline
    read_boolean(); //strikeout
    read_int();     //color
}

void PowerTab::read_floating_text() {
    read_string(); //text
    //rect :
    read_int(); // left
    read_int(); // top
    read_int(); // right
    read_int(); // bottom

    read_u_int8();
    read_font_settings();
}

void PowerTab::read_dynamic() {
    read_short();
    read_u_int8();
    read_u_int8();
    read_short();
}

void PowerTab::read_key_signature() { read_u_int8(); }

void PowerTab::read_rehearsal_sign(PtSection &sec) {
    auto c = read_char();
    auto str = read_string();
    if (str.size()) {
        sec.part_name = str;
        sec.part_marker = c;
    }
}

void PowerTab::read_chord_text(PtSection &sec) {
    PtChordText cht;
    cht.position = read_u_int8();
    cht.key = read_short();
    cht.formula = read_u_int8();
    cht.formula_mod = read_short();
    cht.extra = read_u_int8();
    //sec.chordTextMap.push_back(cht);
    sec.chord_text_map.insert({cht.position, cht});
}

void PowerTab::read_rhytm_slash(PtSection &sec) {
    StRhytmSlash rs;
    rs.position = read_u_int8();
    auto beaming = read_u_int8();
    auto data = read_int();
    auto duration = (data & 0xE00000) >> 21;
    switch (duration) {
        case 0: rs.duration = 1; break;
        case 1: rs.duration = 2; break;
        case 2: rs.duration = 4; break;
        case 3: rs.duration = 8; break;
        case 4: rs.duration = 16;
    }

    rs.triplet = beaming & (0x20 | 0x40);
    rs.tripletend = beaming & 0x80;

    rs.dotted = data & 0x01;
    rs.double_dotted = data & 0x02;

    rs.is_rest = data & 0x04;

    sec.rhythm.emplace_back(rs);
}

void PowerTab::read_guitar_in(PtTrack &info) {
    PtGuitarIn gin;
    gin.section = read_short();
    gin.staff = read_u_int8() + staff_inc;
    gin.position = read_u_int8();
    gin.rhythm_slash = read_u_int8();
    gin.trackinfo = read_u_int8();
    info.guitar_ins.push_back(gin);
}

void PowerTab::read_tempo_marker(PtTrack &info) {
    auto section = read_short();
    /*auto position =*/read_u_int8();
    auto tempo = read_short();
    /*auto data =*/read_short();
    read_string(); //description

    if (tempo > 0) {
        info.get_section(section).tempo = tempo;
    }
}

void PowerTab::read_section_symbol(PtTrack &info) {
    int section = read_short();
    int position = read_u_int8();
    int data = read_int();

    int end_number = data >> 16;
    info.get_section(section).get_position(position).add_component(new PtSymbol(end_number));
}

void PowerTab::read_time_signature(PtBar *bar) {
    skip(3);
    int data = read_u_int8();
    read_u_int8(); // measure pulses

    bar->numerator = ((data - (data % 8)) / 8) + 1;
    bar->denominator = pow(2, data % 8);
}

void PowerTab::read_bar_line(PtSection &sec) {
    auto bar = new PtBar();
    /*int position =*/read_u_int8();
    auto b_type = read_u_int8();

    bar->repeat_start = (b_type >> 5) == 3;
    bar->repeat_close = (b_type >> 5) == 4 ? b_type - 128 : 0;

    bar->measure_no = sec.number;

    read_key_signature();
    read_time_signature(bar);
    read_rehearsal_sign(sec);
    //sec.getPosition(position).addComponent(bar);
    sec.bars.push_back(std::shared_ptr<PtBar>(bar));
    /*if (bars.empty() || ( bars.back()->measureNo < bar->measureNo ))
          bars.push_back(bar);*/
}

void PowerTab::read_staff(int staff, PtSection &sec) {
    skip(5);
    for (int voice = 0; voice < 2; ++voice) {
        int item_count = read_header_items();
        for (int i = 0; i < item_count; ++i) {
            read_position(staff, voice, sec);
            if (i < item_count - 1) {
                read_short();
            }
        }
    }
}

void PowerTab::read_note(PtBeat *beat) {
    PtNote note;
    auto position = read_u_int8();
    auto simple_data = read_short();
    auto symbol_count = read_u_int8();
    for (int i = 0; i < symbol_count; ++i) {
        skip(2);
        auto data3 = read_u_int8();
        auto data4 = read_u_int8();
        note.bend = data4 == 101 ? data3 / 16 + 1 : 0;
        note.slide = data4 == 100 ? data3 + 1 : 0;
    }
    note.value = position & 0x1F;
    note.str = ((position & 0xE0) >> 5);
    note.tied = simple_data & 0x01;
    note.dead = simple_data & 0x02;
    note.hammer = simple_data & 0x08;
    beat->notes.emplace_back(note);
}

void PowerTab::read_position(int staff, int voice, PtSection &sec) {
    auto position = read_u_int8();

    PtBeat beat = PtBeat();
    beat.position = position;
    beat.voice = voice;
    bool add = false;
    if (voice == 0 || static_cast<size_t>(staff) >= sec.beats.size() || sec.beats[staff].empty()) {
        beat = PtBeat(staff, voice);
        beat.position = position;
        add = true;
    }
    auto beaming = read_u_int8();
    beaming = (((int) beaming - 128 < 0) ? beaming : beaming - 128);

    read_u_int8();

    auto data1 = read_u_int8();
    auto data2 = read_u_int8(); //32 - palm mute, 4 - accent, 2 - staccato
    auto data3 = read_u_int8();
    auto duration_value = read_u_int8();

    int multi_bar_rest = 1;
    auto complex_count = read_u_int8();
    for (int i = 0; i < complex_count; ++i) {
        auto count = read_short();
        read_u_int8();
        auto type = read_u_int8();
        if (type & 0x08) {
            multi_bar_rest = count;
        }
    }

    auto item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_note(&beat);
        if (i < item_count - 1) {
            read_short();
        }
    }

    if (staff < 0 || staff >= static_cast<int>(cur_track->infos.size())) {
        LOGE() << "Invalid staff index";
        return;
    }
    cur_track->infos[staff].notes_count += item_count;

    beat.mmrest = (item_count == 0) ? multi_bar_rest : 1;
    beat.vibrato = (((data1 & 0x08) != 0) || ((data1 & 0x10) != 0));
    beat.grace = (data3 & 0x01) != 0;
    beat.tuplet = (data3 & 0x20) != 0;

    if (beat.duration != 0) {
        duration_value = std::max((int) duration_value, beat.duration);
    }

    beat.duration = duration_value;
    beat.dotted = (data1 & 0x01);
    beat.double_dotted = (data1 & 0x02);
    beat.arpeggio_up = (data1 & 0x20);
    beat.arpeggio_down = (data1 & 0x40);
    beat.enters = ((beaming - (beaming % 8)) / 8) + 1;
    beat.times = (beaming % 8) + 1;
    beat.is_rest_value = (data1 & 0x04);
    beat.palm_mute = data2 & 0x20;
    beat.accent = data2 & 0x04;
    beat.staccato = data2 & 0x02;

    for (int i = int(sec.beats.size()); i < staff + 1; ++i) {
        sec.beats.push_back({});
    }
    if (add) {
        sec.beats[staff].push_back(beat);
    }
    //sec.getPosition(position).addComponent(beat);
}

std::vector<int> PowerTab::get_staff_map(PtSection &sec) {
    std::vector<int> result;
    std::vector<int> slash;
    if (!staff_inc && cur_track->guitar_ins.size()) {
        auto first = cur_track->guitar_ins.front();
        while (first.section == sec.number) {
            if (first.trackinfo) {
                for (unsigned int i = 0; i < cur_track->infos.size(); ++i) {
                    if ((1 << i) & first.trackinfo) {
                        result.push_back(i);
                    }
                }
            }

            if (first.rhythm_slash) {
                for (unsigned int i = 0; i < cur_track->infos.size(); ++i) {
                    if ((i << 1) & first.rhythm_slash) {
                        slash.push_back(-1 - i);
                    }
                }
            }

            cur_track->guitar_ins.pop_front();
            if (cur_track->guitar_ins.empty()) {
                break;
            }
            first = cur_track->guitar_ins.front();
        }
    }

    if (result.empty() || int(result.size()) < sec.staves) {
        result.clear();
        if (int(last_staff_map.size()) < sec.staves) {
            for (int i = 0; i < sec.staves; ++i) {
                result.push_back(i + staff_inc);
            }
        } else {
            result = last_staff_map;
        }
    }

    // negative numbers
    //    for (auto i : slash) {
    //        result.push_back(i);
    //    }

    last_staff_map = result;
    return result;
}

//---------------------------------------------------------
//   addPalmMate
//---------------------------------------------------------

void PowerTab::add_palm_mute(Chord *chord) {
    track_idx_t track = chord->track();
    while (palm_mutes.size() < track + 1) {
        palm_mutes.push_back(0);
    }

    if (palm_mutes[track]) {
        PalmMute *pm = palm_mutes[track];
        Chord *last_chord = to_chord(pm->end_cr());
        if (last_chord == chord) {
            return;
        }
        //
        // extend the current palm mute or start a new one
        //
        Fraction tick = chord->segment()->tick();
        if (pm->tick2() < tick) {
            palm_mutes[track] = 0;
        } else {
            pm->set_tick2(chord->end_tick());
            pm->set_end_element(chord);
        }
    }
    if (!palm_mutes[track]) {
        PalmMute *pm = new PalmMute(score->dummy());
        palm_mutes[track] = pm;
        Segment *segment = chord->segment();
        Fraction tick = segment->tick();

        pm->set_tick(tick);
        pm->set_tick2(tick + chord->actual_ticks());
        pm->set_track(track);
        pm->set_track2(track);
        pm->set_start_element(chord);
        pm->set_end_element(chord);
        score->add_element(pm);
    }
}

void PowerTab::fill_measure(tBeatList &elist, Measure *measure, int staff, std::vector<Note *> &tied_notes) {
    Tuplet *tuple = nullptr;
    int tuple_beat_counter{0};
    Fraction tick = measure->tick();
    Fraction endtick = measure->end_tick();
    Chord *hammer{nullptr};

    while (elist.size() && tick < endtick) {
        auto &beat = elist.front();
        auto segment = measure->get_segment(SegmentType::CHORD_REST, tick);
        Fraction l(1, beat.duration);
        int dots = beat.dotted ? (beat.double_dotted ? 2 : 1) : (beat.double_dotted ? 2 : 0);
        switch (dots) {
            case 1: l = l + (l * Fraction(1, 2)); break;
            case 2: l = l + (l * Fraction(3, 4)); break;
        }

        TDuration d(l);
        d.set_dots(dots);

        if (beat.tuplet || tuple_beat_counter) {
            Fraction nt = (l * Fraction(1, 3) * Fraction(2, 1));
            tick += nt;
        } else {
            tick += l;
        }
        ChordRest *cr;
        if (beat.notes.empty()) {
            auto rest = Factory::create_rest(segment);
            cr = rest;
            rest->set_track(staff * ::mu::engraving::voices);
            rest->set_ticks(l);
            rest->set_duration_type(d);
            segment->add(rest);
        } else {
            Chord *chord = Factory::create_chord(segment);
            cr = chord;
            chord->set_track(staff * ::mu::engraving::voices);
            chord->set_ticks(l);
            chord->set_duration_type(d);
            segment->add(chord);

            if (beat.palm_mute) {
                add_palm_mute(chord);
            }
            if (beat.accent) {
                auto accent = Factory::create_articulation(chord);
                accent->set_sym_id(SymId::ARTIC_ACCENT_ABOVE);
                chord->add(accent);
            }
            if (beat.staccato) {
                auto st = Factory::create_articulation(chord);
                st->set_sym_id(SymId::ARTIC_STACCATO_ABOVE);
                chord->add(st);
            }
            bool has_hammer = false;
            for (const auto &n : beat.notes) {
                if (n.str < 0 || static_cast<size_t>(n.str) >= tied_notes.size()) {
                    LOGE() << "string index out of range, skipping note";
                    continue;
                }

                const std::vector<int> &track_strings = cur_track->infos[staff].strings;
                if (track_strings.empty()) {
                    LOGE() << "track has no strings, skipping note";
                    continue;
                }

                const StringData *sd = score->staff(staff)->part()->instrument()->string_data();
                const int k = std::max(int(track_strings.size()) - n.str - 1, 0);
                if (!sd || static_cast<size_t>(k) >= sd->string_list().size()) {
                    LOGE() << "string data missing for note pitch";
                    continue;
                }

                auto note = Factory::create_note(chord);
                chord->add(note);
                if (n.dead) {
                    note->set_head_group(NoteHeadGroup::HEAD_CROSS);
                    note->set_ghost(true);
                }

                if (n.hammer) {
                    has_hammer = true;
                }

                if (n.tied && tied_notes[n.str]) {
                    Tie *tie = Factory::create_tie(tied_notes[n.str]);
                    tie->set_end_note(note);
                    tied_notes[n.str]->add(tie);
                }

                if (n.bend) {
                    Bend *bend = Factory::create_bend(note);
                    //TODO-ws                              bend->setNote(note);
                    bend->points().push_back(PitchValue(0, n.bend * 25 - 12));
                    bend->points().push_back(PitchValue(50, 0));
                    note->add(bend);
                }

                if (false && n.slide) {
                    Text *st = Factory::create_text(chord->notes().front(), TextStyleType::HARMONY_A);
                    st->set_xml_text(String(u"SLIDE %1").arg(n.slide));
                    st->set_track(staff * ::mu::engraving::voices);
                    chord->notes().front()->add(st);
                }

                tied_notes[n.str] = note;
                note->set_fret(n.value);
                note->set_string(n.str);
                int pitch = sd->string_list().at(k).pitch + n.value; //getPitch(n.str, n.value, 0);
                note->set_pitch(pitch);
                note->set_tpc_from_pitch();
            }

            if (chord->notes().empty()) {
                // Every note of the beat was skipped (the track carries no string data, or
                // the note's string index is out of range).  A chord is expected to hold at
                // least one note: layout reads its first note, so an empty one must not
                // reach the score.  Keep the beat's position and duration as a rest.
                LOGE() << "beat has no importable notes, using a rest";
                segment->remove(chord);
                delete chord;
                auto rest = Factory::create_rest(segment);
                cr = rest;
                rest->set_track(staff * ::mu::engraving::voices);
                rest->set_ticks(l);
                rest->set_duration_type(d);
                segment->add(rest);
            } else {
                if (hammer) {
                    auto cr1 = hammer;
                    auto cr2 = chord;
                    hammer = nullptr;

                    Slur *slur = Factory::create_slur(score->dummy());
                    slur->set_start_element(cr1);
                    slur->set_end_element(cr2);
                    slur->set_tick(cr1->tick());
                    slur->set_tick2(tick);
                    slur->set_track(staff * ::mu::engraving::voices);
                    slur->set_track2(staff * ::mu::engraving::voices);
                    score->add_element(slur);

                    Text *st = Factory::create_text(chord->notes().front(), TextStyleType::HARMONY_A);
                    st->set_xml_text(u"H");
                    st->set_track(staff * ::mu::engraving::voices);
                    cr1->notes().front()->add(st);
                }
                if (has_hammer) {
                    hammer = chord;
                }
            }
        }

        if (tuple_beat_counter && tuple) {
            tuple_beat_counter--;
            cr->set_tuplet(tuple);
            tuple->add(cr);
        }

        if (beat.tuplet && !tuple) {
            tuple = Factory::create_tuplet(measure);
            tuple->set_track(cr->track());
            tuple->set_tick(cr->tick());
            tuple->set_base_len(l);
            tuple->set_ratio(Fraction(3, 2));
            tuple->set_ticks(l * tuple->ratio().denominator());
            cr->set_tuplet(tuple);
            tuple->add(cr);
            tuple_beat_counter = 2;
        }
        elist.pop_front();
    }

    if (tick == measure->tick()) {
        Segment *seg = measure->get_segment(SegmentType::CHORD_REST, tick);
        auto rest = Factory::create_rest(seg);
        rest->set_track(staff * ::mu::engraving::voices);
        auto ts = measure->timesig();
        rest->set_ticks(ts);
        rest->set_duration_type(TDuration(ts));
        seg->add(rest);
    }
}

void PowerTab::add_to_score(PtSection &sec) {
    cur_section = &sec;
    Fraction tick = score->last_measure() ? score->last_measure()->end_tick() : Fraction(0, 1);

    Fraction last_ts(-1, -1);
    bool first_measure = true;
    if (score->last_measure()) {
        last_ts = score->last_measure()->timesig();
        first_measure = false;
    }
    if (first_measure) {
        //int lastStaff = sec.staffMap.back() + 1;
        for (int i = 0; i < staves; ++i) {
            Part *part = new Part(score);
            Staff *s = Factory::create_staff(part);
            auto info = &cur_track->infos[i];
            std::string ss = info->name;
            String staff_name;
            if (muse::UtfCodec::is_valid_utf8(ss)) {
                staff_name = muse::String::from_std_string(ss);
            } else {
                // invalid utf most likely windows-125x used
                std::string n = "part " + std::to_string(i + 1);
                staff_name = muse::String::from_std_string(n);
            }
            part->set_plain_long_name(staff_name);

            std::vector<int> reverse_str;
            for (auto it = info->strings.rbegin(); it != info->strings.rend(); ++it) {
                reverse_str.push_back(*it);
            }
            StringData string_data(32, int(info->strings.size()), reverse_str.data());
            part->instrument()->set_string_data(string_data);

            part->set_midi_program(info->instrument);
            part->set_midi_channel(info->number);

            score->append_staff(s);
            score->append_part(part);
        }
    }
    auto bar1 = sec.bars.front();
    while (bar1->denominator == 0) {
        if (sec.bars.size() == 1) {
            break;
        }
        sec.bars.pop_front();
        bar1 = sec.bars.front();
    }
    if (bar1->denominator == 0) {
        bar1->denominator = 4;
        bar1->numerator = 4;
    }
    auto measure = create_measure(bar1.get(), tick);
    if (repeat_count) {
        measure->set_repeat_end(true);
        measure->set_repeat_count(repeat_count);
    }
    repeat_count = bar1->repeat_close;
    if (bar1->repeat_start) {
        measure->set_repeat_start(true);
    }
    if (sec.bars.size() > 1) {
        sec.bars.pop_front();
    }
    if (sec.tempo) {
        Segment *segment = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
        TempoText *tt = new TempoText(segment);
        tt->set_tempo(double(sec.tempo) / 60.0f);
        tt->set_xml_text(String(u"<sym>metNoteQuarterUp</sym> = %1").arg(sec.tempo));
        tt->set_track(0);
        segment->add(tt);
    }
    if (!sec.part_name.empty() && last_part != sec.part_marker) {
        last_part = sec.part_marker;
        auto seg = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
        RehearsalMark *t = new RehearsalMark(seg);
        t->set_frame_type(FrameType::RECTANGLE);
        t->set_plain_text(String(Char::from_ascii(sec.part_marker)));
        t->set_track(0);
        seg->add(t);

        t = new RehearsalMark(seg);
        t->set_frame_type(FrameType::NO_FRAME);
        std::string valid;
        muse::UtfCodec::replace_invalid(sec.part_name, valid);
        t->set_plain_text(String::from_std_string(valid));
        t->set_offset(PointF(10.0, 0.0));
        t->set_track(0);
        seg->add(t);
    }
    if (first_measure) {
        for (int staff_idx = 0; staff_idx < staves; ++staff_idx) {
            int keysig = staff_idx >= staff_inc ? 0 : 1; // Can be parsed int beat section
            Segment *s = measure->get_segment(SegmentType::KEY_SIG, tick);
            KeySig *t = Factory::create_key_sig(s);
            t->set_key(Key(keysig));
            t->set_track(staff_idx * ::mu::engraving::voices);
            s->add(t);

            ClefType clef_id = staff_idx >= staff_inc ? ClefType::F8_VB : ClefType::G15_MB;
            s = measure->get_segment(SegmentType::HEADER_CLEF, Fraction(0, 1));
            auto clef = Factory::create_clef(s);
            clef->set_track(staff_idx * ::mu::engraving::voices);
            clef->set_clef_type(clef_id);
            s->add(clef);
        }
    }

    std::vector<std::vector<Note *>> tied_notes(staves);
    for (auto &nvec : tied_notes) {
        nvec.resize(10);
        memset(nvec.data(), 0, sizeof(Note *) * 10);
    }

    if (sec.beats.size() > static_cast<size_t>(staves)) {
        LOGE() << "Found more beat lists than staves, some beat lists will be skipped";
    }

    //std::vector<tBeatList>
    while (true) {
        bool empty = true;
        while (int(sec.beats.size()) < staves) {
            sec.beats.push_back({});
        }
        for (unsigned int i = 0; i < sec.beats.size() && i < tied_notes.size(); ++i) {
            fill_measure(sec.beats[i], measure, i, tied_notes[i]);
            if (sec.beats[i].size() && i < unsigned(staff_inc)) {
                empty = false;
            }
        }
        if (last_ts != measure->timesig()) {
            last_ts = measure->timesig();
            for (int staff_idx = 0; staff_idx < staves; ++staff_idx) {
                Staff *staff = score->staff(staff_idx);
                auto staff_type = staff->staff_type(Fraction(0, 1));
                if (staff_type->gen_timesig()) {
                    Segment *s = measure->get_segment(SegmentType::TIME_SIG, measure->tick());
                    auto t = Factory::create_time_sig(s);
                    t->set_track(staff_idx * ::mu::engraving::voices);
                    t->set_sig(last_ts);
                    s->add(t);
                }
            }
        }
        if (empty) {
            break;
        }
        auto bar = sec.bars.front();
        while (bar->denominator == 0) {
            if (sec.bars.size() == 1) {
                break;
            }
            sec.bars.pop_front();
            bar = sec.bars.front();
        }
        if (bar->denominator == 0) {
            bar->denominator = measure->timesig().denominator();
            bar->numerator = measure->timesig().numerator();
        }
        measure = create_measure(bar.get(), measure->end_tick());
        if (repeat_count) {
            measure->set_repeat_end(true);
            measure->set_repeat_count(repeat_count);
        }
        repeat_count = bar->repeat_close;
        if (bar->repeat_start) {
            measure->set_repeat_start(true);
        }
        if (sec.bars.size() > 2) {
            sec.bars.pop_front();
        }
    }
    if (sec.bars.size()) {
        auto bar = sec.bars.back();
        if (bar->repeat_start) {
            measure->set_repeat_start(true);
        }
        if (bar->repeat_close) {
            measure->set_repeat_end(true);
            measure->set_repeat_count(bar->repeat_close);
        }
        sec.bars.clear();
    }
}

void PowerTab::PtSection::copy_tracks(PtTrack *track) {
    //if not found GuitarIn in section or all tracks are read -> return
    if (staves == int(staff_map.size())) {
        return;
    }

    auto signature = chord_text_map.begin();
    for (unsigned index = 0; index < staff_map.size(); ++index) {
        auto staff = (staff_map[index] + 1) * -1;
        if (staff < 0) {
            continue;
        }

        for (const auto &rt : rhythm) {
            auto new_sig = chord_text_map.find(rt.position);
            if (new_sig != chord_text_map.end()) {
                signature = new_sig;
            }

            PtBeat beat = PtBeat(staff, 0);
            beat.position = rt.position;
            beat.duration = rt.duration;
            beat.dotted = rt.dotted;
            beat.double_dotted = rt.double_dotted;
            beat.is_rest_value = rt.is_rest;
            beat.tuplet = rt.triplet;
            if (!rt.is_rest) {
                auto diagram = track->diagram_map.find({{signature->second.key, signature->second.formula, signature->second.formula_mod}});

                if (diagram != track->diagram_map.end()) {
                    for (unsigned int string = 0; string < diagram->second.frets.size(); ++string) {
                        int fret = diagram->second.frets[string];
                        if (fret >= 0xFE) {
                            continue;
                        }
                        PtNote note;
                        note.value = fret;
                        note.str = string;
                        if (fret == 0xFE) {
                            note.dead = true;
                            note.value = 0;
                        }
                        beat.notes.emplace_back(note);
                    }
                } else {
                    LOGD() << "diagram wasn't found";
                }
            }
            while (int(beats.size()) <= staff) {
                beats.push_back({});
            }
            beats[staff].push_back(beat);
        }
    }
}

void PowerTab::read_section(PtSection &sec) {
    //rect:
    read_int(); // left
    read_int(); // top
    read_int(); // right
    read_int(); // bottom

    auto last_bar_data = read_u_int8();

    skip(4); //spacing from staff

    read_bar_line(sec);

    int item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_direction(sec);
        if (i < item_count - 1) {
            read_short();
        }
    }
    //ChordText section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_chord_text(sec);
        if (i < item_count - 1) {
            read_short();
        }
    }
    //RhytmSlash
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_rhytm_slash(sec);
        if (i < item_count - 1) {
            read_short();
        }
    }
    // Staff
    sec.staves = read_header_items();
    sec.staff_map = get_staff_map(sec);
    for (int i = 0; i < sec.staves; ++i) {
        int staff = sec.staff_map[i];
        read_staff(staff, sec);
        if (i < sec.staves - 1) {
            read_short();
        }
    }
    sec.copy_tracks(cur_track);
    // MusicBar section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_bar_line(sec);
        if (i < item_count - 1) {
            read_short();
        }
    }

    auto bar = new PtBar();
    bar->repeat_close = (last_bar_data >> 5 == 4) ? last_bar_data - 128 : 0;
    sec.bars.push_back(std::shared_ptr<PtBar>(bar));
    //sec.getPosition(sec.getNextPositionNumber()).addComponent(bar);
}

void PowerTab::read_direction(PtSection &sec) {
    int position = read_u_int8();
    int symbol_count = read_u_int8();
    for (int i = 0; i < symbol_count; ++i) {
        unsigned int data = read_short();
        sec.get_position(position).add_component(new PtDirection((data >> 8), ((data & 0xC0) >> 6), (data & 0x1F)));
    }
}

void PowerTab::read_data_instruments(PtTrack &info) {
    cur_track = &info;
    //Guitar section
    auto item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_track_info(info);
        if (i < item_count - 1) {
            read_short();
        }
    }
    // Chord Diagram Section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_chord(info);
        if (i < item_count - 1) {
            read_short();
        }
    }
    //Floating Text section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_floating_text();
        if (i < item_count - 1) {
            read_short();
        }
    }
    // GuitarIn section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_guitar_in(info);
        if (i < item_count - 1) {
            read_short();
        }
    }
    // Tempo marker
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_tempo_marker(info);
        if (i < item_count - 1) {
            read_short();
        }
    }
    //Dynamic section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_dynamic();
        if (i < item_count - 1) {
            read_short();
        }
    }
    //Symbol section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_section_symbol(info);
        if (i < item_count - 1) {
            read_short();
        }
    }
    //Section section
    item_count = read_header_items();
    for (int i = 0; i < item_count; ++i) {
        read_section(info.get_section(i));
        if (i < item_count - 1) {
            read_short();
        }
    }
}

std::string cr_ts(int strings, int tuning[]) {
    const static char *tune[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    std::vector<int> pitch;
    for (int i = 0; i < strings; ++i) {
        pitch.push_back(tuning[i]);
    }
    std::string t;
    for (auto i : pitch) {
        t += tune[i % 12];
        t += " ";
    }
    return t;
}

Measure *PowerTab::create_measure(PtBar *bar, const Fraction &tick) {
    auto measure = Factory::create_measure(score);
    Fraction nts(bar->numerator, bar->denominator);

    measure->set_tick(tick);
    measure->set_timesig(nts);
    measure->set_ticks(nts);

    score->measures()->append(measure);

    return measure;
}

PowerTab::PtSection &PowerTab::PtTrack::get_section(int ind) {
    for (int i = (int) sections.size(); i < ind + 1; ++i) {
        sections.push_back(PtSection(i));
    }
    return sections[ind];
}

PowerTab::PtSection::PtSection(int num) { number = num; }

PowerTab::PtPosition &PowerTab::PtSection::get_position(int pos) {
    unsigned int i = 0;
    while (i < positions.size()) {
        auto &p = positions[i];
        if (p.position == pos) {
            return p;
        }
        ++i;
    }
    PtPosition p;
    p.position = pos;
    positions.emplace_back(p);
    return positions.back();
}

int PowerTab::PtSection::get_next_position_number() {
    int next = 0;
    unsigned int k = 0;
    while (k < positions.size()) {
        auto p = positions[k];
        next = std::max(next, p.position + 1);
        ++k;
    }
    return next;
}

void PowerTab::PtPosition::add_component(PtComponent *c) { components.push_back(std::shared_ptr<PtComponent>(c)); }

//---------------------------------------------------------
//   read
//---------------------------------------------------------

Err PowerTab::read() {
    try {
        if (!read_version()) {
            return Err::FILE_BAD_FORMAT;
        }
        PtSong song;

        read_song_info(song.info);
        read_data_instruments(song.track1);
        staff_inc = int(song.track1.infos.size());
        last_staff_map.clear();
        read_data_instruments(song.track1);

        staves = int(song.track1.infos.size());

        std::vector<tBeatList> parts(staves);
        for (int i = staff_inc; i < staves; ++i) {
            for (auto &sec : song.track1.sections) {
                while (int(sec.beats.size()) < staves) {
                    sec.beats.push_back({});
                }
                parts[i].insert(parts[i].end(), sec.beats[i].begin(), sec.beats[i].end());
                sec.beats[i].clear();
            }
        }

        for (auto &sec : song.track1.sections) {
            for (int i = 0; i < staves; ++i) {
                if (parts[i].size()) {
                    sec.beats[i].insert(sec.beats[i].begin(), parts[i].begin(), parts[i].end());
                    parts[i].clear();
                }
            }
            add_to_score(sec);
            for (int i = 0; i < staves; ++i) {
                parts[i] = sec.beats[i];
            }
        }

        MeasureBase *m;
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
        // create title
        std::string name = song.info.name;
        if (!name.empty()) {
            Text *s = Factory::create_text(m, TextStyleType::TITLE);
            std::string valid;
            muse::UtfCodec::replace_invalid(name, valid);
            s->set_plain_text(String::from_std_string(valid));
            m->add(s);
        }
        return Err::NO_ERROR;
    } catch (const std::exception &e) {
        LOGE() << "PowerTab import failed: " << e.what();
        return Err::FILE_BAD_FORMAT;
    }
}
} // namespace mu::iex::guitarpro
