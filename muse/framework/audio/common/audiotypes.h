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

#include <map>
#include <variant>
#include <set>
#include <string>
#include <cmath>
#include <sstream>
#include <tuple>
#include <vector>

#include "global/types/number.h"
#include "global/types/secs.h"
#include "global/types/ratio.h"
#include "global/types/string.h"
#include "global/io/iodevice.h"

#include "mpe/events.h"

#include "automatablevalue.h"

// Forward-declared audioplugins aliases; audio must not depend on that module.
namespace muse::audioplugins {
using PluginResourceId = std::string;
using PluginResourceIdList = std::vector<PluginResourceId>;
using PluginType = std::string;
} // namespace muse::audioplugins

namespace muse::audio {
using secs_t = muse::secs_t;
using msecs_t = muse::msecs_t;
using usecs_t = muse::usecs_t;

using samples_t = uint64_t;
using sample_rate_t = uint64_t;
using audioch_t = uint8_t;
using volume_db_t = db_t;
using volume_dbfs_t = db_t;
using gain_t = float;
using balance_t = NumberT<float>;

using AudioCtxId = uint16_t;

using TrackId = int32_t;
using TrackIdList = std::vector<TrackId>;
using TrackName = std::string;

using aux_channel_idx_t = uint8_t;

using PlaybackData = std::variant<mpe::PlaybackData, io::IODevice *>;
using PlaybackSetupData = mpe::PlaybackSetupData;

static constexpr TrackId invalid_track_id = -1;
static constexpr TrackId master_track_id = 0;

static constexpr char default_device_id[] = "default";

#ifdef _WIN32
static constexpr samples_t minimum_buffer_size = 256;
#else
static constexpr samples_t minimum_buffer_size = 128;
#endif

static constexpr samples_t maximum_buffer_size = 4096;

struct OutputSpec {
    sample_rate_t sample_rate = 0;
    samples_t samples_per_channel = 0;
    audioch_t audio_channel_count = 0;

    inline bool is_valid() const { return sample_rate > 0 && samples_per_channel > 0 && audio_channel_count > 0; }

    inline bool operator==(const OutputSpec &other) const {
        return sample_rate == other.sample_rate && samples_per_channel == other.samples_per_channel &&
               audio_channel_count == other.audio_channel_count;
    }

    inline bool operator!=(const OutputSpec &other) const { return !this->operator==(other); }

    std::string dump() const {
        std::stringstream ss;
        ss << "sampleRate: " << sample_rate << ", samplesPerChannel: " << samples_per_channel
           << ", audioChannelCount: " << audio_channel_count;
        return ss.str();
    }
};

enum class SoundTrackType { UNDEFINED = -1, MP3, OGG, FLAC, WAV, AAC };

enum class AudioSampleFormat { UNDEFINED = 0, INT16, INT24, FLOAT32 };

struct SoundTrackFormat {
    SoundTrackType type = SoundTrackType::UNDEFINED;
    OutputSpec output_spec;
    AudioSampleFormat sample_format = AudioSampleFormat::UNDEFINED;
    int bit_rate = 0;
    secs_t leading_silence_duration = 0.0;
    secs_t trailing_silence_duration = 0.0;

    bool operator==(const SoundTrackFormat &other) const {
        return type == other.type && output_spec == other.output_spec && sample_format == other.sample_format &&
               bit_rate == other.bit_rate && leading_silence_duration == other.leading_silence_duration &&
               trailing_silence_duration == other.trailing_silence_duration;
    }

    bool is_valid() const {
        if (!output_spec.is_valid()) {
            return false;
        }

        switch (type) {
            case SoundTrackType::WAV:
            case SoundTrackType::FLAC:
                // For lossless/uncompressed, sample format must be defined
                return sample_format != AudioSampleFormat::UNDEFINED;

            case SoundTrackType::MP3:
            case SoundTrackType::OGG:
            case SoundTrackType::AAC:
                // For lossy, bitrate must be positive
                return bit_rate > 0;

            default: return false;
        }
    }
};

struct AudioEngineConfig {
    bool auto_process_online_sounds_in_background = false;
    bool is_lazy_processing_of_online_sounds_enabled = false;
    bool use_sound_font_low_pass_filter = false;
};

using AudioSourceName = std::string;
using AudioUnitConfig = std::map<std::string, std::string>;

// Audio-pipeline resource id; distinct role from audioplugins::PluginResourceId (both std::string).
using AudioResourceId = std::string;
using AudioResourceIdList = std::vector<AudioResourceId>;

inline AudioResourceId to_audio_resource_id(const muse::audioplugins::PluginResourceId &id) { return id; }

inline AudioResourceIdList to_audio_resource_id_list(const muse::audioplugins::PluginResourceIdList &ids) {
    return AudioResourceIdList(ids.begin(), ids.end());
}

// audioplugins::PluginMeta is the cache-domain twin of AudioResourceMeta;
// kept separate so neither module depends on the other.
using AudioResourceVendor = std::string;
using AudioResourceAttributes = std::map<String, String>;

struct AudioResourceMeta {
    AudioResourceId id;
    AudioResourceVendor vendor;
    AudioResourceAttributes attributes;
    std::string type; // wire-string format identifier; see AudioResourceType enum below

    const String &attribute_val(const String &key) const {
        auto search = attributes.find(key);
        if (search != attributes.cend()) {
            return search->second;
        }
        static const String empty;
        return empty;
    }

    bool is_valid() const { return !id.empty() && !vendor.empty() && !type.empty(); }

    bool operator==(const AudioResourceMeta &other) const {
        return id == other.id && vendor == other.vendor && type == other.type && attributes == other.attributes;
    }

    bool operator!=(const AudioResourceMeta &other) const { return !(*this == other); }

    bool operator<(const AudioResourceMeta &other) const {
        return std::tie(id, vendor, type, attributes) < std::tie(other.id, other.vendor, other.type, other.attributes);
    }
};

using AudioResourceMetaList = std::vector<AudioResourceMeta>;
using AudioResourceMetaSet = std::set<AudioResourceMeta>;

inline bool bool_attribute(const AudioResourceMeta &meta, const String &key, bool fallback = false) {
    const String &v = meta.attribute_val(key);
    if (v.empty()) {
        return fallback;
    }
    return v == u"true" || v == u"1";
}

inline int int_attribute(const AudioResourceMeta &meta, const String &key, int fallback = 0) {
    const String &v = meta.attribute_val(key);
    if (v.empty()) {
        return fallback;
    }
    bool ok = true;
    int n = v.to_int(&ok);
    return ok ? n : fallback;
}

// Wire strings persisted in the plugin cache; constexpr for the RESOURCE_TYPE_NAMES map below.
inline constexpr std::string_view fluid_soundfont_type_name = "FluidSoundfont";
inline constexpr std::string_view native_effect_type_name = "NativeEffect";

// Engine-internal routing enum, distinct from the opaque audioplugins::PluginType string.
enum class AudioResourceType {
    UNDEFINED = -1,
    FLUID_SOUNDFONT,
    VST_PLUGIN,
    NATIVE_EFFECT,
    MUSE_SAMPLER_SOUND_PACK,
};

static const std::map<AudioResourceType, std::string> resource_type_names = {
    {AudioResourceType::FLUID_SOUNDFONT, std::string(::muse::audio::fluid_soundfont_type_name)},
    {AudioResourceType::VST_PLUGIN, "VstPlugin"},
    {AudioResourceType::NATIVE_EFFECT, std::string(::muse::audio::native_effect_type_name)},
    {AudioResourceType::MUSE_SAMPLER_SOUND_PACK, "MuseSamplerSoundPack"},
};

inline const std::string &resource_type_name(AudioResourceType type) {
    auto it = ::muse::audio::resource_type_names.find(type);
    if (it != ::muse::audio::resource_type_names.end()) {
        return it->second;
    }
    static const std::string empty;
    return empty;
}

inline AudioResourceType resource_type_from_string(const std::string &name) {
    for (const auto &kv : ::muse::audio::resource_type_names) {
        if (kv.second == name) {
            return kv.first;
        }
    }
    return AudioResourceType::UNDEFINED;
}

inline bool is_resource_type(const AudioResourceMeta &meta, AudioResourceType type) { return resource_type_from_string(meta.type) == type; }

static const String playback_setup_data_attribute(u"playbackSetupData");
static const String has_native_editor_support_attribute(u"hasNativeEditorSupport");

inline bool has_native_editor_support(const AudioResourceMeta &meta) {
    return bool_attribute(meta, ::muse::audio::has_native_editor_support_attribute);
}

static const std::map<AudioResourceType, String> resource_type_map = {
    {AudioResourceType::UNDEFINED, u"undefined"},
    {AudioResourceType::MUSE_SAMPLER_SOUND_PACK, u"muse_sampler_sound_pack"},
    {AudioResourceType::FLUID_SOUNDFONT, u"fluid_soundfont"},
    {AudioResourceType::VST_PLUGIN, u"vst_plugin"},
    {AudioResourceType::NATIVE_EFFECT, u"muse_plugin"},
};

static const AudioResourceId muse_reverb_id("Muse Reverb");

enum class AudioFxType {
    UNDEFINED = -1,
    VST_FX,
    MUSE_FX,
};

enum class AudioFxCategory {
    UNDEFINED = -1,
    FX_EQUALIZER,
    FX_ANALYZER,
    FX_DELAY,
    FX_DISTORTION,
    FX_DYNAMICS,
    FX_FILTER,
    FX_GENERATOR,
    FX_MASTERING,
    FX_MODULATION,
    FX_PITCH_SHIFT,
    FX_RESTORATION,
    FX_REVERB,
    FX_SURROUND,
    FX_TOOLS,
    FX_OTHER,
};

inline const std::unordered_map<AudioFxCategory, String> audio_fx_category_to_string_map{
    {AudioFxCategory::FX_EQUALIZER, u"EQ"},
    {AudioFxCategory::FX_ANALYZER, u"Analyzer"},
    {AudioFxCategory::FX_DELAY, u"Delay"},
    {AudioFxCategory::FX_DISTORTION, u"Distortion"},
    {AudioFxCategory::FX_DYNAMICS, u"Dynamics"},
    {AudioFxCategory::FX_FILTER, u"Filter"},
    {AudioFxCategory::FX_GENERATOR, u"Generator"},
    {AudioFxCategory::FX_MASTERING, u"Mastering"},
    {AudioFxCategory::FX_MODULATION, u"Modulation"},
    {AudioFxCategory::FX_PITCH_SHIFT, u"Pitch Shift"},
    {AudioFxCategory::FX_RESTORATION, u"Restoration"},
    {AudioFxCategory::FX_REVERB, u"Reverb"},
    {AudioFxCategory::FX_SURROUND, u"Surround"},
    {AudioFxCategory::FX_TOOLS, u"Tools"},
    {AudioFxCategory::FX_OTHER, u"Fx"},
};

using AudioFxCategories = std::set<AudioFxCategory>;
using AudioFxChainOrder = int8_t;

struct AudioFxParams {
    AudioFxCategories categories;
    AudioUnitConfig configuration;
    AudioResourceMeta resource_meta;
    AudioFxChainOrder chain_order = -1;
    bool active = false;

    AudioFxType type() const {
        switch (resource_type_from_string(resource_meta.type)) {
            case AudioResourceType::VST_PLUGIN: return AudioFxType::VST_FX;
            case AudioResourceType::NATIVE_EFFECT: return AudioFxType::MUSE_FX;
            case AudioResourceType::FLUID_SOUNDFONT:
            case AudioResourceType::MUSE_SAMPLER_SOUND_PACK:
            case AudioResourceType::UNDEFINED: break;
        }

        return AudioFxType::UNDEFINED;
    }

    bool operator==(const AudioFxParams &other) const {
        return active == other.active && chain_order == other.chain_order && resource_meta == other.resource_meta &&
               categories == other.categories && configuration == other.configuration;
    }

    bool operator!=(const AudioFxParams &other) const { return !(*this == other); }

    bool operator<(const AudioFxParams &other) const { return resource_meta < other.resource_meta && chain_order < other.chain_order; }

    bool is_valid() const { return resource_meta.is_valid(); }
};

using AudioFxChain = std::map<AudioFxChainOrder, AudioFxParams>;

struct AuxSendParams {
    gain_t signal_amount = 0.f; // [0; 1]
    bool active = false;

    bool operator==(const AuxSendParams &other) const {
        return real_is_equal(signal_amount, other.signal_amount) && active == other.active;
    }
};

using AuxSendsParams = std::vector<AuxSendParams>;

static constexpr volume_db_t volume_db_min = volume_db_t::make(-60.f);
static constexpr volume_db_t volume_db_max = volume_db_t::make(12.f);
static constexpr balance_t balance_min = balance_t::make(-1.f);
static constexpr balance_t balance_max = balance_t::make(1.f);

struct ControlParams {
    AutomatableValue<volume_db_t> volume;
    AutomatableValue<balance_t> balance;
    bool muted = false;

    bool operator==(const ControlParams &other) const { return muted == other.muted && volume == other.volume && balance == other.balance; }

    bool operator!=(const ControlParams &other) const { return !this->operator==(other); }
};

enum class AudioSourceType { UNDEFINED = -1, FLUID, VSTI, MUSE_SAMPLER };

inline AudioSourceType source_type_from_resource_type(const muse::audioplugins::PluginType &meta_type) {
    switch (resource_type_from_string(meta_type)) {
        case AudioResourceType::FLUID_SOUNDFONT: return AudioSourceType::FLUID;
        case AudioResourceType::VST_PLUGIN: return AudioSourceType::VSTI;
        case AudioResourceType::MUSE_SAMPLER_SOUND_PACK: return AudioSourceType::MUSE_SAMPLER;
        case AudioResourceType::NATIVE_EFFECT:
        case AudioResourceType::UNDEFINED: break;
    }

    return AudioSourceType::UNDEFINED;
}

struct AudioSourceParams {
    AudioResourceMeta resource_meta;
    AudioUnitConfig configuration;

    AudioSourceType type() const { return source_type_from_resource_type(resource_meta.type); }

    bool is_valid() const { return type() != AudioSourceType::UNDEFINED && resource_meta.is_valid(); }

    bool operator==(const AudioSourceParams &other) const {
        return type() == other.type() && resource_meta == other.resource_meta && configuration == other.configuration;
    }

    bool operator!=(const AudioSourceParams &other) const { return !this->operator==(other); }
};

using AudioInputParams = AudioSourceParams;

struct TrackParams {
    AudioInputParams source;
    AudioFxChain fx_chain;
    AuxSendsParams aux_sends;
    ControlParams control;
};

struct AudioSignalVal {
    volume_dbfs_t pressure = 0.f;

    inline bool operator==(const AudioSignalVal &other) const { return pressure == other.pressure; }
};

using AudioSignalValuesMap = std::map<audioch_t, AudioSignalVal>;

struct AutomatedControlParams {
    volume_db_t volume = 0.f;
    balance_t balance = 0.f;
};

enum class PlaybackStatus { STOPPED = 0, PAUSED, RUNNING };

using AudioDeviceID = std::string;

struct AudioDevice {
    AudioDeviceID id;
    std::string name;

    bool operator==(const AudioDevice &other) const { return id == other.id; }
};

using AudioDeviceList = std::vector<AudioDevice>;

using SoundPresetAttributes = std::map<String, String>;
static const String playing_techniques_attribute(u"playing_techniques");

struct SoundPreset {
    String code;
    String name;
    bool is_default = false;
    SoundPresetAttributes attributes;

    bool operator==(const SoundPreset &other) const {
        return code == other.code && name == other.name && is_default == other.is_default && attributes == other.attributes;
    }

    bool is_valid() const { return !code.empty(); }
};

using SoundPresetList = std::vector<SoundPreset>;

enum class ProcessMode { UNDEFINED = 0, IDLE, PLAYING, PLAYING_OFFLINE };

inline std::string to_string(ProcessMode mode) {
    switch (mode) {
        case ProcessMode::UNDEFINED: return "Undefined";
        case ProcessMode::IDLE: return "Idle";
        case ProcessMode::PLAYING: return "Playing";
        case ProcessMode::PLAYING_OFFLINE: return "PlayingOffline";
    }
    return "Unknown";
}

inline bool is_mode_playing(ProcessMode mode) { return mode == ProcessMode::PLAYING || mode == ProcessMode::PLAYING_OFFLINE; }

struct InputProcessingProgress {
    struct ChunkInfo {
        secs_t start = 0.0;
        secs_t end = 0.0;

        bool operator==(const ChunkInfo &c) const { return start == c.start && end == c.end; }
    };

    using ChunkInfoList = std::vector<ChunkInfo>;

    struct ProgressInfo {
        int64_t current = 0;
        int64_t total = 0;
    };

    enum Status : unsigned int { UNDEFINED = 0, STARTED, PROCESSING, FINISHED };

    struct StatusInfo {
        Status status = Status::UNDEFINED;
        int error_code = 0;
        std::string error_text;
        using StatusData = std::map<std::string, std::string>;
        StatusData data;
    };

    bool is_started = false;
};

enum SaveSoundTrackStage {
    UNKNOWN = 0,
    PROCESSING_ONLINE_SOUNDS,
    WRITING_SOUND_TRACK,
};

struct TransportEvent {
    enum class Type : unsigned char {
        UNKNOWN = 0,
        PLAY,
        PAUSE,
        STOP,
        SEEK,
    };

    struct SeekData {
        secs_t position = 0.;
    };

    static TransportEvent play() { return {Type::PLAY, {}}; }

    static TransportEvent pause() { return {Type::PAUSE, {}}; }

    static TransportEvent stop() { return {Type::STOP, {}}; }

    static TransportEvent seek(secs_t pos) { return {Type::SEEK, SeekData{pos}}; }

    Type type = Type::UNKNOWN;
    std::variant<std::monostate, SeekData> data;
};

using TransportEvents = std::vector<TransportEvent>;
} // namespace muse::audio

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::audio::OutputSpec &spec) {
    s << "sampleRate: " << spec.sample_rate << ", channels: " << spec.audio_channel_count
      << ", samplesPerChannel: " << spec.samples_per_channel;
    return s;
}
