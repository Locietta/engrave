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

    void allocateForMaxDelaySamples(int maxDelaySamples) {
        for (auto ch = 0; ch < maxNumChannels; ++ch) {
            buffer_[ch].setSize(maxDelaySamples);
        }
    }

    void setDelaySamples(int samples) {
        if (delaySamples_ == samples) {
            return;
        }

        delaySamples_ = samples;
        allocateForMaxDelaySamples(samples);
        reset();
    }

    int getDelaySamples() const { return delaySamples_; }

    void reset() {
        for (auto ch = 0; ch < maxNumChannels; ++ch) {
            buffer_[ch].reset();
        }
    }

    SampleT processSample(const SampleT &sample, int ch) {
        if (delaySamples_ == 0) {
            return sample;
        }

        auto s = buffer_[ch].read(-delaySamples_);
        buffer_[ch].writeOffset0(sample);
        buffer_[ch].advance(1);
        return s;
    }

    void processBlock(SampleT **smp, int numCh, int numSamples) {
        assert(numCh <= maxNumChannels);
        if (delaySamples_ == 0) {
            return;
        }

        for (auto ch = 0; ch < numCh; ++ch) {
            for (auto i = 0; i < numSamples; ++i) {
                auto s = buffer_[ch].read(-delaySamples_);
                buffer_[ch].writeOffset0(smp[ch][i]);
                smp[ch][i] = s;
                buffer_[ch].advance(1);
            }
        }
    }

private:
    CircularSampleBuffer<SampleT> buffer_[maxNumChannels];
    int delaySamples_ = 0;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_SAMPLEDELAY_H
