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
struct CircularSampleBuffer {
    CircularSampleBuffer() {}

    ~CircularSampleBuffer() {
        if (buffer_) {
            free(buffer_);
        }
    }

    void set_size(int n) {
        if (n > allocated_size_) {
            auto old_size = allocated_size_;

            auto find_larger_power_of_two = [](int32_t number) {
                int32_t power_of2 = 1;
                while (power_of2 < number) {
                    power_of2 *= 2;
                }
                return power_of2;
            };

            allocated_size_ = find_larger_power_of_two(n);
            buffer_size_mask_ = allocated_size_ - 1;
            buffer_ = (SampleT *) std::realloc(buffer_, allocated_size_ * sizeof(SampleT));

            // Reset the new memory region
            assert(buffer_);
            std::fill(buffer_ + old_size, buffer_ + allocated_size_, 0.f);
        }
    }

    int get_allocated_size() const { return allocated_size_; }

    void reset() {
        if (buffer_ && allocated_size_ > 0) {
            std::memset(buffer_, 0, sizeof(SampleT) * allocated_size_);
        }
        position_index_ = 0;
    }

    void write(int offset, const SampleT &sample) { buffer_[(position_index_ + offset) & buffer_size_mask_] = sample; }

    void write_offset0(const SampleT &sample) { buffer_[position_index_] = sample; }

    const SampleT &read(int offset) const { return buffer_[(position_index_ + offset) & buffer_size_mask_]; }

    /// change the 0 position by n
    void advance(int n) {
        position_index_ += n;
        position_index_ &= buffer_size_mask_;
    }

private:
    template <typename fnc>
    void split_block_offset_function(int start_offset, int n, fnc f) const {
        assert(n <= allocated_size_);
        int first_index = (position_index_ + start_offset) & buffer_size_mask_;
        int n_to_end = allocated_size_ - first_index;
        if (n_to_end > n) {
            f(first_index, 0, n);
        } else {
            f(first_index, 0, n_to_end);
            f(0, n_to_end, n - n_to_end);
        }
    }

public:
    void write_block(int start_offset, int n, const SampleT *source_block) {
        split_block_offset_function(start_offset, n, [this, source_block](int buffer_off, int sample_off, int n) {
            vo::copy(&source_block[sample_off], &buffer_[buffer_off], n);
        });
    }

    void read_block_with_gain(int start_offset, int n, SampleT *target_block, float gain_factor) const {
        split_block_offset_function(start_offset, n, [this, target_block, gain_factor](int buffer_off, int sample_off, int n) {
            vo::constant_multiply(&buffer_[buffer_off], gain_factor, &target_block[sample_off], n);
        });
    }

    void read_add_block_with_gain(int start_offset, int n, SampleT *target_block, float gain_factor) const {
        split_block_offset_function(start_offset, n, [this, target_block, gain_factor](int buffer_off, int sample_off, int n) {
            vo::constant_multiply_and_add(&buffer_[buffer_off], gain_factor, &target_block[sample_off], n);
        });
    }

private:
    SampleT *buffer_ = nullptr;

    int position_index_ = 0;   // position of sample index 0 inside _buffer. This is where the next sample will be written to
    int allocated_size_ = 0;   // 2^n buffer size
    int buffer_size_mask_ = 0; // 2^n-1 buffer mask
};
} // namespace muse::audio::fx
