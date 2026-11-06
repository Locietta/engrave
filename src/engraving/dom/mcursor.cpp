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

void MCursor::create_measures() {
    Measure *measure;
    for (;;) {
        Fraction tick = Fraction(0, 1);
        measure = score_->last_measure();
        if (measure) {
            tick = measure->end_tick();
            if (tick > tick_) {
                break;
            }
        }
        measure = Factory::create_measure(score_);
        measure->set_tick(tick);
        measure->set_timesig(sig_);
        measure->set_ticks(sig_);
        score_->measures()->append(measure);
    }
}

//---------------------------------------------------------
//   addChord
//---------------------------------------------------------

Chord *MCursor::add_chord(int pitch, const TDuration &duration) {
    create_measures();
    Measure *measure = score_->tick2measure(tick_);
    Segment *segment = measure->get_segment(SegmentType::CHORD_REST, tick_);
    Chord *chord = to_chord(segment->element(track_));
    if (chord == 0) {
        chord = Factory::create_chord(segment);
        chord->set_track(track_);
        chord->set_duration_type(duration);
        chord->set_ticks(duration.fraction());
        segment->add(chord);
    }
    Note *note = Factory::create_note(chord);
    chord->add(note);
    note->set_pitch(pitch);
    note->set_tpc_from_pitch();
    tick_ += duration.ticks();
    return chord;
}

//---------------------------------------------------------
//   addKeySig
//---------------------------------------------------------

void MCursor::add_key_sig(Key key) {
    create_measures();
    Measure *measure = score_->tick2measure(tick_);
    Segment *segment = measure->get_segment(SegmentType::KEY_SIG, tick_);
    size_t n = score_->nstaves();
    for (staff_idx_t i = 0; i < n; ++i) {
        KeySig *ks = Factory::create_key_sig(segment);
        ks->set_key(key);
        ks->set_track(i * ::mu::engraving::voices);
        segment->add(ks);
    }
}

//---------------------------------------------------------
//   addTimeSig
//---------------------------------------------------------

TimeSig *MCursor::add_time_sig(const Fraction &f) {
    create_measures();
    Measure *measure = score_->tick2measure(tick_);
    Segment *segment = measure->get_segment(SegmentType::TIME_SIG, tick_);
    TimeSig *ts = 0;
    for (size_t i = 0; i < score_->nstaves(); ++i) {
        ts = Factory::create_time_sig(segment);
        ts->set_sig(f, TimeSigType::NORMAL);
        ts->set_track(i * ::mu::engraving::voices);
        segment->add(ts);
    }
    score_->sigmap()->add(tick_.ticks(), SigEvent(f));
    return ts;
}

//---------------------------------------------------------
//   createScore
//---------------------------------------------------------

void MCursor::create_score(const EngravingServices &services, const String & /*name*/) {
    delete score_;
    score_ = compat::ScoreAccess::create_master_score_with_base_style(services);
    // TODO: set path/filename
    MUSE_NOT_IMPLEMENTED;
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

void MCursor::add_part(const String &instrument) {
    Part *part = new Part(score_);
    Staff *staff = Factory::create_staff(part);
    const InstrumentTemplate *it = search_template(instrument);
    IF_ASSERT_FAILED(it) { return; }

    part->init_from_instr_template(it);
    score_->append_part(part);
    score_->insert_staff(staff, 0);
    staff->init(it, 0, 0);
}
} // namespace mu::engraving
