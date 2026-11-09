// SPDX-License-Identifier: GPL-3.0-only
#include "audioresources.h"

#include "internal/synthesizers/fluidsynth/sfcachedloader.h"
#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::synth;
using namespace muse::audio::engine;

static const AudioResourceVendor fluid_vendor_name = "Fluid";

AudioResources::AudioResources() : decoder(std::make_unique<codec::VorbisDecoder>()), cache(std::make_unique<SoundFontCache>()) {
    fluid::FluidVorbisDecoder::decoder = decoder.get();
    file_cache_bridge = cache.get();
    repository.set_on_sound_fonts_changed([this] { refresh_audio_resources(*this); });
}

AudioResources::~AudioResources() {
    repository.set_on_sound_fonts_changed({});
    // All render graphs and synthesizers, then the services/resolvers, are already gone.
    // Preserve the old order: detach the Vorbis bridge before releasing cached fonts.
    fluid::FluidVorbisDecoder::decoder = nullptr;
    decoder.reset();
    // The resolver metadata cache and repository used to die with services,
    // between decoder teardown and the final loaded-font cache release.
    resources.clear();
    repository.load_sound_fonts({});
    // Fluid instances deliberately leave fonts and file streams open in the cache.
    // Only after the synths have released their references may the cached defsfonts
    // be deleted, followed by their sfont wrappers and finally their file streams.
    cache.reset();
    file_cache_bridge = nullptr;
}

AudioResourceMetaList muse::audio::engine::audio_resource_list(const AudioResources &owner) {
    AudioResourceMetaList result;
    result.reserve(owner.resources.size());
    for (const auto &entry : owner.resources) result.push_back(entry.second.meta);
    return result;
}

static std::string make_id(const std::string &name, int bank, int program) {
    return name + "\\" + std::to_string(bank) + "\\" + std::to_string(program);
}

void muse::audio::engine::refresh_audio_resources(AudioResources &owner) {

    owner.resources.clear();

    for (const auto &pair : owner.repository.sound_fonts()) {
        const SoundFontMeta &sound_font = pair.second;

        {
            AudioResourceId id = sound_font.name;

            AudioResourceMeta choose_automatic_meta;
            choose_automatic_meta.id = id;
            choose_automatic_meta.type = ::muse::audio::fluid_soundfont_type_name;
            choose_automatic_meta.vendor = ::fluid_vendor_name;
            choose_automatic_meta.attributes = {{::muse::audio::playback_setup_data_attribute, muse::mpe::generic_setup_data_string},
                                                {::muse::audio::synth::soundfont_name_attribute, String::from_std_string(sound_font.name)}};

            owner.resources.emplace(id, AudioResources::SoundFontResource{sound_font.path, std::nullopt, std::move(choose_automatic_meta)});
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

            owner.resources.emplace(id, AudioResources::SoundFontResource{sound_font.path, preset.program, std::move(meta)});
        }
    }
}
