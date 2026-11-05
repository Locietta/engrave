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

#ifndef MUSE_AUDIO_DSP_ENVELOPEFILTERCONFIG_H
#define MUSE_AUDIO_DSP_ENVELOPEFILTERCONFIG_H

#include "audio/common/audiotypes.h"
#include "audiomathutils.h"

namespace muse::audio::dsp {
struct EnvelopeFilterConfig {
    explicit EnvelopeFilterConfig(const unsigned int sampleRate, const float ratio = DEFAULT_RATIO,
                                  const volume_db_t logarithmicThreshold = DEFAULT_THRESHOLD_DB,
                                  const volume_db_t kneeWidth = DEFAULT_KNEE_WIDTH, const float attackTime = DEFAULT_ATTACK_TIME,
                                  const float releaseTime = DEFAULT_RELEASE_TIME,
                                  const volume_db_t minimumOperableLevel = DEFAULT_MINIMUM_OPERABLE_LEVEL,
                                  const volume_db_t makeUpGain = DEFAULT_MAKE_UP_GAIN) {
        sampleRate_ = sampleRate;
        ratio_ = ratio;
        kneeWidth_ = kneeWidth;
        logarithmicThreshold_ = logarithmicThreshold;
        attackTime_ = attackTime;
        releaseTime_ = releaseTime;

        attackTimeCoefficient_ = sampleAttackTimeCoefficient(sampleRate, attackTime);
        releaseTimeCoefficient_ = sampleReleaseTimeCoefficient(sampleRate, releaseTime);

        minimumOperableLevel_ = minimumOperableLevel;

        softThresholdLower_ = logarithmicThreshold_ - (kneeWidth_ / 2.f);
        softThresholdUpper_ = logarithmicThreshold_ + (kneeWidth_ / 2.f);

        makeUpGain_ = makeUpGain;
    }

    static constexpr float DEFAULT_KNEE_WIDTH = 6.f; // [0, 20.f]
    static constexpr float DEFAULT_RATIO = 4.f;      // [1, 50.f]
    static constexpr volume_db_t DEFAULT_THRESHOLD_DB = volume_dbfs_t::make(-10.f);
    static constexpr float DEFAULT_ATTACK_TIME = 0.005f; // [0, 4.f]
    static constexpr float DEFAULT_RELEASE_TIME = 0.02f; // [0, 4.f]
    static constexpr volume_db_t DEFAULT_MINIMUM_OPERABLE_LEVEL = volume_dbfs_t::make(-60.f);
    static constexpr volume_db_t DEFAULT_MAKE_UP_GAIN = volume_dbfs_t::make(-10.f); // [-10.f, 12.f]

    unsigned int sampleRate() const { return sampleRate_; }

    /// Compression ratio is the input/output ratio for signals that overshoot the operation threshold.
    float ratio() const { return ratio_; }

    volume_db_t kneeWidth() const { return kneeWidth_; }

    /// Operation threshold is the level above which gain is applied to the input signal.
    volume_db_t logarithmicThreshold() const { return logarithmicThreshold_; }

    float attackTimeCoefficient() const { return attackTimeCoefficient_; }

    float releaseTimeCoefficient() const { return releaseTimeCoefficient_; }

    volume_db_t minimumOperableLevel() const { return minimumOperableLevel_; }

    volume_db_t softThresholdUpper() const { return softThresholdUpper_; }

    volume_db_t softThresholdLower() const { return softThresholdLower_; }

    /// Make-up gain compensates for gain lost during limiting. It is applied at the output of the dynamic range limiter.
    volume_db_t makeUpGain() const { return makeUpGain_; }

private:
    unsigned int sampleRate_ = 0;
    float ratio_ = 0.f;
    volume_db_t kneeWidth_ = 0.f;
    volume_db_t logarithmicThreshold_ = 0.f;
    float attackTime_ = 0.f;
    float releaseTime_ = 0.f;
    float attackTimeCoefficient_ = 0.f;
    float releaseTimeCoefficient_ = 0.f;
    float minimumOperableLevel_ = 0.f;

    volume_db_t softThresholdUpper_ = 0.f;
    volume_db_t softThresholdLower_ = 0.f;
    volume_db_t makeUpGain_ = 0.f;
};
} // namespace muse::audio::dsp

#endif // MUSE_AUDIO_DSP_ABSTRACTENVELOPEFILTER_H
