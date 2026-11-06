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

#include "../types/types.h"

namespace mu::engraving {
struct Fraction;
struct Measure;
struct Score;
struct TimeSig;
struct Transaction;

struct EditTimeSig {
    static void add_time_sig(Transaction &tx, Score *score, Measure *fm, staff_idx_t staff_idx, TimeSig *ts, bool local);
    static void remove_time_sig(Transaction &tx, Score *score, TimeSig *ts);

    static bool rewrite_measures(Transaction &tx, Score *score, Measure *start_measure, Measure *end_measure, const Fraction &new_time_sig,
                                 staff_idx_t staff_idx);
    static bool rewrite_measures(Transaction &tx, Score *score, Measure *fm, const Fraction &ns, staff_idx_t staff_idx);
};
} // namespace mu::engraving
