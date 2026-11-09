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
#include "mpe/events.h"

namespace muse::audio::engine {
struct FluidTrackSource {
    // Resources and configuration outlive this concrete per-track owner.
    FluidTrackSource(AudioResources &resources, const OfflineRenderSettings &config, const OutputSpec &spec, MidiEventSink sink,
                     const mpe::TrackEvents &events);

    void seek(const TimePosition &position, const bool flush_sound = true);
    void flush();

    const AudioInputParams &input_params() const;
    void apply_input_params(const AudioInputParams &required_params);

    void prepare_to_play();
    bool ready_to_play() const;

    bool has_pending_chunks() const;
    void process_input();
    InputProcessingProgress input_processing_progress() const;

    void clear_cache();

    bool initialized() const { return synth_ != nullptr; }

    void set_output_spec(const OutputSpec &spec) {
        if (output_spec_ == spec) return;
        output_spec_ = spec;
        on_output_spec_changed(spec);
    }

    void process(float *buffer, samples_t frames) { do_self_process(buffer, frames); }

    void set_mode(ProcessMode mode) {
        if (mode_ == mode) return;
        mode_ = mode;
        on_mode_changed(mode);
    }

private:
    OutputSpec output_spec_;
    ProcessMode mode_ = ProcessMode::UNDEFINED;

    void on_mode_changed(const ProcessMode mode);
    void on_output_spec_changed(const OutputSpec &spec);

    void do_self_process(float *buffer, samples_t samples_per_channel);

    AudioResources &resources_;
    OfflineRenderSettings config_;
    OutputSpec default_spec_;
    MidiEventSink sink_;
    // Borrowed only until initial setup. The export model outlives that call.
    const mpe::TrackEvents *initial_data_;
    std::unique_ptr<synth::FluidTrack> synth_ = nullptr;
    AudioInputParams params_;
};

} // namespace muse::audio::engine
