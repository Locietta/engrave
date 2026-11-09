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

#include "fluidtracksource.h"

#include <utility>

#include "../offlinetiming.h"
#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;
using namespace muse::audio::synth;
using namespace muse::mpe;

FluidTrackSource::FluidTrackSource(AudioResources &resources, const IAudioEngineConfiguration &config, const OutputSpec &spec,
                                   MidiEventSink sink, const mpe::TrackEvents &events)
    : resources_(resources), config_(config), default_spec_(spec), sink_(std::move(sink)), initial_data_(&events) {}

void FluidTrackSource::on_mode_changed(const ProcessMode mode) {

    if (!synth_) {
        return;
    }

    synth_->set_mode(mode);
    synth_->flush_sound();
}

void FluidTrackSource::on_output_spec_changed(const OutputSpec &spec) {
    if (!synth_) {
        return;
    }

    synth_->set_output_spec(spec);
}

void FluidTrackSource::do_self_process(float *buffer, samples_t samples_per_channel) {
    if (!synth_) {
        return;
    }

    OfflineTimer timer(OfflineTiming::current().synth);
    synth_->process(buffer, samples_per_channel);
}

void FluidTrackSource::seek(const TimePosition &position, const bool flush_sound) {

    IF_ASSERT_FAILED(synth_) { return; }

    if (synth_->playback_position() == position) {
        return;
    }

    synth_->set_playback_position(position);

    if (flush_sound) {
        synth_->flush_sound();
    }
}

void FluidTrackSource::flush() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->flush_sound();
}

const AudioInputParams &FluidTrackSource::input_params() const { return params_; }

void FluidTrackSource::apply_input_params(const AudioInputParams &required_params) {
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

    auto selected = required_params;
    auto found = resources_.resources.find(selected.resource_meta.id);
    OutputSpec spec = output_spec_;
    if (!selected.is_valid() || selected.type() != AudioSourceType::FLUID || found == resources_.resources.end()) {
        selected = config_.default_audio_input_params();
        found = resources_.resources.find(selected.resource_meta.id);
        spec = default_spec_;
        if (!selected.is_valid() || found == resources_.resources.end()) {
            LOGE() << "Default synth not found!";
            return;
        }
    }
    auto track = std::make_unique<synth::FluidTrack>(selected, config_, sink_, resources_);
    track->init(spec);
    track->add_sound_fonts({found->second.path});
    track->set_preset(found->second.preset);

    // Publish only a fully initialized synth. A throwing setup leaves the node failed.
    track->set_output_spec(output_spec_);
    track->setup(*data);
    track->set_mode(ProcessMode::IDLE);
    synth_ = std::move(track);

    params_ = synth_->params();
}

void FluidTrackSource::prepare_to_play() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->prepare_to_play();
}

bool FluidTrackSource::ready_to_play() const {

    IF_ASSERT_FAILED(synth_) { return false; }

    return synth_->ready_to_play();
}

bool FluidTrackSource::has_pending_chunks() const {

    IF_ASSERT_FAILED(synth_) { return false; }

    return synth_->has_pending_chunks();
}

void FluidTrackSource::process_input() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->process_input();
}

InputProcessingProgress FluidTrackSource::input_processing_progress() const {

    IF_ASSERT_FAILED(synth_) { return {}; }

    return synth_->input_processing_progress();
}

void FluidTrackSource::clear_cache() {

    IF_ASSERT_FAILED(synth_) { return; }

    synth_->clear_cache();
}
