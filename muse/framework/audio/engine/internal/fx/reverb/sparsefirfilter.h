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
struct SparseFirFilter {
    SparseFirFilter() {}

    void reset() { buffer_.reset(); }

    void set_format(int max_block_size, int max_impulse_offset) {
        buffer_.set_size(max_block_size + max_impulse_offset + 1);
        block_size_ = max_block_size;
        max_allowed_offset_ = max_impulse_offset;
    }

    void process_block(const float *signal_in, float *signal_out, int n) {
        assert(n <= block_size_);
        assert(impulses_.size() > 0);

        buffer_.write_block(0, n, signal_in);

        buffer_.read_block_with_gain(-impulses_[0].offset, n, signal_out, impulses_[0].gain_fact);
        for (size_t i = 1; i < impulses_.size(); ++i) {
            buffer_.read_add_block_with_gain(-impulses_[i].offset, n, signal_out, impulses_[i].gain_fact);
        }

        buffer_.advance(n);
    }

    float process_sample(float smp) {
        buffer_.write_offset0(smp);

        smp = 0.f;
        for (const auto &i : impulses_) {
            smp += buffer_.read(-i.offset) * i.gain_fact;
        }

        buffer_.advance(1);
        return smp;
    }

    void clear_impulses() { impulses_.clear(); }

    void append_impulse(int offset, float value) {
        assert(offset >= 0 && offset <= max_allowed_offset_);

        impulses_.push_back({offset, value});
        buffer_.set_size(offset + 1);
    }

private:
    struct Impulse {
        int offset;
        float gain_fact;
    };

    std::vector<Impulse> impulses_;

    int block_size_ = 0;
    int max_allowed_offset_ = 0;
    CircularSampleBuffer<float> buffer_;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_SPARSEFIRFILTER_H
