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

#include "translation.h"

#include "editmeasures.h"
#include "editnote.h"
#include "inserttime.h"
#include "regroup.h"
#include "transaction/transaction.h"
#include "transpose.h"

#include "../infrastructure/messagebox.h"

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

#include "navigation.h"

#include "log.h"

using namespace muse;
using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   resolveNoteInputParams
//---------------------------------------------------------

bool NoteInput::resolve_note_input_params(const Score *score, int note, bool add_flag, NoteInputParams &out) {
    const InputState &is = score->input_state();

    //! NOTE: Drumset params should be defined explicitly (see NotationViewInputController::tryPercussionShortcut)
    if (!is.is_valid() || is.drumset()) {
        return false;
    }

    int octave = 4;

    static const int tab[] = {0, 2, 4, 5, 7, 9, 11};

    // if adding notes, add above the upNote of the current chord
    EngravingItem *el = score->selection().element();
    if (add_flag && el && el->is_note()) {
        Chord *chord = to_note(el)->chord();
        Note *n = chord->up_note();
        int tpc = n->tpc();
        octave = (n->epitch() - int(tpc2alter(tpc))) / ::mu::engraving::pitch_delta_octave;
        if (note <= tpc2step(tpc)) {
            octave++;
        }
    } else {
        int cur_pitch = 60;
        if (is.segment()) {
            const Staff *staff = score->staff(is.track() / ::mu::engraving::voices);
            Segment *seg = is.segment()->prev1(SegmentType::CHORD_REST | SegmentType::CLEF | SegmentType::HEADER_CLEF);
            while (seg) {
                if (seg->is_chord_rest_type()) {
                    EngravingItem *p = seg->element(is.track());
                    if (p && p->is_chord()) {
                        Note *n = to_chord(p)->down_note();
                        // forget any accidental and/or adjustment due to key signature
                        cur_pitch = n->epitch() - static_cast<int>(tpc2alter(n->tpc()));
                        break;
                    }
                } else if (seg->is_clef_type() || seg->is_header_clef_type()) {
                    EngravingItem *p = seg->element(track_zero_voice(is.track())); // clef on voice 1
                    if (p && p->is_clef()) {
                        Clef *clef = to_clef(p);
                        // check if it's an actual change or just a courtesy
                        ClefType ctb = staff->clef(clef->tick() - Fraction::eps());
                        if (ctb != clef->clef_type() || clef->tick().is_zero()) {
                            cur_pitch = line2pitch(4, clef->clef_type(), Key::C); // C 72 for treble clef
                            break;
                        }
                    }
                }
                seg = seg->prev1_mm(SegmentType::CHORD_REST | SegmentType::CLEF | SegmentType::HEADER_CLEF);
            }
            octave = cur_pitch / 12;
        }

        int delta = octave * 12 + tab[note] - cur_pitch;
        if (delta > 6) {
            --octave;
        } else if (delta < -6) {
            ++octave;
        }
    }

    out.step = octave * 7 + note;
    return true;
}

//---------------------------------------------------------
//   noteVal
//---------------------------------------------------------

NoteVal NoteInput::note_val(const Score *score, int pitch, staff_idx_t staff_idx, bool allow_transposition) {
    NoteVal nval(pitch);

    const Staff *st = score->staff(staff_idx);
    if (!st) {
        return nval;
    }

    bool concert_pitch = score->style().style_b(Sid::CONCERT_PITCH);
    Interval v = st->part()->instrument(score->input_state().tick())->transpose();

    // If an accidental is set in the input state, use it as a hint for the pitch spelling
    if (AccidentalType at = score->input_state().accidental_type(); at == AccidentalType::FLAT || at == AccidentalType::SHARP) {
        Prefer prefer = at == AccidentalType::SHARP ? Prefer::SHARPS : Prefer::FLATS;
        if (concert_pitch || v.is_zero()) {
            // Note: using Key::C always and ignoring actual key signature. Otherwise, flat mode would still use sharps
            // sometimes if they're in the key, and vice versa, which seems contrary to the intent of the hint.
            nval.tpc1 = pitch2tpc(nval.pitch, Key::C, prefer);
            Interval v_flipped = v;
            v_flipped.flip();
            nval.tpc2 = Transpose::transpose_tpc(nval.tpc1, v_flipped, true);
        } else {
            // Spell the transposed pitch first, then convert to concert pitch
            int written_pitch = nval.pitch;
            if (!allow_transposition) {
                written_pitch -= v.chromatic;
            }
            nval.tpc2 = pitch2tpc(written_pitch, Key::C, prefer);
            nval.tpc1 = Transpose::transpose_tpc(nval.tpc2, v, true);
        }
    }

    // if transposing, interpret MIDI pitch as representing desired written pitch
    // set pitch based on corresponding sounding pitch
    if (!concert_pitch && allow_transposition) {
        nval.pitch += v.chromatic;
    }

    return nval;
}

//---------------------------------------------------------
//   noteValForPosition
//---------------------------------------------------------

NoteVal NoteInput::note_val_for_position(const Score *score, Position pos, AccidentalType at, bool &error) {
    const InputState &is = score->input_state();

    error = false;
    const Segment *s = pos.segment;
    int line = pos.line;
    Fraction tick = s->tick();
    staff_idx_t staff_idx = pos.staff_idx;
    const Staff *st = score->staff(staff_idx);
    ClefType clef = st->clef(tick);
    const Instrument *instr = st->part()->instrument(s->tick());
    NoteVal nval;
    const StringData *string_data = nullptr;

    // pitched/unpitched note entry depends on instrument (override StaffGroup)
    StaffGroup staff_group = st->staff_type(tick)->group();
    if (staff_group != StaffGroup::TAB) {
        staff_group = instr->use_drumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
    }

    if (staff_group != StaffGroup::PERCUSSION) {
        string_data = st->part()->string_data(s->tick(), st->idx());
    }

    switch (staff_group) {
        case StaffGroup::PERCUSSION: {
            if (is.rest()) {
                break;
            }
            const Drumset *ds = instr->drumset();
            nval.pitch = is.drum_note();
            if (!ds->is_valid(nval.pitch) || ds->line(nval.pitch) != line) {
                // Drum note from input state is not valid - fall back to the first valid pitch for this line...
                const int default_pitch = ds->default_pitch_for_line(line);
                if (ds->is_valid(default_pitch)) {
                    nval.pitch = default_pitch;
                }
            }
            if (nval.pitch < 0) {
                error = true;
                return nval;
            }
            nval.head_group = ds->note_head(nval.pitch);
            if (nval.head_group == NoteHeadGroup::HEAD_INVALID) {
                error = true;
                return nval;
            }
            break;
        }
        case StaffGroup::TAB: {
            if (is.rest()) {
                return nval;
            }
            line = st->staff_type(tick)->visual_string_to_phys(line);
            if (line < 0 || line >= static_cast<int>(string_data->strings())) {
                error = true;
                return nval;
            }
            // build a default NoteVal for that string
            nval.string = line;
            if (pos.fret != ::mu::engraving::invalid_fret_index) { // if a fret is given, use it
                nval.fret = pos.fret;
            } else { // if no fret, use 0 as default
                nval.fret = 0;
            }
            // reduce within fret limit
            if (nval.fret > string_data->frets()) {
                nval.fret = string_data->frets();
            }
            // for open strings, only accepts fret 0 (strings in StringData are from bottom to top)
            size_t strg_data_idx = string_data->strings() - line - 1;
            if (nval.fret > 0 && string_data->string_list().at(strg_data_idx).open) {
                nval.fret = 0;
            }
            nval.pitch = string_data->get_pitch(line, nval.fret, st, pos.segment->tick());
            break;
        }

        case StaffGroup::STANDARD: {
            AccidentalVal acci =
                (at == AccidentalType::NONE ? s->measure()->find_accidental(s, staff_idx, line, error) : Accidental::subtype2value(at));
            if (error) {
                return nval;
            }
            int step = abs_step(line, clef);
            int octave = step / 7;
            nval.pitch = step2pitch(step) + octave * 12 + int(acci);
            if (score->style().style_b(Sid::CONCERT_PITCH)) {
                nval.tpc1 = step2tpc(step % 7, acci);
            } else {
                nval.pitch += instr->transpose().chromatic;
                nval.tpc2 = step2tpc(step % 7, acci);
                Interval v = st->transpose(tick);
                if (v.is_zero()) {
                    nval.tpc1 = nval.tpc2;
                } else {
                    nval.tpc1 = Transpose::transpose_tpc(nval.tpc2, v, true);
                }
            }
            string_data->convert_pitch(nval.pitch, st, pos.segment->tick(), &nval.string, &nval.fret);
        } break;
    }
    return nval;
}

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

Note *NoteInput::add_note_to_tied_chord(Transaction &tx, Score *score, Chord *chord, const NoteVal &note_val, bool force_accidental,
                                        const std::set<SymId> &articulation_ids) {
    IF_ASSERT_FAILED(!chord->notes().empty()) { return nullptr; };
    Note *reference_note = chord->notes().at(0);

    while (true) {
        // don't add note if it is already part of tied notes previously
        if (reference_note->chord()->find_note(note_val.pitch)) {
            return nullptr;
        }
        if (!reference_note->tie_back() || reference_note->incoming_partial_tie()) {
            break;
        }
        reference_note = reference_note->tie_back()->start_note();
    }

    Tie *tie = nullptr;
    Note *new_note = nullptr;

    while (reference_note->tie_for()) {
        chord = reference_note->chord();
        new_note = add_note(tx, score, chord, note_val, force_accidental, articulation_ids);
        if (!new_note) {
            return nullptr;
        }
        if (tie) {
            tie->set_end_note(new_note);
            tie->set_tick2(new_note->tick());
            new_note->set_tie_back(tie);
            score->undo_add_element(tie);
        }

        tie = Factory::create_tie(new_note);
        tie->set_start_note(new_note);
        tie->set_tick(new_note->tick());
        tie->set_track(new_note->track());
        new_note->set_tie_for(tie);

        if (!reference_note->tie_for()->end_note()) {
            break;
        }

        reference_note = reference_note->tie_for()->end_note();
    }

    chord = reference_note->chord();
    new_note = add_note(tx, score, chord, note_val, force_accidental, articulation_ids);

    if (!new_note) {
        return nullptr;
    }

    if (tie) {
        tie->set_end_note(new_note);
        tie->set_tick2(new_note->tick());
        new_note->set_tie_back(tie);
        score->undo_add_element(tie);
    }

    score->connect_ties();

    return new_note;
}

//---------------------------------------------------------
//   addPitch
//---------------------------------------------------------

Note *NoteInput::add_pitch(Transaction &tx, Score *score, NoteVal &nval, bool add_flag, InputState *external_input_state) {
    InputState &is = external_input_state ? (*external_input_state) : score->input_state();

    if (add_flag) {
        const Segment *last_segment = is.last_segment();
        IF_ASSERT_FAILED(last_segment) { return nullptr; }
        ChordRest *c = to_chord_rest(last_segment->element(is.track()));
        if (!c || !c->is_chord()) {
            LOGD("NoteInput::addPitch: cr %s", c ? c->type_name() : "zero");
            return nullptr;
        }
        return add_pitch_to_chord(tx, score, nval, to_chord(c), external_input_state);
    }

    if (is.beyond_score()) {
        score->append_measures(1);
        is.move_to_next_input_pos();
    }

    score->expand_voice(is.segment(), is.track());

    // insert note
    DirectionV stem_direction = DirectionV::AUTO;
    track_idx_t track = is.track();
    if (is.drumset()) {
        const Drumset *ds = is.drumset();
        if (!ds->is_valid(nval.pitch)) {
            return nullptr;
        }

        nval.head_group = ds->note_head(nval.pitch);
        stem_direction = ds->stem_direction(nval.pitch);
        track = ds->voice(nval.pitch) + (is.track() / ::mu::engraving::voices) * ::mu::engraving::voices;
        is.set_track(track);
        score->expand_voice(is.segment(), is.track());
    }

    if (!is.cr()) {
        ChordRest *prev_cr_value = is.segment()->next_chord_rest(is.track(), /*backwards*/ true, /*stopAtMeasureBoundary*/ true);
        if (prev_cr_value && prev_cr_value->end_tick() > is.tick()) {
            const InputState input_state_to_restore = is; // because truncateChordRest will alter the input state
            truncate_chord_rest(tx, score, prev_cr_value, is.tick(), /*fillWithRest*/ true);
            is = input_state_to_restore;
        }
    }

    Measure *measure = is.segment()->measure();
    if (measure->is_measure_repeat_group(track2staff(track))) {
        MeasureRepeat *mr = measure->measure_repeat_element(track2staff(track));
        score->delete_item(mr); // resets any measures related to mr
    }
    Fraction duration;
    if (is.using_note_entry_method(NoteEntryMethod::REALTIME_AUTO) || is.using_note_entry_method(NoteEntryMethod::REALTIME_MANUAL)) {
        // FIXME: truncate duration at barline in real-time modes.
        //   The user might try to enter a duration that is too long to fit in the remaining space in the measure.
        //   We could split the duration at the barline and continue into the next bar, but this would create extra
        //   notes, extra ties, and extra pain. Instead, we simply truncate the duration at the barline.
        Fraction ticks2measure_end = is.segment()->measure()->ticks() - is.segment()->rtick();
        duration = is.duration().fraction() > ticks2measure_end ? ticks2measure_end : is.duration().fraction();
    } else {
        duration = is.duration().fraction();
    }
    Note *note = nullptr;
    Note *last_tied_note = nullptr;
    if (is.using_note_entry_method(NoteEntryMethod::REPITCH) && is.cr()->is_chord()) {
        Chord *chord = to_chord(is.cr());
        std::tie(note, last_tied_note) = repitch_replace_note(tx, score, chord, nval); // the add (not replace) case was handled above
    } else if (!is.using_note_entry_method(NoteEntryMethod::REPITCH)) {
        Segment *seg = score->set_note_rest(is.segment(), track, nval, duration, stem_direction, /* forceAccidental */ false,
                                            is.articulation_ids(), /* rhythmic */ false, external_input_state);
        EngravingItem *chord_item = seg ? seg->element(track) : nullptr;
        if (chord_item && chord_item->is_chord()) {
            note = to_chord(chord_item)->up_note();
        }
    }

    if (is.slur()) {
        //
        // extend slur
        //
        ChordRest *e = score->search_note(is.tick(), is.track());
        EngravingItem *ee = is.slur()->start_element();
        if (e && ee) {
            Fraction stick = Fraction(0, 1);
            if (ee->is_chord_rest()) {
                stick = to_chord_rest(ee)->tick();
            } else if (ee->is_note()) {
                stick = to_note(ee)->chord()->tick();
            }
            if (stick == e->tick()) {
                is.slur()->set_tick(stick);
                is.slur()->set_start_element(e);
            } else {
                is.slur()->set_tick2(e->tick());
                is.slur()->set_end_element(e);
            }
        } else {
            LOGD("addPitch: cannot find slur note");
        }
    }
    if (is.using_note_entry_method(NoteEntryMethod::REPITCH)) {
        // move cursor to next note, but skip tied notes (they were already repitched above)
        ChordRest *next = last_tied_note ? Navigation::next_chord_rest(last_tied_note->chord()) : Navigation::next_chord_rest(is.cr());
        while (next && !next->is_chord()) {
            next = Navigation::next_chord_rest(next);
        }
        if (next) {
            is.move_input_pos(next->segment());
        }
    } else {
        NoteEntryMethod entry_method = is.note_entry_method();
        if (entry_method != NoteEntryMethod::REALTIME_AUTO && entry_method != NoteEntryMethod::REALTIME_MANUAL) {
            is.move_to_next_input_pos();
        }
    }
    return note;
}

Note *NoteInput::add_pitch_to_chord(Transaction &tx, Score *score, NoteVal &nval, Chord *chord, InputState *external_input_state,
                                    bool force_accidental) {
    IF_ASSERT_FAILED(chord) { return nullptr; }

    InputState &is = external_input_state ? (*external_input_state) : score->input_state();

    auto is_tied = [](const Chord *ch) {
        if (ch->notes().empty()) {
            return false;
        }
        Note *n = ch->notes().at(0);
        return (n->tie_for() && !n->tie_for()->is_laissez_vib()) || n->tie_back();
    };

    Note *note = nullptr;
    if (is_tied(chord)) {
        note = add_note_to_tied_chord(tx, score, chord, nval, force_accidental);
        if (!note) {
            note = add_note(tx, score, chord, nval, force_accidental, /* articulationIds */ {}, external_input_state);
        }
    } else {
        note = add_note(tx, score, chord, nval, force_accidental, /* articulationIds */ {}, external_input_state);
    }

    if (is.using_note_entry_method(NoteEntryMethod::REPITCH)) {
        // move cursor to next note
        ChordRest *next = Navigation::next_chord_rest(note->chord());
        while (next && !next->is_chord()) {
            next = Navigation::next_chord_rest(next);
        }
        if (next) {
            is.move_input_pos(next->segment());
        }
    } else if (is.last_segment() == is.segment()) {
        NoteEntryMethod entry_method = is.note_entry_method();
        if (entry_method != NoteEntryMethod::REALTIME_AUTO && entry_method != NoteEntryMethod::REALTIME_MANUAL) {
            is.move_to_next_input_pos();
        }
    }

    return note;
}

//---------------------------------------------------------
//   addMidiPitch
//---------------------------------------------------------

Note *NoteInput::add_midi_pitch(Transaction &tx, Score *score, int pitch, bool add_flag, bool allow_transposition) {
    NoteVal nval = note_val(score, pitch, score->input_state().staff_idx(), allow_transposition);
    return add_pitch(tx, score, nval, add_flag);
}

//---------------------------------------------------------
//   addTiedMidiPitch
//---------------------------------------------------------

Note *NoteInput::add_tied_midi_pitch(Transaction &tx, Score *score, int pitch, bool add_flag, Chord *prev_chord, bool allow_transposition) {
    Note *n = add_midi_pitch(tx, score, pitch, add_flag, allow_transposition);
    if (prev_chord) {
        Note *nn = prev_chord->find_note(n->pitch());
        if (nn) {
            Tie *tie = Factory::create_tie(nn);
            tie->set_start_note(nn);
            tie->set_end_note(n);
            tie->set_tick(tie->start_note()->tick());
            tie->set_tick2(tie->end_note()->tick());
            tie->set_track(n->track());
            n->set_tie_back(tie);
            nn->set_tie_for(tie);
            score->undo_add_element(tie);
        }
    }
    return n;
}

//---------------------------------------------------------
//   repitchReplaceNote
//---------------------------------------------------------

std::pair<Note *, Note *> NoteInput::repitch_replace_note(Transaction &, Score *score, Chord *chord, const NoteVal &nval,
                                                          bool force_accidental) {
    Note *note = Factory::create_note(chord);
    note->set_track(chord->track());
    note->set_nval(nval);

    Note *first_tied_note = nullptr;
    Note *last_tied_note = note;
    ChordLine *chord_line = nullptr;
    std::vector<Note *> notes = chord->notes();
    // break all ties into current chord
    // these will exist only if user explicitly moved cursor to a tied-into note
    // in ordinary use, cursor will automatically skip past these during note entry
    for (Note *n : notes) {
        if (n->tie_back()) {
            score->undo_remove_element(n->tie_back());
        }
    }
    // Keep first chordline only
    chord_line = chord->chord_line() ? chord->chord_line()->clone() : nullptr;
    std::vector<EngravingItem *> chord_els = chord->el();
    for (EngravingItem *e : chord_els) {
        if (e->is_chord_line()) {
            score->undo_remove_element(e);
        }
    }
    // for the first note of the chord only, preserve ties by changing pitch of all forward notes
    // the tie forward itself will be added later
    // multi-note chords get reduced to single note chords anyhow since we remove the old notes below
    if (notes.front()->tie_for()) {
        Note *tn = notes.front()->tie_for()->end_note();
        while (tn) {
            Chord *tc = tn->chord();
            if (tc->notes().size() != 1) {
                std::vector<Note *> notes_to_remove;
                for (Note *n : tc->notes()) {
                    if (n != tn) {
                        notes_to_remove.push_back(n);
                    }
                }
                for (Note *n : notes_to_remove) {
                    score->undo_remove_element(n);
                }
                assert(tc->notes().size() == 1 && tc->notes().front() == tn);
            }
            if (!first_tied_note) {
                first_tied_note = tn;
            }
            last_tied_note = tn;
            EditNote::undo_change_pitch(score, tn, note->pitch(), note->tpc1(), note->tpc2());
            if (tn->tie_for()) {
                tn = tn->tie_for()->end_note();
            } else {
                break;
            }
        }
    }
    // remove all notes from chord
    // the new note will be added below
    while (!chord->notes().empty()) {
        score->undo_remove_element(chord->notes().front());
    }
    notes.clear(); // get rid of vector full of dangling pointers!

    // add new note to chord
    score->undo_add_element(note);

    if (chord_line) {
        chord_line->set_note(note);
        score->undo_add_element(chord_line);
    }

    if (force_accidental) {
        int tpc = score->style().style_b(Sid::CONCERT_PITCH) ? nval.tpc1 : nval.tpc2;
        AccidentalVal alter = tpc2alter(tpc);
        AccidentalType at = Accidental::value2subtype(alter);
        Accidental *a = Factory::create_accidental(note);
        a->set_accidental_type(at);
        a->set_role(AccidentalRole::USER);
        score->undo_add_element(a);
    }
    score->set_play_note(true);
    // recreate tie forward if there is a note to tie to
    // one-sided ties will not be recreated
    if (first_tied_note) {
        Tie *tie = Factory::create_tie(note);
        tie->set_start_note(note);
        tie->set_end_note(first_tied_note);
        tie->set_tick(tie->start_note()->tick());
        tie->set_tick2(tie->end_note()->tick());
        tie->set_track(note->track());
        score->undo_add_element(tie);
    }
    score->select(last_tied_note);

    return {note, last_tied_note};
}

//---------------------------------------------------------
//   putNote
//    mouse click in state NoteType::ENTRY
//---------------------------------------------------------

Ret NoteInput::put_note(Transaction &tx, Score *score, const PointF &pos, bool replace, bool insert) {
    Position p;
    if (!score->get_position(&p, pos, score->input_state().voice())) {
        LOGD("cannot put note here, get position failed");
        return make_ret(Ret::Code::UNKNOWN_ERROR);
    }

    Score *pos_score = p.segment->score();
    // it is not safe to call repitchNote() if p is on a TAB staff
    bool is_tablature = pos_score->staff(p.staff_idx)->is_tab_staff(p.segment->tick());

    //  calculate actual clicked line from staffType offset and stepOffset
    Staff *ss = pos_score->staff(p.staff_idx);
    int step_offset = ss->staff_type(p.segment->tick())->step_offset();
    double st_y_offset = ss->staff_type(p.segment->tick())->yoffset().val();
    double line_dist = ss->staff_type(p.segment->tick())->line_distance().val();
    p.line -= step_offset + 2 * st_y_offset / line_dist;

    if (pos_score->input_state().using_note_entry_method(NoteEntryMethod::REPITCH) && !is_tablature) {
        return repitch_note(tx, pos_score, p, replace);
    } else {
        if (insert || pos_score->input_state().using_note_entry_method(NoteEntryMethod::TIMEWISE)) {
            return insert_chord_by_inserting_time(tx, pos_score, p);
        } else {
            return put_note(tx, pos_score, p, replace);
        }
    }
}

Ret NoteInput::put_note(Transaction &tx, Score *score, const Position &p, bool replace) {
    InputState &is = score->input_state();

    Staff *st = score->staff(p.staff_idx);
    Segment *s = p.segment;

    is.set_track(p.staff_idx * ::mu::engraving::voices + is.voice());
    is.set_segment(s);

    if (p.beyond_score) {
        score->append_measures(1);
        is.move_to_next_input_pos();
    }

    if (mu::engraving::Excerpt *excerpt = score->excerpt()) {
        const TracksMap &tracks = excerpt->tracks_mapping();

        if (!tracks.empty() && muse::key(tracks, is.track(), muse::nidx) == muse::nidx) {
            return make_ret(Ret::Code::UNKNOWN_ERROR);
        }
    }

    DirectionV stem_direction = DirectionV::AUTO;
    bool error = false;
    NoteVal nval = note_val_for_position(score, p, is.accidental_type(), error);
    if (error) {
        return make_ret(Ret::Code::UNKNOWN_ERROR);
    }

    // warn and delete MeasureRepeat if necessary
    Measure *m = is.segment()->measure();
    staff_idx_t staff_idx = track2staff(is.track());
    if (m->is_measure_repeat_group(staff_idx)) {
        auto b = MessageBox().warning(muse::trc("engraving", "Note input will remove measure repeat"),
                                      muse::trc("engraving", "This measure contains a measure repeat."
                                                             " If you enter notes here, it will be deleted."
                                                             " Do you want to continue?"));
        if (b == MessageBox::CANCEL) {
            return make_ret(Ret::Code::CANCEL);
        }

        score->delete_item(m->measure_repeat_element(staff_idx));
    }

    const StringData *string_data = 0;

    // pitched/unpitched note entry depends on instrument (override StaffGroup)
    StaffGroup staff_group = st->staff_type(s->tick())->group();
    if (staff_group != StaffGroup::TAB) {
        staff_group = st->part()->instrument(s->tick())->use_drumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
    }

    switch (staff_group) {
        case StaffGroup::PERCUSSION: {
            const Drumset *ds = st->part()->instrument(s->tick())->drumset();
            DO_ASSERT(ds);

            if (ds) {
                stem_direction = ds->stem_direction(nval.pitch);
                is.set_voice(ds->voice(nval.pitch));
            }
            break;
        }
        case StaffGroup::TAB:
            string_data = st->part()->string_data(s->tick(), st->idx());
            is.set_drum_note(-1);
            break;
        case StaffGroup::STANDARD: is.set_drum_note(-1); break;
    }

    score->expand_voice();

    // If there's an overlapping ChordRest at the current input position, shorten it...
    if (!is.cr()) {
        ChordRest *prev_cr_value = is.segment()->next_chord_rest(is.track(), /*backwards*/ true, /*stopAtMeasureBoundary*/ true);
        if (prev_cr_value && prev_cr_value->end_tick() > is.tick()) {
            const InputState input_state_to_restore = is; // because truncateChordRest will alter the input state
            truncate_chord_rest(tx, score, prev_cr_value, is.tick(), /*fillWithRest*/ true);
            is = input_state_to_restore;
        }
    }

    ChordRest *cr = is.cr();

    auto check_tied = [&]() {
        if (!cr || !cr->is_chord()) {
            return false;
        }
        auto ch = to_chord(cr);
        return (!ch->notes().empty() && !ch->notes()[0]->tie_back()) &&
               (ch->notes()[0]->tie_for() && !ch->notes()[0]->tie_for()->is_laissez_vib());
    };

    bool add_to_chord = false;
    bool should_add_as_tied = check_tied();

    if (cr) {
        // retrieve total duration of current chord
        TDuration d = cr->duration_type();
        // if not in replace mode AND chord duration == input duration AND not rest input
        // we need to add to current chord (otherwise, we will need to replace it or create a new one)
        if (!replace && ((d == is.duration()) || should_add_as_tied) && cr->is_chord() && !is.rest()) {
            if (st->is_tab_staff(cr->tick())) { // TAB
                // if a note on same string already exists, update to new pitch/fret
                for (Note *note : to_chord(cr)->notes()) {
                    if (note->string() == nval.string) { // if string is the same
                        // if adding a new digit will keep fret number within fret limit,
                        // add a digit to existing fret number
                        if (string_data) {
                            int fret = note->fret() * 10 + nval.fret;
                            if (fret <= string_data->frets()) {
                                nval.fret = fret;
                                nval.pitch = string_data->get_pitch(nval.string, nval.fret, st);
                            } else {
                                LOGD("can't increase fret to %d", fret);
                            }
                        }
                        // set fret number (original or combined) in all linked notes
                        int tpc1 = note->tpc1default(nval.pitch);
                        int tpc2 = note->tpc2default(nval.pitch);
                        EditNote::undo_change_fretting(score, note, nval.pitch, nval.string, nval.fret, tpc1, tpc2);
                        score->set_play_note(true);
                        return muse::make_ok();
                    }
                }
            } else { // not TAB
                // if a note with the same pitch already exists in the chord, remove it
                Chord *chord = to_chord(cr);
                Note *note = chord->find_note(nval.pitch);
                if (note) {
                    if (chord->notes().size() > 1) {
                        score->undo_remove_element(note);
                    }
                    return muse::make_ok();
                }
            }
            add_to_chord = true; // if no special case, add note to chord
        }
    }
    bool force_accidental = false;
    if (is.accidental_type() != AccidentalType::NONE) {
        NoteVal nval2 = note_val_for_position(score, p, AccidentalType::NONE, error);
        force_accidental = (nval.pitch == nval2.pitch);
    }

    Ret ret = muse::make_ok();

    if (add_to_chord && cr->is_chord()) {
        // if adding, add!
        Chord *chord = to_chord(cr);
        if (should_add_as_tied) {
            Note *n = add_note_to_tied_chord(tx, score, chord, nval, force_accidental, is.articulation_ids());
            if (!n) {
                ret = make_ret(Ret::Code::UNKNOWN_ERROR);
            }

            is.set_accidental_type(AccidentalType::NONE);
            return ret;
        }
        Note *note = add_note(tx, score, chord, nval, force_accidental, is.articulation_ids());
        if (!note) {
            ret = make_ret(Ret::Code::UNKNOWN_ERROR);
        }

        is.set_accidental_type(AccidentalType::NONE);
        return ret;
    } else {
        // if not adding, replace current chord (or create a new one)
        if (is.rest()) {
            nval.pitch = -1;
        }

        Segment *seg = score->set_note_rest(is.segment(), is.track(), nval, is.duration().fraction(), stem_direction, force_accidental,
                                            is.articulation_ids());
        if (!seg) {
            ret = make_ret(Ret::Code::UNKNOWN_ERROR);
        }

        is.set_accidental_type(AccidentalType::NONE);
    }

    if (cr && !st->is_tab_staff(cr->tick())) {
        is.move_to_next_input_pos();
    }

    return ret;
}

//---------------------------------------------------------
//   repitchNote
//---------------------------------------------------------

Ret NoteInput::repitch_note(Transaction &tx, Score *score, const Position &p, bool replace) {
    InputState &is = score->input_state();

    Segment *s = p.segment;
    Fraction tick = s->tick();
    Staff *st = score->staff(p.staff_idx);
    ClefType clef = st->clef(tick);

    NoteVal nval;
    bool error = false;
    AccidentalType at = is.accidental_type();
    if (is.drumset() && is.drum_note() != -1) {
        nval.pitch = is.drum_note();
    } else {
        AccidentalVal acci =
            (at == AccidentalType::NONE ? s->measure()->find_accidental(s, p.staff_idx, p.line, error) : Accidental::subtype2value(at));
        if (error) {
            return make_ret(Ret::Code::UNKNOWN_ERROR);
        }

        int step = abs_step(p.line, clef);
        int octave = step / 7;
        nval.pitch = step2pitch(step) + octave * 12 + int(acci);

        if (score->style().style_b(Sid::CONCERT_PITCH)) {
            nval.tpc1 = step2tpc(step % 7, acci);
        } else {
            nval.pitch += st->part()->instrument(s->tick())->transpose().chromatic;
            nval.tpc2 = step2tpc(step % 7, acci);
        }
    }

    if (!is.segment()) {
        return make_ret(Ret::Code::UNKNOWN_ERROR);
    }

    Chord *chord;
    ChordRest *cr = is.cr();
    if (!cr) {
        cr = is.segment()->next_chord_rest(is.track());
        if (!cr) {
            return make_ret(Ret::Code::UNKNOWN_ERROR);
        }
    }
    if (cr->is_rest()) { //skip rests
        ChordRest *next = Navigation::next_chord_rest(cr);
        while (next && !next->is_chord()) {
            next = Navigation::next_chord_rest(next);
        }
        if (next) {
            is.move_input_pos(next->segment());
        }
        return muse::make_ok();
    } else {
        chord = to_chord(cr);
    }

    bool force_accidental = false;
    if (is.accidental_type() != AccidentalType::NONE) {
        NoteVal nval2 = note_val_for_position(score, p, AccidentalType::NONE, error);
        force_accidental = (nval.pitch == nval2.pitch);
    }

    // Note: not sure this is ever called with replace == false, since add (not replace) case is handled already in cmdAddPitch
    if (!replace) {
        return add_pitch_to_chord(tx, score, nval, chord, /* externalInputState */ nullptr, force_accidental);
    }

    auto [note, last_tied_note] = repitch_replace_note(tx, score, chord, nval, force_accidental);
    score->set_play_chord(true);

    // move to next Chord
    ChordRest *next = Navigation::next_chord_rest(last_tied_note->chord());
    while (next && !next->is_chord()) {
        next = Navigation::next_chord_rest(next);
    }
    if (next) {
        is.move_input_pos(next->segment());
    }

    return muse::make_ok();
}

//---------------------------------------------------------
//   insertChordByInsertingTime
//---------------------------------------------------------

Ret NoteInput::insert_chord_by_inserting_time(Transaction &tx, Score *score, const Position &pos) {
    // insert by increasing measure length
    //
    // TODO:
    //    - check voices
    //    - split chord/rest

    EngravingItem *el = score->selection().element();
    if (!el || !(el->is_note() || el->is_rest())) {
        return make_ret(Ret::Code::UNKNOWN_ERROR);
    }

    Segment *seg = pos.segment;
    if (seg->splits_tuplet()) {
        MScore::set_error(MsError::CANNOT_INSERT_TUPLET);
        return make_ret(Ret::Code::UNKNOWN_ERROR);
    }

    // remove all two-note tremolos that end on this tick
    for (EngravingItem *e : seg->elist()) {
        if (!e || !e->is_chord()) {
            continue;
        }
        Chord *c = to_chord(e);
        TremoloTwoChord *t = c->tremolo_two_chord();
        if (t && t->chord2() == c) {
            // we have to remove this tremolo because we are adding time in the middle of it
            // (if c is chord1 then we're inserting before the trem so it's fine)
            score->undo_remove_element(t);
        }
    }

    const TDuration duration = score->input_state().duration();
    const Fraction fraction = duration.fraction();
    const Fraction len = fraction;
    Fraction tick = seg->tick();
    Measure *measure = seg->measure()->is_mm_rest() ? seg->measure()->mm_rest_first() : seg->measure();
    const Fraction target_measure_len = measure->ticks() + fraction;

    // Shift spanners, enlarge the measure.
    // The approach is similar to that in Measure::adjustToLen() but does
    // insert time to the middle of the measure rather than to the end.
    score->undo_insert_time(tick, len);

    for (Score *lscore : score->score_list()) {
        tx.push(new InsertTime(lscore, tick, len));
        Measure *m = lscore->tick2measure(tick);
        tx.push(new ChangeMeasureLen(m, target_measure_len));
        Segment *score_seg = m->tick2segment(tick);
        // tuplets store their tick: move those from the insertion on with their segments
        std::set<Tuplet *> tuplets;
        for (Segment *s = score_seg; s; s = s->next()) {
            if (s->is_chord_rest_type()) {
                for (EngravingItem *e : s->elist()) {
                    for (Tuplet *t = e ? to_chord_rest(e)->tuplet() : nullptr; t && t->tick() >= tick; t = t->tuplet()) {
                        // shared tuplets follow their origin
                        if (t->origin_items().empty()) {
                            tuplets.insert(t);
                        }
                    }
                }
            }
            s->undo_change_property(Pid::TICK, s->rtick() + len);
        }
        for (Tuplet *t : tuplets) {
            t->undo_change_property(Pid::TICK, t->tick() + len);
        }
    }

    // Fill the inserted time with rests.
    // This is better to be done in master score to cover all staves.
    MasterScore *ms = score->master_score();
    Measure *ms_measure = ms->tick2measure(tick);
    const size_t ms_tracks = ms->ntracks();

    Segment *first_seg = ms_measure->first(SegmentType::CHORD_REST);
    for (track_idx_t track = 0; track < ms_tracks; ++track) {
        EngravingItem *maybe_rest = first_seg->element(track);
        bool measure_is_full = false;

        // I. Convert any measure rests into normal (non-measure) rest(s) of equivalent duration
        if (maybe_rest && maybe_rest->is_rest() && to_rest(maybe_rest)->duration_type().is_measure()) {
            ms->undo_remove_element(maybe_rest);
            Rest *measure_rest = to_rest(maybe_rest);
            // If measure rest is situated at measure start we will fill
            // the whole measure with rests.
            measure_is_full = measure_rest->rtick().is_zero();
            const Fraction fill_len = measure_is_full ? target_measure_len : measure_rest->ticks();
            ms->set_rest(measure_rest->tick(), track, fill_len, /* useDots */ false, /* tuplet */ nullptr, /* useFullMeasureRest */ false);
        }

        // II. Make chord or rest in other track longer if it crosses the insert area
        if (!measure_is_full) {
            ChordRest *cr = ms->find_cr(tick, track);
            if (cr && cr->tick() < tick && cr->end_tick() > tick) {
                if (cr->is_rest()) {
                    const Fraction fill_len = cr->ticks() + fraction;
                    ms->undo_remove_element(cr);
                    ms->set_rest(cr->tick(), track, fill_len, /* useDots */ false, /* tuplet */ nullptr, /* useFullMeasureRest */ false);
                } else if (cr->is_chord()) {
                    Chord *chord = to_chord(cr);
                    std::vector<TDuration> durations = to_duration_list(chord->ticks() + fraction, /* useDots */ true);
                    Fraction p = chord->tick();
                    ms->undo_remove_element(chord);
                    Chord *prev_chord = nullptr;
                    for (const TDuration &dur : durations) {
                        Chord *prototype = prev_chord ? prev_chord : chord;
                        const bool gen_tie = bool(prev_chord);
                        prev_chord = ms->add_chord(p, dur, prototype, gen_tie, /* tuplet */ nullptr);
                        p += dur.fraction();
                    }
                    // TODO: reconnect ties if this chord was tied to other
                }
                measure_is_full = true;
            }
        }

        // III. insert rest(s) to fill the inserted space
        if (!measure_is_full && ms_measure->has_voice(track)) {
            ms->set_rest(tick, track, fraction, /* useDots */ false, /* tuplet */ nullptr);
        }
    }

    // Put the note itself.
    Segment *s = measure->undo_get_segment(SegmentType::CHORD_REST, tick);
    Position p(pos);
    p.segment = s;
    put_note(tx, score, p, true);

    return muse::make_ok();
}

//---------------------------------------------------------
//   truncateChordRest
//---------------------------------------------------------

void NoteInput::truncate_chord_rest(Transaction &, Score *score, ChordRest *cr, const Fraction &tick, bool fill_with_rest) {
    MasterScore *ms = score->master_score();
    IF_ASSERT_FAILED(ms && cr && cr->end_tick() > tick) { return; }

    const Fraction time_stretch = cr->staff()->time_stretch(cr->tick());
    Tuplet *tuplet = cr->tuplet();
    const Fraction overlap_duration = (cr->end_tick() - tick) / actual_ticks(Fraction(1, 1), tuplet, time_stretch);
    const Fraction desired_duration = cr->ticks() - overlap_duration;

    ms->change_c_rlen(cr, desired_duration, fill_with_rest);

    // Fill the difference with tied notes if necessary...
    const Fraction difference = desired_duration - cr->ticks();
    if (cr->is_chord() && difference.is_not_zero()) {
        Fraction start_tick = cr->end_tick();
        Chord *prev_chord = to_chord(cr);
        const std::vector<TDuration> duration_list = to_duration_list(difference, true);
        for (const TDuration &dur : duration_list) {
            prev_chord = ms->add_chord(start_tick, dur, prev_chord, /*genTie*/ bool(prev_chord), prev_chord->tuplet());
            start_tick += actual_ticks(dur.fraction(), prev_chord->tuplet(), prev_chord->staff()->time_stretch(start_tick));
        }
    }
}

//---------------------------------------------------------
//   nextInputPos
//---------------------------------------------------------

void NoteInput::next_input_pos(Transaction &, Score *score, const ChordRest *cr, bool do_select) {
    InputState &is = score->input_state();

    ChordRest *ncr = Navigation::next_chord_rest(cr);
    if ((!ncr) && (is.track() % ::mu::engraving::voices)) {
        Segment *s = score->tick2segment(cr->end_tick(), false, SegmentType::CHORD_REST);
        ncr = s ? to_chord_rest(s->element(cr->staff_idx() * ::mu::engraving::voices)) : nullptr;
    }
    if (ncr) {
        is.set_segment(ncr->segment());
        if (do_select) {
            score->select(ncr, SelectType::SINGLE, 0);
        }
    }
}

//---------------------------------------------------------
//   addPitch
///   insert note or add note to chord
//---------------------------------------------------------

void NoteInput::add_pitch(Transaction &tx, Score *score, const NoteInputParams &params, bool add_flag, bool insert) {
    InputState &is = score->input_state();
    if (!is.is_valid()) {
        LOGD("cannot enter notes here (no chord rest at current position)");
        return;
    }

    is.set_rest(false);

    const Drumset *ds = is.drumset();
    if (ds) {
        is.set_drum_note(params.drum_pitch);
        is.set_voice(ds->voice(params.drum_pitch));
    }

    add_pitch(tx, score, params.step, add_flag, insert);
}

void NoteInput::add_pitch(Transaction &tx, Score *score, int step, bool add_flag, bool insert) {
    InputState &is = score->input_state();

    insert = insert || is.using_note_entry_method(NoteEntryMethod::TIMEWISE);
    Position pos;
    if (add_flag) {
        EngravingItem *el = score->selection().element();
        if (el && el->is_note()) {
            Note *selected_note = to_note(el);
            Chord *chord = selected_note->chord();
            Segment *seg = chord->segment();
            pos.segment = seg;
            pos.staff_idx = chord->v_staff_idx();
            ClefType clef = score->staff(pos.staff_idx)->clef(seg->tick());
            pos.line = rel_step(step, clef);
            bool error;
            NoteVal nval = note_val_for_position(score, pos, is.accidental_type(), error);
            if (error) {
                return;
            }
            bool force_accidental = false;
            if (is.accidental_type() != AccidentalType::NONE) {
                NoteVal nval2 = note_val_for_position(score, pos, AccidentalType::NONE, error);
                force_accidental = (nval.pitch == nval2.pitch);
            }
            if (is.using_note_entry_method(NoteEntryMethod::REPITCH)) {
                add_pitch_to_chord(tx, score, nval, chord, /* externalInputState */ nullptr, force_accidental);
            } else {
                add_note(tx, score, chord, nval, force_accidental, is.articulation_ids());
            }
            is.set_accidental_type(AccidentalType::NONE);
            return;
        }
    }

    pos.segment = is.segment();
    pos.staff_idx = is.track() / ::mu::engraving::voices;
    ClefType clef = score->staff(pos.staff_idx)->clef(pos.segment->tick());
    pos.line = rel_step(step, clef);
    pos.beyond_score = is.beyond_score();

    if (is.using_note_entry_method(NoteEntryMethod::REPITCH)) {
        repitch_note(tx, score, pos, !add_flag);
    } else {
        if (insert) {
            insert_chord_by_inserting_time(tx, score, pos);
        } else {
            put_note(tx, score, pos, !add_flag);
        }
    }
    is.set_accidental_type(AccidentalType::NONE);
}

//---------------------------------------------------------
//   addFret
///   insert note with given fret on current string
//---------------------------------------------------------

void NoteInput::add_fret(Transaction &tx, Score *score, int fret) {
    InputState &is = score->input_state();
    if (is.track() == muse::nidx) { // invalid state
        return;
    }
    if (!is.segment()) {
        LOGD("cannot enter notes here (no chord rest at current position)");
        return;
    }
    is.set_rest(false);
    Position pos;
    pos.segment = is.segment();
    pos.staff_idx = is.track() / ::mu::engraving::voices;
    pos.line = score->staff(pos.staff_idx)->staff_type(is.tick())->phys_string_to_visual(is.string());
    pos.fret = fret;
    pos.beyond_score = is.beyond_score();
    put_note(tx, score, pos, false);
}

//---------------------------------------------------------
//   noteValueChangeAllowed
//    Note value changes are disallowed while a trill cue note is selected outside note entry mode.
//---------------------------------------------------------

bool NoteInput::note_value_change_allowed(const Score *score) {
    if (score->note_entry_mode()) {
        return true;
    }

    for (const ChordRest *cr : score->get_selected_chord_rests()) {
        if (cr->is_chord() && to_chord(cr)->is_trill_cue_note()) {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------
//   durationTooShortForDots
//    A duration cannot hold `dots` augmentation dots when it is one of the `dots` shortest
//    representable durations (the shortest being V_1024TH).
//---------------------------------------------------------

static bool duration_too_short_for_dots(const TDuration &duration, int dots) {
    // Ordered shortest-first; a duration cannot hold `dots` dots if it is one of the `dots` shortest.
    static constexpr DurationType shortest_types[] = {
        DurationType::V_1024TH,
        DurationType::V_512TH,
        DurationType::V_256TH,
        DurationType::V_128TH,
    };

    const int count = std::min(dots, int(std::size(shortest_types)));
    for (int i = 0; i < count; ++i) {
        if (duration == shortest_types[i]) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   setDuration
//    Set the note input duration (and apply it to the selection).
//---------------------------------------------------------

void NoteInput::set_duration(Transaction &tx, Score *score, DurationType duration) {
    if (!note_value_change_allowed(score)) {
        return;
    }

    InputState &is = score->input_state();

    const TDuration old_duration = is.duration();
    const bool old_rest = is.rest();
    const AccidentalType old_accidental_type = is.accidental_type();

    is.set_duration(duration);
    is.set_dots(0);

    //
    // if in "note enter" mode, reset rest flag
    //
    if (score->note_entry_mode()) {
        if (score->using_note_entry_method(NoteEntryMethod::BY_DURATION) || score->using_note_entry_method(NoteEntryMethod::RHYTHM)) {
            // Preserve the number of dots from the previous duration
            if (old_duration.dots() > 0) {
                toggle_dots(tx, score, old_duration.dots());
            }

            if (is.rest()) {
                // Enter a rest
                score->set_note_rest(is.segment(), is.track(), NoteVal(), is.duration().fraction());
                is.move_to_next_input_pos();
            } else {
                if (score->using_note_entry_method(NoteEntryMethod::RHYTHM)) {
                    const EngravingItem *selected_item = score->selection().element();
                    if (selected_item && selected_item->is_note()) {
                        is.set_notes({to_note(selected_item)->note_val()});
                    }
                }

                if (is.beyond_score()) {
                    score->append_measures(1);
                    is.move_to_next_input_pos();
                }
                ChordRest *cr = is.cr();
                Chord *chord = nullptr;

                if (cr && cr->is_chord() && cr->duration_type() == is.duration()) {
                    chord = to_chord(cr);
                }

                for (const NoteVal &nval : is.notes()) {
                    if (chord && chord->find_note(nval.pitch)) {
                        is.move_to_next_input_pos();
                        continue;
                    }

                    NoteVal copy(nval);
                    const Note *note = nullptr;

                    if (chord) {
                        note = add_pitch_to_chord(tx, score, copy, chord);
                    } else {
                        note = add_pitch(tx, score, copy, false /*addFlag*/);
                    }

                    if (note) {
                        chord = note->chord();
                    }
                }
            }
        } else {
            is.set_rest(false);
        }
    }

    apply_to_selection(score, old_duration, old_rest, old_accidental_type, false /*toggleForSelectionOnly*/, false /*restToggle*/);
}

//---------------------------------------------------------
//   toggleRest
//    Toggle the note input rest flag (or turn the selection into rests).
//---------------------------------------------------------

void NoteInput::toggle_rest(Transaction & /*tx*/, Score *score) {
    if (!note_value_change_allowed(score)) {
        return;
    }

    InputState &is = score->input_state();

    const TDuration old_duration = is.duration();
    const bool old_rest = is.rest();
    const AccidentalType old_accidental_type = is.accidental_type();

    if (score->note_entry_mode()) {
        is.set_rest(!is.rest());
        is.set_accidental_type(AccidentalType::NONE);
    } else if (score->selection().is_none()) {
        is.set_duration(DurationType::V_QUARTER);
        is.set_rest(true);
    } else {
        for (ChordRest *cr : score->get_selected_chord_rests()) {
            if (!cr->is_rest()) {
                score->set_note_rest(cr->segment(), cr->track(), NoteVal(), cr->duration_type_ticks(), DirectionV::AUTO, false,
                                     is.articulation_ids());
            }
        }
    }

    apply_to_selection(score, old_duration, old_rest, old_accidental_type, false /*toggleForSelectionOnly*/, true /*restToggle*/);
}

//---------------------------------------------------------
//   toggleDots
//    Toggle `dots` augmentation dots (1..4) on the note input duration and the selection.
//---------------------------------------------------------

void NoteInput::toggle_dots(Transaction & /*tx*/, Score *score, int dots, bool toggle_for_selection_only) {
    IF_ASSERT_FAILED(dots >= 1 && dots <= 4) { return; }

    if (!note_value_change_allowed(score)) {
        return;
    }

    InputState &is = score->input_state();

    const TDuration old_duration = is.duration();
    const bool old_rest = is.rest();
    const AccidentalType old_accidental_type = is.accidental_type();

    if (is.duration().dots() == dots || duration_too_short_for_dots(is.duration(), dots)) {
        is.set_dots(0);
    } else {
        is.set_dots(dots);
    }

    apply_to_selection(score, old_duration, old_rest, old_accidental_type, toggle_for_selection_only, false /*restToggle*/);
}

//---------------------------------------------------------
//   applyToSelection
//    Shared epilogue for setDuration/toggleRest/toggleDots: apply the (possibly changed) input state
//    duration/dots to the current selection.
//---------------------------------------------------------

void NoteInput::apply_to_selection(Score *score, const TDuration &old_duration, bool old_rest, AccidentalType old_accidental_type,
                                   bool toggle_for_selection_only, bool rest_toggle) {
    InputState &is = score->input_state();

    if (score->note_entry_mode()) {
        if (!toggle_for_selection_only || score->selection().is_none()) {
            return;
        }
    }

    std::vector<ChordRest *> crs;
    std::vector<EngravingItem *> elements_to_select;

    if (score->selection().is_single()) {
        EngravingItem *e = score->selection().element();
        ChordRest *cr = InputState::chord_rest(e);

        // do not allow to add a dot on a full measure rest
        if (cr && cr->is_rest()) {
            Rest *r = to_rest(cr);
            if (r->is_full_measure_rest()) {
                is.set_dots(0);
            }
        }

        // on measure rest, select the first actual rest
        if (cr && cr->is_mm_rest()) {
            Measure *m = cr->measure()->mm_rest_first();
            if (m) {
                cr = m->find_chord_rest(m->tick(), 0);
            }
        }

        if (cr) {
            crs.push_back(cr);
        } else {
            score->deselect(e);
        }
    } else if (score->selection().is_none() && !rest_toggle) {
        TDuration td = is.duration();
        is.set_duration(td);
        is.set_accidental_type(AccidentalType::NONE);
    } else {
        const auto elements = score->selection().unique_elements();
        for (EngravingItem *e : elements) {
            if (score->selection().is_list() || e->is_rest() || e->is_note()) {
                elements_to_select.push_back(e);
                score->deselect(e);
            }
        }
        bool can_adjust_length = true;
        for (EngravingItem *e : elements) {
            ChordRest *cr = InputState::chord_rest(e);
            if (!cr) {
                continue;
            }
            if (cr->is_measure_repeat() || cr->is_mm_rest()) {
                can_adjust_length = false;
                break;
            }
            crs.push_back(cr);
        }

        if (can_adjust_length) {
            // Change length from last to first chord/rest
            std::sort(crs.begin(), crs.end(), [](const ChordRest *cr1, const ChordRest *cr2) {
                if (cr2->track() == cr1->track()) {
                    return cr2->is_before(cr1);
                }
                return cr2->track() < cr1->track();
            });
            // Remove duplicates from the list
            crs.erase(std::unique(crs.begin(), crs.end()), crs.end());
        } else {
            crs.clear();
        }
    }

    const int dots = is.duration().dots();

    if (toggle_for_selection_only && dots > 0 && !crs.empty()) {
        bool should_remove_dots = true;
        for (const ChordRest *cr : crs) {
            if (dots != cr->dots()) {
                should_remove_dots = false;
                break;
            }
        }

        if (should_remove_dots) {
            is.set_dots(0);
        }
    }

    for (ChordRest *cr : crs) {
        if (cr->is_chord() && (to_chord(cr)->is_grace())) {
            //
            // handle appoggiatura and acciaccatura
            //
            score->undo_change_chord_rest_len(cr, is.duration());
        } else {
            score->change_c_rlen(cr, is.duration());
        }
    }

    if (!elements_to_select.empty()) {
        std::vector<EngravingItem *> select_list;
        for (EngravingItem *e : elements_to_select) {
            if (score->can_reselect_item(e)) {
                select_list.push_back(e);
            }
        }

        score->select(select_list, SelectType::ADD, 0);
        score->selection().update_selected_elements();
    }

    if (toggle_for_selection_only) {
        is.set_duration(old_duration);
        is.set_rest(old_rest);
        is.set_accidental_type(old_accidental_type);

        if (score->note_entry_mode()) {
            if (is.last_segment() == is.segment()) {
                is.move_to_next_input_pos();
            }
        }
    }
}

void NoteInput::increase_duration(Transaction &tx, Score *score) {
    switch (score->input_state().duration().type()) {
            // cycle back from longest to shortest?
            //          case TDuration::V_LONG:
            //                setDuration(tx, score, DurationType::V_128TH);
            //                break;
        case DurationType::V_BREVE: set_duration(tx, score, DurationType::V_LONG); break;
        case DurationType::V_WHOLE: set_duration(tx, score, DurationType::V_BREVE); break;
        case DurationType::V_HALF: set_duration(tx, score, DurationType::V_WHOLE); break;
        case DurationType::V_QUARTER: set_duration(tx, score, DurationType::V_HALF); break;
        case DurationType::V_EIGHTH: set_duration(tx, score, DurationType::V_QUARTER); break;
        case DurationType::V_16TH: set_duration(tx, score, DurationType::V_EIGHTH); break;
        case DurationType::V_32ND: set_duration(tx, score, DurationType::V_16TH); break;
        case DurationType::V_64TH: set_duration(tx, score, DurationType::V_32ND); break;
        case DurationType::V_128TH: set_duration(tx, score, DurationType::V_64TH); break;
        case DurationType::V_256TH: set_duration(tx, score, DurationType::V_128TH); break;
        case DurationType::V_512TH: set_duration(tx, score, DurationType::V_256TH); break;
        case DurationType::V_1024TH: set_duration(tx, score, DurationType::V_512TH); break;
        default: break;
    }
}

void NoteInput::decrease_duration(Transaction &tx, Score *score) {
    switch (score->input_state().duration().type()) {
        case DurationType::V_LONG: set_duration(tx, score, DurationType::V_BREVE); break;
        case DurationType::V_BREVE: set_duration(tx, score, DurationType::V_WHOLE); break;
        case DurationType::V_WHOLE: set_duration(tx, score, DurationType::V_HALF); break;
        case DurationType::V_HALF: set_duration(tx, score, DurationType::V_QUARTER); break;
        case DurationType::V_QUARTER: set_duration(tx, score, DurationType::V_EIGHTH); break;
        case DurationType::V_EIGHTH: set_duration(tx, score, DurationType::V_16TH); break;
        case DurationType::V_16TH: set_duration(tx, score, DurationType::V_32ND); break;
        case DurationType::V_32ND: set_duration(tx, score, DurationType::V_64TH); break;
        case DurationType::V_64TH: set_duration(tx, score, DurationType::V_128TH); break;
        case DurationType::V_128TH: set_duration(tx, score, DurationType::V_256TH); break;
        case DurationType::V_256TH: set_duration(tx, score, DurationType::V_512TH); break;
        case DurationType::V_512TH:
            set_duration(tx, score, DurationType::V_1024TH);
            break;
            // cycle back from shortest to longest?
            //          case DurationType::V_1024TH:
            //                setDuration(tx, score, DurationType::V_LONG);
            //                break;
        default: break;
    }
}

//---------------------------------------------------------
//   realtimeAdvance
//---------------------------------------------------------

void NoteInput::realtime_advance(Transaction &tx, Score *score, bool allow_transposition, const std::vector<int> &active_midi_pitches) {
    InputState &is = score->input_state();
    if (!is.note_entry_mode()) {
        return;
    }

    Fraction ticks2measure_end = is.segment()->measure()->ticks() - is.segment()->rtick();
    if (!is.cr() || (is.cr()->ticks() != is.duration().fraction() && is.duration() < ticks2measure_end)) {
        score->set_note_rest(is.segment(), is.track(), NoteVal(), is.duration().fraction(), DirectionV::AUTO);
    }

    ChordRest *prev_cr_value = to_chord_rest(is.cr());
    if (is.beyond_score()) {
        score->append_measures(1);
    }

    is.move_to_next_input_pos();

    if (active_midi_pitches.empty()) {
        score->set_note_rest(is.segment(), is.track(), NoteVal(), is.duration().fraction(), DirectionV::AUTO);
    } else {
        Chord *prev_chord = prev_cr_value->is_chord() ? to_chord(prev_cr_value) : 0;
        bool part_of_chord = false;
        for (int pitch : active_midi_pitches) {
            NoteInput::add_tied_midi_pitch(tx, score, pitch, part_of_chord, prev_chord, allow_transposition);
            part_of_chord = true;
        }
    }

    if (prev_cr_value->measure() != is.segment()->measure()) {
        // just advanced across barline. Now simplify tied notes.
        Regroup::regroup_notes_and_rests(tx, score, prev_cr_value->measure()->tick(), is.segment()->measure()->tick(), is.track());
    }
}
