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
#ifndef IMPORTMIDI_TUPLET_H
#define IMPORTMIDI_TUPLET_H

#include <vector>

#include "importmidi_fraction.h"

namespace mu::engraving {
struct TimeSigMap;
struct DurationElement;
} // namespace mu::engraving

namespace mu::iex::midi {
struct MidiChord;
struct MidiNote;
struct MTrack;

namespace MidiTuplet {
struct TupletInfo;

struct TupletData {
    int voice;
    ReducedFraction on_time;
    ReducedFraction len;
    int tuplet_number;
    std::vector<engraving::DurationElement *> elements;
};

struct TupletLimits {
    // ratio - for conversion from tuplet durations to regular durations
    // for example, 8th note in triplet * 3/2 = regular 8th note
    ReducedFraction ratio;
    int min_note_count;
    int min_note_count_add_voice;
    int min_note_count_staccato;
    int min_note_count_human;
};

const TupletLimits &tuplet_limits(int tuplet_number);

void remove_empty_tuplets(MTrack &track);

bool has_non_trivial_chord(const ReducedFraction &chord_on_time, const std::vector<MidiNote> &notes, const ReducedFraction &tuplet_on_time,
                           const ReducedFraction &tuplet_len);

bool is_tuplet_useless(int voice, const ReducedFraction &on_time, const ReducedFraction &len, const ReducedFraction &max_chord_length,
                       const std::multimap<ReducedFraction, MidiChord> &chords);

std::multimap<ReducedFraction, TupletData>::iterator remove_tuplet(const std::multimap<ReducedFraction, TupletData>::iterator &tuplet_it,
                                                                   std::multimap<ReducedFraction, TupletData> &tuplets,
                                                                   const ReducedFraction &max_chord_length,
                                                                   std::multimap<ReducedFraction, MidiChord> &chords);

std::multimap<ReducedFraction, TupletData>::iterator
remove_tuplet_if_empty(const std::multimap<ReducedFraction, TupletData>::iterator &tuplet_it,
                       std::multimap<ReducedFraction, TupletData> &tuplets, const ReducedFraction &max_chord_length,
                       std::multimap<ReducedFraction, MidiChord> &chords);

const TupletInfo &tuplet_from_id(int id, const std::vector<TupletInfo> &tuplets);
TupletInfo &tuplet_from_id(int id, std::vector<TupletInfo> &tuplets);

std::pair<ReducedFraction, ReducedFraction> tuplet_interval(const TupletInfo &tuplet, const ReducedFraction &basic_quant);

std::vector<std::pair<ReducedFraction, ReducedFraction>> find_tuplet_intervals(const std::vector<TupletInfo> &tuplets,
                                                                               const ReducedFraction &basic_quant);

std::vector<TupletData> find_tuplets_in_bar_for_duration(int voice, const ReducedFraction &bar_start_tick,
                                                         const ReducedFraction &duration_on_time, const ReducedFraction &duration_len,
                                                         const std::multimap<ReducedFraction, TupletData> &tuplet_events);

std::vector<std::multimap<ReducedFraction, TupletData>::const_iterator>
find_tuplets_for_time_range(int voice, const ReducedFraction &on_time, const ReducedFraction &len,
                            const std::multimap<ReducedFraction, TupletData> &tuplet_events, bool strict_comparison);

std::multimap<ReducedFraction, TupletData>::const_iterator
find_tuplet_containing_time(int voice, const ReducedFraction &time, const std::multimap<ReducedFraction, TupletData> &tuplet_events,
                            bool strict_comparison);

// Find tuplets and set bar indexes

void find_all_tuplets(std::multimap<ReducedFraction, TupletData> &tuplets, std::multimap<ReducedFraction, MidiChord> &chords,
                      const engraving::TimeSigMap *sigmap, const ReducedFraction &basic_quant);

ReducedFraction find_on_time_between_chords(const std::pair<const ReducedFraction, MidiChord> &chord,
                                            const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                                            const ReducedFraction &bar_start);

#ifndef NDEBUG

bool are_all_tuplets_referenced(const std::multimap<ReducedFraction, MidiChord> &chords,
                                const std::multimap<ReducedFraction, TupletData> &tuplet_events);

bool are_tuplet_references_valid(const std::multimap<ReducedFraction, MidiChord> &chords);

bool is_tuplet_range_ok(const std::pair<const ReducedFraction, MidiChord> &chord,
                        const std::multimap<ReducedFraction, TupletData> &tuplets);

bool are_tuplet_ranges_ok(const std::multimap<ReducedFraction, MidiChord> &chords,
                          const std::multimap<ReducedFraction, TupletData> &tuplets);

bool are_all_tuplets_different(const std::multimap<ReducedFraction, TupletData> &tuplets);

#endif
} // namespace MidiTuplet
} // namespace mu::iex::midi

#endif // IMPORTMIDI_TUPLET_H
