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

bool InputState::isValid() const { return segment_ != nullptr && track_ != muse::nidx; }

//---------------------------------------------------------
//   drumset
//---------------------------------------------------------

Staff *InputState::staff() const {
    if (!segment_ || track_ == muse::nidx) {
        return nullptr;
    }

    return segment_->score()->staff(mu::engraving::track2staff(track_));
}

staff_idx_t InputState::staffIdx() const { return mu::engraving::track2staff(track_); }

Drumset *InputState::drumset() const {
    if (!segment_ || track_ == muse::nidx) {
        return nullptr;
    }

    const Staff *staff = segment_->score()->staff(track_ / VOICES);
    if (!staff) {
        return nullptr;
    }

    return staff->part()->instrument(segment_->tick())->drumset();
}

//---------------------------------------------------------
//   staffGroup
//---------------------------------------------------------

StaffGroup InputState::staffGroup() const {
    if (!segment_ || track_ == muse::nidx) {
        return StaffGroup::STANDARD;
    }

    Fraction tick = segment_->tick();
    const Staff *staff = segment_->score()->staff(track_ / VOICES);
    if (!staff) {
        return StaffGroup::STANDARD;
    }

    StaffGroup staffGroup = staff->staffType(tick)->group();
    const Instrument *instrument = staff->part()->instrument(tick);

    // if not tab, pitched/unpitched input depends on instrument, not staff (override StaffGroup)
    if (staffGroup != StaffGroup::TAB) {
        staffGroup = instrument->useDrumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
    }

    return staffGroup;
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
    return segment_ && track_ != muse::nidx ? toChordRest(segment_->element(track_)) : 0;
}

//---------------------------------------------------------
//   setDots
//---------------------------------------------------------

void InputState::setDots(int n) {
    if (n && (!duration_.isValid() || duration_.isZero() || duration_.isMeasure())) {
        duration_ = DurationType::V_QUARTER;
    }
    duration_.setDots(n);
}

void InputState::setVoice(voice_idx_t v) {
    const Score *score = segment_ ? segment_->score() : nullptr;

    if (!score || v >= VOICES || v == voice() || track_ == muse::nidx) {
        return;
    }

    const track_idx_t newTrack = (track_ / VOICES) * VOICES + v;
    Segment *currSeg = segment();
    if (!currSeg || currSeg->cr(newTrack)) {
        setTrack(newTrack);
        return;
    }

    // If we haven't returned early - it means that currSeg doesn't have a valid ChordRest for the desired voice. If
    // tuplets are involved, we may need to move the input position backwards to the last valid ChordRest...
    Segment *candidateSeg = currSeg;

    // First thing to check - is currSeg part of a tuplet? If so, move candidateSeg to the start of that tuplet...
    const ChordRest *currCR = currSeg->cr(track());
    const Tuplet *currTuplet = currCR ? currCR->topTuplet() : nullptr;
    if (currTuplet) {
        candidateSeg = score->tick2segment(currTuplet->tick());
        IF_ASSERT_FAILED(candidateSeg) {
            setSegment(currSeg->measure()->first());
            setTrack(newTrack);
            return;
        }
    }

    // If candidateSeg still doesn't have a valid ChordRest for the desired voice, we'll now check whether it is
    // within the range of a tuplet at our desired voice. If so, move candidateSeg to the start of that tuplet...
    const ChordRest *nextCR = candidateSeg->nextChordRest(newTrack, /*backwards*/ true, /*stopAtMeasureBoundary*/ true);
    const Tuplet *nextTuplet = nextCR ? nextCR->topTuplet() : nullptr;
    if (!candidateSeg->cr(newTrack) && nextTuplet) {
        candidateSeg = score->tick2segment(nextTuplet->tick());
        IF_ASSERT_FAILED(candidateSeg) {
            setSegment(currSeg->measure()->first());
            setTrack(newTrack);
            return;
        }
    }

    if (candidateSeg != currSeg) {
        setSegment(candidateSeg);
    }

    setTrack(newTrack);
}

//---------------------------------------------------------
//   note
//---------------------------------------------------------

Note *InputState::note(EngravingItem *e) { return e && e->isNote() ? toNote(e) : nullptr; }

//---------------------------------------------------------
//   chordRest
//---------------------------------------------------------

ChordRest *InputState::chordRest(EngravingItem *e) {
    if (!e) {
        return nullptr;
    }
    if (e->isChordRest()) {
        return toChordRest(e);
    }
    if (e->isNote()) {
        return toNote(e)->chord();
    }
    if (e->isStem()) {
        return toStem(e)->chord();
    }
    if (e->isHook()) {
        return toHook(e)->chord();
    }
    return nullptr;
}

//---------------------------------------------------------
//   update
//---------------------------------------------------------

void InputState::update(Selection &selection) {
    setDuration(DurationType::V_INVALID);
    setRest(false);
    setAccidentalType(AccidentalType::NONE);
    setNotes({});
    setBeyondScore(false);

    Note *n1 = nullptr;
    ChordRest *cr1 = nullptr;
    bool differentAccidentals = false;
    bool differentDurations = false;
    bool chordsAndRests = false;

    std::set<SymId> articulationSymbolIds;
    for (EngravingItem *e : selection.elements()) {
        if (Note *n = note(e)) {
            if (n1) {
                if (n->accidentalType() != n1->accidentalType()) {
                    setAccidentalType(AccidentalType::NONE);
                    differentAccidentals = true;
                }

                std::set<SymId> articulationsIds;
                for (Articulation *artic : n->chord()->articulations()) {
                    articulationsIds.insert(artic->symId());
                }

                articulationsIds = mu::engraving::splitArticulations(articulationsIds);
                articulationsIds = mu::engraving::flipArticulations(articulationsIds, PlacementV::ABOVE);
                for (const SymId &articulationSymbolId : articulationsIds) {
                    if (std::find(articulationSymbolIds.begin(), articulationSymbolIds.end(), articulationSymbolId) ==
                        articulationSymbolIds.end()) {
                        articulationSymbolIds.erase(articulationSymbolId);
                    }
                }
            } else {
                setAccidentalType(n->accidentalType());

                std::set<SymId> articulationsIds;
                for (Articulation *artic : n->chord()->articulations()) {
                    if (!artic->isOrnament()) {
                        articulationsIds.insert(artic->symId());
                    }
                }
                articulationSymbolIds = mu::engraving::flipArticulations(articulationsIds, PlacementV::ABOVE);

                n1 = n;
            }
        }

        if (ChordRest *cr = chordRest(e)) {
            if (cr1) {
                if (cr->durationType() != cr1->durationType()) {
                    setDuration(DurationType::V_INVALID);
                    differentDurations = true;
                }
                if ((cr->isRest() && !cr1->isRest()) || (!cr->isRest() && cr1->isRest())) {
                    setRest(false);
                    chordsAndRests = true;
                }
            } else {
                setDuration(cr->durationType());
                setRest(cr->isRest());
                cr1 = cr;
            }
        }

        if (differentAccidentals && differentDurations && chordsAndRests) {
            break;
        }
    }

    setArticulationIds(joinArticulations(articulationSymbolIds));

    EngravingItem *e = selection.element();
    if (!e) {
        if (!selection.isNone()) {
            setTrack(selection.activeTrack());
            setSegment(selection.startSegment());
        }
        return;
    }

    ChordRest *cr = chordRest(e);
    Note *n = note(e);
    if (!n && cr && cr->isChord()) {
        n = toChord(cr)->upNote();
    }

    if (cr) {
        setTrack(cr->track());
        setNoteType(n ? n->noteType() : NoteType::NORMAL);
        setBeamMode(cr->beamMode());
    }

    setDrumNote(-1);
    if (n) {
        const Instrument *instr = n->part()->instrument(n->tick());
        if (instr->useDrumset()) {
            setDrumNote(n->pitch());
        }
    }
}

//---------------------------------------------------------
//   moveInputPos
//---------------------------------------------------------

void InputState::moveInputPos(EngravingItem *e) {
    if (e == 0) {
        return;
    }

    Segment *s;
    if (e->isChordRest()) {
        s = toChordRest(e)->segment();
    } else {
        s = toSegment(e);
    }

    if (s->isSegment()) {
        if (s->measure()->isMMRest()) {
            Measure *m = s->measure()->mmRestFirst();
            s = m->findSegment(SegmentType::ChordRest, m->tick());
        }
        lastSegment_ = segment_;
        segment_ = s;
    }
}

//---------------------------------------------------------
//   setSegment
//---------------------------------------------------------

void InputState::setSegment(Segment *s) {
    if (s && s->measure()->isMMRest()) {
        Measure *m = s->measure()->mmRestFirst();
        s = m->findSegment(SegmentType::ChordRest, m->tick());
    }
    segment_ = s;
    lastSegment_ = s;
}

//---------------------------------------------------------
//   nextInputPos
//---------------------------------------------------------

Segment *InputState::nextInputPos() const {
    Measure *m = segment_->measure();
    Segment *s = segment_->next1(SegmentType::ChordRest);
    while (s) {
        if (s->element(track_) || s->measure() != m) {
            return s;
        }
        s = s->next1(SegmentType::ChordRest);
    }
    return 0;
}

//---------------------------------------------------------
//   moveToNextInputPos
//   TODO: special case: note is first note of tie: goto to last note of tie
//---------------------------------------------------------

void InputState::moveToNextInputPos() {
    Segment *s = nextInputPos();
    lastSegment_ = segment_;
    if (s) {
        segment_ = s;
        beyondScore_ = false;
    } else {
        beyondScore_ = true;
    }
}
} // namespace mu::engraving
