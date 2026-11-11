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

struct ChangeVelocity : UndoableCommand {
private:

    Note *note = nullptr;
    int user_velocity = 0;

    void flip() override;

public:
    ChangeVelocity(Note *, int);

    UNDO_TYPE(CommandType::CHANGE_VELOCITY)
    UNDO_NAME("ChangeVelocity")
    UNDO_CHANGED_OBJECTS({note})
};

struct ChangeNoteEventList : UndoableCommand {
private:

    Note *note = nullptr;
    NoteEventList new_events;
    PlayEventType new_petype;

    void flip() override;

public:
    ChangeNoteEventList(Note *n, NoteEventList &ne) : note(n), new_events(ne), new_petype(PlayEventType::USER) {}
    UNDO_NAME("ChangeNoteEventList")
    UNDO_CHANGED_OBJECTS({note});
};

struct ChangeNoteEvent : UndoableCommand {
private:

    Note *note = nullptr;
    NoteEvent *old_event = nullptr;
    NoteEvent new_event;
    PlayEventType new_petype;

    void flip() override;

public:
    ChangeNoteEvent(Note *n, NoteEvent *oe, const NoteEvent &ne) : note(n), old_event(oe), new_event(ne), new_petype(PlayEventType::USER) {}
    UNDO_NAME("ChangeNoteEvent")
    UNDO_CHANGED_OBJECTS({note})
};

struct ChangeChordPlayEventType : UndoableCommand {
private:

    Chord *chord = nullptr;
    PlayEventType petype;
    std::vector<NoteEventList> events;

    void flip() override;

public:
    ChangeChordPlayEventType(Chord *c, PlayEventType pet) : chord(c), petype(pet) { events = c->get_note_event_lists(); }

    UNDO_NAME("ChangeChordPlayEventType")
    UNDO_CHANGED_OBJECTS({chord});
};
} // namespace mu::engraving
