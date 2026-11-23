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
#include <vector>

#include "../offlinerendersession.h"
#include "audio/common/audiotypes.h"
#include "audioencoder.h"
#include "global/fn.h"
#include "global/types/ret.h"
#include "global/unique_fn.h"

namespace muse::io {
struct IODevice;
} // namespace muse::io

namespace muse::audio::soundtrack {
struct SoundTrackWriter {

    SoundTrackWriter(io::IODevice &dst_device, const SoundTrackFormat &format, const secs_t total_duration,
                     engine::OfflineRenderSession *source, muse::UniqueFn<void(uint64_t) const> block_completed = {},
                     muse::FnRef<encode::UniqueEncoder(const SoundTrackFormat &, io::IODevice &)> encoder_factory = {},
                     muse::UniqueFn<bool() const> cancelled = {});

    Ret write();
    void abort();


private:
    Ret write_streaming();
    bool poll_cancellation();
    muse::UniqueFn<bool() const> cancelled_;

    engine::OfflineRenderSession *source_;
    muse::UniqueFn<void(uint64_t) const> block_completed_;

    std::vector<float> interm_buffer_;
    samples_t render_step_ = 0;
    samples_t leading_silence_samples_ = 0;
    samples_t data_samples_ = 0;
    samples_t total_samples_ = 0;

    encode::AudioEncoder encoder_ptr_;

    std::atomic<bool> is_aborted_ = false;
};
} // namespace muse::audio::soundtrack
