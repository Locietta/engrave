/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include <memory>

#include "audioencoder.h"

namespace muse::io {
struct IODevice;
} // namespace muse::io

namespace muse::audio::encode {
struct AacEncoderHandler;

struct AacEncoder {
    AacEncoder(const SoundTrackFormat &, io::IODevice &);
    AacEncoder(const AacEncoder &) = delete;
    AacEncoder &operator=(const AacEncoder &) = delete;

    ~AacEncoder() noexcept;

    bool begin(samples_t total_samples_number);
    size_t encode(samples_t samples_per_channel, const float *input);
    Ret end();

private:
    SoundTrackFormat format_;
    std::unique_ptr<AacEncoderHandler> handler_;
    io::IODevice *dst_device_ = nullptr;
};
} // namespace muse::audio::encode
