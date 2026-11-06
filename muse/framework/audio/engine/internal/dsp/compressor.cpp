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
#include "compressor.h"

#include "audiomathutils.h"

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::dsp;

static constexpr float ratio_constant = 4.f;

Compressor::Compressor(const unsigned int sample_rate)
    : filter_config_(sample_rate, ::ratio_constant), feedback_factor_(1.f) // 0 = feed forward, 1 = feed back
{}

bool Compressor::is_active() const { return is_active_; }

void Compressor::set_is_active(const bool active) { is_active_ = active; }

volume_db_t Compressor::gain_smoothing(const float &new_gain_reduction) const {
    float coefficient = 0.f;

    if (new_gain_reduction <= previous_gain_reduction_) {
        coefficient = filter_config_.attack_time_coefficient();
    } else {
        coefficient = filter_config_.release_time_coefficient();
    }

    return (coefficient * previous_gain_reduction_) + ((1 - coefficient) * new_gain_reduction);
}

volume_db_t Compressor::compute_gain(const volume_db_t &logarithm_sample) const {
    if (logarithm_sample < soft_threshold_lower_) {
        return logarithm_sample;
    }

    if (logarithm_sample >= soft_threshold_lower_ && logarithm_sample <= soft_threshold_upper_) {
        return logarithm_sample + ((((1 / filter_config_.ratio()) - 1) * std::pow(logarithm_sample - soft_threshold_upper_, 2)) /
                                   (2 * filter_config_.knee_width()));
    }

    return filter_config_.logarithmic_threshold() + ((logarithm_sample - filter_config_.logarithmic_threshold()) / filter_config_.ratio());
}

void Compressor::process(const float linear_rms, float *buffer, const audioch_t &audio_channels_count,
                         const samples_t samples_per_channel) {
    float db_gain = muse::linear_to_db(linear_rms);

    if (db_gain <= filter_config_.minimum_operable_level()) {
        return;
    }

    // trying to predict the next sample by the previous gain reduction
    db_gain += feedback_factor_ * feedback_gain_;

    float db_diff = compute_gain(db_gain) - db_gain;

    feedback_gain_ = db_diff;
    float gain_fact = muse::db_to_linear(db_diff * (1.f + feedback_factor_));

    float current_gain_reduction = std::min(gain_fact, previous_gain_reduction_);

    // apply gain
    for (audioch_t audio_ch_num = 0; audio_ch_num < audio_channels_count; ++audio_ch_num) {
        multiply_samples(buffer, audio_channels_count, audio_ch_num, samples_per_channel, current_gain_reduction);
    }

    previous_gain_reduction_ = current_gain_reduction;
}
