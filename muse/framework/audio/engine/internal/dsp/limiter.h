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

#ifndef MUSE_AUDIO_LIMITER_H
#define MUSE_AUDIO_LIMITER_H

#include <memory>

#include "envelopefilterconfig.h"

namespace muse::audio::dsp {
struct Limiter {
    Limiter(const unsigned int sample_rate);

    bool is_active() const;
    void set_is_active(const bool active);

    void process(const float &linear_rms, float *buffer, const audioch_t &audio_channels_count, const samples_t samples_per_channel);

private:
    volume_db_t gain_smoothing(const float new_gain_reduction) const;
    volume_db_t compute_gain(const volume_db_t &logarithm_sample) const;

    EnvelopeFilterConfig filter_config_;

    float previous_gain_reduction_ = 0.f;

    bool is_active_ = false;
};

using LimiterPtr = std::unique_ptr<Limiter>;
} // namespace muse::audio::dsp

#endif // MUSE_AUDIO_LIMITER_H
