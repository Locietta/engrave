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

#include "../dom/key.h"
#include "../dom/keysig.h"
#include "engraving/editing/cmd.h"

namespace mu::engraving {
struct Score;
struct Staff;
struct Transaction;

void apply_change_key_sig(KeySig *k, KeySigEvent new_key_sig, bool sc, bool add_evt_to_staff = true);

struct EditKeySig {
    static void undo_change_key_sig(Transaction &tx, Score *score, Staff *ostaff, const Fraction &tick, KeySigEvent key);
};
} // namespace mu::engraving
