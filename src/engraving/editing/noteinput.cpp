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

#include <algorithm>
#include <iterator>
#include <tuple>

#include "../dom/accidental.h"
#include "../dom/chord.h"
#include "../dom/chordline.h"
#include "../dom/drumset.h"
#include "../dom/durationtype.h"
#include "../dom/excerpt.h"
#include "../dom/factory.h"
#include "../dom/input.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/note.h"
#include "../dom/noteval.h"
#include "../dom/part.h"
#include "../dom/range.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/slur.h"
#include "../dom/staff.h"
#include "../dom/stafftype.h"
#include "../dom/stringdata.h"
#include "../dom/tie.h"
#include "../dom/tremolotwochord.h"
#include "../dom/tuplet.h"
#include "../dom/utils.h"
#include "../infrastructure/messagebox.h"
#include "editmeasures.h"
#include "editnote.h"
#include "inserttime.h"
#include "log.h"
#include "navigation.h"
#include "transaction/transaction.h"
#include "translation.h"
#include "transpose.h"

using namespace muse;
using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   resolveNoteInputParams
//---------------------------------------------------------

//---------------------------------------------------------
//   noteVal
//---------------------------------------------------------

//---------------------------------------------------------
//   noteValForPosition
//---------------------------------------------------------

//---------------------------------------------------------
//   addNote from NoteVal
//---------------------------------------------------------

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
        is.set_last_segment(is.segment());
        is.set_segment(note->chord()->segment());
    } else {
        score->select(note, SelectType::SINGLE, 0);
    }

    if (!chord->staff()->is_tab_staff(chord->tick())) {
        NoteEntryMethod entry_method = is.note_entry_method();
        if (entry_method != NoteEntryMethod::REPITCH && entry_method != NoteEntryMethod::REALTIME_AUTO &&
            entry_method != NoteEntryMethod::REALTIME_MANUAL) {
            is.move_to_next_input_pos();
        }
    }
    return note;
}

//---------------------------------------------------------
//   addPitch
//---------------------------------------------------------

//---------------------------------------------------------
//   addMidiPitch
//---------------------------------------------------------

//---------------------------------------------------------
//   addTiedMidiPitch
//---------------------------------------------------------

//---------------------------------------------------------
//   repitchReplaceNote
//---------------------------------------------------------

//---------------------------------------------------------
//   putNote
//    mouse click in state NoteType::ENTRY
//---------------------------------------------------------

//---------------------------------------------------------
//   repitchNote
//---------------------------------------------------------

//---------------------------------------------------------
//   insertChordByInsertingTime
//---------------------------------------------------------

//---------------------------------------------------------
//   truncateChordRest
//---------------------------------------------------------

//---------------------------------------------------------
//   nextInputPos
//---------------------------------------------------------

//---------------------------------------------------------
//   addPitch
///   insert note or add note to chord
//---------------------------------------------------------

//---------------------------------------------------------
//   addFret
///   insert note with given fret on current string
//---------------------------------------------------------

//---------------------------------------------------------
//   noteValueChangeAllowed
//    Note value changes are disallowed while a trill cue note is selected outside note entry mode.
//---------------------------------------------------------

//---------------------------------------------------------
//   durationTooShortForDots
//    A duration cannot hold `dots` augmentation dots when it is one of the `dots` shortest
//    representable durations (the shortest being V_1024TH).
//---------------------------------------------------------

//---------------------------------------------------------
//   setDuration
//    Set the note input duration (and apply it to the selection).
//---------------------------------------------------------

//---------------------------------------------------------
//   toggleRest
//    Toggle the note input rest flag (or turn the selection into rests).
//---------------------------------------------------------

//---------------------------------------------------------
//   toggleDots
//    Toggle `dots` augmentation dots (1..4) on the note input duration and the selection.
//---------------------------------------------------------

//---------------------------------------------------------
//   applyToSelection
//    Shared epilogue for setDuration/toggleRest/toggleDots: apply the (possibly changed) input state
//    duration/dots to the current selection.
//---------------------------------------------------------

//---------------------------------------------------------
//   realtimeAdvance
//---------------------------------------------------------
