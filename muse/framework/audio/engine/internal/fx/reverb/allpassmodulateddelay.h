/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#ifndef MUSE_AUDIO_ALLPASSMODULATEDDELAY_H
#define MUSE_AUDIO_ALLPASSMODULATEDDELAY_H

#include "circularsamplebuffer.h"

namespace muse::audio::fx {
class AllPassModulatedDelay {
public:
    AllPassModulatedDelay() {}

    void allocate_for_max_delay_samples(int max_delay_samples) { buffer_.set_size(max_delay_samples); }

    void set_base_delay(float samples, float max_modulation_offset) {
        int alloc_size = int(samples + max_modulation_offset + 16.f); // some extra space for interpolation
        if (samples != base_delay_ || alloc_size > buffer_.get_allocated_size()) {
            allocate_for_max_delay_samples(alloc_size);
            base_delay_ = samples;
            set_mod_offset(0.f);
            reset();
        }
    }

    float get_base_delay() const { return base_delay_; }

    void set_mod_offset(float smp_offset) {
        float total = -base_delay_ + smp_offset; // negative!
        float flr = std::floor(total);
        mod_fraction_ = total - flr;
        read_offset_ = int(flr);
    }

    void write_sample_and_advance(float sample) {
        buffer_.write_offset0(sample);
        buffer_.advance(1);
    }

    float read_sample() {
        auto res = buffer_.read(read_offset_);
        auto x1 = buffer_.read(read_offset_ + 1);

        res += mod_fraction_ * (x1 - y1_);
        y1_ = res;
        return res;
    }

    float process_sample(float sample) {
        write_sample_and_advance(sample);
        return read_sample();
    }

    void reset() {
        buffer_.reset();
        y1_ = 0.f;
    }

private:
    int read_offset_ = 0;
    float mod_fraction_ = 0.f;
    float base_delay_ = 0.f;

    CircularSampleBuffer<float> buffer_;
    float y1_ = 0.f;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_ALLPASSMODULATEDDELAY_H
