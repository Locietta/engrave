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

#include "splitjoinmeasure.h"

#include "addremoveelement.h"
#include "dom/chordrest.h"
#include "dom/factory.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/range.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/spanner.h"
#include "dom/staff.h"
#include "dom/stafftypechange.h"
#include "dom/timesig.h"
#include "editspanner.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   splitMeasure
//---------------------------------------------------------

void SplitJoinMeasure::split_measure(Transaction &tx, MasterScore *master_score, const Fraction &tick) {
    Segment *segment = master_score->tick2segment(tick, false, SegmentType::CHORD_REST);

    if (segment->rtick().is_zero()) {
        MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_FIRST_BEAT);
        return;
    }
    if (segment->splits_tuplet()) {
        MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_TUPLET);
        return;
    }

    Measure *measure = master_score->tick2measure(tick);

    for (size_t staff_idx = 0; staff_idx < master_score->nstaves(); ++staff_idx) {
        if (measure->is_measure_repeat_group(static_cast<int>(staff_idx))) {
            MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
            return;
        }
    }

    ScoreRange range;
    range.read(measure->first(), measure->last());

    Fraction stick = measure->tick();
    Fraction etick = measure->end_tick();

    std::vector<std::tuple<Spanner *, Fraction, Fraction>> spanners;
    for (auto i : master_score->spanner()) {
        Spanner *s = i.second;
        EngravingItem *start = s->start_element();
        EngravingItem *end = s->end_element();
        if (s->tick() >= stick && s->tick() < etick) {
            start = nullptr;
        }
        if (s->tick2() >= stick && s->tick2() < etick) {
            end = nullptr;
        }
        if (start != s->start_element() || end != s->end_element()) {
            spanners.push_back(std::make_tuple(s, s->tick(), s->ticks()));
            tx.push(new ChangeStartEndSpanner(s, start, end));
        }
        if (s->tick() < stick && s->tick2() > stick) {
            spanners.push_back(std::make_tuple(s, s->tick(), s->ticks()));
        }
    }

    MeasureBase *nm = measure->next();

    // create empty measures:
    Score::InsertMeasureOptions options;
    options.create_measure_rests = true;
    options.move_signatures_clef = false;
    options.move_staff_type_changes = false;
    options.ignore_bar_lines = true;

    master_score->insert_measure(nm, options);
    Measure *m2 = to_measure(nm ? nm->prev() : master_score->last_measure());
    master_score->insert_measure(m2, options);
    Measure *m1 = to_measure(m2->prev());

    for (Score *s : master_score->score_list()) {
        Measure *m = s->tick2measure(tick);
        s->undo_remove_measures(m, m, true, false);
    }
    master_score->undo_insert_time(measure->tick(), -measure->ticks());

    m1->set_tick(measure->tick());
    m2->set_tick(tick);

    Fraction ticks1 = tick - measure->tick();
    Fraction ticks2 = measure->ticks() - ticks1;

    m1->set_timesig(measure->timesig());
    m2->set_timesig(measure->timesig());

    ticks1.reduce();
    ticks2.reduce();
    // Now make sure this reduction doesn't go 'beyond' the original measure's
    // actual denominator for both resultant measures.
    if (ticks1.denominator() < measure->ticks().denominator()) {
        if (measure->ticks().denominator() % m1->timesig().denominator() == 0) {
            int mult = measure->ticks().denominator() / ticks1.denominator();
            // *= operator automatically reduces via GCD, so rather do literal multiplication:
            ticks1.set_denominator(ticks1.denominator() * mult);
            ticks1.set_numerator(ticks1.numerator() * mult);
        }
    }
    if (ticks2.denominator() < measure->ticks().denominator()) {
        if (measure->ticks().denominator() % m2->timesig().denominator() == 0) {
            int mult = measure->ticks().denominator() / ticks2.denominator();
            ticks2.set_denominator(ticks2.denominator() * mult);
            ticks2.set_numerator(ticks2.numerator() * mult);
        }
    }
    if (ticks1.denominator() > 128 || ticks2.denominator() > 128) {
        MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_TOO_SHORT);
        return;
    }

    m1->adjust_to_len(ticks1, false);
    m2->adjust_to_len(ticks2, false);

    // set correct barline types if needed
    for (Score *linked_score : master_score->score_list()) {
        Measure *s_m1 = linked_score->tick2measure(m1->tick());
        Measure *s_m2 = linked_score->tick2measure(m2->tick());
        // TODO: handle other end barline types; they may differ per staff
        if (measure->end_bar_line_type() == BarLineType::END_REPEAT) {
            s_m2->undo_change_property(Pid::REPEAT_END, true);
        }
        if (measure->get_property(Pid::REPEAT_START).to_bool()) {
            s_m1->undo_change_property(Pid::REPEAT_START, true);
        }
    }

    range.write(master_score, m1->tick());

    for (const auto &[s, t, ticks] : spanners) {
        if (s->tick() != t) {
            s->undo_change_property(Pid::SPANNER_TICK, t);
        }
        if (s->ticks() != ticks) {
            s->undo_change_property(Pid::SPANNER_TICKS, ticks);
        }
    }

    for (size_t staff_idx = 0; staff_idx < master_score->nstaves(); ++staff_idx) {
        Staff *staff = master_score->staves().at(staff_idx);
        if (staff->is_staff_type_start_from(stick)) {
            StaffTypeChange *stc = Factory::create_staff_type_change(m1);
            stc->set_track(staff_idx * ::mu::engraving::voices);
            master_score->add_element(stc);
        }
    }
}

//---------------------------------------------------------
//   joinMeasure
//---------------------------------------------------------

void SplitJoinMeasure::join_measures(Transaction &tx, MasterScore *master_score, const Fraction &tick1, const Fraction &tick2) {
    Measure *m1 = master_score->tick2measure(tick1);
    Measure *m2 = master_score->tick2measure(tick2);

    if (!m1 || !m2) {
        return;
    }

    for (size_t staff_idx = 0; staff_idx < master_score->nstaves(); ++staff_idx) {
        if (m1->is_measure_repeat_group_with_prev_m(static_cast<int>(staff_idx)) ||
            m2->is_measure_repeat_group_with_next_m(static_cast<int>(staff_idx))) {
            MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
            return;
        }
        if (master_score->staves().at(staff_idx)->staff_type_range(tick2).first > tick1.ticks()) {
            MScore::set_error(MsError::CANNOT_JOIN_MEASURE_STAFFTYPE_CHANGE);
            return;
        }
    }

    if (m1->is_mm_rest()) {
        m1 = m1->mm_rest_first();
    }
    if (m2->is_mm_rest()) {
        m2 = m2->mm_rest_last();
    }

    master_score->deselect_all();

    ScoreRange range;
    range.read(m1->first(), m2->last());

    Fraction start_tick = m1->tick();
    Fraction end_tick = m2->end_tick();

    auto spanners = master_score->spanner_map().find_contained(start_tick.ticks(), end_tick.ticks());
    for (auto i : spanners) {
        master_score->do_undo_remove_element(i.value);
    }

    std::map<Spanner *, Fraction> spanners_ending_in_range;
    std::multimap<int, Spanner *> spanner_map = master_score->spanner();
    for (std::pair<int, Spanner *> i : spanner_map) {
        Spanner *s = i.second;
        if (s->tick() >= start_tick && s->tick() < end_tick) {
            s->set_start_element(0);
        }
        if (s->tick2() >= start_tick && s->tick2() < end_tick) {
            s->set_end_element(0);
        }
        // Maintain length of spanners starting outside and ending within join range
        if (s->tick() < start_tick && s->tick2() < end_tick) {
            spanners_ending_in_range.emplace(s, s->tick2());
        }
        // Remove spanners starting within join range and ending outside
        // These will be replaced on range.write
        if (s->tick() > start_tick && s->tick() < end_tick && s->tick2() > end_tick) {
            master_score->do_undo_remove_element(s);
        }
    }

    MeasureBase *next = m2->next();
    master_score->delete_measures(m1, m2, true);

    Score::InsertMeasureOptions options;
    options.create_measure_rests = true;
    options.move_signatures_clef = false;
    options.move_staff_type_changes = false;
    options.ignore_bar_lines = true;
    Measure *joined_measure = to_measure(master_score->insert_measure(next, options));

    for (auto spanner_to_fixup : spanners_ending_in_range) {
        Spanner *sp = spanner_to_fixup.first;
        Fraction sp_end_tick = spanner_to_fixup.second;

        sp->set_tick2(sp_end_tick);
    }

    for (size_t staff_idx = 0; staff_idx < master_score->nstaves(); ++staff_idx) {
        Staff *staff = master_score->staves().at(staff_idx);
        if (staff->is_staff_type_start_from(tick1)) {
            StaffTypeChange *stc = engraving::Factory::create_staff_type_change(joined_measure);
            stc->set_track(staff_idx * ::mu::engraving::voices);
            master_score->add_element(stc);
        }
    }

    const Fraction new_timesig = m1->timesig();
    Fraction new_len;
    for (Measure *mm = m1; mm; mm = mm->next_measure()) {
        new_len += mm->ticks();
        if (mm == m2) {
            break;
        }
    }

    // The loop since measures are not currently linked in MuseScore
    // change nominal time sig, as inserted measure took it from next measure
    for (Score *linked_score : master_score->score_list()) {
        Measure *ins = linked_score->tick2measure(start_tick);
        ins->undo_change_property(Pid::TIMESIG_NOMINAL, new_timesig);
        // set correct barline types if needed
        // TODO: handle other end barline types; they may differ per staff
        if (m2->end_bar_line_type() == BarLineType::END_REPEAT) {
            ins->undo_change_property(Pid::REPEAT_END, true);
        }
        if (m1->get_property(Pid::REPEAT_START).to_bool()) {
            ins->undo_change_property(Pid::REPEAT_START, true);
        }
    }
    Measure *inserted = (next ? next->prev_measure() : master_score->last_measure());
    inserted->adjust_to_len(new_len, /* appendRests... */ false);

    range.write(master_score, m1->tick());

    // if there are some Time Signatures in joined measures, move last one to the next measure (if there is not one already)
    Segment *ts = inserted->last()->prev(SegmentType::TIME_SIG);
    if (ts && ts->rtick().is_not_zero()) {
        for (Segment *ins_m_seg = inserted->last()->prev(SegmentType::TIME_SIG); ins_m_seg && ins_m_seg->rtick().is_not_zero();
             ins_m_seg = ins_m_seg->prev(SegmentType::TIME_SIG)) {
            for (staff_idx_t staff_idx = 0; staff_idx < master_score->nstaves(); ++staff_idx) {
                if (TimeSig *ins_time_sig = to_time_sig(ins_m_seg->element(staff_idx * ::mu::engraving::voices))) {
                    TimeSig *lts = nullptr;
                    for (EngravingObject *l : ins_time_sig->link_list()) {
                        Score *linked_score = l->score();
                        TimeSig *time_sig = to_time_sig(l);
                        Segment *t_seg = time_sig->segment();
                        track_idx_t track = time_sig->track();
                        Measure *s_next = next ? linked_score->tick2measure(next->tick()) : nullptr;
                        Segment *next_t_seg = s_next ? s_next->undo_get_segment_r(SegmentType::TIME_SIG, Fraction(0, 1)) : nullptr;
                        if (s_next && !next_t_seg->element(track)) {
                            TimeSig *nsig = Factory::copy_time_sig(*time_sig);
                            nsig->set_score(linked_score);
                            nsig->set_track(track);
                            nsig->set_ownership_parent(next_t_seg);

                            linked_score->do_undo_add_element(nsig);

                            if (!lts) {
                                lts = nsig;
                            } else {
                                tx.push(new Link(lts, nsig));
                            }
                        }
                        linked_score->do_undo_remove_element(time_sig);
                        if (t_seg->empty()) {
                            linked_score->do_undo_remove_element(t_seg);
                        }
                    }
                }
            }
        }
    }
}
