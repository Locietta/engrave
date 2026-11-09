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

#include "abstractsynthesizer.h"

using namespace muse;
using namespace muse::mpe;
using namespace muse::audio;
using namespace muse::audio::synth;

AbstractSynthesizer::AbstractSynthesizer(const AudioInputParams &params, const engine::IAudioEngineConfiguration &config)
    : params_(params), config_(config) {}

void AbstractSynthesizer::set_mode(const ProcessMode mode) { mode_ = mode; }

ProcessMode AbstractSynthesizer::mode() const { return mode_; }

const AudioInputParams &AbstractSynthesizer::params() const { return params_; }

void AbstractSynthesizer::setup(const mpe::TrackEvents &events) {

    setup_data_ = events.setup;

    setup_sound(events.setup);
    setup_events(events);
}

void AbstractSynthesizer::prepare_to_play() {}

bool AbstractSynthesizer::ready_to_play() const { return true; }

bool AbstractSynthesizer::has_pending_chunks() const { return has_pending_chunks_; }

void AbstractSynthesizer::process_input() {}

InputProcessingProgress AbstractSynthesizer::input_processing_progress() const { return input_processing_progress_; }

void AbstractSynthesizer::clear_cache() {}

muse::audio::msecs_t AbstractSynthesizer::samples_to_msecs(const samples_t samples_per_channel, const samples_t sample_rate) const {
    return samples_per_channel * 1000000 / sample_rate;
}

samples_t AbstractSynthesizer::micro_secs_to_samples(const msecs_t msec, const samples_t sample_rate) const {
    return (msec.raw() / 1000000.f) * sample_rate;
}
