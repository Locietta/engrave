/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "eventaudionode.h"
#include "../offlinetiming.h"

#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;
using namespace muse::audio::synth;
using namespace muse::mpe;

EventAudioNode::EventAudioNode(const IAudioFactory &audioFactory, TrackId trackId, const mpe::PlaybackData &playbackData)
    : audioFactory_(audioFactory), trackId_(trackId), playbackData_(playbackData) {}

void EventAudioNode::onModeChanged(const ProcessMode mode) {

    if (!synth_) {
        return;
    }

    synth_->setMode(mode);
    synth_->flushSound();
}

void EventAudioNode::onOutputSpecChanged(const OutputSpec &spec) {
    if (!synth_) {
        return;
    }

    synth_->setOutputSpec(spec);
}

void EventAudioNode::doSelfProcess(float *buffer, samples_t samplesPerChannel) {
    if (!synth_) {
        return;
    }

    OfflineTimer timer(OfflineTiming::current().synth);
    synth_->process(buffer, samplesPerChannel);
}

void EventAudioNode::seek(const TimePosition &position, const bool flushSound) {

    IF_ASSERT_FAILED(synth_) { return; }

    if (synth_->playbackPosition() == position) {
        return;
    }

    synth_->setPlaybackPosition(position);

    if (flushSound) {
        synth_->flushSound();
    }
}

void EventAudioNode::flush() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->flushSound();
}

const AudioInputParams &EventAudioNode::inputParams() const { return params_; }

void EventAudioNode::applyInputParams(const AudioInputParams &requiredParams) {
    IF_ASSERT_FAILED(outputSpec_.isValid()) { return; }

    if (params_.isValid() && params_ == requiredParams) {
        return;
    }

    SynthCtx ctx = currentSynthCtx();

    if (synth_) {
        playbackData_ = synth_->playbackData();
    }

    RetVal<synth::ISynthesizerPtr> synth = audioFactory()->makeSynth(trackId_, requiredParams, playbackData_.setupData);

    if (!synth.ret) {
        synth = audioFactory()->makeDefaultSynth(trackId_);
        IF_ASSERT_FAILED(synth.val) {
            LOGE() << "Default synth not found!";
            return;
        }
    }

    synth_ = synth.val;

    setupSource();

    if (ctx.isValid()) {
        restoreSynthCtx(ctx);
    } else {
        synth_->setMode(ProcessMode::Idle);
    }

    setName(std::string("Source[") + synth_->name() + "]");

    params_ = synth_->params();
}

void EventAudioNode::prepareToPlay() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->prepareToPlay();
}

bool EventAudioNode::readyToPlay() const {

    IF_ASSERT_FAILED(synth_) { return false; }

    return synth_->readyToPlay();
}

bool EventAudioNode::hasPendingChunks() const {

    IF_ASSERT_FAILED(synth_) { return false; }

    return synth_->hasPendingChunks();
}

void EventAudioNode::processInput() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->processInput();
}

InputProcessingProgress EventAudioNode::inputProcessingProgress() const {

    IF_ASSERT_FAILED(synth_) { return {}; }

    return synth_->inputProcessingProgress();
}

void EventAudioNode::clearCache() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->clearCache();
}

EventAudioNode::SynthCtx EventAudioNode::currentSynthCtx() const {
    if (!synth_) {
        return SynthCtx();
    }

    return {synth_->mode(), synth_->playbackPosition()};
}

void EventAudioNode::restoreSynthCtx(const SynthCtx &ctx) {
    synth_->setPlaybackPosition(ctx.playbackPosition);
    synth_->setMode(ctx.mode);
}

void EventAudioNode::setupSource() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->setOutputSpec(outputSpec_);
    synth_->setup(playbackData_);
}
