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
#include "../offlinetiming.h"
#include "../offlinecancellation.h"

#include <algorithm>
#include <cstdint>

#include "global/defer.h"

#include "audio/common/audioerrors.h"

#include "mp3encoder.h"
#include "oggencoder.h"
#include "flacencoder.h"
#include "wavencoder.h"
#include "aacencoder.h"

#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::soundtrack;

static encode::AbstractAudioEncoderPtr createEncoder(const SoundTrackFormat &format, io::IODevice &dstDevice) {
    switch (format.type) {
        case SoundTrackType::MP3: return std::make_unique<encode::Mp3Encoder>(format, dstDevice);
        case SoundTrackType::OGG: return std::make_unique<encode::OggEncoder>(format, dstDevice);
        case SoundTrackType::FLAC: return std::make_unique<encode::FlacEncoder>(format, dstDevice);
        case SoundTrackType::WAV: return std::make_unique<encode::WavEncoder>(format, dstDevice);
        case SoundTrackType::AAC: return std::make_unique<encode::AacEncoder>(format, dstDevice);
        case SoundTrackType::Undefined: break;
    }

    UNREACHABLE;
    return nullptr;
}

SoundTrackWriter::SoundTrackWriter(io::IODevice &dstDevice, const SoundTrackFormat &format, const secs_t totalDuration,
                                   IAudioNodePtr source, std::function<void(uint64_t)> blockCompleted,
                                   std::function<encode::AbstractAudioEncoderPtr(const SoundTrackFormat &, io::IODevice &)> encoderFactory,
                                   std::function<bool()> cancelled)
    : cancelled_(std::move(cancelled)), source_(std::move(source)), blockCompleted_(std::move(blockCompleted)) {
    if (!source_) {
        return;
    }
    IF_ASSERT_FAILED(format.isValid()) { return; }

    const OutputSpec &outputSpec = format.outputSpec;

    auto durationToSamples = [&](const secs_t &duration) {
        const double sec = std::max(0.0, duration.raw());
        return static_cast<samples_t>(std::llround(sec * static_cast<double>(outputSpec.sampleRate)));
    };

    dataSamples_ = durationToSamples(totalDuration);
    leadingSilenceSamples_ = durationToSamples(format.leadingSilenceDuration);
    const samples_t trailingSilenceSamples = durationToSamples(format.trailingSilenceDuration);
    totalSamples_ = leadingSilenceSamples_ + dataSamples_ + trailingSilenceSamples;

    const samples_t intermediateSamplesNumber = outputSpec.samplesPerChannel * outputSpec.audioChannelCount;
    intermBuffer_.resize(intermediateSamplesNumber);
    renderStep_ = outputSpec.samplesPerChannel;

    OfflineTimer timer(OfflineTiming::current().encode);
    encoderPtr_ = encoderFactory ? encoderFactory(format, dstDevice) : createEncoder(format, dstDevice);
    if (!encoderPtr_) {
        return;
    }

    if (pollCancellation() || !encoderPtr_->begin(totalSamples_)) {
        encoderPtr_.reset();
    }
}

Ret SoundTrackWriter::write() {
    TRACEFUNC;

    if (pollCancellation()) return make_ret(Ret::Code::Cancel);
    if (!source_ || !encoderPtr_) {
        return false;
    }

    source_->setOutputSpec(encoderPtr_->format().outputSpec);
    source_->setMode(ProcessMode::PlayingOffline);

    bool finalized = false;
    DEFER {
        // Keep cleanup on an exception, but do not discard the normal result.
        if (!isAborted_ && !finalized) {
            OfflineTimer timer(OfflineTiming::current().encode);
            encoderPtr_->end();
        }

        isAborted_ = false;
    };

    Ret ret = writeStreaming();
    if (pollCancellation()) return make_ret(Ret::Code::Cancel);
    if (!isAborted_) {
        finalized = true;
        OfflineTimer timer(OfflineTiming::current().encode);
        OfflineTimer flushTimer(OfflineTiming::current().flush);
        const Ret finalRet = encoderPtr_->end();
        if (pollCancellation()) return make_ret(Ret::Code::Cancel);
        // Encoding may already have failed; preserve that first error.
        if (ret && !finalRet) return finalRet;
    }
    return ret;
}

bool SoundTrackWriter::pollCancellation() {
    if (cancelled_ && cancelled_()) isAborted_ = true;
    return isAborted_;
}

void SoundTrackWriter::abort() { isAborted_ = true; }

Ret SoundTrackWriter::writeStreaming() {
    TRACEFUNC;
    if (totalSamples_ == 0) {
        LOGI() << "No audio to export";
        return make_ret(Err::NoAudioToExport);
    }

    samples_t framesWritten = 0;

    // Phase 1: leading silence
    const samples_t leadingEnd = leadingSilenceSamples_;
    while (framesWritten < leadingEnd && !pollCancellation()) {
        const samples_t chunk = static_cast<samples_t>(std::min<uint64_t>(renderStep_, leadingEnd - framesWritten));

        std::fill(intermBuffer_.begin(), intermBuffer_.end(), 0.f);

        if (pollCancellation()) return make_ret(Ret::Code::Cancel);
        const size_t encoded = [&] {
            OfflineTimer timer(OfflineTiming::current().encode);
            OfflineCancellation::EncodingScope phase;
            return encoderPtr_->encode(chunk, intermBuffer_.data());
        }();
        if (encoded == 0) {
            return make_ret(Err::ErrorEncode);
        }

        framesWritten += chunk;
        if (blockCompleted_) blockCompleted_(framesWritten);
    }

    // Phase 2: actual audio data
    const samples_t audioEnd = leadingSilenceSamples_ + dataSamples_;
    while (framesWritten < audioEnd && !pollCancellation()) {
        const samples_t chunk = static_cast<samples_t>(std::min<uint64_t>(renderStep_, audioEnd - framesWritten));

        //! NOTE The mixer mixes additively and relies on the caller to zero the output buffer
        //! (real time does this via AudioEngine::fillSilent), so clear it before each block.
        std::fill(intermBuffer_.begin(), intermBuffer_.end(), 0.f);

        {
            OfflineTimer timer(OfflineTiming::current().render);
            source_->process(intermBuffer_.data(), chunk);
        }

        if (pollCancellation()) return make_ret(Ret::Code::Cancel);
        const size_t encoded = [&] {
            OfflineTimer timer(OfflineTiming::current().encode);
            OfflineCancellation::EncodingScope phase;
            return encoderPtr_->encode(chunk, intermBuffer_.data());
        }();
        if (encoded == 0) {
            return make_ret(Err::ErrorEncode);
        }

        framesWritten += chunk;
        if (blockCompleted_) blockCompleted_(framesWritten);
    }

    // Phase 3: trailing silence
    while (framesWritten < totalSamples_ && !pollCancellation()) {
        const samples_t chunk = static_cast<samples_t>(std::min<uint64_t>(renderStep_, totalSamples_ - framesWritten));

        std::fill(intermBuffer_.begin(), intermBuffer_.end(), 0.f);

        if (pollCancellation()) return make_ret(Ret::Code::Cancel);
        const size_t encoded = [&] {
            OfflineTimer timer(OfflineTiming::current().encode);
            OfflineCancellation::EncodingScope phase;
            return encoderPtr_->encode(chunk, intermBuffer_.data());
        }();
        if (encoded == 0) {
            return make_ret(Err::ErrorEncode);
        }

        framesWritten += chunk;
        if (blockCompleted_) blockCompleted_(framesWritten);
    }

    if (isAborted_) {
        return make_ret(Ret::Code::Cancel);
    }

    return muse::make_ok();
}
