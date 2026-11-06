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

#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include "global/types/string.h"
#include "importexport/midi/internal/midishared/generalmidi.h"

// The MIDI importer is compiled both by the Qt application and by the Qt-free
// native host (NO_QT_SUPPORT).  Keep the debug macros local to this module
// instead of pulling in QtGlobal for the Qt-free build.  Release builds retain
// the upstream behavior: assertions compile away.
#ifndef Q_ASSERT
#define Q_ASSERT(condition) assert(condition)
#endif
#ifndef Q_ASSERT_X
#define Q_ASSERT_X(condition, where, what) assert(condition)
#endif
#ifndef Q_UNUSED
#define Q_UNUSED(value) (void) (value)
#endif

#include "engraving/types/types.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_operation.h"
#include "importmidi_tuplet.h"

// ---------------------------------------------------------------------------------------
// These inner classes definitions are used in cpp files only
// Include this header to link tests
// ---------------------------------------------------------------------------------------

namespace mu::engraving {
enum class Key : signed char;
struct Staff;
struct Score;
struct TDuration;
struct DurationElement;
struct Measure;
struct KeyList;
} // namespace mu::engraving

namespace mu::iex::midi {
struct MidiTimeSig;

namespace Meter {
// max level for tuplets: duration cannot go over the tuplet boundary
// this level should be greater than any other level
inline constexpr int tuplet_boundary_level = 10;

struct MaxLevel {
    int level = 0;                 // 0 - the biggest, whole bar level; other: -1, -2, ...
    int level_count = 0;           // number of positions with 'level' value
    ReducedFraction pos = {-1, 1}; // first position with value 'level'; -1 - undefined pos
};

struct DivLengthInfo {
    ReducedFraction len;
    int level;
};

struct DivisionInfo {
    ReducedFraction on_time; // division start tick (tick is counted from the beginning of bar)
    ReducedFraction len;     // length of this whole division
    bool is_tuplet = false;
    std::vector<DivLengthInfo> div_lengths; // lengths of 'len' subdivisions
};

enum class DurationType : char;

ReducedFraction user_time_sig_to_fraction(MidiOperations::TimeSigNumerator time_sig_numerator,
                                          MidiOperations::TimeSigDenominator time_sig_denominator);
MidiOperations::TimeSigNumerator fraction_numerator_to_user_value(int n);
MidiOperations::TimeSigDenominator fraction_denominator_to_user_value(int z);
} // namespace Meter

struct MidiTrack;
struct MidiChord;
struct MidiEvent;

struct MTrack {
    // chords store tuplet iterators, so we need to copy class explicitly
    MTrack() = default;
    MTrack(const MTrack &other);
    MTrack &operator=(MTrack other);

    GM1Program program = GM1Program::ACOUSTIC_GRAND_PIANO;
    engraving::Staff *staff = nullptr;
    const MidiTrack *mtrack = nullptr;
    muse::String name;
    bool has_key = false;
    int index_of_operation = 0;
    int division = 0;
    bool is_division_in_tps = false; // ticks per second
    bool had_initial_notes = false;

    std::multimap<ReducedFraction, int> volumes;
    std::multimap<ReducedFraction, MidiChord> chords;
    std::multimap<ReducedFraction, MidiTuplet::TupletData> tuplets; // <tupletOnTime, ...>

    void create_notes(const ReducedFraction &last_tick);
    void process_pending_notes(std::vector<MidiChord> &midi_chords, int voice, const ReducedFraction &start_chord_tick_frac,
                               const ReducedFraction &next_chord_tick);
    void process_meta(int tick, const MidiEvent &mm);
    void fill_gap_with_rests(engraving::Score *score, int voice, const ReducedFraction &start_chord_tick_frac,
                             const ReducedFraction &rest_length, engraving::track_idx_t track);
    std::vector<std::pair<ReducedFraction, engraving::TDuration>> to_duration_list(const engraving::Measure *measure, int voice,
                                                                                   const ReducedFraction &start_tick,
                                                                                   const ReducedFraction &len,
                                                                                   Meter::DurationType duration_type);
    void create_keys(engraving::Key default_key, const engraving::KeyList &all_key_list);
    void update_tuplets_from_chords();

private:
    void update_tuplet(std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator &);
};

namespace MidiTuplet {
struct TupletInfo {
    int id;
    ReducedFraction on_time = {-1, 1}; // invalid
    ReducedFraction len = {-1, 1};
    int tuplet_number = -1;
    // <chord onTime, chord iterator>
    std::map<ReducedFraction, std::multimap<ReducedFraction, MidiChord>::iterator> chords;
    ReducedFraction tuplet_sum_error;
    ReducedFraction regular_sum_error;
    ReducedFraction sum_length_of_rests;
    int first_chord_index = -1;
    std::map<ReducedFraction, int> staccato_chords; // <onTime, note index>
};

bool have_intersection(const std::pair<ReducedFraction, ReducedFraction> &interval1,
                       const std::pair<ReducedFraction, ReducedFraction> &interval2, bool strict_comparison = true);
bool have_intersection(const std::pair<ReducedFraction, ReducedFraction> &interval,
                       const std::vector<std::pair<ReducedFraction, ReducedFraction>> &intervals, bool strict_comparison = true);
} // namespace MidiTuplet

namespace MidiCharset {
// Decodes a NUL-terminated MIDI meta-event payload.  The requested charset comes
// from the optional MIDI-import operations XML; without one it is UTF-8.
// See importmidi_inner.cpp for the Qt-free fallback policy.
muse::String convert_to_charset(const std::string &text);
std::string default_charset();
std::string from_uchar(const unsigned char *text);
} // namespace MidiCharset

namespace MidiBar {
ReducedFraction find_bar_start(const ReducedFraction &time, const engraving::TimeSigMap *sigmap);
} // namespace MidiBar

namespace MidiDuration {
double duration_count(const std::vector<std::pair<ReducedFraction, engraving::TDuration>> &durations);
} // namespace MidiDuration
} // namespace mu::iex::midi
