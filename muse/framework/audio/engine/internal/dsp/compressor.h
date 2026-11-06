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

#ifndef MUSE_AUDIO_COMPRESSOR_H
#define MUSE_AUDIO_COMPRESSOR_H

#include <memory>

#include "envelopefilterconfig.h"

namespace muse::audio::dsp {
class Compressor {
public:
    Compressor(const unsigned int sample_rate);

    bool is_active() const;
    void set_is_active(const bool active);

    void process(const float linear_rms, float *buffer, const audioch_t &audio_channels_count, const samples_t samples_per_channel);
private:
    float attack_time_coefficient() const;
    float release_time_coefficient() const;

    volume_db_t gain_smoothing(const float &new_gain_reduction) const;
    volume_db_t compute_gain(const volume_db_t &logarithm_sample) const;

    EnvelopeFilterConfig filter_config_;

    float soft_threshold_lower_ = 0.f;
    float soft_threshold_upper_ = 0.f;
    float previous_gain_reduction_ = 1.f;

    float feedback_gain_ = 0.f;
    float feedback_factor_ = 0.f;

    bool is_active_ = false;
};

using CompressorPtr = std::unique_ptr<Compressor>;
} // namespace muse::audio::dsp

#endif // MUSE_AUDIO_COMPRESSOR_H
