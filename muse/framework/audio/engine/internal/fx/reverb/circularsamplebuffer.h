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

#pragma once

#include <cstring>
#include <algorithm>
#include <cassert>

#include "vectorops.h"

/*
 * Utility buffer class for delay-based effects
 * Allocates a 2^n size buffer for branchless write and read using a bitmask
 * It can read and write to past and future samples, but does not check for overlaps
 */

namespace muse::audio::fx {
template <typename SampleT>
class CircularSampleBuffer {
public:
    CircularSampleBuffer() {}

    ~CircularSampleBuffer() {
        if (buffer_) {
            free(buffer_);
        }
    }

    void setSize(int n) {
        if (n > allocatedSize_) {
            auto oldSize = allocatedSize_;

            auto findLargerPowerOfTwo = [](int32_t number) {
                int32_t powerOf2 = 1;
                while (powerOf2 < number) {
                    powerOf2 *= 2;
                }
                return powerOf2;
            };

            allocatedSize_ = findLargerPowerOfTwo(n);
            bufferSizeMask_ = allocatedSize_ - 1;
            buffer_ = (SampleT *) std::realloc(buffer_, allocatedSize_ * sizeof(SampleT));

            // Reset the new memory region
            assert(buffer_);
            std::fill(buffer_ + oldSize, buffer_ + allocatedSize_, 0.f);
        }
    }

    int getAllocatedSize() const { return allocatedSize_; }

    void reset() {
        if (buffer_ && allocatedSize_ > 0) {
            std::memset(buffer_, 0, sizeof(SampleT) * allocatedSize_);
        }
        positionIndex_ = 0;
    }

    void write(int offset, const SampleT &sample) { buffer_[(positionIndex_ + offset) & bufferSizeMask_] = sample; }

    void writeOffset0(const SampleT &sample) { buffer_[positionIndex_] = sample; }

    const SampleT &read(int offset) const { return buffer_[(positionIndex_ + offset) & bufferSizeMask_]; }

    /// change the 0 position by n
    void advance(int n) {
        positionIndex_ += n;
        positionIndex_ &= bufferSizeMask_;
    }

private:
    template <typename fnc>
    void splitBlockOffsetFunction(int startOffset, int n, fnc f) const {
        assert(n <= allocatedSize_);
        int firstIndex = (positionIndex_ + startOffset) & bufferSizeMask_;
        int n_to_end = allocatedSize_ - firstIndex;
        if (n_to_end > n) {
            f(firstIndex, 0, n);
        } else {
            f(firstIndex, 0, n_to_end);
            f(0, n_to_end, n - n_to_end);
        }
    }

public:
    void writeBlock(int startOffset, int n, const SampleT *sourceBlock) {
        splitBlockOffsetFunction(startOffset, n, [this, sourceBlock](int bufferOff, int sampleOff, int n) {
            vo::copy(&sourceBlock[sampleOff], &buffer_[bufferOff], n);
        });
    }

    void readBlockWithGain(int startOffset, int n, SampleT *targetBlock, float gainFactor) const {
        splitBlockOffsetFunction(startOffset, n, [this, targetBlock, gainFactor](int bufferOff, int sampleOff, int n) {
            vo::constantMultiply(&buffer_[bufferOff], gainFactor, &targetBlock[sampleOff], n);
        });
    }

    void readAddBlockWithGain(int startOffset, int n, SampleT *targetBlock, float gainFactor) const {
        splitBlockOffsetFunction(startOffset, n, [this, targetBlock, gainFactor](int bufferOff, int sampleOff, int n) {
            vo::constantMultiplyAndAdd(&buffer_[bufferOff], gainFactor, &targetBlock[sampleOff], n);
        });
    }

private:
    SampleT *buffer_ = nullptr;

    int positionIndex_ = 0;  // position of sample index 0 inside _buffer. This is where the next sample will be written to
    int allocatedSize_ = 0;  // 2^n buffer size
    int bufferSizeMask_ = 0; // 2^n-1 buffer mask
};
} // namespace muse::audio::fx
