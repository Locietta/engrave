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

#pragma once

#include <map>
#include <cassert>

#include "global/allocator.h"
#include "../types/types.h"

namespace mu::engraving {
int ticks_beat(int n);

//-------------------------------------------------------------------
//   BeatType
//-------------------------------------------------------------------

enum class BeatType : unsigned char {
    DOWNBEAT,            // 1st beat of measure (rtick == 0)
    COMPOUND_STRESSED,   // e.g. eighth-note number 7 in 12/8
    SIMPLE_STRESSED,     // e.g. beat 3 in 4/4
    COMPOUND_UNSTRESSED, // e.g. eighth-note numbers 4 or 10 in 12/8
    SIMPLE_UNSTRESSED,   // "offbeat" e.g. beat 2 and 4 in 4/4 (i.e. the denominator unit)
    COMPOUND_SUBBEAT,    // e.g. any other eighth-note in 12/8 (i.e. the denominator unit)
    SUBBEAT              // does not fall on a beat
};

//-------------------------------------------------------------------
//   Time Signature Fraction   (n/d - numerator/denominator)
//-------------------------------------------------------------------

struct TimeSigFrac : Fraction {
private:
    OBJECT_ALLOCATOR(engraving, TimeSigFrac)
public:
    using Fraction::Fraction;

    constexpr TimeSigFrac(int n = 0, int d = 1) : Fraction(n, d) {}

    TimeSigFrac(const Fraction &f) : TimeSigFrac(f.numerator(), f.denominator()) {}

    // isCompound? Note: 3/8, 3/16, ... are NOT considered compound.
    bool is_compound() const { return numerator() > 3 /*&& denominator() >= 8*/ && numerator() % 3 == 0; }

    // isBeatedCompound? Note: Conductors will beat the simple unit at slow tempos (<60 compound units per minute)
    // However, the meter is still considered to be compound (at least for our purposes).
    bool is_beated_compound(double tempo) const { return tempo2beats_per_minute(tempo) >= 60.0; }

    int d_unit_ticks() const;

    int ticks_per_measure() const { return numerator() * d_unit_ticks(); }

    int d_units_per_beat() const { return is_compound() ? 3 : 1; }

    int beat_ticks() const { return d_unit_ticks() * d_units_per_beat(); }

    int beats_per_measure() const { return numerator() / d_units_per_beat(); }

    int subbeat_ticks(int level) const;
    int max_subbeat_level() const;

    bool is_triple() const { return beats_per_measure() % 3 == 0; }

    bool is_duple() const {
        assert(!is_triple());
        return beats_per_measure() % 2 == 0;
    } // note: always test isTriple() first

    // MuseScore stores tempos in quarter-notes-per-second, so conversions to conventional beats-per-minute format are provided here:
    double tempo2beats_per_minute(double tempo) const { return tempo * denominator() * 15.0 / d_units_per_beat(); }

    double beats_per_minute2tempo(double bpm) const { return bpm * d_units_per_beat() / (15.0 * denominator()); }

    BeatType rtick2beat_type(int rtick) const;
    int rtick2subbeat_level(int rtick) const; // returns negative value if not on a well-defined subbeat

    BeatType strongest_beat_in_range(int rtick1, int rtick2, int *d_units_crossed = 0, int *subbeat_tick = 0,
                                     bool save_last = false) const;                            // range is exclusive
    int strongest_subbeat_level_in_range(int rtick1, int rtick2, int *subbeat_tick = 0) const; // range is exclusive

    int ticks_past_d_unit(int rtick) const { return rtick % d_unit_ticks(); } // returns 0 if rtick is exactly on a dUnit

    int ticks_to_next_d_unit(int rtick) const {
        return d_unit_ticks() - ticks_past_d_unit(rtick);
    } // returns dUnitTicks() if rtick is on a dUnit

    int ticks_past_beat(int rtick) const { return rtick % beat_ticks(); } // returns 0 if rtick is exactly on a beat

    int ticks_to_next_beat(int rtick) const { return beat_ticks() - ticks_past_beat(rtick); } // returns beatTicks() if rtick is on a beat

    int ticks_past_subbeat(int rtick, int level) const { return rtick % subbeat_ticks(level); }

    int ticks_to_next_subbeat(int rtick, int level) const { return subbeat_ticks(level) - ticks_past_subbeat(rtick, level); }
};

//-------------------------------------------------------------------
//   Time Signature Event
//    Incomplete measures as for example pickup measures have
//    a nominal duration different from actual duration.
//-------------------------------------------------------------------

struct SigEvent {
    constexpr SigEvent() : bar_(0) {} ///< default SigEvent is invalid

    SigEvent(const Fraction &s, int bar = 0) : timesig_(s), nominal_(s), bar_(bar) {}

    SigEvent(const Fraction &s, const Fraction &ss, int bar = 0) : timesig_(s), nominal_(ss), bar_(bar) {}

    bool operator==(const SigEvent &e) const;

    bool valid() const { return timesig_.is_valid(); }

    String print() const { return timesig_.to_string(); }

    TimeSigFrac timesig() const { return timesig_; }

    void set_timesig(const TimeSigFrac &f) { timesig_ = f; }

    TimeSigFrac nominal() const { return nominal_; }

    void set_nominal(const TimeSigFrac &f) { nominal_ = f; }

    int bar() const { return bar_; }

    void set_bar(int val) { bar_ = val; }

private:

    TimeSigFrac timesig_;
    TimeSigFrac nominal_;
    int bar_ = 0; ///< precomputed value
};

//---------------------------------------------------------
//   SigList
//---------------------------------------------------------

struct TimeSigMap : std::map<int, SigEvent> {
private:
    OBJECT_ALLOCATOR(engraving, TimeSigMap)

public:
    TimeSigMap() {}

    static const TimeSigFrac default_time_signature;

    void add(int tick, const Fraction &);
    void add(int tick, const SigEvent &ev);

    void del(int tick);

    void clear_range(int tick1, int tick2);

    void dump() const;

    const SigEvent &timesig(int tick) const;

    const SigEvent &timesig(const Fraction &f) const { return timesig(f.ticks()); }

    void tick_values(int t, int *bar, int *beat, int *tick) const;
    int bar2tick(int bar, int beat) const;
    String pos(int t) const;

    unsigned raster(unsigned tick, int raster) const;
    unsigned raster1(unsigned tick, int raster) const; // round down
    unsigned raster2(unsigned tick, int raster) const; // round up
    int raster_step(unsigned tick, int raster) const;

    void normalize();
};
} // namespace mu::engraving
