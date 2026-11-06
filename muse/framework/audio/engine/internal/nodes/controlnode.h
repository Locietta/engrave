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
struct ControlTag {
    static constexpr const char *name = "Control";
};
} // namespace muse::audio

namespace muse::audio::engine {
struct ControlNode : AudioNode<ControlTag> {

    void set_volume(float value);
    float volume() const;
    void set_pan(float pan);
    float pan() const;
    void set_muted(bool muted);
    bool muted() const;

protected:

    void update_channel_gains();

    void on_output_spec_changed(const OutputSpec &spec) override;
    void do_self_process(float *buffer, samples_t samples_per_channel) override;

    std::vector<float> channel_gains_;

    float volume_ = 1.0f;
    float pan_ = 0.0f;
};

using ControlNodePtr = std::shared_ptr<ControlNode>;
} // namespace muse::audio::engine
