/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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
struct FlacHandler;

struct FlacEncoder {
    FlacEncoder(const SoundTrackFormat &, io::IODevice &);
    FlacEncoder(const FlacEncoder &) = delete;
    FlacEncoder &operator=(const FlacEncoder &) = delete;

    ~FlacEncoder() noexcept;

    bool begin(samples_t total_samples_number);
    size_t encode(samples_t samples_per_channel, const float *input);
    Ret end();

private:
    SoundTrackFormat format_;
    std::unique_ptr<FlacHandler> flac_;
};
} // namespace muse::audio::encode
