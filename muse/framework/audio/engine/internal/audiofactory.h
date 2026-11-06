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

#include "iaudiofactory.h"

#include "../isynthresolver.h"
#include "../ifxresolver.h"

namespace muse::audio::engine {
class AudioFactory : public IAudioFactory {
    OutputSpec spec_;
    synth::ISynthResolver &synth_resolver_;
    fx::IFxResolver &fx_resolver_;

    synth::ISynthResolver *synth_resolver() const { return &synth_resolver_; }

    fx::IFxResolver *fx_resolver() const { return &fx_resolver_; }

public:
    // Both resolvers must outlive the factory.
    AudioFactory(synth::ISynthResolver &synth_resolver, fx::IFxResolver &fx_resolver)
        : synth_resolver_(synth_resolver), fx_resolver_(fx_resolver) {}

    void set_output_spec(const OutputSpec &spec) { spec_ = spec; }

    // Available resources
    AudioResourceMetaList available_input_resources() const override;
    SoundPresetList available_sound_presets(const AudioResourceMeta &resource_meta) const override;
    AudioResourceMetaList available_output_resources() const override;

    // Make audio sources
    RetVal<synth::ISynthesizerPtr> make_synth(const TrackId track_id, const AudioInputParams &params,
                                              const PlaybackSetupData &setup_data) const override;
    RetVal<synth::ISynthesizerPtr> make_default_synth(const TrackId track_id) const override;
    void clear_synth_sources() override;

    RetVal<AudioSourceNodePtr> make_event_source(const TrackId track_id, const mpe::PlaybackData &playback_data,
                                                 const AudioInputParams &params) const override;

    // Make FX
    FxChainPtr make_master_fx_chain(const AudioFxChain &fx_chain) const override;
    FxChainPtr make_track_fx_chain(const TrackId track_id, const AudioFxChain &fx_chain) const override;
    void clear_all_fx() override;
};
} // namespace muse::audio::engine
