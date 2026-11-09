// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio/common/audiotypes.h"
#include "global/flatmap.h"
#include "internal/codecs/vorbisdecoder.h"
#include "internal/synthesizers/soundfontrepository.h"

namespace muse::audio::synth {
struct SoundFontCache;
} // namespace muse::audio::synth

namespace muse::audio::engine {
// One owner per engine. Services and all render sessions must die before this owner.
struct AudioResources {
    AudioResources();
    ~AudioResources();
    AudioResources(const AudioResources &) = delete;
    AudioResources &operator=(const AudioResources &) = delete;

    struct SoundFontResource {
        io::PathT path;
        std::optional<midi::Program> preset = std::nullopt;
        AudioResourceMeta meta;
    };

    synth::SoundFontRepository repository;
    FlatMap<AudioResourceId, SoundFontResource> resources;
    // Committed setup order; index zero is the default, independent of map order.
    std::vector<std::string> soundfont_paths_value;
    std::vector<AudioInputParams> font_inputs;
    std::string soundfont_uri;
    std::string soundfont_id;
    std::unique_ptr<codec::VorbisDecoder> decoder;
    std::unique_ptr<synth::SoundFontCache> cache;
};

void refresh_audio_resources(AudioResources &resources);
AudioResourceMetaList audio_resource_list(const AudioResources &resources);
} // namespace muse::audio::engine
