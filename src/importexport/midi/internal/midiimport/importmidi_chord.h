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

#include <map>
#include <vector>

#include "importexport/midi/internal/midishared/generalmidi.h"

#include "importmidi_fraction.h"
#include "importmidi_tuplet.h"

namespace mu::engraving {
struct Tie;
struct TimeSigMap;
} // namespace mu::engraving

namespace mu::iex::midi {
struct MidiNote {
    int pitch = 0;
    int velo = 0;
    ReducedFraction off_time;
    engraving::Tie *tie = nullptr;
    bool staccato = false;
    bool is_in_tuplet = false;
    // for offTime quantization
    std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator tuplet;
    // for notation simplification - final quant value
    ReducedFraction off_time_quant = ReducedFraction(-1, 1); // invalid by default
    // to assign lyrics
    ReducedFraction orig_on_time;
};

struct MidiChord {
    int voice = 0;
    std::vector<MidiNote> notes;
    bool is_in_tuplet = false;
    int bar_index = -1;
    // for onTime quantization
    std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator tuplet;

    bool is_staccato() const {
        for (const auto &note : notes) {
            if (note.staccato) {
                return true;
            }
        }
        return false;
    }
};

struct MTrack;

namespace MChord {
bool is_grand_staff_program(GM1Program);

std::multimap<ReducedFraction, MidiChord>::iterator find_first_chord_in_range(std::multimap<ReducedFraction, MidiChord> &chords,
                                                                              const ReducedFraction &start_range_tick,
                                                                              const ReducedFraction &end_range_tick);

std::multimap<ReducedFraction, MidiChord>::const_iterator find_first_chord_in_range(const std::multimap<ReducedFraction, MidiChord> &chords,
                                                                                    const ReducedFraction &start_range_tick,
                                                                                    const ReducedFraction &end_range_tick);

template <typename Iter>
Iter find_first_chord_in_range(const ReducedFraction &start_range_tick, const ReducedFraction &end_range_tick, const Iter &start_chord_it,
                               const Iter &end_chord_it) {
    auto it = start_chord_it;
    for (; it != end_chord_it; ++it) {
        if (it->first >= start_range_tick) {
            if (it->first >= end_range_tick) {
                it = end_chord_it;
            }
            break;
        }
    }
    return it;
}

template <typename Iter>
Iter find_end_chord_in_range(const ReducedFraction &end_range_tick, const Iter &start_chord_it, const Iter &end_chord_it) {
    auto it = start_chord_it;
    for (; it != end_chord_it; ++it) {
        if (it->first >= end_range_tick) {
            break;
        }
    }
    return it;
}

ReducedFraction min_note_off_time(const std::vector<MidiNote> &notes);
ReducedFraction max_note_off_time(const std::vector<MidiNote> &notes);
ReducedFraction min_note_len(const std::pair<const ReducedFraction, MidiChord> &chord);
ReducedFraction max_note_len(const std::pair<const ReducedFraction, MidiChord> &chord);

const ReducedFraction &min_allowed_duration();
ReducedFraction find_min_duration(const ReducedFraction &on_time, const std::vector<MidiChord> &midi_chords, const ReducedFraction &length);
void sort_notes_by_pitch(std::multimap<ReducedFraction, MidiChord> &chords);
void sort_notes_by_length(std::multimap<ReducedFraction, MidiChord> &chords);

void collect_chords(std::multimap<int, MTrack> &tracks, const ReducedFraction &human_tol_coeff, const ReducedFraction &non_human_tol_coeff);

void collect_chords(MTrack &track, const ReducedFraction &human_tol_coeff, const ReducedFraction &non_human_tol_coeff);

void remove_overlapping_notes(std::multimap<int, MTrack> &tracks);
void merge_chords_with_equal_on_time_and_voice(std::multimap<int, MTrack> &tracks);
void split_unequal_chords(std::multimap<int, MTrack> &tracks);
int chord_average_pitch(const std::vector<MidiNote> &notes, int beg, int end);
int chord_average_pitch(const std::vector<MidiNote> &notes);

ReducedFraction find_max_chord_length(const std::multimap<ReducedFraction, MidiChord> &chords);

std::vector<std::multimap<ReducedFraction, MidiChord>::const_iterator>
find_chords_for_time_range(int voice, const ReducedFraction &on_time, const ReducedFraction &off_time,
                           const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &max_chord_length);

void set_bar_indexes(std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                     const ReducedFraction &last_tick, const engraving::TimeSigMap *sigmap);

#ifndef NDEBUG

bool are_on_time_values_different(const std::multimap<ReducedFraction, MidiChord> &chords);
bool are_bar_indexes_successive(const std::multimap<ReducedFraction, MidiChord> &chords);
bool are_notes_long_enough(const std::multimap<ReducedFraction, MidiChord> &chords);
bool is_last_tick_valid(const ReducedFraction &last_tick, const std::multimap<ReducedFraction, MidiChord> &chords);
bool is_last_tick_valid(const ReducedFraction &last_tick, const std::multimap<int, MTrack> &tracks);
bool are_bar_indexes_set(const std::multimap<ReducedFraction, MidiChord> &chords);

#endif
} // namespace MChord
} // namespace mu::iex::midi
