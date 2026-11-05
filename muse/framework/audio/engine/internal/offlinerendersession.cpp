// SPDX-License-Identifier: GPL-3.0-only
#include "offlinerendersession.h"
#include "offlinetiming.h"
#include "nodes/automationcontrolnode.h"
#include "nodes/signalnode.h"
using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

OfflineRenderSession::OfflineRenderSession(IAudioFactory &factory, const OutputSpec &spec)
    : factory_(factory), spec_(spec), mixer_(std::make_shared<Mixer>()), master_(std::make_shared<TrackChain>(MASTER_TRACK_ID, "Master")) {
    mixer_->init();
    auto control = std::make_shared<AutomationControlNode>();
    control->setPlayheadPosition(nullptr);
    master_->setSource(mixer_);
    master_->setFxChain(nullptr);
    master_->setControl(control);
    master_->setSignal(std::make_shared<SignalNode>());
    master_->rebuild();
    master_->setOutputSpec(spec);
    master_->setMode(ProcessMode::Idle);
}

OfflineRenderSession::~OfflineRenderSession() {
    OfflineTimer timer(OfflineTiming::current().teardown);
    master_->setMode(ProcessMode::Idle);
    master_->clear();
    master_->disconnectAll();
    master_.reset();
    for (const auto &track : tracks_) {
        if (auto fx = track->fxChain()) fx->clear();
        track->clear();
        track->disconnectAll();
    }
    tracks_.clear();
    mixer_.reset();
    factory_.clearSynthSources();
    factory_.clearAllFx();
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::addTrack(const std::string &name, const mpe::PlaybackData &data,
                                                             const TrackParams &params) {
    return add(name, &data, params);
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::addAuxTrack(const std::string &name, const TrackParams &params) {
    return add(name, nullptr, params);
}

RetVal2<TrackId, TrackParams> OfflineRenderSession::add(const std::string &name, const mpe::PlaybackData *data, const TrackParams &params) {
    using Result = RetVal2<TrackId, TrackParams>;
    const TrackId id = ++lastId_;
    AudioSourceNodePtr source;
    if (data) {
        auto made = factory_.makeEventSource(id, *data, params.source);
        if (!made.ret) return Result::make_ret(made.ret);
        source = made.val;
    }
    auto control = std::make_shared<AutomationControlNode>();
    control->setPlayheadPosition(nullptr);
    auto chain = std::make_shared<TrackChain>(id, name);
    chain->setOutputSpec(spec_);
    chain->setMode(ProcessMode::Idle);
    if (source) chain->setSource(source);
    chain->setFxChain(nullptr);
    chain->setControl(control);
    chain->setSignal(std::make_shared<SignalNode>());
    chain->rebuild();
    const Ret added = data ? mixer_->addTrack(chain, params.auxSends) : mixer_->addAuxTrack(chain);
    if (!added) return Result::make_ret(added);
    auto fx = factory_.makeTrackFxChain(id, params.fxChain);
    fx->setPlayheadPosition(nullptr);
    chain->setFxChain(fx);
    chain->rebuild();
    control->setVolume(params.control.volume);
    control->setPan(params.control.balance);
    control->setMuted(params.control.muted);
    if (source) source->seek(TimePosition::zero(spec_.sampleRate));
    tracks_.push_back(chain);
    size_t audible = 0;
    for (const auto &track : tracks_)
        if (!track->control()->muted()) ++audible;
    mixer_->setNonMutedTrackCount(audible);
    return Result::make_ok(id, params);
}

void OfflineRenderSession::resetSources() {
    for (const auto &track : tracks_) {
        if (auto source = std::dynamic_pointer_cast<AudioSourceNode>(track->source())) source->seek(TimePosition::zero(spec_.sampleRate));
    }
}
