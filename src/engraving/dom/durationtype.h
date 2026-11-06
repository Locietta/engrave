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

#ifndef MU_ENGRAVING_DURATIONTYPE_H
#define MU_ENGRAVING_DURATIONTYPE_H

#include "../types/types.h"

namespace mu::engraving {
class Measure;
class TimeSigFrac;
enum class BeatType : unsigned char;

//---------------------------------------------------------
//   TDuration
//---------------------------------------------------------

class TDuration {
public:
    TDuration() : val_(DurationType::V_INVALID), dots_(0) {}

    TDuration(const Fraction &l, bool truncate = false, int max_dots = 4, DurationType max_type = DurationType::V_LONG);

    TDuration(DurationType t) : val_(t), dots_(0) {}

    TDuration(DurationTypeWithDots t) : val_(t.type), dots_(t.dots) {}

    DurationType type() const { return val_; }

    DurationTypeWithDots type_with_dots() const { return DurationTypeWithDots(val_, dots_); }

    bool is_valid() const { return val_ != DurationType::V_INVALID; }

    bool is_zero() const { return val_ == DurationType::V_ZERO; }

    bool is_measure() const { return val_ == DurationType::V_MEASURE; }

    void set_val(int tick);
    void set_type(DurationType t);

    Fraction ticks() const;

    bool operator==(const TDuration &t) const { return t.val_ == val_ && t.dots_ == dots_; }

    bool operator==(const DurationType &t) const { return t == val_; }

    bool operator!=(const TDuration &t) const { return t.val_ != val_ || t.dots_ != dots_; }

    bool operator<(const TDuration &t) const;
    bool operator>(const TDuration &t) const;
    bool operator>=(const TDuration &t) const;
    bool operator<=(const TDuration &t) const;
    TDuration &operator-=(const TDuration &t);

    TDuration operator-(const TDuration &t) const { return TDuration(*this) -= t; }

    TDuration &operator+=(const TDuration &t);

    TDuration operator+(const TDuration &t) const { return TDuration(*this) += t; }

    NoteHeadType head_type() const;
    int hooks() const;
    bool has_stem() const;

    TDuration shift(int n_steps) const {
        TDuration d(type());
        d.shift_type(n_steps);
        return d;
    } // dots are not retained

    TDuration shift_retain_dots(int n_steps, bool step_dotted = false) const {
        TDuration d(type());
        d.set_dots(dots_);
        d.shift_type(n_steps, step_dotted);
        return d;
    }

    int dots() const { return dots_; }

    void set_dots(int v);
    Fraction fraction() const;
    static bool is_valid(Fraction f);

private:
    void shift_type(int n_steps, bool step_dotted = false);
    void truncate_to_fraction(const Fraction &l, int max_dots);
    bool set_dots_to_fit_fraction(const Fraction &l, int max_dots);

    DurationType val_ = DurationType::V_INVALID;
    int dots_ = 0;
};

std::vector<TDuration> to_duration_list(Fraction l, bool use_dots, int max_dots = 4, bool print_rest_remains = true);
std::vector<TDuration> to_rhythmic_duration_list(const Fraction &l, bool is_rest, Fraction rtick_start, const TimeSigFrac &nominal,
                                                 Measure *msr, int max_dots, const Fraction &time_stretch = Fraction(1, 1));

bool force_rhythmic_split(bool is_rest, BeatType start_beat, BeatType end_beat, int beats_crossed, BeatType strongest_beat_crossed,
                          const TimeSigFrac &nominal);
bool force_rhythmic_split_simple(bool is_rest, BeatType start_beat, BeatType end_beat, int beats_crossed, BeatType strongest_beat_crossed);
bool force_rhythmic_split_compound(bool is_rest, BeatType start_beat, BeatType end_beat, int beats_crossed,
                                   BeatType strongest_beat_crossed);

void populate_rhythmic_list(std::vector<TDuration> *d_list, const Fraction &l, bool is_rest, const Fraction &rtick_start,
                            const TimeSigFrac &nominal, int max_dots);
void split_compound_beats_for_list(std::vector<TDuration> *d_list, const Fraction &l, bool is_rest, const Fraction &rtick_start,
                                   const TimeSigFrac &nominal, int max_dots);
} // namespace mu::engraving

#endif
