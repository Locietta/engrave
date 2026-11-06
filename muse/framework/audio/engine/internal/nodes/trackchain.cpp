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

TrackChain::TrackChain(TrackId track_id, const std::string &track_name) : track_id_(track_id), track_name_(track_name) {
    set_name(std::string(TrackChainTag::name) + "[" + track_name_ + "]");
}

void TrackChain::rebuild() {
    clear();

    if (signal_) {
        do_add(signal_);
    }
    if (control_) {
        do_add(control_);
    }
    if (fx_chain_) {
        do_add(fx_chain_);
    }
    if (source_) {
        do_add(source_);
    }

    ChainNode<TrackChainTag>::rebuild();
}

TrackId TrackChain::track_id() const { return track_id_; }

const std::string &TrackChain::track_name() const { return track_name_; }

void TrackChain::set_source(IAudioNodePtr source) {
    clear();
    source_ = source;
}

IAudioNodePtr TrackChain::source() const { return source_; }

void TrackChain::set_fx_chain(FxChainPtr fx_chain) {
    clear();
    fx_chain_ = fx_chain;
}

FxChainPtr TrackChain::fx_chain() const { return fx_chain_; }

void TrackChain::set_control(AutomationControlNodePtr control_node) {
    clear();
    control_ = control_node;
}

AutomationControlNodePtr TrackChain::control() const { return control_; }

void TrackChain::set_signal(SignalNodePtr signal_node) {
    clear();
    signal_ = signal_node;
}

SignalNodePtr TrackChain::signal() const { return signal_; }
