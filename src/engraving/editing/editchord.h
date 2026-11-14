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
#include "../dom/parenthesis.h"
#include "engraving/editing/cmd.h"

namespace mu::engraving {
struct Chord;
struct Note;
struct EngravingItem;
struct Articulation;
enum class SymId;

struct EditChord {
    static void add_chord_parentheses(Chord *chord, std::vector<Note *> notes, bool add_to_linked = true, bool generated = false);
    static void remove_chord_parentheses(Chord *chord, std::vector<Note *> notes, bool add_to_linked = true, bool generated = false);

    static bool toggle_articulation(Score *score, EngravingItem *el, Articulation *a);

private:

    static void undo_add_parentheses_to_notes(Chord *chord, std::vector<Note *> notes, bool add_to_linked = true, bool generated = false);
    static void undo_remove_parentheses_from_note(Chord *chord, Note *note, const NoteParenthesisInfo *group,
                                                  bool remove_from_linked = true);
    static void undo_clear_parenthesis_group(Chord *chord, std::vector<Note *> notes, const NoteParenthesisInfo *group,
                                             bool remove_from_linked = true);
};

void apply_change_span_arpeggio(Chord *chord, Arpeggio *span_arp);

void apply_add_note_parenthesis_info(Chord *chord, NoteParenthesisInfo *note_paren_info);

void apply_remove_note_parenthesis_info(Chord *chord, NoteParenthesisInfo *note_paren_info);

void apply_remove_single_note_parentheses(Chord *chord, Note *note, const NoteParenthesisInfo *group);
} // namespace mu::engraving
