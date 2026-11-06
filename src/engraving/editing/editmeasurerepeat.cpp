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

#include "editmeasurerepeat.h"

#include <vector>

#include "../dom/barline.h"
#include "../dom/factory.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/staff.h"
#include "../dom/utils.h"
#include "../infrastructure/messagebox.h"
#include "translation.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   addMeasureRepeat
//---------------------------------------------------------

void EditMeasureRepeat::add_measure_repeat(Transaction &tx, Score *score, Measure *first_measure, int num_measures, staff_idx_t staff_idx) {
    //
    // make measures into group
    //
    if (!make_measure_repeat_group(tx, score, first_measure, num_measures, staff_idx)) {
        return;
    }

    //
    // add MeasureRepeat element
    //
    int measure_with_element_no;
    if (num_measures % 2) {
        // odd number, element anchored to center measure of group
        measure_with_element_no = num_measures / 2 + 1;
    } else {
        // even number, element anchored to last measure in first half of group
        measure_with_element_no = num_measures / 2;
    }
    Measure *measure_with_element = first_measure;
    for (int i = 1; i < measure_with_element_no; ++i) {
        measure_with_element = measure_with_element->next_measure();
    }
    // MeasureRepeat element will be positioned appropriately (in center of measure / on following barline)
    // when stretchMeasure() is called on measureWithElement
    MeasureRepeat *mr = add_measure_repeat(tx, score, measure_with_element->tick(), staff2track(staff_idx), num_measures);
    score->select(mr, SelectType::SINGLE, 0);
}

//---------------------------------------------------------
//   makeMeasureRepeatGroup
///   clear measures, apply noBreak, set measureRepeatCount
///   returns false if these measures won't work or user aborted
//---------------------------------------------------------

bool EditMeasureRepeat::make_measure_repeat_group(Transaction &, Score *score, Measure *first_measure, int num_measures,
                                                  staff_idx_t staff_idx) {
    //
    // check that sufficient measures exist, with equal durations
    //
    std::vector<Measure *> measures;
    Measure *measure = first_measure;
    for (int i = 1; i <= num_measures; ++i) {
        if (!measure || measure->ticks() != first_measure->ticks()) {
            MScore::set_error(MsError::INSUFFICIENT_MEASURES);
            return false;
        }
        measures.push_back(measure);
        measure = measure->next_measure();
    }

    //
    // warn user if things will have to be deleted to make room for measure repeat
    //
    bool empty = true;
    for (auto m : measures) {
        if (m != measures.back()) {
            if (m->end_bar_line_type() != BarLineType::NORMAL) {
                empty = false;
                break;
            }
        }
        for (auto seg = m->first(); seg && empty; seg = seg->next()) {
            if (seg->segment_type() & SegmentType::CHORD_REST) {
                track_idx_t strack = staff_idx * ::mu::engraving::voices;
                track_idx_t etrack = strack + ::mu::engraving::voices;
                for (track_idx_t track = strack; track < etrack; ++track) {
                    EngravingItem *e = seg->element(track);
                    if (e && !e->generated() && !e->is_rest()) {
                        empty = false;
                        break;
                    }
                }
            }
        }
    }

    if (!empty) {
        auto b = MessageBox().warning(muse::trc("engraving", "Current contents of measures will be replaced"),
                                      muse::trc("engraving", "Continue with inserting measure repeat?"));
        if (b == MessageBox::Button::CANCEL) {
            return false;
        }
    }

    //
    // group measures and clear current contents
    //

    score->deselect_all();
    int i = 1;
    for (auto m : measures) {
        score->select(m, SelectType::RANGE, staff_idx);
        if (m->is_measure_repeat_group(staff_idx)) {
            score->delete_item(m->measure_repeat_element(staff_idx)); // reset measures related to an earlier MeasureRepeat
        }
        score->undo_change_measure_repeat_count(m, i++, staff_idx);
        if (m != measures.front()) {
            m->undo_change_property(Pid::REPEAT_START, false);
        }
        if (m != measures.back()) {
            m->undo_set_no_break(true);
            Segment *seg = m->find_segment_r(SegmentType::END_BAR_LINE, m->ticks());
            BarLine *end_bar_line = to_bar_line(seg->element(staff2track(staff_idx)));
            score->delete_item(end_bar_line); // also takes care of Pid::REPEAT_END
        }
    }
    score->cmd_delete_selection();
    return true;
}

//---------------------------------------------------------
//   addMeasureRepeat
//    create one MeasureRepeat at tick of subtype numMeasures
//    create segment if necessary
//    does NOT set measureRepeatCount or do anything else with measure(s)!
//---------------------------------------------------------

MeasureRepeat *EditMeasureRepeat::add_measure_repeat(Transaction &, Score *score, const Fraction &tick, track_idx_t track,
                                                     int num_measures) {
    Measure *measure = score->tick2measure(tick);
    MeasureRepeat *mr = Factory::create_measure_repeat(score->dummy());
    mr->set_num_measures(num_measures);
    mr->set_ticks(measure->stretched_len(score->staff(track2staff(track))));
    mr->set_track(track);
    score->undo_add_cr(mr, measure, tick);
    return mr;
}
