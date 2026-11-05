/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "automationcontrolnode.h"

#include "../dsp/audiomathutils.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

void AutomationControlNode::setPlayheadPosition(const PlayheadPositionPtr &playheadPosition) {
    playheadPosition_ = playheadPosition;
    lastGainsPos_ = std::nullopt;
}

void AutomationControlNode::setVolume(const AutomatableValue<volume_db_t> &volume) {
    volume_ = volume;
    updateChannelGains(currentPos());
}

void AutomationControlNode::setPan(const AutomatableValue<balance_t> &pan) {
    pan_ = pan;
    updateChannelGains(currentPos());
}

void AutomationControlNode::setMuted(bool muted) { setEnabled(!muted); }

bool AutomationControlNode::muted() const { return !enabled(); }

void AutomationControlNode::onOutputSpecChanged(const OutputSpec &) { updateChannelGains(currentPos()); }

secs_t AutomationControlNode::currentPos() const { return playheadPosition_ ? playheadPosition_->currentPosition().time() : secs_t(0.0); }

void AutomationControlNode::updateChannelGains(secs_t pos) {
    const volume_db_t volumeDb = volume_.evaluateAt(pos, VOLUME_DB_MIN, VOLUME_DB_MAX);
    const balance_t panValue = pan_.evaluateAt(pos, BALANCE_MIN, BALANCE_MAX);

    const float volume = muse::db_to_linear(volumeDb.raw());
    const float pan = panValue.raw();

    const audioch_t channelsCount = outputSpec_.audioChannelCount;
    const bool firstUpdate = !lastGainsPos_.has_value();
    channelGains_.resize(channelsCount);

    for (audioch_t audioChNum = 0; audioChNum < channelsCount; ++audioChNum) {
        const float gain = dsp::balanceGain(pan, audioChNum) * volume;
        if (firstUpdate) {
            channelGains_[audioChNum].initWithValue(gain);
        } else {
            channelGains_[audioChNum].setTargetValue(gain);
        }
    }

    lastGainsPos_ = pos;
}

void AutomationControlNode::doSelfProcess(float *buffer, samples_t samplesPerChannel) {
    if (volume_.hasAutomation() || pan_.hasAutomation()) {
        const secs_t pos = currentPos();
        if (lastGainsPos_ != pos) {
            updateChannelGains(pos);
        }
    }

    const audioch_t channelsCount = outputSpec_.audioChannelCount;

    for (size_t s = 0; s < samplesPerChannel; ++s) {
        const size_t frameOffset = s * channelsCount;
        for (size_t ch = 0; ch < channelsCount; ++ch) {
            const size_t idx = frameOffset + ch;
            dsp::SmoothLinearValue<float> &gain = channelGains_[ch];
            buffer[idx] = buffer[idx] * gain.getValue();
            gain.tick();
        }
    }
}
