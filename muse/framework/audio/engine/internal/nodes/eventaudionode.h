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

#include "../iaudiofactory.h"
#include "audio/common/audiotypes.h"
#include "audiosourcenode.h"
#include "mpe/events.h"

namespace muse::audio::engine {
struct EventAudioNode : AudioSourceNode {
    // audioFactory makes the synthesizer for the input params; it must outlive the node.
    EventAudioNode(const IAudioFactory &audio_factory, TrackId track_id, const mpe::PlaybackData &playback_data);

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

private:
    struct SynthCtx {
        ProcessMode mode = ProcessMode::UNDEFINED;
        TimePosition playback_position;

        bool is_valid() const { return mode != ProcessMode::UNDEFINED; }
    };

    void on_mode_changed(const ProcessMode mode) override;
    void on_output_spec_changed(const OutputSpec &spec) override;

    void do_self_process(float *buffer, samples_t samples_per_channel) override;

    void setup_source();
    SynthCtx current_synth_ctx() const;
    void restore_synth_ctx(const SynthCtx &ctx);

    const IAudioFactory *audio_factory() const { return &audio_factory_; }

    const IAudioFactory &audio_factory_;
    TrackId track_id_ = -1;
    mpe::PlaybackData playback_data_;
    synth::ISynthesizerPtr synth_ = nullptr;
    AudioInputParams params_;
};

using EventAudioNodePtr = std::shared_ptr<EventAudioNode>;
} // namespace muse::audio::engine
