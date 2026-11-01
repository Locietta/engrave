#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <engrave/handle.h>

namespace engrave {

// The audio formats the engine encodes. Every one of them is compiled in; a
// build without an encoder would report it through Capabilities and answer
// Unsupported for it.
enum struct AudioFormat {
    Wav,  // 32-bit float PCM, or 16/24-bit with a bit depth
    Flac, // 16-bit or 24-bit
    Opus, // 128 kbps Opus in an Ogg container, which the encoder resamples to 48 kHz
    Mp3,  // 128 kbps
    Aac,  // 128 kbps, ADTS
};

std::string_view to_string(AudioFormat format) noexcept;

// One track's mix values, in the score's audiosettings.json representation.
struct AudioMix {
    double volume_db = 0.0; // -120..24
    double balance = 0.0;   // -1 (left) .. 1 (right)
    bool mute = false;
    bool solo = false;
};

// One playback track of the selected score, in the order the engine plays and
// AudioOptions names them.
struct AudioTrackInfo {
    std::int32_t track = 0; // the value AudioTrackOptions::track uses
    std::string part_id;
    std::string instrument_id;
    std::string part_name;
    std::string instrument_name;
    bool chord_symbols = false; // the part's chord-symbol pseudo track, always muted
    AudioMix mix;               // the document's default for this track
};

struct AudioTracks {
    std::vector<AudioTrackInfo> tracks;
    AudioMix master; // the document's default master volume and balance
};

// Which SoundFont a track plays: nothing for the Engine's default (the first of
// current SoundFont list), an index into that list, a current file's stem, or
// its path from Engine::capabilities() encoded as UTF-8 with '/' separators.
using SoundFontSelection = std::variant<std::monostate, std::int32_t, std::string>;

// One track's overrides. A field that is unset keeps the document's default, as
// audio_tracks reports it.
struct AudioTrackOptions {
    std::int32_t track = 0;
    std::optional<double> volume_db;
    std::optional<double> balance;
    std::optional<bool> mute;
    std::optional<bool> solo;
    SoundFontSelection soundfont;
    // Reserved: selecting a preset is not supported and answers Unsupported.
    std::string preset;
};

struct AudioOptions {
    ScoreSel score;
    AudioFormat format = AudioFormat::Wav;
    // Encoder settings; 0 keeps the format's default. bitrate_kbps applies to
    // Opus, Mp3 and Aac (32..512); bit_depth to Wav (16, 24 or 32) and Flac
    // (16 or 24).
    std::int32_t bitrate_kbps = 0;
    std::int32_t bit_depth = 0;
    bool use_repeats = true;
    std::optional<double> master_volume_db;
    std::optional<double> master_balance;
    std::vector<AudioTrackOptions> tracks;
    // The Muse Reverb aux return; the send is 0..1 and defaults to the 0.30
    // desktop mixes use. With the reverb off the render is the dry mix.
    bool reverb = true;
    double reverb_send = 0.30;
    // 0 takes the Engine's rate, which EngineOptions::audio_sample_rate fixed;
    // any other value must equal it, and is Unsupported when it does not.
    std::int32_t sample_rate = 0;
    // Hard limits on the seekable intermediate encoded file. max_bytes limits
    // its final extent (and therefore bytes sent to the sink); max_write_bytes
    // separately counts every successful write, including header rewrites.
    // Unset means unlimited apart from the engine's existing 1 GiB projection.
    // A limit hit discards the file and calls no sink. Budget for the encoder's
    // headers and final flush as well as the repeat-expanded score and 3 s audio
    // tail. The engine renders stereo at sample_rate: WAV grows with rate and
    // bit depth; FLAC also depends on compressibility; lossy formats depend on
    // bitrate, packet/container overhead and the encoder's final flush.
    std::optional<std::uint64_t> max_bytes;
    std::optional<std::uint64_t> max_write_bytes;
};

// Receives the finished encoded stream in chunks after the intermediate file has
// been finalized. An exception thrown by the sink reaches the caller.
using ByteSink = std::function<void(std::span<const std::byte> chunk)>;

struct AudioRenderResult {
    std::uint64_t bytes = 0;    // what the sink received
    std::uint64_t frames = 0;   // sample frames the engine rendered
    double seconds = 0.0;       // the encoded span: play time plus the 3 s tail
};

} // namespace engrave
