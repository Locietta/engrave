/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "transpose.h"

#include "../dom/factory.h"
#include "../dom/fret.h"
#include "../dom/harmony.h"
#include "../dom/keysig.h"
#include "../dom/linkedobjects.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/part.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "../dom/utils.h"
#include "editenharmonicspelling.h"
#include "editfretboarddiagram.h"
#include "editkeysig.h"
#include "iengravingconfiguration.h" // IWYU pragma: keep
#include "log.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

bool Transpose::transpose(Transaction &tx, Score *score, TransposeMode mode, TransposeDirection direction, Key tr_key,
                          int transpose_interval, bool tr_keys, bool transpose_chord_names, bool use_double_sharps_flats) {
    const Selection &selection = score->selection();
    bool result = true;
    bool range_selection = selection.is_range();
    staff_idx_t start_staff_idx = 0;
    staff_idx_t end_staff_idx = 0;
    Fraction start_tick = Fraction(0, 1);
    if (range_selection) {
        start_staff_idx = selection.staff_start();
        end_staff_idx = selection.staff_end();
        start_tick = selection.tick_start();
    }

    Staff *st = score->staff(start_staff_idx);

    Interval interval;
    if (mode != TransposeMode::DIATONICALLY) {
        if (mode == TransposeMode::TO_KEY) {
            // calculate interval from "transpose to key"
            // find the key of the first pitched staff
            Key key = Key::C;
            for (staff_idx_t i = start_staff_idx; i < end_staff_idx; ++i) {
                Staff *s = score->staff(i);
                if (s->is_pitched_staff(start_tick)) {
                    key = s->concert_key(start_tick);
                    // remember this staff to use as basis in transposing key signatures
                    st = s;
                    break;
                }
            }
            if (key != tr_key) {
                interval = keydiff2_interval(key, tr_key, direction);
            } else { //same key, which direction?
                if (direction == TransposeDirection::UP) {
                    interval = Interval(7, 12);
                } else if (direction == TransposeDirection::DOWN) {
                    interval = Interval(-7, -12);
                } else { //don't do anything for same key and closest direction
                    return true;
                }
            }
        } else {
            interval = Interval::all_intervals[transpose_interval];
            if (direction == TransposeDirection::DOWN) {
                interval.flip();
            }
        }

        if (!range_selection) {
            tr_keys = false;
        }
        bool full_octave = (interval.chromatic % ::mu::engraving::pitch_delta_octave) == 0;
        if (full_octave && (mode != TransposeMode::TO_KEY)) {
            tr_keys = false;
            transpose_chord_names = false;
        }
    } else { // diatonic transposition
        if (direction == TransposeDirection::DOWN) {
            transpose_interval *= -1;
        }
    }

    if (selection.is_list()) {
        const std::list<EngravingItem *> &selection_list = selection.unique_elements();
        for (EngravingItem *e : selection_list) {
            if (!e->staff() || e->staff()->staff_type(e->tick())->group() == StaffGroup::PERCUSSION) {
                continue;
            }
            if (e->staff()->primary_staff() && e->staff()->primary_staff()->staff_type(Fraction(0, 1))->group() == StaffGroup::PERCUSSION) {
                continue;
            }
            if (e->is_note()) {
                Note *note = to_note(e);
                if (!transpose_note(note, mode, transpose_interval, tr_keys, use_double_sharps_flats, interval)) {
                    result = false;
                }
            } else if (e->is_harmony() && transpose_chord_names) {
                EngravingObject *parent = e->parent();
                if (parent && parent->is_fret_diagram()) {
                    if (std::find(selection_list.begin(), selection_list.end(), parent) != selection_list.end()) {
                        continue;
                    }

                    FretDiagram *fd = to_fret_diagram(parent);
                    transpose_fret_diagram(tx, to_fret_diagram(fd), score, interval, mode, transpose_interval, tr_keys,
                                           use_double_sharps_flats);
                } else {
                    transpose_harmony(tx, to_harmony(e), score, interval, mode, transpose_interval, tr_keys, use_double_sharps_flats);
                }
            } else if (e->is_fret_diagram() && transpose_chord_names) {
                transpose_fret_diagram(tx, to_fret_diagram(e), score, interval, mode, transpose_interval, tr_keys, use_double_sharps_flats);
            } else if (e->is_key_sig() && mode != TransposeMode::DIATONICALLY && tr_keys) {
                // TODO: this currently is disabled in dialog
                // if we enabled it, then it will need work
                // probably the code should look more like the range selection code
                KeySig *ks = to_key_sig(e);
                if (!ks->is_atonal()) {
                    Key key = st->key(ks->tick());
                    Key c_key = st->concert_key(ks->tick());
                    KeySigEvent ke = ks->key_sig_event();
                    ke.set_concert_key(c_key);
                    ke.set_key(key);
                    tx.push(new ChangeKeySig(ks, ke, ks->show_courtesy()));
                }
            }
        }
    }

    //--------------------------
    // process range selection
    //--------------------------

    std::vector<Staff *> sl;
    for (staff_idx_t staff_idx = selection.staff_start(); staff_idx < selection.staff_end(); ++staff_idx) {
        Staff *s = score->staff(staff_idx);
        if (s->staff_type(Fraction(0, 1))->group() == StaffGroup::PERCUSSION) { // ignore percussion staff
            continue;
        }
        if (s->primary_staff() && s->primary_staff()->staff_type(Fraction(0, 1))->group() == StaffGroup::PERCUSSION) {
            continue;
        }
        if (muse::contains(sl, s)) {
            continue;
        }
        bool already_there = false;
        for (Staff *s2 : sl) {
            if (s2 == s || (s2->links() && s2->links()->contains(s))) {
                already_there = true;
                break;
            }
        }
        if (!already_there) {
            sl.push_back(s);
        }
    }
    std::vector<track_idx_t> tracks;
    for (Staff *s : sl) {
        track_idx_t idx = s->idx() * ::mu::engraving::voices;
        for (voice_idx_t i = 0; i < ::mu::engraving::voices; ++i) {
            if (score->selection_filter().can_select_voice(i)) {
                tracks.push_back(idx + i);
            }
        }
    }

    Segment *s1 = selection.start_segment();
    if (!s1) {
        return result;
    }
    // if range start on mmRest, get the actual segment instead
    if (s1->measure()->is_mm_rest()) {
        s1 = score->tick2segment(s1->tick(), true, s1->segment_type(), false);
    }
    // if range starts with first CR of measure
    // then start looping from very beginning of measure
    // so we include key signature and can transpose that if requested
    if (s1->rtick().is_zero()) {
        s1 = s1->measure()->first();
    }
    Segment *s2 = selection.end_segment();
    for (Segment *segment = s1; segment && segment != s2; segment = segment->next1()) {
        if (!segment->enabled()) {
            continue;
        }
        for (track_idx_t track : tracks) {
            if (score->staff(track / ::mu::engraving::voices)->staff_type(s1->tick())->group() == StaffGroup::PERCUSSION) {
                continue;
            }
            EngravingItem *e = segment->element(track);
            if (!e) {
                continue;
            }

            if (e->is_chord()) {
                Chord *chord = to_chord(e);
                const std::vector<Note *> nl = chord->notes();
                for (size_t note_idx = 0; note_idx < nl.size(); ++note_idx) {
                    if (!score->selection_filter().can_select_note_idx(note_idx, nl.size(), selection.range_contains_multi_note_chords())) {
                        continue;
                    }
                    Note *note = nl.at(note_idx);
                    if (!transpose_note(note, mode, transpose_interval, tr_keys, use_double_sharps_flats, interval)) {
                        result = false;
                    }
                }
                for (Chord *g : chord->grace_notes()) {
                    for (Note *n : g->notes()) {
                        if (!transpose_note(n, mode, transpose_interval, tr_keys, use_double_sharps_flats, interval)) {
                            result = false;
                        }
                    }
                }
            } else if (e->is_key_sig() && tr_keys && mode != TransposeMode::DIATONICALLY) {
                KeySig *ks = to_key_sig(e);
                Fraction tick = segment->tick();
                bool start_key = tick == s1->tick() && !is_first_system_key_sig(ks);
                bool add_key = ks->is_change();
                if ((start_key || add_key) && !ks->is_atonal()) {
                    Staff *staff = ks->staff();
                    Key o_key = ks->concert_key();
                    Key n_key = transpose_key(o_key, interval);
                    KeySigEvent ke = ks->key_sig_event();
                    // don't transpose instrument change KS, was automagically translated when precedent KS was changed
                    if (!ke.for_instrument_change() || start_key) {
                        ke.set_concert_key(n_key);
                    }
                    // undoChangeKey handles linked staves/parts and generating new keysigs as needed
                    // it always sets the keysig non-generated
                    // so only call it when needed
                    EditKeySig::undo_change_key_sig(tx, score, staff, tick, ke);
                }
            }
        }
        if (transpose_chord_names && (score->selection_filter().is_filtered(ElementsSelectionFilterTypes::CHORD_SYMBOL) ||
                                      score->selection_filter().is_filtered(ElementsSelectionFilterTypes::FRET_DIAGRAM))) {
            for (EngravingItem *e : segment->annotations()) {
                if ((!e->is_harmony() && !e->is_fret_diagram()) || (!muse::contains(tracks, e->track()))) {
                    continue;
                }

                if (e->is_harmony()) {
                    transpose_harmony(tx, to_harmony(e), score, interval, mode, transpose_interval, tr_keys, use_double_sharps_flats);
                }

                if (e->is_fret_diagram()) {
                    transpose_fret_diagram(tx, to_fret_diagram(e), score, interval, mode, transpose_interval, tr_keys,
                                           use_double_sharps_flats);
                }
            }
        }
    }
    //
    // create missing key signatures
    //
    if (tr_keys && (mode != TransposeMode::DIATONICALLY) && (s1->tick() == Fraction(0, 1))) {
        for (track_idx_t track : tracks) {
            if (track % ::mu::engraving::voices) {
                continue;
            }
            Segment *seg = score->first_measure()->undo_get_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
            KeySig *ks = to_key_sig(seg->element(track));
            if (!ks) {
                ks = Factory::create_key_sig(seg);
                ks->set_track(track);
                Key n_key = transpose_key(Key::C, interval);
                ks->set_key(n_key);
                score->undo_add_element(ks);
            }
        }
    }
    return result;
}

//---------------------------------------------------------
//   transposeKeys
//    key -   -7(Cb) - +7(C#)
//---------------------------------------------------------

void Transpose::transpose_keys(Transaction &tx, Score *score, staff_idx_t staff_start, staff_idx_t staff_end, const Fraction &ts,
                               const Fraction &tick_end, bool flip) {
    Fraction tick_start(ts);
    if (tick_start < Fraction(0, 1)) { // -1 and 0 are valid values to indicate start of score
        tick_start = Fraction(0, 1);
    }
    for (staff_idx_t staff_idx = staff_start; staff_idx < staff_end; ++staff_idx) {
        Staff *st = score->staff(staff_idx);
        if (st->staff_type(tick_start)->group() == StaffGroup::PERCUSSION) {
            continue;
        }

        bool create_key = tick_start.is_zero();
        for (Segment *s = score->first_segment(SegmentType::KEY_SIG); s; s = s->next1(SegmentType::KEY_SIG)) {
            if (!s->enabled() || s->tick() < tick_start) {
                continue;
            }
            if (tick_end != Fraction(-1, 1) && s->tick() >= tick_end) {
                break;
            }
            KeySig *ks = to_key_sig(s->element(staff_idx * ::mu::engraving::voices));
            if (!ks || ks->generated()) {
                continue;
            }
            if (!ks->is_atonal()) {
                Interval v = st->part()->instrument(s->tick())->transpose();
                KeySigEvent ke = st->key_sig_event(s->tick());
                PreferSharpFlat pref = ks->part()->prefer_sharp_flat();
                Key n_key = ke.concert_key();
                if (flip && !v.is_zero()) {
                    v.flip();
                    n_key = transpose_key(ke.concert_key(), v, pref);
                }

                ke.set_key(n_key);
                tx.push(new ChangeKeySig(ks, ke, ks->show_courtesy()));
            }
            if (s->tick().is_zero()) {
                create_key = false;
            }
        }
        if (create_key && score->first_measure()) {
            Segment *seg = score->first_measure()->undo_get_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
            seg->set_header(true);
            KeySig *ks = Factory::create_key_sig(seg);
            ks->set_track(staff_idx * ::mu::engraving::voices);
            Interval v = ks->part()->instrument()->transpose();
            Key c_key = Key::C;
            Key n_key = c_key;
            if (flip) {
                if (!v.is_zero()) {
                    v.flip();
                    n_key = transpose_key(Key::C, v, ks->part()->prefer_sharp_flat());
                }
            } else {
                c_key = transpose_key(Key::C, v);
                n_key = c_key;
            }
            ks->set_key(c_key, n_key);
            score->undo_add_element(ks);
        }
    }
}

//---------------------------------------------------------
//   transposeSemitone
//---------------------------------------------------------

void Transpose::transpose_semitone(Transaction &tx, Score *score, int step) {
    if (step == 0) {
        return;
    }

    TransposeDirection dir = step > 0 ? TransposeDirection::UP : TransposeDirection::DOWN;

    if (!transpose(tx, score, TransposeMode::BY_INTERVAL, dir, Key::C, 1, true, true, false)) {
        LOGD("Score::transposeSemitone: failed");
    } else {
        score->set_selection_changed(true);
    }
}

//---------------------------------------------------------
//   transposeDiatonicAlterations
//---------------------------------------------------------
void Transpose::transpose_diatonic_alterations(Transaction &tx, Score *score, TransposeDirection direction) {
    // Transpose current selection diatonically (up/down) while keeping degree alterations
    // Note: Score::transpose() absolutely requires valid selection before invocation.
    if (!score->selection().is_none()) {
        transpose(tx, score, TransposeMode::DIATONICALLY, direction, Key::C, 1, true, true, true);
        score->set_play_note(true);         // For when selection is a single note, also playback that note
        score->set_selection_changed(true); // This will update the on-screen keyboard
    }
}

//---------------------------------------------------------
//   transpositionChanged
//---------------------------------------------------------
void Transpose::transposition_changed(Transaction &tx, Score *score, Part *part, Interval old_v, Fraction tick_start, Fraction tick_end) {
    if (tick_start == Fraction(-1, 1)) {
        tick_start = Fraction(0, 1);
    }

    // transpose keys first
    std::set<Score *> scores;
    for (Staff *ls : part->staff(0)->staff_list()) {
        // TODO: special handling for linked staves within a score
        // could be useful for capo
        Score *ls_score = ls->score();
        if (muse::contains(scores, ls_score)) {
            continue;
        }
        scores.insert(ls_score);
        Part *lp = ls->part();
        if (!ls_score->style().style_b(Sid::CONCERT_PITCH)) {
            const TrackRange track_range = lp->track_range();
            transpose_keys(tx, ls_score, track2staff(track_range.start_track), track2staff(track_range.end_track), tick_start, tick_end,
                           true);
        }
    }

    // now transpose notes and chord symbols
    for (Segment *s = score->first_segment(SegmentType::DURATION); s; s = s->next1(SegmentType::DURATION)) {
        if (s->tick() < tick_start) {
            continue;
        }
        if (tick_end != Fraction(-1, 1) && s->tick() >= tick_end) {
            break;
        }
        Interval v = part->staff(0)->transpose(s->tick());
        v.flip();
        Interval diff_v(old_v.diatonic + v.diatonic, old_v.chromatic + v.chromatic);
        for (Staff *st : part->staves()) {
            if (st->staff_type(tick_start)->group() == StaffGroup::PERCUSSION) {
                continue;
            }
            track_idx_t t1 = st->idx() * ::mu::engraving::voices;
            track_idx_t t2 = t1 + ::mu::engraving::voices;
            for (track_idx_t track = t1; track < t2; ++track) {
                EngravingItem *e = s->element(track);
                if (e && e->is_chord()) {
                    Chord *c = to_chord(e);
                    for (Chord *gc : c->grace_notes()) {
                        for (Note *n : gc->notes()) {
                            int tpc = transpose_tpc(n->tpc1(), v, true);
                            n->undo_change_property(Pid::TPC2, tpc);
                        }
                    }
                    for (Note *n : c->notes()) {
                        int tpc = transpose_tpc(n->tpc1(), v, true);
                        n->undo_change_property(Pid::TPC2, tpc);
                    }
                }
                // find chord symbols
                for (EngravingItem *element : s->annotations()) {
                    if (element->track() != track || !element->is_harmony()) {
                        continue;
                    }
                    Harmony *h = to_harmony(element);
                    for (EngravingObject *score_element : h->link_list()) {
                        if (!score_element->style().style_b(Sid::CONCERT_PITCH)) {
                            undo_transpose_harmony(tx, to_harmony(score_element), diff_v, false);
                        }
                    }
                }
            }
        }
    }
}

void Transpose::transposition_changed(Transaction &tx, Score *score, Part *part, const Fraction &instrument_tick,
                                      Interval old_transposition) {
    Fraction tick_start = instrument_tick;
    Fraction tick_end = {-1, 1};

    auto main_instrument_end_it = part->instruments().upper_bound(tick_start.ticks());
    if (main_instrument_end_it != part->instruments().cend()) {
        tick_end = Fraction::from_ticks(main_instrument_end_it->first);
    }

    transposition_changed(tx, score, part, old_transposition, tick_start, tick_end);
}

//---------------------------------------------------------
//   transposeChord
//---------------------------------------------------------

void Transpose::transpose_chord(Chord *c, const Fraction &tick) {
    Interval dst_transpose = c->staff()->transpose(tick);

    if (dst_transpose.is_zero()) {
        for (Note *n : c->notes()) {
            n->set_tpc2(n->tpc1());
        }
    } else {
        dst_transpose.flip();
        for (Note *n : c->notes()) {
            n->set_tpc2(transpose_tpc(n->tpc1(), dst_transpose, true));
        }
    }
}

//---------------------------------------------------------
//   keydiff2Interval
//    keysig -   -7(Cb) - +7(C#)
//---------------------------------------------------------

Interval Transpose::keydiff2_interval(Key o_key, Key n_key, TransposeDirection dir) {
    int diatonic = tpc2step(key2_tpc(n_key)) - tpc2step(key2_tpc(o_key));
    int chromatic = ((int(n_key) - int(o_key)) + (diatonic * ::mu::engraving::tpc_delta_enharmonic)) / ::mu::engraving::tpc_delta_semitone;

    if ((dir == TransposeDirection::CLOSEST && chromatic > 6) || (dir == TransposeDirection::DOWN && chromatic > 0)) {
        diatonic -= 7;
        chromatic -= 12;
    } else if ((dir == TransposeDirection::CLOSEST && chromatic < -6) || (dir == TransposeDirection::UP && chromatic < 0)) {
        diatonic += 7;
        chromatic += 12;
    }

    return Interval(diatonic, chromatic);
}

bool Transpose::transpose_note(Note *note, TransposeMode mode, int transpose_interval, bool tr_keys, bool use_double_sharps_flats,
                               Interval interval) {
    if (note->dead_note() && note->configuration()->keep_dead_notes_unchanged_on_transpose()) {
        return true;
    }

    if (mode == TransposeMode::DIATONICALLY) {
        return note->transpose_diatonic(transpose_interval, tr_keys, use_double_sharps_flats);
    }

    return note->transpose(interval, use_double_sharps_flats);
}

void Transpose::transpose_harmony(Transaction &tx, Harmony *harmony, Score *score, Interval interval, TransposeMode mode,
                                  int transpose_interval, bool tr_keys, bool use_double_sharps_flats) {
    // TODO also source interval should reflect modified key (f.ex. by prefer flat)
    // now we calculate interval from first pitched staff
    // but even if it were right staff, this may differ each tick (actual key signature)
    // we can  add something like "concert pitch rootTpc" to harmony definition, or ...
    Interval kv = harmony->staff()->transpose(harmony->tick());
    Interval iv = harmony->part()->instrument(harmony->tick())->transpose();
    Interval h_interval((interval.diatonic - kv.diatonic + iv.diatonic), (interval.chromatic - kv.chromatic + iv.chromatic));

    // undoTransposeHarmony does not do links
    // because it is also used to handle transposing instruments
    // and score / parts could be in different concert pitch states
    for (EngravingObject *se : harmony->link_list()) {
        Harmony *h = to_harmony(se);
        if (mode == TransposeMode::DIATONICALLY) {
            undo_transpose_harmony_diatonic(tx, h, transpose_interval, use_double_sharps_flats, tr_keys);
        } else {
            undo_transpose_harmony(tx, h, h_interval, use_double_sharps_flats);
        }
    }
    score->rebuild_fret_box();
}

void Transpose::transpose_fret_diagram(Transaction &tx, FretDiagram *diagram, Score *score, Interval interval, TransposeMode mode,
                                       int transpose_interval, bool tr_keys, bool use_double_sharps_flats) {
    Harmony *harmony = diagram->harmony();

    if (harmony) {
        transpose_harmony(tx, harmony, score, interval, mode, transpose_interval, tr_keys, use_double_sharps_flats);

        std::vector<DiagramInfo> available_diagrams = diagram->patterns_from_harmony(harmony->plain_text());
        if (available_diagrams.empty()) {
            diagram->undo_fret_clear();
            MScore::set_error(MsError::TRANSPOSE_NO_FRET_DIAGRAM, true);
            return;
        }
        tx.push(new FretDataChange(diagram, harmony->plain_text()));

        score->rebuild_fret_box();
        return;
    }

    // No chord symbol attached. Create harmony from diagram, transpose it, then use the new harmony to update the fret diagram
    String pattern = diagram->pattern_from_diagram();
    std::vector<String> names = diagram->harmonies_from_pattern(pattern);
    if (names.empty()) {
        diagram->undo_fret_clear();
        MScore::set_error(MsError::TRANSPOSE_NO_FRET_DIAGRAM, true);
        return;
    }

    Fraction tick = Fraction(0, 1);
    Segment *seg = diagram->segment();
    if (seg) {
        tick = seg->tick();
    }
    Staff *staff = diagram->staff();
    Key key = staff ? staff->key(tick) : Key::C;

    String name = find_best_enharmonic_fit(names, key, score->style());

    // Transpose harmony without undo
    Interval kv = staff ? staff->transpose(tick) : Interval(0, 0);

    Part *part = diagram->part();
    Instrument *instrument = part ? part->instrument(tick) : nullptr;
    Interval iv = instrument ? instrument->transpose() : Interval(0, 0);

    Interval h_interval((interval.diatonic - kv.diatonic + iv.diatonic), (interval.chromatic - kv.chromatic + iv.chromatic));

    String transposed = transposed_chord_name(score, name, [&](int tpc) {
        if (mode == TransposeMode::DIATONICALLY) {
            return transpose_tpc_diatonic_by_key(tpc, transpose_interval, key, tr_keys, use_double_sharps_flats);
        }
        return transpose_tpc(tpc, h_interval, use_double_sharps_flats);
    });

    std::vector<DiagramInfo> available_diagrams = diagram->patterns_from_harmony(transposed);
    if (available_diagrams.empty()) {
        diagram->undo_fret_clear();
        MScore::set_error(MsError::TRANSPOSE_NO_FRET_DIAGRAM, true);
        return;
    }
    tx.push(new FretDataChange(diagram, transposed));

    score->rebuild_fret_box();
}

String Transpose::transposed_chord_name(Score *score, const String &name, const std::function<int(int tpc)> &transform_tpc) {
    Harmony h(score->dummy());
    h.set_harmony(name);
    for (HarmonyInfo *info : h.chords()) {
        info->set_root_tpc(transform_tpc(info->root_tpc()));
        info->set_bass_tpc(transform_tpc(info->bass_tpc()));
    }
    h.set_xml_text(h.harmony_name());
    return h.harmony_name();
}

String Transpose::find_best_enharmonic_fit(const std::vector<String> &notes, Key key, const MStyle &style) {
    std::vector<int> tpcs;
    tpcs.reserve(notes.size());
    NoteSpellingType harmony_spelling = style.style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();
    NoteCaseType harmony_case = NoteCaseType::AUTO;
    size_t idx = 0;
    for (const String &note : notes) {
        int tpc = convert_note(note, harmony_spelling, harmony_case, idx);
        tpcs.push_back(tpc);
    }

    int tpc = EditEnharmonicSpelling::best_enharmonic_fit(tpcs, key);

    if (tpc == Tpc::TPC_INVALID) {
        return notes.front();
    }

    String note = tpc2name(tpc, harmony_spelling, harmony_case);

    return note;
}

//---------------------------------------------------------
//   transposeTpcDiatonicByKey
//
// returns the tpc diatonically transposed by steps, using degrees of given key
// option to keep any alteration tpc had with respect to unaltered corresponding degree of key
// option to enharmonically reduce tpc using double alterations
//---------------------------------------------------------

int Transpose::transpose_tpc_diatonic_by_key(int tpc, int steps, Key key, bool keep_altered_degrees, bool use_double_sharps_flats) {
    if (!tpc_is_valid(tpc)) {
        return tpc;
    }

    // get step for tpc with alteration for key
    int alter;
    int step = tpc2step_by_key(tpc, key, alter);

    // transpose step and get tpc for step/key
    step += steps;
    int new_tpc = step2tpc_by_key(step, key);

    // if required, apply alteration to new tpc
    if (keep_altered_degrees) {
        new_tpc += alter * ::mu::engraving::tpc_delta_semitone;
    }

    // check results are in ranges
    return clamp_enharmonic(new_tpc, use_double_sharps_flats);
}

//---------------------------------------------------------
//   transposeKey
//---------------------------------------------------------

Key Transpose::transpose_key(Key key, const Interval &interval, PreferSharpFlat prefer) {
    int key_as_tpc = key2_tpc(key);
    Key new_key = tpc2_key(transpose_tpc(key_as_tpc, interval, false));

    // ignore prefer for octave transposing instruments
    if (interval.chromatic % ::mu::engraving::pitch_delta_octave == 0 && interval.diatonic % ::mu::engraving::step_delta_octave == 0) {
        prefer = PreferSharpFlat::NONE;
    }

    return clamp_key(new_key, prefer);
}

//---------------------------------------------------------
//   transposeTpc
//
// transposes a pitch spelling by a given interval.
// option to enharmonically reduce tpc using double alterations
//---------------------------------------------------------

int Transpose::transpose_tpc(int tpc, Interval interval, bool use_double_sharps_flats) {
    int delta_tpc =
        (interval.chromatic * ::mu::engraving::tpc_delta_semitone) - (interval.diatonic * ::mu::engraving::tpc_delta_enharmonic);
    if (!tpc_is_valid(tpc) || delta_tpc == 0) { // perfect unison & perfect octave
        return tpc;
    }
    return clamp_enharmonic(tpc + delta_tpc, use_double_sharps_flats);
}

//---------------------------------------------------------
//   undoTransposeHarmony
//---------------------------------------------------------

namespace {
struct TransposeHarmony : UndoableCommand {

public:
    TransposeHarmony(Harmony *harmony, Interval interval, bool use_double_sharps_flats)
        : harmony_(harmony), interval_(interval), use_double_sharps_flats_(use_double_sharps_flats) {}

    UNDO_TYPE(CommandType::TRANSPOSE_HARMONY)
    UNDO_NAME("TransposeHarmony")
    UNDO_CHANGED_OBJECTS({harmony_})

private:
    void flip() override;

    Harmony *harmony_ = nullptr;

    Interval interval_ = Interval(0, 0);
    bool use_double_sharps_flats_ = false;
};
} // namespace

void TransposeHarmony::flip() {
    Transpose::do_undo_transpose_harmony(harmony_, interval_, use_double_sharps_flats_);
    interval_.flip();
}

void Transpose::undo_transpose_harmony(Transaction &tx, Harmony *harmony, Interval interval, bool double_sharp_flat) {
    tx.push(new TransposeHarmony(harmony, interval, double_sharp_flat));
}

void Transpose::do_undo_transpose_harmony(Harmony *harmony, Interval interval, bool use_double_sharps_flats) {
    harmony->realized_harmony().set_dirty(true); // harmony should be re-realized after transposition

    for (HarmonyInfo *info : harmony->chords()) {
        info->set_root_tpc(Transpose::transpose_tpc(info->root_tpc(), interval, use_double_sharps_flats));
        info->set_bass_tpc(Transpose::transpose_tpc(info->bass_tpc(), interval, use_double_sharps_flats));
    }

    harmony->set_xml_text(harmony->harmony_name());
    harmony->trigger_layout();
}

//---------------------------------------------------------
//   undoTransposeHarmonyDiatonic
//---------------------------------------------------------

namespace {
struct TransposeHarmonyDiatonic : UndoableCommand {

public:
    TransposeHarmonyDiatonic(Harmony *harmony, int interval, bool use_double_sharps_flats, bool transpose_keys)
        : harmony_(harmony), interval_(interval), use_double_sharps_flats_(use_double_sharps_flats), transpose_keys_(transpose_keys) {}

    UNDO_TYPE(CommandType::TRANSPOSE_HARMONY)
    UNDO_NAME("TransposeHarmonyDiatonic")
    UNDO_CHANGED_OBJECTS({harmony_})

private:
    void flip() override;

    Harmony *harmony_ = nullptr;
    int interval_ = 0;
    bool use_double_sharps_flats_ = false;
    bool transpose_keys_ = false;
};
} // namespace

void TransposeHarmonyDiatonic::flip() {
    Transpose::do_undo_transpose_harmony_diatonic(harmony_, interval_, use_double_sharps_flats_, transpose_keys_);

    interval_ *= -1;
}

void Transpose::undo_transpose_harmony_diatonic(Transaction &tx, Harmony *harmony, int interval, bool double_sharp_flat,
                                                bool transpose_keys) {
    tx.push(new TransposeHarmonyDiatonic(harmony, interval, double_sharp_flat, transpose_keys));
}

void Transpose::do_undo_transpose_harmony_diatonic(Harmony *harmony, int interval, bool use_double_sharps_flats, bool transpose_keys) {
    harmony->realized_harmony().set_dirty(true); // harmony should be re-realized after transposition

    Fraction tick = Fraction(0, 1);
    Segment *seg = harmony->get_parent_seg();
    if (seg) {
        tick = seg->tick();
    }
    Key key = !harmony->staff() ? Key::C : harmony->staff()->key(tick);

    for (HarmonyInfo *info : harmony->chords()) {
        info->set_root_tpc(
            Transpose::transpose_tpc_diatonic_by_key(info->root_tpc(), interval, key, transpose_keys, use_double_sharps_flats));
        info->set_bass_tpc(
            Transpose::transpose_tpc_diatonic_by_key(info->bass_tpc(), interval, key, transpose_keys, use_double_sharps_flats));
    }

    harmony->set_xml_text(harmony->harmony_name());
    harmony->trigger_layout();
}
