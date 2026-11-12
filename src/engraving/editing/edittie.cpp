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

#include "edittie.h"

#include "../dom/chord.h"
#include "../dom/factory.h"
#include "../dom/input.h"
#include "../dom/laissezvib.h"
#include "../dom/masterscore.h"
#include "../dom/note.h"
#include "../dom/partialtie.h"
#include "../dom/score.h"
#include "../dom/select.h"
#include "../dom/tie.h"
#include "../dom/tiejumppointlist.h"
#include "../dom/utils.h"
#include "editchord.h"
#include "log.h"
#include "noteinput.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

void ChangeTieJumpPointActive::flip() {
    TieJumpPoint *jump_point = jump_point_list_->find_jump_point(id_);
    if (!jump_point) {
        return;
    }
    bool old_active = jump_point->active();

    if (active_ && jump_point->end_tie() && jump_point->end_tie()->jump_point() != jump_point) {
        jump_point->end_tie()->set_jump_point(jump_point);
    }

    jump_point->set_active(active_);
    active_ = old_active;
}

std::vector<Note *> EditTie::cmd_tie_note_list(const Selection &selection, bool note_entry_mode) {
    EngravingItem *el = selection.element();
    if (Note *n = InputState::note(el)) {
        if (note_entry_mode) {
            return n->chord()->notes();
        } else {
            return {n};
        }
    } else {
        ChordRest *cr = InputState::chord_rest(el);
        if (cr && cr->is_chord()) {
            return to_chord(cr)->notes();
        }
    }
    return selection.note_list();
}

static Tie *create_and_add_tie(Note *start_note, Note *end_note) {
    Score *score = start_note->score();
    Tie *tie = end_note ? Factory::create_tie(start_note) : Factory::create_partial_tie(start_note);
    tie->set_start_note(start_note);
    tie->set_track(start_note->track());
    tie->set_tick(start_note->chord()->segment()->tick());
    if (end_note) {
        if (end_note->tie_back()) {
            score->undo_remove_element(end_note->tie_back());
        }
        tie->set_end_note(end_note);
        tie->set_ticks(end_note->chord()->segment()->tick() - start_note->chord()->segment()->tick());
    }
    score->undo_add_element(tie);

    tie->add_ties_to_jump_points();
    if (!tie->end_note() && tie->tie_jump_points() && tie->tie_jump_points()->empty()) {
        score->undo_remove_element(tie);
        tie = nullptr;
    }

    return tie;
}

void EditTie::cmd_add_tie(Score *score, bool add_to_chord) {
    std::vector<Note *> note_list = cmd_tie_note_list(score->selection(), score->note_entry_mode());
    if (note_list.empty()) {
        LOGD("no notes selected");
        return;
    }

    std::sort(note_list.begin(), note_list.end(), [](const Note *a, const Note *b) { return a->track() < b->track(); });
    track_idx_t track = note_list.at(0)->track();

    std::vector<EngravingItem *> to_select;

    score->start_cmd(TranslatableString("undoableAction", "Add tie"));
    score->master_score()->update_repeat_list(false);
    Chord *last_added_chord = nullptr;

    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();

    InputState &is = score->input_state();

    for (Note *note : note_list) {
        if (note->tie_for()) {
            LOGD("cmdAddTie: note %p has already tie? noteFor: %p", note, note->tie_for());
            if (add_to_chord) {
                continue;
            } else {
                score->undo_remove_element(note->tie_for());
            }
        }

        ChordRest *cr = nullptr;
        Chord *c = note->chord();
        int staff_move = c->staff_move();

        // set cursor at position after note
        if (c->is_grace_before()) {
            // tie grace note before to main note
            cr = to_chord(c->ownership_parent());
            add_to_chord = true;
        } else {
            is.set_track(note->chord()->track());
            is.set_segment(note->chord()->segment());
            is.move_to_next_input_pos();
            if (is.beyond_score()) {
                score->append_measures(1);
                is.move_to_next_input_pos();
            }
            is.set_last_segment(is.segment());

            if (!is.cr()) {
                score->expand_voice();
            }
            cr = is.cr();
        }
        if (!cr) {
            break;
        }

        bool add_flag = last_added_chord != nullptr;
        if (c->track() != track) {
            add_flag = false;
            track = c->track();
        }
        // try to re-use existing note or chord
        Note *n = nullptr;
        if (add_to_chord && cr->is_chord()) {
            Chord *chord = to_chord(cr);
            Note *nn = chord->find_note(note->pitch());
            if (nn && nn->tpc() == note->tpc()) {
                n = nn; // re-use note
            } else {
                add_flag = true; // re-use chord
            }
        }

        // if no note to re-use, create one
        NoteVal nval(note->note_val());
        if (!n) {
            n = NoteInput::add_pitch(tx, score, nval, add_flag);
            if (staff_move != 0) {
                score->undo(new ChangeChordStaffMove(n->chord(), staff_move));
            }
        } else {
            score->select(n);
        }

        if (n) {
            if (!last_added_chord) {
                last_added_chord = n->chord();
            }
            // n is not necessarily next note if duration span over measure
            Note *nnote = search_tie_note(note);
            while (nnote) {
                // DEBUG: if duration spans over measure
                // this does not set line for intermediate notes
                // tpc was set correctly already
                //n->setLine(note->line());
                //n->setTpc(note->tpc());
                create_and_add_tie(note, nnote);

                if (!add_flag || nnote->chord()->tick() >= last_added_chord->tick() || nnote->chord()->is_grace()) {
                    break;
                } else {
                    note = nnote;
                    is.set_last_segment(is.segment());
                    nnote = NoteInput::add_pitch(tx, score, nval, true);
                }
            }
            if (staff_move != 0) {
                for (Note *tied_note : n->tied_notes()) {
                    score->undo(new ChangeChordStaffMove(tied_note->chord(), staff_move));
                }
            }
        }
        to_select.push_back(n);
    }
    if (last_added_chord) {
        NoteInput::next_input_pos(tx, score, last_added_chord, false);
    }
    for (EngravingItem *e : to_select) {
        if (score->can_reselect_item(e)) {
            score->select(e, SelectType::ADD);
        }
    }
    score->end_cmd();
}

Tie *EditTie::cmd_toggle_tie(Score *score) {
    std::vector<Note *> note_list = cmd_tie_note_list(score->selection(), score->note_entry_mode());

    if (note_list.empty()) {
        LOGD("no notes selected");
        return nullptr;
    }

    score->master_score()->update_repeat_list(false);
    std::vector<Note *> tie_note_list(note_list.size());
    bool single_tick = true;
    bool some_have_existing_next_note_to_tie_to = false;
    bool all_have_existing_next_note_to_tie_to = true;
    for (size_t i = 0; i < note_list.size(); ++i) {
        Note *n = note_list[i];
        if (n->chord()->tick() != note_list.front()->tick()) {
            single_tick = false;
        }
        if (n->tie_for()) {
            tie_note_list[i] = nullptr;
        } else {
            Note *tie_note = search_tie_note(n);
            tie_note_list[i] = tie_note;
            if (tie_note || n->chord()->has_following_jump_item()) {
                some_have_existing_next_note_to_tie_to = true;
            } else {
                all_have_existing_next_note_to_tie_to = false;
            }
        }
    }

    const bool should_tie_list_selection = note_list.size() >= 2 && !single_tick;

    if (single_tick /* i.e. all notes are in the same tick */ && !all_have_existing_next_note_to_tie_to) {
        cmd_add_tie(score);
        return nullptr;
    }

    const TranslatableString action_name = some_have_existing_next_note_to_tie_to ? TranslatableString("undoableAction", "Add tie") :
                                                                                    TranslatableString("undoableAction", "Remove tie");

    score->start_cmd(action_name);

    Tie *tie = nullptr;

    for (size_t i = 0; i < note_list.size(); ++i) {
        Note *note = note_list[i];
        Note *tie_to_note = tie_note_list[i];

        if (!note) {
            continue;
        }

        // Tie to adjacent unselected note
        if (some_have_existing_next_note_to_tie_to && tie_to_note) {
            Note *start_note = note->tick() <= tie_to_note->tick() ? note : tie_to_note;
            Note *end_note = start_note == tie_to_note ? note : tie_to_note;
            tie = create_and_add_tie(start_note, end_note);
            continue;
        }

        Tie *old_tie = note->tie_for();
        Chord *chord = note->chord();
        if (old_tie) {
            // Toggle existing tie off
            if (old_tie->tie_jump_points()) {
                old_tie->undo_remove_ties_from_jump_points();
            }
            score->undo_remove_element(old_tie);
            continue;
        }

        if (chord->has_following_jump_item()) {
            // Create partial tie
            tie = create_and_add_tie(note, nullptr);
            continue;
        }

        if (!should_tie_list_selection || i > note_list.size() - 2) {
            continue;
        }

        // Tie to next appropriate note in selection
        Note *note2 = nullptr;

        for (size_t j = i + 1; j < note_list.size(); ++j) {
            Note *candidate_note = note_list[j];
            if (!candidate_note) {
                continue;
            }
            const bool same_part = note->part() == candidate_note->part();
            const bool same_pitch = note->pitch() == candidate_note->pitch();
            const bool same_unison_idx = note->unison_index() == candidate_note->unison_index();
            const bool diff_tick = note->tick() != candidate_note->tick();
            if (same_part && same_pitch && same_unison_idx && diff_tick) {
                note2 = candidate_note;
                note_list[j] = nullptr;
                break;
            }
        }

        if (!(note && note2)) {
            continue;
        }

        Note *start_note = note->tick() <= note2->tick() ? note : note2;
        Note *end_note = start_note == note2 ? note : note2;

        tie = create_and_add_tie(start_note, end_note);
    }

    score->end_cmd();

    return tie;
}

void EditTie::cmd_toggle_laissez_vib(Score *score) {
    const std::vector<Note *> note_list = score->selection().note_list();

    if (note_list.empty()) {
        LOGD("no notes selected");
        return;
    }

    score->start_cmd(TranslatableString("undoableAction", "Toggle laissez vibrer"));

    for (Note *note : note_list) {
        if (LaissezVib *lv = note->laissez_vib()) {
            score->undo_remove_element(lv);
        } else if (note->tie_for()) {
            continue;
        } else {
            LaissezVib *lv_tie = Factory::create_laissez_vib(note);
            score->undo_add_element(lv_tie);
        }
    }

    score->end_cmd();
}
