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
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct Chord;
struct Note;
struct EngravingItem;
struct Articulation;
enum class SymId;

struct EditChord {
    static void add_chord_parentheses(Chord *chord, std::vector<Note *> notes, bool add_to_linked = true, bool generated = false);
    static void remove_chord_parentheses(Chord *chord, std::vector<Note *> notes, bool add_to_linked = true, bool generated = false);
    static void toggle_articulation(Score *score, SymId attr);
    static bool toggle_articulation(Score *score, EngravingItem *el, Articulation *a);

private:

    static void undo_add_parentheses_to_notes(Chord *chord, std::vector<Note *> notes, bool add_to_linked = true, bool generated = false);
    static void undo_remove_parentheses_from_note(Chord *chord, Note *note, Parenthesis *left_paren, bool remove_from_linked = true);
    static void undo_clear_parenthesis_group(Chord *chord, std::vector<Note *> notes, Parenthesis *left_paren, Parenthesis *right_paren,
                                             bool remove_from_linked = true);

    static void do_add_note_parentheses(Chord *chord, std::vector<Note *> notes, Parenthesis *left_paren, Parenthesis *right_paren);
    static void do_remove_single_note_parenthesis(Chord *chord, Note *note, Parenthesis *left_paren);
    static void do_remove_all_note_parentheses(Chord *chord, Parenthesis *left_paren);
};

struct ChangeChordStaffMove : UndoableCommand {
private:

    ChordRest *chord_rest = nullptr;
    int staff_move = 0;

    void flip() override;

public:
    ChangeChordStaffMove(ChordRest *cr, int v) : chord_rest(cr), staff_move(v) {}

    UNDO_TYPE(CommandType::CHANGE_CHORD_STAFF_MOVE)
    UNDO_NAME("ChangeChordStaffMove")
    UNDO_CHANGED_OBJECTS({chord_rest})
};

struct SwapCR : UndoableCommand {
private:

    ChordRest *cr1 = nullptr;
    ChordRest *cr2 = nullptr;

    void flip() override;

public:
    SwapCR(ChordRest *a, ChordRest *b) : cr1(a), cr2(b) {}

    UNDO_TYPE(CommandType::SWAP_CR)
    UNDO_NAME("SwapCR")
    UNDO_CHANGED_OBJECTS({cr1, cr2})
};

struct ChangeSpanArpeggio : UndoableCommand {
private:

    Chord *chord_ = nullptr;
    Arpeggio *span_arpeggio_ = nullptr;

    void flip() override;
public:
    ChangeSpanArpeggio(Chord *chord, Arpeggio *span_arp) : chord_(chord), span_arpeggio_(span_arp) {}

    UNDO_NAME("ChangeSpanArpeggio")
    UNDO_CHANGED_OBJECTS({chord_})
};

struct AddNoteParenthesisInfo : UndoableCommand {
private:

    void redo() override;
    void undo() override;

    void cleanup(bool was_done) override;

    Chord *chord_ = nullptr;
    NoteParenthesisInfo *note_paren_info_ = nullptr;

public:
    AddNoteParenthesisInfo(Chord *chord, NoteParenthesisInfo *note_paren_info) : chord_(chord), note_paren_info_(note_paren_info) {}

    UNDO_NAME("AddNoteParenthesesInfo")
    UNDO_TYPE(CommandType::ADD_NOTE_PARENTHESES_INFO)
};

struct RemoveNoteParenthesisInfo : UndoableCommand {
private:

    void redo() override;
    void undo() override;

    void cleanup(bool was_done) override;

    Chord *chord_ = nullptr;
    NoteParenthesisInfo *note_paren_info_ = nullptr;

public:
    RemoveNoteParenthesisInfo(Chord *chord, NoteParenthesisInfo *note_paren_info) : chord_(chord), note_paren_info_(note_paren_info) {}

    UNDO_NAME("RemoveNoteParenthesesInfo")
    UNDO_TYPE(CommandType::REMOVE_NOTE_PARENTHESES_INFO)
};

struct RemoveSingleNoteParentheses : UndoableCommand {
private:

    Chord *chord_ = nullptr;
    Note *note_ = nullptr;
    Parenthesis *paren_ = nullptr;

    void redo() override;
    void undo() override;

public:
    RemoveSingleNoteParentheses(Chord *chord, Note *note, Parenthesis *paren) : chord_(chord), note_(note), paren_(paren) {}

    UNDO_NAME("RemoveSingleNoteParentheses")
    UNDO_TYPE(CommandType::REMOVE_SINGLE_NOTE_PARENTHESES)
    UNDO_CHANGED_OBJECTS({chord_, paren_})
};
} // namespace mu::engraving
