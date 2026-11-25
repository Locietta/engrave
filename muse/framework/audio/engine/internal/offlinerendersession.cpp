// SPDX-License-Identifier: GPL-3.0-only
#include "offlinerendersession.h"

#include <algorithm>
#include <chrono>

#include "audio/common/audioerrors.h"
#include "log.h"
#include "offlinecancellation.h"
#include "offlinetiming.h"
#include "playbackcapture.h"
#include "workerpool.h"
using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

#ifdef ENGRAVE_PLAYBACK_HARNESS
thread_local int OfflineRenderSession::fail_after_tracks = -1;
#endif

OfflineRenderSession::OfflineRenderSession(const OutputSpec &spec, AudioResources &resources, const OfflineRenderSettings &config,
                                           muse::UniqueFn<void(const midi::Event &) const> sink)
    : resources_(resources), config_(config), sink_(std::move(sink)), spec_(spec) {
    session_master_.set_playhead_position(nullptr);
    session_master_.set_output_spec(spec);
    outer_master_.set_output_spec(spec);
    session_signal_.set_output_spec(spec);
    outer_signal_.set_output_spec(spec);
}

OfflineRenderSession::~OfflineRenderSession() {
    cancel_prefetch();
    if (OfflineTiming::enabled()) {
        auto &timing = OfflineTiming::current();
        for (const auto &track : tracks_) {
            timing.synth_total += track.synth_seconds;
            timing.synth_max_track = std::max(timing.synth_max_track, track.synth_seconds);
        }
        timing.synth_threads = synthesis_threads();
        timing.synth_tracks = tracks_.size();
    }
    OfflineTimer timer(OfflineTiming::current().teardown);
    set_mode(ProcessMode::IDLE);
    tracks_.clear();
    // Account for the aux DSP teardown in the same phase as the former graph.
    reverb_.effects.clear();
    std::vector<float>().swap(reverb_.slots[0].buffer);
}

void OfflineRenderSession::initialize(Track &track, const TrackParams &params) {
    track.slots[0].buffer.resize(spec_.samples_per_channel * spec_.audio_channel_count);
    track.control.set_playhead_position(nullptr);
    track.control.set_output_spec(spec_);
    track.signal.set_output_spec(spec_);
    for (const auto &entry : params.fx_chain) {
        const auto &fx = entry.second;
        if (!fx.is_valid() || fx.resource_meta.id != muse_reverb_id) continue;
        auto effect = std::make_unique<fx::ReverbProcessor>(fx);
        effect->init(spec_);
        effect->set_output_spec(spec_);
        effect->set_mode(ProcessMode::IDLE);
        track.effects.push_back(std::move(effect));
    }
    track.control.set_volume(params.control.volume);
    track.control.set_pan(params.control.balance);
    track.control.set_muted(params.control.muted);
    track.sends = params.aux_sends;
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::add_track(const std::string &, const mpe::TrackEvents &events,
                                                              const TrackParams &params) {
    using Result = RetVal2<TrackId, TrackParams>;
    Track track;
    track.source = std::make_unique<FluidTrackSource>(resources_, config_, spec_, sink_, events);
    track.source->set_output_spec(spec_);
    track.source->apply_input_params(params.source);
    if (!track.source->initialized()) return Result::make_ret(make_ret(Err::INVALID_SYNTH));
    track.source->set_mode(ProcessMode::IDLE);
    initialize(track, params);
    track.source->seek(TimePosition::zero(spec_.sample_rate));
#ifdef ENGRAVE_PLAYBACK_HARNESS
    if (fail_after_tracks == static_cast<int>(tracks_.size())) throw std::bad_alloc();
#endif
    tracks_.push_back(std::move(track));
    return Result::make_ok(++last_id_, params);
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::add_aux_track(const std::string &, const TrackParams &params) {
    if (!has_reverb_) {
        initialize(reverb_, params);
        has_reverb_ = true;
    } else {
        has_aux2_ = true;
    }
    return RetVal2<TrackId, TrackParams>::make_ok(++last_id_, params);
}

void OfflineRenderSession::reset_sources() {
    for (auto &track : tracks_) track.source->seek(TimePosition::zero(spec_.sample_rate));
}

void OfflineRenderSession::set_mode(ProcessMode mode) {
    if (mode != ProcessMode::PLAYING_OFFLINE) cancel_prefetch();
    for (auto &track : tracks_) track.source->set_mode(mode);
}

void OfflineRenderSession::cancel_prefetch() noexcept {
    if (!prefetched_frames_) return;
    pool_->abandon();
    prefetched_frames_ = 0;
}

void OfflineRenderSession::set_parallelism(WorkerPool *pool, std::size_t threads) {
    pool_ = pool;
    threads_ = std::max<std::size_t>(threads, 1);
    granted_workers_.reset();
}

bool OfflineRenderSession::serial_only() {
#ifdef ENGRAVE_PLAYBACK_HARNESS
    // Captures write shared streams in track order and phase-tagged allocation counts assume one thread.
    return test::PlaybackCapture::synth || test::PlaybackCapture::track || test::AllocationMetrics::enabled.load() ||
           fail_after_tracks >= 0;
#else
    return false;
#endif
}

std::size_t OfflineRenderSession::active_tracks() const {
    return static_cast<std::size_t>(std::ranges::count_if(tracks_, [](const Track &track) { return !track.control.muted(); }));
}

std::size_t OfflineRenderSession::synthesis_threads() const {
    const std::size_t active = active_tracks();
    if (!pool_ || threads_ <= 1 || active <= 1 || serial_only()) return 1;
    std::size_t effective = std::min(threads_, active);
    if (granted_workers_) {
        if (*granted_workers_ == 0) return 1;
        effective = std::min(effective, *granted_workers_);
    }
    return effective;
}

void OfflineRenderSession::process_track(Track &track, float *buffer, samples_t frames) {
    if (!track.control.muted()) {
        if (track.source) track.source->process(buffer, frames);
        for (auto &effect : track.effects)
            if (effect->active()) effect->process(buffer, frames, 0);
        track.control.process(buffer, frames);
    }
    track.signal.process(buffer, frames);
}

void OfflineRenderSession::render_track(Track &track, samples_t frames, int slot, bool measure) {
    using Clock = std::chrono::steady_clock;
    Slot &out = track.slots[slot];
    std::fill_n(out.buffer.begin(), frames * spec_.audio_channel_count, 0.f);
    const auto start = measure ? Clock::now() : Clock::time_point();
    process_track(track, out.buffer.data(), frames);
    if (measure) {
        track.last_seconds = std::chrono::duration<double>(Clock::now() - start).count();
        track.synth_seconds += track.last_seconds;
    }
    out.silent = track.signal.is_silent();
    out.processed = true;
}

void OfflineRenderSession::BlockTask::operator()(std::size_t index) const {
    const ScopedDenormalGuard::WorkerScope fp_guard(self->block_fp_mode_);
    const auto stopping = [pool = self->pool_] { return pool->stopping(); };
    OfflineCancellation cancellation(stopping);
    self->render_track(self->tracks_[self->order_[index]], self->block_frames_, self->block_slot_, self->block_measure_);
}

bool OfflineRenderSession::synthesize(samples_t frames, int slot, bool async) {
    std::size_t threads = synthesis_threads();
    // If the OS grants fewer workers than requested (or none), record the actual
    // count; 0 falls back to the ordinary serial loop so the caller's live
    // cancellation scope stays active inside each track.
    if (threads > 1 && !granted_workers_) {
        granted_workers_ = pool_->ensure_workers(threads);
        threads = synthesis_threads();
    }
    // Check prefetch eligibility before sizing the alternate slot so serial
    // renders never allocate unused second buffers.
    if (async && threads <= 1) return false;
    const size_t size = frames * spec_.audio_channel_count;
    // Growing a buffer allocates; keep that on the calling thread.
    for (auto &track : tracks_) {
        Slot &out = track.slots[slot];
        out.processed = false;
        if (out.buffer.size() < size) out.buffer.resize(size);
    }
    const bool measure = threads > 1 || OfflineTiming::enabled();
    std::optional<OfflineTimer> timer;
    if (!async) timer.emplace(OfflineTiming::current().synth);
    if (threads <= 1) {
        for (auto &track : tracks_) {
            if (OfflineCancellation::requested()) return true;
            render_track(track, frames, slot, measure);
        }
        return true;
    }
    if (!async && OfflineCancellation::requested()) return true;
    // A block lasts as long as its slowest active track, so start the active tracks that were
    // slowest in the previous block first. Muted tracks only clear their signal state and stay
    // on the calling thread. Order affects scheduling only: each track still renders alone.
    order_.clear();
    order_.reserve(tracks_.size());
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].control.muted())
            render_track(tracks_[i], frames, slot, measure);
        else
            order_.push_back(i);
    }
    std::sort(order_.begin(), order_.end(), [&](std::size_t a, std::size_t b) {
        return tracks_[a].last_seconds != tracks_[b].last_seconds ? tracks_[a].last_seconds > tracks_[b].last_seconds : a < b;
    });
    block_frames_ = frames;
    block_slot_ = slot;
    block_measure_ = measure;
    // Capture the coordinator's active FP mode (including rounding mode) for this block so
    // persistent workers match serial synthesis bit-for-bit regardless of pool history.
    block_fp_mode_ = ScopedDenormalGuard::capture_mode();
    if (async) {
        if (OfflineTiming::enabled()) prefetch_start_ = std::chrono::steady_clock::now();
        if (!pool_->start(threads, order_.size(), block_task_)) return false;
        prefetched_frames_ = frames;
        return true;
    }
    // The caller keeps polling the owner's cancellation while it waits; tasks see only the pool's flag.
    OfflineCancellation::SynthesisScope phase;
    const auto poll = [] { return OfflineCancellation::requested(); };
    pool_->run(threads, order_.size(), block_task_, poll);
    return true;
}

void OfflineRenderSession::mix(float *out, samples_t frames, int slot) {
    const size_t size = frames * spec_.audio_channel_count;
    Slot &reverb = reverb_.slots[0];
    reverb.processed = false;
    if (has_reverb_) {
        reverb.buffer.resize(size);
        std::fill_n(reverb.buffer.begin(), size, 0.f);
    }
    for (const auto &track : tracks_) {
        if (OfflineCancellation::requested()) return;
        const Slot &in = track.slots[slot];
        if (!in.processed || in.silent) continue;
        for (size_t i = 0; i < size; ++i) out[i] += in.buffer[i];
        if (!has_reverb_ || track.sends.empty()) continue;
        const auto &send = track.sends[0];
        if (!send.active || muse::is_zero(send.signal_amount)) continue;
        for (size_t i = 0; i < size; ++i) reverb.buffer[i] += in.buffer[i] * send.signal_amount;
        reverb.processed = true;
    }
    if (has_reverb_) {
        if (OfflineCancellation::requested()) return;
        if (reverb.processed || !reverb_.signal.is_silent()) {
            process_track(reverb_, reverb.buffer.data(), frames);
            if (!reverb_.signal.is_silent())
                for (size_t i = 0; i < size; ++i) out[i] += reverb.buffer[i];
        }
    }
    if (has_aux2_ && OfflineCancellation::requested()) return;
}

void OfflineRenderSession::process(float *out, samples_t frames, samples_t next_frames) {
    using Clock = std::chrono::steady_clock;
    if (prefetched_frames_) {
        IF_ASSERT_FAILED(prefetched_frames_ == frames) {
            cancel_prefetch();
            return;
        }
        prefetched_frames_ = 0;
        OfflineCancellation::SynthesisScope phase;
        const auto poll = [] { return OfflineCancellation::requested(); };
        pool_->finish(poll);
        if (OfflineTiming::enabled())
            OfflineTiming::current().synth += std::chrono::duration<double>(Clock::now() - prefetch_start_).count();
    } else {
        synthesize(frames, slot_, false);
    }
    const int slot = slot_;
    const bool cancelled = OfflineCancellation::requested();
    if (!cancelled && next_frames && synthesize(next_frames, slot ^ 1, true)) slot_ = slot ^ 1;
    const auto mix_start = OfflineTiming::enabled() ? Clock::now() : Clock::time_point();
    if (!cancelled) mix(out, frames, slot);
    // A cancelled mixer pull still passed through both master stages in the graph.
    session_master_.process(out, frames);
    session_signal_.process(out, frames);
    outer_master_.process(out, frames);
    outer_signal_.process(out, frames);
    if (OfflineTiming::enabled()) OfflineTiming::current().mix += std::chrono::duration<double>(Clock::now() - mix_start).count();
}
