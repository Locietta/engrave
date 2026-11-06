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
#ifndef IMPORTMIDI_QUANT_H
#define IMPORTMIDI_QUANT_H

#include "importmidi_operation.h"

namespace mu::engraving {
class TimeSigMap;
} // namespace mu::engraving

namespace mu::iex::midi {
class MidiChord;
class MTrack;
class ReducedFraction;

namespace Quantize {
ReducedFraction quant_value_to_fraction(MidiOperations::QuantValue quant_value);
MidiOperations::QuantValue fraction_to_quant_value(const ReducedFraction &fraction);
MidiOperations::QuantValue default_quant_value_from_preferences();

ReducedFraction find_quant_for_range(const std::multimap<ReducedFraction, MidiChord>::const_iterator &beg,
                                     const std::multimap<ReducedFraction, MidiChord>::const_iterator &end,
                                     const ReducedFraction &basic_quant);

ReducedFraction find_quantized_tuplet_chord_on_time(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                    const ReducedFraction &tuplet_len, const ReducedFraction &tuplet_ratio,
                                                    const ReducedFraction &range_start);

ReducedFraction find_quantized_chord_on_time(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant);

// <offTime, resulting quant>
std::pair<ReducedFraction, ReducedFraction>
find_quantized_tuplet_note_off_time(const ReducedFraction &on_time, const ReducedFraction &off_time, const ReducedFraction &tuplet_len,
                                    const ReducedFraction &tuplet_ratio, const ReducedFraction &range_start);

// <offTime, resulting quant>
std::pair<ReducedFraction, ReducedFraction> find_quantized_note_off_time(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                                         const ReducedFraction &off_time,
                                                                         const ReducedFraction &basic_quant);

ReducedFraction find_min_quantized_on_time(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant);

ReducedFraction find_max_quantized_tuplet_off_time(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                   const ReducedFraction &tuplet_len, const ReducedFraction &tuplet_ratio,
                                                   const ReducedFraction &range_start);

ReducedFraction find_max_quantized_off_time(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant);

ReducedFraction find_on_time_tuplet_quant_error(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &tuplet_len,
                                                const ReducedFraction &tuplet_ratio, const ReducedFraction &range_start);

ReducedFraction find_on_time_quant_error(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant);

ReducedFraction find_off_time_tuplet_quant_error(const ReducedFraction &on_time, const ReducedFraction &off_time,
                                                 const ReducedFraction &tuplet_len, const ReducedFraction &tuplet_ratio,
                                                 const ReducedFraction &range_start);

ReducedFraction find_off_time_quant_error(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &off_time,
                                          const ReducedFraction &basic_quant);

void set_if_human_performance(const std::multimap<int, MTrack> &tracks, engraving::TimeSigMap *sigmap);

ReducedFraction quantize_value(const ReducedFraction &value, const ReducedFraction &quant);

ReducedFraction quant_for_len(const ReducedFraction &note_len, const ReducedFraction &basic_quant);

ReducedFraction quantize_to_large(const ReducedFraction &time, const ReducedFraction &quant);

void quantize_chords(std::multimap<ReducedFraction, MidiChord> &chords, const engraving::TimeSigMap *sigmap,
                     const ReducedFraction &basic_quant);
} // namespace Quantize
} // namespace mu::iex::midi

#endif // IMPORTMIDI_QUANT_H
