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

#include <vector>

#include "../types/types.h"
#include "draw/types/geometry.h"

namespace mu::engraving {
struct ChordRest;
struct Dynamic;
struct Fraction;
struct Hairpin;
struct Score;
struct Transaction;

enum class HairpinType : signed char;

struct EditHairpin {
    static std::vector<Hairpin *> add_hairpins(Transaction &tx, Score *score, HairpinType type);
    static Hairpin *add_hairpin(Transaction &tx, Score *score, HairpinType type, ChordRest *cr1, ChordRest *cr2 = nullptr);
    static Hairpin *add_hairpin(Transaction &tx, Score *score, HairpinType type, Fraction s_tick, Fraction e_tick, track_idx_t track);
    static void add_hairpin(Transaction &tx, Score *score, Hairpin *hairpin, ChordRest *cr1, ChordRest *cr2 = nullptr);
    static void add_hairpin_to_dynamic(Transaction &tx, Score *score, Hairpin *hairpin, Dynamic *dynamic);
    static Hairpin *add_hairpin_to_dynamic_on_grip_drag(Transaction &tx, Score *score, Dynamic *dynamic, bool is_left_grip,
                                                        const muse::PointF &pos);
};
} // namespace mu::engraving
