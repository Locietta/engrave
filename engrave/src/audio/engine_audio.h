// SPDX-License-Identifier: GPL-3.0-only
//
// Offline driver for the retained audio graph, synthesizers and encoders.
// Services are initialized directly; no RPC channel or controller is installed.
//
// Process-wide audio services and SoundFont resources require one engine per
// process and calls on its owner thread. Each export owns a fresh render graph
// and releases its tracks, synth sources and effects before returning. The
// native runtime must be initialized before initialize_engine_audio().

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio/engine/internal/export/audioencoder.h"
#include "engraving/types/types.h"
#include "midi/miditypes.h"

namespace muse::mpe {
struct ArticulationProfilesRepository;
} // namespace muse::mpe

namespace muse::io {
struct IODevice;
} // namespace muse::io

namespace muse::audio {
struct SoundTrackFormat;

} // namespace muse::audio

namespace mu::engraving {
struct MasterScore;
} // namespace mu::engraving

namespace music {

// The engine's audio services, SoundFonts and render state. EngineContext owns
// the only instance and creates it idle; initialize_engine_audio() builds the
// services on first use. profiles must outlive it.
struct EngineAudio {
    explicit EngineAudio(const muse::mpe::ArticulationProfilesRepository &profiles);
    ~EngineAudio();

    EngineAudio(const EngineAudio &) = delete;
    EngineAudio &operator=(const EngineAudio &) = delete;

    struct State;

    State &state() const { return *state_; }

private:
    std::unique_ptr<State> state_;
};

// MIDI golden observer: FluidTrack calls this after the legacy staff % 16 remap.
// Frame is the beginning of the writer's 4096-frame chunk, not an intra-chunk
// event offset. The probe retains its existing event schema and truncation limit.
// Setup/program/expression updates outside handle_event are not captured.
// Startup flush reports all-notes-off, sustain and pitch bend for last_staff
// when available. An empty hook disables capture. Calls run on the engine
// thread; the hook must not re-enter the engine.
using EngineAudioMidiEventHook = std::function<void(const muse::midi::Event &, std::uint64_t frame)>;
void set_engine_audio_midi_event_hook(EngineAudio &audio, EngineAudioMidiEventHook hook);

// Initialises the engine's audio services and loads its SoundFonts:
//   Capturing MIDI output, owned audio services, explicit
//   configuration/synth/engine initialization, then SoundFont loading.
// The first font of the list is the default: every track plays it unless its
// request names another index. A SoundFont's resource id is its complete basename
// ("FluidR3Mono_GM" for ".../FluidR3Mono_GM.sf3"), matching
// FluidSoundFontParser::parseSoundFont (fluidsoundfontparser.cpp:70) and
// refresh_audio_resources in audioresources.cpp.
//
// The sample rate is fixed here for the life of the process, like the context
// output spec it feeds; every render the engine writes is at that rate.
//
// Returns false and fills `error` (when not null) if the engine was already
// initialised at a different rate, a path is not .sf2/.sf3, the
// native runtime/IFileSystem is missing, a font cannot be parsed, or a parsed
// font does not show up as an engine input resource. A failed replacement
// attempts to restore the previous repository. Failure during initial service
// construction does not promise that a retry is safe.
struct EngineAudioSetup {
    std::vector<std::string> soundfonts; // in order; the first is the default
    int sample_rate = 44100;
};

// Fully parse candidates without constructing audio services or refreshing live
// SoundFont handles. The caller supplies immutable paths when retaining a list.
bool validate_engine_soundfonts(const std::vector<std::string> &soundfonts, std::string *error = nullptr);
bool initialize_engine_audio(EngineAudio &audio, const EngineAudioSetup &setup, std::string *error = nullptr);
// One SoundFont at 44100 Hz.
bool initialize_engine_audio(EngineAudio &audio, const std::string &soundfont_path, std::string *error = nullptr);
bool engine_audio_initialized(const EngineAudio &audio);

// SoundFont identity of the successfully initialised engine's default (first)
// font (empty when not initialised). The URI is the encoded "file://" form used
// as the repository key; it lets callers verify the Windows drive-letter/UTF-8
// round trip.
std::string engine_soundfont_resource_id(const EngineAudio &audio);
std::string engine_soundfont_uri(const EngineAudio &audio);

enum class EngineAudioFormat {
    WAV,
    FLAC,
    OGG,
    MP3,
    AAC,
};

// Per-track mix override. Entries are indexed by the public trackId returned
// by engine_audio_tracks() (and score_audio_tracks / metadata.tracks), i.e. by
// position in the selected score's track list. Tracks without an entry use
// neutral values. `muted` is folded with the derived forceMute (any solo and
// this track is not solo) and with the engine's chord-symbol/metronome mute
// rules; see export_score_audio().
struct EngineAudioTrackParams {
    double volume_db = 0.0;
    double balance = 0.0;
    bool solo = false;
    bool muted = false;
    // Index into EngineAudioSetup::soundfonts, or -1 for the default (the
    // first). Every track resolves its own Fluid source from it.
    int soundfont = -1;
};

struct AudioRenderRequest {
    EngineAudioFormat format = EngineAudioFormat::WAV;
    std::string output_path;
    // Internal test seam; an empty factory uses the format's built-in encoder.
    std::function<muse::audio::encode::UniqueEncoder(const muse::audio::SoundTrackFormat &, muse::io::IODevice &)> encoder_factory;
    // -1 renders the master score. A non-negative value renders the master
    // score with every part that is absent from, or invisible in, that excerpt
    // muted, like MasterNotation::initNotationSoloMuteState
    // (masternotation.cpp:748-770).
    int excerpt_id = -1;
    bool use_repeats = true;
    double master_volume_db = 0.0;
    double master_balance = 0.0;
    // Indexed by engine_audio_tracks(master, excerpt_id) order.
    std::vector<EngineAudioTrackParams> tracks;
    // Encoder settings. bit_rate is in kbps and applies to MP3, Ogg and AAC;
    // bit_depth is 16 or 24 for WAV and FLAC, and 32 for a Float32 WAV. 0 keeps
    // the format's default (128 kbps; Float32 WAV, Int16 FLAC).
    int bit_rate = 0;
    int bit_depth = 0;
    std::optional<std::uint64_t> max_bytes;
    std::optional<std::uint64_t> max_write_bytes;
    // The Muse Reverb aux send: on by default at 0.30, as desktop mixes; off
    // sends nothing to the reverb return and renders the dry mix.
    bool reverb = true;
    double reverb_send = 0.30;
    // Kept for source compatibility. 4.7.6 waited for saveSoundTrack()'s
    // promise and could at least catch a promise that never ran; 5.0's
    // SoundTrackWriter renders synchronously inside export_score_audio()
    // (soundtrackwriter.cpp:96-121 -> 133-207), so this driver no longer pumps
    // an event loop and cannot honour a timeout. A caller that needs a deadline
    // must enforce it out of process: for the synchronous export the parent's
    // duration-derived deadline is the real limit (engine_audio.cpp).
    std::int64_t timeout_ms = 0;
    std::function<bool()> cancelled;
};

struct AudioRenderResult {
    // Why a render failed. EngineFailure is the only kind the caller cannot act
    // on: the engine's graph, synthesizer or encoder failed.
    enum class Failure {
        NONE,
        CANCELLED,
        INVALID_SCORE,  // nothing to play, or no length to play it
        RESOURCE_LIMIT, // the projected output exceeds the 1 GiB cap
        IO,             // opening, writing or closing the output failed
        ENGINE_FAILURE,
    };

    bool ok = false;
    Failure failure = Failure::NONE;
    std::string error;
    // Total sample frames the writer emits: (repeat-expanded play time + 3 s,
    // truncated to milliseconds) at 44100 Hz. Computed from the same duration
    // handed to SoundTrackWriter (engine_audio.cpp); the writer has no
    // frames-written return value.
    std::uint64_t frames = 0;
    // Repeat-expanded score play time, without the 3 s tail.
    double play_time_secs = 0.0;
    // Audio span of the written file: play_time_secs + 3 s, in milliseconds.
    double duration_secs = 0.0;
    // File size after a successful export; 0 when the file does not exist.
    std::uint64_t bytes = 0;
};

// Renders `master` with the engine and writes one audio file. The request
// format selects the upstream encoder defaults, which bit_rate/bit_depth can
// override (engine_audio.cpp validates neither: the caller does).
//   WAV  Float32, FLAC Int16, MP3 128 kbps, OGG 128 kbps (Opus in Ogg),
//   AAC  128 kbps (ADTS; desktop AacWriter reuses exportMp3Bitrate).
// The engine renders at the rate initialize_engine_audio() fixed, in 4096-frame
// chunks. Ogg/Opus is resampled to 48 kHz inside libopusenc (upstream OggWriter
// behavior too), so the decoded Ogg stream is 48 kHz while its duration follows
// the engine render.
//
// A PlaybackScoreBuilder walks the score once into a flat, repeat-expanded MPE
// store; each track's sequencer lowers it, and the store is released before
// synthesis. Tracks use explicit SoundFont resources and two
// auxiliary returns (Muse Reverb and an empty return). The retained nested
// master chains preserve the existing gain initialization and mix behavior.
// The writer clears its additive mix buffer before each 4096-frame block,
// finalizes the encoder, and returns through the existing public ByteSink
// adapter (which uses a seekable temporary file for header finalization).
//
// The metronome and chord-symbol tracks are added when the score walk creates
// them, but are always muted (the metronome/chord playback switches
// are off, like PlaybackController::trackOutputParams). request.tracks is
// indexed by the track list for the same excerpt (engine_audio_tracks);
// tracks outside the excerpt keep neutral values but are muted as described
// above.
//
// Never throws for a bad score/format/engine state; inspect AudioRenderResult.
// Exceptions from request.cancelled are rethrown after closing/removing the
// intermediate and idling the render graph; none unwind through codec callbacks.
AudioRenderResult export_score_audio(EngineAudio &audio, mu::engraving::MasterScore *master, const AudioRenderRequest &request);

// One playable track for score_audio_tracks. `id` is the raw score identity
// (muse::ID hex string / muse::String); the public trackId is this entry's
// zero-based index in the list. The chord-symbol pseudo track has
// chord_symbols == true. The metronome pseudo track is not listed because it
// is not addressable through a part/instrument identity.
struct EngineAudioTrackInfo {
    mu::engraving::InstrumentTrackId id;
    std::string part_id;
    std::string instrument_id;
    std::string part_name;
    std::string instrument_name;
    bool chord_symbols = false;
    // Zero-based index in MasterScore::parts().
    std::size_t part_index = 0;
};

// Lists the selected score's parts, their instrument tracks (one per
// InstrumentTrackId, including instrument changes) and chord-symbol
// pseudo-tracks in deterministic score order; the list index is the public
// trackId. excerpt_id == -1 lists the master score; a non-negative value lists
// only the excerpt's parts, matching what export_score_audio() renders.
// Throws std::invalid_argument for a null master or an invalid excerpt id.
std::vector<EngineAudioTrackInfo> engine_audio_tracks(mu::engraving::MasterScore *master, int excerpt_id = -1);

} // namespace music
