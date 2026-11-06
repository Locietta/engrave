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
#include "limiter.h"

#include "audiomathutils.h"

using namespace muse::audio;
using namespace muse::audio::dsp;

static constexpr volume_db_t threshold = volume_dbfs_t::make(0.f);

Limiter::Limiter(const unsigned int sample_rate) : filter_config_(sample_rate, 1.f, ::threshold) {}

bool Limiter::is_active() const { return is_active_; }

void Limiter::set_is_active(const bool active) { is_active_ = active; }

volume_db_t Limiter::gain_smoothing(const float new_gain_reduction) const {
    float coefficient = 0.f;

    if (new_gain_reduction <= previous_gain_reduction_) {
        coefficient = filter_config_.attack_time_coefficient();
    } else {
        coefficient = filter_config_.release_time_coefficient();
    }

    return (coefficient * previous_gain_reduction_) + ((1 - coefficient) * new_gain_reduction);
}

volume_db_t Limiter::compute_gain(const volume_db_t &logarithm_sample) const {
    if (logarithm_sample < filter_config_.soft_threshold_lower()) {
        return logarithm_sample;
    }

    if (logarithm_sample >= filter_config_.soft_threshold_lower() && logarithm_sample <= filter_config_.soft_threshold_upper()) {
        return logarithm_sample -
               (std::pow((logarithm_sample - filter_config_.soft_threshold_upper()), 2) / (2 * filter_config_.knee_width()));
    }

    return filter_config_.logarithmic_threshold();
}

void Limiter::process(const float &linear_rms, float *buffer, const audioch_t &audio_channels_count, const samples_t samples_per_channel) {
    volume_db_t rms_db = muse::linear_to_db(linear_rms);

    if (rms_db <= filter_config_.minimum_operable_level()) {
        return;
    }

    // gain computing
    float computed_gain = compute_gain(rms_db) - rms_db;

    // gain smoothing
    float smoothed_gain = gain_smoothing(computed_gain);
    previous_gain_reduction_ = smoothed_gain;

    // gain make-up
    float make_up_gain = smoothed_gain + filter_config_.make_up_gain();

    // total linear gain
    float total_linear_gain = muse::db_to_linear(make_up_gain);

    // apply linear gain
    for (audioch_t audio_ch_num = 0; audio_ch_num < audio_channels_count; ++audio_ch_num) {
        multiply_samples(buffer, audio_channels_count, audio_ch_num, samples_per_channel, total_linear_gain);
    }
}
