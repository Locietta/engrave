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
#include "importmidi_fraction.h"

#include <cassert>
#include <limits>

#include "engraving/dom/mscore.h"
#ifndef Q_ASSERT
#define Q_ASSERT(condition) assert(condition)
#endif
#ifndef Q_ASSERT_X
#define Q_ASSERT_X(condition, where, what) assert(condition)
#endif

namespace mu::iex::midi {
#ifndef NDEBUG

//---------------------------------------------------------------------------------------
// https://www.securecoding.cert.org/confluence/display/seccode/
// INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow?showComments=false
//
// Access date: 2013.11.28

bool is_addition_overflow(int a, int b) // a + b
{
    return (b > 0 && a > (std::numeric_limits<int>::max() - b)) || (b < 0 && a < std::numeric_limits<int>::min() - b);
}

bool is_subtraction_overflow(int a, int b) // a - b
{
    return (b > 0 && a < std::numeric_limits<int>::min() + b) || (b < 0 && a > std::numeric_limits<int>::max() + b);
}

bool is_multiplication_overflow(int a, int b) // a * b
{
    if (a > 0) {
        if (b > 0) {
            if (a > std::numeric_limits<int>::max() / b) {
                return true;
            }
        } else {
            if (b < std::numeric_limits<int>::min() / a) {
                return true;
            }
        }
    } else {
        if (b > 0) {
            if (a < std::numeric_limits<int>::min() / b) {
                return true;
            }
        } else {
            if (a != 0 && b < std::numeric_limits<int>::max() / a) {
                return true;
            }
        }
    }

    return false;
}

bool is_division_overflow(int a, int b) // a / b
{
    return (b == 0) || ((a == std::numeric_limits<int>::min()) && (b == -1));
}

bool is_remainder_overflow(int a, int b) // a % b
{
    return (b == 0) || ((a == std::numeric_limits<int>::min()) && (b == -1));
}

bool is_unary_negation_overflow(int a) // -a
{
    return a == std::numeric_limits<int>::min();
}

#endif

namespace {
//---------------------------------------------------------
//   lcm
//    least common multiple. always returns a positive val
//---------------------------------------------------------

static unsigned lcm(int a, int b) {
    const int g = std::gcd(a, b);

#ifndef NDEBUG
    Q_ASSERT_X(!is_division_overflow(a, g), "ReducedFraction, lcm", "Division overflow");
    Q_ASSERT_X(!is_multiplication_overflow(a / g, b), "ReducedFraction, lcm", "Multiplication overflow");
#endif

    const int l = (a / g) * b; // Divide first to minimize overflow risk
    return l >= 0 ? l : -l;
}
} // namespace

//-----------------------------------------------------------------------------

ReducedFraction::ReducedFraction() : numerator_(0), denominator_(1) {}

ReducedFraction::ReducedFraction(int z, int n) : numerator_(z), denominator_(n) {}

ReducedFraction::ReducedFraction(const engraving::Fraction &fraction)
    : numerator_(fraction.numerator()), denominator_(fraction.denominator()) {}

ReducedFraction ReducedFraction::from_ticks(int ticks) { return ReducedFraction(ticks, engraving::Constants::division * 4).reduced(); }

ReducedFraction ReducedFraction::reduced() const {
    const int tmp = std::gcd(numerator_, denominator_);

#ifndef NDEBUG
    Q_ASSERT_X(!is_division_overflow(numerator_, tmp), "ReducedFraction::reduced", "Division overflow");
    Q_ASSERT_X(!is_division_overflow(denominator_, tmp), "ReducedFraction::reduced", "Division overflow");
#endif

    return ReducedFraction(numerator_ / tmp, denominator_ / tmp);
}

int ReducedFraction::ticks() const {
    int integral = numerator_ / denominator_;
    int new_numerator = numerator_ % denominator_;
    int division = engraving::Constants::division * 4;

#ifndef NDEBUG
    Q_ASSERT_X(!is_multiplication_overflow(new_numerator, division), "ReducedFraction::ticks", "Multiplication overflow");
    Q_ASSERT_X(!is_addition_overflow(new_numerator * division, denominator_ / 2), "ReducedFraction::ticks", "Addition overflow");
#endif

    const int tmp = new_numerator * division + denominator_ / 2;

#ifndef NDEBUG
    Q_ASSERT_X(!is_division_overflow(tmp, denominator_), "ReducedFraction::ticks", "Division overflow");
    Q_ASSERT_X(!is_multiplication_overflow(integral, division), "ReducedFraction::ticks", "Multiplication overflow");
    Q_ASSERT_X(!is_addition_overflow(tmp / denominator_, integral * division), "ReducedFraction::ticks", "Addition overflow");
#endif

    return tmp / denominator_ + integral * division;
}

void ReducedFraction::reduce() {
    if (numerator_ == 0) {
        denominator_ = 1;
        return;
    }
    const int tmp = std::gcd(numerator_, denominator_);

#ifndef NDEBUG
    Q_ASSERT_X(!is_division_overflow(numerator_, tmp), "ReducedFraction::reduce", "Division overflow");
    Q_ASSERT_X(!is_division_overflow(denominator_, tmp), "ReducedFraction::reduce", "Division overflow");
#endif

    numerator_ /= tmp;
    denominator_ /= tmp;
}

void ReducedFraction::prevent_overflow() {
    static const int reduce_limit = 10000;
    if (numerator_ >= reduce_limit || denominator_ >= reduce_limit) {
        reduce();
    }
}

// helper function

int fraction_part(int lcm_part, int numerator, int denominator) {
#ifndef NDEBUG
    Q_ASSERT_X(!is_division_overflow(lcm_part, denominator), "ReducedFraction::fractionPart", "Division overflow");
#endif
    const int part = lcm_part / denominator;

#ifndef NDEBUG
    Q_ASSERT_X(!is_multiplication_overflow(numerator, part), "ReducedFraction::fractionPart", "Multiplication overflow");
#endif
    return numerator * part;
}

ReducedFraction &ReducedFraction::operator+=(const ReducedFraction &val) {
    prevent_overflow();
    ReducedFraction value = val;
    value.prevent_overflow();

    const int tmp = lcm(denominator_, val.denominator_);
    numerator_ = fraction_part(tmp, numerator_, denominator_) + fraction_part(tmp, val.numerator_, val.denominator_);
    denominator_ = tmp;
    return *this;
}

ReducedFraction &ReducedFraction::operator-=(const ReducedFraction &val) {
    prevent_overflow();
    ReducedFraction value = val;
    value.prevent_overflow();

    const int tmp = lcm(denominator_, val.denominator_);
    numerator_ = fraction_part(tmp, numerator_, denominator_) - fraction_part(tmp, val.numerator_, val.denominator_);
    denominator_ = tmp;
    return *this;
}

ReducedFraction &ReducedFraction::operator*=(const ReducedFraction &val) {
    prevent_overflow();
    ReducedFraction value = val;
    value.prevent_overflow();

#ifndef NDEBUG
    Q_ASSERT_X(!is_multiplication_overflow(numerator_, val.numerator_), "ReducedFraction::operator*=", "Multiplication overflow");
    Q_ASSERT_X(!is_multiplication_overflow(denominator_, val.denominator_), "ReducedFraction::operator*=", "Multiplication overflow");
#endif
    numerator_ *= val.numerator_;
    denominator_ *= val.denominator_;
    return *this;
}

ReducedFraction &ReducedFraction::operator*=(int val) {
    prevent_overflow();
#ifndef NDEBUG
    Q_ASSERT_X(!is_multiplication_overflow(numerator_, val), "ReducedFraction::operator*=", "Multiplication overflow");
#endif
    numerator_ *= val;
    return *this;
}

ReducedFraction &ReducedFraction::operator/=(const ReducedFraction &val) {
    prevent_overflow();
    ReducedFraction value = val;
    value.prevent_overflow();
#ifndef NDEBUG
    Q_ASSERT_X(!is_multiplication_overflow(numerator_, val.denominator_), "ReducedFraction::operator/=", "Multiplication overflow");
    Q_ASSERT_X(!is_multiplication_overflow(denominator_, val.numerator_), "ReducedFraction::operator/=", "Multiplication overflow");
#endif
    numerator_ *= val.denominator_;
    denominator_ *= val.numerator_;
    return *this;
}

ReducedFraction &ReducedFraction::operator/=(int val) {
    prevent_overflow();
#ifndef NDEBUG
    Q_ASSERT_X(!is_multiplication_overflow(denominator_, val), "ReducedFraction::operator/=", "Multiplication overflow");
#endif
    denominator_ *= val;
    return *this;
}

bool ReducedFraction::operator<(const ReducedFraction &val) const {
    const int v = lcm(denominator_, val.denominator_);
    return fraction_part(v, numerator_, denominator_) < fraction_part(v, val.numerator_, val.denominator_);
}

bool ReducedFraction::operator<=(const ReducedFraction &val) const {
    const int v = lcm(denominator_, val.denominator_);
    return fraction_part(v, numerator_, denominator_) <= fraction_part(v, val.numerator_, val.denominator_);
}

bool ReducedFraction::operator>(const ReducedFraction &val) const {
    const int v = lcm(denominator_, val.denominator_);
    return fraction_part(v, numerator_, denominator_) > fraction_part(v, val.numerator_, val.denominator_);
}

bool ReducedFraction::operator>=(const ReducedFraction &val) const {
    const int v = lcm(denominator_, val.denominator_);
    return fraction_part(v, numerator_, denominator_) >= fraction_part(v, val.numerator_, val.denominator_);
}

bool ReducedFraction::operator==(const ReducedFraction &val) const {
    const int v = lcm(denominator_, val.denominator_);
    return fraction_part(v, numerator_, denominator_) == fraction_part(v, val.numerator_, val.denominator_);
}

bool ReducedFraction::operator!=(const ReducedFraction &val) const {
    const int v = lcm(denominator_, val.denominator_);
    return fraction_part(v, numerator_, denominator_) != fraction_part(v, val.numerator_, val.denominator_);
}

//-------------------------------------------------------------------------

ReducedFraction to_muse_score_ticks(int tick, int old_division, bool is_division_in_tps) {
    if (is_division_in_tps) {
        return ReducedFraction::from_ticks(tick);
    }
#ifndef NDEBUG
    Q_ASSERT_X(!is_division_overflow(tick, old_division), "ReducedFraction::toMuseScoreTicks", "Division overflow");
    Q_ASSERT_X(!is_remainder_overflow(tick, old_division), "ReducedFraction::toMuseScoreTicks", "Remainder overflow");
#endif
    const int integral = tick / old_division;
    const int remainder = tick % old_division;
#ifndef NDEBUG
    Q_ASSERT_X(!is_multiplication_overflow(remainder, engraving::Constants::division), "ReducedFraction::toMuseScoreTicks",
               "Multiplication overflow");
    Q_ASSERT_X(!is_addition_overflow(remainder * engraving::Constants::division, old_division / 2), "ReducedFraction::toMuseScoreTicks",
               "Addition overflow");
#endif
    const int tmp = remainder * engraving::Constants::division + old_division / 2;
#ifndef NDEBUG
    Q_ASSERT_X(!is_division_overflow(tmp, old_division), "ReducedFraction::toMuseScoreTicks", "Division overflow");
    Q_ASSERT_X(!is_multiplication_overflow(integral, engraving::Constants::division), "ReducedFraction::toMuseScoreTicks",
               "Multiplication overflow");
    Q_ASSERT_X(!is_addition_overflow(tmp / old_division, integral * engraving::Constants::division), "ReducedFraction::toMuseScoreTicks",
               "Addition overflow");
#endif
    return ReducedFraction::from_ticks(tmp / old_division + integral * engraving::Constants::division);
}
} // namespace mu::iex::midi
