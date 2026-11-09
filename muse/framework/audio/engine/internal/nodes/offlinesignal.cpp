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

#include "offlinesignal.h"

using namespace muse::audio;
using namespace muse::audio::engine;

bool OfflineSignal::is_silent() const { return is_silent_; }

void OfflineSignal::on_output_spec_changed(const OutputSpec &spec) { channel_peaks_.resize(spec.audio_channel_count, 0.f); }

void OfflineSignal::do_self_process(float *buffer, samples_t samples_per_channel) {
    const audioch_t channels_count = output_spec_.audio_channel_count;

    for (audioch_t ch = 0; ch < channels_count; ++ch) {
        channel_peaks_[ch] = 0.f;
    }

    for (size_t s = 0; s < samples_per_channel; ++s) {
        const size_t frame_offset = s * channels_count;
        for (size_t ch = 0; ch < channels_count; ++ch) {
            const size_t idx = frame_offset + ch;
            const float a = std::fabs(buffer[idx]);
            if (a > channel_peaks_[ch]) {
                channel_peaks_[ch] = a;
            }
        }
    }

    is_silent_ = true;
    for (audioch_t ch = 0; ch < channels_count; ++ch) {
        if (channel_peaks_[ch] > 0.f) {
            is_silent_ = false;
        }
    }
}
