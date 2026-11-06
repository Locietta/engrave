/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
#include <unordered_map>

#include "../../../isynthresolver.h"

#include "../../../iaudioengineconfiguration.h"
#include "../../../isoundfontrepository.h"

#include "fluidsynth.h"

namespace muse::audio::synth {
class FluidResolver : public ISynthResolver::IResolver {
public:
    // The repository, configuration and MIDI port (optional: it receives the
    // events every synthesizer handles) must outlive the resolver.
    FluidResolver(const ISoundFontRepository &sound_font_repository, const engine::IAudioEngineConfiguration &config,
                  midi::IMidiOutPort *midi_out_port);
    ~FluidResolver() override;

    ISynthesizerPtr resolve_synth(const audio::TrackId track_id, const audio::AudioInputParams &params,
                                  const OutputSpec &spec) const override;
    bool has_compatible_resources(const audio::PlaybackSetupData &setup) const override;

    audio::AudioResourceMetaList resolve_resources() const override;
    audio::SoundPresetList resolve_sound_presets(const AudioResourceMeta &resource_meta) const override;

    void refresh() override;
    void clear_sources() override;

private:
    FluidSynthPtr create_synth(const audio::AudioResourceId &resource_id) const;

    struct SoundFontResource {
        io::PathT path;
        std::optional<midi::Program> preset = std::nullopt;
        AudioResourceMeta meta;
    };

    const ISoundFontRepository &sound_font_repository_;
    const engine::IAudioEngineConfiguration &config_;
    midi::IMidiOutPort *midi_out_port_ = nullptr;
    std::unordered_map<AudioResourceId, SoundFontResource> resources_cache_;
};
} // namespace muse::audio::synth
