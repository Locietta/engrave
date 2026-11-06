/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include <unordered_map>
#include <vector>

#include <engraving/types/fraction.h>
#include <engraving/types/types.h>

namespace mu::engraving {
class Note;
class Chord;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
constexpr int gp_pitch_per_quartertone = 25;

// ---- shared types ----

struct SegmentData {
    double start_factor = 0.0;
    double end_factor = 1.0;
    int quarter_tones = 0;
};

// tie between originally tied notes should be removed and instead the bend after last grace note should be placed
struct LastGraceNoteData {
    bool should_move_tie = false;
    double end_factor = 1.0; // grace notes' durations are split equally and they have startFactor 0 (bend starts from the beginning)
};

struct GraceNotesImportInfo {
    std::vector<SegmentData> data;
    LastGraceNoteData last_note_data;
};

using segment_data_map_t = std::map<size_t, SegmentData>;
using grace_segment_map_t = std::map<size_t /* idx in chord */, GraceNotesImportInfo>; // each note can have multiple grace notes connected

using GraceAfterTrackMap = std::unordered_map<mu::engraving::track_idx_t, std::map<mu::engraving::Fraction, grace_segment_map_t>>;

using TiedNotesTrackMap = std::unordered_map<mu::engraving::track_idx_t, std::map<mu::engraving::Fraction, segment_data_map_t>>;

// ---- bend types ----

struct BendChordData {
    mu::engraving::Fraction start_tick;
    segment_data_map_t note_data_by_idx;
};

struct BendSegment {
    int start_time = -1;
    int middle_time = -1;
    int end_time = -1;
    int start_pitch = -1;
    int end_pitch = -1;

    int pitch_diff() const { return end_pitch - start_pitch; }
};

enum class BendType { NORMAL_BEND, PREBEND, HOLD, SLIGHT_BEND };

enum class ConnectionToNextNoteType {
    NONE,               // note doesn't connect to next tied note -> using "grace note after"
    MAIN_NOTE_CONNECTS, // new note isn't added, only adding bend between 2 originally tied notes
    LAST_GRACE_CONNECTS // grace notes are created and last of them connects to next tied note with bend
};

struct ImportedBendInfo {
    const mu::engraving::Note *note = nullptr;
    std::vector<BendSegment> segments;
    BendType type = BendType::NORMAL_BEND;
    int time_offset_from_start = 0;
    int pitch_offset_from_start = 0;

    ConnectionToNextNoteType connection_type = ConnectionToNextNoteType::NONE;
};

struct ChordImportedBendData {
    const mu::engraving::Chord *chord = nullptr;
    std::unordered_map<const mu::engraving::Note *, ImportedBendInfo> data_by_note;
};

using tied_chords_bend_data_chunk_t = std::map<mu::engraving::Fraction, ChordImportedBendData>;

// ---- dive types ----

enum class DiveType {
    NONE,
    DIVE,
    PRE_DIVE,
};

struct ImportedDiveInfo {
    const mu::engraving::Note *note = nullptr;
    DiveType type = DiveType::NONE;
    int quarter_tones = 0;
    double start_factor = 0.0;
    double end_factor = 1.0;
    std::vector<SegmentData> grace_dive_segments;
};

// ---- data contexts ----

struct BendDataContextSplitChord;

struct BendDataContext {
    GraceAfterTrackMap grace_after_bend_data;
    TiedNotesTrackMap tied_notes_bends_data;
    std::unordered_map<mu::engraving::track_idx_t, std::map<mu::engraving::Fraction, segment_data_map_t>> prebend_data;
    std::unordered_map<mu::engraving::track_idx_t, std::map<mu::engraving::Fraction, segment_data_map_t>> slight_bend_data;
    std::unique_ptr<BendDataContextSplitChord> split_chord_ctx;
};

struct DiveDataContext {
    GraceAfterTrackMap grace_after_dive_data;
    TiedNotesTrackMap tied_notes_dives_data;
    std::unordered_map<mu::engraving::track_idx_t, std::map<mu::engraving::Fraction, segment_data_map_t>> pre_dive_data;
};
} // namespace mu::iex::guitarpro
