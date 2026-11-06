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

#ifndef MUSE_AUDIO_SAMPLEDELAY_H
#define MUSE_AUDIO_SAMPLEDELAY_H

#include <cassert>

#include "circularsamplebuffer.h"

namespace muse::audio::fx {
template <typename SampleT, int maxNumChannels>
class SampleDelay {
public:
    SampleDelay() {}

    ~SampleDelay() {}

    void allocate_for_max_delay_samples(int max_delay_samples) {
        for (auto ch = 0; ch < maxNumChannels; ++ch) {
            buffer_[ch].set_size(max_delay_samples);
        }
    }

    void set_delay_samples(int samples) {
        if (delay_samples_ == samples) {
            return;
        }

        delay_samples_ = samples;
        allocate_for_max_delay_samples(samples);
        reset();
    }

    int get_delay_samples() const { return delay_samples_; }

    void reset() {
        for (auto ch = 0; ch < maxNumChannels; ++ch) {
            buffer_[ch].reset();
        }
    }

    SampleT process_sample(const SampleT &sample, int ch) {
        if (delay_samples_ == 0) {
            return sample;
        }

        auto s = buffer_[ch].read(-delay_samples_);
        buffer_[ch].write_offset0(sample);
        buffer_[ch].advance(1);
        return s;
    }

    void process_block(SampleT **smp, int num_ch, int num_samples) {
        assert(num_ch <= maxNumChannels);
        if (delay_samples_ == 0) {
            return;
        }

        for (auto ch = 0; ch < num_ch; ++ch) {
            for (auto i = 0; i < num_samples; ++i) {
                auto s = buffer_[ch].read(-delay_samples_);
                buffer_[ch].write_offset0(smp[ch][i]);
                smp[ch][i] = s;
                buffer_[ch].advance(1);
            }
        }
    }

private:
    CircularSampleBuffer<SampleT> buffer_[maxNumChannels];
    int delay_samples_ = 0;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_SAMPLEDELAY_H
