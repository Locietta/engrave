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

void AbstractSynthesizer::setMode(const ProcessMode mode) { mode_ = mode; }

ProcessMode AbstractSynthesizer::mode() const { return mode_; }

const AudioInputParams &AbstractSynthesizer::params() const { return params_; }

void AbstractSynthesizer::setup(const mpe::PlaybackData &playbackData) {

    setupData_ = playbackData.setupData;

    setupSound(playbackData.setupData);
    setupEvents(playbackData);
}

void AbstractSynthesizer::prepareToPlay() {}

bool AbstractSynthesizer::readyToPlay() const { return true; }

bool AbstractSynthesizer::hasPendingChunks() const { return hasPendingChunks_; }

void AbstractSynthesizer::processInput() {}

InputProcessingProgress AbstractSynthesizer::inputProcessingProgress() const { return inputProcessingProgress_; }

void AbstractSynthesizer::clearCache() {}

muse::audio::msecs_t AbstractSynthesizer::samplesToMsecs(const samples_t samplesPerChannel, const samples_t sampleRate) const {
    return samplesPerChannel * 1000000 / sampleRate;
}

samples_t AbstractSynthesizer::microSecsToSamples(const msecs_t msec, const samples_t sampleRate) const {
    return (msec.raw() / 1000000.f) * sampleRate;
}
