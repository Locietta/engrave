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

#include "nodes/eventaudionode.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

AudioResourceMetaList AudioFactory::available_input_resources() const { return synth_resolver()->resolve_available_resources(); }

SoundPresetList AudioFactory::available_sound_presets(const AudioResourceMeta &resource_meta) const {
    return synth_resolver()->resolve_available_sound_presets(resource_meta);
}

AudioResourceMetaList AudioFactory::available_output_resources() const { return fx_resolver()->resolve_available_resources(); }

RetVal<synth::ISynthesizerPtr> AudioFactory::make_synth(const TrackId track_id, const AudioInputParams &params,
                                                        const PlaybackSetupData &setup_data) const {
    auto synth = synth_resolver()->resolve_synth(track_id, params, spec_, setup_data);
    if (!synth) {
        return RetVal<synth::ISynthesizerPtr>::make_ret(Err::INVALID_SYNTH);
    }
    return RetVal<synth::ISynthesizerPtr>::make_ok(synth);
}

RetVal<synth::ISynthesizerPtr> AudioFactory::make_default_synth(const TrackId track_id) const {
    auto synth = synth_resolver()->resolve_default_synth(track_id);
    if (!synth) {
        return RetVal<synth::ISynthesizerPtr>::make_ret(Err::INVALID_SYNTH);
    }
    return RetVal<synth::ISynthesizerPtr>::make_ok(synth);
}

void AudioFactory::clear_synth_sources() { synth_resolver()->clear_sources(); }

RetVal<AudioSourceNodePtr> AudioFactory::make_event_source(const TrackId track_id, const mpe::PlaybackData &playback_data,
                                                           const AudioInputParams &params) const {
    EventAudioNodePtr source = std::make_shared<EventAudioNode>(*this, track_id, playback_data);
    source->set_output_spec(spec_);
    source->apply_input_params(params);
    return RetVal<AudioSourceNodePtr>::make_ok(source);
}

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
