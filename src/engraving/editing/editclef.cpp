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

#include "editclef.h"

#include "../dom/chordrest.h"
#include "../dom/clef.h"
#include "../dom/factory.h"
#include "../dom/instrument.h"
#include "../dom/linkedobjects.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/part.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "log.h"

using namespace mu::engraving;

// compute line position of noteheads after clef change
static void update_note_lines(Segment *segment, track_idx_t track) {
    Staff *staff = segment->score()->staff(track / ::mu::engraving::voices);
    if (staff->is_drum_staff(segment->tick()) || staff->is_tab_staff(segment->tick())) {
        return;
    }
    for (Segment *s = segment->next1(); s; s = s->next1()) {
        if ((s->segment_type() & (SegmentType::CLEF | SegmentType::HEADER_CLEF)) && s->element(track) && !s->element(track)->generated()) {
            break;
        }
        if (!s->is_chord_rest_type()) {
            continue;
        }
        for (track_idx_t t = track; t < track + ::mu::engraving::voices; ++t) {
            EngravingItem *e = s->element(t);
            if (e && e->is_chord()) {
                Chord *chord = to_chord(e);
                for (Note *n : chord->notes()) {
                    n->update_line();
                }
                chord->sort_notes();
                for (Chord *gc : chord->grace_notes()) {
                    for (Note *gn : gc->notes()) {
                        gn->update_line();
                    }
                    gc->sort_notes();
                }
            }
        }
    }
}

//---------------------------------------------------------
//   ChangeClefType
//---------------------------------------------------------

ChangeClefType::ChangeClefType(Clef *c, ClefType cl, ClefType tc) {
    clef = c;
    concert_clef = cl;
    transposing_clef = tc;
}

void ChangeClefType::flip() {
    ClefType ocl = clef->concert_clef();
    ClefType otc = clef->transposing_clef();

    clef->set_concert_clef(concert_clef);
    clef->set_transposing_clef(transposing_clef);

    clef->staff()->set_clef(clef);
    Segment *segment = clef->segment();
    update_note_lines(segment, clef->track());
    clef->trigger_layout_all(); // TODO: reduce layout to clef range

    concert_clef = ocl;
    transposing_clef = otc;
}

//---------------------------------------------------------
//   EditClef
//---------------------------------------------------------

bool EditClef::can_insert_clef(const Score *score, ClefType type) {
    if (type == ClefType::INVALID) {
        return false;
    }

    const Staff *staff = score->staff(score->input_state().track() / ::mu::engraving::voices);
    const ChordRest *cr = score->input_state().cr();

    return staff && cr;
}

void EditClef::insert_clef(Transaction &tx, Score *score, ClefType type) {
    undo_change_clef(tx, score, score->staff(score->input_state().track() / ::mu::engraving::voices), score->input_state().cr(), type);
}

//---------------------------------------------------------
//   insertClef
//    insert clef before cr
//---------------------------------------------------------

void EditClef::insert_clef(Transaction &tx, Score *score, Clef *clef, ChordRest *cr) {
    undo_change_clef(tx, score, cr->staff(), cr, clef->clef_type());
    delete clef;
}

//---------------------------------------------------------
//   undoChangeClef
//    change clef if e is a clef
//    else
//    create a clef before element e
//---------------------------------------------------------

void EditClef::undo_change_clef(Transaction &, Score *score, Staff *ostaff, EngravingItem *e, ClefType ct, bool for_instrument_change,
                                Clef *clef_to_relink) {
    IF_ASSERT_FAILED(ostaff && e) { return; }

    bool move_clef = false;
    SegmentType st = SegmentType::CLEF;
    if (e->is_measure()) {
        if (to_measure(e)->prev_measure()) {
            move_clef = true;
        } else {
            st = SegmentType::HEADER_CLEF;
        }
    } else if (e->is_clef()) {
        Clef *clef = to_clef(e);
        if (clef->segment()->is_header_clef_type()) {
            if (clef->measure()->prev_measure()) {
                move_clef = true;
            } else {
                st = SegmentType::HEADER_CLEF;
            }
        } else if (clef->rtick() == clef->measure()->ticks()) {
            move_clef = true;
        }
    } else if (e->rtick() == Fraction(0, 1)) {
        Measure *cur_measure = e->find_measure();
        Measure *prev_measure = cur_measure ? cur_measure->prev_measure() : nullptr;
        if (prev_measure && !prev_measure->section_break()) {
            move_clef = true;
        }
    }

    bool concert_pitch = score->style().style_b(Sid::CONCERT_PITCH);
    Clef *gclef = 0;
    Fraction tick = e->tick();
    Fraction rtick = e->rtick();
    bool is_small = (st == SegmentType::CLEF);
    for (Staff *staff : ostaff->staff_list()) {
        if (clef_to_relink && ostaff == staff) {
            continue;
        }

        Score *staff_score = staff->score();
        Measure *measure = staff_score->tick2measure(tick);

        if (!measure) {
            LOGW("measure for tick %d not found!", tick.ticks());
            continue;
        }

        Segment *dest_seg;
        Fraction rt;
        if (move_clef) { // if at start of measure and there is a previous measure
            measure = measure->prev_measure();
            rt = measure->ticks();
        } else {
            rt = rtick;
        }
        dest_seg = measure->undo_get_segment_r(st, rt);

        staff_idx_t staff_idx = staff->idx();
        track_idx_t track = staff_idx * ::mu::engraving::voices;
        Clef *clef = to_clef(dest_seg->element(track));

        StaffType *staff_type = staff->staff_type(e->tick());
        StaffGroup staff_group = staff_type->group();
        if (ClefInfo::staff_group(ct) != staff_group && !for_instrument_change) {
            continue;
        }

        if (clef) {
            //
            // for transposing instruments, differentiate
            // clef type for concertPitch
            //
            Instrument *i = staff->part()->instrument(tick);
            ClefType cp, tp;
            if (i->transpose().is_zero()) {
                cp = ct;
                tp = ct;
            } else {
                if (concert_pitch) {
                    cp = ct;
                    tp = clef->transposing_clef();
                } else {
                    cp = clef->concert_clef();
                    tp = ct;
                }
            }
            clef->set_generated(false);
            staff_score->undo(new ChangeClefType(clef, cp, tp));
            Clef *o_clef = clef->other_clef();
            if (o_clef && !(o_clef->generated())) {
                staff_score->undo(new ChangeClefType(o_clef, cp, tp));
            }
            // change the clef in the mmRest if any
            if (measure->has_mm_rest()) {
                Measure *mm_measure = measure->mm_rest();
                Segment *mm_dest_seg = mm_measure->find_segment(SegmentType::CLEF, tick);
                if (mm_dest_seg) {
                    Clef *mm_clef = to_clef(mm_dest_seg->element(clef->track()));
                    if (mm_clef) {
                        staff_score->undo(new ChangeClefType(mm_clef, cp, tp));
                    }
                }
            }
        } else {
            if (gclef) {
                clef = to_clef(gclef->linked_clone());
                clef->set_score(staff_score);
                clef->set_ownership_parent(dest_seg);
            } else {
                clef = Factory::create_clef(dest_seg);
                clef->set_clef_type(ct);
                gclef = clef;
            }
            clef->set_track(track);
            staff_score->do_undo_add_element(clef);
        }
        if (for_instrument_change) {
            clef->set_for_instrument_change(true);
        }
        clef->set_small(is_small);

        if (clef_to_relink) {
            LinkedObjects *links = clef->links();
            if (!links) {
                clef->link_to(clef_to_relink);
            } else if (!clef->is_linked(clef_to_relink)) {
                clef_to_relink->set_links(links);
                links->push_back(clef_to_relink);
            }
        }
    }
}
