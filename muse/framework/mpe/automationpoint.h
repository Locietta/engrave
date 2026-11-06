/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include <algorithm>
#include <variant>
#include <optional>
#include <type_traits>

#include "types/sharedmap.h"
#include "types/number.h"

#include "containers.h"
#include "realfn.h"

namespace muse::mpe {
struct AutomationPoint {
    //! NOTE: eases the segment through point (t, value)
    struct Ease {
        real_t t = real_t::make(0.5);     // [0; 1]
        real_t value = real_t::make(0.5); // [0; 1]

        static constexpr Ease none() { return {}; }

        bool is_none() const { return *this == none(); }

        bool operator==(const Ease &e) const { return t == e.t && value == e.value; }
    };

    //! NOTE: arrival value equals whatever precedes this point in the curve
    struct ArrivalFromPrevious {
        bool operator==(const ArrivalFromPrevious &) const { return true; }
    };

    //! NOTE: arrival value is an explicit number
    struct ExplicitArrival {
        real_t value = 0.;
        Ease ease;

        bool operator==(const ExplicitArrival &o) const { return value == o.value && ease == o.ease; }
    };

    using InValue = std::variant<ArrivalFromPrevious, ExplicitArrival>;

    InValue in_value = ArrivalFromPrevious{};
    real_t out_value = 0.;

    bool operator==(const AutomationPoint &p) const { return in_value == p.in_value && out_value == p.out_value; }
};

template <typename KeyT>
using AutomationCurve = SharedMap<KeyT, AutomationPoint>;

//! NOTE: this point's ease, if it can have one; nullopt for ArrivalFromPrevious
inline std::optional<AutomationPoint::Ease> ease(const AutomationPoint &point) noexcept {
    return std::visit(
        [](const auto &v) -> std::optional<AutomationPoint::Ease> {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, AutomationPoint::ArrivalFromPrevious>) {
                return std::nullopt;
            } else {
                return v.ease;
            }
        },
        point.in_value);
}

//! NOTE: resolves what this point's arrival value actually is, given prevOutValue
//! (the outValue of whatever precedes it in its curve, if any)
inline real_t resolve_in_value(const AutomationPoint &point, std::optional<real_t> prev_out_value) noexcept {
    return std::visit(
        [&](const auto &v) -> real_t {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, AutomationPoint::ArrivalFromPrevious>) {
                return prev_out_value ? *prev_out_value : real_t(0.0);
            } else {
                return v.value;
            }
        },
        point.in_value);
}

//! NOTE: value at normalized position t ([0, 1]) across the segment arriving at point
//! The segment is split into two quadratic Bezier arcs meeting at the bend point. The arcs
//! share a tangent there, producing a smooth bend whose position follows bend.t. When there's
//! no bend, this reduces to a straight line
inline real_t evaluate_at(const AutomationPoint &point, std::optional<real_t> prev_out_value, real_t t) noexcept {
    const real_t prev_out = prev_out_value ? *prev_out_value : real_t(0.0);
    const real_t this_in = resolve_in_value(point, prev_out_value);
    const real_t range = this_in - prev_out;
    const std::optional<AutomationPoint::Ease> point_ease = ease(point);

    if (!point_ease || point_ease->is_none() || point_ease->t <= 0.0 || point_ease->t >= 1.0) {
        return std::clamp(prev_out + range * t, real_t(0.0), real_t(1.0));
    }

    auto quadratic_bezier = [](real_t s, real_t p0, real_t p1, real_t p2) {
        const real_t u = 1.0 - s;
        return std::clamp(u * u * p0 + 2.0 * u * s * p1 + s * s * p2, real_t(0.0), real_t(1.0));
    };

    const real_t fraction = std::clamp(point_ease->value, real_t(0.0), real_t(1.0));
    const real_t bend_value = prev_out + fraction * range;
    const real_t lo = std::min(prev_out, this_in);
    const real_t hi = std::max(prev_out, this_in);
    const real_t half_slope = 0.5 * range;
    const real_t remainder = 1.0 - point_ease->t;

    // Clamped control points (q1, q2) for each arc
    const real_t q1 = std::clamp(bend_value - point_ease->t * half_slope, lo, hi);

    if (t <= point_ease->t) {
        return quadratic_bezier(t / point_ease->t, prev_out, q1, bend_value);
    }

    const real_t q2 = std::clamp(bend_value + remainder * half_slope, lo, hi);
    return quadratic_bezier((t - point_ease->t) / remainder, bend_value, q2, this_in);
}

template <typename Key>
inline real_t evaluate_curve_at(const AutomationCurve<Key> &curve, const typename AutomationCurve<Key>::key_type &pos) {
    if (curve.empty()) {
        return real_t{};
    }

    auto it = find_less_or_equal(curve, pos);
    if (it == curve.end()) {
        //! NOTE: hold the first point's value backwards in time, matching standard envelope semantics
        return curve.begin()->second.out_value;
    }

    real_t normalized = it->second.out_value;
    const auto next = std::next(it);
    if (next != curve.end()) {
        const real_t t = static_cast<real_t>(pos - it->first) / static_cast<real_t>(next->first - it->first);
        normalized = evaluate_at(next->second, it->second.out_value, t);
    }

    return normalized;
}

//! NOTE: resamples a curve into discrete samples for consumers that can't evaluate on read
template <typename Key, typename Callback>
void resample_curve(const AutomationCurve<Key> &curve, Key step_interval, Callback &&on_sample) {
    for (auto it = curve.cbegin(); it != curve.cend(); ++it) {
        on_sample(it->first, it->second.out_value);

        const auto next = std::next(it);
        if (next == curve.cend()) {
            continue;
        }

        const real_t next_arrival = resolve_in_value(next->second, it->second.out_value);
        const std::optional<AutomationPoint::Ease> next_ease = ease(next->second);
        const bool is_flat = (!next_ease || next_ease->is_none()) && muse::real_is_equal(next_arrival, it->second.out_value);
        if (is_flat) {
            continue;
        }

        const Key interval_duration = next->first - it->first;
        const size_t steps = std::max(size_t((interval_duration + step_interval - 1) / step_interval), size_t(1));

        for (size_t j = 1; j < steps; ++j) {
            const Key t = it->first + interval_duration * j / steps;
            const real_t normalized = evaluate_at(next->second, it->second.out_value, real_t(j) / real_t(steps));
            on_sample(t, normalized);
        }
    }
}
} // namespace muse::mpe
