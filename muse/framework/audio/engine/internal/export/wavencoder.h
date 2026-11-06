/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "abstractaudioencoder.h"

namespace muse::io {
class IODevice;
} // namespace muse::io

namespace muse::audio::encode {
class WavEncoder : public AbstractAudioEncoder {
public:
    WavEncoder(const SoundTrackFormat &, io::IODevice &);

    bool begin(samples_t total_samples_number) override;
    size_t encode(samples_t samples_per_channel, const float *input) override;
    Ret end() override;

private:
    bool write_placeholder_header();
    bool patch_header_sizes();

    muse::io::IODevice *dst_device_ = nullptr;
    bool header_written_ = false;
    size_t pcm_start_pos_ = 0;
};
} // namespace muse::audio::encode
