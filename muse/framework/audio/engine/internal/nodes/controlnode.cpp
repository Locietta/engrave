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

void ControlNode::set_volume(float value) {
    volume_ = value;
    update_channel_gains();
}

float ControlNode::volume() const { return volume_; }

void ControlNode::set_pan(float pan) {
    pan_ = pan;
    update_channel_gains();
}

float ControlNode::pan() const { return pan_; }

void ControlNode::set_muted(bool muted) { set_enabled(!muted); }

bool ControlNode::muted() const { return !enabled(); }

void ControlNode::on_output_spec_changed(const OutputSpec &) { update_channel_gains(); }

void ControlNode::update_channel_gains() {
    const audioch_t channels_count = output_spec_.audio_channel_count;
    channel_gains_.resize(channels_count);
    for (audioch_t audio_ch_num = 0; audio_ch_num < channels_count; ++audio_ch_num) {
        channel_gains_[audio_ch_num] = dsp::balance_gain(pan_, audio_ch_num) * volume_;
    }
}

void ControlNode::do_self_process(float *buffer, samples_t samples_per_channel) {
    const audioch_t channels_count = output_spec_.audio_channel_count;

    for (size_t s = 0; s < samples_per_channel; ++s) {
        const size_t frame_offset = s * channels_count;
        for (size_t ch = 0; ch < channels_count; ++ch) {
            const size_t idx = frame_offset + ch;
            buffer[idx] = buffer[idx] * channel_gains_[ch];
        }
    }
}
