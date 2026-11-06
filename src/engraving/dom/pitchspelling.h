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

#include "types/string.h"
#include "../types/types.h"

namespace mu::engraving {
struct MidiNote;
struct Note;
enum class Key : signed char;

static constexpr int invalid_pitch = -1;
static constexpr int min_pitch = 0;
static constexpr int max_pitch = 127;

// a list of tpc's, with legal ranges, not really an enum, so no way to convert into a class
enum Tpc : signed char {
    TPC_INVALID = -9,
    TPC_F_BBB,
    TPC_C_BBB,
    TPC_G_BBB,
    TPC_D_BBB,
    TPC_A_BBB,
    TPC_E_BBB,
    TPC_B_BBB,
    TPC_F_BB,
    TPC_C_BB,
    TPC_G_BB,
    TPC_D_BB,
    TPC_A_BB,
    TPC_E_BB,
    TPC_B_BB,
    TPC_F_B,
    TPC_C_B,
    TPC_G_B,
    TPC_D_B,
    TPC_A_B,
    TPC_E_B,
    TPC_B_B,
    TPC_F,
    TPC_C,
    TPC_G,
    TPC_D,
    TPC_A,
    TPC_E,
    TPC_B,
    TPC_F_S,
    TPC_C_S,
    TPC_G_S,
    TPC_D_S,
    TPC_A_S,
    TPC_E_S,
    TPC_B_S,
    TPC_F_SS,
    TPC_C_SS,
    TPC_G_SS,
    TPC_D_SS,
    TPC_A_SS,
    TPC_E_SS,
    TPC_B_SS,
    TPC_F_SSS,
    TPC_C_SSS,
    TPC_G_SSS,
    TPC_D_SSS,
    TPC_A_SSS,
    TPC_E_SSS,
    TPC_B_SSS,
    TPC_MIN = TPC_F_BBB,
    TPC_MAX = TPC_B_SSS
};

static constexpr int tpc_delta_semitone = 7;    // the delta in tpc value to go 1 semitone up or down
static constexpr int tpc_delta_enharmonic = 12; // the delta in tpc value to reach the next (or prev) enharmonic spelling
//static constexpr int TPC_FIRST_STEP       = 3;  // the step of the first valid tpc (= F = step 3)
static constexpr int pitch_delta_octave = 12; // the delta in pitch value to go 1 octave up or down
static constexpr int step_delta_octave = 7;   // the number of steps in an octave
//static constexpr int STEP_DELTA_TPC       = 4;  // the number of steps in a tpc step (= a fifth = 4 steps)
static constexpr int tpcs_per_step = (Tpc::TPC_MAX - Tpc::TPC_MIN + 1) / ::mu::engraving::step_delta_octave;
static constexpr int min_step = ::mu::engraving::min_pitch * ::mu::engraving::step_delta_octave / ::mu::engraving::pitch_delta_octave;
static constexpr int max_step = ::mu::engraving::max_pitch * ::mu::engraving::step_delta_octave / ::mu::engraving::pitch_delta_octave;
static constexpr int key_to_tpc_offset = (int) Tpc::TPC_C - (int) Key::C;

//---------------------------------------------------------
//   pitch2tpc
//    muse::Returns a default tpc for a given midi pitch.
//    Midi pitch 60 is middle C.
//---------------------------------------------------------

// pitch2tpc(pitch) replaced by pitch2tpc(pitch, Key::C, Prefer::NEAREST)

enum class Prefer : char { FLATS = 8, NEAREST = 11, SHARPS = 13 };
enum class NoteCaseType : signed char { AUTO = -1, CAPITAL = 0, LOWER, UPPER };

extern int pitch2tpc(int pitch, Key, Prefer prefer);

extern String tpc2name(int tpc, NoteSpellingType spelling, NoteCaseType note_case, bool explicit_accidental = false, bool full = false);
extern void tpc2name(int tpc, NoteSpellingType note_spelling, NoteCaseType note_case, String &s, String &acc,
                     bool explicit_accidental = false, bool full = false);
extern void tpc2name(int tpc, NoteSpellingType note_spelling, NoteCaseType note_case, String &s, AccidentalVal &acc);
extern String tpc_user_name(int tpc, int pitch, bool explicit_accidental = false, bool full = false);
extern int step2tpc(const String &step_name, AccidentalVal alter);
extern int step2tpc(int step);
extern int step2tpc(int step, AccidentalVal alter);
extern int step2tpc_by_key(int step, Key);
extern int tpc2pitch(int tpc);
extern int tpc2step(int tpc);
extern int tpc2step_by_key(int tpc, Key, int &alter);
extern int tpc2alter_by_key(int tpc, Key);
extern int pitch2abs_step_by_key(int pitch, int tpc, Key, int &alter);
extern int abs_step2pitch_by_key(int step, Key);
extern int tpc2degree(int tpc, Key key);
extern int tpc_interval(int start_tpc, int interval, int alter);
extern int step2pitch_interval(int step, int alter);
extern String tpc2_function(int tpc, Key key);
extern void tpc2_function(int tpc, Key key, String &acc_name, String &step_name);
extern int function2_tpc(const String &s, Key key);
extern int function2_tpc(const String &s, Key key, size_t &idx);
extern int convert_note(const String &s, NoteSpellingType note_spelling, NoteCaseType &note_case, size_t &idx);
extern int clamp_enharmonic(int tpc, bool use_double_sharps_flats = true);
extern int clamp_pitch(int pitch);
extern int clamp_pitch_octaved(int pitch);
extern Key clamp_key(Key key, PreferSharpFlat prefer = PreferSharpFlat::NONE);
extern int key2_tpc(Key key);
extern Key tpc2_key(int tpc);

//---------------------------------------------------------
//   tpc2alter
//---------------------------------------------------------

inline static AccidentalVal tpc2alter(int tpc) {
    return AccidentalVal(((tpc - Tpc::TPC_MIN) / ::mu::engraving::tpc_delta_semitone) + int(AccidentalVal::MIN));
}

inline static int playing_octave(int pitch, int tpc) {
    return ((pitch - static_cast<int>(tpc2alter(tpc))) / ::mu::engraving::pitch_delta_octave) - 1;
}

extern Char tpc2step_name(int tpc);
extern bool tpc_is_valid(int val);

inline bool pitch_is_valid(int pitch) { return pitch >= ::mu::engraving::min_pitch && pitch <= ::mu::engraving::max_pitch; }
} // namespace mu::engraving
