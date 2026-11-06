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

#include "editbeam.h"

#include "../dom/chordrest.h"
#include "../dom/groups.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/utils.h"
#include "log.h"
#include "navigation.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   setBeamMode
//---------------------------------------------------------

void EditBeam::set_beam_mode(Transaction &, Score *score, BeamMode mode) {
    for (ChordRest *cr : score->get_selected_chord_rests()) {
        if (cr) {
            cr->undo_change_property(Pid::BEAM_MODE, mode);
        }
    }
}

//---------------------------------------------------------
//   beamSelectedRange
//---------------------------------------------------------

void EditBeam::beam_selected_range(Transaction &tx, Score *score) {
    if (!score->selection().is_range()) {
        return;
    }

    reset_beam_mode(tx, score);

    const track_idx_t start_track = staff2track(score->selection().staff_start());
    const track_idx_t end_track = staff2track(score->selection().staff_end());
    const SelectionFilter filter = score->selection_filter();

    for (staff_idx_t track_idx = start_track; track_idx < end_track; ++track_idx) {
        if (!filter.can_select_voice(track_idx)) {
            continue;
        }

        ChordRest *first_chord_rest = score->selection().first_chord_rest(track_idx);
        ChordRest *last_chord_rest = score->selection().last_chord_rest(track_idx);

        if (!first_chord_rest || !last_chord_rest) {
            continue;
        }

        ChordRest *prev = Navigation::prev_chord_rest(first_chord_rest);
        ChordRest *cr = first_chord_rest;
        BeamMode actual_beam_mode = Groups::actual_beam_mode(cr, prev);
        if (actual_beam_mode != BeamMode::BEGIN) {
            cr->undo_change_property(Pid::BEAM_MODE, BeamMode::BEGIN);
        }

        while (cr != last_chord_rest) {
            prev = cr;
            cr = Navigation::next_chord_rest(cr);
            actual_beam_mode = Groups::actual_beam_mode(cr, prev);
            if (actual_beam_mode == BeamMode::BEGIN || actual_beam_mode == BeamMode::NONE) {
                cr->undo_change_property(Pid::BEAM_MODE, BeamMode::MID);
            }
        }

        prev = cr;
        cr = Navigation::next_chord_rest(cr);
        if (cr) {
            actual_beam_mode = Groups::actual_beam_mode(cr, prev);
            if (actual_beam_mode != BeamMode::BEGIN && actual_beam_mode != BeamMode::NONE) {
                cr->undo_change_property(Pid::BEAM_MODE, BeamMode::BEGIN);
            }
        }
    }
}

//---------------------------------------------------------
//   resetBeamMode
//---------------------------------------------------------

void EditBeam::reset_beam_mode(Transaction &, Score *score) {
    bool no_selection = score->selection().is_none();
    if (no_selection) {
        score->cmd_select_all();
    } else if (!score->selection().is_range()) {
        LOGD("no system or staff selected");
        return;
    }

    ChordRest *first_cr = score->selection().first_chord_rest();
    if (!first_cr) {
        LOGD("no chord/rest in selection");
        return;
    }

    const track_idx_t track_start = staff2track(score->selection().staff_start());
    const track_idx_t track_end = staff2track(score->selection().staff_end());
    const Fraction end_tick = score->selection().tick_end();
    const SelectionFilter filter = score->selection_filter();

    for (Segment *seg = first_cr->segment(); seg && seg->tick() < end_tick; seg = seg->next1(SegmentType::CHORD_REST)) {
        for (track_idx_t track = track_start; track < track_end; ++track) {
            if (!filter.can_select_voice(track)) {
                continue;
            }

            ChordRest *cr = to_chord_rest(seg->element(track));
            if (!cr) {
                continue;
            }
            if (cr->is_chord()) {
                if (cr->beam_mode() != BeamMode::AUTO) {
                    cr->undo_change_property(Pid::BEAM_MODE, BeamMode::AUTO);
                }
            } else if (cr->is_rest()) {
                if (cr->beam_mode() != BeamMode::NONE) {
                    cr->undo_change_property(Pid::BEAM_MODE, BeamMode::NONE);
                }
            }
        }
    }

    if (no_selection) {
        score->deselect_all();
    }
}
