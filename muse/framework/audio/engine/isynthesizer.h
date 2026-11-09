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

#include <string>

#include "audio/common/audiotypes.h"
#include "audio/common/timeposition.h"
#include "mpe/flatevents.h"

namespace muse::audio::synth {
struct ISynthesizer {
    virtual ~ISynthesizer() = default;

    virtual std::string name() const = 0;
    virtual AudioSourceType type() const = 0;
    virtual bool is_valid() const = 0;

    virtual void set_mode(const ProcessMode mode) = 0;
    virtual ProcessMode mode() const = 0;

    virtual void set_output_spec(const OutputSpec &spec) = 0;

    virtual void setup(const mpe::TrackEvents &events) = 0;

    virtual const audio::AudioInputParams &params() const = 0;

    virtual TimePosition playback_position() const = 0;
    virtual void set_playback_position(const TimePosition &position) = 0;

    virtual void prepare_to_play() = 0;
    virtual bool ready_to_play() const = 0;

    virtual void flush_sound() = 0;

    virtual bool has_pending_chunks() const = 0;
    virtual void process_input() = 0;
    virtual InputProcessingProgress input_processing_progress() const = 0;

    virtual void clear_cache() = 0;

    virtual samples_t process(float *buffer, samples_t samples_per_channel) = 0;
};

using ISynthesizerPtr = std::shared_ptr<ISynthesizer>;
} // namespace muse::audio::synth
