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

#include "audio/engine/internal/codecs/vorbisdecoder.h"

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::synth;

static const AudioResourceVendor fluid_vendor_name = "Fluid";

FluidResolver::FluidResolver(const ISoundFontRepository &sound_font_repository, const engine::IAudioEngineConfiguration &config,
                             midi::IMidiOutPort *midi_out_port)
    : sound_font_repository_(sound_font_repository), config_(config), midi_out_port_(midi_out_port) {

    fluid::FluidVorbisDecoder::decoder = new codec::VorbisDecoder();

    refresh();
}

FluidResolver::~FluidResolver() {
    delete fluid::FluidVorbisDecoder::decoder;
    fluid::FluidVorbisDecoder::decoder = nullptr;
}

ISynthesizerPtr FluidResolver::resolve_synth(const TrackId /*trackId*/, const AudioInputParams &params, const OutputSpec &spec) const {

    auto search = resources_cache_.find(params.resource_meta.id);
    if (search == resources_cache_.end()) {
        LOGE() << "Not found: " << params.resource_meta.id;
        return nullptr;
    }

    FluidSynthPtr synth = std::make_shared<FluidSynth>(params, config_, midi_out_port_);
    synth->init(spec);
    synth->add_sound_fonts({search->second.path});
    synth->set_preset(search->second.preset);

    return synth;
}

bool FluidResolver::has_compatible_resources(const PlaybackSetupData & /*setup*/) const { return true; }

AudioResourceMetaList FluidResolver::resolve_resources() const {

    AudioResourceMetaList result;
    result.reserve(resources_cache_.size());

    for (const auto &pair : resources_cache_) {
        result.push_back(pair.second.meta);
    }

    return result;
}

SoundPresetList FluidResolver::resolve_sound_presets(const audio::AudioResourceMeta &) const { return SoundPresetList(); }

static std::string make_id(const std::string &name, int bank, int program) {
    return name + "\\" + std::to_string(bank) + "\\" + std::to_string(program);
}

void FluidResolver::refresh() {

    resources_cache_.clear();

    for (const auto &pair : sound_font_repository_.sound_fonts()) {
        const SoundFontMeta &sound_font = pair.second;

        {
            AudioResourceId id = sound_font.name;

            AudioResourceMeta choose_automatic_meta;
            choose_automatic_meta.id = id;
            choose_automatic_meta.type = ::muse::audio::fluid_soundfont_type_name;
            choose_automatic_meta.vendor = ::fluid_vendor_name;
            choose_automatic_meta.attributes = {{::muse::audio::playback_setup_data_attribute, muse::mpe::generic_setup_data_string},
                                                {::muse::audio::synth::soundfont_name_attribute, String::from_std_string(sound_font.name)}};

            resources_cache_.emplace(id, SoundFontResource{sound_font.path, std::nullopt, std::move(choose_automatic_meta)});
        }

        for (const SoundFontPreset &preset : sound_font.presets) {
            AudioResourceId id = make_id(sound_font.name, preset.program.bank, preset.program.program);

            AudioResourceMeta meta;
            meta.id = id;
            meta.type = ::muse::audio::fluid_soundfont_type_name;
            meta.vendor = ::fluid_vendor_name;
            meta.attributes = {
                {::muse::audio::playback_setup_data_attribute, muse::mpe::generic_setup_data_string},
                {::muse::audio::synth::soundfont_name_attribute, String::from_std_string(sound_font.name)},
                {::muse::audio::synth::preset_name_attribute, String::from_std_string(preset.name)},
                {::muse::audio::synth::preset_bank_attribute, String::number(preset.program.bank)},
                {::muse::audio::synth::preset_program_attribute, String::number(preset.program.program)},
            };

            resources_cache_.emplace(id, SoundFontResource{sound_font.path, preset.program, std::move(meta)});
        }
    }
}

void FluidResolver::clear_sources() {}
