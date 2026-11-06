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
#ifndef IMPORTMIDI_METER_H
#define IMPORTMIDI_METER_H

#include <utility>
#include <vector>

namespace mu::engraving {
struct TDuration;
} // namespace mu::engraving

namespace mu::iex::midi {
struct ReducedFraction;

namespace MidiTuplet {
struct TupletData;
} // namespace MidiTuplet

namespace Meter {
enum class DurationType : char { NOTE, REST };

bool is_simple(const ReducedFraction &bar_fraction);
bool is_compound(const ReducedFraction &bar_fraction);
bool is_complex(const ReducedFraction &bar_fraction);
bool is_duple(const ReducedFraction &bar_fraction);
bool is_triple(const ReducedFraction &bar_fraction);
bool is_quadruple(const ReducedFraction &bar_fraction);
bool is_quintuple(const ReducedFraction &bar_fraction);
bool is_septuple(const ReducedFraction &bar_fraction);

ReducedFraction beat_length(const ReducedFraction &bar_fraction);

struct DivisionInfo;

DivisionInfo metric_divisions_of_bar(const ReducedFraction &bar_fraction);
DivisionInfo metric_divisions_of_tuplet(const MidiTuplet::TupletData &tuplet, int tuplet_start_level);

// result in vector: first elements - all tuplets info, one at the end - bar division info
std::vector<DivisionInfo> division_info(const ReducedFraction &bar_fraction, const std::vector<MidiTuplet::TupletData> &tuplets_in_bar);

// tick is counted from the beginning of bar
int level_of_tick(const ReducedFraction &tick, const std::vector<DivisionInfo> &divs_info);

std::vector<int> metric_levels_of_bar(const ReducedFraction &bar_fraction, const std::vector<DivisionInfo> &divs_info,
                                      const ReducedFraction &min_duration);

bool is_simple_note_duration(const ReducedFraction &duration); // quarter, half, eighth, 16th ...

// division lengths of bar, each can be a tuplet length
std::vector<ReducedFraction> divisions_of_bar_for_tuplets(const ReducedFraction &bar_fraction);

// duration and all tuplets should belong to the same voice
// nested tuplets are not allowed
std::vector<std::pair<ReducedFraction, engraving::TDuration>>
to_duration_list(const ReducedFraction &start_tick_in_bar, const ReducedFraction &end_tick_in_bar, const ReducedFraction &bar_fraction,
                 const std::vector<MidiTuplet::TupletData> &tuplets_in_bar, DurationType duration_type, bool use_dots,
                 bool print_rest_remains = true);
} // namespace Meter
} // namespace mu::iex::midi

#endif // IMPORTMIDI_METER_H
