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

#include "paste.h"

#include "../dom/articulation.h"
#include "../dom/beam.h"
#include "../dom/breath.h"
#include "../dom/chord.h"
#include "../dom/drumset.h"
#include "../dom/dynamic.h"
#include "../dom/factory.h"
#include "../dom/figuredbass.h"
#include "../dom/fret.h"
#include "../dom/harmony.h"
#include "../dom/image.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/note.h"
#include "../dom/part.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/sig.h"
#include "../dom/staff.h"
#include "../dom/tie.h"
#include "../dom/timesig.h"
#include "../dom/tremolosinglechord.h"
#include "../dom/tuplet.h"
#include "../dom/utils.h"
#include "draw/iimageprovider.h"
#include "editmeasures.h"
#include "editparentheses.h"
#include "editstaff.h"
#include "io/buffer.h"
#include "log.h"
#include "noteinput.h"
#include "rw/rwregister.h"
#include "transaction/transaction.h"
#include "transpose.h"

using namespace mu;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   pasteStaff
//    return false if paste fails
//---------------------------------------------------------
bool Paste::paste_staff(Transaction &, Score *, XmlReader &e, Segment *dst, staff_idx_t dst_staff, Fraction scale) {
    //! NOTE Needs refactoring - reading should be separated from insertion
    //! (we read the elements into some structure, then inserted them)
    return rw::RWRegister::reader()->paste_staff(e, dst, dst_staff, scale);
}

//---------------------------------------------------------
//   pasteChordRest
//---------------------------------------------------------

void Paste::paste_chord_rest(Transaction &tx, Score *score, ChordRest *cr, const Fraction &t) {
    Fraction tick(t);
    // LOGD("pasteChordRest %s at %d, len %d/%d", cr->typeName(), tick, cr->ticks().numerator(), cr->ticks().denominator() );

    Measure *measure = score->tick2measure(tick);
    if (!measure) {
        return;
    }

    int two_note_tremolo_factor = 1;
    if (cr->is_chord()) {
        Chord *chord = to_chord(cr);
        if (chord->v_staff_idx() >= chord->score()->nstaves()) {
            // check if staffMove moves a note to a
            // nonexistent staff
            chord->set_staff_move(0);
        }
        Transpose::transpose_chord(chord, tick);
        if (chord->tremolo_two_chord()) {
            two_note_tremolo_factor = 2;
        } else if (cr->duration_type_ticks() == (cr->ticks() * 2)) {
            // this could be the 2nd note of a two-note tremolo
            // check previous CR on same track, if it has a two-note tremolo, then set twoNoteTremoloFactor to 2
            Segment *seg = measure->undo_get_segment(SegmentType::CHORD_REST, tick);
            ChordRest *crt = seg->next_chord_rest(cr->track(), true);
            if (crt && crt->is_chord()) {
                Chord *chrt = to_chord(crt);
                TremoloTwoChord *tr = chrt->tremolo_two_chord();
                if (tr) {
                    two_note_tremolo_factor = 2;
                }
            }
        }
    }

    // we can paste a measure rest as such only at start of measure
    // and only if the lengths of the rest and measure match
    // otherwise, we need to convert to duration rest(s)
    // and potentially split the rest up (eg, 5/4 => whole + quarter)
    bool convert_measure_rest = cr->is_rest() && cr->duration_type().type() == DurationType::V_MEASURE &&
                                (tick != measure->tick() || cr->actual_ticks_at(tick) != measure->ticks());

    Fraction measure_end = measure->end_tick();
    bool is_grace = cr->is_chord() && to_chord(cr)->note_type() != NoteType::NORMAL;

    // adjust measures for measure repeat
    if (cr->is_measure_repeat()) {
        MeasureRepeat *mr = to_measure_repeat(cr);
        Measure *m = (mr->num_measures() == 4 ? measure->prev_measure() : measure);
        for (int i = 1; i <= mr->num_measures(); ++i) {
            tx.push(new ChangeMeasureRepeatCount(m, i, mr->staff_idx()));
            if (i < mr->num_measures()) {
                m->undo_set_no_break(true);
            }
            m = m->next_measure();
        }
    }

    // find out if the chordrest was only partially contained in the copied range
    bool partial_copy = false;
    if (cr->is_measure_repeat()) {
        partial_copy = to_measure_repeat(cr)->actual_ticks() != measure->ticks();
    } else if (!is_grace && !cr->tuplet()) {
        partial_copy = cr->duration_type_ticks() != (cr->ticks() * two_note_tremolo_factor);
    }

    // if note is too long to fit in measure, split it up with a tie across the barline
    // exclude tuplets from consideration
    // we have already disallowed a tuplet from crossing the barline, so there is no problem here
    // but due to rounding, it might appear from actualTicks() that the last note is too long by a couple of ticks

    Staff *stf = cr->staff();
    const bool should_split = tick + cr->actual_ticks_at(tick) > measure_end || partial_copy || convert_measure_rest;
    if (is_grace || cr->tuplet() || !should_split) {
        score->undo_add_cr(cr, measure, tick);
        return;
    }
    if (cr->is_chord()) {
        // split Chord
        Chord *c = to_chord(cr);
        Fraction rest = c->ticks();
        bool firstpart = true;
        while (rest.is_not_zero()) {
            measure = score->tick2measure(tick);
            Chord *c2 = firstpart ? c : to_chord(c->clone());
            if (!firstpart) {
                c2->remove_markings(true);
            }
            Fraction time_stretch = stf->time_stretch(tick);
            Fraction mlen = (measure->end_tick() - tick) * time_stretch;
            Fraction len = mlen > rest ? rest : mlen;
            std::vector<TDuration> dl =
                to_rhythmic_duration_list(len, false, (tick - measure->tick()) * time_stretch, score->sigmap()->timesig(tick).nominal(),
                                          measure, ::mu::engraving::max_dots, time_stretch);
            if (dl.empty()) {
                LOGD("Could not make durations for: %d/%d", len.numerator(), len.denominator());
                return;
            }
            TDuration d = dl[0];
            c2->set_duration_type(d);
            c2->set_ticks(d.fraction());
            score->undo_add_cr(c2, measure, tick);

            std::vector<Note *> nl1 = c->notes();
            std::vector<Note *> nl2 = c2->notes();

            if (!firstpart) {
                for (unsigned i = 0; i < nl1.size(); ++i) {
                    Tie *tie = Factory::create_tie(nl1[i]);
                    tie->set_start_note(nl1[i]);
                    tie->set_end_note(nl2[i]);
                    tie->set_tick(tie->start_note()->tick());
                    tie->set_tick2(tie->end_note()->tick());
                    tie->set_track(c->track());
                    Tie *tie2 = nl1[i]->tie_for();
                    if (tie2) {
                        nl2[i]->set_tie_for(nl1[i]->tie_for());
                        tie2->set_start_note(nl2[i]);
                    }
                    nl1[i]->set_tie_for(tie);
                    nl2[i]->set_tie_back(tie);
                }
            }
            rest -= c2->ticks();
            tick += c2->actual_ticks_at(tick);
            firstpart = false;
            c = c2;
        }
    } else if (cr->is_rest()) {
        // split Rest
        Rest *r = to_rest(cr);
        Fraction rest = r->ticks();

        bool firstpart = true;
        while (!rest.is_zero()) {
            Rest *r2 = firstpart ? r : to_rest(r->clone());
            measure = score->tick2measure(tick);
            Fraction time_stretch = stf->time_stretch(tick);
            Fraction mlen = (measure->end_tick() - tick) * time_stretch;
            Fraction len = rest > mlen ? mlen : rest;
            std::vector<TDuration> dl =
                to_rhythmic_duration_list(len, true, (tick - measure->tick()) * time_stretch, score->sigmap()->timesig(tick).nominal(),
                                          measure, ::mu::engraving::max_dots, time_stretch);
            if (dl.empty()) {
                LOGD("Could not make durations for: %d/%d", len.numerator(), len.denominator());
                return;
            }
            TDuration d = dl[0];
            r2->set_duration_type(d);
            r2->set_ticks(d.is_measure() ? measure->stretched_len(stf) : d.fraction());
            score->undo_add_cr(r2, measure, tick);
            rest -= r2->ticks();
            tick += r2->actual_ticks_at(tick);
            firstpart = false;
        }
    } else if (cr->is_measure_repeat()) {
        MeasureRepeat *mr = to_measure_repeat(cr);
        std::vector<TDuration> list = to_duration_list(mr->ticks(), true);
        for (auto dur : list) {
            Rest *r = Factory::create_rest(score->dummy(), dur);
            r->set_track(cr->track());
            Fraction rest = r->ticks();
            while (!rest.is_zero()) {
                Rest *r2 = to_rest(r->clone());
                measure = score->tick2measure(tick);
                Fraction time_stretch = stf->time_stretch(tick);
                Fraction mlen = (measure->end_tick() - tick) * time_stretch;
                Fraction len = rest > mlen ? mlen : rest;
                std::vector<TDuration> dl = to_duration_list(len, false);
                if (dl.empty()) {
                    LOGD("Could not make durations for: %d/%d", len.numerator(), len.denominator());
                    return;
                }
                TDuration d = dl[0];
                r2->set_ticks(d.fraction());
                r2->set_duration_type(d);
                score->undo_add_cr(r2, measure, tick);
                rest -= r2->ticks();
                tick += r2->actual_ticks_at(tick);
            }
            delete r;
        }
        delete cr;
    }
}

//---------------------------------------------------------
//   pasteSymbols
//
//    pastes a list of symbols into cr and following ChordRest's
//
//    (Note: info about delta ticks is currently ignored)
//---------------------------------------------------------
void Paste::paste_symbols(Transaction &, XmlReader &e, ChordRest *dst) {
    //! NOTE Needs refactoring - reading should be separated from insertion
    //! (we read the elements into some structure, then inserted them)
    rw::RWRegister::reader()->paste_symbols(e, dst);
}

bool Paste::repeat_list_selection(Transaction &tx, Score *score) {

    InputState &is = score->input_state();

    std::vector<Note *> notes = score->selection().note_list();
    std::sort(notes.begin(), notes.end(), [](const Note *a, const Note *b) {
        return std::make_tuple(a->track(), a->tick(), a->pitch()) < std::make_tuple(b->track(), b->tick(), b->pitch());
    });

    std::vector<EngravingItem *> to_select;
    std::unordered_set<const Chord *> found_chords;

    // Parenthesis logic: group new notes by the left parenthesis (if any) of their old equivalent. Once all
    // new notes have been created we can call cmdAddParentheses on each group then set their offset & visibility
    // Use a vector of pairs to preserve insertion order
    using NoteList = std::vector<Note *>;

    struct ParenGroup {
        NoteList notes;
        bool left_visible = true;
        bool right_visible = true;
        PointF left_offset;
        PointF right_offset;
    };

    std::vector<std::pair<const Parenthesis *, ParenGroup>> paren_entries;

    for (Note *n : notes) {
        if (n->is_grace() || n->incoming_partial_tie() || n->outgoing_partial_tie()) {
            continue;
        }

        const Chord *source_chord = n->chord();
        is.set_track(source_chord->track());

        const bool add_flag = muse::contains(found_chords, source_chord);
        if (!add_flag) {
            // If the note doesn't belong to a chord we've seen before...
            found_chords.emplace(source_chord);
            is.set_segment(source_chord->segment());
            is.move_to_next_input_pos();

            if (is.note_entry_mode()) {
                // In note input mode - use the duration of from the note input panel (InputState)...
                IF_ASSERT_FAILED(is.duration().is_valid()) {
                    LOGE() << "Invalid InputState duration";
                    is.set_duration(source_chord->duration_type());
                }
            } else {
                // Otherwise set it based on the duration of the previous chord...
                is.set_duration(source_chord->duration_type());
            }
        }

        NoteVal nval = n->note_val();
        Note *new_note = NoteInput::add_pitch(tx, score, nval, add_flag);
        IF_ASSERT_FAILED(new_note) { continue; }

        new_note->undo_change_property(Pid::VISIBLE, n->visible());

        Chord *new_chord = new_note->chord();

        new_chord->update_articulations(source_chord->articulation_symbol_ids());

        const TremoloSingleChord *old_tsc = source_chord->tremolo_single_chord();
        if (!new_chord->tremolo_single_chord() && old_tsc) {
            TremoloSingleChord *new_tsc = old_tsc->clone();
            new_tsc->set_ownership_parent(new_chord);
            new_tsc->set_track(new_chord->track());
            score->do_undo_add_element(new_tsc);
        }

        to_select.push_back(new_note);

        const NoteParenthesisInfo *paren_info = n->parenthesis_info();
        const Parenthesis *left_paren = paren_info ? paren_info->left_paren() : nullptr;
        if (!left_paren) {
            continue;
        }
        const Parenthesis *right_paren = paren_info->right_paren();
        IF_ASSERT_FAILED(right_paren) { continue; }

        NoteList notes_for_paren;
        notes_for_paren.emplace_back(new_note);

        // All tied notes should be parenthesized...
        Tie *tie = new_note->tie_back();
        while (tie) {
            Note *tied_note = tie->start_note();
            if (!tied_note) {
                break;
            }
            notes_for_paren.emplace_back(tied_note);
            tie = tied_note->tie_back();
        }

        auto search = std::find_if(paren_entries.begin(), paren_entries.end(),
                                   [left_paren](const std::pair<const Parenthesis *, ParenGroup> &p) { return p.first == left_paren; });
        if (search != paren_entries.end()) {
            NoteList &nl = search->second.notes;
            nl.insert(nl.end(), notes_for_paren.begin(), notes_for_paren.end());
            continue;
        }

        ParenGroup group;
        group.notes = std::move(notes_for_paren);
        group.left_visible = left_paren->visible();
        group.right_visible = right_paren->visible();
        group.left_offset = left_paren->offset();
        group.right_offset = right_paren->offset();
        paren_entries.emplace_back(left_paren, std::move(group));
    }

    for (auto &[paren, group] : paren_entries) {
        EditParentheses::add_parentheses_to_notes(tx, group.notes);

        // Restore the original parentheses' visibility and offset on each newly-created pair...
        std::unordered_set<const Parenthesis *> processed_parens;
        for (Note *note : group.notes) {
            const NoteParenthesisInfo *new_paren_info = note->parenthesis_info();
            Parenthesis *new_left_paren = new_paren_info ? new_paren_info->left_paren() : nullptr;
            if (!new_left_paren || muse::contains(processed_parens, static_cast<const Parenthesis *>(new_left_paren))) {
                continue;
            }
            processed_parens.insert(new_left_paren);

            new_left_paren->undo_set_visible(group.left_visible);
            new_left_paren->undo_change_property(Pid::OFFSET, group.left_offset);

            Parenthesis *new_right_paren = new_paren_info->right_paren();
            IF_ASSERT_FAILED(new_right_paren) { continue; }

            new_right_paren->undo_set_visible(group.right_visible);
            new_right_paren->undo_change_property(Pid::OFFSET, group.right_offset);
        }
    }

    score->select(to_select, SelectType::ADD);
    return !to_select.empty();
}

static ChordRest *replace_with_rest(ChordRest *target) {
    target->score()->undo_remove_element(target);
    return target->score()->add_rest(target->segment(), target->track(), target->ticks(), target->tuplet());
}

static Note *prepare_target(ChordRest *target, Note *with, const Fraction &duration) {
    if (!target->segment()->element(target->track())) {
        return nullptr; // target was removed by previous operation, ignore this
    }
    if (target->is_chord() && target->ticks() > duration) {
        target = replace_with_rest(target); // prevent unexpected note splitting
    }
    Segment *segment = target->segment();
    if (segment->measure()->is_mm_rest()) {
        Measure *m = segment->measure()->mm_rest_first();
        segment = m->find_segment(SegmentType::CHORD_REST, m->tick());
    }

    const Staff *staff = target->staff();
    const StaffGroup staff_group = staff->staff_type(segment->tick())->group();
    DirectionV stem_direction = DirectionV::AUTO;
    if (staff_group == StaffGroup::PERCUSSION) {
        const Drumset *ds = staff->part()->instrument(segment->tick())->drumset();
        DO_ASSERT(ds);

        if (ds) {
            stem_direction = ds->stem_direction(with->note_val().pitch);
        }
    }

    segment = target->score()->set_note_rest(segment, target->track(), with->note_val(), duration, stem_direction, false, {}, false,
                                             &target->score()->input_state());
    return to_chord(segment->next_chord_rest(target->track()))->up_note();
}

static EngravingItem *prepare_target(EngravingItem *target, Note *with, const Fraction &duration) {
    if (target->is_note() && to_note(target)->chord()->ticks() != duration) {
        return prepare_target(to_note(target)->chord(), with, duration);
    }
    if (target->is_chord_rest() &&
        (to_chord_rest(target)->ticks() != duration || to_chord_rest(target)->duration_type().type() == DurationType::V_MEASURE)) {
        return prepare_target(to_chord_rest(target), with, duration);
    }
    return target;
}

static EngravingItem *paste_system_object(Transaction &tx, EditData &src_data, EngravingItem *target) {
    if (!target) {
        return nullptr;
    }

    if (src_data.element && src_data.element->is_time_sig()) {
        // Don't allow copypasting time signatures
        return nullptr;
    }

    Score *target_score = target->score();
    Staff *target_staff = target->staff();
    if (!target_score || !target_staff) {
        return nullptr;
    }

    // System objects can only be pasted on the *top* staff of an instrument
    Part *target_part = target_staff->part();
    target_staff = target_part ? target_part->staves().front() : nullptr;
    if (!target_staff) {
        return nullptr;
    }

    if (target_staff == target_score->staff(0) || target_staff->is_system_object_staff()) {
        return target->drop(tx, src_data);
    }

    tx.push(new AddSystemObjectStaff(target_staff));

    const std::vector<EngravingItem *> top_system_objects = collect_system_objects(target_score);
    const staff_idx_t staff_idx = target_staff->idx();
    const Fraction target_tick = target->tick();

    EngravingItem *pasted_item = nullptr;

    for (EngravingItem *obj : top_system_objects) {
        const bool visible = obj->type() == src_data.drop_element->type() && obj->tick() == target_tick;

        EngravingItem *copy = obj->linked_clone();
        copy->set_visible(visible);
        copy->set_staff_idx(staff_idx);
        target_score->undo_add_element(copy, false /*addToLinkedStaves*/);

        if (visible) {
            pasted_item = copy;
        }
    }

    if (!pasted_item) {
        pasted_item = target->drop(tx, src_data);
    }

    return pasted_item;
}

//---------------------------------------------------------
//   cmdPaste
//---------------------------------------------------------

} // namespace mu::engraving

bool Paste::paste_staff_list(Transaction &tx, Score *score, muse::ByteArray &data, Fraction scale) {
    if (MScore::debug_mode) {
        LOGD() << "Pasting staff list: " << data.data();
    }

    ChordRest *cr = nullptr;
    if (score->selection().is_range()) {
        cr = score->selection().first_chord_rest();
    } else if (score->selection().is_single()) {
        EngravingItem *e = score->selection().element();
        Measure *measure = e->find_measure();
        cr = measure ? measure->find_chord_rest(e->tick(), e->track()) : nullptr;
        if (!cr) {
            LOGE() << "Cannot paste staff list onto " << e->type_name();
            MScore::set_error(MsError::DEST_NO_CR);
            return false;
        }
    }

    if (!cr) {
        MScore::set_error(MsError::NO_DEST);
        return false;
    }

    if (cr->tuplet() && cr->tick() != cr->top_tuplet()->tick()) {
        MScore::set_error(MsError::DEST_TUPLET);
        return false;
    }

    XmlReader xml_reader(data);
    return paste_staff(tx, score, xml_reader, cr->segment(), cr->staff_idx(), scale);
}

bool Paste::paste_symbol_list(Transaction &tx, Score *score, muse::ByteArray &data) {
    if (MScore::debug_mode) {
        LOGD() << "Pasting element list: " << data.data();
    }

    ChordRest *cr = nullptr;
    if (score->selection().is_range()) {
        cr = score->selection().first_chord_rest();
    } else if (score->selection().is_single()) {
        EngravingItem *e = score->selection().element();
        Measure *measure = e->find_measure();
        cr = measure ? measure->find_chord_rest(e->tick(), e->track()) : nullptr;
        if (!cr) {
            LOGE() << "Cannot paste element list onto " << e->type_name();
            MScore::set_error(MsError::DEST_NO_CR);
            return false;
        }
    }

    if (!cr) {
        MScore::set_error(MsError::NO_DEST);
        return false;
    }

    XmlReader xml_reader(data);
    paste_symbols(tx, xml_reader, cr);
    return true;
}
