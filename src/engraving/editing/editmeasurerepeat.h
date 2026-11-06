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
struct MeasureRepeat;
struct Score;
struct Transaction;

struct EditMeasureRepeat {
    static void add_measure_repeat(Transaction &tx, Score *score, Measure *first_measure, int num_measures, staff_idx_t staff_idx);
    static bool make_measure_repeat_group(Transaction &tx, Score *score, Measure *first_measure, int num_measures, staff_idx_t staff_idx);

    // Creates one MeasureRepeat at tick of subtype numMeasures, creating a segment if necessary.
    // Does NOT set measureRepeatCount or do anything else with the measure(s)!
    static MeasureRepeat *add_measure_repeat(Transaction &tx, Score *score, const Fraction &tick, track_idx_t track, int num_measures);
};
} // namespace mu::engraving
