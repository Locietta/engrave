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

#include "../dom/chord.h"
#include "../dom/note.h"
#include "engraving/editing/cmd.h"

namespace mu::engraving {
enum class SymId;
enum class AccidentalType : unsigned char;
enum class Key : signed char;
struct Score;
struct EngravingItem;
struct Articulation;

struct EditNote {

    static void change_accidental(Score *score, Note *note, AccidentalType accidental);
    static void undo_change_pitch(Score *score, Note *note, int pitch, int tpc1, int tpc2);

private:
    static void change_accidental2(Note *n, int pitch, int tpc);
};

} // namespace mu::engraving
