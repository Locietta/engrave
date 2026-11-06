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
#ifndef IMPORTMIDI_TUPLET_VOICE_H
#define IMPORTMIDI_TUPLET_VOICE_H

#include <vector>
#include <map>
#include <list>

namespace mu::iex::midi {
class ReducedFraction;
class MidiChord;

namespace MidiTuplet {
struct TupletInfo;

struct TiedTuplet {
    int tuplet_id;
    int voice;
    std::pair<const ReducedFraction, MidiChord> *chord; // chord the tuplet is tied with
    std::vector<int> tied_note_indexes;                 // indexes of tied notes of that chord
};

int tuplet_voice_limit();

bool exclude_extra_voice_tuplets(std::vector<TupletInfo> &tuplets,
                                 std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                                 std::list<TiedTuplet> &back_tied_tuplets, const std::multimap<ReducedFraction, MidiChord> &chords,
                                 const ReducedFraction &basic_quant, const ReducedFraction &bar_start, int bar_index);

std::list<TiedTuplet> find_back_tied_tuplets(const std::multimap<ReducedFraction, MidiChord> &chords,
                                             const std::vector<TupletInfo> &tuplets, const ReducedFraction &prev_bar_start,
                                             const ReducedFraction &start_bar_tick, const ReducedFraction &basic_quant,
                                             int current_bar_index);

void assign_voices(std::vector<TupletInfo> &tuplets, std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                   std::list<TiedTuplet> &back_tied_tuplets, const std::multimap<ReducedFraction, MidiChord> &chords,
                   const ReducedFraction &basic_quant, const ReducedFraction &bar_start, int bar_index);

std::pair<ReducedFraction, ReducedFraction> chord_interval(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                           const std::multimap<ReducedFraction, MidiChord> &chords,
                                                           const ReducedFraction &basic_quant, const ReducedFraction &bar_start);

#ifndef NDEBUG

bool have_overlapping_voices(const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                             const std::vector<TupletInfo> &tuplets, const std::list<TiedTuplet> &back_tied_tuplets,
                             const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                             const ReducedFraction &bar_start);

#endif
} // namespace MidiTuplet
} // namespace mu::iex::midi

#endif // IMPORTMIDI_TUPLET_VOICE_H
