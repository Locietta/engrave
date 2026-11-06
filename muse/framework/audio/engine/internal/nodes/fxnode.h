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
#include "../../ifxprocessor.h"
#include "../../iplayhead.h"

namespace muse::audio {
struct FxTag {
    static constexpr const char *name = "Fx";
};
} // namespace muse::audio

namespace muse::audio::engine {
class FxNode : public AudioNode<FxTag> {
public:

    explicit FxNode(IFxProcessorPtr fx_processor, PlayheadPositionPtr playhead_position = nullptr);

    void set_playhead_position(PlayheadPositionPtr playhead_position);

    const AudioFxParams &params() const;

    bool should_process_during_silence() const;

protected:

    void on_mode_changed(const ProcessMode mode) override;
    void on_output_spec_changed(const OutputSpec &spec) override;

    void do_self_process(float *buffer, samples_t samples_per_channel) override;

    IFxProcessorPtr fx_processor_;
    PlayheadPositionPtr playhead_position_;
};

using FxNodePtr = std::shared_ptr<FxNode>;
} // namespace muse::audio::engine
