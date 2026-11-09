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

#include "fluidresolver.h"

#include "audio/engine/audioresources.h"
#include "log.h"

using namespace muse::audio;
using namespace muse::audio::synth;

ISynthesizerPtr FluidResolver::resolve_synth(const TrackId /*trackId*/, const AudioInputParams &params, const OutputSpec &spec) const {

    auto search = resources_.resources.find(params.resource_meta.id);
    if (search == resources_.resources.end()) {
        LOGE() << "Not found: " << params.resource_meta.id;
        return nullptr;
    }

    FluidSynthPtr synth = std::make_shared<FluidSynth>(params, config_, midi_out_port_, resources_);
    synth->init(spec);
    synth->add_sound_fonts({search->second.path});
    synth->set_preset(search->second.preset);

    return synth;
}

bool FluidResolver::has_compatible_resources(const PlaybackSetupData & /*setup*/) const { return true; }

FluidResolver::FluidResolver(engine::AudioResources &resources, const engine::IAudioEngineConfiguration &config,
                             midi::IMidiOutPort *midi_out_port)
    : resources_(resources), config_(config), midi_out_port_(midi_out_port) {}

AudioResourceMetaList FluidResolver::resolve_resources() const { return audio_resource_list(resources_); }

SoundPresetList FluidResolver::resolve_sound_presets(const audio::AudioResourceMeta &) const { return SoundPresetList(); }

void FluidResolver::refresh() { refresh_audio_resources(resources_); }

void FluidResolver::clear_sources() {}
