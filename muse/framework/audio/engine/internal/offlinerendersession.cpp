// SPDX-License-Identifier: GPL-3.0-only
#include "offlinerendersession.h"

#include "nodes/automationcontrolnode.h"
#include "nodes/signalnode.h"
#include "offlinetiming.h"
using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

OfflineRenderSession::OfflineRenderSession(IAudioFactory &factory, const OutputSpec &spec)
    : factory_(factory), spec_(spec), mixer_(std::make_shared<Mixer>()),
      master_(std::make_shared<TrackChain>(::muse::audio::master_track_id, "Master")) {
    mixer_->init();
    auto control = std::make_shared<AutomationControlNode>();
    control->set_playhead_position(nullptr);
    master_->set_source(mixer_);
    master_->set_fx_chain(nullptr);
    master_->set_control(control);
    master_->set_signal(std::make_shared<SignalNode>());
    master_->rebuild();
    master_->set_output_spec(spec);
    master_->set_mode(ProcessMode::IDLE);
}

OfflineRenderSession::~OfflineRenderSession() {
    OfflineTimer timer(OfflineTiming::current().teardown);
    master_->set_mode(ProcessMode::IDLE);
    master_->clear();
    master_->disconnect_all();
    master_.reset();
    for (const auto &track : tracks_) {
        if (auto fx = track->fx_chain()) fx->clear();
        track->clear();
        track->disconnect_all();
    }
    tracks_.clear();
    mixer_.reset();
    factory_.clear_synth_sources();
    factory_.clear_all_fx();
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::add_track(const std::string &name, const mpe::PlaybackData &data,
                                                              const TrackParams &params) {
    return add(name, &data, params);
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::add_aux_track(const std::string &name, const TrackParams &params) {
    return add(name, nullptr, params);
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::add(const std::string &name, const mpe::PlaybackData *data, const TrackParams &params) {
    using Result = RetVal2<TrackId, TrackParams>;
    const TrackId id = ++last_id_;
    AudioSourceNodePtr source;
    if (data) {
        auto made = factory_.make_event_source(id, *data, params.source);
        if (!made.ret) return Result::make_ret(made.ret);
        source = made.val;
    }
    auto control = std::make_shared<AutomationControlNode>();
    control->set_playhead_position(nullptr);
    auto chain = std::make_shared<TrackChain>(id, name);
    chain->set_output_spec(spec_);
    chain->set_mode(ProcessMode::IDLE);
    if (source) chain->set_source(source);
    chain->set_fx_chain(nullptr);
    chain->set_control(control);
    chain->set_signal(std::make_shared<SignalNode>());
    chain->rebuild();
    const Ret added = data ? mixer_->add_track(chain, params.aux_sends) : mixer_->add_aux_track(chain);
    if (!added) return Result::make_ret(added);
    auto fx = factory_.make_track_fx_chain(id, params.fx_chain);
    fx->set_playhead_position(nullptr);
    chain->set_fx_chain(fx);
    chain->rebuild();
    control->set_volume(params.control.volume);
    control->set_pan(params.control.balance);
    control->set_muted(params.control.muted);
    if (source) source->seek(TimePosition::zero(spec_.sample_rate));
    tracks_.push_back(chain);
    size_t audible = 0;
    for (const auto &track : tracks_)
        if (!track->control()->muted()) ++audible;
    mixer_->set_non_muted_track_count(audible);
    return Result::make_ok(id, params);
}

void OfflineRenderSession::reset_sources() {
    for (const auto &track : tracks_) {
        if (auto source = std::dynamic_pointer_cast<AudioSourceNode>(track->source())) source->seek(TimePosition::zero(spec_.sample_rate));
    }
}
