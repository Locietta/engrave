/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include <functional>

#include "../dom/harmony.h"
#include "../dom/interval.h"

namespace mu::engraving {
struct Transaction;

struct Transpose {
    static bool transpose(Transaction &tx, Score *score, TransposeMode mode, TransposeDirection direction, Key tr_key,
                          int transpose_interval, bool tr_keys, bool transpose_chord_names, bool use_double_sharps_flats);
    static void transpose_keys(Transaction &tx, Score *score, staff_idx_t staff_start, staff_idx_t staff_end, const Fraction &ts,
                               const Fraction &tick_end, bool flip);
    static void transpose_semitone(Transaction &tx, Score *score, int step);
    static void transpose_diatonic_alterations(Transaction &tx, Score *score, TransposeDirection direction);
    static void transposition_changed(Transaction &tx, Score *score, Part *part, Interval old_v, Fraction tick_start = {0, 1},
                                      Fraction tick_end = {-1, 1});
    static void transposition_changed(Transaction &tx, Score *score, Part *part, const Fraction &instrument_tick,
                                      Interval old_transposition);
    static void transpose_chord(Chord *c, const Fraction &tick);

    static int transpose_tpc_diatonic_by_key(int tpc, int steps, Key key, bool keep_altered_degrees, bool use_double_sharps_flats);
    static Key transpose_key(Key old_key, const Interval &interval, PreferSharpFlat prefer = PreferSharpFlat::NONE);
    static int transpose_tpc(int tpc, Interval interval, bool use_double_sharps_flats);

    static String transposed_chord_name(Score *score, const String &name, const std::function<int(int tpc)> &transform_tpc);
    static void undo_transpose_harmony(Transaction &tx, Harmony *harmony, Interval interval, bool double_sharp_flat = true);
    static void do_undo_transpose_harmony(Harmony *harmony, Interval interval, bool double_sharp_flat = true);
    static void do_undo_transpose_harmony_diatonic(Harmony *harmony, int interval, bool double_sharp_flat, bool transpose_keys);

private:
    static void undo_transpose_harmony_diatonic(Transaction &tx, Harmony *harmony, int interval, bool double_sharp_flat,
                                                bool transpose_keys);
    static Interval keydiff2_interval(Key o_key, Key n_key, TransposeDirection dir);

    static bool transpose_note(Note *note, TransposeMode mode, int transpose_interval, bool tr_keys, bool use_double_sharps_flats,
                               Interval interval);
    static void transpose_harmony(Transaction &tx, Harmony *harmony, Score *score, Interval interval, TransposeMode mode,
                                  int transpose_interval, bool tr_keys, bool use_double_sharps_flats);
    static void transpose_fret_diagram(Transaction &tx, FretDiagram *diagram, Score *score, Interval interval, TransposeMode mode,
                                       int transpose_interval, bool tr_keys, bool use_double_sharps_flats);

    static String find_best_enharmonic_fit(const std::vector<String> &notes, Key key, const MStyle &style);
};
} // namespace mu::engraving
