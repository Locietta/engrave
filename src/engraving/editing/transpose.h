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

    static void transpose_keys(Transaction &tx, Score *score, staff_idx_t staff_start, staff_idx_t staff_end, const Fraction &ts,
                               const Fraction &tick_end, bool flip);

    static void transposition_changed(Transaction &tx, Score *score, Part *part, Interval old_v, Fraction tick_start = {0, 1},
                                      Fraction tick_end = {-1, 1});

    static Key transpose_key(Key old_key, const Interval &interval, PreferSharpFlat prefer = PreferSharpFlat::NONE);
    static int transpose_tpc(int tpc, Interval interval, bool use_double_sharps_flats);

    static void undo_transpose_harmony(Transaction &tx, Harmony *harmony, Interval interval, bool double_sharp_flat = true);
    static void do_undo_transpose_harmony(Harmony *harmony, Interval interval, bool double_sharp_flat = true);
};
} // namespace mu::engraving
