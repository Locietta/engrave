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

#include "soundtrackwriter.h"

#include <algorithm>
#include <cstdint>

#include "../offlinecancellation.h"
#include "../offlinetiming.h"
#include "aacencoder.h"
#include "audio/common/audioerrors.h"
#include "flacencoder.h"
#include "global/defer.h"
#include "log.h"
#include "mp3encoder.h"
#include "oggencoder.h"
#include "wavencoder.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::soundtrack;

static encode::AbstractAudioEncoderPtr create_encoder(const SoundTrackFormat &format, io::IODevice &dst_device) {
    switch (format.type) {
        case SoundTrackType::MP3: return std::make_unique<encode::Mp3Encoder>(format, dst_device);
        case SoundTrackType::OGG: return std::make_unique<encode::OggEncoder>(format, dst_device);
        case SoundTrackType::FLAC: return std::make_unique<encode::FlacEncoder>(format, dst_device);
        case SoundTrackType::WAV: return std::make_unique<encode::WavEncoder>(format, dst_device);
        case SoundTrackType::AAC: return std::make_unique<encode::AacEncoder>(format, dst_device);
        case SoundTrackType::UNDEFINED: break;
    }

    UNREACHABLE;
    return nullptr;
}

SoundTrackWriter::SoundTrackWriter(io::IODevice &dst_device, const SoundTrackFormat &format, const secs_t total_duration,
                                   engine::OfflineRenderSession *source, std::function<void(uint64_t)> block_completed,
                                   std::function<encode::AbstractAudioEncoderPtr(const SoundTrackFormat &, io::IODevice &)> encoder_factory,
                                   std::function<bool()> cancelled)
    : cancelled_(std::move(cancelled)), source_(std::move(source)), block_completed_(std::move(block_completed)) {
    if (!source_) {
        return;
    }
    IF_ASSERT_FAILED(format.is_valid()) { return; }

    const OutputSpec &output_spec = format.output_spec;

    auto duration_to_samples = [&](const secs_t &duration) {
        const double sec = std::max(0.0, duration.raw());
        return static_cast<samples_t>(std::llround(sec * static_cast<double>(output_spec.sample_rate)));
    };

    data_samples_ = duration_to_samples(total_duration);
    leading_silence_samples_ = duration_to_samples(format.leading_silence_duration);
    const samples_t trailing_silence_samples = duration_to_samples(format.trailing_silence_duration);
    total_samples_ = leading_silence_samples_ + data_samples_ + trailing_silence_samples;

    const samples_t intermediate_samples_number = output_spec.samples_per_channel * output_spec.audio_channel_count;
    interm_buffer_.resize(intermediate_samples_number);
    render_step_ = output_spec.samples_per_channel;

    OfflineTimer timer(OfflineTiming::current().encode);
    encoder_ptr_ = encoder_factory ? encoder_factory(format, dst_device) : create_encoder(format, dst_device);
    if (!encoder_ptr_) {
        return;
    }

    if (poll_cancellation() || !encoder_ptr_->begin(total_samples_)) {
        encoder_ptr_.reset();
    }
}

Ret SoundTrackWriter::write() {
    TRACEFUNC;

    if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
    if (!source_ || !encoder_ptr_) {
        return false;
    }

    source_->set_mode(ProcessMode::PLAYING_OFFLINE);

    bool finalized = false;
    DEFER {
        // Keep cleanup on an exception, but do not discard the normal result.
        if (!is_aborted_ && !finalized) {
            OfflineTimer timer(OfflineTiming::current().encode);
            encoder_ptr_->end();
        }

        is_aborted_ = false;
    };

    Ret ret = write_streaming();
    if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
    if (!is_aborted_) {
        finalized = true;
        OfflineTimer timer(OfflineTiming::current().encode);
        OfflineTimer flush_timer(OfflineTiming::current().flush);
        const Ret final_ret = encoder_ptr_->end();
        if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
        // Encoding may already have failed; preserve that first error.
        if (ret && !final_ret) return final_ret;
    }
    return ret;
}

bool SoundTrackWriter::poll_cancellation() {
    if (cancelled_ && cancelled_()) is_aborted_ = true;
    return is_aborted_;
}

void SoundTrackWriter::abort() { is_aborted_ = true; }

Ret SoundTrackWriter::write_streaming() {
    TRACEFUNC;
    if (total_samples_ == 0) {
        LOGI() << "No audio to export";
        return make_ret(Err::NO_AUDIO_TO_EXPORT);
    }

    samples_t frames_written = 0;

    // Phase 1: leading silence
    const samples_t leading_end = leading_silence_samples_;
    while (frames_written < leading_end && !poll_cancellation()) {
        const samples_t chunk = static_cast<samples_t>(std::min<uint64_t>(render_step_, leading_end - frames_written));

        std::fill(interm_buffer_.begin(), interm_buffer_.end(), 0.f);

        if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
        const size_t encoded = [&] {
            OfflineTimer timer(OfflineTiming::current().encode);
            OfflineCancellation::EncodingScope phase;
            return encoder_ptr_->encode(chunk, interm_buffer_.data());
        }();
        if (encoded == 0) {
            return make_ret(Err::ERROR_ENCODE);
        }

        frames_written += chunk;
        if (block_completed_) block_completed_(frames_written);
    }

    // Phase 2: actual audio data
    const samples_t audio_end = leading_silence_samples_ + data_samples_;
    while (frames_written < audio_end && !poll_cancellation()) {
        const samples_t chunk = static_cast<samples_t>(std::min<uint64_t>(render_step_, audio_end - frames_written));

        //! NOTE The mixer mixes additively and relies on the caller to zero the output buffer
        //! (real time does this via AudioEngine::fillSilent), so clear it before each block.
        std::fill(interm_buffer_.begin(), interm_buffer_.end(), 0.f);

        {
            OfflineTimer timer(OfflineTiming::current().render);
            source_->process(interm_buffer_.data(), chunk);
        }

        if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
        const size_t encoded = [&] {
            OfflineTimer timer(OfflineTiming::current().encode);
            OfflineCancellation::EncodingScope phase;
            return encoder_ptr_->encode(chunk, interm_buffer_.data());
        }();
        if (encoded == 0) {
            return make_ret(Err::ERROR_ENCODE);
        }

        frames_written += chunk;
        if (block_completed_) block_completed_(frames_written);
    }

    // Phase 3: trailing silence
    while (frames_written < total_samples_ && !poll_cancellation()) {
        const samples_t chunk = static_cast<samples_t>(std::min<uint64_t>(render_step_, total_samples_ - frames_written));

        std::fill(interm_buffer_.begin(), interm_buffer_.end(), 0.f);

        if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
        const size_t encoded = [&] {
            OfflineTimer timer(OfflineTiming::current().encode);
            OfflineCancellation::EncodingScope phase;
            return encoder_ptr_->encode(chunk, interm_buffer_.data());
        }();
        if (encoded == 0) {
            return make_ret(Err::ERROR_ENCODE);
        }

        frames_written += chunk;
        if (block_completed_) block_completed_(frames_written);
    }

    if (is_aborted_) {
        return make_ret(Ret::Code::CANCEL);
    }

    return muse::make_ok();
}
