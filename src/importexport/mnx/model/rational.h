// SPDX-License-Identifier: GPL-3.0-only
// Exact rational numbers for MNX durations and rhythmic positions.
#pragma once

#include <compare>
#include <cstdint>
#include <numeric>

#include "mnx_model.h"

namespace mu::iex::mnxio::model {

// A reduced fraction with a positive denominator. MNX values are small, so 64-bit terms do
// not overflow for any document the index accepts.
struct Rational {
    std::int64_t numerator = 0;
    std::int64_t denominator = 1;

    constexpr Rational() = default;

    // `den` must not be zero.
    constexpr Rational(std::int64_t num, std::int64_t den = 1) : numerator(num), denominator(den) {
        if (denominator < 0) {
            numerator = -numerator;
            denominator = -denominator;
        }
        const std::int64_t divisor = std::gcd(numerator, denominator);
        if (divisor > 1) {
            numerator /= divisor;
            denominator /= divisor;
        }
    }

    // An MNX fraction; its denominator must not be zero (build_index checks).
    constexpr explicit Rational(const Fraction &value) : Rational(value.numerator, value.denominator) {}

    constexpr Rational operator+(const Rational &rhs) const {
        return {numerator * rhs.denominator + rhs.numerator * denominator, denominator * rhs.denominator};
    }

    constexpr Rational operator-(const Rational &rhs) const {
        return {numerator * rhs.denominator - rhs.numerator * denominator, denominator * rhs.denominator};
    }

    constexpr Rational operator*(const Rational &rhs) const { return {numerator * rhs.numerator, denominator * rhs.denominator}; }

    // `rhs` must not be zero.
    constexpr Rational operator/(const Rational &rhs) const { return {numerator * rhs.denominator, denominator * rhs.numerator}; }

    constexpr Rational &operator+=(const Rational &rhs) { return *this = *this + rhs; }

    constexpr Rational &operator-=(const Rational &rhs) { return *this = *this - rhs; }

    constexpr bool operator==(const Rational &) const = default;

    constexpr std::strong_ordering operator<=>(const Rational &rhs) const {
        return numerator * rhs.denominator <=> rhs.numerator * denominator;
    }

    constexpr double to_double() const { return double(numerator) / double(denominator); }
};

} // namespace mu::iex::mnxio::model
