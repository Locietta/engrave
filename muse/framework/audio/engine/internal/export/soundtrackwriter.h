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
#include <atomic>

#include "global/types/ret.h"

#include <vector>
#include <functional>

#include "audio/common/audiotypes.h"
#include "../nodes/audionode.h"

#include "abstractaudioencoder.h"

namespace muse::io {
class IODevice;
} // namespace muse::io

namespace muse::audio::soundtrack {
class SoundTrackWriter {

public:
    SoundTrackWriter(io::IODevice &dstDevice, const SoundTrackFormat &format, const secs_t totalDuration, IAudioNodePtr source,
                     std::function<void(uint64_t)> blockCompleted = {},
                     std::function<encode::AbstractAudioEncoderPtr(const SoundTrackFormat &, io::IODevice &)> encoderFactory = {},
                     std::function<bool()> cancelled = {});

    Ret write();
    void abort();


private:
    Ret writeStreaming();
    bool pollCancellation();
    std::function<bool()> cancelled_;

    IAudioNodePtr source_;
    std::function<void(uint64_t)> blockCompleted_;

    std::vector<float> intermBuffer_;
    samples_t renderStep_ = 0;
    samples_t leadingSilenceSamples_ = 0;
    samples_t dataSamples_ = 0;
    samples_t totalSamples_ = 0;

    encode::AbstractAudioEncoderPtr encoderPtr_;

    std::atomic<bool> isAborted_ = false;
};
} // namespace muse::audio::soundtrack
