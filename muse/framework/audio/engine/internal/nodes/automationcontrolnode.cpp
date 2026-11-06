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

void AutomationControlNode::set_playhead_position(const PlayheadPositionPtr &playhead_position) {
    playhead_position_ = playhead_position;
    last_gains_pos_ = std::nullopt;
}

void AutomationControlNode::set_volume(const AutomatableValue<volume_db_t> &volume) {
    volume_ = volume;
    update_channel_gains(current_pos());
}

void AutomationControlNode::set_pan(const AutomatableValue<balance_t> &pan) {
    pan_ = pan;
    update_channel_gains(current_pos());
}

void AutomationControlNode::set_muted(bool muted) { set_enabled(!muted); }

bool AutomationControlNode::muted() const { return !enabled(); }

void AutomationControlNode::on_output_spec_changed(const OutputSpec &) { update_channel_gains(current_pos()); }

secs_t AutomationControlNode::current_pos() const {
    return playhead_position_ ? playhead_position_->current_position().time() : secs_t(0.0);
}

void AutomationControlNode::update_channel_gains(secs_t pos) {
    const volume_db_t volume_db = volume_.evaluate_at(pos, ::muse::audio::volume_db_min, ::muse::audio::volume_db_max);
    const balance_t pan_value = pan_.evaluate_at(pos, ::muse::audio::balance_min, ::muse::audio::balance_max);

    const float volume = muse::db_to_linear(volume_db.raw());
    const float pan = pan_value.raw();

    const audioch_t channels_count = output_spec_.audio_channel_count;
    const bool first_update = !last_gains_pos_.has_value();
    channel_gains_.resize(channels_count);

    for (audioch_t audio_ch_num = 0; audio_ch_num < channels_count; ++audio_ch_num) {
        const float gain = dsp::balance_gain(pan, audio_ch_num) * volume;
        if (first_update) {
            channel_gains_[audio_ch_num].init_with_value(gain);
        } else {
            channel_gains_[audio_ch_num].set_target_value(gain);
        }
    }

    last_gains_pos_ = pos;
}

void AutomationControlNode::do_self_process(float *buffer, samples_t samples_per_channel) {
    if (volume_.has_automation() || pan_.has_automation()) {
        const secs_t pos = current_pos();
        if (last_gains_pos_ != pos) {
            update_channel_gains(pos);
        }
    }

    const audioch_t channels_count = output_spec_.audio_channel_count;

    for (size_t s = 0; s < samples_per_channel; ++s) {
        const size_t frame_offset = s * channels_count;
        for (size_t ch = 0; ch < channels_count; ++ch) {
            const size_t idx = frame_offset + ch;
            dsp::SmoothLinearValue<float> &gain = channel_gains_[ch];
            buffer[idx] = buffer[idx] * gain.get_value();
            gain.tick();
        }
    }
}
