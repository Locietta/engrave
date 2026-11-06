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

#include "input.h"

#include "articulation.h"
#include "chord.h"
#include "durationtype.h"
#include "hook.h"
#include "measure.h"
#include "note.h"
#include "part.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "select.h"
#include "staff.h"
#include "stem.h"
#include "tuplet.h"

using namespace mu;

namespace mu::engraving {
class DrumSet;

bool InputState::is_valid() const { return segment_ != nullptr && track_ != muse::nidx; }

//---------------------------------------------------------
//   drumset
//---------------------------------------------------------

Staff *InputState::staff() const {
    if (!segment_ || track_ == muse::nidx) {
        return nullptr;
    }

    return segment_->score()->staff(mu::engraving::track2staff(track_));
}

staff_idx_t InputState::staff_idx() const { return mu::engraving::track2staff(track_); }

Drumset *InputState::drumset() const {
    if (!segment_ || track_ == muse::nidx) {
        return nullptr;
    }

    const Staff *staff = segment_->score()->staff(track_ / ::mu::engraving::voices);
    if (!staff) {
        return nullptr;
    }

    return staff->part()->instrument(segment_->tick())->drumset();
}

//---------------------------------------------------------
//   staffGroup
//---------------------------------------------------------

StaffGroup InputState::staff_group() const {
    if (!segment_ || track_ == muse::nidx) {
        return StaffGroup::STANDARD;
    }

    Fraction tick = segment_->tick();
    const Staff *staff = segment_->score()->staff(track_ / ::mu::engraving::voices);
    if (!staff) {
        return StaffGroup::STANDARD;
    }

    StaffGroup staff_group = staff->staff_type(tick)->group();
    const Instrument *instrument = staff->part()->instrument(tick);

    // if not tab, pitched/unpitched input depends on instrument, not staff (override StaffGroup)
    if (staff_group != StaffGroup::TAB) {
        staff_group = instrument->use_drumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
    }

    return staff_group;
}

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

Fraction InputState::tick() const { return segment_ ? segment_->tick() : Fraction(0, 1); }

//---------------------------------------------------------
//   cr
//---------------------------------------------------------

ChordRest *InputState::cr() const {
    // _track could potentially be invalid, for instance after navigation through a frame
    return segment_ && track_ != muse::nidx ? to_chord_rest(segment_->element(track_)) : 0;
}

//---------------------------------------------------------
//   setDots
//---------------------------------------------------------

void InputState::set_dots(int n) {
    if (n && (!duration_.is_valid() || duration_.is_zero() || duration_.is_measure())) {
        duration_ = DurationType::V_QUARTER;
    }
    duration_.set_dots(n);
}

void InputState::set_voice(voice_idx_t v) {
    const Score *score = segment_ ? segment_->score() : nullptr;

    if (!score || v >= ::mu::engraving::voices || v == voice() || track_ == muse::nidx) {
        return;
    }

    const track_idx_t new_track = (track_ / ::mu::engraving::voices) * ::mu::engraving::voices + v;
    Segment *curr_seg = segment();
    if (!curr_seg || curr_seg->cr(new_track)) {
        set_track(new_track);
        return;
    }

    // If we haven't returned early - it means that currSeg doesn't have a valid ChordRest for the desired voice. If
    // tuplets are involved, we may need to move the input position backwards to the last valid ChordRest...
    Segment *candidate_seg = curr_seg;

    // First thing to check - is currSeg part of a tuplet? If so, move candidateSeg to the start of that tuplet...
    const ChordRest *curr_cr = curr_seg->cr(track());
    const Tuplet *curr_tuplet = curr_cr ? curr_cr->top_tuplet() : nullptr;
    if (curr_tuplet) {
        candidate_seg = score->tick2segment(curr_tuplet->tick());
        IF_ASSERT_FAILED(candidate_seg) {
            set_segment(curr_seg->measure()->first());
            set_track(new_track);
            return;
        }
    }

    // If candidateSeg still doesn't have a valid ChordRest for the desired voice, we'll now check whether it is
    // within the range of a tuplet at our desired voice. If so, move candidateSeg to the start of that tuplet...
    const ChordRest *next_cr = candidate_seg->next_chord_rest(new_track, /*backwards*/ true, /*stopAtMeasureBoundary*/ true);
    const Tuplet *next_tuplet = next_cr ? next_cr->top_tuplet() : nullptr;
    if (!candidate_seg->cr(new_track) && next_tuplet) {
        candidate_seg = score->tick2segment(next_tuplet->tick());
        IF_ASSERT_FAILED(candidate_seg) {
            set_segment(curr_seg->measure()->first());
            set_track(new_track);
            return;
        }
    }

    if (candidate_seg != curr_seg) {
        set_segment(candidate_seg);
    }

    set_track(new_track);
}

//---------------------------------------------------------
//   note
//---------------------------------------------------------

Note *InputState::note(EngravingItem *e) { return e && e->is_note() ? to_note(e) : nullptr; }

//---------------------------------------------------------
//   chordRest
//---------------------------------------------------------

ChordRest *InputState::chord_rest(EngravingItem *e) {
    if (!e) {
        return nullptr;
    }
    if (e->is_chord_rest()) {
        return to_chord_rest(e);
    }
    if (e->is_note()) {
        return to_note(e)->chord();
    }
    if (e->is_stem()) {
        return to_stem(e)->chord();
    }
    if (e->is_hook()) {
        return to_hook(e)->chord();
    }
    return nullptr;
}

//---------------------------------------------------------
//   update
//---------------------------------------------------------

void InputState::update(Selection &selection) {
    set_duration(DurationType::V_INVALID);
    set_rest(false);
    set_accidental_type(AccidentalType::NONE);
    set_notes({});
    set_beyond_score(false);

    Note *n1 = nullptr;
    ChordRest *cr1 = nullptr;
    bool different_accidentals = false;
    bool different_durations = false;
    bool chords_and_rests = false;

    std::set<SymId> articulation_symbol_ids;
    for (EngravingItem *e : selection.elements()) {
        if (Note *n = note(e)) {
            if (n1) {
                if (n->accidental_type() != n1->accidental_type()) {
                    set_accidental_type(AccidentalType::NONE);
                    different_accidentals = true;
                }

                std::set<SymId> articulations_ids;
                for (Articulation *artic : n->chord()->articulations()) {
                    articulations_ids.insert(artic->sym_id());
                }

                articulations_ids = mu::engraving::split_articulations(articulations_ids);
                articulations_ids = mu::engraving::flip_articulations(articulations_ids, PlacementV::ABOVE);
                for (const SymId &articulation_symbol_id : articulations_ids) {
                    if (std::find(articulation_symbol_ids.begin(), articulation_symbol_ids.end(), articulation_symbol_id) ==
                        articulation_symbol_ids.end()) {
                        articulation_symbol_ids.erase(articulation_symbol_id);
                    }
                }
            } else {
                set_accidental_type(n->accidental_type());

                std::set<SymId> articulations_ids;
                for (Articulation *artic : n->chord()->articulations()) {
                    if (!artic->is_ornament()) {
                        articulations_ids.insert(artic->sym_id());
                    }
                }
                articulation_symbol_ids = mu::engraving::flip_articulations(articulations_ids, PlacementV::ABOVE);

                n1 = n;
            }
        }

        if (ChordRest *cr = chord_rest(e)) {
            if (cr1) {
                if (cr->duration_type() != cr1->duration_type()) {
                    set_duration(DurationType::V_INVALID);
                    different_durations = true;
                }
                if ((cr->is_rest() && !cr1->is_rest()) || (!cr->is_rest() && cr1->is_rest())) {
                    set_rest(false);
                    chords_and_rests = true;
                }
            } else {
                set_duration(cr->duration_type());
                set_rest(cr->is_rest());
                cr1 = cr;
            }
        }

        if (different_accidentals && different_durations && chords_and_rests) {
            break;
        }
    }

    set_articulation_ids(join_articulations(articulation_symbol_ids));

    EngravingItem *e = selection.element();
    if (!e) {
        if (!selection.is_none()) {
            set_track(selection.active_track());
            set_segment(selection.start_segment());
        }
        return;
    }

    ChordRest *cr = chord_rest(e);
    Note *n = note(e);
    if (!n && cr && cr->is_chord()) {
        n = to_chord(cr)->up_note();
    }

    if (cr) {
        set_track(cr->track());
        set_note_type(n ? n->note_type() : NoteType::NORMAL);
        set_beam_mode(cr->beam_mode());
    }

    set_drum_note(-1);
    if (n) {
        const Instrument *instr = n->part()->instrument(n->tick());
        if (instr->use_drumset()) {
            set_drum_note(n->pitch());
        }
    }
}

//---------------------------------------------------------
//   moveInputPos
//---------------------------------------------------------

void InputState::move_input_pos(EngravingItem *e) {
    if (e == 0) {
        return;
    }

    Segment *s;
    if (e->is_chord_rest()) {
        s = to_chord_rest(e)->segment();
    } else {
        s = to_segment(e);
    }

    if (s->is_segment()) {
        if (s->measure()->is_mm_rest()) {
            Measure *m = s->measure()->mm_rest_first();
            s = m->find_segment(SegmentType::CHORD_REST, m->tick());
        }
        last_segment_ = segment_;
        segment_ = s;
    }
}

//---------------------------------------------------------
//   setSegment
//---------------------------------------------------------

void InputState::set_segment(Segment *s) {
    if (s && s->measure()->is_mm_rest()) {
        Measure *m = s->measure()->mm_rest_first();
        s = m->find_segment(SegmentType::CHORD_REST, m->tick());
    }
    segment_ = s;
    last_segment_ = s;
}

//---------------------------------------------------------
//   nextInputPos
//---------------------------------------------------------

Segment *InputState::next_input_pos() const {
    Measure *m = segment_->measure();
    Segment *s = segment_->next1(SegmentType::CHORD_REST);
    while (s) {
        if (s->element(track_) || s->measure() != m) {
            return s;
        }
        s = s->next1(SegmentType::CHORD_REST);
    }
    return 0;
}

//---------------------------------------------------------
//   moveToNextInputPos
//   TODO: special case: note is first note of tie: goto to last note of tie
//---------------------------------------------------------

void InputState::move_to_next_input_pos() {
    Segment *s = next_input_pos();
    last_segment_ = segment_;
    if (s) {
        segment_ = s;
        beyond_score_ = false;
    } else {
        beyond_score_ = true;
    }
}
} // namespace mu::engraving
