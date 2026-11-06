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

#ifndef MUSE_AUDIO_AUDIOMATHUTILS_H
#define MUSE_AUDIO_AUDIOMATHUTILS_H

#include <cstdlib>
#include <limits>

#include "audio/common/audiotypes.h"

namespace muse::audio::dsp {
inline float balance_gain(const balance_t balance, const int audio_channel_number) {
    return (audio_channel_number * 2 - 1) * balance + 1.f;
}

inline float samples_root_mean_square(const float squared_sum, const samples_t sample_count) {
    return std::sqrt(squared_sum / sample_count);
}

inline float sample_attack_time_coefficient(const unsigned int sample_rate, const float attack_time_in_secs) {
    return std::exp(-std::log(9) / (sample_rate * attack_time_in_secs));
}

inline float sample_release_time_coefficient(const unsigned int sample_rate, const float release_time_in_secs) {
    return std::exp(-std::log(9) / (sample_rate * release_time_in_secs));
}

inline void multiply_samples(float *buffer, const audioch_t &audio_channels_count, const audioch_t &audio_channel_number,
                             const samples_t &samples_per_channel, const float &multiplier) {
    for (samples_t i = 0; i < samples_per_channel; ++i) {
        int idx = i * audio_channels_count + audio_channel_number;

        buffer[idx] *= multiplier;
    }
}

template <typename T>
constexpr T convert_float_samples(const float value, const int bits) {
    const int64_t max_val = (1LL << (bits - 1)) - 1LL;
    const float clamped_value = std::clamp(value, -1.0f, 1.0f);
    const float scaled_value = clamped_value * max_val;
    return static_cast<T>(std::round(scaled_value));
}
} // namespace muse::audio::dsp

#endif // MUSE_AUDIO_AUDIOMATHUTILS_H
