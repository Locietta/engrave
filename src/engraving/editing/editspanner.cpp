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

#include "log.h"
#include "editspanner.h"

#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/spanner.h"
#include "../dom/textlinebase.h"
#include "../dom/tie.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   EditSpanner
//---------------------------------------------------------

void EditSpanner::add_spanner(Transaction &, Score *score, Spanner *spanner, const PointF &pos, bool system_staves_only) {
    staff_idx_t staff_idx = spanner->staff_idx();
    Segment *segment;
    Measure *measure = score->pos2measure(pos, &staff_idx, /*pitch*/ 0, &segment, /*offset*/ 0);
    if (system_staves_only) {
        staff_idx = 0;
    }
    // ignore if we do not have a measure
    if (!measure) {
        LOGD("addSpanner: cannot put object here");
        delete spanner;
        return;
    }

    // all spanners live in voice 0 (except slurs/ties)
    track_idx_t track = staff_idx == muse::nidx ? muse::nidx : staff_idx * ::mu::engraving::voices;

    spanner->set_track(track);
    spanner->set_track2(track);

    if (spanner->anchor() == Spanner::Anchor::SEGMENT) {
        spanner->set_tick(segment->tick());
        Fraction tick2 = std::min(segment->measure()->end_tick(), score->last_measure()->end_tick());
        spanner->set_tick2(tick2);
    } else { // Anchor::MEASURE, Anchor::CHORD, Anchor::NOTE
        spanner->set_tick(measure->tick());
        spanner->set_tick2(measure->end_tick());
    }
    spanner->erase_spanner_segments();

    bool ctrl_modifier = is_system_text_line(spanner) && !system_staves_only;
    score->undo_add_element(spanner, true /*addToLinkedStaves*/, ctrl_modifier);
}

void EditSpanner::add_spanner(Transaction &, Score *score, Spanner *spanner, staff_idx_t staff_idx, Segment *start_segment,
                              Segment *end_segment, bool ctrl_modifier) {
    track_idx_t track = staff_idx * ::mu::engraving::voices;
    spanner->set_track(track);
    spanner->set_track2(track);
    for (auto ss : spanner->spanner_segments()) {
        ss->set_track(track);
    }

    bool is_measure_anchor = spanner->anchor() == Spanner::Anchor::MEASURE;
    Fraction tick1 = is_measure_anchor ? start_segment->measure()->tick() : start_segment->tick();
    spanner->set_tick(tick1);

    Fraction tick2;
    if (!end_segment) {
        tick2 = score->last_segment()->tick();
    } else if (end_segment == start_segment) {
        tick2 = start_segment->measure()->last()->tick();
    } else {
        tick2 = end_segment->tick();
    }
    if (is_measure_anchor) {
        Measure *end_measure = score->tick2measure_mm(tick2);
        if (end_measure->tick() != tick2) {
            tick2 = end_measure->end_tick();
        }
    }

    spanner->set_tick2(tick2);
    score->undo_add_element(spanner, true, ctrl_modifier);
}

//---------------------------------------------------------
//   ChangeSpannerElements
//---------------------------------------------------------

void ChangeSpannerElements::flip() {
    EngravingItem *old_start_element = spanner->start_element();
    EngravingItem *old_end_element = spanner->end_element();
    bool is_partial_spanner = spanner->is_partial_tie() || spanner->is_laissez_vib();
    if (spanner->anchor() == Spanner::Anchor::NOTE) {
        // be sure new spanner elements are of the right type
        if (!is_partial_spanner && (!start_element || !start_element->is_note() || !end_element || !end_element->is_note())) {
            return;
        }
        Note *old_start_note = to_note(old_start_element);
        Note *old_end_note = to_note(old_end_element);
        Note *new_start_note = to_note(start_element);
        Note *new_end_note = to_note(end_element);
        // update spanner's start and end notes
        if ((new_start_note && new_end_note) || (is_partial_spanner && (new_start_note || new_end_note))) {
            spanner->set_note_span(new_start_note, new_end_note);
            if (spanner->is_tie()) {
                Tie *tie = to_tie(spanner);
                if (old_start_note && new_start_note) {
                    old_start_note->set_tie_for(nullptr);
                    new_start_note->set_tie_for(tie);
                }
                if (old_end_note && new_end_note) {
                    old_end_note->set_tie_back(nullptr);
                    new_end_note->set_tie_back(tie);
                }
            } else {
                old_start_note->remove_spanner_for(spanner);
                old_end_note->remove_spanner_back(spanner);
                new_start_note->add_spanner_for(spanner);
                new_end_note->add_spanner_back(spanner);
                if (spanner->is_glissando()) {
                    old_end_note->chord()->update_ends_note_anchored_line();
                }
            }
        }
    } else {
        spanner->set_start_element(start_element);
        spanner->set_end_element(end_element);
    }
    start_element = old_start_element;
    end_element = old_end_element;
    spanner->trigger_layout();
}

//---------------------------------------------------------
//   ChangeStartEndSpanner
//---------------------------------------------------------

void ChangeStartEndSpanner::flip() {
    EngravingItem *s = spanner->start_element();
    EngravingItem *e = spanner->end_element();
    spanner->set_start_element(start);
    spanner->set_end_element(end);
    start = s;
    end = e;
}
