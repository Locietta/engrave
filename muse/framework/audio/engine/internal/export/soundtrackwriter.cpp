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
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>
#include <type_traits>

#include "../denormalguard.h"
#include "../offlinecancellation.h"
#include "../offlinetiming.h"
#include "audio/common/audioerrors.h"
#include "flacencoder.h"
#include "global/defer.h"
#include "global/pollwait.h"
#include "log.h"
#include "mp3encoder.h"
#include "oggencoder.h"
#include "wavencoder.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::soundtrack;

static encode::UniqueEncoder create_default_encoder(const SoundTrackFormat &format, io::IODevice &dst_device) {
    switch (format.type) {
        case SoundTrackType::MP3: return std::make_unique<encode::Mp3Encoder>(format, dst_device);
        case SoundTrackType::OGG: return std::make_unique<encode::OggEncoder>(format, dst_device);
        case SoundTrackType::FLAC: return std::make_unique<encode::FlacEncoder>(format, dst_device);
        case SoundTrackType::WAV: return std::make_unique<encode::WavEncoder>(format, dst_device);
        case SoundTrackType::UNDEFINED: break;
    }

    UNREACHABLE;
    return {};
}

// Encodes each block on the calling thread as soon as it is rendered.
struct SoundTrackWriter::SerialSink {
    SoundTrackWriter &writer;

    float *acquire(bool) { return writer.interm_buffer_.data(); }

    bool commit(samples_t chunk, bool, samples_t frames_written) {
        const size_t encoded = [&] {
            OfflineTimer timer(OfflineTiming::current().encode);
            OfflineCancellation::EncodingScope phase;
            return writer.encoder_ptr_->encode(chunk, writer.interm_buffer_.data());
        }();
        if (encoded == 0) return false;
        if (writer.block_completed_) writer.block_completed_(frames_written);
        return true;
    }
};

// A single-producer/single-consumer ring between the calling thread, which renders blocks into free
// slots, and an encoder thread that owns the encoder for its whole life. Silent blocks carry only a
// flag; the encoder reads them from the writer's zeroed buffer.
struct SoundTrackWriter::AsyncEncoder {
    static constexpr std::size_t k_slots = 4;
    enum class Outcome { ABORTED, NO_ENCODER, ENCODE_FAILED, END_FAILED, OK };

    struct Slot {
        std::vector<float> samples;
        samples_t frames = 0;
        bool silent = false;
        // Calling thread only: the frame count block_completed reports for this block.
        samples_t frames_written = 0;
    };

    explicit AsyncEncoder(SoundTrackWriter &writer) : writer_(writer), stop_(*writer.encoder_stop_) {
        for (auto &slot : slots_) slot.samples.resize(writer.interm_buffer_.size());
    }

    ~AsyncEncoder() {
        if (!thread_.joinable()) return;
        request_stop();
        thread_.join();
    }

    AsyncEncoder(const AsyncEncoder &) = delete;
    AsyncEncoder &operator=(const AsyncEncoder &) = delete;

    bool start() {
        stop_.store(false, std::memory_order_relaxed);
        // The encoder computes in the render's pinned FP mode, as it would on the calling thread.
        const ScopedDenormalGuard::Mode mode = ScopedDenormalGuard::capture_mode();
        try {
            thread_ = std::thread([this, mode] { run(mode); });
        } catch (const std::system_error &) {
            return false;
        }
        return true;
    }

    // Null once the encoder thread has ended early or the caller cancelled.
    float *acquire(bool) {
        std::unique_lock lock(mutex_);
        for (;;) {
            report(lock);
            if (done_) return nullptr;
            if (produced_ - consumed_ < k_slots) return slots_[produced_ % k_slots].samples.data();
            wait(lock);
            lock.unlock();
            const bool cancelled = poll_encoding();
            lock.lock();
            if (cancelled) return nullptr;
        }
    }

    bool commit(samples_t chunk, bool silent, samples_t frames_written) {
        // produced_ changes only on this thread, so reading it unlocked is safe.
        Slot &slot = slots_[produced_ % k_slots];
        slot.frames = chunk;
        slot.silent = silent;
        slot.frames_written = frames_written;
        {
            const std::scoped_lock lock(mutex_);
            ++produced_;
        }
        work_cv_.notify_one();
        return true;
    }

    // With `drain`, lets the encoder finish the queued blocks and end the stream, polling
    // cancellation at least once as the calling thread would while encoding; otherwise stops it.
    // Returns once the encoder thread has joined.
    void finish(bool drain) {
        if (drain) {
            {
                const std::scoped_lock lock(mutex_);
                closed_ = true;
            }
            work_cv_.notify_one();
        } else {
            request_stop();
        }
        std::unique_lock lock(mutex_);
        for (;;) {
            report(lock);
            if (!stop_.load(std::memory_order_relaxed)) {
                lock.unlock();
                if (poll_encoding()) request_stop();
                lock.lock();
            }
            if (done_) break;
            wait(lock);
        }
        lock.unlock();
        thread_.join();
        if (OfflineTiming::enabled()) {
            auto &timing = OfflineTiming::current();
            timing.encode += encode_seconds_;
            timing.flush += flush_seconds_;
        }
    }

    // Valid after finish().
    Outcome outcome() const { return outcome_; }

    const Ret &end_ret() const { return end_ret_; }

    std::exception_ptr error() const { return error_; }
private:
    // Waiting on the encoder is this thread's share of encoding.
    bool poll_encoding() {
        OfflineCancellation::EncodingScope phase;
        return writer_.poll_cancellation();
    }

    void request_stop() {
        stop_.store(true, std::memory_order_relaxed);
        { const std::scoped_lock lock(mutex_); }
        work_cv_.notify_all();
    }

    void wait(std::unique_lock<std::mutex> &lock) {
        using Clock = std::chrono::steady_clock;
        const auto start = OfflineTiming::enabled() ? Clock::now() : Clock::time_point();
        progress_.wait(lock);
        if (OfflineTiming::enabled()) OfflineTiming::current().encode_wait += std::chrono::duration<double>(Clock::now() - start).count();
    }

    // Reports newly encoded blocks in order, outside the lock.
    void report(std::unique_lock<std::mutex> &lock) {
        while (reported_ < consumed_) {
            const samples_t frames = slots_[reported_ % k_slots].frames_written;
            ++reported_;
            if (!writer_.block_completed_) continue;
            lock.unlock();
            writer_.block_completed_(frames);
            lock.lock();
        }
    }

    void run(const ScopedDenormalGuard::Mode mode) {
        const ScopedDenormalGuard::WorkerScope fp_guard(mode);
        const auto stopping = [this] { return stop_.load(std::memory_order_relaxed); };
        OfflineCancellation cancellation(stopping);
        OfflineCancellation::EncodingScope phase;
        Outcome outcome = Outcome::ABORTED;
        Ret end_ret;
        std::exception_ptr error;
        try {
            outcome = encode_all(end_ret);
        } catch (...) {
            error = std::current_exception();
        }
        {
            const std::scoped_lock lock(mutex_);
            outcome_ = outcome;
            end_ret_ = std::move(end_ret);
            error_ = std::move(error);
            encode_seconds_ = OfflineTiming::current().encode;
            flush_seconds_ = OfflineTiming::current().flush;
            done_ = true;
        }
        progress_.signal();
    }

    Outcome encode_all(Ret &end_ret) {
        auto &timing = OfflineTiming::current();
        encode::AudioEncoder encoder;
        {
            OfflineTimer timer(timing.encode);
            if (!writer_.make_encoder(encoder)) return Outcome::NO_ENCODER;
            if (stop_.load(std::memory_order_relaxed)) return Outcome::ABORTED;
            if (!encoder->begin(writer_.total_samples_)) return Outcome::NO_ENCODER;
        }
        for (;;) {
            const Slot *slot = nullptr;
            {
                std::unique_lock lock(mutex_);
                work_cv_.wait(lock, [&] { return produced_ != consumed_ || closed_ || stop_.load(std::memory_order_relaxed); });
                if (stop_.load(std::memory_order_relaxed)) return Outcome::ABORTED;
                if (produced_ == consumed_) break;
                slot = &slots_[consumed_ % k_slots];
            }
            const float *input = slot->silent ? writer_.interm_buffer_.data() : slot->samples.data();
            // As on the calling thread, a failed or throwing stream is still ended unless cancelled;
            // keep the primary encode failure or exception if finalization itself throws.
            const auto end_failed_stream = [&] {
                if (stop_.load(std::memory_order_relaxed)) return;
                try {
                    OfflineTimer timer(timing.encode);
                    encoder->end();
                } catch (...) {}
            };

            struct FailedStreamGuard {
                const decltype(end_failed_stream) &fn;
                bool ok = false;

                ~FailedStreamGuard() noexcept {
                    if (!ok) fn();
                }
            } guard{end_failed_stream};

            size_t encoded = 0;
            {
                OfflineTimer timer(timing.encode);
                encoded = encoder->encode(slot->frames, input);
            }
            if (encoded == 0) {
                return Outcome::ENCODE_FAILED;
            }
            guard.ok = true;
            {
                const std::scoped_lock lock(mutex_);
                ++consumed_;
            }
            progress_.signal();
        }
        OfflineTimer timer(timing.encode);
        OfflineTimer flush_timer(timing.flush);
        end_ret = encoder->end();
        return end_ret ? Outcome::OK : Outcome::END_FAILED;
    }

    SoundTrackWriter &writer_;
    std::atomic<bool> &stop_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable work_cv_;
    muse::PollWait progress_;
    std::array<Slot, k_slots> slots_;
    // Guarded by mutex_.
    std::uint64_t produced_ = 0, consumed_ = 0;
    bool closed_ = false, done_ = false;
    Outcome outcome_ = Outcome::ABORTED;
    Ret end_ret_;
    std::exception_ptr error_;
    double encode_seconds_ = 0, flush_seconds_ = 0;
    // Calling thread only.
    std::uint64_t reported_ = 0;
};

SoundTrackWriter::SoundTrackWriter(io::IODevice &dst_device, const SoundTrackFormat &format, const secs_t total_duration,
                                   engine::OfflineRenderSession *source, muse::UniqueFn<void(uint64_t) const> block_completed,
                                   muse::FnRef<encode::UniqueEncoder(const SoundTrackFormat &, io::IODevice &) const> encoder_factory,
                                   muse::UniqueFn<bool() const> cancelled, std::atomic<bool> *encoder_stop)
    : cancelled_(std::move(cancelled)), source_(std::move(source)), block_completed_(std::move(block_completed)), dst_device_(&dst_device),
      format_(format), encoder_factory_(encoder_factory) {
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

    // The encoder thread creates its own encoder in write().
    if (encoder_stop && total_samples_ > 0) {
        encoder_stop_ = encoder_stop;
        return;
    }
    create_encoder();
}

SoundTrackWriter::~SoundTrackWriter() = default;

bool SoundTrackWriter::make_encoder(encode::AudioEncoder &encoder) const {
    encoder = (encoder_factory_ ? encoder_factory_(format_, *dst_device_) : create_default_encoder(format_, *dst_device_)).take();
    return static_cast<bool>(encoder);
}

void SoundTrackWriter::create_encoder() {
    OfflineTimer timer(OfflineTiming::current().encode);
    if (!make_encoder(encoder_ptr_)) return;
    if (poll_cancellation() || !encoder_ptr_->begin(total_samples_)) {
        encoder_ptr_.reset();
    }
}

Ret SoundTrackWriter::write() {

    if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
    if (!source_) {
        return false;
    }
    if (encoder_stop_) {
        if (std::optional<Ret> ret = write_async()) return std::move(*ret);
        // No encoder thread could start: encode here instead.
        encoder_stop_ = nullptr;
        create_encoder();
    }
    if (!encoder_ptr_) {
        return false;
    }

    source_->set_mode(ProcessMode::PLAYING_OFFLINE);

    bool finalized = false;
    DEFER {
        source_->cancel_prefetch();
        // Keep cleanup on an exception without terminating if finalization throws.
        if (!is_aborted_ && !finalized) {
            try {
                OfflineTimer timer(OfflineTiming::current().encode);
                encoder_ptr_->end();
            } catch (...) {}
        }

        is_aborted_ = false;
    };

    SerialSink sink{*this};
    Ret ret = write_streaming(sink);
    if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
    if (!is_aborted_) {
        finalized = true;
        OfflineTimer timer(OfflineTiming::current().encode);
        OfflineTimer flush_timer(OfflineTiming::current().flush);
        Ret final_ret;
        if (!ret) {
            try {
                final_ret = encoder_ptr_->end();
            } catch (...) {
                // Encoding already failed; preserve that first error.
            }
        } else {
            final_ret = encoder_ptr_->end();
        }
        if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
        // Encoding may already have failed; preserve that first error.
        if (ret && !final_ret) return final_ret;
    }
    return ret;
}

std::optional<Ret> SoundTrackWriter::write_async() {
    AsyncEncoder encoder(*this);
    if (!encoder.start()) return std::nullopt;

    source_->set_mode(ProcessMode::PLAYING_OFFLINE);
    // Declared after the encoder, so unwinding drains prefetched synthesis before the encoder joins.
    DEFER {
        source_->cancel_prefetch();
        is_aborted_ = false;
    };

    const Ret ret = write_streaming(encoder);
    encoder.finish(static_cast<bool>(ret));
    if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
    if (const std::exception_ptr error = encoder.error()) std::rethrow_exception(error);
    switch (encoder.outcome()) {
        case AsyncEncoder::Outcome::NO_ENCODER: return Ret(false);
        case AsyncEncoder::Outcome::ENCODE_FAILED: return make_ret(Err::ERROR_ENCODE);
        case AsyncEncoder::Outcome::END_FAILED: return encoder.end_ret();
        case AsyncEncoder::Outcome::ABORTED:
        case AsyncEncoder::Outcome::OK: break;
    }
    return ret;
}

bool SoundTrackWriter::poll_cancellation() {
    if (cancelled_ && cancelled_()) is_aborted_ = true;
    return is_aborted_;
}

void SoundTrackWriter::abort() { is_aborted_ = true; }

template <class Sink>
Ret SoundTrackWriter::write_streaming(Sink &sink) {
    if (total_samples_ == 0) {
        LOGI() << "No audio to export";
        return make_ret(Err::NO_AUDIO_TO_EXPORT);
    }

    samples_t frames_written = 0;
    const samples_t audio_end = leading_silence_samples_ + data_samples_;
    const size_t block_size = interm_buffer_.size();

    // Leading silence, the actual audio data, then trailing silence.
    const struct {
        samples_t end;
        bool silent;
    } phases[] = {{leading_silence_samples_, true}, {audio_end, false}, {total_samples_, true}};

    for (const auto &phase : phases) {
        while (frames_written < phase.end && !poll_cancellation()) {
            const samples_t chunk = static_cast<samples_t>(std::min<uint64_t>(render_step_, phase.end - frames_written));

            float *buffer = sink.acquire(phase.silent);
            // The encoder ended early; the caller reports its outcome.
            if (!buffer) return poll_cancellation() ? make_ret(Ret::Code::CANCEL) : make_ret(Err::ERROR_ENCODE);

            if (!phase.silent || std::is_same_v<Sink, SerialSink>) {
                //! NOTE The mixer mixes additively and relies on the caller to zero the output buffer
                //! (real time does this via AudioEngine::fillSilent), so clear it before each block.
                std::fill_n(buffer, block_size, 0.f);
            }
            if (!phase.silent) {
                // Never prefetch past the audio data, so the final partial block keeps its size.
                const samples_t next_start = frames_written + chunk;
                const samples_t next = next_start < audio_end ? std::min<samples_t>(render_step_, audio_end - next_start) : 0;
                OfflineTimer timer(OfflineTiming::current().render);
                source_->process(buffer, chunk, next);
            }

            if (poll_cancellation()) return make_ret(Ret::Code::CANCEL);
            if (!sink.commit(chunk, phase.silent, frames_written + chunk)) {
                return make_ret(Err::ERROR_ENCODE);
            }
            frames_written += chunk;
        }
    }

    if (is_aborted_) {
        return make_ret(Ret::Code::CANCEL);
    }

    return muse::make_ok();
}
