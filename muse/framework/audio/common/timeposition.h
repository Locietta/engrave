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
#include "global/asserts.h"

#include "audiotypes.h"

namespace muse::audio {
struct TimePosition {
    inline samples_t samples() const { return samples_; }

    inline sample_rate_t sample_rate() const { return sample_rate_; }

    inline secs_t time() const { return time_; }

    TimePosition() = default;
    TimePosition(const TimePosition &other) = default;
    TimePosition &operator=(const TimePosition &other) = default;

    inline bool operator==(const TimePosition &other) const { return samples_ == other.samples_ && sample_rate_ == other.sample_rate_; }

    inline bool operator!=(const TimePosition &other) const { return !this->operator==(other); }

    inline bool is_valid() const { return sample_rate_ > 0; }

    inline void forward(samples_t delta) {
        IF_ASSERT_FAILED(sample_rate_ > 0) { return; }
        samples_ += delta;
        time_ += static_cast<double>(delta) / sample_rate_;
    }

    inline void forward(const TimePosition &delta) {
        IF_ASSERT_FAILED(delta.sample_rate() == sample_rate_) { return; }
        forward(delta.samples());
    }

    inline TimePosition forwarded(samples_t delta) const {
        IF_ASSERT_FAILED(sample_rate_ > 0) { return TimePosition(); }
        return TimePosition(samples_ + delta, sample_rate_);
    }

    inline TimePosition forwarded(const TimePosition &delta) const {
        IF_ASSERT_FAILED(delta.sample_rate() == sample_rate_) { return TimePosition(); }
        return forwarded(delta.samples());
    }

    static inline TimePosition zero(sample_rate_t sample_rate) {
        IF_ASSERT_FAILED(sample_rate > 0) { return TimePosition(); }
        return TimePosition(0, sample_rate);
    }

    static inline TimePosition from_samples(samples_t samples, sample_rate_t sample_rate) {
        IF_ASSERT_FAILED(sample_rate > 0) { return TimePosition(); }

        return TimePosition(samples, sample_rate);
    }

    static inline TimePosition from_time(secs_t time, sample_rate_t sample_rate) {
        IF_ASSERT_FAILED(sample_rate > 0) { return TimePosition(); }

        IF_ASSERT_FAILED(time >= 0.0) { return TimePosition(); }

        return TimePosition(static_cast<samples_t>(std::llround(time.raw() * sample_rate)), sample_rate);
    }

private:
    inline TimePosition(samples_t samples, sample_rate_t sample_rate)
        : samples_(samples), sample_rate_(sample_rate), time_(sample_rate > 0 ? static_cast<double>(samples) / sample_rate : 0.0) {}

    samples_t samples_ = 0;
    sample_rate_t sample_rate_ = 0;
    secs_t time_ = 0.0; // cache
};
} // namespace muse::audio
