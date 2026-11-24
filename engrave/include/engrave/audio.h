#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <engrave/error.h>
#include <engrave/handle.h>

namespace engrave {

// The audio formats the engine encodes. Every one of them is compiled in; a
// build without an encoder would report it through Capabilities and answer
// Unsupported for it.
enum struct AudioFormat {
    WAV,  // 32-bit float PCM, or 16/24-bit with a bit depth
    FLAC, // 16-bit or 24-bit
    OPUS, // 128 kbps Opus in an Ogg container, which the encoder resamples to 48 kHz
    MP3,  // 128 kbps
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
    AudioFormat format = AudioFormat::WAV;
    // Encoder settings; 0 keeps the format's default. bitrate_kbps applies to
    // Opus and MP3 (32..512); bit_depth to WAV (16, 24 or 32) and FLAC
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
    // Same callback contract as CaptureOptions: owner thread, no reentry;
    // exceptions propagate. Polled during setup, then per synthesis block (and
    // about every millisecond while tracks synthesize in parallel) and
    // encoded-file write, including finalization. No sink is called when
    // cancellation is observed. prepare_audio polls it during setup only;
    // PreparedAudio::run takes its own callback.
    std::function<bool()> cancelled;
};

// Receives the finished encoded stream in chunks after the intermediate file has
// been finalized. An exception thrown by the sink reaches the caller.
using ByteSink = std::function<void(std::span<const std::byte> chunk)>;

struct AudioRenderResult {
    std::uint64_t bytes = 0;  // what the sink received, or the size of the file PreparedAudio::run wrote
    std::uint64_t frames = 0; // sample frames the engine rendered
    double seconds = 0.0;     // the encoded span: play time plus the 3 s tail
};

// An audio render that Document::prepare_audio set up on the Engine's thread:
// every track is loaded and nothing refers to the Document any more, so the
// Document may be edited, rendered or destroyed while run() synthesizes.
//
// One render at a time per Engine: from prepare_audio until the PreparedAudio
// holding the render is destroyed, whether or not run() was called,
// prepare_audio and render_audio answer ResourceLimit. Destroying it (on any
// thread) tears the render graph down and then frees the Engine for the next
// render. It does not keep the Engine alive: destroy every PreparedAudio before
// the Engine and all of its Documents are gone; releasing the last of them
// while a render is held aborts.
struct PreparedAudio {
    PreparedAudio(PreparedAudio &&) noexcept;
    PreparedAudio &operator=(PreparedAudio &&) noexcept;
    ~PreparedAudio();

    // Synthesizes, mixes and encodes on the calling thread, which may be any
    // thread (one call at a time), writing directly to output_path. Its parent
    // directory must exist: IO otherwise. A render runs once; InvalidArgument
    // for a second call, a moved-from object or an empty path. Cancelled,
    // ResourceLimit (a byte limit of the AudioOptions) and IO as render_audio
    // reports them; throws std::runtime_error if the engine's audio graph or
    // encoder fails. Cancellation, a byte limit or an exception removes
    // output_path. `cancelled` is polled only on this thread (worker and encoder
    // threads see only stop flags) and must not reenter the Engine or any
    // Document; its exceptions propagate after cleanup.
    Result<AudioRenderResult> run(const std::filesystem::path &output_path, std::function<bool()> cancelled = {});

    // Document::revision() when prepare_audio ran.
    std::uint64_t source_revision() const noexcept;

private:
    friend struct Document;
    struct Impl;
    explicit PreparedAudio(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace engrave
