/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <set>
#include <utility>
#include <vector>

#include "../types/types.h"
#include "draw/types/geometry.h"
#include "global/types/ret.h"

namespace mu::engraving {
struct Chord;
struct ChordRest;
struct Fraction;
struct InputState;
struct Note;
struct Score;
struct Segment;
struct TDuration;
struct Transaction;

struct NoteVal;
struct Position;

enum class AccidentalType : unsigned char;
enum class SymId;

struct NoteInputParams {
    int step = 0;
    int drum_pitch = -1;
};

struct NoteInput {
    //! NOTE: Methods related to entering notes/rests into the score.

    static bool resolve_note_input_params(const Score *score, int note, bool add_flag, NoteInputParams &out);

    static NoteVal note_val(const Score *score, int pitch, staff_idx_t staff_idx, bool allow_transposition);
    static NoteVal note_val_for_position(const Score *score, Position pos, AccidentalType at, bool &error);

    static Note *add_note_to_chord(Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental = false,
                                   const std::set<SymId> &articulation_ids = {});
    static Note *add_note(Transaction &tx, Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental = false,
                          const std::set<SymId> &articulation_ids = {}, InputState *external_input_state = nullptr);
    static Note *add_note_to_tied_chord(Transaction &tx, Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental = false,
                                        const std::set<SymId> &articulation_ids = {});

    static Note *add_pitch(Transaction &tx, Score *score, NoteVal &nval, bool add_flag, InputState *external_input_state = nullptr);
    static Note *add_pitch_to_chord(Transaction &tx, Score *score, NoteVal &nval, Chord *chord, InputState *external_input_state = nullptr,
                                    bool force_accidental = false);

    static Note *add_midi_pitch(Transaction &tx, Score *score, int pitch, bool add_flag, bool allow_transposition);
    static Note *add_tied_midi_pitch(Transaction &tx, Score *score, int pitch, bool add_flag, Chord *prev_chord, bool allow_transposition);

    static std::pair<Note *, Note *> repitch_replace_note(Transaction &tx, Score *score, Chord *chord, const NoteVal &nval,
                                                          bool force_accidental = false);

    static muse::Ret put_note(Transaction &tx, Score *score, const muse::PointF &pos, bool replace, bool insert);
    static muse::Ret put_note(Transaction &tx, Score *score, const Position &pos, bool replace);
    static muse::Ret repitch_note(Transaction &tx, Score *score, const Position &pos, bool replace);
    static muse::Ret insert_chord_by_inserting_time(Transaction &tx, Score *score, const Position &pos);

    static void truncate_chord_rest(Transaction &tx, Score *score, ChordRest *cr, const Fraction &tick, bool fill_with_rest);

    static void next_input_pos(Transaction &tx, Score *score, const ChordRest *cr, bool do_select);

    // Command entry points
    static void add_pitch(Transaction &tx, Score *score, const NoteInputParams &params, bool add_flag, bool insert);
    static void add_pitch(Transaction &tx, Score *score, int step, bool add_flag, bool insert);
    static void add_fret(Transaction &tx, Score *score, int fret);

    static void set_duration(Transaction &tx, Score *score, DurationType duration);
    static void toggle_rest(Transaction &tx, Score *score);
    static void toggle_dots(Transaction &tx, Score *score, int dots, bool toggle_for_selection_only = false);
    static void increase_duration(Transaction &tx, Score *score);
    static void decrease_duration(Transaction &tx, Score *score);

    static void realtime_advance(Transaction &tx, Score *score, bool allow_transposition, const std::vector<int> &active_midi_pitches);

private:
    //! NOTE: Shared prologue/epilogue for the setDuration/toggleRest/toggleDots commands above.
    static bool note_value_change_allowed(const Score *score);
    static void apply_to_selection(Score *score, const TDuration &old_duration, bool old_rest, AccidentalType old_accidental_type,
                                   bool toggle_for_selection_only, bool rest_toggle);
};
} // namespace mu::engraving
