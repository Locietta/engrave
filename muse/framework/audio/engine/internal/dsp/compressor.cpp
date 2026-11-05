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
#include "compressor.h"

#include "audiomathutils.h"

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::dsp;

static constexpr float RATIO = 4.f;

Compressor::Compressor(const unsigned int sampleRate)
    : filterConfig_(sampleRate, RATIO), feedbackFactor_(1.f) // 0 = feed forward, 1 = feed back
{}

bool Compressor::isActive() const { return isActive_; }

void Compressor::setIsActive(const bool active) { isActive_ = active; }

volume_db_t Compressor::gainSmoothing(const float &newGainReduction) const {
    float coefficient = 0.f;

    if (newGainReduction <= previousGainReduction_) {
        coefficient = filterConfig_.attackTimeCoefficient();
    } else {
        coefficient = filterConfig_.releaseTimeCoefficient();
    }

    return (coefficient * previousGainReduction_) + ((1 - coefficient) * newGainReduction);
}

volume_db_t Compressor::computeGain(const volume_db_t &logarithmSample) const {
    if (logarithmSample < softThresholdLower_) {
        return logarithmSample;
    }

    if (logarithmSample >= softThresholdLower_ && logarithmSample <= softThresholdUpper_) {
        return logarithmSample +
               ((((1 / filterConfig_.ratio()) - 1) * std::pow(logarithmSample - softThresholdUpper_, 2)) / (2 * filterConfig_.kneeWidth()));
    }

    return filterConfig_.logarithmicThreshold() + ((logarithmSample - filterConfig_.logarithmicThreshold()) / filterConfig_.ratio());
}

void Compressor::process(const float linearRms, float *buffer, const audioch_t &audioChannelsCount, const samples_t samplesPerChannel) {
    float dbGain = muse::linear_to_db(linearRms);

    if (dbGain <= filterConfig_.minimumOperableLevel()) {
        return;
    }

    // trying to predict the next sample by the previous gain reduction
    dbGain += feedbackFactor_ * feedbackGain_;

    float dbDiff = computeGain(dbGain) - dbGain;

    feedbackGain_ = dbDiff;
    float gainFact = muse::db_to_linear(dbDiff * (1.f + feedbackFactor_));

    float currentGainReduction = std::min(gainFact, previousGainReduction_);

    // apply gain
    for (audioch_t audioChNum = 0; audioChNum < audioChannelsCount; ++audioChNum) {
        multiplySamples(buffer, audioChannelsCount, audioChNum, samplesPerChannel, currentGainReduction);
    }

    previousGainReduction_ = currentGainReduction;
}
