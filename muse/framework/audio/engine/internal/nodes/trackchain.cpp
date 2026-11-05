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

#include "trackchain.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

TrackChain::TrackChain(TrackId trackId, const std::string &trackName) : trackId_(trackId), trackName_(trackName) {
    setName(std::string(TrackChainTag::name) + "[" + trackName_ + "]");
}

void TrackChain::rebuild() {
    clear();

    if (signal_) {
        doAdd(signal_);
    }
    if (control_) {
        doAdd(control_);
    }
    if (fxChain_) {
        doAdd(fxChain_);
    }
    if (source_) {
        doAdd(source_);
    }

    ChainNode<TrackChainTag>::rebuild();
}

TrackId TrackChain::trackId() const { return trackId_; }

const std::string &TrackChain::trackName() const { return trackName_; }

void TrackChain::setSource(IAudioNodePtr source) {
    clear();
    source_ = source;
}

IAudioNodePtr TrackChain::source() const { return source_; }

void TrackChain::setFxChain(FxChainPtr fxChain) {
    clear();
    fxChain_ = fxChain;
}

FxChainPtr TrackChain::fxChain() const { return fxChain_; }

void TrackChain::setControl(AutomationControlNodePtr controlNode) {
    clear();
    control_ = controlNode;
}

AutomationControlNodePtr TrackChain::control() const { return control_; }

void TrackChain::setSignal(SignalNodePtr signalNode) {
    clear();
    signal_ = signalNode;
}

SignalNodePtr TrackChain::signal() const { return signal_; }
