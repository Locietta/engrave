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

#include "editnote.h"

#include <algorithm>
#include <set>

#include "dom/accidental.h"
#include "dom/articulation.h"
#include "dom/chord.h"
#include "dom/drumset.h"
#include "dom/factory.h"
#include "dom/linkedobjects.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/ornament.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/staff.h"
#include "dom/stringdata.h"
#include "dom/utils.h"
#include "editchord.h"
#include "editing/transaction/transaction.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/normalization/normalize.h"
#include "log.h"

using namespace mu::engraving;

static void apply_change_pitch(Note *note, int pitch, int tpc1, int tpc2) {
    if (note->pitch() == pitch && note->tpc1() == tpc1 && note->tpc2() == tpc2) return;
    note_change(note);
    note->set_pitch(pitch, tpc1, tpc2);
    note->trigger_layout();
}

void EditNote::change_accidental2(Note *n, int pitch, int tpc) {
    Score *score = n->score();
    Chord *chord = n->chord();
    Staff *st = chord->staff();
    int fret = n->fret();
    int string = n->string();

    if (st->is_tab_staff(chord->tick())) {
        if (pitch != n->pitch()) {
            //
            // as pitch has changed, calculate new
            // string & fret
            //
            const StringData *string_data = n->origin_part()->string_data(n->tick(), st->idx());
            if (string_data) {
                string_data->convert_pitch(pitch, st, &string, &fret);
            }
        }
    }
    int tpc1;
    int tpc2 = n->transpose_tpc(tpc);
    if (n->style().style_b(Sid::CONCERT_PITCH)) {
        tpc1 = tpc;
    } else {
        tpc1 = tpc2;
        tpc2 = tpc;
    }

    if (!st->is_tab_staff(chord->tick())) {
        //
        // handle ties
        //
        if (n->tie_back()) {
            if (pitch != n->pitch()) {
                score->undo_remove_element(n->tie_back());
                if (n->tie_for()) {
                    score->undo_remove_element(n->tie_for());
                }
            }
        } else {
            Note *nn = n;
            while (nn && nn->tie_for()) {
                nn = nn->tie_for()->end_note();
                if (nn) {
                    apply_change_pitch(nn, pitch, tpc1, tpc2);
                }
            }
        }
    }
    undo_change_pitch(score, n, pitch, tpc1, tpc2);
}

///   Change accidental to subtype \accidental for
///   note \a note.

void EditNote::change_accidental(Score *score, Note *note, AccidentalType accidental) {
    prepare_semantic_edit(*score);
    Chord *chord = note ? note->chord() : nullptr;
    if (!chord) {
        return;
    }
    Segment *segment = chord->segment();
    if (!segment) {
        return;
    }
    Measure *measure = segment->measure();
    if (!measure) {
        return;
    }
    Fraction tick = segment->tick();
    Staff *estaff = score->staff(chord->v_staff_idx());
    if (!estaff) {
        return;
    }
    ClefType clef = estaff->clef(tick);
    if (clef == ClefType::TAB || clef == ClefType::TAB4 || clef == ClefType::TAB_SERIF || clef == ClefType::TAB4_SERIF) {
        return;
    }
    int step = ClefInfo::pitch_offset(clef) - note->line();
    while (step < 0) {
        step += 7;
    }
    step %= 7;
    //
    // accidental change may result in pitch change
    //
    AccidentalVal acc2 = measure->find_accidental(note);
    AccidentalVal acc = (accidental == AccidentalType::NONE) ? acc2 : Accidental::subtype2value(accidental);

    int pitch = line2pitch(note->line(), clef, Key::C) + int(acc);
    if (!note->concert_pitch()) {
        pitch += note->transposition();
    }

    int tpc = step2tpc(step, acc);

    bool force_remove = false;
    bool force_add = false;

    // delete accidental
    // both for this note and for any linked notes
    if (accidental == AccidentalType::NONE) {
        force_remove = true;
    }
    // precautionary or microtonal accidental
    // either way, we display it unconditionally
    // both for this note and for any linked notes
    else if (acc == acc2 || (pitch == note->pitch() && !Accidental::is_microtonal(note->accidental_type())) ||
             Accidental::is_microtonal(accidental)) {
        force_add = true;
    }

    for (EngravingObject *se : note->link_list()) {
        Note *ln = to_note(se);
        if (ln->concert_pitch() != note->concert_pitch()) {
            continue;
        }
        Score *lns = ln->score();
        Accidental *a = ln->accidental();
        if (force_remove) {
            if (a) {
                lns->undo_remove_element(a);
            }
            if (ln->tie_back()) {
                continue;
            }
        } else if (force_add) {
            if (a) {
                score->undo_remove_element(a);
            }
            Accidental *a1 = Factory::create_accidental(ln);
            a1->set_accidental_type(accidental);
            a1->set_role(AccidentalRole::USER);
            lns->undo_add_element(a1);
        } else if (a && Accidental::is_microtonal(a->accidental_type())) {
            lns->undo_remove_element(a);
        }
        change_accidental2(ln, pitch, tpc);
    }
    score->set_play_note(true);
    score->set_selection_changed(true);
}

void EditNote::undo_change_pitch(Score *score, Note *note, int pitch, int tpc1, int tpc2) {
    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();

    for (EngravingObject *e : note->link_list()) {
        Note *n = to_note(e);
        apply_change_pitch(n, pitch, tpc1, tpc2);
    }
}
