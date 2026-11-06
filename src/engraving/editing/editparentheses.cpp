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

#include "editparentheses.h"

#include <algorithm>
#include <map>
#include <set>

#include "editchord.h"

#include "../dom/accidental.h"
#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/select.h"
#include "../dom/timesig.h"
#include "../dom/utils.h"

#include "log.h"

using namespace mu::engraving;

namespace {
struct NoteComparator {
    bool operator()(const Note *n1, const Note *n2) const {
        if (note_is_before(n1, n2)) {
            return true;
        }
        if (note_is_before(n2, n1)) {
            return false;
        }
        // tiebreak by pointer address so we can have unison notes in the set
        return n1 < n2;
    }
};

static std::map<Chord *, std::set<Note *, NoteComparator>> get_notes_by_chord(const std::vector<Note *> &notes) {
    // Return map of notes by chord
    // Include all notes between highest and lowest in each chord
    std::map<Chord *, std::set<Note *, NoteComparator>> notes_by_chord;
    for (Note *note_to_add : notes) {
        Chord *chord = note_to_add->chord();
        auto notes_by_chord_it = notes_by_chord.find(chord);

        if (notes_by_chord_it == notes_by_chord.end()) {
            notes_by_chord.emplace(chord, std::set<Note *, NoteComparator>{note_to_add});
            continue;
        }

        // Add all notes between last note and this one
        const Note *prev_note = *notes_by_chord_it->second.rbegin();

        const Note *first_note = note_is_before(note_to_add, prev_note) ? note_to_add : prev_note;
        const Note *second_note = first_note == note_to_add ? prev_note : note_to_add;

        const std::vector<Note *> &chord_notes = chord->notes();

        std::vector<Note *>::const_iterator first_note_it = std::find(chord_notes.begin(), chord_notes.end(), first_note);
        std::vector<Note *>::const_iterator second_note_it = std::find(chord_notes.begin(), chord_notes.end(), second_note);

        assert(first_note_it != chord_notes.end() && second_note_it != chord_notes.end());

        for (std::vector<Note *>::const_iterator chord_note_it = first_note_it; chord_note_it != std::next(second_note_it);
             chord_note_it = std::next(chord_note_it)) {
            Note *note = *chord_note_it;
            notes_by_chord_it->second.insert(note);
        }
    }

    return notes_by_chord;
}
} // namespace

void EditParentheses::toggle_parentheses_on_notes(Transaction &tx, Score *score) {
    std::vector<Note *> notes = score->selection().unique_notes(muse::nidx, false);

    bool add = false;

    for (Note *note : notes) {
        if (note->get_property(Pid::HAS_PARENTHESES).value<ParenthesesMode>() == ParenthesesMode::NONE) {
            add = true;
            break;
        }
    }

    if (add) {
        add_parentheses_to_notes(tx, notes);
    } else {
        remove_parentheses_from_notes(tx, notes);
    }
}

void EditParentheses::add_parentheses_to_notes(Transaction &, const std::vector<Note *> &notes) {
    std::map<Chord *, std::set<Note *, NoteComparator>> notes_by_chord = get_notes_by_chord(notes);

    for (auto &[chord, note_set] : notes_by_chord) {
        std::vector<Note *> note_vec(note_set.begin(), note_set.end());

        for (Note *note : note_vec) {
            // User has overriden generated parentheses
            note->undo_change_property(Pid::HIDE_GENERATED_PARENTHESES, true);
            note->undo_change_property(Pid::HAS_PARENTHESES, ParenthesesMode::BOTH);
        }

        EditChord::remove_chord_parentheses(chord, note_vec);
        EditChord::add_chord_parentheses(chord, std::move(note_vec));
    }
}

void EditParentheses::add_parentheses_to_notes(Transaction &tx, Score *score) {
    add_parentheses_to_notes(tx, score->selection().unique_notes(muse::nidx, false));
}

void EditParentheses::remove_parentheses_from_notes(Transaction &, const std::vector<Note *> &notes) {
    std::map<Chord *, std::set<Note *, NoteComparator>> notes_by_chord = get_notes_by_chord(notes);

    for (auto &[chord, note_set] : notes_by_chord) {
        std::vector<Note *> note_vec(note_set.begin(), note_set.end());

        for (Note *note : note_vec) {
            note->undo_change_property(Pid::HAS_PARENTHESES, ParenthesesMode::NONE);

            // User has overriden generated parentheses
            note->undo_change_property(Pid::HIDE_GENERATED_PARENTHESES, true);
        }

        EditChord::remove_chord_parentheses(chord, std::move(note_vec));
    }
}

void EditParentheses::remove_parentheses_from_notes(Transaction &tx, Score *score) {
    remove_parentheses_from_notes(tx, score->selection().unique_notes(muse::nidx, false));
}

void EditParentheses::toggle_parentheses(Transaction &tx, Score *score) {
    toggle_parentheses_on_notes(tx, score);

    for (EngravingItem *el : score->selection().elements()) {
        if (el->is_note()) {
            continue;
        }
        toggle_parentheses_on_item(tx, el);
    }
}

void EditParentheses::toggle_parentheses_on_item(Transaction &, EngravingItem *el) {
    if (el->is_accidental()) {
        Accidental *acc = to_accidental(el);
        acc->undo_change_property(Pid::ACCIDENTAL_BRACKET, int(AccidentalBracket::PARENTHESIS));
    } else if (el->type() == ElementType::TIMESIG) {
        TimeSig *ts = to_time_sig(el);
        ts->set_large_parentheses(true);
    } else {
        ParenthesesMode p = el->left_paren() || el->right_paren() ? ParenthesesMode::NONE : ParenthesesMode::BOTH;
        el->undo_change_property(Pid::HAS_PARENTHESES, p);
    }
}
