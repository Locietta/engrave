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
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct Score;
struct Staff;
struct Transaction;

struct ChangeKeySig : UndoableCommand {
private:

    KeySig *keysig = nullptr;
    KeySigEvent ks;
    bool show_courtesy = false;
    bool evt_in_staff = false;

    void flip() override;

public:
    ChangeKeySig(KeySig *k, KeySigEvent new_key_sig, bool sc, bool add_evt_to_staff = true);

    UNDO_TYPE(CommandType::CHANGE_KEY_SIG)
    UNDO_NAME("ChangeKeySig")
    UNDO_CHANGED_OBJECTS({keysig})
};

struct EditKeySig {
    static void undo_change_key_sig(Transaction &tx, Score *score, Staff *ostaff, const Fraction &tick, KeySigEvent key);
};
} // namespace mu::engraving
