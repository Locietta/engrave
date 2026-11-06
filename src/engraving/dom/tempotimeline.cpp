/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include "tempotimeline.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "automation/tempovalues.h"
#include "types/constants.h"

#include "global/realfn.h"
#include "global/log.h"

using namespace mu::engraving;

static constexpr int min_tempo_resample_steps = 8;

static TempoValues resample_tempo_values(const AutomationCurve &tempo_curve) {
    TRACEFUNC;

    // No tempo markings at all - hold the default tempo
    if (tempo_curve.empty()) {
        return {{0, Constants::default_tempo.val}};
    }

    // Start with every authored point, denormalized back to bps,
    // plus a default-tempo point at tick 0 if the curve doesn't already start there
    TempoValues authored;
    authored.reserve(tempo_curve.size() + 1);
    if (tempo_curve.begin()->first > 0) {
        authored.emplace_back(0, Constants::default_tempo.val);
    }

    for (const auto &[utick, point] : tempo_curve) {
        authored.emplace_back(utick, denormalize_tempo(point.value.out_value).val);
    }

    // Fill in the ramp shape of gradual tempo changes (accelerando/ritardando) between where
    // they start and where they arrive.
    // anchorUTick/anchorNorm track where the CURRENT ramp started, only advancing on a real
    // ramp arrival, so a marking inside another ramp's span doesn't restart it.
    // currentNorm is the actual last-known tempo, updated by every point - used to check if a
    // new ramp is flat, so it doesn't get bridged to an old, unrelated ramp's anchor
    TempoValues resampled;

    int anchor_u_tick = tempo_curve.begin()->first;
    real_t anchor_norm = tempo_curve.begin()->second.value.out_value;
    real_t current_norm = anchor_norm;

    // Resampled ticks only increase across the whole loop below, so this hint can walk
    // `authored` forward once instead of a find() at every resampled tick
    auto authored_hint = authored.begin();

    for (auto it = std::next(tempo_curve.begin()); it != tempo_curve.end(); ++it) {
        if (!std::holds_alternative<AutomationPoint::ExplicitArrival>(it->second.value.in_value)) {
            current_norm = it->second.value.out_value;
            continue;
        }

        const std::optional<AutomationPoint::Ease> point_ease = ease(it->second);
        const real_t arrival_norm = muse::mpe::resolve_in_value(it->second.value, std::nullopt);
        const bool is_flat = (!point_ease || point_ease->is_none()) && muse::real_is_equal(arrival_norm.raw(), current_norm.raw());
        if (is_flat) {
            anchor_u_tick = it->first;
            anchor_norm = it->second.value.out_value;
            current_norm = anchor_norm;
            continue;
        }

        const int segment_ticks = it->first - anchor_u_tick;
        const int steps = std::max(::min_tempo_resample_steps, segment_ticks / Constants::division);
        for (int s = 1; s < steps; ++s) {
            const int utick = anchor_u_tick + segment_ticks * s / steps;

            while (authored_hint != authored.end() && authored_hint->first < utick) {
                ++authored_hint;
            }
            if (authored_hint != authored.end() && authored_hint->first == utick) {
                continue; // there is already an authored point at this tick
            }
            if (!resampled.empty() && resampled.back().first == utick) {
                continue; // rounding produced the same tick as the previous step in this ramp
            }

            const muse::real_t t = muse::real_t(s) / muse::real_t(steps);
            const real_t normalized = muse::mpe::evaluate_at(it->second.value, anchor_norm, t);
            const BeatsPerSecond bps = denormalize_tempo(normalized);
            resampled.emplace_back(utick, bps.val);
        }

        anchor_u_tick = it->first;
        anchor_norm = it->second.value.out_value;
        current_norm = anchor_norm;
    }

    if (resampled.empty()) {
        return authored;
    }

    TempoValues merged;
    merged.reserve(authored.size() + resampled.size());
    std::merge(authored.begin(), authored.end(), resampled.begin(), resampled.end(), std::back_inserter(merged),
               [](const std::pair<int, double> &a, const std::pair<int, double> &b) { return a.first < b.first; });
    return merged;
}

void TempoTimeline::rebuild(const AutomationCurve &tempo_curve, const PausesMap &pauses) {
    rebuild(resample_tempo_values(tempo_curve), pauses);
}

void TempoTimeline::rebuild(const TempoValues &tempo_values, const PausesMap &pauses) {
    TRACEFUNC;

    points_.clear();
    pauses_ = pauses;
    utick2utime_hint_ = 0;
    utime2utick_hint_ = 0;
    tempo_hint_ = 0;

    points_.reserve(tempo_values.size() + pauses.size());

    // Merge-walk values (tick->bps) and pauses (tick->held seconds) by tick in one pass
    auto vit = tempo_values.begin();
    auto pit = pauses.begin();

    while (vit != tempo_values.end() || pit != pauses.end()) {
        const bool value_is_next = pit == pauses.end() || (vit != tempo_values.end() && vit->first <= pit->first);
        const int utick = value_is_next ? vit->first : pit->first;

        if (value_is_next) {
            double time = 0.0;
            if (!points_.empty()) {
                const TempoTimePoint &prev = points_.back();
                time = prev.time + double(utick - prev.utick) / (Constants::division * prev.bps);
            }
            // Clamp to the valid tempo range so a zero/negative bps can't make time math divide by zero
            const double bps = std::clamp(vit->second, Constants::min_tempo.val, Constants::max_tempo.val);
            points_.push_back({utick, time, bps, 0.0});
            ++vit;
        }

        // A pause can land on the same tick as the value point just emitted above
        if (pit != pauses.end() && pit->first == utick) {
            if (points_.empty()) {
                // No tempo value at or before this pause - anchor with the default tempo
                points_.push_back({utick, 0.0, Constants::default_tempo.val, 0.0});
            }
            const TempoTimePoint &prev = points_.back();
            const double time = prev.utick == utick ? prev.time : prev.time + double(utick - prev.utick) / (Constants::division * prev.bps);
            points_.push_back({utick, time + pit->second, prev.bps, pit->second});
            ++pit;
        }
    }
}

double TempoTimeline::utick2utime(int utick) const {
    const size_t n = points_.size();
    if (n == 0) {
        return 0.;
    }

    bool hint_valid = false;
    if (utick2utime_hint_ < n) {
        const int hint_utick = points_[utick2utime_hint_].utick;
        const bool is_last_point = (utick2utime_hint_ + 1 == n);
        hint_valid = utick >= hint_utick && (is_last_point || utick < points_[utick2utime_hint_ + 1].utick);
    }

    if (!hint_valid) {
        const auto it = std::upper_bound(points_.begin(), points_.end(), utick, [](int u, const TempoTimePoint &p) { return u < p.utick; });
        utick2utime_hint_ = it == points_.begin() ? 0 : size_t(std::prev(it) - points_.begin());
    }

    const TempoTimePoint &from = points_[utick2utime_hint_];
    const double nominal_time = from.time + double(utick - from.utick) / (Constants::division * from.bps);
    return nominal_time / tempo_multiplier_.val;
}

int TempoTimeline::utime2utick(double time) const {
    const size_t n = points_.size();
    if (n == 0) {
        return 0;
    }

    const double nominal_secs = time * tempo_multiplier_.val;

    bool hint_valid = false;
    if (utime2utick_hint_ < n) {
        const double hint_time = points_[utime2utick_hint_].time;
        const bool is_last_point = (utime2utick_hint_ + 1 == n);
        hint_valid = nominal_secs >= hint_time && (is_last_point || nominal_secs < points_[utime2utick_hint_ + 1].time);
    }

    if (!hint_valid) {
        const auto it =
            std::upper_bound(points_.begin(), points_.end(), nominal_secs, [](double s, const TempoTimePoint &p) { return s < p.time; });
        utime2utick_hint_ = it == points_.begin() ? 0 : size_t(std::prev(it) - points_.begin());
    }

    const size_t idx = utime2utick_hint_;
    const TempoTimePoint &from = points_[idx];

    if (idx + 1 < n) {
        const TempoTimePoint &next = points_[idx + 1];
        if (next.pause > 0.0 && nominal_secs >= next.time - next.pause) {
            return next.utick; // still inside next's pause window: hold there
        }
    }

    const double elapsed = nominal_secs - from.time;
    return from.utick + int(std::lround(elapsed * Constants::division * from.bps));
}

BeatsPerSecond TempoTimeline::tempo(int utick) const {
    const size_t n = points_.size();
    if (n == 0) {
        return Constants::default_tempo;
    }

    bool hint_valid = false;
    if (tempo_hint_ < n) {
        const int hint_utick = points_[tempo_hint_].utick;
        const bool is_last_point = (tempo_hint_ + 1 == n);
        hint_valid = utick >= hint_utick && (is_last_point || utick < points_[tempo_hint_ + 1].utick);
    }

    if (!hint_valid) {
        const auto it = std::upper_bound(points_.begin(), points_.end(), utick, [](int u, const TempoTimePoint &p) { return u < p.utick; });
        tempo_hint_ = it == points_.begin() ? 0 : size_t(std::prev(it) - points_.begin());
    }

    return BeatsPerSecond(points_[tempo_hint_].bps);
}

void TempoTimeline::set_tempo_multiplier(const BeatsPerSecond &bps) {
    IF_ASSERT_FAILED(bps.val > 0.0) { return; }

    tempo_multiplier_ = bps;
}
