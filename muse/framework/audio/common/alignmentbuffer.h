/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include <cassert>
#include <vector>

namespace muse::audio {
//! NOTE This is a ring buffer, not thread-safe.
//! Designed to align processed audio blocks
//! For example, we expect block 512, but the driver requests 470
struct AlignmentBuffer {
    AlignmentBuffer(size_t capacity) : buf_(capacity) {}

    inline size_t capacity() const { return buf_.size(); }

    inline size_t availableRead() const { return availableRead_; }

    inline size_t availableWrite() const { return buf_.size() - availableRead_; }

    bool write(const float *data, size_t size) {
        assert(size <= availableWrite());
        if (!(size <= availableWrite())) {
            return false;
        }

        if (tail_ + size <= buf_.size()) {
            std::copy(data, data + size, buf_.begin() + tail_);
            tail_ += size;
        } else {
            size_t first_part = buf_.size() - tail_;
            std::copy(data, data + first_part, buf_.begin() + tail_);
            std::copy(data + first_part, data + size, buf_.begin());
            tail_ = size - first_part;
        }

        availableRead_ += size;
        return true;
    }

    bool read(float *dest, size_t size) {
        if (size == 0) {
            return true;
        }

        assert(size <= availableRead_);
        if (!(size <= availableRead_)) {
            return false;
        }

        if (head_ + size <= buf_.size()) {
            std::copy(buf_.begin() + head_, buf_.begin() + head_ + size, dest);
            head_ += size;
        } else {
            size_t first_part = buf_.size() - head_;
            std::copy(buf_.begin() + head_, buf_.begin() + buf_.size(), dest);
            std::copy(buf_.begin(), buf_.begin() + (size - first_part), dest + first_part);
            head_ = size - first_part;
        }

        availableRead_ -= size;
        return true;
    }

private:
    std::vector<float> buf_;
    size_t availableRead_ = 0;
    size_t head_ = 0;
    size_t tail_ = 0;
};
} // namespace muse::audio
