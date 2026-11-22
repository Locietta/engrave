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

#include "noteinput.h"

#include "../dom/accidental.h"
#include "../dom/chord.h"
#include "../dom/factory.h"
#include "../dom/input.h"
#include "../dom/note.h"
#include "../dom/noteval.h"
#include "../dom/pitchspelling.h"
#include "../dom/score.h"
#include "../dom/staff.h"
#include "../style/style.h"
#include "engraving/dom/dummyparent.h"

using namespace muse;
using namespace mu;
using namespace mu::engraving;

Note *NoteInput::add_note_to_chord(Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental,
                                   const std::set<SymId> &articulation_ids) {
    Note *note = Factory::create_note(chord);
    note->set_track(chord->track());
    note->set_nval(note_val);
    score->undo_add_element(note);
    if (force_accidental) {
        int tpc = score->style().style_b(Sid::CONCERT_PITCH) ? note_val.tpc1 : note_val.tpc2;
        AccidentalVal alter = tpc2alter(tpc);
        AccidentalType at = Accidental::value2subtype(alter);
        Accidental *a = Factory::create_accidental(note);
        a->set_accidental_type(at);
        a->set_role(AccidentalRole::USER);
        score->undo_add_element(a);
    }

    if (!articulation_ids.empty()) {
        chord->update_articulations(articulation_ids);
    }

    score->set_play_note(true);
    score->set_play_chord(true);

    return note;
}

Note *NoteInput::add_note(Transaction &, Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental,
                          const std::set<SymId> &articulation_ids, InputState *external_input_state) {
    InputState &is = external_input_state ? (*external_input_state) : score->input_state();

    Note *note = add_note_to_chord(score, chord, note_val, force_accidental, articulation_ids);

    if (external_input_state) {
        is.set_track(note->track());
        is.set_segment(note->chord()->segment());
    } else {
        score->select(note, SelectType::SINGLE, 0);
    }

    if (!chord->staff()->is_tab_staff(chord->tick())) {
        is.move_to_next_input_pos();
    }
    return note;
}
