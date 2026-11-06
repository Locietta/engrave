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

#include "audio/engine/ifxprocessor.h"
#include "audio/engine/iplayhead.h"
#include "chainnode.h"

namespace muse::audio {
struct FxChainTag {
    static constexpr const char *name = "FxChain";
};
} // namespace muse::audio

namespace muse::audio::engine {
struct FxChain : ChainNode<FxChainTag> {
    void set_fx_list(const std::vector<IFxProcessorPtr> &fx_list);

    //! NOTE Must be set after adding all nodes to the chain
    void set_fx_chain_spec(const AudioFxChain &fx_chain_spec);
    const AudioFxChain &fx_chain_spec() const;

    void set_playhead_position(PlayheadPositionPtr playhead_position);

    bool should_process_during_silence() const;

private:
    void rebuild() override;
    void do_self_process(float *buffer, samples_t samples_per_channel) override;

    void update_should_process_during_silence();

    AudioFxChain fx_chain_spec_;

    bool should_process_during_silence_ = false;
};

using FxChainPtr = std::shared_ptr<FxChain>;
} // namespace muse::audio::engine
