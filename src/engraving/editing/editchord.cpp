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
    // Split based on parentheses group
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
            // All notes marked for paren removal, clear parentheses group
            undo_clear_parenthesis_group(chord, paren_info->notes(), paren_info->left_paren(), paren_info->right_paren(), add_to_linked);
            continue;
        }

        // Only some notes marked for removal. Remove from lowest note up. This keeps the paren group splitting logic safe
        for (auto note_iterator = group_notes_pair.second.begin(); note_iterator != group_notes_pair.second.end();
             note_iterator = std::next(note_iterator)) {
            // Remove paren from single note and create new paren group for all notes below
            Note *note = *note_iterator;
            const NoteParenthesisInfo *cur_paren_info = note->parenthesis_info();
            const std::vector<Note *> notes_list = cur_paren_info->notes();
            Parenthesis *left_paren = cur_paren_info->left_paren();
            Parenthesis *right_paren = cur_paren_info->right_paren();
            auto note_pos = std::find(notes_list.begin(), notes_list.end(), note);

            if (note_pos != notes_list.end() && std::next(note_pos) != notes_list.end() && note_pos != notes_list.begin()) {
                // Create new group
                std::vector<Note *> new_note_group(note_pos + 1, notes_list.end());

                for (Note *note_to_remove : new_note_group) {
                    if (cur_paren_info->notes().size() == 1) {
                        undo_clear_parenthesis_group(chord, {note_to_remove}, left_paren, right_paren, add_to_linked);
                    } else {
                        undo_remove_parentheses_from_note(chord, note_to_remove, left_paren, add_to_linked);
                    }
                }

                undo_add_parentheses_to_notes(chord, new_note_group, add_to_linked, generated);
            }
            if (cur_paren_info->notes().size() == 1) {
                undo_clear_parenthesis_group(chord, {note}, left_paren, right_paren, add_to_linked);
            } else {
                undo_remove_parentheses_from_note(chord, note, left_paren, add_to_linked);
            }
        }
    }
}

void EditChord::undo_add_parentheses_to_notes(Chord *chord, std::vector<Note *> notes, bool add_to_linked, bool generated) {
    track_idx_t track = chord->track();
    Parenthesis *left_paren = Factory::create_parenthesis(chord);
    left_paren->set_track(track);
    left_paren->set_direction(DirectionH::LEFT);
    left_paren->set_generated(generated);
    Parenthesis *right_paren = Factory::create_parenthesis(chord);
    right_paren->set_track(track);
    right_paren->set_direction(DirectionH::RIGHT);
    right_paren->set_generated(generated);

    if (!add_to_linked) {
        do_add_note_parentheses(chord, notes, left_paren, right_paren);
        return;
    }

    for (EngravingObject *linked_object : chord->link_list()) {
        if (linked_object == chord) {
            do_add_note_parentheses(chord, notes, left_paren, right_paren);
            continue;
        }

        Chord *linked_chord = to_chord(linked_object);
        Score *linked_score = linked_chord->score();
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

        Parenthesis *linked_paren_left = to_parenthesis(left_paren->linked_clone());
        linked_paren_left->set_score(linked_score);
        linked_paren_left->set_ownership_parent(linked_chord);
        linked_paren_left->set_track(linked_chord->track());
        Parenthesis *linked_paren_right = to_parenthesis(right_paren->linked_clone());
        linked_paren_right->set_score(linked_score);
        linked_paren_right->set_ownership_parent(linked_chord);
        linked_paren_right->set_track(linked_chord->track());

        do_add_note_parentheses(linked_chord, linked_notes, linked_paren_left, linked_paren_right);
    }
}

void EditChord::undo_remove_parentheses_from_note(Chord *chord, Note *note, Parenthesis *left_paren, bool remove_from_linked) {
    if (!remove_from_linked) {
        do_remove_single_note_parenthesis(chord, note, left_paren);
        return;
    }

    for (EngravingObject *linked_object : chord->link_list()) {
        if (linked_object == chord) {
            do_remove_single_note_parenthesis(chord, note, left_paren);
            continue;
        }

        Chord *linked_chord = to_chord(linked_object);
        Staff *linked_staff = linked_chord->staff();
        Parenthesis *linked_left_paren = to_parenthesis(left_paren->find_linked_in_staff(linked_staff));
        if (!linked_left_paren) {
            continue;
        }

        Note *linked_note = to_note(note->find_linked_in_staff(linked_staff));
        if (!linked_note) {
            continue;
        }

        const NoteParenthesisInfo *note_paren_info = linked_chord->find_note_parenthesis_info(linked_note);
        if (note_paren_info) {
            do_remove_single_note_parenthesis(linked_chord, linked_note, linked_left_paren);
        }
    }
}

void EditChord::undo_clear_parenthesis_group(Chord *chord, std::vector<Note *> notes, Parenthesis *left_paren, Parenthesis *right_paren,
                                             bool remove_from_linked) {
    if (!remove_from_linked) {
        do_remove_all_note_parentheses(chord, left_paren);
        return;
    }

    for (EngravingObject *linked_object : chord->link_list()) {
        if (linked_object == chord) {
            do_remove_all_note_parentheses(chord, left_paren);
            continue;
        }

        Chord *linked_chord = to_chord(linked_object);
        Staff *linked_staff = linked_chord->staff();
        Parenthesis *linked_left_paren = to_parenthesis(left_paren->find_linked_in_staff(linked_staff));
        Parenthesis *linked_right_paren = to_parenthesis(right_paren->find_linked_in_staff(linked_staff));

        if (!linked_left_paren && !linked_right_paren) {
            continue;
        }

        std::vector<Note *> linked_notes;
        linked_notes.reserve(notes.size());
        for (Note *note : notes) {
            Note *linked_note = to_note(note->find_linked_in_staff(linked_staff));
            if (!linked_note) {
                continue;
            }
            linked_notes.push_back(linked_note);
        }

        do_remove_all_note_parentheses(linked_chord, linked_left_paren);
    }
}

void EditChord::do_add_note_parentheses(Chord *chord, std::vector<Note *> notes, Parenthesis *left_paren, Parenthesis *right_paren) {
    NoteParenthesisInfo *paren_info = new NoteParenthesisInfo(left_paren, right_paren, notes);
    if (left_paren->generated()) {
        chord->add_note_parenthesis_info(paren_info);
    } else {
        Score *score = chord->score();
        score->undo(new AddNoteParenthesisInfo(chord, paren_info));
    }
}

void EditChord::do_remove_single_note_parenthesis(Chord *chord, Note *note, Parenthesis *left_paren) {
    if (left_paren->generated()) {
        chord->remove_note_from_parenthesis_info(note, left_paren);

        chord->trigger_layout();
    } else {
        Score *score = chord->score();
        score->undo(new RemoveSingleNoteParentheses(chord, note, left_paren));
    }
}

void EditChord::do_remove_all_note_parentheses(Chord *chord, Parenthesis *left_paren) {
    if (left_paren->generated()) {
        const NoteParenthesisInfo *note_paren_info = chord->find_note_parenthesis_info(left_paren);

        chord->remove_note_parenthesis_info(note_paren_info);
        //! HACK: don't delete as it may still be used in PropertiesPanel - see Score::doUndoRemoveElement
        // delete noteParenInfo

        chord->trigger_layout();
    } else {
        Score *score = chord->score();
        NoteParenthesisInfo *note_paren_info = chord->find_note_parenthesis_info(left_paren);

        score->undo(new RemoveNoteParenthesisInfo(chord, note_paren_info));
    }
}

//---------------------------------------------------------
//   ChangeChordStaffMove
//---------------------------------------------------------

void ChangeChordStaffMove::flip() {
    int v = chord_rest->staff_move();
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
    staff_move = v;
}

//---------------------------------------------------------
//   SwapCR
//---------------------------------------------------------

void SwapCR::flip() {
    Segment *s1 = cr1->segment();
    Segment *s2 = cr2->segment();
    track_idx_t track = cr1->track();

    if (cr1->is_chord() && cr2->is_chord() && to_chord(cr1)->tremolo_two_chord() &&
        (to_chord(cr1)->tremolo_two_chord() == to_chord(cr2)->tremolo_two_chord())) {
        TremoloTwoChord *t = to_chord(cr1)->tremolo_two_chord();
        Chord *c1 = t->chord1();
        Chord *c2 = t->chord2();
        t->set_ownership_parent(to_chord(c2));
        t->set_chords(to_chord(c2), to_chord(c1));
    }

    EngravingItem *cr = s1->element(track);
    s1->set_element(track, s2->element(track));
    s2->set_element(track, cr);
    cr1->score()->set_layout(s1->tick(), cr1->staff_idx(), cr1);
    cr1->score()->set_layout(s2->tick(), cr1->staff_idx(), cr1);
}

//---------------------------------------------------------
//   ChangeSpanArpeggio
//---------------------------------------------------------

void ChangeSpanArpeggio::flip() {
    Arpeggio *f_span_arp = chord_->span_arpeggio();

    chord_->set_span_arpeggio(span_arpeggio_);
    span_arpeggio_ = f_span_arp;
}

void AddNoteParenthesisInfo::redo() {
    chord_->add_note_parenthesis_info(note_paren_info_);

    chord_->trigger_layout();
}

void mu::engraving::AddNoteParenthesisInfo::undo() {
    chord_->remove_note_parenthesis_info(note_paren_info_);

    chord_->trigger_layout();
}

void AddNoteParenthesisInfo::cleanup(bool was_done) {
    if (!was_done) {
        delete note_paren_info_;
        note_paren_info_ = nullptr;
    }
}

void RemoveNoteParenthesisInfo::redo() {
    chord_->remove_note_parenthesis_info(note_paren_info_);

    chord_->trigger_layout();
}

void RemoveNoteParenthesisInfo::undo() {
    chord_->add_note_parenthesis_info(note_paren_info_);

    chord_->trigger_layout();
}

void RemoveNoteParenthesisInfo::cleanup(bool was_done) {
    if (was_done) {
        delete note_paren_info_;
        note_paren_info_ = nullptr;
    }
}

void RemoveSingleNoteParentheses::redo() {
    chord_->remove_note_from_parenthesis_info(note_, paren_);

    chord_->trigger_layout();
}

void RemoveSingleNoteParentheses::undo() {
    chord_->add_note_to_parenthesis_info(note_, paren_);

    chord_->trigger_layout();
}
