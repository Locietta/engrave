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
#ifndef IMPORTMIDI_FRACTION_H
#define IMPORTMIDI_FRACTION_H

#include <cstdlib>

#include "engraving/types/fraction.h"

namespace mu::iex::midi {
class ReducedFraction {
public:
    ReducedFraction();
    ReducedFraction(int z, int n);
    explicit ReducedFraction(const engraving::Fraction &);

    engraving::Fraction fraction() const { return engraving::Fraction(numerator_, denominator_); }

    int numerator() const { return numerator_; }

    int denominator() const { return denominator_; }

    static ReducedFraction from_ticks(int ticks);
    ReducedFraction reduced() const;

    ReducedFraction abs_value() const { return ReducedFraction(std::abs(numerator_), std::abs(denominator_)); }

    double to_double() const { return numerator_ * 1.0 / denominator_; }

    int ticks() const;
    void reduce();

    bool is_identical_to(const ReducedFraction &f) const { return f.numerator_ == numerator_ && f.denominator_ == denominator_; }

    ReducedFraction &operator+=(const ReducedFraction &);
    ReducedFraction &operator-=(const ReducedFraction &);
    ReducedFraction &operator*=(const ReducedFraction &);
    ReducedFraction &operator*=(int);
    ReducedFraction &operator/=(const ReducedFraction &);
    ReducedFraction &operator/=(int);

    ReducedFraction operator+(const ReducedFraction &v) const { return ReducedFraction(*this) += v; }

    ReducedFraction operator-(const ReducedFraction &v) const { return ReducedFraction(*this) -= v; }

    ReducedFraction operator*(const ReducedFraction &v) const { return ReducedFraction(*this) *= v; }

    ReducedFraction operator*(int v) const { return ReducedFraction(*this) *= v; }

    ReducedFraction operator/(const ReducedFraction &v) const { return ReducedFraction(*this) /= v; }

    ReducedFraction operator/(int v) const { return ReducedFraction(*this) /= v; }

    bool operator<(const ReducedFraction &) const;
    bool operator<=(const ReducedFraction &) const;
    bool operator>=(const ReducedFraction &) const;
    bool operator>(const ReducedFraction &) const;
    bool operator==(const ReducedFraction &) const;
    bool operator!=(const ReducedFraction &) const;

private:
    void prevent_overflow();

    int numerator_;
    int denominator_;
};

ReducedFraction to_muse_score_ticks(int tick, int old_division, bool is_division_in_tps);
} // namespace mu::iex::midi

#endif // IMPORTMIDI_FRACTION_H
