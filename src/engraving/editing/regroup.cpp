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

#include "regroup.h"

#include "../dom/chord.h"
#include "../dom/factory.h"
#include "../dom/input.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/sig.h"
#include "../dom/slur.h"
#include "../dom/staff.h"
#include "../dom/tie.h"
#include "../dom/utils.h"
#include "editspanner.h"
#include "log.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   regroupNotesAndRests
//    * combine consecutive rests into fewer rests of longer duration.
//    * combine tied notes/chords into fewer notes of longer duration.
//    Only operates on one voice - protects manual layout adjustment, etc.
//---------------------------------------------------------

void Regroup::regroup_notes_and_rests(Transaction &tx, Score *score, const Fraction &start_tick, const Fraction &end_tick,
                                      track_idx_t track) {
    InputState &is = score->input_state();
    Segment *input_segment = is.segment(); // store this so we can get back to it later.
    Segment *seg = score->tick2segment(start_tick, true, SegmentType::CHORD_REST);
    for (Measure *msr = seg->measure(); msr && msr->tick() < end_tick; msr = msr->next_measure()) {
        Fraction max_tick = end_tick > msr->end_tick() ? msr->end_tick() : end_tick;
        if (!seg || seg->measure() != msr) {
            seg = msr->first(SegmentType::CHORD_REST);
        }
        for (; seg; seg = seg->next(SegmentType::CHORD_REST)) {
            ChordRest *curr = seg->cr(track);
            if (!curr) {
                continue; // this voice is empty here (CR overlaps with CR in other track)
            }
            if (curr->end_tick() > max_tick) {
                break; // outside range
            }
            if (curr->is_rest() && !(curr->tuplet()) && !(to_rest(curr)->is_gap())) {
                // combine consecutive rests
                ChordRest *last_rest = curr;
                for (Segment *s = seg->next(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                    ChordRest *cr = s->cr(track);
                    if (!cr) {
                        continue; // this voice is empty here
                    }
                    if (!cr->is_rest() || cr->tuplet() || cr->end_tick() > max_tick || to_rest(cr)->is_gap()) {
                        break; // next element in the same voice is not a rest, or it exceeds the selection, or it is a gap
                    }
                    last_rest = cr;
                }
                Fraction rest_ticks = (last_rest->end_tick() - curr->tick()) * curr->staff()->time_stretch(curr->tick());
                seg = score->set_note_rest(seg, curr->track(), NoteVal(), rest_ticks, DirectionV::AUTO, false, {}, true);
            } else if (curr->is_chord()) {
                // combine tied chords
                Chord *chord = to_chord(curr);
                Chord *last_tied_chord = chord;
                for (Chord *next = chord->next_tied_chord(); next && next->end_tick() <= max_tick; next = next->next_tied_chord()) {
                    last_tied_chord = next;
                }
                if (!last_tied_chord) {
                    last_tied_chord = chord;
                }
                Fraction note_ticks = (last_tied_chord->end_tick() - chord->tick()) * chord->staff()->time_stretch(chord->tick());
                if (!(curr->tuplet())) {
                    // store start/end note for backward/forward ties ending/starting on the group of notes being rewritten
                    size_t num_notes = chord->notes().size();
                    std::vector<Note *> tie_back(num_notes);
                    std::vector<Note *> tie_for(num_notes);
                    for (size_t i = 0; i < num_notes; i++) {
                        Note *n = chord->notes()[i];
                        Note *nn = last_tied_chord->notes()[i];
                        if (n->tie_back()) {
                            tie_back[i] = n->tie_back()->start_note();
                        } else {
                            tie_back[i] = 0;
                        }
                        if (nn->tie_for()) {
                            tie_for[i] = nn->tie_for()->end_note();
                        } else {
                            tie_for[i] = 0;
                        }
                    }
                    Fraction tick = seg->tick();
                    track_idx_t tr = chord->track();
                    Fraction sd = note_ticks;
                    std::vector<Tie *> ties;
                    Segment *segment = seg;
                    ChordRest *cr = to_chord_rest(segment->element(tr));
                    Chord *nchord = to_chord(chord->clone());
                    for (size_t i = 0; i < num_notes; i++) { // strip ties from cloned chord
                        Note *n = nchord->notes()[i];
                        if (Tie *tie_for2 = n->tie_for()) {
                            n->set_tie_for(nullptr);
                            delete tie_for2;
                        }
                        if (Tie *tie_back2 = n->tie_back()) {
                            n->set_tie_back(nullptr);
                            delete tie_back2;
                        }
                    }
                    Chord *start_chord = nchord;
                    Measure *measure = nullptr;
                    bool firstpart = true;
                    for (;;) {
                        if (tr % ::mu::engraving::voices) {
                            score->expand_voice(segment, tr);
                        }
                        // the returned gap ends at the measure boundary or at tuplet end
                        Fraction dd = score->make_gap(segment, tr, sd, cr->tuplet());
                        if (dd.is_zero()) {
                            break;
                        }
                        measure = segment->measure();
                        std::vector<TDuration> dl;
                        dl = to_rhythmic_duration_list(dd, false, segment->rtick(), score->sigmap()->timesig(tick.ticks()).nominal(),
                                                       measure, 1, score->staff(track2staff(track))->time_stretch(tick));
                        size_t n = dl.size();
                        for (size_t i = 0; i < n; ++i) {
                            const TDuration &d = dl[i];
                            Chord *nchord2 = to_chord(nchord->clone());
                            if (!firstpart) {
                                nchord2->remove_markings(true);
                            }
                            nchord2->set_duration_type(d);
                            nchord2->set_ticks(d.fraction());
                            std::vector<Note *> nl1 = nchord->notes();
                            std::vector<Note *> nl2 = nchord2->notes();
                            if (!firstpart) {
                                for (size_t j = 0; j < nl1.size(); ++j) {
                                    Tie *tie = Factory::create_tie(nl1[j]);
                                    tie->set_start_note(nl1[j]);
                                    tie->set_end_note(nl2[j]);
                                    tie->set_tick(tie->start_note()->tick());
                                    tie->set_tick2(tie->end_note()->tick());
                                    tie->set_track(tr);
                                    nl1[j]->set_tie_for(tie);
                                    nl2[j]->set_tie_back(tie);
                                    ties.push_back(tie);
                                }
                            }
                            score->undo_add_cr(nchord2, measure, tick);
                            segment = nchord2->segment();
                            tick += nchord2->actual_ticks();
                            nchord = nchord2;
                            firstpart = false;
                        }
                        sd -= dd;
                        if (sd.is_zero()) {
                            break;
                        }
                        Segment *nseg = score->tick2segment(tick, false, SegmentType::CHORD_REST);
                        if (nseg == 0) {
                            break;
                        }
                        segment = nseg;
                        cr = to_chord_rest(segment->element(tr));
                        if (cr == 0) {
                            if (tr % ::mu::engraving::voices) {
                                cr = score->add_rest(segment, tr, TDuration(DurationType::V_MEASURE), 0);
                            } else {
                                break;
                            }
                        }
                    }
                    if (is.slur()) {
                        // extend slur
                        is.slur()->undo_change_property(Pid::SPANNER_TICKS, nchord->tick() - is.slur()->tick());
                        for (EngravingObject *e : is.slur()->link_list()) {
                            Slur *slur = to_slur(e);
                            for (EngravingObject *ee : nchord->link_list()) {
                                EngravingItem *e1 = static_cast<EngravingItem *>(ee);
                                if (e1->score() == slur->score() && e1->track() == slur->track2()) {
                                    tx.push(new ChangeSpannerElements(slur, slur->start_element(), e1));
                                    break;
                                }
                            }
                        }
                    }
                    // recreate previously stored pending ties
                    for (size_t i = 0; i < num_notes; i++) {
                        Note *n = start_chord->notes()[i];
                        Note *nn = nchord->notes()[i];
                        if (tie_back[i]) {
                            Tie *tie = Factory::create_tie(tie_back[i]);
                            tie->set_start_note(tie_back[i]);
                            tie->set_end_note(n);
                            tie->set_tick(tie->start_note()->tick());
                            tie->set_tick2(tie->end_note()->tick());
                            tie->set_track(track);
                            n->set_tie_back(tie);
                            tie_back[i]->set_tie_for(tie);
                            ties.push_back(tie);
                        }
                        if (tie_for[i]) {
                            Tie *tie = Factory::create_tie(nn);
                            tie->set_start_note(nn);
                            tie->set_end_note(tie_for[i]);
                            tie->set_tick(tie->start_note()->tick());
                            tie->set_tick2(tie->end_note()->tick());
                            tie->set_track(track);
                            nn->set_tie_for(tie);
                            tie_for[i]->set_tie_back(tie);
                            ties.push_back(tie);
                        }
                    }
                    if (!ties.empty()) { // at least one tie was created
                        for (Tie *tie : ties) {
                            score->undo_add_element(tie);
                        }
                        score->connect_ties();
                    }
                }
            }
        }
    }
    // now put the input state back where it was before
    is.set_segment(input_segment);
}

void Regroup::regroup_notes_and_rests_in_selection(Transaction &tx, Score *score) {
    bool no_selection = score->selection().is_none();
    if (no_selection) {
        score->cmd_select_all();
    } else if (!score->selection().is_range()) {
        LOGD("no system or staff selected");
        return;
    }

    // save selection values because selection changes during grouping
    Fraction s_tick = score->selection().tick_start();
    Fraction e_tick = score->selection().tick_end();
    staff_idx_t s_staff = score->selection().staff_start();
    staff_idx_t e_staff = score->selection().staff_end();
    Box *s_box = score->selection().start_box();
    Box *e_box = score->selection().end_box();

    for (staff_idx_t staff = s_staff; staff < e_staff; staff++) {
        track_idx_t s_track = staff * ::mu::engraving::voices;
        track_idx_t e_track = s_track + ::mu::engraving::voices;
        for (track_idx_t track = s_track; track < e_track; track++) {
            if (score->selection_filter().can_select_voice(track)) {
                regroup_notes_and_rests(tx, score, s_tick, e_tick, track);
            }
        }
    }

    if (no_selection) {
        score->deselect_all();
        return;
    }

    // Reset selection to original selection
    score->selection().set_range_ticks(s_tick, e_tick, s_staff, e_staff, s_box, e_box);
    score->selection().update_selected_elements();
}
