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

static const AudioResourceVendor FLUID_VENDOR_NAME = "Fluid";

FluidResolver::FluidResolver(const ISoundFontRepository &soundFontRepository, const engine::IAudioEngineConfiguration &config,
                             midi::IMidiOutPort *midiOutPort)
    : soundFontRepository_(soundFontRepository), config_(config), midiOutPort_(midiOutPort) {

    fluid::FluidVorbisDecoder::decoder = new codec::VorbisDecoder();

    refresh();
}

FluidResolver::~FluidResolver() {
    delete fluid::FluidVorbisDecoder::decoder;
    fluid::FluidVorbisDecoder::decoder = nullptr;
}

ISynthesizerPtr FluidResolver::resolveSynth(const TrackId /*trackId*/, const AudioInputParams &params, const OutputSpec &spec) const {

    auto search = resourcesCache_.find(params.resourceMeta.id);
    if (search == resourcesCache_.end()) {
        LOGE() << "Not found: " << params.resourceMeta.id;
        return nullptr;
    }

    FluidSynthPtr synth = std::make_shared<FluidSynth>(params, config_, midiOutPort_);
    synth->init(spec);
    synth->addSoundFonts({search->second.path});
    synth->setPreset(search->second.preset);

    return synth;
}

bool FluidResolver::hasCompatibleResources(const PlaybackSetupData & /*setup*/) const { return true; }

AudioResourceMetaList FluidResolver::resolveResources() const {

    AudioResourceMetaList result;
    result.reserve(resourcesCache_.size());

    for (const auto &pair : resourcesCache_) {
        result.push_back(pair.second.meta);
    }

    return result;
}

SoundPresetList FluidResolver::resolveSoundPresets(const audio::AudioResourceMeta &) const { return SoundPresetList(); }

static std::string makeId(const std::string &name, int bank, int program) {
    return name + "\\" + std::to_string(bank) + "\\" + std::to_string(program);
}

void FluidResolver::refresh() {

    resourcesCache_.clear();

    for (const auto &pair : soundFontRepository_.soundFonts()) {
        const SoundFontMeta &soundFont = pair.second;

        {
            AudioResourceId id = soundFont.name;

            AudioResourceMeta chooseAutomaticMeta;
            chooseAutomaticMeta.id = id;
            chooseAutomaticMeta.type = FLUID_SOUNDFONT_TYPE_NAME;
            chooseAutomaticMeta.vendor = FLUID_VENDOR_NAME;
            chooseAutomaticMeta.attributes = {{PLAYBACK_SETUP_DATA_ATTRIBUTE, muse::mpe::GENERIC_SETUP_DATA_STRING},
                                              {SOUNDFONT_NAME_ATTRIBUTE, String::fromStdString(soundFont.name)}};

            resourcesCache_.emplace(id, SoundFontResource{soundFont.path, std::nullopt, std::move(chooseAutomaticMeta)});
        }

        for (const SoundFontPreset &preset : soundFont.presets) {
            AudioResourceId id = makeId(soundFont.name, preset.program.bank, preset.program.program);

            AudioResourceMeta meta;
            meta.id = id;
            meta.type = FLUID_SOUNDFONT_TYPE_NAME;
            meta.vendor = FLUID_VENDOR_NAME;
            meta.attributes = {
                {PLAYBACK_SETUP_DATA_ATTRIBUTE, muse::mpe::GENERIC_SETUP_DATA_STRING},
                {SOUNDFONT_NAME_ATTRIBUTE, String::fromStdString(soundFont.name)},
                {PRESET_NAME_ATTRIBUTE, String::fromStdString(preset.name)},
                {PRESET_BANK_ATTRIBUTE, String::number(preset.program.bank)},
                {PRESET_PROGRAM_ATTRIBUTE, String::number(preset.program.program)},
            };

            resourcesCache_.emplace(id, SoundFontResource{soundFont.path, preset.program, std::move(meta)});
        }
    }
}

void FluidResolver::clearSources() {}
