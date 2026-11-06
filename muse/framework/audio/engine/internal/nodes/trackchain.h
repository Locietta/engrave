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

#include "audiosourcenode.h"
#include "fxchain.h"
#include "automationcontrolnode.h"
#include "signalnode.h"

namespace muse::audio {
struct TrackChainTag {
    static constexpr const char *name = "TrackChain";
};
} // namespace muse::audio

namespace muse::audio::engine {
//! NOTE A typical node chain for processing a track:
//! signalnode <- controlnode <- fxnode[] <- audiosource
class TrackChain : public ChainNode<TrackChainTag> {
public:

    TrackChain(TrackId track_id, const std::string &track_name);

    TrackId track_id() const;
    const std::string &track_name() const;

    void set_source(IAudioNodePtr source);
    IAudioNodePtr source() const;

    void set_fx_chain(FxChainPtr fx_chain);
    FxChainPtr fx_chain() const;

    void set_control(AutomationControlNodePtr control_node);
    AutomationControlNodePtr control() const;

    void set_signal(SignalNodePtr signal_node);
    SignalNodePtr signal() const;

    void rebuild() override;

protected:

    void do_self_process(float *, samples_t) override {}

    TrackId track_id_ = ::muse::audio::invalid_track_id;
    std::string track_name_;

    IAudioNodePtr source_;
    FxChainPtr fx_chain_;
    AutomationControlNodePtr control_;
    SignalNodePtr signal_;
};

using TrackChainPtr = std::shared_ptr<TrackChain>;
} // namespace muse::audio::engine
