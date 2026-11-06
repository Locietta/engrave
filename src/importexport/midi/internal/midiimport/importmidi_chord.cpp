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
#include "importmidi_chord.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <utility>
#include <vector>

#include "global/containers.h"

#include "importmidi_inner.h"
#include "importmidi_chord.h"
#include "importmidi_operations.h"
#include "importmidi_quant.h"
#include "engraving/dom/sig.h"

#include "log.h"

namespace mu::iex::midi { namespace MChord {
bool is_grand_staff_program(GM1Program program) {
    static constexpr std::array grand_staff_programs = {
        GM1Program::ACOUSTIC_GRAND_PIANO,
        GM1Program::BRIGHT_ACOUSTIC_PIANO,
        GM1Program::ELECTRIC_GRAND_PIANO,
        GM1Program::HONKY_TONK_PIANO,
        GM1Program::ELECTRIC_PIANO1,
        GM1Program::ELECTRIC_PIANO2,
        GM1Program::HARPSICHORD,
        GM1Program::CLAVI,

        GM1Program::CELESTA,
        GM1Program::MUSIC_BOX,
        GM1Program::VIBRAPHONE,
        GM1Program::MARIMBA,
        GM1Program::XYLOPHONE,
        GM1Program::DULCIMER,

        GM1Program::DRAWBAR_ORGAN,
        GM1Program::PERCUSSIVE_ORGAN,
        GM1Program::ROCK_ORGAN,
        GM1Program::CHURCH_ORGAN,
        GM1Program::REED_ORGAN,
        GM1Program::ACCORDION,
        GM1Program::TANGO_ACCORDION,

        GM1Program::ORCHESTRAL_HARP,

        GM1Program::SYNTH_STRINGS1,
        GM1Program::SYNTH_STRINGS2,
        GM1Program::SYNTH_VOICE,

        GM1Program::SYNTH_BRASS1,
        GM1Program::SYNTH_BRASS2,

        GM1Program::LEAD1_SQUARE,
        GM1Program::LEAD2_SAWTOOTH,
        GM1Program::LEAD3_CALLIOPE,
        GM1Program::LEAD4_CHIFF,
        GM1Program::LEAD5_CHARANG,
        GM1Program::LEAD6_VOICE,
        GM1Program::LEAD7_FIFTHS,
        GM1Program::LEAD8_BASS_AND_LEAD,

        GM1Program::PAD1_NEW_AGE,
        GM1Program::PAD2_WARM,
        GM1Program::PAD3_POLYSYNTH,
        GM1Program::PAD4_CHOIR,
        GM1Program::PAD5_BOWED,
        GM1Program::PAD6_METALLIC,
        GM1Program::PAD7_HALO,
        GM1Program::PAD8_SWEEP,

        GM1Program::F_X1_RAIN,
        GM1Program::F_X2_SOUNDTRACK,
        GM1Program::F_X3_CRYSTAL,
        GM1Program::F_X4_ATMOSPHERE,
        GM1Program::F_X5_BRIGHTNESS,
        GM1Program::F_X6_GOBLINS,
        GM1Program::F_X7_ECHOES,
        GM1Program::F_X8_SCI_FI,
    };

    return muse::contains(grand_staff_programs, program);
}

std::multimap<ReducedFraction, MidiChord>::iterator find_first_chord_in_range(std::multimap<ReducedFraction, MidiChord> &chords,
                                                                              const ReducedFraction &start_range_tick,
                                                                              const ReducedFraction &end_range_tick) {
    auto iter = chords.lower_bound(start_range_tick);
    if (iter != chords.end() && iter->first >= end_range_tick) {
        iter = chords.end();
    }
    return iter;
}

std::multimap<ReducedFraction, MidiChord>::const_iterator find_first_chord_in_range(const std::multimap<ReducedFraction, MidiChord> &chords,
                                                                                    const ReducedFraction &start_range_tick,
                                                                                    const ReducedFraction &end_range_tick) {
    auto iter = chords.lower_bound(start_range_tick);
    if (iter != chords.end() && iter->first >= end_range_tick) {
        iter = chords.end();
    }
    return iter;
}

const ReducedFraction &min_allowed_duration() {
    const static auto min_duration = ReducedFraction::from_ticks(engraving::Constants::division) / 32;
    return min_duration;
}

ReducedFraction min_note_off_time(const std::vector<MidiNote> &notes) {
    if (notes.empty()) {
        return {0, 1};
    }
    auto it = notes.begin();
    ReducedFraction min_off_time = it->off_time;
    for (++it; it != notes.end(); ++it) {
        if (it->off_time < min_off_time) {
            min_off_time = it->off_time;
        }
    }
    return min_off_time;
}

ReducedFraction max_note_off_time(const std::vector<MidiNote> &notes) {
    ReducedFraction max_off_time(0, 1);
    for (const auto &note : notes) {
        if (note.off_time > max_off_time) {
            max_off_time = note.off_time;
        }
    }
    return max_off_time;
}

ReducedFraction min_note_len(const std::pair<const ReducedFraction, MidiChord> &chord) {
    const auto min_off_time = min_note_off_time(chord.second.notes);
    return min_off_time - chord.first;
}

ReducedFraction max_note_len(const std::pair<const ReducedFraction, MidiChord> &chord) {
    const auto max_off_time = max_note_off_time(chord.second.notes);
    return max_off_time - chord.first;
}

void remove_overlapping_notes(std::vector<MidiNote> &notes) {
    std::list<MidiNote> temp_notes;
    for (const auto &note : notes) {
        temp_notes.push_back(note);
    }

    for (auto note_it1 = temp_notes.begin(); note_it1 != temp_notes.end(); ++note_it1) {
        for (auto note_it2 = std::next(note_it1); note_it2 != temp_notes.end();) {
            if (note_it2->pitch == note_it1->pitch) {
                // overlapping notes found
                if (note_it2->off_time > note_it1->off_time) { // set max len before erase
                    note_it1->off_time = note_it2->off_time;
                }
                note_it2 = temp_notes.erase(note_it2);
                LOGD() << "MIDI import: removeOverlappingNotes: note was removed";
                continue;
            }
            ++note_it2;
        }
    }
    notes.clear();
    for (const auto &note : temp_notes) {
        notes.push_back(note);
    }
}

// remove overlapping notes with the same pitch

void remove_overlapping_notes(std::multimap<int, MTrack> &tracks) {
    for (auto &track : tracks) {
        auto &chords = track.second.chords;
        if (chords.empty()) {
            continue;
        }
#ifndef NDEBUG
        Q_ASSERT_X(MidiTuplet::are_tuplet_ranges_ok(chords, track.second.tuplets), "MChord::removeOverlappingNotes",
                   "Tuplet chord/note is outside tuplet "
                   "or non-tuplet chord/note is inside tuplet before overlaps remove");
#endif
        for (auto i1 = chords.begin(); i1 != chords.end();) {
            const auto &on_time1 = i1->first;
            auto &chord1 = i1->second;
            remove_overlapping_notes(chord1.notes);

            for (auto note1_it = chord1.notes.begin(); note1_it != chord1.notes.end();) {
                auto &note1 = *note1_it;

                for (auto i2 = std::next(i1); i2 != chords.end(); ++i2) {
                    const auto &on_time2 = i2->first;
                    if (on_time2 >= note1.off_time) {
                        break;
                    }
                    auto &chord2 = i2->second;
                    if (chord1.voice != chord2.voice) {
                        continue;
                    }
                    for (auto &note2 : chord2.notes) {
                        if (note2.pitch != note1.pitch) {
                            continue;
                        }
                        // overlapping notes found
                        note1.off_time = on_time2;
                        if (!note1.is_in_tuplet && chord2.is_in_tuplet) {
                            if (note1.off_time > chord2.tuplet->second.on_time) {
                                note1.is_in_tuplet = true;
                                note1.tuplet = chord2.tuplet;
                            }
                        } else if (note1.is_in_tuplet && !chord2.is_in_tuplet) {
                            note1.is_in_tuplet = false;
                        }

                        i2 = std::prev(chords.end());
                        break;
                    }
                }
                if (note1.off_time - on_time1 < MChord::min_allowed_duration()) {
                    note1_it = chord1.notes.erase(note1_it);
                    LOGD("MIDI import: removeOverlappingNotes: note was removed");
                    continue;
                }
                ++note1_it;
            }
            if (chord1.notes.empty()) {
                i1 = chords.erase(i1);
                continue;
            }
            ++i1;
        }

        MidiTuplet::remove_empty_tuplets(track.second);
#ifndef NDEBUG
        Q_ASSERT_X(MidiTuplet::are_tuplet_ranges_ok(chords, track.second.tuplets), "MChord::removeOverlappingNotes",
                   "Tuplet chord/note is outside tuplet "
                   "or non-tuplet chord/note is inside tuplet after overlaps remove");
#endif
    }
}

#ifndef NDEBUG

// check for equal on time values with the same voice that is invalid
bool are_on_time_values_different(const std::multimap<ReducedFraction, MidiChord> &chords) {
    std::map<ReducedFraction, int> on_time_voices;
    for (const auto &chord_event : chords) {
        const auto it = on_time_voices.find(chord_event.first);
        if (it == on_time_voices.end()) {
            on_time_voices.insert({chord_event.first, chord_event.second.voice});
        } else if (chord_event.second.voice == it->second) {
            return false;
        }
    }
    return true;
}

bool are_notes_long_enough(const std::multimap<ReducedFraction, MidiChord> &chords) {
    for (const auto &chord : chords) {
        if (min_note_len(chord) < min_allowed_duration()) {
            return false;
        }
    }
    return true;
}

bool are_bar_indexes_successive(const std::multimap<ReducedFraction, MidiChord> &chords) {
    int bar_index = 0;
    for (const auto &chord : chords) {
        const MidiChord &c = chord.second;
        if (c.bar_index < 0) {
            return false;
        }
        if (c.bar_index < bar_index) {
            return false;
        }
        bar_index = c.bar_index;
    }
    return true;
}

bool is_last_tick_valid(const ReducedFraction &last_tick, const std::multimap<ReducedFraction, MidiChord> &chords) {
    for (const auto &chord : chords) {
        if (max_note_off_time(chord.second.notes) > last_tick) {
            return false;
        }
    }
    return true;
}

bool is_last_tick_valid(const ReducedFraction &last_tick, const std::multimap<int, MTrack> &tracks) {
    for (const auto &track : tracks) {
        if (!(is_last_tick_valid(last_tick, track.second.chords))) {
            return false;
        }
    }
    return true;
}

bool are_bar_indexes_set(const std::multimap<ReducedFraction, MidiChord> &chords) {
    for (const auto &chord : chords) {
        if (chord.second.bar_index == -1) {
            return false;
        }
    }
    return true;
}

#endif

void set_to_negative(ReducedFraction &v1, ReducedFraction &v2, ReducedFraction &v3) {
    v1 = ReducedFraction(-1, 1);
    v2 = ReducedFraction(-1, 1);
    v3 = ReducedFraction(-1, 1);
}

bool has_notes_with_equal_pitch(const MidiChord &chord1, const MidiChord &chord2) {
    std::set<int> notes1;
    for (const auto &note : chord1.notes) {
        notes1.insert(note.pitch);
    }
    for (const auto &note : chord2.notes) {
        if (notes1.find(note.pitch) != notes1.end()) {
            return true;
        }
    }
    return false;
}

void collect_chords(std::multimap<int, MTrack> &tracks, const ReducedFraction &human_tol_coeff,
                    const ReducedFraction &non_human_tol_coeff) {
    for (auto &track : tracks) {
        collect_chords(track.second, human_tol_coeff, non_human_tol_coeff);
    }
}

// based on quickthresh algorithm
//
// http://www.cycling74.com/docs/max5/refpages/max-ref/quickthresh.html
// (link date 9 July 2013)
//
// here are default values for audio, in milliseconds
// for midi there will be another values, in ticks

// all notes received in the left inlet within this time period are collected into a chord
// threshTime = 40 ms

// if there are any incoming values within this amount of time
// at the end of the base thresh time,
// the threshold is extended to allow more notes to be added to the chord
// fudgeTime = 10 ms

// this is an extension value of the base thresh time, which is used if notes arrive
// in the object's inlet in the "fudge" time zone
// threshExtTime = 20 ms

//     chord                             |<--fudge time-->|
// ------x-------------------------------|----------------|---------------------|------
//       |<-----------------thresh time------------------>|<--thresh ext time-->|
//
void collect_chords(MTrack &track, const ReducedFraction &human_tol_coeff, const ReducedFraction &non_human_tol_coeff) {
    auto &chords = track.chords;
    if (chords.empty()) {
        return;
    }
#ifndef NDEBUG
    Q_ASSERT_X(are_notes_long_enough(chords), "MChord::collectChords", "There are too short notes");
#endif

    const auto &opers = midi_import_operations.data()->track_opers;
    const auto min_allowed_dur = min_allowed_duration();

    const auto thresh_time =
        (opers.is_human_performance.value()) ? min_allowed_dur * human_tol_coeff : min_allowed_dur * non_human_tol_coeff;
    const auto fudge_time = thresh_time / 4;
    const auto thresh_ext_time = thresh_time / 2;

    ReducedFraction current_chord_start;
    ReducedFraction cur_thresh_time;
    // if note onTime goes after max chord offTime
    // then this is not a chord but arpeggio
    ReducedFraction max_off_time;

    set_to_negative(current_chord_start, cur_thresh_time, max_off_time); // invalidate

    for (auto it = chords.begin(); it != chords.end();) {
        if (it->second.is_in_tuplet) {
            set_to_negative(current_chord_start, cur_thresh_time, max_off_time);
            ++it;
            continue;
        }

        const auto max_note_off_time = MChord::max_note_off_time(it->second.notes);
        if (it->first < current_chord_start + cur_thresh_time) {
            // this branch should not be executed when it == chords.begin()
            Q_ASSERT_X(it != chords.begin(), "MChord: collectChords", "it == chords.begin()");

            if (it->first <= max_off_time - min_allowed_dur) {
                // add current note to the previous chord
                auto chord_add_to = std::prev(it);
                if (it->second.voice != chord_add_to->second.voice) {
                    set_to_negative(current_chord_start, cur_thresh_time, max_off_time);
                    ++it;
                    continue;
                }

                if (!has_notes_with_equal_pitch(chord_add_to->second, it->second)) {
                    for (const auto &note : std::as_const(it->second.notes)) {
                        chord_add_to->second.notes.push_back(note);
                    }
                    if (max_note_off_time > max_off_time) {
                        max_off_time = max_note_off_time;
                    }
                }
                if (it->first >= current_chord_start + cur_thresh_time - fudge_time && cur_thresh_time == thresh_time) {
                    cur_thresh_time += thresh_ext_time;
                }

                it = chords.erase(it);
                continue;
            }
        }

        current_chord_start = it->first;
        max_off_time = max_note_off_time;
        cur_thresh_time = thresh_time;
        ++it;
    }
#ifndef NDEBUG
    Q_ASSERT_X(are_on_time_values_different(chords), "MChord: collectChords", "onTime values of chords are equal but should be different");
#endif
}

void sort_notes_by_pitch(std::multimap<ReducedFraction, MidiChord> &chords) {
    struct {
        bool operator()(const MidiNote &note1, const MidiNote &note2) { return note1.pitch < note2.pitch; }
    } pitch_sort;

    for (auto &chord_event : chords) {
        // in each chord sort notes by pitches
        auto &notes = chord_event.second.notes;
        std::sort(notes.begin(), notes.end(), pitch_sort);
    }
}

void sort_notes_by_length(std::multimap<ReducedFraction, MidiChord> &chords) {
    struct {
        bool operator()(const MidiNote &note1, const MidiNote &note2) { return note1.off_time < note2.off_time; }
    } len_sort;

    for (auto &chord_event : chords) {
        // in each chord sort notes by lengths
        auto &notes = chord_event.second.notes;
        std::sort(notes.begin(), notes.end(), len_sort);
    }
}

// find notes of each chord that have different durations
// and separate them into different chords
// so all notes inside every chord will have equal lengths

void split_unequal_chords(std::multimap<int, MTrack> &tracks) {
    for (auto &track : tracks) {
        std::vector<std::pair<ReducedFraction, MidiChord>> new_chord_events;
        auto &chords = track.second.chords;
        if (chords.empty()) {
            continue;
        }
        sort_notes_by_length(chords);
        for (auto &chord_event : chords) {
            auto &chord = chord_event.second;
            auto &notes = chord.notes;
            ReducedFraction off_time;
            for (auto it = notes.begin(); it != notes.end();) {
                if (it == notes.begin()) {
                    off_time = it->off_time;
                } else {
                    ReducedFraction new_off_time = it->off_time;
                    if (new_off_time != off_time) {
                        MidiChord new_chord(chord);
                        new_chord.notes.clear();
                        for (int j = it - notes.begin(); j > 0; --j) {
                            new_chord.notes.push_back(notes[j - 1]);
                        }
                        new_chord_events.push_back({chord_event.first, new_chord});
                        it = notes.erase(notes.begin(), it);
                        continue;
                    }
                }
                ++it;
            }
        }
        for (const auto &event : new_chord_events) {
            chords.insert(event);
        }
    }
}

ReducedFraction find_min_duration(const ReducedFraction &on_time, const std::vector<MidiChord> &midi_chords,
                                  const ReducedFraction &length) {
    ReducedFraction len = length;
    for (const auto &chord : midi_chords) {
        for (const auto &note : chord.notes) {
            if ((note.off_time - on_time < len) && (note.off_time - on_time != ReducedFraction(0, 1))) {
                len = note.off_time - on_time;
            }
        }
    }
    return len;
}

void merge_chords_with_equal_on_time_and_voice(std::multimap<int, MTrack> &tracks) {
    for (auto &track : tracks) {
        auto &chords = track.second.chords;
        if (chords.empty()) {
            continue;
        }
        // the key is pair<onTime, voice>
        std::map<std::pair<ReducedFraction, int>, std::multimap<ReducedFraction, MidiChord>::iterator> on_times;

        for (auto it = chords.begin(); it != chords.end();) {
            const auto &on_time = it->first;
            const int voice = it->second.voice;
            auto fit = on_times.find({on_time, voice});
            if (fit == on_times.end()) {
                on_times.insert({{on_time, voice}, it});
            } else {
                auto &old_notes = fit->second->second.notes;
                auto &new_notes = it->second.notes;
                old_notes.insert(old_notes.end(), new_notes.begin(), new_notes.end());
                it = chords.erase(it);
                continue;
            }
            ++it;
        }
    }
}

int chord_average_pitch(const std::vector<MidiNote> &notes, int beg, int end) {
    Q_ASSERT_X(!notes.empty(), "MChord::chordAveragePitch", "Empty notes");
    Q_ASSERT_X(end > 0 && beg >= 0 && end > beg, "MChord::chordAveragePitch", "Invalid note indexes");

    int sum = 0;
    for (int i = beg; i != end; ++i) {
        sum += notes[i].pitch;
    }
    return static_cast<int>(std::lround(sum * 1.0 / (end - beg)));
}

int chord_average_pitch(const std::vector<MidiNote> &notes) {
    Q_ASSERT_X(!notes.empty(), "MChord::chordAveragePitch", "Empty notes");

    return chord_average_pitch(notes, 0, notes.size());
}

// it's an optimization function: we can don't check chords
// with (on time + max chord len) < given time moment
// because chord cannot be longer than found max length

ReducedFraction find_max_chord_length(const std::multimap<ReducedFraction, MidiChord> &chords) {
    ReducedFraction max_chord_length;

    for (const auto &chord : chords) {
        const auto off_time = max_note_off_time(chord.second.notes);
        if (off_time - chord.first > max_chord_length) {
            max_chord_length = off_time - chord.first;
        }
    }
    return max_chord_length;
}

std::vector<std::multimap<ReducedFraction, MidiChord>::const_iterator>
find_chords_for_time_range(int voice, const ReducedFraction &on_time, const ReducedFraction &off_time,
                           const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &max_chord_length) {
    std::vector<std::multimap<ReducedFraction, MidiChord>::const_iterator> result;

    if (chords.empty()) {
        return result;
    }

    auto it = chords.lower_bound(off_time);
    if (it == chords.begin()) {
        return result;
    }
    --it;

    while (it->first + max_chord_length > on_time) {
        const MidiChord &chord = it->second;
        if (chord.voice == voice) {
            const auto chord_interval = std::make_pair(it->first, max_note_off_time(chord.notes));
            const auto duration_interval = std::make_pair(on_time, off_time);

            if (MidiTuplet::have_intersection(chord_interval, duration_interval)) {
                result.push_back(it);
            }
        }
        if (it == chords.begin()) {
            break;
        }
        --it;
    }

    return result;
}

void set_bar_indexes(std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                     const ReducedFraction &last_tick, const engraving::TimeSigMap *sigmap) {
    if (chords.empty()) {
        return;
    }
    auto it = chords.begin();
    for (int bar_index = 0;; ++bar_index) { // iterate over all measures by indexes
        const auto end_bar_tick = ReducedFraction::from_ticks(sigmap->bar2tick(bar_index + 1, 0));
        if (end_bar_tick <= it->first) {
            continue;
        }
        for (; it != chords.end(); ++it) {
            const auto on_time = Quantize::find_quantized_chord_on_time(*it, basic_quant);
#ifndef NDEBUG
            const auto bar_start = ReducedFraction::from_ticks(sigmap->bar2tick(bar_index, 0));
            Q_ASSERT_X(!(it->first >= bar_start && on_time < bar_start), "MChord::setBarIndexes",
                       "quantized on time cannot be in previous bar");
#endif
            if (on_time < end_bar_tick) {
                it->second.bar_index = bar_index;
                continue;
            }
            break;
        }
        if (it == chords.end() || end_bar_tick > last_tick) {
            break;
        }
    }
#ifndef NDEBUG
    Q_ASSERT_X(are_bar_indexes_set(chords), "MChord::setBarIndexes", "Not all bar indexes were set");
    Q_ASSERT_X(are_bar_indexes_successive(chords), "MChord::setBarIndexes", "Bar indexes are not successive");
#endif
}
}} // namespace mu::iex::midi::MChord
