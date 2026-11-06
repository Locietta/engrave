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

#include "audionode.h"

namespace muse::audio {
struct SignalTag {
    static constexpr const char *name = "Signal";
};
} // namespace muse::audio

namespace muse::audio::engine {
struct SignalNode : AudioNode<SignalTag> {

    bool is_silent() const;

protected:

    void on_output_spec_changed(const OutputSpec &spec) override;

    void do_self_process(float *buffer, samples_t samples_per_channel) override;

    std::vector<float> channel_peaks_;
    bool is_silent_ = true;
};

using SignalNodePtr = std::shared_ptr<SignalNode>;
} // namespace muse::audio::engine
