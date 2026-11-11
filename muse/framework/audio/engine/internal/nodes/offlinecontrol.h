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

#pragma once

#include <optional>

#include "audio/common/audiotypes.h"
#include "audio/common/timeposition.h"
#include "audio/engine/internal/dsp/smoothlinearvalue.h"

namespace muse::audio::engine {
struct OfflineControl {

    void set_playhead_position(const TimePosition *playhead_position);
    void set_volume(const AutomatableValue<volume_db_t> &volume);
    void set_pan(const AutomatableValue<balance_t> &pan);
    void set_muted(bool muted);
    bool muted() const;

    void set_output_spec(const OutputSpec &spec) {
        if (output_spec_ == spec) return;
        output_spec_ = spec;
        on_output_spec_changed(spec);
    }

    void process(float *buffer, samples_t frames) {
        if (enabled_) do_self_process(buffer, frames);
    }

private:
    OutputSpec output_spec_;
    bool enabled_ = true;

    secs_t current_pos() const;
    void update_channel_gains(secs_t pos);

    void on_output_spec_changed(const OutputSpec &spec);
    void do_self_process(float *buffer, samples_t samples_per_channel);

    // Borrowed position data; current offline callers pass null.
    const TimePosition *playhead_position_ = nullptr;
    AutomatableValue<volume_db_t> volume_;
    AutomatableValue<balance_t> pan_;
    std::vector<dsp::SmoothLinearValue<float>> channel_gains_;
    std::optional<secs_t> last_gains_pos_;
};

} // namespace muse::audio::engine
