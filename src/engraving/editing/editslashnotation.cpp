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

#include "editslashnotation.h"

#include "noteinput.h"

#include "containers.h"

#include "../dom/chord.h"
#include "../dom/durationtype.h"
#include "../dom/input.h"
#include "../dom/linkedobjects.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/staff.h"
#include "../dom/stafftype.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   slashFill
///   fills selected region with slashes
//---------------------------------------------------------

void EditSlashNotation::slash_fill(Transaction &, Score *score) {
    staff_idx_t start_staff = score->selection().staff_start();
    staff_idx_t end_staff = score->selection().staff_end();
    Segment *start_segment = score->selection().start_segment();
    if (!start_segment) { // empty score?
        return;
    }

    Segment *end_segment = score->selection().end_segment();

    // operate on measures underlying mmrests
    if (start_segment && start_segment->measure() && start_segment->measure()->is_mm_rest()) {
        start_segment = start_segment->measure()->mm_rest_first()->first();
    }
    if (end_segment && end_segment->measure() && end_segment->measure()->is_mm_rest()) {
        end_segment = end_segment->measure()->mm_rest_last()->last();
    }

    Fraction end_tick = end_segment ? end_segment->tick() : score->last_segment()->tick() + Fraction::eps();
    Chord *first_slash = 0;
    Chord *last_slash = 0;

    // loop through staves in selection
    for (staff_idx_t staff_idx = start_staff; staff_idx < end_staff; ++staff_idx) {
        track_idx_t track = staff_idx * ::mu::engraving::voices;
        voice_idx_t voice = muse::nidx;
        // loop through segments adding slashes on each beat
        for (Segment *s = start_segment; s && s->tick() < end_tick; s = s->next1()) {
            if (s->segment_type() != SegmentType::CHORD_REST) {
                continue;
            }
            // determine beat type based on time signature
            int d = s->measure()->timesig().denominator();
            int n = (d > 4 && s->measure()->timesig().numerator() % 3 == 0) ? 3 : 1;
            Fraction f(n, d);
            // skip over any leading segments before next (first) beat
            if (s->rtick().ticks() % f.ticks()) {
                continue;
            }
            // determine voice to use - first available voice for this measure / staff
            if (voice == muse::nidx || s->rtick().is_zero()) {
                bool need_gap[::mu::engraving::voices];
                for (voice = 0; voice < ::mu::engraving::voices; ++voice) {
                    need_gap[voice] = false;
                    ChordRest *cr = to_chord_rest(s->element(track + voice));
                    // no chordrest == treat as ordinary rest for purpose of determining availability of voice
                    // but also, we will need to make a gap for this voice if we do end up choosing it
                    if (!cr) {
                        need_gap[voice] = true;
                    }
                    // chord == keep looking for an available voice
                    else if (cr->is_chord()) {
                        continue;
                    }
                    // full measure rest == OK to use voice
                    else if (cr->duration_type() == DurationType::V_MEASURE) {
                        break;
                    }
                    // no chordrest or ordinary rest == OK to use voice
                    // if there are nothing but rests for duration of measure / selection
                    bool ok = true;
                    for (Segment *ns = s->next(SegmentType::CHORD_REST); ns && ns != end_segment; ns = ns->next(SegmentType::CHORD_REST)) {
                        ChordRest *ncr = to_chord_rest(ns->element(track + voice));
                        if (ncr && ncr->is_chord()) {
                            ok = false;
                            break;
                        }
                    }
                    if (ok) {
                        break;
                    }
                }
                // no available voices, just use voice 0
                if (voice == ::mu::engraving::voices) {
                    voice = 0;
                }
                // no cr was found in segment for this voice, so make gap
                if (need_gap[voice]) {
                    score->make_gap_voice(s, track + voice, f, s->tick());
                }
            }
            // construct note
            int line = 0;
            bool error = false;
            NoteVal nv;
            if (score->staff(staff_idx)->staff_type(s->tick())->group() == StaffGroup::TAB) {
                line = score->staff(staff_idx)->lines(s->tick()) / 2;
            } else {
                line = score->staff(staff_idx)->middle_line(s->tick()); // staff(staffIdx)->lines() - 1;
            }
            if (score->staff(staff_idx)->staff_type(s->tick())->group() == StaffGroup::PERCUSSION) {
                nv.pitch = 0;
                nv.head_group = NoteHeadGroup::HEAD_SLASH;
            } else {
                Position p;
                p.segment = s;
                p.staff_idx = staff_idx;
                p.line = line;
                p.fret = ::mu::engraving::invalid_fret_index;
                score->input_state().set_rest(false); // needed for tab
                nv = NoteInput::note_val_for_position(score, p, AccidentalType::NONE, error);
            }
            if (error) {
                continue;
            }
            // insert & turn into slash
            s = score->set_note_rest(s, track + voice, nv, f);
            Chord *c = to_chord(s->element(track + voice));
            if (c) {
                if (c->links()) {
                    for (EngravingObject *e : *c->links()) {
                        Chord *lc = to_chord(e);
                        lc->set_slash(true, true);
                    }
                } else {
                    c->set_slash(true, true);
                }
            }
            last_slash = c;
            if (!first_slash) {
                first_slash = c;
            }
        }
    }

    // re-select the slashes
    score->deselect_all();
    if (first_slash && last_slash) {
        score->select(first_slash, SelectType::RANGE);
        score->select(last_slash, SelectType::RANGE);
    }
}

//---------------------------------------------------------
//   slashRhythm
///   converts rhythms in selected region to slashes
//---------------------------------------------------------

void EditSlashNotation::slash_rhythm(Transaction &, Score *score) {
    std::set<Chord *> chords;
    // loop through all notes in selection
    for (EngravingItem *e : score->selection().elements()) {
        if (e->voice() >= 2 && e->is_rest()) {
            Rest *r = to_rest(e);
            if (r->links()) {
                for (EngravingObject *se : *r->links()) {
                    Rest *lr = to_rest(se);
                    lr->set_accent(!lr->accent());
                }
            } else {
                r->set_accent(!r->accent());
            }
            continue;
        } else if (e->is_note()) {
            Note *n = to_note(e);
            if (n->note_type() != NoteType::NORMAL) {
                continue;
            }
            Chord *c = n->chord();
            // check for duplicates (chords with multiple notes)
            if (muse::contains(chords, c)) {
                continue;
            }
            chords.insert(c);
            // toggle slash setting
            c->set_slash(!c->slash(), false);
        }
    }
}
