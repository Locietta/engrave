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
#include <cstdint>
#include <exception>
#include <set>
#include <string>
#include <vector>

#include "translation.h"

#include "engraving/engravingerrors.h"
#include "engraving/rw/xmlwriter.h"

#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/box.h"
#include "engraving/dom/bracket.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/key.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/sig.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/utils.h"
#include "engraving/editing/transpose.h"

#include "internal/midishared/generalmidi.h"
#include "../midishared/midifile.h"
#include "importmidi_beat.h"
#include "importmidi_chord.h"
#include "importmidi_chordname.h"
#include "importmidi_clef.h"
#include "importmidi_drum.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "importmidi_instrument.h"
#include "importmidi_key.h"
#include "importmidi_lrhand.h"
#include "importmidi_lyrics.h"
#include "importmidi_meter.h"
#include "importmidi_operations.h"
#include "importmidi_quant.h"
#include "importmidi_simplify.h"
#include "importmidi_swing.h"
#include "importmidi_tempo.h"
#include "importmidi_tie.h"
#include "importmidi_tuplet.h"
#include "importmidi_tuplet_tonotes.h"
#include "importmidi_voice.h"
#include "importmidi.h"

#include "io/buffer.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::midi {
void lengthen_too_short_notes(std::multimap<int, MTrack> &tracks) {
    for (auto &track : tracks) {
        MTrack &mtrack = track.second;
        for (auto &chord : mtrack.chords) {
            for (auto &note : chord.second.notes) {
                if (note.off_time - chord.first < MChord::min_allowed_duration()) {
                    note.off_time = chord.first + MChord::min_allowed_duration();
                }
            }
        }
    }
}

#ifndef NDEBUG

bool do_notes_overlap(const MTrack &track) {
    const auto &chords = track.chords;
    for (auto i1 = chords.begin(); i1 != chords.end(); ++i1) {
        const auto &chord1 = i1->second;
        for (auto note_it1 = chord1.notes.begin(); note_it1 != chord1.notes.end(); ++note_it1) {
            for (auto note_it2 = std::next(note_it1); note_it2 != chord1.notes.end(); ++note_it2) {
                if (note_it2->pitch == note_it1->pitch) {
                    return true;
                }
            }
            for (auto i2 = std::next(i1); i2 != chords.end(); ++i2) {
                if (i2->first >= note_it1->off_time) {
                    break;
                }
                const auto &chord2 = i2->second;
                if (chord1.voice != chord2.voice) {
                    continue;
                }
                for (const auto &note2 : chord2.notes) {
                    if (note2.pitch == note_it1->pitch) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool do_notes_overlap(const std::multimap<int, MTrack> &tracks) {
    bool result = false;
    for (const auto &track : tracks) {
        result = do_notes_overlap(track.second);
    }
    return result;
}

bool no_too_short_notes(const std::multimap<int, MTrack> &tracks) {
    for (const auto &track : tracks) {
        const auto &chords = track.second.chords;
        for (const auto &chord : chords) {
            for (const auto &note : chord.second.notes) {
                if (note.off_time - chord.first < MChord::min_allowed_duration()) {
                    return false;
                }
            }
        }
    }
    return true;
}

#endif

std::vector<std::multimap<ReducedFraction, MidiChord>>
separate_drum_chords_to2_voices(const std::multimap<ReducedFraction, MidiChord> &chords) {
    std::vector<std::multimap<ReducedFraction, MidiChord>> separated_chords(2);
    for (const auto &chord : chords) {
        const MidiChord &c = chord.second;

        Q_ASSERT(c.voice == 0 || c.voice == 1);

        separated_chords[c.voice].insert({chord.first, c});
    }
    return separated_chords;
}

void set_chord_voice(MidiChord &chord, int voice) {
    chord.voice = voice;
    if (chord.is_in_tuplet) {
        chord.tuplet->second.voice = voice;
    }
    for (auto &note : chord.notes) {
        if (note.is_in_tuplet) {
            note.tuplet->second.voice = voice;
        }
    }
}

void find_all_tuplets_for_drums(MTrack &mtrack, TimeSigMap *sigmap, const ReducedFraction &basic_quant) {
    const size_t drum_voice_count = 2;
    // drum track has 2 voices (stem up and stem down),
    // and tuplet detection is applicable for single voice drum tracks,
    // so split track chords into 2 voice groups,
    // detect tuplets and merge chords (and found tuplets) back;
    // it's a small hack due to the fact that tuplet detection
    // is designed to work before voice setting

    std::vector<std::multimap<ReducedFraction, MidiChord>> chords(drum_voice_count);
    for (const auto &chord : mtrack.chords) {
        const MidiChord &c = chord.second;

        Q_ASSERT(c.voice == 0 || c.voice == 1);

        chords[c.voice].insert({chord.first, c});
    }

    std::vector<std::multimap<ReducedFraction, MidiTuplet::TupletData>> tuplets(drum_voice_count);
    for (size_t voice = 0; voice < drum_voice_count; ++voice) {
        if (!chords[voice].empty()) {
            MidiTuplet::find_all_tuplets(tuplets[voice], chords[voice], sigmap, basic_quant);
        }
    }
    mtrack.chords.clear();
    for (size_t voice = 0; voice < drum_voice_count; ++voice) {
        for (auto &chord : chords[voice]) {
            // correct voice because it can be changed during tuplet detection
            set_chord_voice(chord.second, int(voice));
            mtrack.chords.insert({chord.first, chord.second});
        }
    }
    mtrack.update_tuplets_from_chords();
    // note: temporary local tuplets and chords are deleted here
}

void quantize_all_tracks(std::multimap<int, MTrack> &tracks, TimeSigMap *sigmap, const ReducedFraction &last_tick) {
    auto &opers = midi_import_operations;

    for (auto &track : tracks) {
        MTrack &mtrack = track.second;
        if (mtrack.chords.empty()) {
            continue;
        }
        // pass current track index through MidiImportOperations
        // for further usage
        MidiOperations::CurrentTrackSetter set_current_track{opers, mtrack.index_of_operation};

        if (opers.data()->processings_of_opened_file == 0) {
            opers.data()->track_opers.is_drum_track.set_value(opers.current_track(), mtrack.mtrack->drum_track());
            if (mtrack.mtrack->drum_track()) {
                opers.data()->track_opers.max_voice_count.set_value(opers.current_track(), MidiOperations::VoiceCount::V_1);
            }
        }
        const auto basic_quant = Quantize::quant_value_to_fraction(opers.data()->track_opers.quant_value.value(mtrack.index_of_operation));
#ifndef NDEBUG
        Q_ASSERT_X(MChord::is_last_tick_valid(last_tick, mtrack.chords), "quantizeAllTracks", "Last tick is less than max note off time");
#endif
        MChord::set_bar_indexes(mtrack.chords, basic_quant, last_tick, sigmap);

        if (mtrack.mtrack->drum_track()) {
            find_all_tuplets_for_drums(mtrack, sigmap, basic_quant);
        } else {
            MidiTuplet::find_all_tuplets(mtrack.tuplets, mtrack.chords, sigmap, basic_quant);
        }
#ifndef NDEBUG
        Q_ASSERT_X(!do_notes_overlap(track.second), "quantizeAllTracks", "There are overlapping notes of the same voice that is incorrect");
#endif
        // (4/3 of the smallest duration) tol is less sensitive
        // to on time inaccuracies than 1/2 earlier
        MChord::collect_chords(mtrack, {2, 1}, {4, 3});
        Quantize::quantize_chords(mtrack.chords, sigmap, basic_quant);
        MidiTuplet::remove_empty_tuplets(mtrack);
#ifndef NDEBUG
        Q_ASSERT_X(MidiTuplet::are_tuplet_ranges_ok(mtrack.chords, mtrack.tuplets), "quantizeAllTracks",
                   "Tuplet chord/note is outside tuplet "
                   "or non-tuplet chord/note is inside tuplet");
#endif
    }
}

//---------------------------------------------------------
//   processMeta
//---------------------------------------------------------

void MTrack::process_meta(int tick, const MidiEvent &mm) {
    if (!staff) {
        LOGD("processMeta: no staff");
        return;
    }
    const uchar *data = mm.edata();
    Score *cs = staff->score();

    switch (mm.meta_type()) {
        case META_TEXT:
        case META_LYRIC: break; // lyric and text are added in importmidi_lyrics.cpp
        case META_TRACK_NAME: {
            const std::string text = MidiCharset::from_uchar(data);

            auto &opers = midi_import_operations;
            if (opers.data()->processings_of_opened_file == 0) {
                const int current_track = index_of_operation;
                opers.data()->track_opers.staff_name.set_value(current_track, text);
            }

            if (name.is_empty()) {
                name = MidiCharset::convert_to_charset(text);
            }
        } break;
        case META_TEMPO: // add later, after adding of notes
            break;
        case META_KEY_SIGNATURE: {
            const signed char key = ((const signed char *) data)[0];
            if (key < -7 || key > 7) {
                LOGD("ImportMidi: illegal key %d", key);
                break;
            }
            KeySigEvent ke;
            Key t_key = Key(key);
            Key c_key = t_key;
            Fraction t = Fraction::from_ticks(tick);
            Interval v = staff->part()->instrument(t)->transpose();
            if (!v.is_zero() && !cs->style().style_b(Sid::CONCERT_PITCH)) {
                c_key = Transpose::transpose_key(t_key, v);
                // if there are more than 6 accidentals in transposing key, it cannot be PreferSharpFlat::AUTO
                if ((t_key > 6 || t_key < -6) && staff->part()->prefer_sharp_flat() == PreferSharpFlat::AUTO) {
                    staff->part()->set_prefer_sharp_flat(PreferSharpFlat::NONE);
                }
            }
            ke.set_concert_key(c_key);
            ke.set_key(t_key);
            staff->set_key(t, ke);
            has_key = true;
        } break;
        case META_COMPOSER: // mscore extension
        case META_POET:
        case META_TRANSLATOR:
        case META_SUBTITLE:
        case META_TITLE: {
            TextStyleType ssid = TextStyleType::DEFAULT;
            switch (mm.meta_type()) {
                case META_COMPOSER: ssid = TextStyleType::COMPOSER; break;
                case META_TRANSLATOR: ssid = TextStyleType::TRANSLATOR; break;
                case META_POET: ssid = TextStyleType::LYRICIST; break;
                case META_SUBTITLE: ssid = TextStyleType::SUBTITLE; break;
                case META_TITLE: ssid = TextStyleType::TITLE; break;
            }

            MeasureBase *measure = cs->first();
            Text *text = Factory::create_text(measure, ssid);
            text->set_plain_text(String::from_utf8((const char *) (mm.edata())));

            if (!measure->is_v_box()) {
                measure = Factory::create_v_box(cs);
                measure->set_tick(Fraction(0, 1));
                measure->set_next(cs->first());
                cs->measures()->append(measure);
            }
            measure->add(text);
        } break;
        case META_COPYRIGHT: cs->set_meta_tag(u"copyright", muse::String::from_utf8(reinterpret_cast<const char *>(mm.edata()))); break;
        case META_TIME_SIGNATURE: break; // added earlier
        case META_PORT_CHANGE: staff->part()->set_midi_channel(-1, (int) data[0]); break;
        default:
            if (MScore::debug_mode) {
                LOGD("unknown meta type 0x%02x", mm.meta_type());
            }
            break;
    }
}

std::vector<std::pair<ReducedFraction, TDuration>> MTrack::to_duration_list(const Measure *measure, int voice,
                                                                            const ReducedFraction &start_tick, const ReducedFraction &len,
                                                                            Meter::DurationType duration_type) {
    // find tuplets over which duration goes
    auto bar_tick = ReducedFraction(measure->tick());
    auto tuplets_data = MidiTuplet::find_tuplets_in_bar_for_duration(voice, bar_tick, start_tick, len, tuplets);

    struct {
        bool operator()(const MidiTuplet::TupletData &d1, const MidiTuplet::TupletData &d2) { return d1.len > d2.len; }
    } comparator;

    // sort by tuplet length in desc order
    sort(tuplets_data.begin(), tuplets_data.end(), comparator);

    const ReducedFraction start_tick_in_bar = start_tick - bar_tick;
    const ReducedFraction end_tick_in_bar = start_tick_in_bar + len;

    const auto &opers = midi_import_operations;
    const bool use_dots = opers.data()->track_opers.use_dots.value(index_of_operation);
    return Meter::to_duration_list(start_tick_in_bar, end_tick_in_bar, ReducedFraction(measure->timesig()), tuplets_data, duration_type,
                                   use_dots);
}

ReducedFraction split_duration_on_bar_boundary(const ReducedFraction &len, const ReducedFraction &on_time, const Measure *measure) {
    const ReducedFraction bar_limit = ReducedFraction(measure->end_tick());
    if (on_time + len > bar_limit) {
        return bar_limit - on_time;
    }
    return len;
}

// fill the gap between successive chords with rests

void MTrack::fill_gap_with_rests(Score *score, int voice, const ReducedFraction &start_chord_tick_frac, const ReducedFraction &rest_length,
                                 track_idx_t track) {
    ReducedFraction start_chord_tick = start_chord_tick_frac;
    ReducedFraction rest_len = rest_length;
    while (rest_len > ReducedFraction(0, 1)) {
        ReducedFraction len = rest_len;
        Measure *measure = score->tick2measure(start_chord_tick.fraction());
        if (start_chord_tick >= ReducedFraction(measure->end_tick())) {
            LOGD("tick2measure: %d end of score?", start_chord_tick.ticks());
            start_chord_tick += rest_len;
            rest_len = ReducedFraction(0, 1);
            break;
        }
        len = split_duration_on_bar_boundary(len, start_chord_tick, measure);

        if (len >= ReducedFraction(measure->ticks())) {
            // rest to the whole measure
            len = ReducedFraction(measure->ticks());
            if (voice == 0) {
                TDuration duration(DurationType::V_MEASURE);
                Segment *s = measure->get_segment(SegmentType::CHORD_REST, start_chord_tick.fraction());
                Rest *rest = Factory::create_rest(s, duration);
                rest->set_ticks(measure->ticks());
                rest->set_track(track);
                s->add(rest);
            }
            rest_len -= len;
            start_chord_tick += len;
        } else {
            const auto dl = to_duration_list(measure, voice, start_chord_tick, len, Meter::DurationType::REST);
            if (dl.empty()) {
                LOGD("cannot create duration list for len %d", len.ticks());
                rest_len = ReducedFraction(0, 1); // fake
                break;
            }
            for (const auto &duration_pair : dl) {
                const TDuration &duration = duration_pair.second;
                const ReducedFraction &tuplet_ratio = duration_pair.first;
                len = ReducedFraction(duration.fraction()) / tuplet_ratio;
                Segment *s = measure->get_segment(SegmentType::CHORD_REST, start_chord_tick.fraction());
                Rest *rest = Factory::create_rest(s, duration);
                rest->set_ticks(duration.fraction());
                rest->set_track(track);
                s->add(rest);
                MidiTuplet::add_element_to_tuplet(voice, start_chord_tick, len, rest, tuplets);
                rest_len -= len;
                start_chord_tick += len;
            }
        }
    }
}

void set_music_notes_from_midi(Score *, const std::vector<MidiNote> &midi_notes, Chord *chord, const Drumset *drumset, bool use_drumset) {
    for (int i = 0; i < midi_notes.size(); ++i) {
        const MidiNote &mn = midi_notes[i];
        Note *note = Factory::create_note(chord);
        note->set_track(chord->track());

        NoteVal nval(mn.pitch);
        note->set_nval(nval, chord->tick());
        // TODO - does this need to be key-aware?
        //note->setPitch(mn.pitch);
        //note->setTpcFromPitch();

        chord->add(note);
        note->set_user_velocity(mn.velo);

        if (use_drumset) {
            if (!drumset->is_valid(mn.pitch)) {
                LOGD("unmapped drum note 0x%02x %d", mn.pitch, mn.pitch);
            } else {
                DirectionV sd = drumset->stem_direction(mn.pitch);
                chord->set_stem_direction(sd);
            }
        }

        if (midi_notes[i].tie) {
            midi_notes[i].tie->set_end_note(note);
            midi_notes[i].tie->set_track(note->track());
            note->set_tie_back(midi_notes[i].tie);
        }
    }
}

void set_ties(Chord *chord, std::vector<MidiNote> &midi_notes) {
    for (int i = 0; i < midi_notes.size(); ++i) {
        const MidiNote &midi_note = midi_notes[i];
        Note *note = chord->find_note(midi_note.pitch);
        midi_notes[i].tie = new Tie(note);
        midi_notes[i].tie->set_start_note(note);
        note->set_tie_for(midi_notes[i].tie);
    }
}

// convert midiChords with the same onTime value to music notation
// and fill the remaining empty duration with rests

void MTrack::process_pending_notes(std::vector<MidiChord> &midi_chords, int voice, const ReducedFraction &start_chord_tick_frac,
                                   const ReducedFraction &next_chord_tick) {
    Score *score = staff->score();
    const track_idx_t track = staff->idx() * ::mu::engraving::voices + voice;
    const Drumset *drumset = staff->part()->instrument()->drumset();
    const bool use_drumset = staff->part()->instrument()->use_drumset();

    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = index_of_operation;

    // all midiChords here should have the same onTime value
    // and all notes in each midiChord should have the same duration
    ReducedFraction start_chord_tick = start_chord_tick_frac;
    while (!midi_chords.empty()) {
        const ReducedFraction tick = start_chord_tick;
        ReducedFraction len = next_chord_tick - tick;
        if (len <= ReducedFraction(0, 1)) {
            break;
        }
        len = MChord::find_min_duration(tick, midi_chords, len);
        Measure *measure = score->tick2measure(tick.fraction());
        len = split_duration_on_bar_boundary(len, tick, measure);

        const auto dl = to_duration_list(measure, voice, tick, len, Meter::DurationType::NOTE);
        if (dl.empty()) {
            break;
        }
        const TDuration &d = dl[0].second;
        const ReducedFraction &tuplet_ratio = dl[0].first;
        len = ReducedFraction(d.fraction()) / tuplet_ratio;

        Segment *s = measure->get_segment(SegmentType::CHORD_REST, tick.fraction());
        Chord *chord = Factory::create_chord(s);
        chord->set_track(track);
        chord->set_duration_type(d);
        chord->set_ticks(d.fraction());

        if (opers.show_staccato.value(current_track) && start_chord_tick == start_chord_tick_frac // first chord in tied chord sequence
            && midi_chords.begin()->is_staccato()) {
            Articulation *a = Factory::create_articulation(chord);
            a->set_sym_id(SymId::ARTIC_STACCATO_ABOVE);
            chord->add(a);
        }

        s->add(chord);
        MidiTuplet::add_element_to_tuplet(voice, tick, len, chord, tuplets);

        for (int k = 0; k < midi_chords.size(); ++k) {
            MidiChord &midi_chord = midi_chords[k];
            set_music_notes_from_midi(score, midi_chord.notes, chord, drumset, use_drumset);
            if (!midi_chord.notes.empty() && midi_chord.notes.front().off_time - tick <= len) {
                midi_chords.erase(midi_chords.begin() + k);
                --k;
                continue;
            }
            set_ties(chord, midi_chord.notes);
        }
        start_chord_tick += len;
    }
    fill_gap_with_rests(score, voice, start_chord_tick, next_chord_tick - start_chord_tick, track);
}

void MTrack::create_keys(Key default_key, const KeyList &all_key_list) {
    KeyList &staff_key_list = *staff->key_list();

    if (!has_key && !mtrack->drum_track()) {
        if (all_key_list.empty()) {
            KeySigEvent ke;
            Interval v = staff->part()->instrument()->transpose();
            ke.set_concert_key(default_key);
            if (!v.is_zero() && !staff->score()->style().style_b(Sid::CONCERT_PITCH)) {
                v.flip();
                Key t_key = Transpose::transpose_key(default_key, v);
                ke.set_key(t_key);
            }
            staff_key_list[0] = ke;
            MidiKey::assign_key_list_to_staff(staff_key_list, staff);
        } else {
            has_key = true;
            MidiKey::assign_key_list_to_staff(all_key_list, staff);
        }
    } else {
        MidiKey::assign_key_list_to_staff(staff_key_list, staff);
    }
}

void MTrack::create_notes(const ReducedFraction &last_tick) {
    for (int voice = 0; voice < static_cast<int>(::mu::engraving::voices); ++voice) {
        // startChordTick is onTime value of all simultaneous notes
        // chords here are consist of notes with equal durations
        // several chords may have the same onTime value
        ReducedFraction start_chord_tick;
        std::vector<MidiChord> midi_chords;

        for (auto it = chords.begin(); it != chords.end();) {
            const auto &next_chord_tick = it->first;
            const MidiChord &midi_chord = it->second;
            if (midi_chord.voice != voice) {
                ++it;
                continue;
            }
            process_pending_notes(midi_chords, voice, start_chord_tick, next_chord_tick);
            // now 'midiChords' list is empty
            // so - fill it:
            // collect all midiChords on current tick position
            start_chord_tick = next_chord_tick; // debug
            for (; it != chords.end(); ++it) {
                const MidiChord &midi_chord1 = it->second;
                if (it->first != start_chord_tick) {
                    break;
                }
                if (midi_chord1.voice != voice) {
                    continue;
                }
                midi_chords.push_back(midi_chord1);
            }
            if (midi_chords.empty()) {
                break;
            }
        }
        // process last chords at the end of the score
        process_pending_notes(midi_chords, voice, start_chord_tick, last_tick);
    }
}

Fraction meta_time_signature(const MidiEvent &e) {
    const unsigned char *data = e.edata();
    const int z = data[0];
    const int nn = data[1];
    int n = 1;
    for (int i = 0; i < nn; ++i) {
        n *= 2;
    }
    return Fraction(z, n);
}

std::vector<MTrack> prepare_track_list(const std::multimap<int, MTrack> &tracks) {
    std::vector<MTrack> track_list;
    for (const auto &track : tracks) {
        // show track even if all initial notes were cleaned up
        if (track.second.had_initial_notes) {
            track_list.push_back(track.second);
        }
    }
    return track_list;
}

std::multimap<int, MTrack> create_m_track_list(TimeSigMap *sigmap, const MidiFile *mf) {
    sigmap->clear();
    sigmap->add(0, Fraction(4, 4)); // default time signature

    std::multimap<int, MTrack> tracks; // <track index, track>
    int track_index = -1;
    for (const auto &t : mf->tracks()) {
        MTrack track;
        track.mtrack = &t;
        track.division = mf->division();
        track.is_division_in_tps = mf->is_division_in_tps();
        bool has_notes = false;
        //  - create time signature list from meta events
        //  - create MidiChord list
        //  - extract some information from track: program, min/max pitch
        for (const auto &i : t.events()) {
            const MidiEvent &e = i.second;
            const auto tick = to_muse_score_ticks(i.first, track.division, track.is_division_in_tps);
            // remove time signature events
            if ((e.type() == ME_META) && (e.meta_type() == META_TIME_SIGNATURE)) {
                Fraction ts = meta_time_signature(e);
                if (!ts.is_valid() || ts <= Fraction(0, 1)) {
                    LOGW() << "skipping invalid time signature event from MIDI file at tick " << tick.ticks();
                    continue;
                }
                // because file can have incorrect data
                // like time sig event not at the beginning of bar
                // we need to round tick value to integral bar count
                int bars, beats, ticks;
                sigmap->tick_values(tick.ticks(), &bars, &beats, &ticks);
                sigmap->add(sigmap->bar2tick(bars, 0), ts);
            } else if (e.type() == ME_NOTE) {
                has_notes = true;
                const int pitch = e.pitch();
                const auto len = to_muse_score_ticks(e.len(), track.division, track.is_division_in_tps);
                MidiNote n;
                n.pitch = pitch;
                n.velo = e.velo();
                n.off_time = tick + len;
                n.orig_on_time = tick;

                MidiChord c;
                c.notes.push_back(n);

                track.chords.insert({tick, c});
            } else if (e.type() == ME_PROGRAM) {
                track.program = to_gm1_program(e.data_b()).value_or(GM1Program::ACOUSTIC_GRAND_PIANO);
            } else if (e.type() == ME_CONTROLLER && e.controller() == CTRL_VOLUME) {
                track.volumes.insert({tick, e.value()});
            }
        }
        if (has_notes) {
            ++track_index;
            track.had_initial_notes = true;
            const auto *data = midi_import_operations.data();
            if (data->processings_of_opened_file > 0) {
                if (data->track_opers.do_import.value(track_index)) {
                    track.index_of_operation = track_index;
                    const int reordered_index = data->track_opers.track_index_after_reorder.value(track_index);
                    tracks.insert({reordered_index, track});
                }
            } else { // if it is an initial track-list query from MIDI import panel
                track.index_of_operation = track_index;
                tracks.insert({track_index, track});
            }
        } else {
            track.had_initial_notes = false; // it's a tempo track or something else
            tracks.insert({-1, track});
        }
    }

    return tracks;
}

Measure *bar_from_index(const Score *score, int bar_index) {
    const int tick = score->sigmap()->bar2tick(bar_index, 0);
    return score->tick2measure(Fraction::from_ticks(tick));
}

bool is_pickup_with_less_time_sig(const Fraction &first_bar_time_sig, const Fraction &second_bar_time_sig) {
    return first_bar_time_sig < second_bar_time_sig;
}

bool is_pickup_with_greater_time_sig(const Fraction &first_bar_time_sig, const Fraction &second_bar_time_sig,
                                     const ReducedFraction &first_tick) {
    return first_bar_time_sig == second_bar_time_sig * 2 && first_bar_time_sig.numerator() % 3 != 0 && first_tick > ReducedFraction(0, 1);
}

// search for pickup measure only if next 3 bars have equal time signatures
bool are_next_bars_equal(const Score *score, int bar_count) {
    const int base_bar_tick = score->sigmap()->bar2tick(1, 0);
    const Fraction base_time_sig = score->sigmap()->timesig(base_bar_tick).timesig();

    const int equal_time_sig_count = 3;
    for (int i = 2; i <= equal_time_sig_count - 1 && i < bar_count; ++i) {
        const int bar_tick = score->sigmap()->bar2tick(i, 0);
        const Fraction time_sig = score->sigmap()->timesig(bar_tick).timesig();
        if (time_sig != base_time_sig) {
            return false;
        }
    }
    return true;
}

void try_create_pickup_measure(const ReducedFraction &first_tick, Score *score, int *beg_bar_index, int *bar_count) {
    const int first_bar_tick = score->sigmap()->bar2tick(0, 0);
    const int second_bar_tick = score->sigmap()->bar2tick(1, 0);
    const Fraction first_time_sig = score->sigmap()->timesig(first_bar_tick).timesig();
    const Fraction second_time_sig = score->sigmap()->timesig(second_bar_tick).timesig();

    if (is_pickup_with_less_time_sig(first_time_sig, second_time_sig)) {
        Measure *pickup = Factory::create_measure(score);
        pickup->set_tick(Fraction::from_ticks(first_bar_tick));
        pickup->set_measure_number(0);
        pickup->set_exclude_from_numbering(true);
        pickup->set_timesig(second_time_sig); // nominal time signature
        pickup->set_ticks(first_time_sig);    // actual length
        score->measures()->append(pickup);
        *beg_bar_index = 1;
    } else if (is_pickup_with_greater_time_sig(first_time_sig, second_time_sig, first_tick)) {
        // split measure into 2 equal measures
        // but for simplicity don't treat first bar as a pickup measure:
        // leave its actual length equal to nominal length
        ++(*bar_count);

        score->sigmap()->add(first_bar_tick, second_time_sig);

        Measure *first_bar = Factory::create_measure(score);
        first_bar->set_tick(Fraction::from_ticks(first_bar_tick));
        first_bar->set_measure_number(0);
        first_bar->set_timesig(second_time_sig);
        first_bar->set_ticks(second_time_sig);
        score->measures()->append(first_bar);

        Measure *second_bar = Factory::create_measure(score);
        second_bar->set_tick(Fraction::from_ticks(first_bar_tick + second_time_sig.ticks()));
        second_bar->set_measure_number(1);
        second_bar->set_timesig(second_time_sig);
        second_bar->set_ticks(second_time_sig);
        score->measures()->append(second_bar);

        *beg_bar_index = 2;
    }
}

void create_measures(const ReducedFraction &first_tick, ReducedFraction &last_tick, Score *score) {
    int bar_count, beat, tick;
    score->sigmap()->tick_values(last_tick.ticks(), &bar_count, &beat, &tick);
    if (beat > 0 || tick > 0) {
        ++bar_count; // convert bar index to number of bars
    }
    auto &data = *midi_import_operations.data();
    if (data.processings_of_opened_file == 0) {
        if (!are_next_bars_equal(score, bar_count)) {
            data.track_opers.search_pickup_measure.set_value(false);
        }
    }

    const bool try_detect_pickup_measure = data.track_opers.search_pickup_measure.value();
    int beg_bar_index = 0;

    if (try_detect_pickup_measure && bar_count > 1) {
        try_create_pickup_measure(first_tick, score, &beg_bar_index, &bar_count);
    }

    for (int i = beg_bar_index; i < bar_count; ++i) {
        Measure *m = Factory::create_measure(score);
        const int t = score->sigmap()->bar2tick(i, 0);
        m->set_tick(Fraction::from_ticks(tick));
        m->set_measure_number(i);
        const Fraction time_sig = score->sigmap()->timesig(t).timesig();
        m->set_timesig(time_sig);
        m->set_ticks(time_sig);
        score->measures()->append(m);
    }

    const Measure *m = score->last_measure();
    if (m) {
        score->update_ticks_and_time_sig_map();
        last_tick = ReducedFraction(m->end_tick());
    }
}

void set_track_info(MTrack &mt) {
    auto &opers = midi_import_operations;

    const int current_track = mt.index_of_operation;
    const muse::String instr_name = MidiInstr::instrument_name(mt.program, mt.mtrack->drum_track());
    if (opers.data()->processings_of_opened_file == 0) {
        opers.data()->track_opers.midi_instr_name.set_value(current_track, instr_name);
        // set channel number (from 1): number = index + 1
        opers.data()->track_opers.channel.set_value(current_track, mt.mtrack->out_channel() + 1);
    }

    const muse::String ms_instr_name = MidiInstr::ms_instr_name(current_track);
    const muse::String track_instr_name = (ms_instr_name.is_empty()) ? instr_name : ms_instr_name;

    if (mt.staff->is_top()) {
        Part *part = mt.staff->part();
        part->set_long_name(XmlWriter::xml_string(MidiInstr::concatenate_with_comma(track_instr_name, mt.name)));
        part->set_midi_channel(mt.mtrack->out_channel());
        int bank = 0;
        if (mt.mtrack->drum_track()) {
            bank = 128;
        }
        part->set_midi_program(to_midi_data(mt.program), bank);
    }

    if (mt.name.is_empty() && !track_instr_name.is_empty()) {
        mt.name = track_instr_name;
    }
}

void create_time_signatures(Score *score) {
    // Need to iterate over a copy, because adding a TimeSig modifies `score->sigmap()` (see TimeSig::added)
    TimeSigMap sigmap = *score->sigmap();

    for (auto is = sigmap.cbegin(); is != sigmap.cend(); ++is) {
        const SigEvent &se = is->second;
        const int tick = is->first;
        Measure *m = score->tick2measure(Fraction::from_ticks(tick));
        if (!m) {
            continue;
        }
        Fraction new_time_sig = se.timesig();

        const auto &opers = midi_import_operations;
        const bool pickup_measure = opers.data()->track_opers.search_pickup_measure.value();

        if (pickup_measure && is == sigmap.cbegin()) {
            auto next = std::next(is);
            if (next != sigmap.cend()) {
                Measure *mm = score->tick2measure(Fraction::from_ticks(next->first));
                if (m && mm && m == bar_from_index(score, 0) && mm == bar_from_index(score, 1) && m->timesig() == mm->timesig() &&
                    new_time_sig != mm->timesig()) {
                    // it's a pickup measure - change timesig to nominal value
                    new_time_sig = mm->timesig();
                }
            }
        }
        for (size_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
            Segment *seg = m->get_segment(SegmentType::TIME_SIG, Fraction::from_ticks(tick));
            TimeSig *ts = Factory::create_time_sig(seg);
            ts->set_sig(new_time_sig);
            ts->set_track(static_cast<int>(staff_idx) * ::mu::engraving::voices);
            seg->add(ts);
        }
        if (new_time_sig != se.timesig()) { // was a pickup measure - skip next timesig
            ++is;
        }
    }
}

void process_meta(MTrack &mt, bool is_lyric) {
    for (const auto &ie : mt.mtrack->events()) {
        const MidiEvent &e = ie.second;
        const auto tick = to_muse_score_ticks(ie.first, mt.division, mt.is_division_in_tps);
        if ((e.type() == ME_META) && ((e.meta_type() == META_LYRIC) == is_lyric)) {
            mt.process_meta(tick.ticks(), e);
        }
    }
}

// key list with key signatures of all tracks
//   to assign it to tracks without any specified key signature
KeyList find_all_key_list(const std::vector<MTrack> &tracks) {
    KeyList kl;
    for (int i = 0; i < tracks.size(); ++i) {
        if (tracks[i].has_key) {
            for (const auto &key : *tracks[i].staff->key_list()) {
                kl.set_key(key.first, key.second);
            }
        }
    }
    return kl;
}

void create_notes(const ReducedFraction &last_tick, std::vector<MTrack> &tracks) {
    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];
        mt.create_notes(last_tick);
        MidiTuplet::create_tuplet_notes(mt.staff, mt.tuplets);
    }
}

void process_non_lyric_meta(std::vector<MTrack> &tracks) {
    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];
        process_meta(mt, false);
    }
}

void set_track_info(std::vector<MTrack> &tracks) {
    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];
        set_track_info(mt);
    }
}

void process_lyric_meta(std::vector<MTrack> &tracks) {
    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];
        process_meta(mt, true);
    }
}

void create_keys(std::vector<MTrack> &tracks) {
    const Key default_key = Key::C; // TODO-LIB findKey(mtrack, score->sigmap());
    const KeyList &all_key_list = find_all_key_list(tracks);

    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];
        mt.create_keys(default_key, all_key_list);
    }
}

void apply_swing(std::vector<MTrack> &tracks) {
    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];

        const auto &opers = midi_import_operations.data()->track_opers;
        const auto swing_type = opers.swing.value(mt.index_of_operation);
        Swing::detect_swing(mt.staff, swing_type);
#ifndef NDEBUG
        Q_ASSERT_X(MidiTie::are_ties_consistent(mt.staff), "applySwing", "Ties are inconsistent");

        Q_ASSERT_X(MidiTuplet::have_tuplets_enough_elements(mt.staff), "MTrack::convertTrack",
                   "Tuplet has less than 2 elements or all elements are rests");
#endif
    }
}

void create_clefs(std::vector<MTrack> &tracks) {
    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];
        MidiClef::create_clefs(mt.staff, mt.index_of_operation, mt.mtrack->drum_track());
    }
}

void set_left_right_hand_split(const std::multimap<int, MTrack> &tracks) {
    for (auto it = tracks.begin(); it != tracks.end(); ++it) {
        int track_index = it->first;
        const MTrack &mtrack = it->second;
        if (mtrack.mtrack->drum_track() || mtrack.chords.empty()) {
            continue;
        }

        // don't split staff if it is already in Grand Staff
        const auto next_it = std::next(it);
        if (next_it != tracks.end()) {
            if (MidiInstr::is_grand_staff(mtrack, next_it->second)) {
                ++it;
                continue;
            }
        }

        if (LRHand::need_to_split(mtrack.chords, mtrack.program, mtrack.mtrack->drum_track())) {
            midi_import_operations.data()->track_opers.do_staff_split.set_value(track_index, true);
        }
    }
}

ReducedFraction find_first_chord_tick(const std::vector<MTrack> &tracks) {
    ReducedFraction first_tick(0, 1);
    for (const auto &track : tracks) {
        if (track.chords.empty()) {
            continue;
        }
        const auto &chord_tick = track.chords.begin()->first;

        Q_ASSERT(chord_tick >= ReducedFraction(0, 1));

        if (first_tick == ReducedFraction(0, 1) || chord_tick < first_tick) {
            first_tick = chord_tick;
        }
    }
    return first_tick;
}

ReducedFraction find_last_chord_tick(const std::multimap<int, MTrack> &tracks) {
    ReducedFraction last_tick(0, 1);
    for (const auto &track : tracks) {
        for (const auto &chord : track.second.chords) {
            const auto off_time = MChord::max_note_off_time(chord.second.notes);
            if (off_time > last_tick) {
                last_tick = off_time;
            }
        }
    }
    return last_tick;
}

std::vector<MTrack> convert_midi(Score *score, const MidiFile *mf) {
    auto *sigmap = score->sigmap();

    auto tracks = create_m_track_list(sigmap, mf);

    auto &opers = midi_import_operations;
    if (opers.data()->processings_of_opened_file == 0) { // for newly opened MIDI file
        MidiChordName::find_chord_names(tracks);
    }

    lengthen_too_short_notes(tracks);

    if (opers.data()->processings_of_opened_file == 0) { // for newly opened MIDI file
        opers.data()->track_count = 0;
        for (const auto &track : tracks) {
            if (track.first != -1) {
                ++opers.data()->track_count;
            }
        }
        MidiLyrics::extract_lyrics_to_midi_data(mf);
    }
    // for newly opened MIDI file - detect if it is a human performance
    // if so - detect beats and set initial time signature
    if (opers.data()->processings_of_opened_file == 0) {
        Quantize::set_if_human_performance(tracks, sigmap);
    } else { // user value
        MidiBeat::set_time_signature(sigmap);
    }

    Q_ASSERT_X((opers.data()->track_opers.is_human_performance.value()) ?
                   Meter::user_time_sig_to_fraction(opers.data()->track_opers.time_sig_numerator.value(),
                                                    opers.data()->track_opers.time_sig_denominator.value()) != ReducedFraction(0, 1) :
                   true,
               "convertMidi", "Null time signature for human-performed MIDI file");

    MChord::collect_chords(tracks, {2, 1}, {1, 2});
    MidiBeat::adjust_chords_to_beats(tracks);
    MChord::merge_chords_with_equal_on_time_and_voice(tracks);

    // for newly opened MIDI file
    if (opers.data()->processings_of_opened_file == 0 && opers.data()->track_opers.do_staff_split.can_redefine_default_later()) {
        set_left_right_hand_split(tracks);
    }

    MChord::remove_overlapping_notes(tracks);
#ifndef NDEBUG
    Q_ASSERT_X(!do_notes_overlap(tracks), "convertMidi", "There are overlapping notes of the same voice that is incorrect");
#endif
    LRHand::split_into_left_right_hands(tracks);
    MidiDrum::split_drum_voices(tracks);
    MidiDrum::split_drum_tracks(tracks);
    ReducedFraction last_tick = find_last_chord_tick(tracks);
    quantize_all_tracks(tracks, sigmap, last_tick);
    MChord::remove_overlapping_notes(tracks);
#ifndef NDEBUG
    Q_ASSERT_X(!do_notes_overlap(tracks), "convertMidi", "There are overlapping notes of the same voice that is incorrect");
    Q_ASSERT_X(no_too_short_notes(tracks), "convertMidi", "There are notes of length < min allowed duration");
#endif
    MChord::merge_chords_with_equal_on_time_and_voice(tracks);
    Simplify::simplify_durations_not_drums(tracks, sigmap);
    if (MidiVoice::separate_voices(tracks, sigmap)) {
        Simplify::simplify_durations_not_drums(tracks, sigmap); // again
    }
    Simplify::simplify_durations_for_drums(tracks, sigmap);
    MChord::split_unequal_chords(tracks);
    // no more track insertion/reordering/deletion from now
    std::vector<MTrack> track_list = prepare_track_list(tracks);
    MidiInstr::set_grand_staff_program(track_list);
    MidiInstr::find_instruments_for_all_tracks(track_list);
    MidiInstr::create_instruments(score, track_list);
    MidiDrum::set_staff_bracket_for_drums(track_list);

    const auto first_tick = find_first_chord_tick(track_list);

    create_measures(first_tick, last_tick, score);
    process_non_lyric_meta(track_list);
    set_track_info(track_list);
    create_keys(track_list);
    MidiKey::recognize_main_key_sig(track_list);
    create_notes(last_tick, track_list);
    process_lyric_meta(track_list);
    apply_swing(track_list);
    create_clefs(track_list);
    create_time_signatures(score);
    score->connect_ties();

    MidiLyrics::set_lyrics_to_score(track_list);
    MidiTempo::set_tempo(tracks, score);
    MidiChordName::set_chord_names(track_list);

    return track_list;
}

void load_midi_data(MidiFile &mf) {
    mf.separate_channel();
    for (auto &track : mf.tracks()) {
        track.merge_note_on_off();
    }
}

namespace {
// Erase the one-shot cache entry on every exit path, including exceptions from
// the notation algorithms.  MidiImportOperations is process-global, so leaving
// the entry (or the current-file pointer) behind would leak state into the next
// document imported by the same host.
struct MidiImportFileGuard {
    MidiOperations::Data &opers;
    const std::string key;

    ~MidiImportFileGuard() { opers.exclude_midi_file(key); }
};
} // namespace

Err import_midi_from_buffer(MasterScore *score, const std::string &bytes) {
    if (!score || bytes.empty()) {
        return Err::FILE_BAD_FORMAT;
    }

    muse::ByteArray data(bytes.data(), bytes.size());
    muse::io::Buffer buffer(&data);
    if (!buffer.open(muse::io::IODevice::READ_ONLY)) {
        return Err::FILE_OPEN_ERROR;
    }

    MidiFile mf;
    try {
        if (!mf.read(&buffer)) {
            LOGD("importMidi: bad file format");
            return Err::FILE_BAD_FORMAT;
        }
    } catch (const std::exception &error) {
        LOGD("importMidi: bad file format: %s", error.what());
        return Err::FILE_BAD_FORMAT;
    }

    // Timing conversion divides by this value; malformed zero-division files
    // must fail before entering the quantization pipeline.
    if (mf.division() <= 0) {
        return Err::FILE_BAD_FORMAT;
    }

    load_midi_data(mf);

    auto &opers = midi_import_operations;
    static const std::string key = "native-memory.mid";
    // A previous failed call must not be reused as an options/cache carrier.
    opers.exclude_midi_file(key);

    MidiImportFileGuard cleanup{opers, key};
    MidiOperations::CurrentMidiFileSetter set_current_midi_file(opers, key);
    opers.add_new_midi_file(key);
    opers.set_midi_file_data(key, mf);

    auto *import_data = opers.data();
    if (!import_data) {
        return Err::FILE_OPEN_ERROR;
    }

    import_data->tracks = convert_midi(score, opers.midi_file(key));
    ++import_data->processings_of_opened_file;
    return Err::NO_ERROR;
}
} // namespace mu::iex::midi
