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
#ifndef MUSE_AUDIO_EQUALISER_H
#define MUSE_AUDIO_EQUALISER_H

#include "../../ifxprocessor.h"

namespace muse::audio::fx {
class Equaliser : public IFxProcessor {
public:
    Equaliser();

    void set_output_spec(const OutputSpec &spec) override;

    bool active() const override;
    void set_active(bool active) override;

    void set_frequency(float value);
    void set_gain(float value);
    void set_q(float value);

    void set_mode(const ProcessMode mode) override;
    bool should_process_during_silence() const override;

    void process(float *buffer, samples_t sample_count, samples_t playback_position_samples = 0) override;

private:
    void calculate();

    OutputSpec output_spec_;
    bool active_ = true;

    float gain_ = 0, frequency_ = 1000.f, q_ = 1.f;
    float a_[3] = {0, 0, 0};
    float b_[3] = {0, 0, 0};
    float x_[3] = {0, 0, 0};
    float y_[3] = {0, 0, 0};
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_EQUALISER_H
