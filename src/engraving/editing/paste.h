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

#include "../types/fraction.h"
#include "../types/types.h"

namespace muse {
struct ByteArray;
} // namespace muse

namespace mu::engraving {
struct ChordRest;
struct Score;
struct Segment;
struct Transaction;
struct XmlReader;

struct Paste {

    static bool paste_staff_list(Transaction &tx, Score *score, muse::ByteArray &data, Fraction scale = Fraction(1, 1));
    static bool paste_symbol_list(Transaction &tx, Score *score, muse::ByteArray &data);

    static bool paste_staff(Transaction &tx, Score *score, XmlReader &e, Segment *dst, staff_idx_t staff_idx,
                            Fraction scale = Fraction(1, 1));
    static void paste_symbols(Transaction &tx, XmlReader &e, ChordRest *dst);

    static void paste_chord_rest(Transaction &tx, Score *score, ChordRest *cr, const Fraction &tick);

    static bool repeat_list_selection(Transaction &tx, Score *score);
};
} // namespace mu::engraving
