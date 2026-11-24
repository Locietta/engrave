// The audio API: audio_tracks(), prepare_audio() with PreparedAudio::run(),
// and render_audio(). The tracks and the default mix come from the score code
// in engrave/src/audio/ (score_audio), the mix and the render from the engine
// driver there (engine_audio).
//
// The driver encodes to a file. PreparedAudio::run writes the caller's file;
// render_audio hands the encoded stream to a sink. The WAV and FLAC encoders
// patch their headers when the stream ends (the WAV chunk sizes, FLAC's
// STREAMINFO), so a truly streaming destination cannot work: render_audio runs
// into one temporary file in the engine's cache directory, which is then read
// back in chunks and removed.

#include <atomic>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <engrave/document.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/audio/engine_audio.h"
#include "engrave/src/audio/score_audio.h"
#include "engrave/src/runtime/score_io.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/score.h"
#include "engraving/engravingproject.h"

namespace engrave {
namespace {

using mu::engraving::MasterScore;

constexpr std::size_t audio_chunk_bytes = 1 << 20;

std::unexpected<Error> fail(ErrorCode code, std::string message) { return std::unexpected(Error{code, std::move(message)}); }

std::optional<music::EngineAudioFormat> engine_format(AudioFormat format) {
    switch (format) {
        case AudioFormat::WAV: return music::EngineAudioFormat::WAV;
        case AudioFormat::FLAC: return music::EngineAudioFormat::FLAC;
        case AudioFormat::OPUS: return music::EngineAudioFormat::OGG;
        case AudioFormat::MP3: return music::EngineAudioFormat::MP3;
    }
    return std::nullopt;
}

const char *file_extension(AudioFormat format) {
    switch (format) {
        case AudioFormat::WAV: return ".wav";
        case AudioFormat::FLAC: return ".flac";
        case AudioFormat::OPUS: return ".ogg";
        case AudioFormat::MP3: return ".mp3";
    }
    return ".audio";
}

bool is_lossy(AudioFormat format) { return format == AudioFormat::OPUS || format == AudioFormat::MP3; }

AudioMix to_mix(const music::AudioOutputParams &output) { return {output.volume_db, output.balance, output.muted, output.solo}; }

// A mix value the engine accepts; the ranges are the ones the host enforced.
std::optional<Error> mix_error(std::optional<double> volume_db, std::optional<double> balance, std::string_view label) {
    if (volume_db && (!std::isfinite(*volume_db) || *volume_db < -120.0 || *volume_db > 24.0))
        return Error{ErrorCode::INVALID_ARGUMENT, std::format("{} volume_db must be finite and in [-120, 24]", label)};
    if (balance && (!std::isfinite(*balance) || *balance < -1.0 || *balance > 1.0))
        return Error{ErrorCode::INVALID_ARGUMENT, std::format("{} balance must be finite and in [-1, 1]", label)};
    return std::nullopt;
}

// The index of the SoundFont a track plays: the default (the first) unless the
// track names one by index or by name.
Result<int> soundfont_index(const SoundFontSelection &selection, const std::vector<std::filesystem::path> &fonts) {
    if (std::holds_alternative<std::monostate>(selection)) return 0;
    if (const auto *index = std::get_if<std::int32_t>(&selection)) {
        if (*index < 0 || static_cast<std::size_t>(*index) >= fonts.size())
            return fail(ErrorCode::INVALID_ARGUMENT, std::format("no SoundFont {} of {}", *index, fonts.size()));
        return *index;
    }
    const std::string &name = std::get<std::string>(selection);
    for (std::size_t index = 0; index < fonts.size(); ++index) {
        if (utf8(fonts[index].stem()) == name || utf8(fonts[index]) == name) return static_cast<int>(index);
    }
    std::string available;
    for (const auto &font : fonts) available += (available.empty() ? "" : ", ") + utf8(font.stem());
    return fail(ErrorCode::NOT_FOUND, std::format("no SoundFont named '{}' (available: {})", name, available));
}

struct TempFile {
    std::filesystem::path path;
    bool armed = true;

    ~TempFile() {
        if (!armed || path.empty()) return;
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

// The tracks of the selected score and its default mix, from the audiosettings
// the document was opened with (Document::Impl::audio_defaults).
struct MixContext {
    std::vector<music::ScoreAudioTrackInfo> tracks;
    music::AudioMix defaults;
};

Result<MixContext> resolve_mix(const music::ScoreAudioMixDefaults &audio_defaults, int excerpt_id, mu::engraving::Score &selected,
                               const std::string &excerpt_name) {
    MixContext context;
    try {
        context.tracks = music::score_audio_track_info(&selected);
    } catch (const std::invalid_argument &error) {
        return fail(ErrorCode::INVALID_ARGUMENT, error.what());
    }
    context.defaults = music::score_audio_default_mix(audio_defaults, context.tracks, excerpt_id, excerpt_name);
    return context;
}

std::string excerpt_name_of(const music::ScoreProject &project, int excerpt_id) {
    if (excerpt_id < 0) return {};
    const auto &excerpts = project->master_score()->excerpts();
    if (static_cast<std::size_t>(excerpt_id) < excerpts.size() && excerpts[excerpt_id]) return excerpts[excerpt_id]->name().to_std_string();
    return {};
}

} // namespace

Result<AudioTracks> Document::audio_tracks(const ScoreSel &selection) {
    impl_->engine->check_thread();
    MasterScore &master = *impl_->project->master_score();
    const Result<int> excerpt_id = excerpt_id_of(master, selection);
    if (!excerpt_id) return std::unexpected(excerpt_id.error());
    const Result<mu::engraving::Score *> selected = select_score(master, selection);
    if (!selected) return std::unexpected(selected.error());
    const Result<MixContext> context =
        resolve_mix(impl_->audio_defaults, *excerpt_id, **selected, excerpt_name_of(impl_->project, *excerpt_id));
    if (!context) return std::unexpected(context.error());

    AudioTracks result;
    result.master = to_mix(context->defaults.master);
    result.tracks.reserve(context->tracks.size());
    for (std::size_t index = 0; index < context->tracks.size(); ++index) {
        const music::ScoreAudioTrackInfo &track = context->tracks[index];
        AudioTrackInfo info;
        info.track = static_cast<std::int32_t>(index);
        info.part_id = track.part_id;
        info.instrument_id = track.instrument_id;
        info.part_name = track.part_name;
        info.instrument_name = track.instrument_name;
        info.chord_symbols = track.chord_symbols;
        info.mix = to_mix(context->defaults.tracks[index].output);
        result.tracks.push_back(std::move(info));
    }
    return result;
}

namespace {

// The public error for a failed prepare or run; the engine's own failures throw.
std::unexpected<Error> render_failure(const music::AudioRenderResult &rendered) {
    switch (rendered.failure) {
        case music::AudioRenderResult::Failure::CANCELLED: return fail(ErrorCode::CANCELLED, rendered.error);
        case music::AudioRenderResult::Failure::RESOURCE_LIMIT: return fail(ErrorCode::RESOURCE_LIMIT, rendered.error);
        case music::AudioRenderResult::Failure::INVALID_SCORE: return fail(ErrorCode::INVALID_ARGUMENT, rendered.error);
        case music::AudioRenderResult::Failure::IO: return fail(ErrorCode::IO, rendered.error);
        case music::AudioRenderResult::Failure::ENGINE_FAILURE:
        case music::AudioRenderResult::Failure::NONE: break;
    }
    throw std::runtime_error(rendered.error);
}

} // namespace

struct PreparedAudio::Impl {
    Impl(std::atomic<bool> &job_active, music::PreparedAudioRender render, std::uint64_t revision) noexcept
        : job_active(job_active), render(std::move(render)), revision(revision) {
        // Published only after operator new succeeds so allocation failure never poisons the lane.
        job_active.store(true, std::memory_order_release);
    }

    // The render graph is gone before the Engine may start another render or be released.
    ~Impl() {
        render = {};
        job_active.store(false, std::memory_order_release);
    }

    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;

    std::atomic<bool> &job_active;
    music::PreparedAudioRender render;
    std::uint64_t revision = 0;
};

PreparedAudio::PreparedAudio(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

PreparedAudio::PreparedAudio(PreparedAudio &&) noexcept = default;
PreparedAudio &PreparedAudio::operator=(PreparedAudio &&) noexcept = default;
PreparedAudio::~PreparedAudio() = default;

std::uint64_t PreparedAudio::source_revision() const noexcept { return impl_ ? impl_->revision : 0; }

Result<AudioRenderResult> PreparedAudio::run(const std::filesystem::path &output_path, std::function<bool()> cancelled) {
    if (!impl_ || !impl_->render.ready()) return fail(ErrorCode::INVALID_ARGUMENT, "the prepared audio has already run or was moved from");
    if (output_path.empty()) return fail(ErrorCode::INVALID_ARGUMENT, "the audio output path is empty");
    const std::string path = utf8(output_path);
    muse::UniqueFn<bool() const> poll;
    if (cancelled) poll = std::move(cancelled);
    const music::AudioRenderResult rendered = music::run_prepared_audio(impl_->render, path, std::move(poll));
    if (rendered.ok) return AudioRenderResult{rendered.bytes, rendered.frames, rendered.duration_secs};
    return render_failure(rendered);
}

Result<PreparedAudio> Document::prepare_audio(const AudioOptions &options) {
    impl_->engine->check_thread();
    const auto format = engine_format(options.format);
    if (!format) return fail(ErrorCode::INVALID_ARGUMENT, "unrecognized audio format");
    if (options.cancelled && options.cancelled()) return fail(ErrorCode::CANCELLED, "audio render cancelled");
    EngineState &engine = *impl_->engine;
    if (engine.audio_job_active.load(std::memory_order_acquire))
        return fail(ErrorCode::RESOURCE_LIMIT, "another audio render is already active");
    MasterScore &master = *impl_->project->master_score();
    const Result<int> excerpt_id = excerpt_id_of(master, options.score);
    if (!excerpt_id) return std::unexpected(excerpt_id.error());
    const Result<mu::engraving::Score *> selected = select_score(master, options.score);
    if (!selected) return std::unexpected(selected.error());

    if (engine.soundfonts.empty()) return fail(ErrorCode::UNSUPPORTED, "no SoundFont is selected (an empty list disables audio)");
    if (options.sample_rate != 0 && options.sample_rate != engine.audio_sample_rate) {
        return fail(ErrorCode::UNSUPPORTED,
                    std::format("the Engine renders at {} Hz, not {}", engine.audio_sample_rate, options.sample_rate));
    }
    if (options.bit_depth != 0) {
        if (is_lossy(options.format)) return fail(ErrorCode::INVALID_ARGUMENT, "bit_depth applies to WAV and FLAC, not to a lossy format");
        const bool allowed = options.format == AudioFormat::WAV ?
                                 options.bit_depth == 16 || options.bit_depth == 24 || options.bit_depth == 32 :
                                 options.bit_depth == 16 || options.bit_depth == 24;
        if (!allowed)
            return fail(ErrorCode::INVALID_ARGUMENT,
                        std::format("{} does not take a bit depth of {}", to_string(options.format), options.bit_depth));
    }
    if (options.bitrate_kbps != 0) {
        if (!is_lossy(options.format))
            return fail(ErrorCode::INVALID_ARGUMENT, "bitrate_kbps applies to Opus and MP3, not to a lossless format");
        if (options.bitrate_kbps < 32 || options.bitrate_kbps > 512)
            return fail(ErrorCode::INVALID_ARGUMENT, std::format("bitrate_kbps must be between 32 and 512, not {}", options.bitrate_kbps));
    }
    if (!std::isfinite(options.reverb_send) || options.reverb_send < 0.0 || options.reverb_send > 1.0)
        return fail(ErrorCode::INVALID_ARGUMENT, "reverb_send must be in [0, 1]");
    if (const auto error = mix_error(options.master_volume_db, options.master_balance, "master")) return std::unexpected(*error);

    const Result<MixContext> context =
        resolve_mix(impl_->audio_defaults, *excerpt_id, **selected, excerpt_name_of(impl_->project, *excerpt_id));
    if (!context) return std::unexpected(context.error());

    std::vector<const AudioTrackOptions *> by_track(context->tracks.size(), nullptr);
    for (const AudioTrackOptions &track : options.tracks) {
        if (track.track < 0 || static_cast<std::size_t>(track.track) >= context->tracks.size())
            return fail(ErrorCode::INVALID_ARGUMENT, std::format("no track {} of {}", track.track, context->tracks.size()));
        if (by_track[static_cast<std::size_t>(track.track)])
            return fail(ErrorCode::INVALID_ARGUMENT, std::format("track {} is named twice", track.track));
        if (!track.preset.empty()) return fail(ErrorCode::UNSUPPORTED, "choosing a preset is not supported");
        if (const auto error = mix_error(track.volume_db, track.balance, std::format("track {}", track.track))) {
            return std::unexpected(*error);
        }
        by_track[static_cast<std::size_t>(track.track)] = &track;
    }

    music::AudioRenderRequest request;
    request.format = *format;
    request.excerpt_id = *excerpt_id;
    request.use_repeats = options.use_repeats;
    request.master_volume_db = options.master_volume_db.value_or(context->defaults.master.volume_db);
    request.master_balance = options.master_balance.value_or(context->defaults.master.balance);
    request.bit_rate = options.bitrate_kbps;
    request.bit_depth = options.bit_depth;
    request.max_bytes = options.max_bytes;
    request.max_write_bytes = options.max_write_bytes;
    // Reference options.cancelled instead of copying it so stateful mutable
    // predicates preserve state across setup and run in synchronous render_audio.
    if (options.cancelled) request.cancelled = [&options] { return options.cancelled(); };
    request.reverb = options.reverb;
    request.reverb_send = options.reverb_send;
    request.tracks.resize(context->tracks.size());
    for (std::size_t index = 0; index < context->tracks.size(); ++index) {
        const music::AudioOutputParams &defaults = context->defaults.tracks[index].output;
        music::EngineAudioTrackParams &track = request.tracks[index];
        track.volume_db = defaults.volume_db;
        track.balance = defaults.balance;
        track.muted = defaults.muted;
        track.solo = defaults.solo;
        if (const AudioTrackOptions *override = by_track[index]) {
            track.volume_db = override->volume_db.value_or(track.volume_db);
            track.balance = override->balance.value_or(track.balance);
            track.muted = override->mute.value_or(track.muted);
            track.solo = override->solo.value_or(track.solo);
            const Result<int> font = soundfont_index(override->soundfont, engine.soundfonts);
            if (!font) return std::unexpected(font.error());
            track.soundfont = *font;
        }
    }

    // The audio engine is process-global and starts on first use, never at
    // Engine::create: a caller that only reads or draws pages pays nothing.
    music::EngineAudioSetup setup;
    setup.sample_rate = engine.audio_sample_rate;
    setup.audio_threads = engine.audio_threads;
    for (const auto &soundfont : engine.soundfonts) setup.soundfonts.push_back(utf8(soundfont));
    std::string error;
    // This is also a cheap no-op for an unchanged selection; a replacement is
    // refreshed here rather than by set_soundfonts().
    if (!music::initialize_engine_audio(engine.context->audio(), setup, &error)) return fail(ErrorCode::INVALID_INPUT, error);

    auto prepared = music::prepare_score_audio(engine.context->audio(), &master, request);
    if (!prepared) return render_failure(prepared.error());
    return PreparedAudio(std::make_unique<PreparedAudio::Impl>(engine.audio_job_active, *std::move(prepared), impl_->revision));
}

Result<AudioRenderResult> Document::render_audio(const AudioOptions &options, const ByteSink &sink) {
    EngineState &engine = *impl_->engine;
    std::optional<TempFile> file;
    Result<AudioRenderResult> rendered;
    {
        Result<PreparedAudio> prepared = prepare_audio(options);
        if (!prepared) return std::unexpected(prepared.error());

        std::error_code ec;
        std::filesystem::create_directories(engine.cache_dir, ec);
        if (!std::filesystem::is_directory(engine.cache_dir, ec))
            return fail(ErrorCode::IO, "cannot use the cache directory '" + utf8(engine.cache_dir) + "' for the render");
#ifdef _WIN32
        const int process = _getpid();
#else
        const int process = getpid();
#endif
        file.emplace(engine.cache_dir /
                     std::format("engrave-audio-{}-{}{}", process, ++engine.audio_render_index, file_extension(options.format)));

        std::function<bool()> run_cancelled;
        if (options.cancelled) run_cancelled = [&options] { return options.cancelled(); };
        // run_prepared_audio removes the file on any post-open failure/exception,
        // so arm TempFile cleanup only after run() succeeds to avoid double removal.
        file->armed = false;
        rendered = prepared->run(file->path, std::move(run_cancelled));
        if (rendered) file->armed = true;
    }
    // Release the prepared render graph and audio lane before calling the synchronous sink.
    if (!rendered) return std::unexpected(rendered.error());

    // Publication starts only after the whole encoded file has succeeded.
    if (options.cancelled && options.cancelled()) return fail(ErrorCode::CANCELLED, "audio render cancelled");
    std::ifstream in(file->path, std::ios::binary);
    if (!in) return fail(ErrorCode::IO, "cannot read the rendered audio '" + utf8(file->path) + "'");
    std::uint64_t bytes = 0;
    std::vector<std::byte> chunk(audio_chunk_bytes);
    while (in) {
        if (options.cancelled && options.cancelled()) return fail(ErrorCode::CANCELLED, "audio render cancelled");
        in.read(reinterpret_cast<char *>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        const auto count = in.gcount();
        if (count <= 0) break;
        sink(std::span<const std::byte>(chunk.data(), static_cast<std::size_t>(count)));
        bytes += static_cast<std::uint64_t>(count);
    }
    if (in.bad()) return fail(ErrorCode::IO, "cannot read the rendered audio '" + utf8(file->path) + "'");
    return AudioRenderResult{bytes, rendered->frames, rendered->seconds};
}

} // namespace engrave
