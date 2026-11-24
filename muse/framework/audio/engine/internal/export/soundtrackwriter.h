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
#include <memory>
#include <optional>
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
// Renders and encodes on the calling thread, or, given `encoder_stop`, hands
// rendered blocks through a bounded ring to an encoder thread that creates,
// begins, feeds and ends the encoder; that thread and the device then observe
// cancellation only through `*encoder_stop`, which the writer raises. Either
// way `cancelled` and `block_completed` run on the calling thread, the latter
// once per encoded block in order, and write() returns only after the encoder
// thread has joined and any prefetched synthesis has been drained.
struct SoundTrackWriter {

    SoundTrackWriter(io::IODevice &dst_device, const SoundTrackFormat &format, const secs_t total_duration,
                     engine::OfflineRenderSession *source, muse::UniqueFn<void(uint64_t) const> block_completed = {},
                     muse::FnRef<encode::UniqueEncoder(const SoundTrackFormat &, io::IODevice &) const> encoder_factory = {},
                     muse::UniqueFn<bool() const> cancelled = {}, std::atomic<bool> *encoder_stop = nullptr);
    ~SoundTrackWriter();

    Ret write();
    void abort();


private:
    struct SerialSink;
    struct AsyncEncoder;

    template <class Sink>
    Ret write_streaming(Sink &);
    // Empty when no encoder thread could start.
    std::optional<Ret> write_async();
    bool make_encoder(encode::AudioEncoder &) const;
    // Creates and begins the calling thread's encoder; leaves it empty on failure or cancellation.
    void create_encoder();
    bool poll_cancellation();
    muse::UniqueFn<bool() const> cancelled_;

    engine::OfflineRenderSession *source_;
    muse::UniqueFn<void(uint64_t) const> block_completed_;

    io::IODevice *dst_device_ = nullptr;
    SoundTrackFormat format_;
    muse::FnRef<encode::UniqueEncoder(const SoundTrackFormat &, io::IODevice &) const> encoder_factory_;
    std::atomic<bool> *encoder_stop_ = nullptr;

    // Zero-filled while encoding asynchronously, where it serves every silent block.
    std::vector<float> interm_buffer_;
    samples_t render_step_ = 0;
    samples_t leading_silence_samples_ = 0;
    samples_t data_samples_ = 0;
    samples_t total_samples_ = 0;

    encode::AudioEncoder encoder_ptr_;

    std::atomic<bool> is_aborted_ = false;
};
} // namespace muse::audio::soundtrack
