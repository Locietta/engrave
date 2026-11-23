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

#include <utility>
#include <vector>

#include "../types/types.h"
#include "draw/types/geometry.h"
#include "global/flatset.h"
#include "global/types/ret.h"

namespace mu::engraving {
struct Chord;
struct ChordRest;
struct Fraction;
struct InputState;
struct Note;
struct Score;
struct Segment;
struct TDuration;
struct Transaction;

struct NoteVal;

enum class AccidentalType : unsigned char;
enum class SymId;

struct NoteInput {

    static Note *add_note_to_chord(Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental = false,
                                   const muse::FlatSet<SymId> &articulation_ids = {});
    static Note *add_note(Transaction &tx, Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental = false,
                          const muse::FlatSet<SymId> &articulation_ids = {}, InputState *external_input_state = nullptr);

private:
};
} // namespace mu::engraving
