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

#pragma once

#include "../synthesizers/fluidsynth/fluidtrack.h"
#include "audio/common/audiotypes.h"
#include "audio/engine/audioresources.h"
#include "audiosourcenode.h"
#include "mpe/events.h"

namespace muse::audio::engine {
struct FluidTrackNode : AudioSourceNode {
    // Graph adapter; resources and configuration outlive the render session.
    FluidTrackNode(AudioResources &resources, const IAudioEngineConfiguration &config, const OutputSpec &spec, MidiEventSink sink,
                   const mpe::TrackEvents &events);

    void seek(const TimePosition &position, const bool flush_sound = true) override;
    void flush() override;

    const AudioInputParams &input_params() const override;
    void apply_input_params(const AudioInputParams &required_params) override;

    void prepare_to_play() override;
    bool ready_to_play() const override;

    bool has_pending_chunks() const override;
    void process_input() override;
    InputProcessingProgress input_processing_progress() const override;

    void clear_cache() override;

    bool initialized() const { return synth_ != nullptr; }

private:
    void on_mode_changed(const ProcessMode mode) override;
    void on_output_spec_changed(const OutputSpec &spec) override;

    void do_self_process(float *buffer, samples_t samples_per_channel) override;

    AudioResources &resources_;
    const IAudioEngineConfiguration &config_;
    OutputSpec default_spec_;
    MidiEventSink sink_;
    // Borrowed only until initial setup. The export model outlives that call.
    const mpe::TrackEvents *initial_data_;
    std::unique_ptr<synth::FluidTrack> synth_ = nullptr;
    AudioInputParams params_;
};

using FluidTrackNodePtr = std::shared_ptr<FluidTrackNode>;
} // namespace muse::audio::engine
