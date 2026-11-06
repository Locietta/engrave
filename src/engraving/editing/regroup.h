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
struct Score;
struct Transaction;

struct Regroup {
    /// Combine consecutive rests into fewer rests of longer duration, and tied
    /// notes/chords into fewer notes of longer duration. Only operates on one voice.
    static void regroup_notes_and_rests(Transaction &tx, Score *score, const Fraction &start_tick, const Fraction &end_tick,
                                        track_idx_t track);

    /// Regroups notes and rests in the current selection (or the whole score if nothing
    /// is selected), restoring the original selection afterwards.
    static void regroup_notes_and_rests_in_selection(Transaction &tx, Score *score);
};
} // namespace mu::engraving
