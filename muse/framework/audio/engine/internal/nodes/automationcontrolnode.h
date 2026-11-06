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

#include "audio/engine/internal/dsp/smoothlinearvalue.h"
#include "audio/engine/iplayhead.h"
#include "audionode.h"

namespace muse::audio {
struct AutomationControlTag {
    static constexpr const char *name = "AutomationControl";
};
} // namespace muse::audio

namespace muse::audio::engine {
struct AutomationControlNode : AudioNode<AutomationControlTag> {

    void set_playhead_position(const PlayheadPositionPtr &playhead_position);
    void set_volume(const AutomatableValue<volume_db_t> &volume);
    void set_pan(const AutomatableValue<balance_t> &pan);
    void set_muted(bool muted);
    bool muted() const;

protected:

    secs_t current_pos() const;
    void update_channel_gains(secs_t pos);

    void on_output_spec_changed(const OutputSpec &spec) override;
    void do_self_process(float *buffer, samples_t samples_per_channel) override;

    PlayheadPositionPtr playhead_position_;
    AutomatableValue<volume_db_t> volume_;
    AutomatableValue<balance_t> pan_;
    std::vector<dsp::SmoothLinearValue<float>> channel_gains_;
    std::optional<secs_t> last_gains_pos_;
};

using AutomationControlNodePtr = std::shared_ptr<AutomationControlNode>;
} // namespace muse::audio::engine
