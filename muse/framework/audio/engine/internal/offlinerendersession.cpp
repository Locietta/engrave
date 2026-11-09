// SPDX-License-Identifier: GPL-3.0-only
#include "offlinerendersession.h"

#include "audio/common/audioerrors.h"
#include "offlinecancellation.h"
#include "offlinetiming.h"
using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

#ifdef ENGRAVE_PLAYBACK_HARNESS
thread_local int OfflineRenderSession::fail_after_tracks = -1;
#endif

OfflineRenderSession::OfflineRenderSession(AudioFactory &, const OutputSpec &spec, AudioResources &resources,
                                           const IAudioEngineConfiguration &config, MidiEventSink sink)
    : resources_(resources), config_(config), sink_(std::move(sink)), spec_(spec) {
    session_master_.set_playhead_position(nullptr);
    session_master_.set_output_spec(spec);
    outer_master_.set_output_spec(spec);
    session_signal_.set_output_spec(spec);
    outer_signal_.set_output_spec(spec);
}

OfflineRenderSession::~OfflineRenderSession() {
    OfflineTimer timer(OfflineTiming::current().teardown);
    set_mode(ProcessMode::IDLE);
    tracks_.clear();
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

void OfflineRenderSession::process_track(Track &track, samples_t frames) {
    if (!track.control.muted()) {
        if (track.source) track.source->process(track.buffer.data(), frames);
        for (auto &effect : track.effects)
            if (effect->active()) effect->process(track.buffer.data(), frames, 0);
        track.control.process(track.buffer.data(), frames);
    }
    track.signal.process(track.buffer.data(), frames);
}

void OfflineRenderSession::process(float *out, samples_t frames) {
    const size_t size = frames * spec_.audio_channel_count;
    // Synthesis is separate from fixed-order mixing, ready for a later worker pass.
    for (auto &track : tracks_) {
        if (OfflineCancellation::requested()) return;
        track.processed = false;
        if (track.buffer.size() < size) track.buffer.resize(size);
        std::fill_n(track.buffer.begin(), size, 0.f);
        process_track(track, frames);
        track.processed = true;
    }
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
    session_master_.process(out, frames);
    session_signal_.process(out, frames);
    outer_master_.process(out, frames);
    outer_signal_.process(out, frames);
}
