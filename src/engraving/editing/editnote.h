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
#include "transaction/undoablecommand.h"

namespace mu::engraving {
enum class SymId;
enum class AccidentalType : unsigned char;
enum class Key : signed char;
struct Score;
struct EngravingItem;
struct Articulation;

enum class UpDownMode : char { CHROMATIC, OCTAVE, DIATONIC };

struct EditNote {
    static void toggle_ornament(Score *score, SymId attr);
    static void toggle_accidental(Score *score, AccidentalType at);
    static void apply_accidental_to_input_notes(Score *score, AccidentalType accidental_type);
    static void change_accidental(Score *score, AccidentalType idx);
    static void change_accidental(Score *score, Note *note, AccidentalType accidental);
    static void undo_change_pitch(Score *score, Note *note, int pitch, int tpc1, int tpc2);
    static void undo_change_fretting(Score *score, Note *note, int pitch, int string, int fret, int tpc1, int tpc2);
    static void up_down(Score *score, bool up, UpDownMode mode);
private:
    static void change_accidental2(Note *n, int pitch, int tpc);
    static void up_down_chromatic(bool up, int pitch, Note *n, Key key, int tpc1, int tpc2, int &new_pitch, int &new_tpc1, int &new_tpc2);
};

} // namespace mu::engraving
