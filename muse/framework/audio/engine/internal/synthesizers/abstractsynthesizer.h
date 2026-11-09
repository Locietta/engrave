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

#include "../../iaudioengineconfiguration.h"
#include "../../isynthesizer.h"
#include "audio/common/audiotypes.h"
#include "mpe/events.h"

namespace muse::audio::synth {
struct AbstractSynthesizer : ISynthesizer {
    // config must outlive the synthesizer.
    AbstractSynthesizer(const audio::AudioInputParams &params, const engine::IAudioEngineConfiguration &config);
    virtual ~AbstractSynthesizer() = default;

    virtual void set_mode(const ProcessMode mode) override;
    ProcessMode mode() const override;

    const audio::AudioInputParams &params() const override;

    void setup(const mpe::TrackEvents &events) override;

    void prepare_to_play() override;
    bool ready_to_play() const override;

    bool has_pending_chunks() const override;
    void process_input() override;
    InputProcessingProgress input_processing_progress() const override;

    void clear_cache() override;

protected:
    const engine::IAudioEngineConfiguration *config() const { return &config_; }

    virtual void setup_sound(const mpe::PlaybackSetupData &setup_data) = 0;
    virtual void setup_events(const mpe::TrackEvents &events) = 0;

    msecs_t samples_to_msecs(const samples_t samples_per_channel, const samples_t sample_rate) const;
    samples_t micro_secs_to_samples(const msecs_t msec, const samples_t sample_rate) const;

    ProcessMode mode_ = ProcessMode::UNDEFINED;

    mpe::PlaybackSetupData setup_data_;

    audio::AudioInputParams params_;
    const engine::IAudioEngineConfiguration &config_;

    InputProcessingProgress input_processing_progress_;
    bool has_pending_chunks_ = false;
};
} // namespace muse::audio::synth
