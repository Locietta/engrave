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

#ifndef MUSE_AUDIO_SPARSEFIRFILTER_H
#define MUSE_AUDIO_SPARSEFIRFILTER_H

#include <cassert>
#include <vector>

#include "circularsamplebuffer.h"

namespace muse::audio::fx {
class SparseFirFilter {
public:
    SparseFirFilter() {}

    void reset() { buffer_.reset(); }

    void setFormat(int maxBlockSize, int maxImpulseOffset) {
        buffer_.setSize(maxBlockSize + maxImpulseOffset + 1);
        blockSize_ = maxBlockSize;
        maxAllowedOffset_ = maxImpulseOffset;
    }

    void processBlock(const float *signal_in, float *signal_out, int n) {
        assert(n <= blockSize_);
        assert(impulses_.size() > 0);

        buffer_.writeBlock(0, n, signal_in);

        buffer_.readBlockWithGain(-impulses_[0].offset, n, signal_out, impulses_[0].gainFact);
        for (size_t i = 1; i < impulses_.size(); ++i) {
            buffer_.readAddBlockWithGain(-impulses_[i].offset, n, signal_out, impulses_[i].gainFact);
        }

        buffer_.advance(n);
    }

    float processSample(float smp) {
        buffer_.writeOffset0(smp);

        smp = 0.f;
        for (const auto &i : impulses_) {
            smp += buffer_.read(-i.offset) * i.gainFact;
        }

        buffer_.advance(1);
        return smp;
    }

    void clearImpulses() { impulses_.clear(); }

    void appendImpulse(int offset, float value) {
        assert(offset >= 0 && offset <= maxAllowedOffset_);

        impulses_.push_back({offset, value});
        buffer_.setSize(offset + 1);
    }

private:
    struct Impulse {
        int offset;
        float gainFact;
    };

    std::vector<Impulse> impulses_;

    int blockSize_ = 0;
    int maxAllowedOffset_ = 0;
    CircularSampleBuffer<float> buffer_;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_SPARSEFIRFILTER_H
