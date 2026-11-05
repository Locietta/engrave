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

#include <cstdint>
#include <cmath>
#include <limits>
#include <numeric>

#include "global/types/string.h"

#include "constants.h"

// everything contained in .h file for performance reasons

namespace mu::engraving {
class Fraction {
    // ensure 64 bit to avoid overflows in comparisons
    int64_t numerator_ = 0;
    int64_t denominator_ = 1;

public:
    // no implicit conversion from int to Fraction:
    constexpr Fraction() = default;

    constexpr Fraction(int z, int n) : numerator_{n < 0 ? -z : z}, denominator_{n < 0 ? -n : n} {}

    constexpr int numerator() const { return static_cast<int>(numerator_); }

    constexpr int denominator() const { return static_cast<int>(denominator_); }

    // Arithmetic can leave unreduced terms wider than the int inputs.
    constexpr int64_t numerator64() const { return numerator_; }

    constexpr int64_t denominator64() const { return denominator_; }

    /// Use this when you need to initialize a Fraction to an arbitrary high value
    static constexpr Fraction max() { return Fraction(std::numeric_limits<int>::max(), 1); }

    constexpr void setNumerator(int v) { numerator_ = v; }

    constexpr void setDenominator(int v) {
        if (v < 0) {
            numerator_ = -numerator_;
            denominator_ = -v;
        } else {
            denominator_ = v;
        }
    }

    constexpr void set(int z, int n) {
        if (n < 0) {
            numerator_ = -z;
            denominator_ = -n;
        } else {
            numerator_ = z;
            denominator_ = n;
        }
    }

    constexpr bool isZero() const { return numerator_ == 0; }

    constexpr bool isNotZero() const { return numerator_ != 0; }

    constexpr bool negative() const { return numerator_ < 0; }

    constexpr bool positive() const { return numerator_ >= 0; }

    constexpr bool isValid() const { return denominator_ != 0; }

    // check if two fractions are identical (numerator & denominator)
    // == operator checks for equal value:
    constexpr bool identical(const Fraction &v) const { return (numerator_ == v.numerator_) && (denominator_ == v.denominator_); }

    Fraction absValue() const { return Fraction(static_cast<int>(std::abs(numerator_)), static_cast<int>(denominator_)); }

    constexpr Fraction inverse() const { return Fraction(static_cast<int>(denominator_), static_cast<int>(numerator_)); }

    // reduction

    constexpr void reduce() {
        const int64_t g = std::gcd(numerator_, denominator_);
        if (g) {
            numerator_ /= g;
            denominator_ /= g;
        }
    }

    constexpr Fraction reduced() const {
        const int64_t g = std::gcd(numerator_, denominator_);
        if (g) {
            return Fraction(static_cast<int>(numerator_ / g), static_cast<int>(denominator_ / g));
        }
        return Fraction(numerator_, denominator_);
    }

    // comparison

    constexpr bool operator<(const Fraction &val) const { return numerator_ * val.denominator_ < val.numerator_ * denominator_; }

    constexpr bool operator<=(const Fraction &val) const { return numerator_ * val.denominator_ <= val.numerator_ * denominator_; }

    constexpr bool operator>=(const Fraction &val) const { return numerator_ * val.denominator_ >= val.numerator_ * denominator_; }

    constexpr bool operator>(const Fraction &val) const { return numerator_ * val.denominator_ > val.numerator_ * denominator_; }

    constexpr bool operator==(const Fraction &val) const { return numerator_ * val.denominator_ == val.numerator_ * denominator_; }

    constexpr bool operator!=(const Fraction &val) const { return numerator_ * val.denominator_ != val.numerator_ * denominator_; }

    // arithmetic

    constexpr Fraction &operator+=(const Fraction &val) {
        if (denominator_ == val.denominator_) {
            // Common enough use case to be handled separately for efficiency
            numerator_ += val.numerator_;
        } else {
            const int64_t g = std::gcd(denominator_, val.denominator_);
            if (g) {
                const int64_t m1 = val.denominator_ / g; // This saves one division over straight lcm
                numerator_ = numerator_ * m1 + val.numerator_ * (denominator_ / g);
                denominator_ = m1 * denominator_;
            }
        }
        return *this;
    }

    constexpr Fraction &operator-=(const Fraction &val) {
        if (denominator_ == val.denominator_) {
            // Common enough use case to be handled separately for efficiency
            numerator_ -= val.numerator_;
        } else {
            const int64_t g = std::gcd(denominator_, val.denominator_);
            if (g) {
                const int64_t m1 = val.denominator_ / g; // This saves one division over straight lcm
                numerator_ = numerator_ * m1 - val.numerator_ * (denominator_ / g);
                denominator_ = m1 * denominator_;
            }
        }
        return *this;
    }

    constexpr Fraction &operator*=(const Fraction &val) {
        numerator_ *= val.numerator_;
        denominator_ *= val.denominator_;
        if (val.denominator_ > 1 || val.denominator_ < -1) {
            // We should be free to fully reduce here
            reduce();
        }
        return *this;
    }

    constexpr Fraction &operator*=(int val) {
        numerator_ *= val;
        return *this;
    }

    constexpr Fraction &operator/=(const Fraction &val) {
        const int sign = (val.numerator_ >= 0 ? 1 : -1);
        numerator_ *= (sign * val.denominator_);
        denominator_ *= (sign * val.numerator_);
        if (val.numerator_ != sign) {
            reduce();
        }
        return *this;
    }

    constexpr Fraction &operator/=(int val) {
        denominator_ *= val;
        if (denominator_ < 0) {
            denominator_ = -denominator_;
            numerator_ = -numerator_;
        }
        reduce();
        return *this;
    }

    constexpr Fraction operator+(const Fraction &v) const { return Fraction(*this) += v; }

    constexpr Fraction operator-(const Fraction &v) const { return Fraction(*this) -= v; }

    constexpr Fraction operator-() const { return Fraction(static_cast<int>(-numerator_), static_cast<int>(denominator_)); }

    constexpr Fraction operator*(const Fraction &v) const { return Fraction(*this) *= v; }

    constexpr Fraction operator/(const Fraction &v) const { return Fraction(*this) /= v; }

    constexpr Fraction operator/(int v) const { return Fraction(*this) /= v; }

    // conversion
    constexpr int ticks() const {
        if (numerator_ == -1 && denominator_ == 1) { // HACK
            return -1;
        }

        // Constants::division     - ticks per quarter note
        // Constants::division * 4 - ticks per whole note
        // result: rounded (Constants::division * 4 * numerator_ * 1.0 / denominator_) value
        const int sgn = (numerator_ < 0) ? -1 : 1;
        const auto result =
            sgn * (static_cast<int_least64_t>(sgn * numerator_) * Constants::DIVISION * 4 + (denominator_ / 2)) / denominator_;
        return static_cast<int>(result);
    }

    static constexpr Fraction fromTicks(int ticks) {
        if (ticks == -1) {
            return Fraction(-1, 1); // HACK
        }
        return Fraction(ticks, Constants::DIVISION * 4).reduced();
    }

    // A very small fraction, corresponds to 1 MIDI tick
    static constexpr Fraction eps() { return Fraction(1, Constants::DIVISION * 4); }

    muse::String toString() const { return muse::String(u"%1/%2").arg(numerator_, denominator_); }

    static Fraction fromString(const muse::String &str, bool *ok = nullptr) {
        const size_t i = str.indexOf(u'/');
        if (i == muse::nidx) {
            return Fraction(str.toInt(ok), 1);
        }

        int numerator = str.left(i).toInt(ok);
        if (ok && !*ok) {
            return Fraction();
        }

        int denominator = str.mid(i + 1).toInt(ok);
        if (ok && !*ok) {
            return Fraction();
        }

        return Fraction(numerator, denominator);
    }

    constexpr double toDouble() const { return static_cast<double>(numerator_) / static_cast<double>(denominator_); }
};

constexpr Fraction operator*(const Fraction &f, int v) { return Fraction(f) *= v; }

constexpr Fraction operator*(int v, const Fraction &f) { return Fraction(f) *= v; }
} // namespace mu::engraving
