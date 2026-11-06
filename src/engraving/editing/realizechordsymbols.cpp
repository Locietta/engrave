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

#include "realizechordsymbols.h"

#include <vector>

#include "style/style.h"

#include "../dom/chord.h"
#include "../dom/factory.h"
#include "../dom/harmony.h"
#include "../dom/instrument.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/part.h"
#include "../dom/realizedharmony.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/staff.h"
#include "../dom/tie.h"
#include "../dom/tuplet.h"
#include "../dom/utils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   setChord
//    'dur' is in local (stretched) time
//    return segment of last created chord
//---------------------------------------------------------
static Segment *set_chord(Score *score, Segment *segment, track_idx_t track, const Chord *chord_template, Fraction dur) {
    assert(segment->segment_type() == SegmentType::CHORD_REST);

    Fraction tick = segment->tick();
    Chord *nr = nullptr; //current added chord used so we can select the last added chord and so we can apply ties
    std::vector<Tie *> tie(chord_template->notes().size()); //keep pointer to a tie for each note in the chord in case we need to tie notes
    ChordRest *cr = to_chord_rest(segment->element(track)); //chord rest under the segment for the specified track

    bool add_tie = false;

    Measure *measure = nullptr;
    //keep creating chords and tieing them until we created the full duration asked for (dur)
    for (;;) {
        if (track % ::mu::engraving::voices) {
            score->expand_voice(segment, track);
        }

        Tuplet *t = cr ? cr->tuplet() : 0;
        Fraction t_dur = segment->ticks();
        Segment *seg = segment->next();

        //we need to get a correct subduration so that makeGap can function properly
        //since makeGap() takes "normal" duration rather than actual length
        while (seg) {
            if (seg->segment_type() == SegmentType::CHORD_REST) {
                //design choice made to keep multiple notes across a tuplet as tied single notes rather than combining them
                //since it's arguably more readable, but the other code is still here (commented)
                ChordRest *test_cr = to_chord_rest(seg->element(track));

                //code here allows us to combine tuplet realization together which I have opted not to do for readability (of the music)
                //if (!!t ^ (testCr && testCr->tuplet())) //stop if we started with a tuplet and reach something that's not a tuplet,
                //      break;                          //or start with not a tuplet and reach a tuplet

                if (test_cr && test_cr->tuplet()) { //stop on tuplet
                    break;
                }
                t_dur += seg->ticks();
            }
            if (t_dur >= dur) { //do not go further than the duration asked for
                t_dur = dur;
                break;
            }
            seg = seg->next(); //iterate only across measure (hence usage of next() rather than next1())
        }
        if (t) {
            t_dur *= t->ratio(); //scale by tuplet ratio to get "normal" length rather than actual length when dealing with tuplets
        }
        // the returned gap ends at the measure boundary or at tuplet end
        Fraction dd = score->make_gap(segment, track, t_dur, t);

        if (dd.is_zero()) {
            LOGD("cannot get gap at %d type: %d/%d", tick.ticks(), dur.numerator(), dur.denominator());
            break;
        }

        measure = segment->measure();
        std::vector<TDuration> dl = to_duration_list(dd, true);
        size_t n = dl.size();
        //add chord, tieing when necessary within measure
        for (size_t i = 0; i < n; ++i) {
            const TDuration &d = dl[i];

            //create new chord from template and add it
            Chord *chord = Factory::copy_chord(*chord_template);
            nr = chord;

            chord->set_track(track);
            chord->set_duration_type(d);
            chord->set_ticks(d.fraction());
            chord->set_tuplet(t);
            score->undo_add_cr(chord, measure, tick);
            //if there is something to tie, complete tie backwards
            //and add the tie to score
            const std::vector<Note *> notes = chord->notes();
            if (add_tie) {
                for (size_t j = 0; j < notes.size(); ++j) {
                    tie[j]->set_end_note(notes[j]);
                    notes[j]->set_tie_back(tie[j]);
                    score->undo_add_element(tie[j]);
                }
                add_tie = false;
            }
            //if we're not the last element in the duration list,
            //set tie forward
            if (i + 1 < n) {
                for (size_t j = 0; j < notes.size(); ++j) {
                    tie[j] = Factory::create_tie(notes[j]);
                    tie[j]->set_start_note(notes[j]);
                    tie[j]->set_tick(tie[j]->start_note()->tick());
                    tie[j]->set_track(track);
                    notes[j]->set_tie_for(tie[j]);
                    add_tie = true;
                }
            }
            score->set_play_chord(true);
            segment = chord->segment();
            tick += chord->actual_ticks();
        }

        //subtract the duration already realized and move on
        if (t) {
            dur -= dd / t->ratio();
        } else {
            dur -= dd;
        }
        //we are done when there is no duration left to realize
        if (dur.is_zero()) {
            break;
        }

        //go to next segment unless we are at the score (which means we will just be done there)
        Segment *nseg = score->tick2segment(tick, false, SegmentType::CHORD_REST);
        if (!nseg) {
            LOGD("reached end of score");
            break;
        }

        //it is possible that the next measure's ticks have not been computed yet. compute them now
        if (nseg->ticks().is_zero()) {
            nseg->measure()->compute_ticks();
        }

        segment = nseg;

        cr = to_chord_rest(segment->element(track));

        if (!cr) {
            if (track % ::mu::engraving::voices) {
                cr = score->add_rest(segment, track, TDuration(DurationType::V_MEASURE), 0);
            } else {
                LOGD("no rest in voice 0");
                break;
            }
        }
        //
        //  Note does not fit on current measure, create Tie to
        //  next part of note
        std::vector<Note *> notes = nr->notes();
        for (size_t i = 0; i < notes.size(); ++i) {
            tie[i] = Factory::create_tie(notes[i]);
            tie[i]->set_start_note(notes[i]);
            tie[i]->set_tick(tie[i]->start_note()->tick());
            tie[i]->set_track(notes[i]->track());
            notes[i]->set_tie_for(tie[i]);
        }
    }
    if (!tie.empty()) {
        score->connect_ties();
    }
    if (nr) {
        score->select(nr, SelectType::SINGLE, 0);
    }
    return segment;
}

//---------------------------------------------------------
//   realizeChordSymbols
///   Realize selected chord symbols into notes on the staff.
///
///   If a voicing and duration type are specified, the
///   harmony voicing settings will be overridden by the
///   passed parameters. Otherwise, the settings set on the
///   harmony object will be used.
//---------------------------------------------------------

void RealizeChordSymbols::realize_chord_symbols(Transaction &, Score *score, bool literal, Voicing voicing, HDuration duration_type) {
    // Create copy, because setChord selects newly created chord and thus
    // modifies selection().elements() while we're iterating over it
    const std::vector<EngravingItem *> elist = score->selection().elements();

    for (EngravingItem *e : elist) {
        if (!e->is_harmony()) {
            continue;
        }
        Harmony *h = to_harmony(e);
        if (!h->is_realizable()) {
            continue;
        }
        const RealizedHarmony &r = h->get_realized_harmony();
        Segment *seg =
            h->ownership_parent()->is_segment() ? to_segment(h->ownership_parent()) : to_segment(h->ownership_parent()->ownership_parent());
        Fraction tick = seg->tick();
        Fraction duration = r.get_actual_duration(tick.ticks(), duration_type) * h->staff()->time_stretch(tick);
        bool concert_pitch = score->style().style_b(Sid::CONCERT_PITCH);

        Chord *chord = Factory::create_chord(score->dummy()); //chord template
        chord->set_track(h->track());                         //set track so notes have a track to sit on

        //create chord from notes
        RealizedHarmony::PitchMap notes;
        if (voicing == Voicing::INVALID || duration_type == HDuration::INVALID) {
            notes = r.notes(); //no override, just use notes from realize harmony
        } else {
            //generate notes list based on overridden settings
            int offset = 0;
            Interval interval = h->staff()->part()->instrument(h->tick())->transpose();
            if (!concert_pitch) {
                offset = interval.chromatic;
            }
            notes = r.generate_notes(h->root_tpc(), h->bass_tpc(), literal, voicing, offset);
        }

        for (const auto &p : notes) {
            Note *note = Factory::create_note(chord);
            NoteVal nval;
            nval.pitch = p.first;
            if (concert_pitch) {
                nval.tpc1 = p.second;
            } else {
                nval.tpc2 = p.second;
            }
            chord->add(note); //add note first to set track and such
            note->set_nval(nval, tick);
        }

        if (!seg->is_chord_rest_type()) {
            Segment *new_cr_seg = seg->measure()->undo_get_segment(SegmentType::CHORD_REST, seg->tick());
            new_cr_seg->set_ticks(seg->ticks());
            seg = new_cr_seg;
        }

        set_chord(score, seg, h->track(), chord, duration); //add chord using template
        delete chord;
    }
}
