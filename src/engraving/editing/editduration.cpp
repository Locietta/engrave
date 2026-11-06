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

#include "editduration.h"

#include <algorithm>

#include "rw/xmlreader.h"

#include "../dom/chord.h"
#include "../dom/chordrest.h"
#include "../dom/durationtype.h"
#include "../dom/factory.h"
#include "../dom/input.h"
#include "../dom/laissezvib.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/tie.h"
#include "../dom/utils.h"

#include "navigation.h"
#include "noteinput.h"
#include "paste.h"
#include "transaction/transaction.h"

#include "log.h"

using namespace mu::engraving;

void EditDuration::double_duration(Transaction &tx, Score *score) { inc_dec_duration(tx, score, -1, false); }

void EditDuration::half_duration(Transaction &tx, Score *score) { inc_dec_duration(tx, score, 1, false); }

void EditDuration::inc_duration_dotted(Transaction &tx, Score *score) { inc_dec_duration(tx, score, -1, true); }

void EditDuration::dec_duration_dotted(Transaction &tx, Score *score) { inc_dec_duration(tx, score, 1, true); }

//---------------------------------------------------------
//   incDecDuration
//     When stepDotted is false and nSteps is 1 or -1, will halve or double the duration
//     When stepDotted is true, will step by nearest dotted or undotted note
//---------------------------------------------------------

void EditDuration::inc_dec_duration(Transaction &tx, Score *score, int n_steps, bool step_dotted) {
    Selection &selection = score->selection();
    InputState &is = score->input_state();

    if (selection.is_range()) {
        if (!selection.can_copy()) {
            return;
        }
        ChordRest *first_cr = selection.first_chord_rest();
        if (first_cr->is_grace()) {
            first_cr = to_chord_rest(first_cr->parent());
        }
        TDuration initial_duration = first_cr->ticks();
        TDuration d = initial_duration.shift_retain_dots(n_steps, step_dotted);
        if (!d.is_valid()) {
            return;
        }
        Fraction scale = d.ticks() / initial_duration.ticks();
        for (ChordRest *cr : score->get_selected_chord_rests()) {
            Fraction new_ticks = cr->ticks() * scale;
            if (new_ticks < Fraction(1, 1024) ||
                (step_dotted && cr->duration_type().dots() != first_cr->duration_type().dots() && !cr->is_grace())) {
                return;
            }
        }
        const muse::ByteArray mime_data(selection.mime_data());
        XmlReader e(mime_data);
        score->delete_range(selection.start_segment(), selection.end_segment(), staff2track(selection.staff_start()),
                            staff2track(selection.staff_end()), score->selection_filter(), selection.range_contains_multi_note_chords());
        Paste::paste_staff(tx, score, e, selection.start_segment(), selection.staff_start(), scale);
    } else if (selection.is_list()) {
        const std::vector<Note *> notes = selection.note_list();
        const std::set<ChordRest *> crs_set = score->get_selected_chord_rests();
        std::vector<ChordRest *> crs(crs_set.begin(), crs_set.end());
        std::sort(crs.begin(), crs.end(), [](const ChordRest *a, const ChordRest *b) { return a->tick() > b->tick(); });

        for (ChordRest *cr : crs) {
            // if measure rest is selected as input, then the correct initialDuration will be the
            // duration of the measure's time signature, else is just the ChordRest's duration
            TDuration initial_duration = cr->duration_type();
            if (initial_duration == DurationType::V_MEASURE) {
                initial_duration = TDuration(cr->measure()->timesig(), true);

                if (initial_duration.fraction() < cr->measure()->timesig() && n_steps > 0) {
                    // Duration already shortened by truncation; shorten one step less
                    --n_steps;
                }
            }

            TDuration new_duration{step_dotted ? initial_duration.shift_retain_dots(n_steps, step_dotted) :
                                                 initial_duration.shift(n_steps)};
            if (!new_duration.is_valid()) {
                continue;
            }

            if (cr->is_grace()) {
                score->undo_change_chord_rest_len(cr, new_duration);
            } else {
                score->change_c_rlen(cr, new_duration);
            }
        }
        for (Note *n : notes) {
            if (score->can_reselect_item(n)) {
                score->select(to_engraving_item(n), SelectType::ADD);
            }
        }
        if (is.note_entry_mode()) {
            const ChordRest *cr = crs.size() == 1 ? crs.front() : nullptr;
            IF_ASSERT_FAILED(cr) {
                // (At time of writing) it shouldn't be possible to have more than
                // one CR selected during note entry...
                return;
            }
            is.set_duration(cr->duration_type());
            NoteInput::next_input_pos(tx, score, cr, false);
        }
    }
}

//---------------------------------------------------------
//   extendToNextNote
//---------------------------------------------------------

void EditDuration::extend_to_next_note(Transaction &tx, Score *score) {
    Selection &selection = score->selection();
    InputState &is = score->input_state();

    const Fraction start_tick = selection.tick_start();
    Fraction end_tick = selection.tick_end();
    const staff_idx_t start_staff = selection.staff_start();
    const staff_idx_t end_staff = selection.staff_end();
    Box *start_box = selection.start_box();
    Box *end_box = selection.end_box();

    std::vector<EngravingItem *> to_select;
    const bool was_range_selection = selection.is_range();
    const std::vector<Note *> initial_selection = selection.note_list();

    for (ChordRest *cr : score->get_selected_chord_rests()) {
        ChordRest *ncr = Navigation::next_chord_rest(cr);
        if (cr->is_rest() || cr->is_grace() || cr->end_tick() == score->end_tick() ||
            (ncr && ncr->is_chord() && cr->end_tick() == ncr->tick())) {
            continue;
        }
        std::vector<Note *> chord_notes = to_chord(cr)->notes();
        std::vector<Note *> selected_chord_notes;
        for (Note *n : chord_notes) {
            if (std::find(initial_selection.begin(), initial_selection.end(), n) != initial_selection.end()) {
                selected_chord_notes.push_back(n);
                to_select.push_back(n);
                if (LaissezVib *lv = n->laissez_vib()) {
                    score->undo_remove_element(lv);
                }
            }
        }
        bool all_notes_selected = chord_notes.size() == selected_chord_notes.size() ? true : false;

        while (cr->end_tick() != score->end_tick()) { // not at end of score
            if (ncr && cr->end_tick() == ncr->tick() && ncr->is_chord()) {
                break;
            }
            if (!ncr || cr->end_tick() != ncr->tick() // if voices>0 have empty measures till end OR have empty measures between cr and ncr
                || cr->tuplet() != ncr->tuplet() || !all_notes_selected) {
                std::vector<Note *> notes_to_extend =
                    !all_notes_selected ? (all_notes_selected = true, selected_chord_notes) : to_chord(cr)->notes();
                is.set_track(cr->track());
                is.set_segment(cr->segment());
                is.move_to_next_input_pos();
                is.set_last_segment(is.segment());

                if (!is.cr()) {
                    score->expand_voice();
                }
                is.set_duration(is.cr()->duration_type());

                Note *nn = nullptr;
                for (size_t i = 0; i < notes_to_extend.size(); i++) {
                    Note *note = notes_to_extend[i];
                    NoteVal nval(note->note_val());
                    nn = NoteInput::add_pitch(tx, score, nval, i != 0);

                    Tie *tie = Factory::create_tie(note);
                    tie->set_start_note(note);
                    tie->set_track(note->track());
                    tie->set_tick(note->chord()->segment()->tick());
                    tie->set_end_note(nn);
                    tie->set_ticks(nn->chord()->segment()->tick() - note->chord()->segment()->tick());
                    score->undo_add_element(tie);
                }
                cr = to_chord_rest(nn->chord());
                to_select.push_back(cr);
            } else {
                Fraction new_dur = cr->ticks() + ncr->ticks();
                score->change_c_rlen(cr, new_dur);
                while (to_chord(cr)->notes()[0]->tie_for()) {
                    cr = Navigation::next_chord_rest(cr);
                    to_select.push_back(cr);
                }
            }
            ncr = Navigation::next_chord_rest(cr);
            end_tick = cr->end_tick() >= end_tick ? cr->end_tick() : end_tick;
        }
    }

    if (was_range_selection) {
        selection.set_range_ticks(start_tick, end_tick, start_staff, end_staff, start_box, end_box);
        selection.update_selected_elements();
    } else {
        for (EngravingItem *ei : to_select) {
            if (ei->is_chord()) {
                std::vector<Note *> notes = to_chord(ei)->notes();
                score->select({notes.begin(), notes.end()}, SelectType::ADD);
            } else {
                score->select(ei, SelectType::ADD);
            }
        }
    }
}
