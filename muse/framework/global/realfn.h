/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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
#ifndef MUSE_GLOBAL_REALFN_H
#define MUSE_GLOBAL_REALFN_H

#include <cmath>
#include <algorithm>
#include <vector>

namespace muse {
// default
constexpr double default_compare_double_epsilon(1000000000.0);
constexpr double default_compare_double_null(0.000000001);
constexpr float default_compare_float_epsilon(1000000.0);
constexpr float default_compare_float_null(0.000001F);

// use
inline double compare_double_epsilon_value(::muse::default_compare_double_epsilon);
inline double compare_double_null_value(::muse::default_compare_double_null);
inline double compare_float_epsilon_value(::muse::default_compare_float_epsilon);
inline double compare_float_null_value(::muse::default_compare_float_null);

inline constexpr int pow10(int power) {
    int result = 1;
    for (int i = 0; i < power; ++i) {
        result *= 10;
    }
    return result;
}

inline void set_compare_double_precision(int prec) {
    compare_double_epsilon_value = pow10(prec);
    compare_double_null_value = 1.0 / compare_double_epsilon_value;
}

inline void set_default_compare_double_precision() {
    compare_double_epsilon_value = ::muse::default_compare_double_epsilon;
    compare_double_null_value = ::muse::default_compare_double_null;
}

inline void set_compare_float_precision(int prec) {
    compare_float_epsilon_value = pow10(prec);
    compare_float_null_value = 1.0 / compare_float_epsilon_value;
}

inline void set_default_compare_float_precision() {
    compare_float_epsilon_value = ::muse::default_compare_float_epsilon;
    compare_float_null_value = ::muse::default_compare_float_null;
}

// both double and float
inline void set_compare_real_precision(int prec) {
    set_compare_double_precision(prec);
    set_compare_float_precision(prec);
}

inline void set_default_compare_real_precision() {
    set_default_compare_double_precision();
    set_default_compare_float_precision();
}

inline bool real_is_equal(double p1, double p2) {
    return std::abs(p1 - p2) * compare_double_epsilon_value <= std::min(std::abs(p1), std::abs(p2));
}

inline bool real_is_equal(float p1, float p2) {
    return std::fabs(p1 - p2) * compare_float_epsilon_value <= std::min(std::fabs(p1), std::fabs(p2));
}

inline bool real_is_equal(const std::vector<double> &v1, const std::vector<double> &v2) {
    if (v1.size() != v2.size()) {
        return false;
    }

    for (size_t i = 0; i < v1.size(); ++i) {
        if (!real_is_equal(v1.at(i), v2.at(i))) {
            return false;
        }
    }

    return true;
}

inline bool real_is_equal(const std::vector<float> &v1, const std::vector<float> &v2) {
    if (v1.size() != v2.size()) {
        return false;
    }

    for (size_t i = 0; i < v1.size(); ++i) {
        if (!real_is_equal(v1.at(i), v2.at(i))) {
            return false;
        }
    }

    return true;
}

inline bool real_is_equal_or_more(double p1, double p2) { return p1 > p2 || real_is_equal(p1, p2); }

inline bool real_is_equal_or_less(double p1, double p2) { return p1 < p2 || real_is_equal(p1, p2); }

inline bool real_is_equal_or_more(float p1, float p2) { return p1 > p2 || real_is_equal(p1, p2); }

inline bool real_is_equal_or_less(float p1, float p2) { return p1 < p2 || real_is_equal(p1, p2); }

inline bool real_is_null(double d) { return std::abs(d) <= compare_double_null_value; }

inline bool real_is_null(float d) { return std::fabs(d) <= compare_float_null_value; }

inline double real_round(double value, int prec) {
    int round = pow10(prec);
    return std::floor(value * round + 0.5) / round;
}

inline double real_round(float value, int prec) { return real_round(static_cast<double>(value), prec); }

inline double real_floor(double value, int prec) {
    int round = pow10(prec);
    return std::floor(value * round) / round;
}

inline double real_floor(float value, int prec) { return real_round(static_cast<double>(value), prec); }
} // namespace muse

#endif // MUSE_GLOBAL_REALFN_H
