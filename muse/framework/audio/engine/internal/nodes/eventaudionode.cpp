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

#include <utility>

#include "../offlinetiming.h"
#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;
using namespace muse::audio::synth;
using namespace muse::mpe;

EventAudioNode::EventAudioNode(const IAudioFactory &audio_factory, TrackId track_id, const mpe::PlaybackData &playback_data)
    : audio_factory_(audio_factory), track_id_(track_id), initial_data_(&playback_data) {}

void EventAudioNode::on_mode_changed(const ProcessMode mode) {

    if (!synth_) {
        return;
    }

    synth_->set_mode(mode);
    synth_->flush_sound();
}

void EventAudioNode::on_output_spec_changed(const OutputSpec &spec) {
    if (!synth_) {
        return;
    }

    synth_->set_output_spec(spec);
}

void EventAudioNode::do_self_process(float *buffer, samples_t samples_per_channel) {
    if (!synth_) {
        return;
    }

    OfflineTimer timer(OfflineTiming::current().synth);
    synth_->process(buffer, samples_per_channel);
}

void EventAudioNode::seek(const TimePosition &position, const bool flush_sound) {

    IF_ASSERT_FAILED(synth_) { return; }

    if (synth_->playback_position() == position) {
        return;
    }

    synth_->set_playback_position(position);

    if (flush_sound) {
        synth_->flush_sound();
    }
}

void EventAudioNode::flush() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->flush_sound();
}

const AudioInputParams &EventAudioNode::input_params() const { return params_; }

void EventAudioNode::apply_input_params(const AudioInputParams &required_params) {
    // Consume the borrow before any assertion, resolver or synth call can fail.
    const auto *data = std::exchange(initial_data_, nullptr);
    IF_ASSERT_FAILED(output_spec_.is_valid()) { return; }

    if (params_.is_valid() && params_ == required_params) {
        return;
    }

    // Offline sources load once. Their MPE input may already have been released.
    if (!data) {
        LOGE() << "Cannot reload an offline event source";
        return;
    }

    RetVal<synth::ISynthesizerPtr> synth = audio_factory()->make_synth(track_id_, required_params, data->setup_data);

    if (!synth.ret) {
        synth = audio_factory()->make_default_synth(track_id_);
        if (!synth.ret || !synth.val) {
            LOGE() << "Default synth not found!";
            return;
        }
    }

    // Publish only a fully initialized synth. A throwing setup leaves the node failed.
    synth.val->set_output_spec(output_spec_);
    synth.val->setup(*data);
    synth.val->set_mode(ProcessMode::IDLE);
    synth_ = synth.val;

    set_name(std::string("Source[") + synth_->name() + "]");

    params_ = synth_->params();
}

void EventAudioNode::prepare_to_play() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->prepare_to_play();
}

bool EventAudioNode::ready_to_play() const {

    IF_ASSERT_FAILED(synth_) { return false; }

    return synth_->ready_to_play();
}

bool EventAudioNode::has_pending_chunks() const {

    IF_ASSERT_FAILED(synth_) { return false; }

    return synth_->has_pending_chunks();
}

void EventAudioNode::process_input() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->process_input();
}

InputProcessingProgress EventAudioNode::input_processing_progress() const {

    IF_ASSERT_FAILED(synth_) { return {}; }

    return synth_->input_processing_progress();
}

void EventAudioNode::clear_cache() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->clear_cache();
}
