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

#include "mcursor.h"

#include "compat/scoreaccess.h"

#include "chord.h"
#include "durationtype.h"
#include "factory.h"
#include "instrtemplate.h"
#include "keysig.h"
#include "masterscore.h"
#include "measure.h"
#include "note.h"
#include "part.h"
#include "segment.h"
#include "staff.h"
#include "timesig.h"
#include "sig.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   MCursor
//---------------------------------------------------------

MCursor::MCursor(MasterScore *s) {
    score_ = s;
    move(0, Fraction(0, 1));
}

//---------------------------------------------------------
//   createMeasures
//---------------------------------------------------------

void MCursor::createMeasures() {
    Measure *measure;
    for (;;) {
        Fraction tick = Fraction(0, 1);
        measure = score_->lastMeasure();
        if (measure) {
            tick = measure->endTick();
            if (tick > tick_) {
                break;
            }
        }
        measure = Factory::createMeasure(score_);
        measure->setTick(tick);
        measure->setTimesig(sig_);
        measure->setTicks(sig_);
        score_->measures()->append(measure);
    }
}

//---------------------------------------------------------
//   addChord
//---------------------------------------------------------

Chord *MCursor::addChord(int pitch, const TDuration &duration) {
    createMeasures();
    Measure *measure = score_->tick2measure(tick_);
    Segment *segment = measure->getSegment(SegmentType::ChordRest, tick_);
    Chord *chord = toChord(segment->element(track_));
    if (chord == 0) {
        chord = Factory::createChord(segment);
        chord->setTrack(track_);
        chord->setDurationType(duration);
        chord->setTicks(duration.fraction());
        segment->add(chord);
    }
    Note *note = Factory::createNote(chord);
    chord->add(note);
    note->setPitch(pitch);
    note->setTpcFromPitch();
    tick_ += duration.ticks();
    return chord;
}

//---------------------------------------------------------
//   addKeySig
//---------------------------------------------------------

void MCursor::addKeySig(Key key) {
    createMeasures();
    Measure *measure = score_->tick2measure(tick_);
    Segment *segment = measure->getSegment(SegmentType::KeySig, tick_);
    size_t n = score_->nstaves();
    for (staff_idx_t i = 0; i < n; ++i) {
        KeySig *ks = Factory::createKeySig(segment);
        ks->setKey(key);
        ks->setTrack(i * VOICES);
        segment->add(ks);
    }
}

//---------------------------------------------------------
//   addTimeSig
//---------------------------------------------------------

TimeSig *MCursor::addTimeSig(const Fraction &f) {
    createMeasures();
    Measure *measure = score_->tick2measure(tick_);
    Segment *segment = measure->getSegment(SegmentType::TimeSig, tick_);
    TimeSig *ts = 0;
    for (size_t i = 0; i < score_->nstaves(); ++i) {
        ts = Factory::createTimeSig(segment);
        ts->setSig(f, TimeSigType::NORMAL);
        ts->setTrack(i * VOICES);
        segment->add(ts);
    }
    score_->sigmap()->add(tick_.ticks(), SigEvent(f));
    return ts;
}

//---------------------------------------------------------
//   createScore
//---------------------------------------------------------

void MCursor::createScore(const EngravingServices &services, const String & /*name*/) {
    delete score_;
    score_ = compat::ScoreAccess::createMasterScoreWithBaseStyle(services);
    // TODO: set path/filename
    NOT_IMPLEMENTED;
    move(0, Fraction(0, 1));
}

//---------------------------------------------------------
//   move
//---------------------------------------------------------

void MCursor::move(int t, const Fraction &tick) {
    track_ = t;
    tick_ = tick;
}

//---------------------------------------------------------
//   addPart
//---------------------------------------------------------

void MCursor::addPart(const String &instrument) {
    Part *part = new Part(score_);
    Staff *staff = Factory::createStaff(part);
    const InstrumentTemplate *it = searchTemplate(instrument);
    IF_ASSERT_FAILED(it) { return; }

    part->initFromInstrTemplate(it);
    score_->appendPart(part);
    score_->insertStaff(staff, 0);
    staff->init(it, 0, 0);
}
} // namespace mu::engraving
