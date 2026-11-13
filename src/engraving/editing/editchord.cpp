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

#include "editchord.h"

#include "../dom/arpeggio.h"
#include "../dom/articulation.h"
#include "../dom/chord.h"
#include "../dom/chordrest.h"
#include "../dom/factory.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/tapping.h"
#include "../dom/tremolotwochord.h"
#include "../dom/utils.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

void EditChord::add_chord_parentheses(Chord *chord, std::vector<Note *> notes, bool add_to_linked, bool generated) {
    if (notes.empty()) {
        return;
    }
    undo_add_parentheses_to_notes(chord, notes, add_to_linked, generated);
}

//---------------------------------------------------------
//   toggleArticulation
///   Toggle attribute \a attr for all selected notes/rests.
//---------------------------------------------------------

void EditChord::toggle_articulation(Score *score, SymId attr) {
    std::set<Chord *> set;
    for (EngravingItem *el : score->selection().elements()) {
        if (el->is_note() || el->is_chord()) {
            Chord *cr = nullptr;
            if (el->is_note()) {
                cr = to_note(el)->chord();
                if (muse::contains(set, cr)) {
                    continue;
                }
            }
            Articulation *na = Factory::create_articulation(score->dummy());
            na->set_sym_id(attr);
            if (!EditChord::toggle_articulation(score, el, na)) {
                delete na;
            }
            if (cr) {
                set.insert(cr);
            }
        }
    }
}

bool EditChord::toggle_articulation(Score *score, EngravingItem *el, Articulation *a) {
    Chord *c;
    if (el->is_note()) {
        c = to_note(el)->chord();
    } else if (el->is_chord()) {
        c = to_chord(el);
    } else {
        return false;
    }
    Articulation *oa = c->has_articulation(a);
    if (oa) {
        score->undo_remove_element(oa);
        return false;
    }

    Tapping *tap = c->tapping();
    if (tap) {
        score->undo_remove_element(tap);
    }

    if (!a->is_double()) {
        a->set_ownership_parent(c);
        a->set_track(c->track());
        score->undo_add_element(a);
        return true;
    }

    std::set<SymId> new_sub_component_ids = split_articulations({a->sym_id()});
    for (const SymId &id : new_sub_component_ids) {
        Articulation *artic_copy = a->clone();
        artic_copy->set_sym_id(id);

        if (!c->has_articulation(artic_copy)) {
            artic_copy->set_ownership_parent(c);
            artic_copy->set_track(c->track());
            score->undo_add_element(artic_copy);
            continue;
        }
        delete artic_copy;
    }
    return true;
}

void EditChord::remove_chord_parentheses(Chord *chord, std::vector<Note *> notes, bool add_to_linked, bool generated) {
    if (notes.empty()) {
        return;
    }
    std::map<const NoteParenthesisInfo *, std::vector<Note *>> notes_by_group;

    for (Note *n : notes) {
        const NoteParenthesisInfo *note_paren_info = chord->find_note_parenthesis_info(n);
        if (!note_paren_info) {
            continue;
        }

        auto group_it = notes_by_group.find(note_paren_info);

        if (group_it == notes_by_group.end()) {
            std::vector<Note *> note_vec{n};
            notes_by_group.insert(std::make_pair(note_paren_info, note_vec));
            continue;
        }

        group_it->second.push_back(n);
    }

    for (auto pair_iterator = notes_by_group.rbegin(); pair_iterator != notes_by_group.rend(); pair_iterator = std::next(pair_iterator)) {
        auto group_notes_pair = *pair_iterator;
        const NoteParenthesisInfo *paren_info = group_notes_pair.first;
        if (paren_info->notes().size() == group_notes_pair.second.size()) {
            undo_clear_parenthesis_group(chord, paren_info->notes(), paren_info, add_to_linked);
            continue;
        }

        // Only some notes marked for removal. Remove from lowest note up. This keeps the paren group splitting logic safe
        for (auto note_iterator = group_notes_pair.second.begin(); note_iterator != group_notes_pair.second.end();
             note_iterator = std::next(note_iterator)) {
            Note *note = *note_iterator;
            const NoteParenthesisInfo *cur_paren_info = note->parenthesis_info();
            const std::vector<Note *> notes_list = cur_paren_info->notes();
            auto note_pos = std::find(notes_list.begin(), notes_list.end(), note);

            if (note_pos != notes_list.end() && std::next(note_pos) != notes_list.end() && note_pos != notes_list.begin()) {
                std::vector<Note *> new_note_group(note_pos + 1, notes_list.end());

                for (Note *note_to_remove : new_note_group) {
                    if (cur_paren_info->notes().size() == 1) {
                        undo_clear_parenthesis_group(chord, {note_to_remove}, cur_paren_info, add_to_linked);
                    } else {
                        undo_remove_parentheses_from_note(chord, note_to_remove, cur_paren_info, add_to_linked);
                    }
                }

                undo_add_parentheses_to_notes(chord, new_note_group, add_to_linked, generated);
            }
            if (cur_paren_info->notes().size() == 1) {
                undo_clear_parenthesis_group(chord, {note}, cur_paren_info, add_to_linked);
            } else {
                undo_remove_parentheses_from_note(chord, note, cur_paren_info, add_to_linked);
            }
        }
    }
}

void EditChord::undo_add_parentheses_to_notes(Chord *chord, std::vector<Note *> notes, bool add_to_linked, bool generated) {
    if (!add_to_linked) {
        apply_add_note_parenthesis_info(chord, new NoteParenthesisInfo(notes, generated));
        return;
    }

    for (EngravingObject *linked_object : chord->link_list()) {
        if (linked_object == chord) {
            apply_add_note_parenthesis_info(chord, new NoteParenthesisInfo(notes, generated));
            continue;
        }

        Chord *linked_chord = to_chord(linked_object);
        Staff *linked_staff = linked_chord->staff();

        std::vector<Note *> linked_notes;
        linked_notes.reserve(notes.size());
        for (Note *note : notes) {
            Note *linked_note = to_note(note->find_linked_in_staff(linked_staff));
            if (!linked_note) {
                continue;
            }
            linked_notes.push_back(linked_note);
        }
        if (linked_notes.empty()) {
            continue;
        }

        apply_add_note_parenthesis_info(linked_chord, new NoteParenthesisInfo(linked_notes, generated));
    }
}

void EditChord::undo_remove_parentheses_from_note(Chord *chord, Note *note, const NoteParenthesisInfo *group, bool remove_from_linked) {
    if (!remove_from_linked) {
        apply_remove_single_note_parentheses(chord, note, group);
        return;
    }
    const auto original = *group;
    for (EngravingObject *object : chord->link_list()) {
        auto *linked_chord = to_chord(object);
        auto *linked_note = object == chord ? note : to_note(note->find_linked_in_staff(linked_chord->staff()));
        if (!linked_note) continue;
        const auto *linked_group = linked_chord == chord ? group : linked_chord->matching_parenthesis_group(original);
        if (linked_group) apply_remove_single_note_parentheses(linked_chord, linked_note, linked_group);
    }
}

void EditChord::undo_clear_parenthesis_group(Chord *chord, std::vector<Note *> notes, const NoteParenthesisInfo *group,
                                             bool remove_from_linked) {
    if (!remove_from_linked) {
        apply_remove_note_parenthesis_info(chord, const_cast<NoteParenthesisInfo *>(group));
        return;
    }
    const auto original = *group;
    for (EngravingObject *object : chord->link_list()) {
        auto *linked_chord = to_chord(object);
        auto *note = object == chord ? notes.front() : to_note(notes.front()->find_linked_in_staff(linked_chord->staff()));
        if (!note) continue;
        const auto *linked_group = linked_chord == chord ? group : linked_chord->matching_parenthesis_group(original);
        if (linked_group) apply_remove_note_parenthesis_info(linked_chord, const_cast<NoteParenthesisInfo *>(linked_group));
    }
}

void mu::engraving::apply_change_chord_staff_move(ChordRest *chord_rest, int staff_move) {
    note_change(chord_rest, CommandType::CHANGE_CHORD_STAFF_MOVE);

    staff_idx_t old_staff = chord_rest->v_staff_idx();

    chord_rest->set_staff_move(staff_move);
    chord_rest->check_staff_move_validity();
    chord_rest->trigger_layout();
    if (chord_rest->v_staff_idx() == old_staff) {
        return;
    }

    for (EngravingObject *e : chord_rest->link_list()) {
        ChordRest *cr = to_chord_rest(e);
        if (cr == chord_rest) {
            continue;
        }
        cr->set_staff_move(staff_move);
        cr->check_staff_move_validity();
        cr->trigger_layout();
    }
}

void mu::engraving::apply_change_span_arpeggio(Chord *chord_, Arpeggio *span_arpeggio_) {
    note_change(chord_, CommandType::UNKNOWN);

    chord_->set_span_arpeggio(span_arpeggio_);
}

void mu::engraving::apply_add_note_parenthesis_info(Chord *chord_, NoteParenthesisInfo *note_paren_info_) {
    note_change(chord_, CommandType::ADD_NOTE_PARENTHESES_INFO);

    chord_->add_note_parenthesis_info(note_paren_info_);

    chord_->trigger_layout();
}

void mu::engraving::apply_remove_note_parenthesis_info(Chord *chord_, NoteParenthesisInfo *note_paren_info_) {
    note_change(chord_, CommandType::REMOVE_NOTE_PARENTHESES_INFO);

    chord_->remove_note_parenthesis_info(note_paren_info_);

    chord_->trigger_layout();

    defer_root(*chord_, note_paren_info_);
}

void mu::engraving::apply_remove_single_note_parentheses(Chord *chord_, Note *note_, const NoteParenthesisInfo *group) {
    note_change(chord_, CommandType::REMOVE_SINGLE_NOTE_PARENTHESES);

    chord_->remove_note_from_parenthesis_info(note_, group);

    chord_->trigger_layout();
}
