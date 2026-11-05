/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
#include "equaliser.h"

#include <cmath>

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::fx;

void Equaliser::setOutputSpec(const OutputSpec &spec) {
    outputSpec_ = spec;
    calculate();
}

bool Equaliser::active() const { return active_; }

void Equaliser::setActive(bool active) { active_ = active; }

void Equaliser::setMode(const ProcessMode) {}

bool Equaliser::shouldProcessDuringSilence() const { return false; }

void Equaliser::process(float *buffer, samples_t sampleCount, samples_t) {
    for (samples_t i = 0; i < sampleCount; ++i) {
        x_[2] = x_[1];
        x_[1] = x_[0];
        x_[0] = buffer[i];

        y_[2] = y_[1];
        y_[1] = y_[0];
        y_[0] = (b_[2] * x_[2] + b_[1] * x_[1] + b_[0] * x_[0] - a_[1] * y_[1] - a_[2] * y_[2]) / a_[0];

        buffer[i] = y_[0];
    }
}

void Equaliser::calculate() {
    if (!outputSpec_.sampleRate) {
        return;
    }
    float a = std::pow(10.f, gain_ / 40.f);
    float w0 = 2 * M_PI * frequency_ / outputSpec_.sampleRate;
    float alpha = std::sin(w0) * a / (2 * q_);

    b_[0] = 1 + alpha * a;
    b_[1] = -2 * std::cos(w0);
    b_[2] = 1 - alpha * a;

    a_[0] = 1 + alpha / a;
    a_[1] = -2 * std::cos(w0);
    a_[2] = 1 - alpha / a;
}

void Equaliser::setFrequency(float value) {
    frequency_ = value;
    calculate();
}

void Equaliser::setGain(float value) {
    gain_ = value;
    calculate();
}

void Equaliser::setQ(float value) {
    q_ = value;
    calculate();
}
