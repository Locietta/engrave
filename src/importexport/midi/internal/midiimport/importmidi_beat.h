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
#ifndef IMPORTMIDI_BEAT_H
#define IMPORTMIDI_BEAT_H

#include <map>
#include <set>

namespace mu::engraving {
struct TimeSigMap;
} // namespace mu::engraving

namespace mu::iex::midi {
struct ReducedFraction;
struct MidiChord;
struct MTrack;

namespace MidiBeat {
void remove_every2nd_beat(std::set<ReducedFraction> &beat_set);
void find_beat_locations(const std::multimap<ReducedFraction, MidiChord> &all_chords, engraving::TimeSigMap *sigmap, double ticks_per_sec);

void adjust_chords_to_beats(std::multimap<int, MTrack> &tracks);
void set_time_signature(engraving::TimeSigMap *sigmap);
} // namespace MidiBeat
} // namespace mu::iex::midi

#endif // IMPORTMIDI_BEAT_H
