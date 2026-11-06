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

#include "edittimesig.h"

#include <map>
#include <tuple>
#include <vector>

#include "../dom/excerpt.h"
#include "../dom/factory.h"
#include "../dom/layoutbreak.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/range.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/sig.h"
#include "../dom/slur.h"
#include "../dom/staff.h"
#include "../dom/system.h"
#include "../dom/timesig.h"
#include "../dom/tremolotwochord.h"
#include "addremoveelement.h"
#include "editmeasures.h"
#include "editspanner.h"
#include "edittremolo.h"
#include "global/containers.h"
#include "log.h"
#include "transaction/transaction.h"
#include "transaction/undostack.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   rewriteMeasures
//    rewrite all measures from startMeasure to endMeasure (including)
//    If staffIdx is valid (>= 0), then rewrite a local
//    timesig change.
//---------------------------------------------------------

bool EditTimeSig::rewrite_measures(Transaction &tx, Score *score, Measure *start_measure, Measure *end_measure,
                                   const Fraction &new_time_sig, staff_idx_t staff_idx) {
    if (staff_idx != muse::nidx) {
        // local timesig
        // don't actually rewrite, just update measure rest durations
        // abort if there is anything other than measure rests in range
        track_idx_t strack = staff_idx * ::mu::engraving::voices;
        track_idx_t etrack = strack + ::mu::engraving::voices;
        for (Measure *m = start_measure;; m = m->next_measure()) {
            for (Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                for (track_idx_t track = strack; track < etrack; ++track) {
                    ChordRest *cr = to_chord_rest(s->element(track));
                    if (!cr) {
                        continue;
                    }
                    if (cr->is_rest() && cr->duration_type() == DurationType::V_MEASURE) {
                        cr->undo_change_property(Pid::DURATION, new_time_sig);
                    } else {
                        return false;
                    }
                }
            }
            if (m == end_measure) {
                break;
            }
        }
        return true;
    }

    bool fmr = true;

    // Format: chord 1 tick, chord 2 tick, tremolo, track
    std::vector<std::tuple<Fraction, Fraction, TremoloTwoChord *, track_idx_t>> tremolo_chord_ticks;

    track_idx_t strack, etrack;
    if (staff_idx == muse::nidx) {
        strack = 0;
        etrack = score->ntracks();
    } else {
        strack = staff_idx * ::mu::engraving::voices;
        etrack = strack + ::mu::engraving::voices;
    }

    std::vector<Segment *> end_of_measure_time_sigs_to_remove;

    for (Measure *m = start_measure; m; m = m->next_measure()) {
        if (!m->is_full_measure_rest()) {
            fmr = false;
        }

        for (Segment *s = m->first(); s; s = s->next()) {
            if (!s->is_chord_rest_type() && !(s->is_time_sig_type() && s->end_of_measure_change())) {
                continue;
            }

            if (s->is_time_sig_type()) {
                end_of_measure_time_sigs_to_remove.push_back(s);
                continue;
            }

            for (track_idx_t track = strack; track < etrack; ++track) {
                ChordRest *cr = to_chord_rest(s->element(track));
                if (cr && cr->is_chord()) {
                    Chord *chord = to_chord(cr);
                    if (chord->tremolo_two_chord()) {
                        TremoloTwoChord *trem = chord->tremolo_two_chord();

                        // Don't add same chord twice
                        if (trem->chord2() == chord) {
                            continue;
                        }

                        std::tuple<Fraction, Fraction, TremoloTwoChord *, track_idx_t> new_p = {
                            cr->tick(), trem->chord2()->segment()->tick(), trem, track};
                        tremolo_chord_ticks.push_back(new_p);
                    }
                }
            }
        }

        if (m == end_measure) {
            break;
        }
    }

    if (!fmr) {
        // check for local time signatures
        for (Measure *m = start_measure; m; m = m->next_measure()) {
            for (size_t si = 0; si < score->nstaves(); ++si) {
                if (score->staff(si)->time_stretch(m->tick()) != Fraction(1, 1)) {
                    // we cannot change a staff with a local time signature
                    return false;
                }
                if (m == end_measure) {
                    break;
                }
            }
        }
    }

    for (Segment *seg : end_of_measure_time_sigs_to_remove) {
        score->do_undo_remove_element(seg);
    }

    ScoreRange range;
    Measure *next_measure = end_measure->next_measure();
    Segment *final_seg = end_measure->last();
    if (next_measure) {
        final_seg = next_measure->first();
    }
    range.read(start_measure->first(), final_seg);

    //
    // calculate number of required measures = newMeasures
    //
    Fraction ticks = range.ticks().is_not_zero() ? range.ticks() : score->end_tick() - start_measure->first()->tick();
    Fraction k = ticks / new_time_sig;
    int new_measures = (k.numerator() + k.denominator() - 1) / k.denominator();

    Fraction new_duration = new_time_sig * Fraction(new_measures, 1);

    // evtl. we have to fill the last measure
    Fraction fill = new_duration - ticks;
    range.fill(fill);

    for (Score *s : score->score_list()) {
        Measure *m1 = s->tick2measure(start_measure->tick());
        Measure *m2 = s->tick2measure(end_measure->tick());

        Fraction tick1 = m1->tick();
        Fraction tick2 = m2->end_tick();
        auto spanners = s->spanner_map().find_overlapping(tick1.ticks(), tick2.ticks());
        for (auto i : spanners) {
            if (i.value->tick() >= tick1 && i.value->tick() < tick2) {
                score->do_undo_remove_element(i.value);
            }
        }
        s->undo_remove_measures(m1, m2, true);

        Measure *new_first_measure = nullptr;
        Measure *new_last_measure = nullptr;
        Fraction tick = start_measure->tick();
        for (int i = 0; i < new_measures; ++i) {
            Measure *m = Factory::create_measure(s);
            m->set_prev(new_last_measure);
            if (new_last_measure) {
                new_last_measure->set_next(m);
            }
            m->set_timesig(new_time_sig);
            m->set_ticks(new_time_sig);
            m->set_tick(tick);
            tick += m->ticks();
            new_last_measure = m;
            if (new_first_measure == 0) {
                new_first_measure = m;
            }
        }
        //
        // insert new calculated measures
        //
        new_first_measure->set_prev(m1->prev());
        new_last_measure->set_next(m2->next());
        tx.push(new InsertMeasures(new_first_measure, new_last_measure));
    }
    if (!fill.is_zero()) {
        score->undo_insert_time(end_measure->end_tick(), fill);
    }

    if (!range.write(score->master_score(), start_measure->tick())) {
        return false;
    }

    for (Score *s : score->score_list()) {
        s->connect_ties(true);
    }

    // reset start and end elements for slurs that overlap the rewritten measures
    for (auto spanner : score->spanner_map().find_overlapping(start_measure->tick().ticks(), end_measure->tick().ticks())) {
        Slur *slur = (spanner.value->is_slur() ? to_slur(spanner.value) : nullptr);
        if (slur) {
            EngravingItem *start_el = slur->start_element();
            EngravingItem *end_el = slur->end_element();
            if (!start_el || !end_el) {
                continue;
            }
            tx.push(new ChangeStartEndSpanner(spanner.value, slur->find_start_cr(), slur->find_end_cr()));
        }
    }
    // Attempt to move tremolos to correct chords
    for (auto trem_pair : tremolo_chord_ticks) {
        Fraction chord1_tick = std::get<0>(trem_pair);
        Fraction chord2_tick = std::get<1>(trem_pair);
        TremoloTwoChord *trem = std::get<2>(trem_pair);
        track_idx_t track = std::get<3>(trem_pair);

        tx.push(new MoveTremolo(trem->score(), chord1_tick, chord2_tick, trem, track));
    }

    if (score->note_entry_mode()) {
        // set input cursor to possibly re-written segment
        Fraction ic_tick = score->input_state().tick();
        Segment *ic_segment = score->tick2segment(ic_tick, false, SegmentType::CHORD_REST);
        if (!ic_segment) {
            // this can happen if cursor was on a rest
            // and in the rewriting it got subsumed into a full measure rest
            Measure *ic_measure = score->tick2measure(ic_tick);
            if (!ic_measure) { // shouldn't happen, but just in case
                ic_measure = score->first_measure();
            }
            ic_segment = ic_measure->first(SegmentType::CHORD_REST);
        }
        score->input_state().set_segment(ic_segment);
    }

    return true;
}

//---------------------------------------------------------
//   rewriteMeasures
//    rewrite all measures up to the next time signature or section break
//---------------------------------------------------------

bool EditTimeSig::rewrite_measures(Transaction &tx, Score *score, Measure *fm, const Fraction &ns, staff_idx_t staff_idx) {
    Measure *lm = fm;
    Measure *fm1 = fm;
    Measure *nm = nullptr;
    LayoutBreak *section_break = nullptr;
    Segment *end_of_measure_time_sig_seg = nullptr;

    //
    // split into Measure segments fm-lm
    //
    auto found_lm = [fm, staff_idx, &end_of_measure_time_sig_seg](MeasureBase *cur_meas, Measure *cur_lm) {
        if (!cur_meas || !cur_meas->is_measure() || cur_lm->section_break()) {
            return true;
        }
        Segment *time_sig_seg = to_measure(cur_meas)->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
        if (!time_sig_seg && cur_meas->prev_measure()) {
            // find time sig at end of previous measure
            Measure *prev_measure = cur_meas->prev_measure();
            time_sig_seg = prev_measure->find_segment_r(SegmentType::TIME_SIG, prev_measure->ticks());
            if (time_sig_seg) {
                end_of_measure_time_sig_seg = time_sig_seg;
            }
        }
        if (time_sig_seg && cur_meas != fm) {
            return staff_idx == muse::nidx || time_sig_seg->element(staff2track(staff_idx));
        }
        return false;
    };
    for (MeasureBase *measure = fm;; measure = measure->next()) {
        if (found_lm(measure, lm)) {
            // save section break to reinstate after rewrite
            LayoutBreak *layout_break = lm->section_break_element();

            if (lm && layout_break) {
                section_break = Factory::copy_layout_break(*layout_break);
            }

            if (!rewrite_measures(tx, score, fm1, lm, ns, staff_idx)) {
                if (staff_idx != muse::nidx) {
                    MScore::set_error(MsError::CANNOT_CHANGE_LOCAL_TIMESIG_MEASURE_NOT_EMPTY);
                    // restore measure rests that were prematurely modified
                    Fraction fr(score->staff(staff_idx)->time_sig(fm->tick())->sig());
                    for (Measure *m = fm1; m; m = m->next_measure()) {
                        ChordRest *cr = m->find_chord_rest(m->tick(), staff_idx * ::mu::engraving::voices);
                        if (cr && cr->is_rest() && cr->duration_type() == DurationType::V_MEASURE) {
                            cr->undo_change_property(Pid::DURATION, fr);
                        } else {
                            break;
                        }
                    }
                }
                for (Measure *m = fm1; m; m = m->next_measure()) {
                    if (m->first(SegmentType::TIME_SIG)) {
                        break;
                    }
                    Fraction fr(ns);
                    m->undo_change_property(Pid::TIMESIG_NOMINAL, fr);
                }
                return false;
            }

            // after rewrite, lm is not necessarily valid
            // m is first MeasureBase after rewritten range
            // m->prevMeasure () is new last measure of range
            // set nm to first true Measure after rewritten range
            // we may use this to reinstate time signatures
            if (measure && measure->prev_measure()) {
                nm = measure->prev_measure()->next_measure();
            } else {
                nm = nullptr;
            }

            if (section_break) {
                // reinstate section break, then stop rewriting
                if (measure && measure->prev_measure()) {
                    section_break->set_ownership_parent(measure->prev_measure());
                    score->undo_add_element(section_break);
                } else if (!measure) {
                    section_break->set_ownership_parent(score->last_measure());
                    score->undo_add_element(section_break);
                } else {
                    LOGD("unable to restore section break");
                    nm = nullptr;
                    section_break = nullptr;
                }
                break;
            }

            // stop rewriting at end of score
            // or at a measure (which means we found a time signature segment)
            if (!measure || measure->is_measure()) {
                break;
            }

            // skip frames
            while (!measure->is_measure()) {
                LayoutBreak *layout_break2 = measure->section_break_element();

                if (layout_break2) {
                    // frame has a section break; we can stop skipping ahead
                    section_break = layout_break2;
                    break;
                }
                measure = measure->next();
                if (!measure) {
                    break;
                }
            }
            // stop rewriting if we encountered a section break on a frame
            // or if there is a time signature on first measure after the frame
            if (section_break || (measure && to_measure(measure)->first(SegmentType::TIME_SIG))) {
                break;
            }

            // set up for next range to rewrite
            fm1 = to_measure(measure);
            if (fm1 == 0) {
                break;
            }
        }

        // if we didn't break the loop already,
        // we must have an ordinary measure
        // add measure to range to rewrite
        lm = to_measure(measure);
    }

    // if any staves don't have time signatures at the point where we stopped,
    // we need to reinstate their previous time signatures
    if (!nm) {
        return true;
    }
    Segment *time_sig_seg = nm->undo_get_segment(SegmentType::TIME_SIG, nm->tick());

    for (size_t i = 0; i < score->nstaves(); ++i) {
        if (staff_idx != muse::nidx && i != staff_idx) {
            continue;
        }
        if (!time_sig_seg->element(staff2track(i))) {
            TimeSig *ots = end_of_measure_time_sig_seg ? to_time_sig(end_of_measure_time_sig_seg->element(staff2track(i))) :
                                                         score->staff(i)->time_sig(nm->tick());
            if (ots) {
                TimeSig *nts = Factory::copy_time_sig(*ots);
                nts->set_ownership_parent(time_sig_seg);
                if (section_break) {
                    nts->set_generated(false);
                    nts->set_show_courtesy_sig(false);
                }
                score->undo_add_element(nts);
            }
        }
    }

    return true;
}

//---------------------------------------------------------
//   addTimeSig
//
//    Add or change time signature at measure in response
//    to gui command (drop timesig on measure or timesig)
//---------------------------------------------------------

void EditTimeSig::add_time_sig(Transaction &tx, Score *score, Measure *fm, staff_idx_t staff_idx, TimeSig *ts, bool local) {
    score->deselect_all();

    if (fm->is_mm_rest()) {
        fm = fm->mm_rest_first();
    }

    Fraction ns = ts->sig();
    Fraction tick = fm->tick();
    if (local) {
        Fraction stretch = (ns / fm->timesig()).reduced();
        ts->set_stretch(stretch);
    }

    track_idx_t track = staff_idx * ::mu::engraving::voices;
    Segment *seg = fm->find_segment(SegmentType::TIME_SIG, tick);
    TimeSig *ots = seg ? to_time_sig(seg->element(track)) : nullptr;

    // Check same tick at end of previous measure
    if (!ots) {
        Measure *prev_measure = fm->prev_measure();
        seg = prev_measure ? prev_measure->find_segment_r(SegmentType::TIME_SIG, prev_measure->ticks()) : seg;
        ots = seg ? to_time_sig(seg->element(track)) : nullptr;
    }

    if (ots && (*ots == *ts)) {
        //
        //  ignore if there is already a timesig
        //  with same values
        //
        delete ts;
        return;
    }

    seg = fm->undo_get_segment(SegmentType::TIME_SIG, tick);

    auto get_staff_idx_range = [score, local, staff_idx](const Score *lscore) -> std::pair<staff_idx_t /*start*/, staff_idx_t /*end*/> {
        staff_idx_t start_staff_idx, end_staff_idx;
        if (local) {
            if (lscore == score) {
                start_staff_idx = staff_idx;
                end_staff_idx = start_staff_idx + 1;
            } else {
                const Staff *this_staff = score->staff(staff_idx);
                const Staff *linked_staff = this_staff->find_linked_in_score(lscore);
                if (linked_staff) {
                    start_staff_idx = linked_staff->idx();
                    end_staff_idx = start_staff_idx + 1;
                } else {
                    start_staff_idx = 0;
                    end_staff_idx = 0;
                }
            }
        } else {
            start_staff_idx = 0;
            end_staff_idx = lscore->nstaves();
        }
        return std::make_pair(start_staff_idx, end_staff_idx);
    };

    if (ots && ots->sig() == ns && ots->stretch() == ts->stretch()) {
        //
        // the measure duration does not change,
        // so its ok to just update the time signatures
        //
        TimeSig *nts = score->staff(staff_idx)->next_time_sig(tick + Fraction::eps());
        const Fraction lm_tick = nts ? nts->segment()->tick() : Fraction(-1, 1);
        for (Score *lscore : score->score_list()) {
            Measure *mf = lscore->tick2measure(tick);
            Measure *lm = (lm_tick != Fraction(-1, 1)) ? lscore->tick2measure(lm_tick) : nullptr;
            for (Measure *m = mf; m != lm; m = m->next_measure()) {
                bool change_actual = m->ticks() == m->timesig();
                m->undo_change_property(Pid::TIMESIG_NOMINAL, ns);
                if (change_actual) {
                    m->undo_change_property(Pid::TIMESIG_ACTUAL, ns);
                }
            }
            std::pair<staff_idx_t, staff_idx_t> staff_idx_range = get_staff_idx_range(lscore);
            for (staff_idx_t si = staff_idx_range.first; si < staff_idx_range.second; ++si) {
                TimeSig *nsig = to_time_sig(seg->element(si * ::mu::engraving::voices));
                if (!nsig) {
                    continue;
                }
                nsig->undo_change_property(Pid::SHOW_COURTESY, ts->show_courtesy_sig());
                nsig->undo_change_property(Pid::TIMESIG, ts->sig());
                nsig->undo_change_property(Pid::TIMESIG_TYPE, int(ts->time_sig_type()));
                nsig->undo_change_property(Pid::NUMERATOR_STRING, ts->numerator_string());
                nsig->undo_change_property(Pid::DENOMINATOR_STRING, ts->denominator_string());
                nsig->undo_change_property(Pid::TIMESIG_STRETCH, ts->stretch());
                nsig->undo_change_property(Pid::GROUP_NODES, ts->groups().nodes());
                nsig->set_selected(false);
                nsig->set_drop_target(false);
            }
        }
    } else {
        Score *m_score = score->master_score();
        Measure *mf = m_score->tick2measure(tick);

        //
        // rewrite all measures up to the next time signature
        //
        if (mf == m_score->first_measure() && mf->next_measure() && (mf->ticks() != mf->timesig())) {
            // handle upbeat
            mf->undo_change_property(Pid::TIMESIG_NOMINAL, ns);
            Measure *m = mf->next_measure();
            Segment *s = m->find_segment(SegmentType::TIME_SIG, m->tick());
            mf = s ? 0 : mf->next_measure();
        } else {
            if (score->sigmap()->timesig(seg->tick().ticks()).nominal().identical(ns)) {
                // no change to global time signature,
                // but we need to rewrite any staves with local time signatures
                for (size_t i = 0; i < score->nstaves(); ++i) {
                    if (score->staff(i)->time_sig(tick) && score->staff(i)->time_sig(tick)->is_local()) {
                        if (!rewrite_measures(tx, m_score, mf, ns, i)) {
                            score->undo_stack()->active_transaction()->unwind();
                            return;
                        }
                    }
                }
                mf = 0;
            }
        }

        // try to rewrite the measures first
        // we will only add time signatures if this succeeds
        // this means, however, that the rewrite cannot depend on the time signatures being in place
        if (mf) {
            auto staff_idx_range_on_master = get_staff_idx_range(m_score);
            if (staff_idx_range_on_master.second != staff_idx_range_on_master.first &&
                !rewrite_measures(tx, m_score, mf, ns, local ? staff_idx_range_on_master.first : muse::nidx)) {
                score->undo_stack()->active_transaction()->unwind();
                return;
            }
        }
        // add the time signatures
        std::map<track_idx_t, TimeSig *> master_time_sigs;
        for (Score *lscore : score->score_list()) {
            Measure *nfm = lscore->tick2measure(tick);
            seg = nfm->undo_get_segment(SegmentType::TIME_SIG, nfm->tick());
            std::pair<staff_idx_t, staff_idx_t> staff_idx_range = get_staff_idx_range(lscore);
            for (staff_idx_t si = staff_idx_range.first; si < staff_idx_range.second; ++si) {
                if (fm->is_measure_repeat_group(si)) {
                    score->delete_item(fm->measure_repeat_element(si));
                }
                TimeSig *nsig = to_time_sig(seg->element(si * ::mu::engraving::voices));
                if (nsig == 0) {
                    nsig = Factory::copy_time_sig(*ts);
                    nsig->set_score(lscore);
                    nsig->set_track(si * ::mu::engraving::voices);
                    nsig->set_ownership_parent(seg);
                    nsig->style_changed();
                    score->undo_add_element(nsig);
                    if (lscore->excerpt()) {
                        const track_idx_t master_track = muse::key(lscore->excerpt()->tracks_mapping(), nsig->track());
                        TimeSig *master_time_sig = master_time_sigs[master_track];
                        if (master_time_sig) {
                            tx.push(new Link(master_time_sig, nsig));
                        }
                    }
                } else {
                    nsig->undo_change_property(Pid::SHOW_COURTESY, ts->show_courtesy_sig());
                    nsig->undo_change_property(Pid::TIMESIG_TYPE, int(ts->time_sig_type()));
                    nsig->undo_change_property(Pid::TIMESIG, ts->sig());
                    nsig->undo_change_property(Pid::NUMERATOR_STRING, ts->numerator_string());
                    nsig->undo_change_property(Pid::DENOMINATOR_STRING, ts->denominator_string());

                    // HACK do it twice to accommodate undo
                    nsig->undo_change_property(Pid::TIMESIG_TYPE, int(ts->time_sig_type()));
                    nsig->undo_change_property(Pid::TIMESIG_STRETCH, ts->stretch());
                    nsig->undo_change_property(Pid::GROUP_NODES, ts->groups().nodes());
                    nsig->set_selected(false);
                    nsig->set_drop_target(false); // DEBUG
                }

                if (lscore->is_master()) {
                    master_time_sigs[nsig->track()] = nsig;
                }
            }
        }
    }
    delete ts;
}

//---------------------------------------------------------
//   removeTimeSig
//---------------------------------------------------------

void EditTimeSig::remove_time_sig(Transaction &tx, Score *score, TimeSig *ts) {
    Measure *m = ts->measure();
    Segment *s = ts->segment();

    //
    // we cannot remove a courtesy time signature
    //
    if (s->is_courtesy_segment()) {
        return;
    }
    Fraction tick = m->tick();

    // if we remove all time sigs from segment, segment will be already removed by now
    // but this would leave us no means of detecting that we have measures in a local timesig
    // in cases where we try deleting the local time sig
    // known bug: this means we do not correctly detect non-empty measures when deleting global timesig change after a local one
    // see http://musescore.org/en/node/51596
    // Delete the time sig segment from the root score, we will rewriteMeasures from it
    // since it contains all the music while the part doesn't
    Score *r_score = score->master_score();
    Measure *rm = r_score->tick2measure(m->tick());
    Segment *rs = rm->find_segment(SegmentType::TIME_SIG, s->tick());
    if (rs) {
        r_score->undo_remove_element(rs);
    }
    // Measure can contain mmRest that can have its own timesig. We need to delete it too
    if (rm->mm_rest()) {
        Segment *mm_rest_timesig = rm->mm_rest()->find_segment(SegmentType::TIME_SIG, s->tick());
        if (mm_rest_timesig) {
            r_score->undo_remove_element(mm_rest_timesig);
        }
    }

    Measure *pm = m->prev_measure();
    Fraction ns(pm ? pm->timesig() : Fraction(4, 4));

    if (!rewrite_measures(tx, r_score, rm, ns, muse::nidx)) {
        score->undo_stack()->active_transaction()->unwind();
    } else {
        m = score->tick2measure(tick); // old m may have been replaced
        // hack: fix measure rest durations for staves with local time signatures
        // if a time signature was deleted to reveal a previous local one,
        // then rewriteMeasures() got the measure rest durations wrong
        // (if we fixed it to work for delete, it would fail for add)
        // so we will fix measure rest durations here
        // TODO: fix rewriteMeasures() to get this right
        for (size_t i = 0; i < score->nstaves(); ++i) {
            TimeSig *tsig = score->staff(i)->time_sig(tick);
            if (tsig && tsig->is_local()) {
                for (Measure *nm = m; nm; nm = nm->next_measure()) {
                    // stop when time signature changes
                    if (score->staff(i)->time_sig(nm->tick()) != tsig) {
                        break;
                    }
                    // fix measure rest duration
                    ChordRest *cr = nm->find_chord_rest(nm->tick(), i * ::mu::engraving::voices);
                    if (cr && cr->is_rest() && cr->duration_type() == DurationType::V_MEASURE) {
                        cr->undo_change_property(Pid::DURATION, nm->stretched_len(score->staff(i)));
                    }
                    //cr->setTicks(nm->stretchedLen(staff(i)));
                }
            }
        }
    }
}
