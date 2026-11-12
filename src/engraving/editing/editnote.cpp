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
#include "engraving/normalization/normalize.h"
#include "log.h"
#include "noteinput.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ChangePitch
//---------------------------------------------------------

struct ChangePitch : UndoableCommand {
private:

    Note *note = nullptr;
    int pitch = 0;
    int tpc1 = 0;
    int tpc2 = 0;

    void flip() override {
        int f_pitch = note->pitch();
        int f_tpc1 = note->tpc1();
        int f_tpc2 = note->tpc2();
        // do not change unless necessary
        if (f_pitch == pitch && f_tpc1 == tpc1 && f_tpc2 == tpc2) {
            return;
        }

        note->set_pitch(pitch, tpc1, tpc2);
        pitch = f_pitch;
        tpc1 = f_tpc1;
        tpc2 = f_tpc2;

        note->trigger_layout();
    }

public:
    ChangePitch(Note *input_note, int input_pitch, int input_tpc1, int input_tpc2) {
        note = input_note;
        pitch = input_pitch;
        tpc1 = input_tpc1;
        tpc2 = input_tpc2;
    }

    UNDO_TYPE(CommandType::CHANGE_PITCH)
    UNDO_NAME("ChangePitch")
    UNDO_CHANGED_OBJECTS({note})
};

//---------------------------------------------------------
//   ChangeFretting
//
//    To use with tablatures to force a specific note fretting;
//    Pitch, string and fret must be changed all together; otherwise,
//    if they are not consistent among themselves, the refretting algorithm may re-assign
//    fret and string numbers for (potentially) all the notes of all the chords of a segment.
//---------------------------------------------------------

struct ChangeFretting : UndoableCommand {
private:

    Note *note = nullptr;
    int pitch = 0;
    int string = 0;
    int fret = 0;
    int tpc1 = 0;
    int tpc2 = 0;

    void flip() override {
        int f_pitch = note->pitch();
        int f_string = note->string();
        int f_fret = note->fret();
        int f_tpc1 = note->tpc1();
        int f_tpc2 = note->tpc2();
        // do not change unless necessary
        if (f_pitch == pitch && f_string == string && f_fret == fret && f_tpc1 == tpc1 && f_tpc2 == tpc2) {
            return;
        }

        note->set_pitch(pitch, tpc1, tpc2);
        note->set_string(string);
        note->set_fret(fret);
        pitch = f_pitch;
        string = f_string;
        fret = f_fret;
        tpc1 = f_tpc1;
        tpc2 = f_tpc2;
        note->trigger_layout();
    }

public:
    ChangeFretting(Note *input_note, int input_pitch, int input_string, int input_fret, int input_tpc1, int input_tpc2) {
        note = input_note;
        pitch = input_pitch;
        string = input_string;
        fret = input_fret;
        tpc1 = input_tpc1;
        tpc2 = input_tpc2;
    }

    UNDO_TYPE(CommandType::CHANGE_FRETTING)
    UNDO_NAME("ChangeFretting")
    UNDO_CHANGED_OBJECTS({note})
};

//---------------------------------------------------------
//   toggleOrnament
//---------------------------------------------------------

void EditNote::toggle_ornament(Score *score, SymId attr) {
    std::set<Chord *> set;
    for (EngravingItem *el : score->selection().elements()) {
        if (el->is_note() || el->is_chord()) {
            Chord *cr = 0;
            // apply articulation on a given chord only once
            if (el->is_note()) {
                cr = to_note(el)->chord();
                if (muse::contains(set, cr)) {
                    continue;
                }
            }
            Ornament *na = Factory::create_ornament(score->dummy());
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

//---------------------------------------------------------
//   toggleAccidental
//---------------------------------------------------------

void EditNote::toggle_accidental(Score *score, AccidentalType at) {
    bool apply_natural_to_input_notes = false;
    if (score->input_state().accidental_type() == at && at != AccidentalType::NONE) {
        at = AccidentalType::NONE; // NONE also means "search for previous accidental and use it if found"
        apply_natural_to_input_notes = true;
    }

    if (score->note_entry_mode()) {
        score->input_state().set_accidental_type(at);
        score->input_state().set_rest(false);

        if (!score->input_state().notes().empty()) {
            EditNote::apply_accidental_to_input_notes(score, apply_natural_to_input_notes ? AccidentalType::NATURAL : at);
        }
    } else {
        if (score->selection().is_none()) {
            score->input_state().set_accidental_type(at);
            score->input_state().set_duration(DurationType::V_QUARTER);
            score->input_state().set_rest(false);
        } else {
            EditNote::change_accidental(score, at);
        }
    }
}

//---------------------------------------------------------
//   applyAccidentalToInputNotes
//---------------------------------------------------------

void EditNote::apply_accidental_to_input_notes(Score *score, AccidentalType accidental_type) {
    NoteValList notes;
    notes.reserve(score->input_state().notes().size());

    Position pos;
    pos.segment = score->input_state().segment();
    pos.staff_idx = score->input_state().staff_idx();

    for (const NoteVal &old_val : score->input_state().notes()) {
        pos.line = note_val_to_line(old_val, score->input_state().staff(), score->input_state().tick());

        bool error = false;
        const NoteVal new_val = NoteInput::note_val_for_position(score, pos, accidental_type, error);

        if (error) {
            notes.push_back(old_val);
        } else {
            notes.push_back(new_val);
        }
    }

    score->input_state().set_notes(notes);
}

//---------------------------------------------------------
//   changeAccidental
///   Change accidental to subtype \a idx for all selected
///   notes.
//---------------------------------------------------------

void EditNote::change_accidental(Score *score, AccidentalType idx) {
    for (EngravingItem *item : score->selection().elements()) {
        switch (item->type()) {
            case ElementType::ACCIDENTAL: {
                Accidental *accidental = to_accidental(item);
                if (accidental->accidental_type() == idx) {
                    EditNote::change_accidental(score, accidental->note(), AccidentalType::NONE);
                } else {
                    EditNote::change_accidental(score, accidental->note(), idx);
                }
                break;
            }
            case ElementType::NOTE: EditNote::change_accidental(score, to_note(item), idx); break;
            default: break;
        }
    }
}

//---------------------------------------------------------
//   changeAccidental2
//---------------------------------------------------------

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
                    score->undo(new ChangePitch(nn, pitch, tpc1, tpc2));
                }
            }
        }
    }
    undo_change_pitch(score, n, pitch, tpc1, tpc2);
}

//---------------------------------------------------------
//   changeAccidental
///   Change accidental to subtype \accidental for
///   note \a note.
//---------------------------------------------------------

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

//---------------------------------------------------------
//   upDownChromatic
//---------------------------------------------------------

void EditNote::up_down_chromatic(bool up, int pitch, Note *n, Key key, int tpc1, int tpc2, int &new_pitch, int &new_tpc1, int &new_tpc2) {
    bool concert_pitch = n->concert_pitch();
    AccidentalVal note_acc_val = tpc2alter(concert_pitch ? tpc1 : tpc2);
    AccidentalVal acc_state = AccidentalVal::NATURAL;
    if (Measure *m = n->find_measure()) {
        acc_state = m->find_accidental(n);
    }
    if (up && pitch < 127) {
        new_pitch = pitch + 1;
        if (concert_pitch) {
            if (tpc1 > Tpc::TPC_A + int(key) && note_acc_val >= acc_state) {
                new_tpc1 = tpc1 - 5; // up semitone diatonic
            } else {
                new_tpc1 = tpc1 + 7; // up semitone chromatic
            }
            new_tpc2 = n->transpose_tpc(new_tpc1);
        } else {
            if (tpc2 > Tpc::TPC_A + int(key) && note_acc_val >= acc_state) {
                new_tpc2 = tpc2 - 5; // up semitone diatonic
            } else {
                new_tpc2 = tpc2 + 7; // up semitone chromatic
            }
            new_tpc1 = n->transpose_tpc(new_tpc2);
        }
    } else if (!up && pitch > 0) {
        new_pitch = pitch - 1;
        if (concert_pitch) {
            if (tpc1 > Tpc::TPC_C + int(key) || note_acc_val > acc_state) {
                new_tpc1 = tpc1 - 7; // down semitone chromatic
            } else {
                new_tpc1 = tpc1 + 5; // down semitone diatonic
            }
            new_tpc2 = n->transpose_tpc(new_tpc1);
        } else {
            if (tpc2 > Tpc::TPC_C + int(key) || note_acc_val > acc_state) {
                new_tpc2 = tpc2 - 7; // down semitone chromatic
            } else {
                new_tpc2 = tpc2 + 5; // down semitone diatonic
            }
            new_tpc1 = n->transpose_tpc(new_tpc2);
        }
    }
}

//---------------------------------------------------------
//   upDown
//---------------------------------------------------------

void EditNote::up_down(Score *score, bool up, UpDownMode mode) {
    prepare_semantic_edit(*score);
    std::vector<Note *> el = score->selection().unique_notes();

    std::sort(el.begin(), el.end(), [up](Note *a, Note *b) {
        if (up) {
            return a->string() < b->string();
        } else {
            return a->string() > b->string();
        }
    });

    for (Note *o_note : el) {
        Fraction tick = o_note->chord()->tick();
        Staff *staff = o_note->staff();
        Part *part = o_note->origin_part();
        Key key = staff->key(tick);
        int tpc1 = o_note->tpc1();
        int tpc2 = o_note->tpc2();
        int pitch = o_note->pitch();
        int pitch_offset = staff->pitch_offset(tick);
        int new_tpc1 = tpc1;   // default to unchanged
        int new_tpc2 = tpc2;   // default to unchanged
        int new_pitch = pitch; // default to unchanged
        int string = o_note->string();
        int fret = o_note->fret();

        StaffGroup staff_group = staff->staff_type(o_note->chord()->tick())->group();
        // if not tab, check for instrument instead of staffType (for pitched to unpitched instrument changes)
        if (staff_group != StaffGroup::TAB) {
            staff_group = part->instrument(o_note->tick())->use_drumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
        }

        switch (staff_group) {
            case StaffGroup::PERCUSSION: {
                const Drumset *ds = part->instrument(tick)->drumset();
                if (ds) {
                    new_pitch = up ? ds->next_pitch(pitch) : ds->prev_pitch(pitch);
                    new_tpc1 = pitch2tpc(new_pitch, Key::C, Prefer::NEAREST);
                    new_tpc2 = new_tpc1;
                }
            } break;
            case StaffGroup::TAB: {
                const StringData *string_data = part->string_data(tick, staff->idx());
                switch (mode) {
                    case UpDownMode::OCTAVE: {
                        const StaffType *stt = staff->staff_type(tick);
                        string = stt->phys_string_to_visual(string);
                        string += (up ? -1 : 1);
                        if (string < 0 || string >= static_cast<int>(string_data->strings())) {
                            return; // no next string to move to
                        }
                        string = stt->visual_string_to_phys(string);
                        fret = string_data->fret(pitch, string, staff, tick);
                        if (fret == -1) {
                            return;
                        }
                    } break;

                    case UpDownMode::DIATONIC: up_down_chromatic(up, pitch, o_note, key, tpc1, tpc2, new_pitch, new_tpc1, new_tpc2); break;

                    case UpDownMode::CHROMATIC: {
                        if (!string_data->frets()) {
                            LOGD("upDown tab chromatic: no frets?");
                            return;
                        }
                        fret += (up ? 1 : -1);
                        if (fret < 0 || fret > string_data->frets()) {
                            LOGD("upDown tab in-string: out of fret range");
                            return;
                        }
                        up_down_chromatic(up, pitch, o_note, key, tpc1, tpc2, new_pitch, new_tpc1, new_tpc2);
                        if (new_pitch + pitch_offset != string_data->get_pitch(string, fret, staff, o_note->tick()) &&
                            !o_note->bend_back()) {
                            LOGD("upDown tab in-string: pitch mismatch");
                            return;
                        }
                        o_note->undo_change_property(Pid::FRET, fret);
                    } break;
                }
            } break;
            case StaffGroup::STANDARD:
                switch (mode) {
                    case UpDownMode::OCTAVE:
                        if (up) {
                            if (pitch < 116) {
                                new_pitch = pitch + 12;
                            }
                        } else {
                            if (pitch > 11) {
                                new_pitch = pitch - 12;
                            }
                        }
                        break;

                    case UpDownMode::CHROMATIC: up_down_chromatic(up, pitch, o_note, key, tpc1, tpc2, new_pitch, new_tpc1, new_tpc2); break;

                    case UpDownMode::DIATONIC: {
                        Note *first_tied_note = o_note->first_tied_note();
                        int new_line = first_tied_note->line() + (up ? -1 : 1);
                        Staff *v_staff = score->staff(first_tied_note->chord()->v_staff_idx());

                        bool error = false;
                        AccidentalVal acc_offs = first_tied_note->chord()->measure()->find_accidental(
                            first_tied_note->chord()->segment(), first_tied_note->chord()->v_staff_idx(), new_line, error);
                        if (error) {
                            acc_offs = Accidental::subtype2value(AccidentalType::NONE);
                        }
                        int n_step = abs_step(new_line, v_staff->clef(tick));
                        int octave = n_step / 7;
                        int test_pitch = step2pitch(n_step) + octave * 12 + int(acc_offs);

                        if (test_pitch <= 127 && test_pitch > 0) {
                            new_pitch = test_pitch;
                            new_tpc1 = new_tpc2 = step2tpc(n_step % 7, acc_offs);
                            if (first_tied_note->concert_pitch()) {
                                new_tpc2 = first_tied_note->transpose_tpc(new_tpc1);
                            } else {
                                new_pitch += v_staff->transpose(tick).chromatic;
                                new_tpc1 = first_tied_note->transpose_tpc(new_tpc2);
                            }
                        }
                    } break;
                }
                break;
        }

        if ((o_note->pitch() != new_pitch) || (o_note->tpc1() != new_tpc1) || o_note->tpc2() != new_tpc2) {
            if (mode != UpDownMode::OCTAVE) {
                auto l = o_note->link_list();
                for (EngravingObject *e : l) {
                    Note *ln = to_note(e);
                    if (ln->accidental()) {
                        score->do_undo_remove_element(ln->accidental());
                    }
                }
            }
            EditNote::undo_change_pitch(score, o_note, new_pitch, new_tpc1, new_tpc2);
            if (mode == UpDownMode::DIATONIC) {
                part->string_data(tick, staff->idx())->convert_pitch(new_pitch, staff, tick, &string, &fret);
                EditNote::undo_change_fretting(score, o_note, new_pitch, string, fret, new_tpc1, new_tpc2);
            }
        } else if (staff->staff_type(tick)->group() == StaffGroup::TAB) {
            bool refret = false;
            if (o_note->string() != string) {
                o_note->undo_change_property(Pid::STRING, string);
                refret = true;
            }
            if (o_note->fret() != fret) {
                o_note->undo_change_property(Pid::FRET, fret);
                refret = true;
            }
            if (refret) {
                const StringData *string_data = part->string_data(tick, staff->idx());
                string_data->fret_chords(o_note->chord());
            }
        }

        score->set_play_note(true);
    }
    score->set_selection_changed(true);
}

//---------------------------------------------------------
//   undoChangePitch
//---------------------------------------------------------

void EditNote::undo_change_pitch(Score *score, Note *note, int pitch, int tpc1, int tpc2) {
    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();

    for (EngravingObject *e : note->link_list()) {
        Note *n = to_note(e);
        tx.push(new ChangePitch(n, pitch, tpc1, tpc2));
    }
}

//---------------------------------------------------------
//   undoChangeFretting
//---------------------------------------------------------

void EditNote::undo_change_fretting(Score *score, Note *note, int pitch, int string, int fret, int tpc1, int tpc2) {
    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();

    for (EngravingObject *e : note->link_list()) {
        Note *n = to_note(e);
        tx.push(new ChangeFretting(n, pitch, string, fret, tpc1, tpc2));
    }
}

//---------------------------------------------------------
//   ChangeVelocity
//---------------------------------------------------------

ChangeVelocity::ChangeVelocity(Note *n, int o) : note(n), user_velocity(o) {}

void ChangeVelocity::flip() {
    int v = note->user_velocity();
    note->set_user_velocity(user_velocity);
    user_velocity = v;
}

//---------------------------------------------------------
//   ChangeNoteEventList::flip
//---------------------------------------------------------

void ChangeNoteEventList::flip() {
    // Get copy of current list.
    NoteEventList nel = note->play_events();
    // Replace current copy with new list.
    note->set_play_events(new_events);
    // Save copy of replaced list.
    new_events = nel;
    // Get a copy of the current playEventType.
    PlayEventType petval = note->chord()->play_event_type();
    // Replace current setting with new setting.
    note->chord()->set_play_event_type(new_petype);
    // Save copy of old setting.
    new_petype = petval;
}

//---------------------------------------------------------
//   ChangeNoteEvent::flip
//---------------------------------------------------------

void ChangeNoteEvent::flip() {
    NoteEvent e = *old_event;
    *old_event = new_event;
    new_event = e;
    // Get a copy of the current playEventType.
    PlayEventType petval = note->chord()->play_event_type();
    // Replace current setting with new setting.
    note->chord()->set_play_event_type(new_petype);
    // Save copy of old setting.
    new_petype = petval;
}

//---------------------------------------------------------
//   ChangeChordPlayEventType::flip
//---------------------------------------------------------

void ChangeChordPlayEventType::flip() {
    // Flips data between NoteEventList's.
    size_t n = chord->notes().size();
    for (size_t i = 0; i < n; ++i) {
        Note *note = chord->notes()[i];
        note->play_events().swap(events[int(i)]);
    }
    // Flips PlayEventType between chord and undo.
    PlayEventType cur_petype = chord->play_event_type();
    chord->set_play_event_type(petype);
    petype = cur_petype;
}
