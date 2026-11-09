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

#include "audio/common/audiotypes.h"

namespace muse::audio::engine {
struct OfflineSignal {

    bool is_silent() const;

    void set_output_spec(const OutputSpec &spec) {
        if (output_spec_ == spec) return;
        output_spec_ = spec;
        on_output_spec_changed(spec);
    }

    void process(float *buffer, samples_t frames) { do_self_process(buffer, frames); }

private:
    OutputSpec output_spec_;

    void on_output_spec_changed(const OutputSpec &spec);

    void do_self_process(float *buffer, samples_t samples_per_channel);

    std::vector<float> channel_peaks_;
    bool is_silent_ = true;
};

} // namespace muse::audio::engine
