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
#include <functional>
#include <vector>

#include "../nodes/audionode.h"
#include "abstractaudioencoder.h"
#include "audio/common/audiotypes.h"
#include "global/types/ret.h"

namespace muse::io {
struct IODevice;
} // namespace muse::io

namespace muse::audio::soundtrack {
struct SoundTrackWriter {

    SoundTrackWriter(io::IODevice &dst_device, const SoundTrackFormat &format, const secs_t total_duration, IAudioNodePtr source,
                     std::function<void(uint64_t)> block_completed = {},
                     std::function<encode::AbstractAudioEncoderPtr(const SoundTrackFormat &, io::IODevice &)> encoder_factory = {},
                     std::function<bool()> cancelled = {});

    Ret write();
    void abort();


private:
    Ret write_streaming();
    bool poll_cancellation();
    std::function<bool()> cancelled_;

    IAudioNodePtr source_;
    std::function<void(uint64_t)> block_completed_;

    std::vector<float> interm_buffer_;
    samples_t render_step_ = 0;
    samples_t leading_silence_samples_ = 0;
    samples_t data_samples_ = 0;
    samples_t total_samples_ = 0;

    encode::AbstractAudioEncoderPtr encoder_ptr_;

    std::atomic<bool> is_aborted_ = false;
};
} // namespace muse::audio::soundtrack
