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

#include "audiofactory.h"

#include "audio/common/audioerrors.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

FxChainPtr AudioFactory::make_master_fx_chain(const AudioFxChain &fx_chain) const {
    std::vector<IFxProcessorPtr> fxlist = fx_resolver()->resolve_master_fx_list(fx_chain, spec_);

    FxChainPtr chain = std::make_shared<FxChain>();
    chain->set_name("FxChain [master]");
    chain->set_fx_list(fxlist);
    chain->set_fx_chain_spec(fx_chain);

    return chain;
}

FxChainPtr AudioFactory::make_track_fx_chain(const TrackId track_id, const AudioFxChain &fx_chain) const {
    std::vector<IFxProcessorPtr> fxlist = fx_resolver()->resolve_fx_list(track_id, fx_chain, spec_);

    FxChainPtr chain = std::make_shared<FxChain>();
    if (track_id == ::muse::audio::master_track_id) {
        chain->set_name("FxChain [master]");
    } else {
        chain->set_name("FxChain [track " + std::to_string(track_id) + "]");
    }
    chain->set_fx_list(fxlist);
    chain->set_fx_chain_spec(fx_chain);

    return chain;
}

void AudioFactory::clear_all_fx() { fx_resolver()->clear_all_fx(); }
