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

    void allocateForMaxDelaySamples(int maxDelaySamples) { buffer_.setSize(maxDelaySamples); }

    void setBaseDelay(float samples, float maxModulationOffset) {
        int allocSize = int(samples + maxModulationOffset + 16.f); // some extra space for interpolation
        if (samples != baseDelay_ || allocSize > buffer_.getAllocatedSize()) {
            allocateForMaxDelaySamples(allocSize);
            baseDelay_ = samples;
            setModOffset(0.f);
            reset();
        }
    }

    float getBaseDelay() const { return baseDelay_; }

    void setModOffset(float smpOffset) {
        float total = -baseDelay_ + smpOffset; // negative!
        float flr = std::floor(total);
        modFraction_ = total - flr;
        readOffset_ = int(flr);
    }

    void writeSampleAndAdvance(float sample) {
        buffer_.writeOffset0(sample);
        buffer_.advance(1);
    }

    float readSample() {
        auto res = buffer_.read(readOffset_);
        auto x1 = buffer_.read(readOffset_ + 1);

        res += modFraction_ * (x1 - y1_);
        y1_ = res;
        return res;
    }

    float processSample(float sample) {
        writeSampleAndAdvance(sample);
        return readSample();
    }

    void reset() {
        buffer_.reset();
        y1_ = 0.f;
    }

private:
    int readOffset_ = 0;
    float modFraction_ = 0.f;
    float baseDelay_ = 0.f;

    CircularSampleBuffer<float> buffer_;
    float y1_ = 0.f;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_ALLPASSMODULATEDDELAY_H
