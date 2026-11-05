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

#include <cstdint>
#include <memory>
#include <vector>

struct LameHandler;

namespace muse::io {
class IODevice;
} // namespace muse::io

namespace muse::audio::encode {
class Mp3Encoder : public AbstractAudioEncoder {
public:
    Mp3Encoder(const SoundTrackFormat &, io::IODevice &);

    ~Mp3Encoder() noexcept override;

    bool begin(samples_t totalSamplesNumber) override;
    size_t encode(samples_t samplesPerChannel, const float *input) override;
    Ret end() override;

private:
    std::vector<std::uint8_t> outputBuffer_;
    std::unique_ptr<LameHandler> handler_;
    io::IODevice *dstDevice_ = nullptr;
};
} // namespace muse::audio::encode
