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

#include "sig.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   ticks_beat
//---------------------------------------------------------

int ticks_beat(int n) {
    int m = (Constants::division * 4) / n;
    if ((Constants::division * 4) % n) {
        ASSERT_X(String(u"Mscore: ticks_beat(): bad divisor %1").arg(n));
    }
    return m;
}

//---------------------------------------------------------
//   ticks_measure
//---------------------------------------------------------

static int ticks_measure(const Fraction &f) { return (Constants::division * 4 * f.numerator()) / f.denominator(); }

//---------------------------------------------------------
//   rtick2beatType
//    caller must adjust rtick as appropriate if the measure's
//    actual timeSig is different from the nominal timeSig.
//---------------------------------------------------------

BeatType TimeSigFrac::rtick2beat_type(int rtick) const {
    if (rtick == 0) {
        return BeatType::DOWNBEAT; // note: only return DOWNBEAT for rtick = 0, not for rtick = measureTicks,
    }
    if (rtick % d_unit_ticks() != 0) {
        return BeatType::SUBBEAT;
    }

    if (is_compound()) {
        if (rtick % beat_ticks() != 0) {
            return BeatType::COMPOUND_SUBBEAT;
        }
    }

    const int beat_num = rtick / beat_ticks();

    int stress_beat = 0;

    if (is_triple()) {
        stress_beat = 3;
    } else if (is_duple()) {
        stress_beat = 2;
    } else {
        stress_beat = (numerator() + 1) / 2; // Assumes 5/4 timeSig = (3+2)/4. (The same assumption is used for beaming)
    }
    if (stress_beat && beat_num % stress_beat == 0) {
        return is_compound() ? BeatType::COMPOUND_STRESSED : BeatType::SIMPLE_STRESSED;
    }

    return is_compound() ? BeatType::COMPOUND_UNSTRESSED : BeatType::SIMPLE_UNSTRESSED;
}

//---------------------------------------------------------
//   strongestBeatInRange
//    dUnitsCrossed - pointer to store number crossed
//    subbeatTick - pointer to store tick of strongest beat
//    saveLast - which tick to store if strongest type is
//                crossed more than once
//
//    caller must adjust rticks as appropriate if the measure's
//    actual timeSig is different from the nominal timeSig.
//---------------------------------------------------------

BeatType TimeSigFrac::strongest_beat_in_range(int rtick1, int rtick2, int *d_units_crossed, int *subbeat_tick, bool save_last) const {
    assert(rtick2 > rtick1);

    BeatType strongest = BeatType::SUBBEAT;

    for (int rtick = rtick1 + ticks_to_next_d_unit(rtick1); rtick < rtick2; rtick += d_unit_ticks()) {
        if (d_units_crossed) {
            (*d_units_crossed)++;
        }
        BeatType type = rtick2beat_type(rtick);
        if (static_cast<int>(type) < static_cast<int>(strongest) + save_last) { // "<" behaves like "<=" if saveLast is true
            strongest = type;
            if (subbeat_tick) {
                (*subbeat_tick) = rtick;
            }
        }
    }

    return strongest;
}

//---------------------------------------------------------
//   subbeatTicks
//    divides dUnitTicks() by 2 once for each level.
//---------------------------------------------------------

int TimeSigFrac::subbeat_ticks(int level) const {
    assert(level <= max_subbeat_level());
    int subbeat_ticks = d_unit_ticks();
    while (level > 0) {
        subbeat_ticks /= 2;
        level--;
    }
    return subbeat_ticks;
}

//---------------------------------------------------------
//   maxSubbeatLevel
//    subdivision beyond this level would result in rounding errors
//---------------------------------------------------------

int TimeSigFrac::max_subbeat_level() const {
    int level = 0;
    int subbeat_ticks = d_unit_ticks();
    while (subbeat_ticks % 2 == 0) {
        subbeat_ticks /= 2;
        level++;
    }
    return level;
}

//---------------------------------------------------------
//   rtick2subbeatLevel
//    returns 0 if rtick is on a beat or denominator unit.
//    returns 1 if rtick lies halfway between dUnits
//    returns 2 if rtick lies on a multiple of  1/4  of dUnit
//            3                                 1/8
//            4                                 1/16
//            n                                 1/(2**n)
//    returns -(n+1) if max n is reached and rtick still not found.
//
//    Caller must adjust rtick as appropriate if the measure's
//    actual timeSig is different from the nominal timeSig.
//---------------------------------------------------------

int TimeSigFrac::rtick2subbeat_level(int rtick) const {
    int level = 0;
    int subbeat_ticks = d_unit_ticks();
    int remainder = rtick % subbeat_ticks;
    while (remainder != 0) {
        level++;
        if (subbeat_ticks % 2 != 0) {
            return -level; // further sub-division would split measure into chunks of unequal length.
        }
        subbeat_ticks /= 2;
        remainder %= subbeat_ticks;
    }
    return level;
}

//---------------------------------------------------------
//   strongestSubbeatLevelInRange
//    Return value is negative if none are found.
//
//    Caller must adjust rtick as appropriate if the measure's
//    actual timeSig is different from the nominal timeSig.
//---------------------------------------------------------

int TimeSigFrac::strongest_subbeat_level_in_range(int rtick1, int rtick2, int *subbeat_tick) const {
    assert(rtick2 > rtick1);

    for (int level = 0, subbeat_ticks = d_unit_ticks();;) {
        int n = rtick1 / subbeat_ticks;
        int m = (rtick2 - 1) / subbeat_ticks; // -1 to make the range exclusive
        if (m > n) {
            if (subbeat_tick) {
                (*subbeat_tick) = m * subbeat_ticks;
            }
            return level;
        }
        level++;
        if (subbeat_ticks % 2 != 0) {
            return -level; // further sub-division would split measure into chunks of unequal length.
        }
        subbeat_ticks /= 2;
    }
}

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool SigEvent::operator==(const SigEvent &e) const { return timesig_.identical(e.timesig_); }

//---------------------------------------------------------
//   add
//---------------------------------------------------------

const TimeSigFrac TimeSigMap::default_time_signature(4, 4);

void TimeSigMap::add(int tick, const Fraction &f) {
    if (!f.is_valid()) {
        LOGD("illegal signature %d/%d", f.numerator(), f.denominator());
    }
    (*this)[tick] = SigEvent(f);
    normalize();
}

void TimeSigMap::add(int tick, const SigEvent &ev) {
    (*this)[tick] = ev;
    normalize();
}

//---------------------------------------------------------
//   del
//---------------------------------------------------------

void TimeSigMap::del(int tick) {
    erase(tick);
    normalize();
}

//---------------------------------------------------------
//   clearRange
//    Clears the given range, start tick included, end tick
//    excluded.
//---------------------------------------------------------

void TimeSigMap::clear_range(int tick1, int tick2) {
    iterator first = lower_bound(tick1);
    iterator last = lower_bound(tick2);
    if (first == last) {
        return;
    }
    erase(first, last);
    normalize();
}

//---------------------------------------------------------
//   TimeSigMap::normalize
//---------------------------------------------------------

void TimeSigMap::normalize() {
    int z = 4;
    int n = 4;
    int tick = 0;
    TimeSigFrac bar;
    int tm = ticks_measure(TimeSigFrac(z, n));

    for (auto i = begin(); i != end(); ++i) {
        SigEvent &e = i->second;
        bar += TimeSigFrac(i->first - tick, tm).reduced();
        e.set_bar(bar.numerator() / bar.denominator());
        tick = i->first;
        tm = ticks_measure(e.timesig());
    }
}

//---------------------------------------------------------
//   timesig
//---------------------------------------------------------

const SigEvent &TimeSigMap::timesig(int tick) const {
    static const SigEvent ev(::mu::engraving::TimeSigMap::default_time_signature);
    if (empty()) {
        return ev;
    }
    auto i = upper_bound(tick);
    if (i != begin()) {
        --i;
    }
    return i->second;
}

//---------------------------------------------------------
//   tickValues
//    t - some time moment on timeline (in ticks)
//
//    Result - values computed for this time moment:
//    bar - index of bar containing time moment t
//    beat - index of beat in bar containing t
//    tick - position of t in beat (in ticks)
//---------------------------------------------------------

void TimeSigMap::tick_values(int t, int *bar, int *beat, int *tick) const {
    if (empty()) {
        *bar = 0;
        *beat = 0;
        *tick = 0;
        return;
    }
    auto e = upper_bound(t);
    IF_ASSERT_FAILED_X(e != begin(), String(u"tickValue(0x%1) not found").arg(t)) {
        *bar = 0;
        *beat = 0;
        *tick = 0;
        return;
    }
    --e;
    int delta = t - e->first;
    int ticks_b = ticks_beat(e->second.timesig().denominator()); // ticks in beat
    int ticks_m = ticks_b * e->second.timesig().numerator();     // ticks in measure (bar)
    if (ticks_m == 0) {
        LOGD("TimeSigMap::tickValues: at %d %s", t, muPrintable(e->second.timesig().to_string()));
        *bar = 0;
        *beat = 0;
        *tick = 0;
        return;
    }
    *bar = e->second.bar() + delta / ticks_m;
    int rest = delta % ticks_m;
    *beat = rest / ticks_b;
    *tick = rest % ticks_b;
}

//---------------------------------------------------------
//   pos
//    Return string representation of tick position.
//    This is not reentrant and only for debugging!
//---------------------------------------------------------

String TimeSigMap::pos(int t) const {
    int bar, beat, tick;
    tick_values(t, &bar, &beat, &tick);
    return String(u"%1:%2:%3").arg(bar + 1, beat, tick);
}

//---------------------------------------------------------
//   bar2tick
//    Returns the absolute start time (in ticks)
//    of beat in bar
//---------------------------------------------------------

int TimeSigMap::bar2tick(int bar, int beat) const {
    // bar - index of current bar (terminology: bar == measure)
    // beat - index of beat in current bar
    auto e = begin();

    for (; e != end(); ++e) {
        if (bar < e->second.bar()) {
            break;
        }
    }
    if (empty() || e == begin()) {
        LOGD("TimeSigMap::bar2tick(): not found(%d,%d) not found", bar, beat);
        if (empty()) {
            LOGD("   list is empty");
        }
        return 0;
    }
    --e;                                                         // current TimeSigMap value
    int ticks_b = ticks_beat(e->second.timesig().denominator()); // ticks per beat
    int ticks_m = ticks_b * e->second.timesig().numerator();     // bar length in ticks
    return e->first + (bar - e->second.bar()) * ticks_m + ticks_b * beat;
}

//---------------------------------------------------------
//   ticksPerMeasure
//---------------------------------------------------------

int ticks_per_measure(int numerator, int denominator) { return ticks_beat(denominator) * numerator; }

//---------------------------------------------------------
//   rasterEval
//---------------------------------------------------------

unsigned raster_eval(unsigned t, int raster, int start_tick, int numerator, int denominator, int addition) {
    int delta = t - start_tick;
    int ticks_m = ticks_per_measure(numerator, denominator);
    if (raster == 0) {
        raster = ticks_m;
    }
    int rest = delta % ticks_m;
    int bb = (delta / ticks_m) * ticks_m;
    return start_tick + bb + ((rest + addition) / raster) * raster;
}

//---------------------------------------------------------
//   raster
//---------------------------------------------------------

unsigned TimeSigMap::raster(unsigned t, int raster) const {
    if (raster == 1) {
        return t;
    }
    auto e = upper_bound(t);
    if (e == end()) {
        LOGD("TimeSigMap::raster(%x,)", t);
        return t;
    }
    auto timesig = e->second.timesig();
    return raster_eval(t, raster, e->first, timesig.numerator(), timesig.denominator(), raster / 2);
}

//---------------------------------------------------------
//   raster1
//    round down
//---------------------------------------------------------

unsigned TimeSigMap::raster1(unsigned t, int raster) const {
    if (raster == 1) {
        return t;
    }
    auto e = upper_bound(t);
    auto timesig = e->second.timesig();
    return raster_eval(t, raster, e->first, timesig.numerator(), timesig.denominator(), 0);
}

//---------------------------------------------------------
//   raster2
//    round up
//---------------------------------------------------------

unsigned TimeSigMap::raster2(unsigned t, int raster) const {
    if (raster == 1) {
        return t;
    }
    auto e = upper_bound(t);
    auto timesig = e->second.timesig();
    return raster_eval(t, raster, e->first, timesig.numerator(), timesig.denominator(), raster - 1);
}

//---------------------------------------------------------
//   rasterStep
//---------------------------------------------------------

int TimeSigMap::raster_step(unsigned t, int raster) const {
    if (raster == 0) {
        auto timesig = upper_bound(t)->second.timesig();
        return ticks_per_measure(timesig.denominator(), timesig.numerator());
    }
    return raster;
}

//---------------------------------------------------------
//   TimeSigMap::dump
//---------------------------------------------------------

void TimeSigMap::dump() const {
    LOGD("TimeSigMap:");
    for (auto i = begin(); i != end(); ++i) {
        LOGD("%6d timesig: %s measure: %d", i->first, muPrintable(i->second.timesig().to_string()), i->second.bar());
    }
}

//---------------------------------------------------------
//   dUnitTicks
//---------------------------------------------------------

int TimeSigFrac::d_unit_ticks() const { return (4 * Constants::division) / denominator(); }
} // namespace mu::engraving
