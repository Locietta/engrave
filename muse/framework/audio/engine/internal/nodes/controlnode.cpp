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

#include "controlnode.h"

#include "../dsp/audiomathutils.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

void ControlNode::setVolume(float value) {
    volume_ = value;
    updateChannelGains();
}

float ControlNode::volume() const { return volume_; }

void ControlNode::setPan(float pan) {
    pan_ = pan;
    updateChannelGains();
}

float ControlNode::pan() const { return pan_; }

void ControlNode::setMuted(bool muted) { setEnabled(!muted); }

bool ControlNode::muted() const { return !enabled(); }

void ControlNode::onOutputSpecChanged(const OutputSpec &) { updateChannelGains(); }

void ControlNode::updateChannelGains() {
    const audioch_t channelsCount = outputSpec_.audioChannelCount;
    channelGains_.resize(channelsCount);
    for (audioch_t audioChNum = 0; audioChNum < channelsCount; ++audioChNum) {
        channelGains_[audioChNum] = dsp::balanceGain(pan_, audioChNum) * volume_;
    }
}

void ControlNode::doSelfProcess(float *buffer, samples_t samplesPerChannel) {
    const audioch_t channelsCount = outputSpec_.audioChannelCount;

    for (size_t s = 0; s < samplesPerChannel; ++s) {
        const size_t frameOffset = s * channelsCount;
        for (size_t ch = 0; ch < channelsCount; ++ch) {
            const size_t idx = frameOffset + ch;
            buffer[idx] = buffer[idx] * channelGains_[ch];
        }
    }
}
