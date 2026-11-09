// SPDX-License-Identifier: GPL-3.0-only
//
// Offline audio driver. See engine_audio.h for the lifetime/threading contract.

#include "engine_audio.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>

#include <fluidsynth.h>

#include "audio/common/audioutils.h"
#include "audio/common/soundfonttypes.h"
#include "audio/engine/audioservices.h"
#include "audio/engine/iaudioengineconfiguration.h"
#include "audio/engine/internal/audiofactory.h"
#include "audio/engine/internal/export/soundtrackwriter.h"
#include "audio/engine/internal/nodes/automationcontrolnode.h"
#include "audio/engine/internal/nodes/signalnode.h"
#include "audio/engine/internal/nodes/trackchain.h"
#include "audio/engine/internal/offlinecancellation.h"
#include "audio/engine/internal/offlinerendersession.h"
#include "audio/engine/internal/offlinetiming.h"
#include "audio/engine/internal/playbackcapture.h"
#include "audio/engine/internal/synthesizers/fluidsynth/fluidsoundfontparser.h"
#include "audio/engine/internal/synthesizers/fluidsynth/fluidtrack.h"
#include "audio/engine/isoundfontrepository.h"
#include "audio_output.h"
#include "engrave/src/score/score_excerpts.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/playback/flatplaybackscore.h"
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include "engraving/playback/playbackmodel.h"
#endif
#include "engraving/playback/playbackwalk.h"
#include "flat_playback_capture.h"
#include "global/defer.h"
#include "global/io/ifilesystem.h"
#include "global/io/iodevice.h"
#include "global/io/ioretcodes.h"
#include "global/io/path.h"
#include "global/io/streamfile.h"
#include "global/types/ret.h"
#include "global/types/string.h"
#include "global/types/uri.h"
#include "log.h"
#include "score_audio.h"

using namespace muse;
using namespace muse::audio;
using namespace mu::engraving;

namespace music {
namespace {
constexpr sample_rate_t k_default_sample_rate = 44100;
constexpr samples_t k_samples_per_channel = 4096;
constexpr audioch_t k_audio_channel_count = 2;
constexpr double k_playback_tail_secs = 3.0;
constexpr const char *k_reverb_track_name = "Reverb";
constexpr const char *k_aux_track_name = "Aux 2";
constexpr const char *k_metronome_track_name = "Metronome";
constexpr const char *k_chords_track_name_prefix = "Chords.";

// Stored MIDI observer. FluidTrack sends every event it handles (after
// the staff -> channel remap), which is the event stream later golden tests
// compare. The hook keeps the sink reusable by the probe and by tests.
// renderedFrames is the number of output frames already encoded: the writer
// reports completed frames after each chunk, so this is the start of the
// chunk currently being rendered.
struct MidiCapture {
    void send_event(const midi::Event &event) {
        if (hook) hook(event, rendered_frames);
    }

    EngineAudioMidiEventHook hook;
    std::uint64_t rendered_frames = 0;
};

} // namespace

// Resources outlive services and every per-render graph.
struct EngineAudio::State {
    explicit State(const muse::mpe::IArticulationProfilesRepository &profiles) : profiles(profiles) {}

    const muse::mpe::IArticulationProfilesRepository &profiles;
    MidiCapture midi_out_port;
    engine::AudioResources resources;
    std::unique_ptr<engine::AudioServices> services;
    bool services_initialized = false;
    engine::AudioFactory *factory = nullptr;
    int sample_rate_value = ::music::k_default_sample_rate;
};

EngineAudio::EngineAudio(const muse::mpe::IArticulationProfilesRepository &profiles) : state_(std::make_unique<State>(profiles)) {}

EngineAudio::~EngineAudio() = default;

namespace {
OutputSpec output_spec(int input_sample_rate) {
    OutputSpec spec;
    spec.sample_rate = input_sample_rate;
    spec.samples_per_channel = ::music::k_samples_per_channel;
    spec.audio_channel_count = ::music::k_audio_channel_count;
    return spec;
}

// Mirrors playbackcontroller.cpp:63-73 (makeReverbOutputParams). 4.7.6 wrapped
// the chain in an AudioOutputParams because the engine took output params;
// 5.0's TrackParams carries the fx chain directly.
AudioFxChain make_reverb_fx_chain() {
    AudioFxParams reverb_params;
    reverb_params.resource_meta = make_reverb_meta();
    reverb_params.categories.insert(AudioFxCategory::FX_REVERB);
    reverb_params.chain_order = 0;
    reverb_params.active = true;

    AudioFxChain chain;
    chain.emplace(reverb_params.chain_order, std::move(reverb_params));
    return chain;
}

SoundTrackFormat make_soundtrack_format(EngineAudioFormat format, const AudioRenderRequest &request, const OutputSpec &spec) {
    SoundTrackFormat result;
    result.output_spec = spec;

    switch (format) {
        case EngineAudioFormat::WAV:
            result.type = SoundTrackType::WAV;
            result.sample_format = AudioSampleFormat::FLOAT32;
            result.bit_rate = 0;
            break;
        case EngineAudioFormat::FLAC:
            result.type = SoundTrackType::FLAC;
            result.sample_format = AudioSampleFormat::INT16;
            result.bit_rate = 0;
            break;
        case EngineAudioFormat::OGG:
            // Desktop OggWriter hard-codes 128 kbps (oggwriter.cpp:42).
            result.type = SoundTrackType::OGG;
            result.sample_format = AudioSampleFormat::UNDEFINED;
            result.bit_rate = 128;
            break;
        case EngineAudioFormat::MP3:
            // Desktop default exportMp3Bitrate is 128
            // (audioexportconfiguration.cpp:44).
            result.type = SoundTrackType::MP3;
            result.sample_format = AudioSampleFormat::UNDEFINED;
            result.bit_rate = 128;
            break;
        case EngineAudioFormat::AAC:
            // Desktop AacWriter reuses exportMp3Bitrate for the AAC bitrate.
            result.type = SoundTrackType::AAC;
            result.sample_format = AudioSampleFormat::UNDEFINED;
            result.bit_rate = 128;
            break;
    }

    // Caller overrides; the API validates them against the format.
    if (request.bit_rate > 0) result.bit_rate = request.bit_rate;
    if (request.bit_depth == 16)
        result.sample_format = AudioSampleFormat::INT16;
    else if (request.bit_depth == 24)
        result.sample_format = AudioSampleFormat::INT24;
    else if (request.bit_depth == 32)
        result.sample_format = AudioSampleFormat::FLOAT32;

    return result;
}

const char *audio_format_name(EngineAudioFormat format) {
    switch (format) {
        case EngineAudioFormat::WAV: return "WAV";
        case EngineAudioFormat::FLAC: return "FLAC";
        case EngineAudioFormat::OGG: return "Ogg";
        case EngineAudioFormat::MP3: return "MP3";
        case EngineAudioFormat::AAC: return "AAC";
    }
    return "audio";
}

AudioOutputCodec audio_output_codec_of(EngineAudioFormat format) {
    switch (format) {
        case EngineAudioFormat::WAV: return AudioOutputCodec::WAV;
        case EngineAudioFormat::FLAC: return AudioOutputCodec::FLAC;
        case EngineAudioFormat::OGG: return AudioOutputCodec::OGG;
        case EngineAudioFormat::MP3: return AudioOutputCodec::MP3;
        case EngineAudioFormat::AAC: return AudioOutputCodec::AAC;
    }
    return AudioOutputCodec::WAV;
}

// Mirrors the desktop mixer title rules in playbackcontroller.cpp:1155-1186
// (addTrack) and resolveAuxTrackTitle() (:77-89). Translation is not available
// in the native host, so the English labels are used verbatim.
std::string track_name(const MasterScore *master, const InstrumentTrackId &id) {
    if (id == playback_metronome_track_id()) {
        return ::music::k_metronome_track_name;
    }

    const Part *part = master->part_by_id(id.part_id);
    if (!part) {
        return id.instrument_id.to_std_string();
    }

    if (is_playback_chord_symbols_track(id)) {
        return std::string(::music::k_chords_track_name_prefix) + part->part_name().to_std_string();
    }

    const Instrument *primary = part->instrument();
    if (primary && primary->id() == id.instrument_id) {
        return part->part_name().to_std_string();
    }

    if (const Instrument *instrument = part->instrument_by_id(id.instrument_id)) {
        return "(" + instrument->track_name().to_std_string() + ")";
    }

    return id.instrument_id.to_std_string();
}

// 4.7.6 filled AudioOutputParams. 5.0 splits it: ControlParams carries only
// volume/balance/mute (audiotypes.h:435-447); solo is a desktop UI concept
// that PlaybackController folds into `muted` before it reaches the engine
// (playbackcontroller.cpp:1811-1824) and `forceMute` does not exist at all.
void set_track_control(ControlParams &control, const EngineAudioTrackParams &params) {
    control.volume = volume_db_t(static_cast<float>(params.volume_db));
    control.balance = balance_t(static_cast<float>(params.balance));
    control.muted = params.muted;
}

// The public master controls wrap the neutral session master chain. Keeping
// both chains preserves existing gain initialization and sample arithmetic.
engine::TrackChainPtr make_master_chain(const IAudioNodePtr &context, const OutputSpec &spec, const AudioRenderRequest &request,
                                        std::string &error) {
    IAudioNodePtr context_node = std::dynamic_pointer_cast<IAudioNode>(context);
    if (!context_node) {
        error = "audio context is not an audio node";
        return nullptr;
    }

    engine::AutomationControlNodePtr control = std::make_shared<engine::AutomationControlNode>();
    engine::TrackChainPtr chain = std::make_shared<engine::TrackChain>(::muse::audio::master_track_id, "Master");

    // Output spec and mode first: TrackChain::rebuild() pushes both into every
    // node it adds (nodes/chainnode.h:70-80), so the control node computes its
    // channel gains once, and only the explicit set* calls below can retarget
    // them.
    chain->set_output_spec(spec);
    chain->set_mode(ProcessMode::IDLE);

    chain->set_source(context_node);
    chain->set_fx_chain(nullptr); // 5.0 has no master fx chain (audiocontext.cpp:72)
    chain->set_control(control);
    chain->set_signal(std::make_shared<engine::SignalNode>());
    chain->rebuild();

    // Mirrors AudioContext::onControlParamsChanged() (audiocontext.cpp:310-331)
    // but latches the result instead of smoothing to it. 4.7.6 applied the
    // master params to every chunk without any smoothing (4.7.6
    // mixer.cpp:583-607); 5.0's AutomationControlNode re-targets its channel
    // gains over SmoothLinearValue's 1024 steps (dsp/smoothlinearvalue.h:37-46,
    // ~23 ms at 44.1 kHz), which would fade the master pan in from centre and
    // so leak the panned signal into the silenced channel.
    // updateChannelGains() initializes the gains directly when the automation
    // marker is unset (nodes/automationcontrolnode.cpp:74-96, initWithValue at
    // dsp/smoothlinearvalue.h:108-114) and only the first set* call after a
    // marker reset does so, so the pan is stored first and the volume written
    // once more to re-initialize both channel gains from (volume, pan).
    const volume_db_t master_volume(static_cast<float>(request.master_volume_db));
    control->set_pan(balance_t(static_cast<float>(request.master_balance)));
    control->set_muted(false);
    control->set_playhead_position(nullptr);
    control->set_volume(master_volume);

    return chain;
}

void fluid_log_to_logger(int level, const char *message, void *) {
    switch (level) {
        case FLUID_PANIC:
        case FLUID_ERR: LOGE() << message; break;
        case FLUID_WARN: LOGW() << message; break;
        default: LOGI() << message; break;
    }
}
} // namespace

void set_engine_audio_midi_event_hook(EngineAudio &audio, EngineAudioMidiEventHook hook) {
    audio.state().midi_out_port.hook = std::move(hook);
}

bool validate_engine_soundfonts(const std::vector<std::string> &soundfont_paths, std::string *error) {
    const auto fail = [error](std::string message) {
        if (error) {
            *error = std::move(message);
        }
        return false;
    };

    if (soundfont_paths.empty()) {
        return fail("no SoundFont was given (one .sf2 or .sf3 file is needed)");
    }
    for (const std::string &path : soundfont_paths) {
        if (path.empty() || !synth::is_sound_font(io::PathT(path))) {
            return fail("SoundFont must be an existing .sf2 or .sf3 file: " + path);
        }
    }

    // Uri::fromLocalFile() resolves the absolute path through the engine's file system.
    if (!io::file_system()) {
        return fail("native runtime (IFileSystem) is not initialised; create the EngineContext first");
    }

    // The pinned FluidSynth default sink writes parser errors to stdout, which
    // corrupts a JSON CLI's output. FluidTrack::init() installs its own
    // per-synth callbacks later, so this only covers parsing here.
    fluid_set_log_function(FLUID_PANIC, fluid_log_to_logger, nullptr);
    fluid_set_log_function(FLUID_ERR, fluid_log_to_logger, nullptr);
    fluid_set_log_function(FLUID_WARN, fluid_log_to_logger, nullptr);
    fluid_set_log_function(FLUID_INFO, fluid_log_to_logger, nullptr);

    // Parse every candidate before changing the live repository. The retained
    // files are immutable, so a later repository load reads the same bytes.
    FlatMap<std::string, std::string> names;
    for (const std::string &path : soundfont_paths) {
        const auto parsed = synth::FluidSoundFontParser::parse_sound_font(io::PathT(path));
        if (!parsed.ret) return fail("failed to load SoundFont '" + path + "'");
        const auto [it, inserted] = names.emplace(parsed.val.name, path);
        if (!inserted && it->second != path) return fail("SoundFonts have the same resource name: " + parsed.val.name);
    }
    return true;
}

bool initialize_engine_audio(EngineAudio &audio, const EngineAudioSetup &setup, std::string *error) {
    EngineAudio::State &state = audio.state();
    const auto fail = [error](std::string message) {
        if (error) *error = std::move(message);
        return false;
    };
    const std::vector<std::string> &soundfont_paths = setup.soundfonts;
    if (state.services_initialized) {
        if (!state.resources.soundfont_id.empty() && state.resources.soundfont_paths_value == soundfont_paths &&
            state.sample_rate_value == setup.sample_rate)
            return true;
        if (state.sample_rate_value != setup.sample_rate) return fail("audio sample rate is fixed");
    }
    if (setup.sample_rate < 8000 || setup.sample_rate > 192000)
        return fail("audio sample rate must be between 8000 and 192000 Hz: " + std::to_string(setup.sample_rate));
    if (!validate_engine_soundfonts(soundfont_paths, error)) return false;

    if (!state.services_initialized) {
        // Configuration and effects services live with the engine; each render
        // creates concrete tracks borrowing its AudioResources.
        state.services = std::make_unique<engine::AudioServices>();
        state.services_initialized = true;
        AudioEngineConfig config;
        config.use_sound_font_low_pass_filter = true;
        state.sample_rate_value = setup.sample_rate;
        engine::AudioEngineConfiguration &configuration = state.services->configuration();
        configuration.set_config(config);
        state.factory = &state.services->factory();
        state.factory->set_output_spec(output_spec(state.sample_rate_value));
    }
    auto *repository = &state.resources.repository;
    const auto uris = [](const std::vector<std::string> &paths) {
        std::vector<Uri> result;
        for (const auto &path : paths) result.push_back(Uri::from_local_file(io::PathT(path)));
        return result;
    };
    // load_sound_fonts replaces the repository and notifies AudioResources, which
    // clears and rebuilds its resource cache. If loading fails, restore the
    // previous repository before returning without publishing the new selection.
    repository->load_sound_fonts(uris(soundfont_paths));
    const auto restore = [&]() {
        if (!state.resources.soundfont_paths_value.empty()) repository->load_sound_fonts(uris(state.resources.soundfont_paths_value));
    };

    // 6. Explicit source resource for every track, taken from the engine's own
    // resource list (refresh_audio_resources). Without
    // it a track would resolve to the engine default ("MS Basic",
    // audioengineconfiguration.cpp:35-41, 103-109), which is absent from this
    // process, and the track would be silent.
    const AudioResourceMetaList resources = engine::audio_resource_list(state.resources);
    std::vector<AudioInputParams> inputs;
    std::string default_id, default_uri;
    for (std::size_t index = 0; index < soundfont_paths.size(); ++index) {
        const std::string &path = soundfont_paths[index];
        const Uri uri = Uri::from_local_file(io::PathT(path));
        const synth::SoundFontsMap &soundfonts = repository->sound_fonts();
        auto it = soundfonts.find(uri);
        if (it == soundfonts.end()) {
            restore();
            return fail("failed to load SoundFont '" + path + "'");
        }

        const AudioResourceMeta *found = nullptr;
        for (const AudioResourceMeta &meta : resources) {
            if (meta.id == it->second.name && is_resource_type(meta, AudioResourceType::FLUID_SOUNDFONT)) {
                found = &meta;
                break;
            }
        }
        if (!found) {
            restore();
            return fail("SoundFont '" + path + "' is not available as an audio input resource");
        }

        AudioInputParams input;
        input.resource_meta = *found;
        inputs.push_back(input);
        if (index == 0) {
            default_uri = uri.to_string();
            default_id = it->second.name;
        }
    }

    state.resources.font_inputs = std::move(inputs);
    state.resources.soundfont_paths_value = soundfont_paths;
    state.resources.soundfont_uri = std::move(default_uri);
    state.resources.soundfont_id = std::move(default_id);

    return true;
}

bool initialize_engine_audio(EngineAudio &audio, const std::string &soundfont_path, std::string *error) {
    return initialize_engine_audio(audio, EngineAudioSetup{{soundfont_path}, ::music::k_default_sample_rate}, error);
}

bool engine_audio_initialized(const EngineAudio &audio) {
    EngineAudio::State &state = audio.state();
    return state.services_initialized && !state.resources.soundfont_id.empty();
}

std::string engine_soundfont_resource_id(const EngineAudio &audio) { return audio.state().resources.soundfont_id; }

std::string engine_soundfont_uri(const EngineAudio &audio) { return audio.state().resources.soundfont_uri; }

AudioRenderResult export_score_audio(EngineAudio &audio, MasterScore *master, const AudioRenderRequest &request) {
    OfflineTimingReport timing_report;
    AudioRenderResult result;
    const auto fail = [&result](AudioRenderResult::Failure kind, std::string message) {
        result.ok = false;
        result.failure = kind;
        result.error = std::move(message);
        return result;
    };

    EngineAudio::State &state = audio.state();
    if (!state.services_initialized || !state.factory || state.resources.soundfont_id.empty()) {
        return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "audio engine is not initialised");
    }
    if (!master) {
        return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "score is null");
    }
    if (request.output_path.empty()) {
        return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "output path is empty");
    }

    bool cancelled = false;
    std::exception_ptr callback_error;
    const auto poll = [&] {
        // Encoder file writes may originate in C codec callbacks. Unwind only
        // after leaving the codec, closing the file and idling the graph.
        if (!cancelled && request.cancelled) {
            try {
                cancelled = request.cancelled();
            } catch (...) {
                callback_error = std::current_exception();
                cancelled = true;
            }
        }
        return cancelled;
    };
    const std::function<bool()> cancellation_poll = poll;
    OfflineCancellation cancellation_scope(cancellation_poll);
    const auto abort_setup = [&]() {
        if (callback_error) std::rethrow_exception(callback_error);
        return fail(AudioRenderResult::Failure::CANCELLED, "audio render cancelled during setup");
    };
    if (poll()) return abort_setup();

    auto setup_timer = std::make_unique<OfflineTimer>(OfflineTiming::current().setup);
    MasterScore *master_score = master;
    // Excerpt selection mutes every master part that is absent from, or
    // invisible in, the excerpt (masternotation.cpp:748-770).
    std::set<ID> muted_parts;
    bool excerpt_selected = false;
    if (request.excerpt_id >= 0) {
        const std::vector<Excerpt *> &excerpts = master_score->excerpts();
        if (static_cast<std::size_t>(request.excerpt_id) >= excerpts.size()) {
            return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "excerpt id is out of range");
        }

        const Excerpt *excerpt = excerpts[static_cast<std::size_t>(request.excerpt_id)];
        if (!excerpt) {
            return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "excerpt entry is null");
        }

        const Score *excerpt_score = excerpt->excerpt_score();
        if (!excerpt_score || excerpt_score->master_score() != master_score) {
            return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "excerpt has no score owned by this master score");
        }

        for (const Part *part : master_score->parts()) {
            if (!part) {
                continue;
            }
            const Part *excerpt_part = excerpt_score->part_by_id(part->id());
            if (!excerpt_part || !excerpt_part->is_visible()) {
                muted_parts.insert(part->id());
            }
        }
        excerpt_selected = true;
    }

    // The export's flat MPE store, one track per walk track in id order. Sources borrow it only while
    // loading their MIDI sequences; it is released after the track loop (design 4.4).
    PlaybackScore playback;
    DEFER { playback.release(); };

    // Only this private sentinel unwinds construction; callback exceptions
    // remain latched by poll() and are rethrown by abortSetup().
    struct CancelledModel {};

    const auto checkpoint = [&] {
        if (poll()) throw CancelledModel{};
    };
    {
        OfflineTimer timer(OfflineTiming::current().model);

        OfflineCancellation::ModelScope phase;
        try {
            PlaybackScoreBuilder builder{&state.profiles};
            builder.play_repeats = request.use_repeats;
            builder.metronome_enabled = false;
#ifdef ENGRAVE_PLAYBACK_HARNESS
            builder.dump_note = FlatPlaybackCapture::dump_note;
#endif
            playback = builder.build(master_score, checkpoint);
        } catch (const CancelledModel &) {
            return abort_setup();
        } catch (const engine::StoreLimit &limit) {
            // A flat buffer would leave the uint32 offset format
            if (callback_error) std::rethrow_exception(callback_error);
            return fail(AudioRenderResult::Failure::RESOURCE_LIMIT, limit.message);
        }
    }
    if (poll()) return abort_setup();

    if (playback.tracks.empty()) {
        return fail(AudioRenderResult::Failure::INVALID_SCORE, "score has no playable tracks");
    }

    // Repeat-expanded play time plus the upstream 3 s tail
    // (notationplayback.cpp:235-252). The tail contains reverb/note decay; the
    // writer truncates everything after it (soundtrackwriter.cpp:149-204).
    //
    // 4.7.6 derived the duration with secsToMilisecs(), which truncates the
    // double towards zero (4.7.6 audio/common/audiotypes.h:49). 5.0 replaced it
    // with muse::secs_to_msecs(), which rounds (global/types/secs.h:33), so the
    // conversion is spelled out here: the truncation is what fixes the render
    // length in whole milliseconds, and therefore the exact frame count.
    const int last_utick = master_score->repeat_list(request.use_repeats).ticks();
    const double play_time = master_score->utick2utime(last_utick);
    const msecs_t total_duration_ms = msecs_t(static_cast<int64_t>((play_time + ::music::k_playback_tail_secs) * 1000.0));
    // Compare the raw value: msecs_t is a number_t<int64_t> since 5.0 and its
    // operator<=(T) / operator<=(number_t<T>) overloads are equally viable for an
    // int literal (C2666). The meaning is unchanged - a non-positive millisecond
    // duration means the score has nothing to play.
    if (total_duration_ms.raw() <= 0) {
        return fail(AudioRenderResult::Failure::INVALID_SCORE, "score has no audio duration");
    }

    // 4.7.6 wrote (msecs * 1000 * rate) / 1000000 output frames. The writer
    // derives its frame count from a secs_t duration with llround()
    // (soundtrackwriter.cpp:66-72), so hand it the frame count converted back
    // to time: the render then has exactly the same length as before.
    const std::uint64_t total_frames = static_cast<std::uint64_t>(total_duration_ms) * 1000ULL * state.sample_rate_value / 1000000ULL;
    const secs_t render_secs = secs_t(static_cast<double>(total_frames) / static_cast<double>(state.sample_rate_value));

    // Refuse an oversized render before the encoder creates the file. The
    // parent applies the same projection before spawning a worker; this guard
    // also covers direct callers (engine_audio_probe, tests).
    const double duration_secs_value = static_cast<double>(total_duration_ms) / 1000.0;
    const std::uint64_t projected_bytes =
        projected_audio_bytes(audio_output_codec_of(request.format), duration_secs_value, state.sample_rate_value);
    if (projected_bytes > ::music::k_audio_output_limit) {
        return fail(AudioRenderResult::Failure::RESOURCE_LIMIT,
                    std::string("projected ") + audio_format_name(request.format) + " output of " +
                        std::to_string(static_cast<int>(duration_secs_value / 60.0)) + " min (~" +
                        std::to_string(projected_bytes / (1024 * 1024)) + " MiB) exceeds the 1 GiB audio export limit");
    }

    engine::OfflineRenderSession context(*state.factory, output_spec(state.sample_rate_value), state.resources,
                                         state.services->configuration(),
                                         [&state](const midi::Event &event) { state.midi_out_port.send_event(event); });
    std::string chain_error;
    engine::TrackChainPtr master_chain = make_master_chain(context.output(), output_spec(state.sample_rate_value), request, chain_error);
    if (!master_chain) {
        return fail(AudioRenderResult::Failure::ENGINE_FAILURE, std::move(chain_error));
    }

    DEFER { master_chain->clear(); };

    // The public mix is indexed by the same deterministic track list that
    // score_audio_tracks reports. Keep the list for the selected excerpt: for
    // a part export it contains only the excerpt's parts, while master tracks
    // outside it keep neutral values and are muted below.
    const std::vector<EngineAudioTrackInfo> mix_tracks = engine_audio_tracks(master_score, request.excerpt_id);
    std::map<InstrumentTrackId, std::size_t> mix_index;
    for (std::size_t index = 0; index < mix_tracks.size(); ++index) {
        mix_index.emplace(mix_tracks[index].id, index);
    }
    const auto track_params = [&request](std::size_t index) {
        return index < request.tracks.size() ? request.tracks[index] : EngineAudioTrackParams{};
    };

    // updateSoloMuteStates() derives forceMute from any non-metronome solo.
    bool has_solo = false;
    for (std::size_t index = 0; index < mix_tracks.size(); ++index) {
        if (mix_tracks[index].id == playback_metronome_track_id()) {
            continue;
        }
        if (track_params(index).solo) {
            has_solo = true;
            break;
        }
    }

    // Deterministic track order: upstream iterates the unordered track set, but
    // deterministic ids/mix order are needed for byte-identical exports. The
    // store is sorted by id.
#ifdef ENGRAVE_PLAYBACK_HARNESS
    for (const auto &[id, events] : playback.tracks) {
        if (test::PlaybackCapture::synth)
            *test::PlaybackCapture::synth << "track " << id.part_id.to_uint64() << ' ' << id.instrument_id.to_std_string() << '\n';
        if (test::PlaybackCapture::track) test::PlaybackCapture::track(id.part_id.to_uint64(), id.instrument_id, events);
    }
    if (test::PlaybackCapture::synth) *test::PlaybackCapture::synth << "total_frames " << total_frames << '\n';
    if (FlatPlaybackCapture::projection) {
        // The legacy model is the test oracle (design E1): a separate walk of the same score
        PlaybackModel oracle(&state.profiles);
        oracle.set_play_repeats(request.use_repeats);
        oracle.set_play_chord_symbols(false);
        oracle.set_is_metronome_enabled(false);
        oracle.load(master_score);
        const InstrumentTrackIdSet ids = oracle.existing_track_id_set();
        std::vector<InstrumentTrackId> ordered(ids.cbegin(), ids.cend());
        std::sort(ordered.begin(), ordered.end());
        FlatPlaybackCapture::LegacyTracks legacy;
        for (const auto &id : ordered) legacy.emplace_back(id, &oracle.resolve_track_playback_data(id));
        FlatPlaybackCapture::projection(playback, legacy);
    }
#endif
    auto tracks_timer = std::make_unique<OfflineTimer>(OfflineTiming::current().tracks);
    for (const auto &[track_id, events] : playback.tracks) {
        if (poll()) return abort_setup();
        if (!events.setup.is_valid()) {
            continue;
        }

        const bool is_metronome = track_id == playback_metronome_track_id();
        const bool is_chord_symbols = is_playback_chord_symbols_track(track_id);

        const auto index = mix_index.find(track_id);
        EngineAudioTrackParams mix;
        if (index != mix_index.end()) {
            mix = track_params(index->second);
        }

        // Fold forceMute as PlaybackController::updateSoloMuteStates does:
        // params.muted = mute || forceMute (the engine only honours
        // ControlParams::muted, nodes/automationcontrolnode.cpp:49-52).
        // Chord-symbol and metronome tracks are force-muted because chord
        // playback and the metronome are off.
        const bool force_mute = !is_metronome && has_solo && !mix.solo;

        TrackParams params;
        const int font = mix.soundfont < 0 ? 0 : mix.soundfont;
        if (static_cast<std::size_t>(font) >= state.resources.font_inputs.size()) {
            return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "track names an unknown SoundFont");
        }
        params.source = state.resources.font_inputs[static_cast<std::size_t>(font)];
        set_track_control(params.control, mix);
        params.control.muted =
            mix.muted || force_mute || is_chord_symbols || is_metronome || (excerpt_selected && muted_parts.count(track_id.part_id) > 0);
        if (!is_metronome) {
            // Desktop adds the two Fluid default sends only for non-metronome
            // tracks (playbackcontroller.cpp:1220-1227; defaultAuxSendValue is
            // 0.30 for both, playbackconfiguration.cpp:272-289). Mixer index 0
            // feeds the first aux track (muse/framework/audio/engine/internal/
            // mixer.cpp:283-309), which is the reverb added below. With the
            // reverb off the first send stays at zero, so the master is the dry
            // mix; the aux tracks are still added, so the graph keeps its shape.
            //
            // The second send must stay at zero: 5.0 always gives an aux track
            // an FxChain (AudioContext::onFxChainParamsChanged builds one even
            // for an empty AudioFxChain, audiocontext.cpp:355-369) and the
            // mixer only skips an aux whose chain *pointer* is null
            // (mixer.cpp:275-277, 291-293), so the empty "Aux 2" track is a
            // pass-through that would add 0.30 x the track signal - i.e. 1.3x
            // dry, +2.28 dB - to the master. 4.7.6 skipped an aux whose fx
            // chain *params* were empty (4.7.6 mixer.cpp:507-509, 527-529), so
            // that send was discarded there; keeping it zero reproduces the
            // 4.7.6 mix bit-exactly.
            const double send = request.reverb ? request.reverb_send : 0.0;
            params.aux_sends = {AuxSendParams{static_cast<gain_t>(send), true}, AuxSendParams{0.0f, true}};
        }

        const std::string name = track_name(master_score, track_id);
        RetVal2<TrackId, TrackParams> added;
        try {
            added = context.add_track(name, events, params);
        } catch (const engine::StoreLimit &limit) {
            if (callback_error) std::rethrow_exception(callback_error);
            return fail(AudioRenderResult::Failure::RESOURCE_LIMIT, limit.message);
        }
        if (poll()) return abort_setup();
        if (!added.ret) {
            return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "addTrack failed: " + added.ret.to_string());
        }
    }

    {
        // Every sequencer has lowered its track and keeps its own copy of the dynamics, so the flat store
        // is released before synthesis. Drain cold owners and the retained context in polled strides,
        // then free the flat buffers.
        OfflineCancellation::EventReleaseScope phase;
        if (poll()) return abort_setup();
        try {
            playback.release(checkpoint);
        } catch (const CancelledModel &) {
            return abort_setup();
        }
        if (poll()) return abort_setup();
    }

    // Aux 0 is the Muse Reverb send target; aux 1 is empty (desktop adds
    // AUX_CHANNEL_NUM aux tracks, playbackcontroller.cpp:1717-1719, in
    // addAuxTrack (:1304-1360)).
    tracks_timer.reset();
    auto aux_timer = std::make_unique<OfflineTimer>(OfflineTiming::current().aux);
    TrackParams reverb_params;
    reverb_params.fx_chain = make_reverb_fx_chain();
    RetVal2<TrackId, TrackParams> reverb = context.add_aux_track(::music::k_reverb_track_name, reverb_params);
    if (!reverb.ret) {
        return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "addAuxTrack(reverb) failed: " + reverb.ret.to_string());
    }
    RetVal2<TrackId, TrackParams> aux2 = context.add_aux_track(::music::k_aux_track_name, TrackParams());
    if (!aux2.ret) {
        return fail(AudioRenderResult::Failure::ENGINE_FAILURE, "addAuxTrack(aux 2) failed: " + aux2.ret.to_string());
    }

    aux_timer.reset();
    if (poll()) return abort_setup();
    context.reset_sources();
    if (poll()) return abort_setup();

    // libopusenc dithers with rand(). 4.7.6 also relied on this for the Ogg
    // stream serial; 5.0 pins the serial itself (oggencoder.cpp:88-91), so the
    // seed only keeps the encoder deterministic. Seed after all engine setup
    // (synth/sequence creation) so a given request renders identically across
    // processes.
    std::srand(0);

    // The writer reports completed frames after each successfully encoded block.
    state.midi_out_port.rendered_frames = 0;

    setup_timer.reset();
    io::StreamingFileDevice device(std::filesystem::u8path(request.output_path), request.max_bytes, request.max_write_bytes, poll);
    if (!device.open(io::IODevice::WRITE_ONLY)) {
        return fail(AudioRenderResult::Failure::IO,
                    std::string("cannot write ") + audio_format_name(request.format) + " output: '" + request.output_path + "'");
    }

    // The writer owns the render loop (soundtrackwriter.cpp:133-207): it sets
    // the source to PlayingOffline, renders in 4096-frame chunks, encodes them
    // and reports the completed frame count after each one.
    soundtrack::SoundTrackWriter writer(
        device, make_soundtrack_format(request.format, request, output_spec(state.sample_rate_value)), render_secs, master_chain,
        [&state](uint64_t frames) { state.midi_out_port.rendered_frames = frames; }, request.encoder_factory, poll);
    Ret write_ret;
    try {
        write_ret = writer.write();
    } catch (const engine::StoreLimit &limit) {
        device.close();
        master_chain->set_mode(ProcessMode::IDLE);
        std::error_code remove_error;
        std::filesystem::remove(std::filesystem::u8path(request.output_path), remove_error);
        if (callback_error) std::rethrow_exception(callback_error);
        return fail(AudioRenderResult::Failure::RESOURCE_LIMIT, limit.message);
    }

    // Flush before the FileSystem size check below.
    device.close();

    // Leave the graph idle, like AudioContext::doSaveSoundTrack()
    // (audiocontext.cpp:934-940).
    master_chain->set_mode(ProcessMode::IDLE);

    if (poll()) {
        std::error_code remove_error;
        std::filesystem::remove(std::filesystem::u8path(request.output_path), remove_error);
        if (remove_error) return fail(AudioRenderResult::Failure::IO, "cannot remove cancelled audio output: " + remove_error.message());
        if (callback_error) std::rethrow_exception(callback_error);
        return fail(AudioRenderResult::Failure::CANCELLED, "audio render cancelled");
    }

    if (device.limit_exceeded()) {
        std::error_code remove_error;
        std::filesystem::remove(std::filesystem::u8path(request.output_path), remove_error);
        if (remove_error) {
            return fail(AudioRenderResult::Failure::IO,
                        "cannot remove over-budget audio output: '" + request.output_path + "': " + remove_error.message());
        }
        return fail(AudioRenderResult::Failure::RESOURCE_LIMIT, "audio intermediate byte limit exceeded");
    }

    if (!write_ret || device.has_error()) {
        // SoundTrackWriter returns Err::ErrorEncode when an encoder could not
        // be created or stopped accepting data, e.g. the destination is not
        // writable, and the bare Ret has no context. Name the format and path
        // instead of reporting the raw code.
        const std::string detail = device.has_error() ? device.error_string() : write_ret.to_string();
        return fail(device.has_error() ? AudioRenderResult::Failure::IO : AudioRenderResult::Failure::ENGINE_FAILURE,
                    std::string(audio_format_name(request.format)) + " encoder failed (" + detail + "): '" + request.output_path + "'");
    }

    result.ok = true;
    result.frames = total_frames;
    result.play_time_secs = play_time;
    result.duration_secs = duration_secs_value;
    std::error_code size_error;
    const auto size = std::filesystem::file_size(std::filesystem::u8path(request.output_path), size_error);
    if (size_error) {
        return fail(AudioRenderResult::Failure::IO,
                    "cannot determine audio output size: '" + request.output_path + "': " + size_error.message());
    }
    result.bytes = static_cast<std::uint64_t>(size);
    if (result.bytes == 0) {
        result.ok = false;
        result.failure = AudioRenderResult::Failure::ENGINE_FAILURE;
        result.error = "audio export reported success but wrote no data";
    }

    return result;
}

std::vector<EngineAudioTrackInfo> engine_audio_tracks(MasterScore *master, int excerpt_id) {
    if (!master) {
        throw std::invalid_argument("engine_audio_tracks: master score is null");
    }

    mu::engraving::Score *selected = nullptr;
    try {
        selected = select_score(master, excerpt_id);
    } catch (const std::out_of_range &error) {
        throw std::invalid_argument(error.what());
    }

    std::vector<EngineAudioTrackInfo> result;
    for (const ScoreAudioTrackInfo &description : score_audio_track_info(selected)) {
        EngineAudioTrackInfo info;
        info.id = description.id;
        info.part_id = description.part_id;
        info.instrument_id = description.instrument_id;
        info.part_name = description.part_name;
        info.instrument_name = description.instrument_name;
        info.chord_symbols = description.chord_symbols;
        info.part_index = description.part_index;
        result.push_back(std::move(info));
    }

    return result;
}
} // namespace music
