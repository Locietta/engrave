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
#ifndef IMPORTMIDI_VOICE_H
#define IMPORTMIDI_VOICE_H

#include <ankerl/unordered_dense.h>
#include <gtl/btree.hpp>

#include "importmidi_operation.h"
#include "importmidi_timeline.h"

namespace mu::engraving {
struct TimeSigMap;
} // namespace mu::engraving

namespace mu::iex::midi {
struct MTrack;
struct MidiChord;

namespace MidiTuplet {
struct TupletData;
} // namespace MidiTuplet

namespace MidiVoice {
size_t to_int_voice_count(MidiOperations::VoiceCount value);
int voice_limit();
bool separate_voices(gtl::btree_multimap<int, MTrack> &tracks, const engraving::TimeSigMap *sigmap);

bool split_chord_to_voice(MidiTimeline<MidiChord>::iterator &chord_it, const ankerl::unordered_dense::set<int> &notes_to_move,
                          int new_voice, MidiTimeline<MidiChord> &chords, MidiTimeline<MidiTuplet::TupletData> &tuplets,
                          gtl::btree_multimap<ReducedFraction, MidiTimeline<MidiTuplet::TupletData>::iterator> &inserted_tuplets,
                          const ReducedFraction &max_chord_length, bool allow_parallel_tuplets = false);

#ifndef NDEBUG

bool are_voices_same(const MidiTimeline<MidiChord> &chords);

#endif
} // namespace MidiVoice
} // namespace mu::iex::midi

#endif // IMPORTMIDI_VOICE_H
