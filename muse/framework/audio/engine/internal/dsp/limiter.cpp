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
#include "limiter.h"

#include "audiomathutils.h"

using namespace muse::audio;
using namespace muse::audio::dsp;

static constexpr volume_db_t THRESHOLD = volume_dbfs_t::make(0.f);

Limiter::Limiter(const unsigned int sampleRate) : filterConfig_(sampleRate, 1.f, THRESHOLD) {}

bool Limiter::isActive() const { return isActive_; }

void Limiter::setIsActive(const bool active) { isActive_ = active; }

volume_db_t Limiter::gainSmoothing(const float newGainReduction) const {
    float coefficient = 0.f;

    if (newGainReduction <= previousGainReduction_) {
        coefficient = filterConfig_.attackTimeCoefficient();
    } else {
        coefficient = filterConfig_.releaseTimeCoefficient();
    }

    return (coefficient * previousGainReduction_) + ((1 - coefficient) * newGainReduction);
}

volume_db_t Limiter::computeGain(const volume_db_t &logarithmSample) const {
    if (logarithmSample < filterConfig_.softThresholdLower()) {
        return logarithmSample;
    }

    if (logarithmSample >= filterConfig_.softThresholdLower() && logarithmSample <= filterConfig_.softThresholdUpper()) {
        return logarithmSample - (std::pow((logarithmSample - filterConfig_.softThresholdUpper()), 2) / (2 * filterConfig_.kneeWidth()));
    }

    return filterConfig_.logarithmicThreshold();
}

void Limiter::process(const float &linearRms, float *buffer, const audioch_t &audioChannelsCount, const samples_t samplesPerChannel) {
    volume_db_t rmsDb = muse::linear_to_db(linearRms);

    if (rmsDb <= filterConfig_.minimumOperableLevel()) {
        return;
    }

    // gain computing
    float computedGain = computeGain(rmsDb) - rmsDb;

    // gain smoothing
    float smoothedGain = gainSmoothing(computedGain);
    previousGainReduction_ = smoothedGain;

    // gain make-up
    float makeUpGain = smoothedGain + filterConfig_.makeUpGain();

    // total linear gain
    float totalLinearGain = muse::db_to_linear(makeUpGain);

    // apply linear gain
    for (audioch_t audioChNum = 0; audioChNum < audioChannelsCount; ++audioChNum) {
        multiplySamples(buffer, audioChannelsCount, audioChNum, samplesPerChannel, totalLinearGain);
    }
}
