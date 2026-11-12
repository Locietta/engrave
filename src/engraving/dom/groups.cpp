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

#include "groups.h"

#include <unordered_map>

#include "chordrest.h"
#include "durationtype.h"
#include "log.h"
#include "measure.h"
#include "staff.h"
#include "timesig.h"
#include "tuplet.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   NoteGroup
//---------------------------------------------------------

struct NoteGroup {
    Fraction time_sig;
    Groups endings;
};

//---------------------------------------------------------
//   noteGroups
//---------------------------------------------------------

static std::vector<NoteGroup> note_groups{
    {Fraction(2, 2), Groups({{4, 0x200}, {8, 0x110}, {12, 0x200}, {16, 0x111}, {20, 0x200}, {24, 0x110}, {28, 0x200}})},
    {Fraction(3, 2), Groups({{4, 0x200},
                             {8, 0x110},
                             {12, 0x200},
                             {16, 0x111},
                             {20, 0x200},
                             {24, 0x110},
                             {28, 0x200},
                             {32, 0x111},
                             {36, 0x200},
                             {40, 0x110},
                             {44, 0x200}})},
    {Fraction(4, 2), Groups({{4, 0x200},
                             {8, 0x110},
                             {12, 0x200},
                             {16, 0x111},
                             {20, 0x200},
                             {24, 0x110},
                             {28, 0x200},
                             {32, 0x111},
                             {36, 0x200},
                             {40, 0x110},
                             {44, 0x200},
                             {48, 0x111},
                             {52, 0x200},
                             {56, 0x110},
                             {60, 0x200}})},
    {Fraction(2, 4), Groups({{4, 0x200}, {8, 0x111}, {12, 0x200}})},
    {Fraction(3, 4), Groups({{4, 0x200}, {8, 0x111}, {12, 0x200}, {16, 0x111}, {20, 0x200}})},
    {Fraction(4, 4), Groups({{4, 0x200}, {8, 0x110}, {12, 0x200}, {16, 0x111}, {20, 0x200}, {24, 0x110}, {28, 0x200}})},
    {Fraction(5, 4),
     Groups({{4, 0x200}, {8, 0x110}, {12, 0x200}, {16, 0x110}, {20, 0x200}, {24, 0x111}, {28, 0x200}, {32, 0x110}, {36, 0x200}})},
    {Fraction(6, 4), Groups({{4, 0x200},
                             {8, 0x110},
                             {12, 0x200},
                             {16, 0x110},
                             {20, 0x200},
                             {24, 0x111},
                             {28, 0x200},
                             {32, 0x110},
                             {36, 0x200},
                             {40, 0x110},
                             {44, 0x200}})},
    {Fraction(3, 8), Groups({{4, 0x200}, {8, 0x200}})},
    {Fraction(5, 8), Groups({{4, 0x200}, {8, 0x200}, {12, 0x111}, {16, 0x200}})},
    {Fraction(6, 8), Groups({{4, 0x200}, {8, 0x200}, {12, 0x111}, {16, 0x200}, {20, 0x200}})},
    {Fraction(7, 8), Groups({{4, 0x200}, {8, 0x200}, {12, 0x111}, {16, 0x200}, {20, 0x111}, {24, 0x200}})},
    {Fraction(9, 8), Groups({{4, 0x200}, {8, 0x200}, {12, 0x111}, {16, 0x200}, {20, 0x200}, {24, 0x111}, {28, 0x200}, {32, 0x200}})},
    {Fraction(12, 8), Groups({{4, 0x200},
                              {8, 0x200},
                              {12, 0x111},
                              {16, 0x200},
                              {20, 0x200},
                              {24, 0x111},
                              {28, 0x200},
                              {32, 0x200},
                              {36, 0x111},
                              {40, 0x200},
                              {44, 0x200}})},
};

//---------------------------------------------------------
//   baseBeamMode
//    based on time signature properties but respecting manual settings
//---------------------------------------------------------

BeamMode Groups::base_beam_mode(const ChordRest *cr, const ChordRest *prev) { return base_beam_mode(cr, prev, cr->duration_type()); }

BeamMode Groups::base_beam_mode(const ChordRest *cr, const ChordRest *prev, TDuration cr_duration) {
    if (cr->is_grace() || cr->beam_mode() != BeamMode::AUTO) {
        return cr->beam_mode();
    }
    assert(cr->staff());

    // we need to figure out the longest note value beat upon which cr falls in the measure
    Fraction max_tick_len = std::max(cr->ticks(), prev ? prev->ticks() : Fraction());
    Fraction smallest_tick_len = Fraction(1, 8); // start with 8th
    Fraction tick_len_limit = Fraction(1, 32);   // only check up to 32nds because that's all thats available
                                                 // in timesig properties
    while (smallest_tick_len > max_tick_len || cr->rtick().ticks() % smallest_tick_len.ticks() != 0) {
        smallest_tick_len /= 2; // proceed to 16th, 32nd, etc
        if (smallest_tick_len < tick_len_limit) {
            smallest_tick_len = cr->ticks();
            break;
        }
    }
    DurationType big_beat_duration = TDuration(smallest_tick_len).type();

    const Groups &g = cr->staff()->group(cr->tick());
    Fraction stretch = cr->staff()->time_stretch(cr->tick());
    Fraction tick = cr->rtick() * stretch + cr->measure()->anacrusis_offset();

    // We can choose to break beams based on its place in the measure, or by its duration. These
    // can be consolidated mostly, with bias towards its duration.
    BeamMode by_type = g.beam_mode(tick.ticks(), cr_duration.type());
    BeamMode by_pos = g.beam_mode(tick.ticks(), big_beat_duration);
    BeamMode val = by_type == BeamMode::AUTO ? by_pos : by_type;

    // context-dependent checks
    if (val == BeamMode::AUTO && tick.is_not_zero()) {
        // if current or previous cr is in tuplet (but not both in same tuplet):
        // consider it as if this were next shorter duration
        if (prev && (cr->tuplet() != prev->tuplet()) && (cr_duration == prev->duration_type())) {
            if (cr_duration >= DurationType::V_EIGHTH) {
                val = g.beam_mode(tick.ticks(), DurationType::V_16TH);
            } else if (cr_duration == DurationType::V_16TH) {
                val = g.beam_mode(tick.ticks(), DurationType::V_32ND);
            } else {
                val = g.beam_mode(tick.ticks(), DurationType::V_64TH);
            }
        }
        // if there is a hole between previous and current cr, break beam
        // exclude tuplets from this check; tick calculations can be unreliable
        // and they seem to be handled well anyhow
        if (cr->voice() && prev && !prev->tuplet() && prev->end_tick() < cr->tick()) {
            val = BeamMode::BEGIN;
        }
    }

    return val;
}

//---------------------------------------------------------
//   actualBeamMode
//    final beam mode after applying contextual corrections
//---------------------------------------------------------

BeamMode Groups::actual_beam_mode(const ChordRest *cr, const ChordRest *prev) {
    Measure *measure = cr->measure();
    const Staff *stf = cr->staff();
    TimeSig *ts = stf->time_sig(measure->tick());
    bool check_beats = ts && ts->denominator() == 4;

    std::unordered_map<int, TDuration> beat_subdivision;
    if (check_beats && cr->rtick().is_not_zero()) {
        Fraction stretch = ts->stretch();
        Fraction tick = cr->rtick() * stretch;

        if ((tick.ticks() % Constants::division) == 0) {
            for (Segment *s = measure->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                ChordRest *mcr = to_chord_rest(s->element(cr->track()));
                if (mcr == 0) {
                    continue;
                }
                int beat = (mcr->rtick() * stretch).ticks() / Constants::division;
                if (muse::contains(beat_subdivision, beat)) {
                    beat_subdivision[beat] = std::min(beat_subdivision[beat], mcr->duration_type());
                } else {
                    beat_subdivision[beat] = mcr->duration_type();
                }
            }
        }
    }

    return actual_beam_mode(cr, prev, &beat_subdivision);
}

BeamMode Groups::actual_beam_mode(const ChordRest *cr, const ChordRest *prev, const std::unordered_map<int, TDuration> *beat_subdivision) {
    // do not beam rests set to BeamMode::AUTO or with only other rests
    if (cr->is_rest() && cr->beam_mode() == BeamMode::AUTO) {
        return BeamMode::NONE;
    }

    TDuration duration_type = cr->duration_type();
    // chord with no hooks cannot be beamed
    if (cr->is_chord() && duration_type.hooks() == 0) {
        return BeamMode::NONE;
    }
    // if chord has hooks and is 2nd element of a cross-measure value
    // set beam mode to NONE (do not combine with following chord beam/hook, if any)
    if (duration_type.hooks() > 0 && cr->cross_measure() == CrossMeasure::SECOND) {
        return BeamMode::NONE;
    }

    BeamMode bm = Groups::base_beam_mode(cr, prev);

    if (bm == BeamMode::AUTO) {
        // start beam at the beginning of measures
        if (cr->rtick().is_zero()) {
            return BeamMode::BEGIN;
        }

        // perform additional context-dependent checks
        Measure *measure = cr->measure();
        const Staff *stf = cr->staff();
        TimeSig *ts = stf->time_sig(measure->tick());
        bool check_beats = ts && ts->denominator() == 4;
        Fraction stretch = ts ? ts->stretch() : Fraction(1, 1);

        // check if we need to break beams according to minimum duration in current / previous beat
        if (check_beats) {
            Fraction tick = cr->rtick() * stretch;
            // check if on the beat
            if ((tick.ticks() % Constants::division) == 0) {
                int beat = tick.ticks() / Constants::division;
                // get minimum duration for this & previous beat
                auto it_current = beat_subdivision->find(beat);
                auto it_prev = beat_subdivision->find(beat - 1);
                TDuration current_min = (it_current != beat_subdivision->end()) ? it_current->second : TDuration(DurationType::V_INVALID);
                TDuration prev_min = (it_prev != beat_subdivision->end()) ? it_prev->second : TDuration(DurationType::V_INVALID);
                TDuration min_duration = std::min(current_min, prev_min);
                // re-calculate beam as if this were the duration of current chordrest
                bm = Groups::base_beam_mode(cr, prev, min_duration);
            }
        }
    }

    return bm == BeamMode::AUTO ? BeamMode::MID : bm;
}

//---------------------------------------------------------
//   beamMode
//    tick is relative to begin of measure
//---------------------------------------------------------

BeamMode Groups::beam_mode(int tick, DurationType d) const {
    int shift;
    switch (d) {
        case DurationType::V_EIGHTH: shift = 0; break;
        case DurationType::V_16TH: shift = 4; break;
        case DurationType::V_32ND:
        // All beams shorter than 32nds break in the same place
        case DurationType::V_64TH:
        case DurationType::V_128TH:
        case DurationType::V_256TH:
        case DurationType::V_512TH:
        case DurationType::V_1024TH: shift = 8; break;
        default: return BeamMode::AUTO;
    }
    const int dm = Constants::division / 8;
    for (const GroupNode &e : nodes_) {
        if (e.pos * dm < tick) {
            continue;
        }
        if (e.pos * dm > tick) {
            break;
        }

        int action = (e.action >> shift) & 0xf;
        switch (action) {
            case 0: return BeamMode::AUTO;
            case 1: return BeamMode::BEGIN;
            case 2: return BeamMode::BEGIN16;
            case 3: return BeamMode::BEGIN32;
            default: LOGD("   Groups::beamMode: bad action %d", action); return BeamMode::AUTO;
        }
    }
    return BeamMode::AUTO;
}

//---------------------------------------------------------
//   endings
//---------------------------------------------------------

const Groups &Groups::endings(const Fraction &f) {
    for (const NoteGroup &g : note_groups) {
        if (g.time_sig.identical(f)) {
            return g.endings;
        }
    }
    NoteGroup g;
    g.time_sig = f;
    note_groups.push_back(g);

    int pos = 0;
    switch (f.denominator()) {
        case 2: pos = 16; break;
        case 4: pos = 8; break;
        case 8: pos = 4; break;
        case 16: pos = 2; break;
        case 32:
            pos = 1;
            break;
            break;
    }
    for (int i = 1; i < f.numerator(); ++i) {
        GroupNode n;
        n.pos = pos * i;
        n.action = 0x111;
        g.endings.add_node(n);
    }
    return note_groups.back().endings;
}

//---------------------------------------------------------
//   addStop
//---------------------------------------------------------

void Groups::add_stop(int pos, DurationType d, BeamMode bm) {
    int shift;
    switch (d) {
        case DurationType::V_EIGHTH: shift = 0; break;
        case DurationType::V_16TH: shift = 4; break;
        case DurationType::V_32ND: shift = 8; break;
        default: return;
    }
    int action;
    if (bm == BeamMode::BEGIN) {
        action = 1;
    } else if (bm == BeamMode::BEGIN16) {
        action = 2;
    } else if (bm == BeamMode::BEGIN32) {
        action = 3;
    } else {
        return;
    }

    pos /= 60;
    action <<= shift;

    auto i = nodes_.begin();
    for (; i != nodes_.end(); ++i) {
        if (i->pos == pos) {
            i->action = (i->action & ~(0xf << shift)) | action;
            return;
        }
        if (i->pos > pos) {
            break;
        }
    }
    nodes_.insert(i, GroupNode({pos, action}));
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Groups::dump(const char *m) const {
    LOGD("%s", m);
    for (const GroupNode &n : nodes_) {
        LOGD("  group tick %d action 0x%02x", n.pos * 60, n.action);
    }
}
} // namespace mu::engraving
