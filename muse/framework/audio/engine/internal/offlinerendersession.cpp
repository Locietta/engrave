// SPDX-License-Identifier: GPL-3.0-only
#include "offlinerendersession.h"

#include <algorithm>
#include <chrono>

#include "audio/common/audioerrors.h"
#include "denormalguard.h"
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
    std::vector<float>().swap(reverb_.buffer);
}

void OfflineRenderSession::initialize(Track &track, const TrackParams &params) {
    track.buffer.resize(spec_.samples_per_channel * spec_.audio_channel_count);
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
    for (auto &track : tracks_) track.source->set_mode(mode);
}

void OfflineRenderSession::set_parallelism(WorkerPool *pool, std::size_t threads) {
    pool_ = pool;
    threads_ = std::max<std::size_t>(threads, 1);
    granted_workers_.reset();
}

std::size_t OfflineRenderSession::active_tracks() const {
    return static_cast<std::size_t>(std::ranges::count_if(tracks_, [](const Track &track) { return !track.control.muted(); }));
}

std::size_t OfflineRenderSession::synthesis_threads() const {
    const std::size_t active = active_tracks();
    if (!pool_ || threads_ <= 1 || active <= 1) return 1;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    // Captures write shared streams in track order and phase-tagged allocation counts assume one thread.
    if (test::PlaybackCapture::synth || test::PlaybackCapture::track || test::AllocationMetrics::enabled.load() || fail_after_tracks >= 0)
        return 1;
#endif
    std::size_t effective = std::min(threads_, active);
    if (granted_workers_) {
        if (*granted_workers_ == 0) return 1;
        effective = std::min(effective, *granted_workers_);
    }
    return effective;
}

void OfflineRenderSession::process_track(Track &track, samples_t frames) {
    if (!track.control.muted()) {
        if (track.source) track.source->process(track.buffer.data(), frames);
        for (auto &effect : track.effects)
            if (effect->active()) effect->process(track.buffer.data(), frames, 0);
        track.control.process(track.buffer.data(), frames);
    }
    track.signal.process(track.buffer.data(), frames);
}

void OfflineRenderSession::synthesize(samples_t frames) {
    using Clock = std::chrono::steady_clock;
    const size_t size = frames * spec_.audio_channel_count;
    // Growing a buffer allocates; keep that on the calling thread.
    for (auto &track : tracks_) {
        track.processed = false;
        if (track.buffer.size() < size) track.buffer.resize(size);
    }
    std::size_t threads = synthesis_threads();
    // If the OS grants fewer workers than requested (or none), record the actual
    // count; 0 falls back to the ordinary serial loop so the caller's live
    // cancellation scope stays active inside each track.
    if (threads > 1 && !granted_workers_) {
        granted_workers_ = pool_->ensure_workers(threads);
        threads = synthesis_threads();
    }
    const bool measure = threads > 1 || OfflineTiming::enabled();
    const auto render = [&](Track &track) {
        std::fill_n(track.buffer.begin(), size, 0.f);
        const auto start = measure ? Clock::now() : Clock::time_point();
        process_track(track, frames);
        if (measure) {
            track.last_seconds = std::chrono::duration<double>(Clock::now() - start).count();
            track.synth_seconds += track.last_seconds;
        }
        track.processed = true;
    };
    OfflineTimer timer(OfflineTiming::current().synth);
    if (threads <= 1) {
        for (auto &track : tracks_) {
            if (OfflineCancellation::requested()) return;
            render(track);
        }
        return;
    }
    if (OfflineCancellation::requested()) return;
    // A block lasts as long as its slowest active track, so start the active tracks that were
    // slowest in the previous block first. Muted tracks only clear their signal state and stay
    // on the calling thread. Order affects scheduling only: each track still renders alone.
    order_.clear();
    order_.reserve(tracks_.size());
    for (std::size_t i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].control.muted())
            render(tracks_[i]);
        else
            order_.push_back(i);
    }
    std::sort(order_.begin(), order_.end(), [&](std::size_t a, std::size_t b) {
        return tracks_[a].last_seconds != tracks_[b].last_seconds ? tracks_[a].last_seconds > tracks_[b].last_seconds : a < b;
    });
    // Capture the coordinator's active FP mode (including rounding mode) for this block so
    // persistent workers match serial synthesis bit-for-bit regardless of pool history.
    const auto fp_mode = ScopedDenormalGuard::capture_mode();
    // The caller keeps polling the owner's cancellation while it waits; tasks see only the pool's flag.
    OfflineCancellation::SynthesisScope phase;
    const auto task = [&](std::size_t index) {
        const ScopedDenormalGuard::WorkerScope fp_guard(fp_mode);
        const auto stopping = [this] { return pool_->stopping(); };
        OfflineCancellation cancellation(stopping);
        render(tracks_[order_[index]]);
    };
    const auto poll = [] { return OfflineCancellation::requested(); };
    pool_->run(threads, order_.size(), task, poll);
}

void OfflineRenderSession::mix(float *out, samples_t frames) {
    const size_t size = frames * spec_.audio_channel_count;
    synthesize(frames);
    if (OfflineCancellation::requested()) return;
    reverb_.processed = false;
    if (has_reverb_) {
        reverb_.buffer.resize(size);
        std::fill_n(reverb_.buffer.begin(), size, 0.f);
    }
    for (auto &track : tracks_) {
        if (OfflineCancellation::requested()) return;
        if (!track.processed || track.signal.is_silent()) continue;
        for (size_t i = 0; i < size; ++i) out[i] += track.buffer[i];
        if (!has_reverb_ || track.sends.empty()) continue;
        const auto &send = track.sends[0];
        if (!send.active || muse::is_zero(send.signal_amount)) continue;
        for (size_t i = 0; i < size; ++i) reverb_.buffer[i] += track.buffer[i] * send.signal_amount;
        reverb_.processed = true;
    }
    if (has_reverb_) {
        if (OfflineCancellation::requested()) return;
        if (reverb_.processed || !reverb_.signal.is_silent()) {
            process_track(reverb_, frames);
            if (!reverb_.signal.is_silent())
                for (size_t i = 0; i < size; ++i) out[i] += reverb_.buffer[i];
        }
    }
    if (has_aux2_ && OfflineCancellation::requested()) return;
}

void OfflineRenderSession::process(float *out, samples_t frames) {
    // A cancelled mixer pull still passed through both master stages in the graph.
    mix(out, frames);
    session_master_.process(out, frames);
    session_signal_.process(out, frames);
    outer_master_.process(out, frames);
    outer_signal_.process(out, frames);
}
