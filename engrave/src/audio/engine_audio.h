// SPDX-License-Identifier: GPL-3.0-only
//
// Offline driver for the retained audio graph, synthesizers and encoders.
// Services are initialized directly; no RPC channel or controller is installed.
//
// Process-wide audio services and SoundFont resources require one engine per
// process and calls on its owner thread. Each export owns a fresh render graph
// and releases its tracks, synth sources and effects before returning. Track
// synthesis may fan out to a worker pool the engine audio owns. An export is
// prepared on the owner thread and run on any one thread (one render at a time
// per EngineAudio); every callback (cancellation, MIDI hook, encoder factory)
// runs on the thread of the step it belongs to. The native runtime must be
// initialized before initialize_engine_audio().

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio/engine/internal/export/audioencoder.h"
#include "engraving/types/types.h"
#include "global/fn.h"
#include "global/unique_fn.h"
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
// when available. An empty hook disables capture. Calls run on the thread
// that prepares (setup events) or runs the render; the hook must not re-enter
// the engine and must not change while a render is active. While a hook is
// installed, tracks are synthesized serially so events keep track order.
using MidiEventCallback = void(const muse::midi::Event &, std::uint64_t);
using EngineAudioMidiEventHook = muse::UniqueFn<MidiEventCallback>;
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
// Returns false and fills `error` (when not null) while a PreparedAudioRender
// is alive, if the engine was already
// initialised at a different rate, a path is not .sf2/.sf3, the
// native runtime/NativeFileSystem is missing, a font cannot be parsed, or a parsed
// font does not show up as an engine input resource. A failed replacement
// attempts to restore the previous repository. Failure during initial service
// construction does not promise that a retry is safe.
struct EngineAudioSetup {
    std::vector<std::string> soundfonts; // in order; the first is the default
    int sample_rate = 44100;
    // Per-track synthesis threads, as engrave::EngineOptions::audio_threads;
    // negative is rejected. Unlike the rate it may change between calls.
    int audio_threads = 0;
};

// Fully parse candidates without constructing audio services or refreshing live
// SoundFont handles. The caller supplies immutable paths when retaining a list.
bool validate_engine_soundfonts(const std::vector<std::string> &soundfonts, std::string *error = nullptr);
bool initialize_engine_audio(EngineAudio &audio, const EngineAudioSetup &setup, std::string *error = nullptr);
// One SoundFont at 44100 Hz.
bool initialize_engine_audio(EngineAudio &audio, const std::string &soundfont_path, std::string *error = nullptr);
bool engine_audio_initialized(const EngineAudio &audio);
// True while prepare_score_audio() runs and then until the PreparedAudioRender
// it returned (and therefore its render session) has been destroyed. Any thread.
bool engine_audio_render_active(const EngineAudio &audio);
// Threads the synthesis pool has started. Renders reuse them, so the count only
// grows until the EngineAudio is destroyed.
std::size_t engine_audio_worker_threads(const EngineAudio &audio);

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

using EncoderFactory = muse::audio::encode::UniqueEncoder(const muse::audio::SoundTrackFormat &, muse::io::IODevice &) const;

struct AudioRenderRequest {
    EngineAudioFormat format = EngineAudioFormat::WAV;
    // Read only by export_score_audio(); a prepared render takes its path at run time.
    std::string output_path;
    // Internal test seam; an empty factory uses the format's built-in encoder.
    // Read only by export_score_audio().
    muse::UniqueFn<EncoderFactory> encoder_factory;
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
    // Encoder settings. bit_rate is in kbps and applies to MP3 and Ogg Opus;
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
    muse::UniqueFn<bool() const> cancelled;
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
//   WAV Float32, FLAC Int16, MP3 128 kbps, OGG 128 kbps (Opus in Ogg).
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
// Muted tracks (including metronome, chord-symbol, solo-excluded, and
// excerpt-excluded tracks) and note-free tracks are skipped instead of added
// to the render session, so no FluidSynth instance is created for them.
// request.tracks is indexed by the track list for the same excerpt
// (engine_audio_tracks); tracks outside the excerpt keep neutral values and
// are skipped as muted.
//
// Never throws for a bad score/format/engine state; inspect AudioRenderResult.
// Exceptions from request.cancelled are rethrown after closing/removing the
// intermediate and idling the render graph; none unwind through codec callbacks.
// prepare_score_audio() followed by run_prepared_audio() on the same thread.
AudioRenderResult export_score_audio(EngineAudio &audio, mu::engraving::MasterScore *master, const AudioRenderRequest &request);

// A render graph with every track loaded, owning no reference to the score, its
// playback model or the request. While one exists (including after it ran), its
// EngineAudio refuses another prepare_score_audio() and initialize_engine_audio().
// Destruction may happen on any thread: it tears the render session down, then
// releases the EngineAudio. Destroy it before the EngineAudio.
struct PreparedAudioRender {
    PreparedAudioRender() noexcept;
    PreparedAudioRender(PreparedAudioRender &&) noexcept;
    PreparedAudioRender &operator=(PreparedAudioRender &&) noexcept;
    ~PreparedAudioRender();

    // False once run_prepared_audio() has taken the session (even when that run
    // failed or threw) and after a move.
    bool ready() const noexcept;
    // Active FluidSynth tracks instantiated in this session (excludes muted,
    // metronome, chord-symbol, excerpt-excluded, and note-free tracks).
    std::size_t synth_track_count() const noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Owner thread. Everything that reads the score or loads SoundFonts: excerpt
// selection, the flat playback model, the duration and 1 GiB projection, the
// mix, every track's synth, SoundFont, preset and sample loads, and the aux
// returns. request.cancelled is polled here only; request.output_path and
// request.encoder_factory are ignored. A failure carries the AudioRenderResult
// export_score_audio() would return; RESOURCE_LIMIT while another render is
// active. Callback exceptions are rethrown after the partial graph is released.
std::expected<PreparedAudioRender, AudioRenderResult> prepare_score_audio(EngineAudio &audio, mu::engraving::MasterScore *master,
                                                                          const AudioRenderRequest &request);

// Any one thread at a time. Synthesizes, mixes and encodes directly into
// output_path, polling `cancelled` only on the calling thread (worker and
// encoder threads see only stop flags), and tears the render session down
// before returning, so a render runs once. Error precedence and output removal
// follow export_score_audio(); a callback exception is rethrown after cleanup.
// encoder_factory is the export_score_audio() test seam.
AudioRenderResult run_prepared_audio(PreparedAudioRender &prepared, const std::string &output_path, muse::UniqueFn<bool() const> cancelled,
                                     muse::FnRef<EncoderFactory> encoder_factory = {}, bool remove_on_encoder_failure = true);

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
