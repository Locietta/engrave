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

#ifndef MUSE_AUDIO_DSP_ENVELOPEFILTERCONFIG_H
#define MUSE_AUDIO_DSP_ENVELOPEFILTERCONFIG_H

#include "audio/common/audiotypes.h"
#include "audiomathutils.h"

namespace muse::audio::dsp {
struct EnvelopeFilterConfig {
    explicit EnvelopeFilterConfig(
        const unsigned int sample_rate, const float ratio = ::muse::audio::dsp::EnvelopeFilterConfig::default_ratio,
        const volume_db_t logarithmic_threshold = ::muse::audio::dsp::EnvelopeFilterConfig::default_threshold_db,
        const volume_db_t knee_width = ::muse::audio::dsp::EnvelopeFilterConfig::default_knee_width,
        const float attack_time = ::muse::audio::dsp::EnvelopeFilterConfig::default_attack_time,
        const float release_time = ::muse::audio::dsp::EnvelopeFilterConfig::default_release_time,
        const volume_db_t minimum_operable_level = ::muse::audio::dsp::EnvelopeFilterConfig::default_minimum_operable_level,
        const volume_db_t make_up_gain = ::muse::audio::dsp::EnvelopeFilterConfig::default_make_up_gain) {
        sample_rate_ = sample_rate;
        ratio_ = ratio;
        knee_width_ = knee_width;
        logarithmic_threshold_ = logarithmic_threshold;
        attack_time_ = attack_time;
        release_time_ = release_time;

        attack_time_coefficient_ = sample_attack_time_coefficient(sample_rate, attack_time);
        release_time_coefficient_ = sample_release_time_coefficient(sample_rate, release_time);

        minimum_operable_level_ = minimum_operable_level;

        soft_threshold_lower_ = logarithmic_threshold_ - (knee_width_ / 2.f);
        soft_threshold_upper_ = logarithmic_threshold_ + (knee_width_ / 2.f);

        make_up_gain_ = make_up_gain;
    }

    static constexpr float default_knee_width = 6.f; // [0, 20.f]
    static constexpr float default_ratio = 4.f;      // [1, 50.f]
    static constexpr volume_db_t default_threshold_db = volume_dbfs_t::make(-10.f);
    static constexpr float default_attack_time = 0.005f; // [0, 4.f]
    static constexpr float default_release_time = 0.02f; // [0, 4.f]
    static constexpr volume_db_t default_minimum_operable_level = volume_dbfs_t::make(-60.f);
    static constexpr volume_db_t default_make_up_gain = volume_dbfs_t::make(-10.f); // [-10.f, 12.f]

    unsigned int sample_rate() const { return sample_rate_; }

    /// Compression ratio is the input/output ratio for signals that overshoot the operation threshold.
    float ratio() const { return ratio_; }

    volume_db_t knee_width() const { return knee_width_; }

    /// Operation threshold is the level above which gain is applied to the input signal.
    volume_db_t logarithmic_threshold() const { return logarithmic_threshold_; }

    float attack_time_coefficient() const { return attack_time_coefficient_; }

    float release_time_coefficient() const { return release_time_coefficient_; }

    volume_db_t minimum_operable_level() const { return minimum_operable_level_; }

    volume_db_t soft_threshold_upper() const { return soft_threshold_upper_; }

    volume_db_t soft_threshold_lower() const { return soft_threshold_lower_; }

    /// Make-up gain compensates for gain lost during limiting. It is applied at the output of the dynamic range limiter.
    volume_db_t make_up_gain() const { return make_up_gain_; }

private:
    unsigned int sample_rate_ = 0;
    float ratio_ = 0.f;
    volume_db_t knee_width_ = 0.f;
    volume_db_t logarithmic_threshold_ = 0.f;
    float attack_time_ = 0.f;
    float release_time_ = 0.f;
    float attack_time_coefficient_ = 0.f;
    float release_time_coefficient_ = 0.f;
    float minimum_operable_level_ = 0.f;

    volume_db_t soft_threshold_upper_ = 0.f;
    volume_db_t soft_threshold_lower_ = 0.f;
    volume_db_t make_up_gain_ = 0.f;
};
} // namespace muse::audio::dsp

#endif // MUSE_AUDIO_DSP_ABSTRACTENVELOPEFILTER_H
