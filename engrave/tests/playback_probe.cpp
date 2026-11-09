// SPDX-License-Identifier: GPL-3.0-only
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/engine/internal/offlinecancellation.h"
#include "audio/engine/internal/offlinetiming.h"
#include "audio/engine/internal/playbackcapture.h"
#include "engrave/src/api/state.h"
#include "engrave/src/audio/engine_audio.h"
#include "engrave/src/audio/flat_playback_capture.h"
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/score/score_excerpts.h"
#include "playback_dump.h"
#include "playback_projection_flat.h"
#include "playback_projection_legacy.h"
#include <engrave/engine.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
#ifdef ENGRAVE_PLAYBACK_COUNTING
void install_allocation_hooks();
#endif

namespace {
using Clock = std::chrono::steady_clock;

std::string utf8(const std::filesystem::path &p) {
    auto s = p.generic_u8string();
    return {s.begin(), s.end()};
}

double milliseconds(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 5)
            throw std::invalid_argument("playback_probe ASSETS FONT SCORE OUTDIR [--rate N] [--no-repeats] [--no-reverb] [--mute-part N] "
                                        "[--excerpt N] [--measure] [--corrupt-flat] [--cancel-phase 1..5] [--cancel-poll N]");
        using namespace muse::audio;
        const auto dir = std::filesystem::u8path(argv[4]);
        std::filesystem::create_directories(dir);
        music::AudioRenderRequest request;
        request.output_path = utf8(dir / "audio.wav");
        int rate = 44100, cancel_phase = 0, cancel_poll = 1, mute_part = -1;
        bool measure = false, corrupt_flat = false;
        for (int i = 5; i < argc; ++i) {
            std::string arg = argv[i];
            const auto number = [&] {
                if (++i >= argc) throw std::invalid_argument("missing option value");
                return std::stoi(argv[i]);
            };
            if (arg == "--rate")
                rate = number();
            else if (arg == "--no-repeats")
                request.use_repeats = false;
            else if (arg == "--no-reverb")
                request.reverb = false;
            else if (arg == "--measure")
                measure = true;

            else if (arg == "--corrupt-flat")
                corrupt_flat = true;
            else if (arg == "--cancel-phase")
                cancel_phase = number();
            else if (arg == "--cancel-poll")
                cancel_poll = number();
            else if (arg == "--mute-part")
                mute_part = number();
            else if (arg == "--excerpt")
                request.excerpt_id = number();
            else
                throw std::invalid_argument("unknown option: " + arg);
        }
        if ((rate != 44100 && rate != 48000) || cancel_phase < 0 || cancel_phase > 5 || cancel_poll < 1)
            throw std::invalid_argument("invalid rate/cancellation option");
        engrave::EngineOptions engine_options;
        engine_options.resources = std::filesystem::u8path(argv[1]);
        engine_options.audio_sample_rate = rate;
        engine_options.deterministic_ids = true;
        auto engine = engrave::Engine::create(engine_options);
        if (!engine) throw std::runtime_error(engine.error().message);
        auto document = (*engine)->open(std::filesystem::u8path(argv[3]));
        if (!document) throw std::runtime_error(document.error().message);
        auto &context = engrave::DocumentAccess::context(**document);
        auto *master = engrave::DocumentAccess::master_score(**document);
        std::string error;
        if (!music::initialize_engine_audio(context.audio(), music::EngineAudioSetup{{argv[2]}, rate}, &error))
            throw std::runtime_error(error);
        if (request.excerpt_id >= 0) music::generate_score_excerpts(master);
        auto tracks = music::engine_audio_tracks(master, request.excerpt_id);
        request.tracks.resize(tracks.size());
        for (std::size_t i = 0; i < tracks.size(); ++i)
            if (static_cast<int>(tracks[i].part_index) == mute_part) request.tracks[i].muted = true;
        std::ofstream a, c;
        std::string projection_error;
        bool projection_written = false;
        if (!measure) {
            a.open(dir / "A.txt", std::ios::binary);
            c.open(dir / "C.txt", std::ios::binary);
            a << "legacy-mpe-v1\n";
            c << "synth-input-v2\n";
            // Dump A from the new path: each NoteEvent as the builder freezes it, then the flat track
            music::FlatPlaybackCapture::dump_note = [](const muse::mpe::NoteEvent &event) {
                std::ostringstream text;
                music::test::LegacyDump{text}.note(event);
                return text.str();
            };
            test::PlaybackCapture::track = [&](auto id, const auto &instrument, const auto &events) {
                music::test::FlatDump{a}.track(id, instrument, events);
            };
            test::PlaybackCapture::synth = &c;
            // Dump B: the export's flat records against an independent extractor over the legacy oracle model, in this binary
            music::FlatPlaybackCapture::projection = [&](const mu::engraving::PlaybackScore &flat,
                                                         const music::FlatPlaybackCapture::LegacyTracks &legacy) {
                mu::engraving::PlaybackScore copy;
                const mu::engraving::PlaybackScore *projected = &flat;
                if (corrupt_flat) {
                    // Negative control: one flat field changes, so the projections must differ
                    copy = flat;
                    for (auto &[id, track] : copy.tracks) {
                        if (!track.notes.empty()) {
                            track.notes[0].actual_duration += 1;
                            projected = &copy;
                            break;
                        }
                    }
                    if (projected != &copy) projection_error = "corrupt-flat: no note record to corrupt";
                }
                music::test::FlatProjection flat_projection;
                flat_projection.score(*projected);
                std::vector<music::test::LegacyProjectionTrack> tracks;
                for (const auto &[id, data] : legacy) tracks.push_back({id.part_id.to_uint64(), id.instrument_id, data});
                music::test::LegacyProjection legacy_projection;
                legacy_projection.score(tracks);
                const std::string flat_text = flat_projection.str();
                const auto write = [&](const char *name, const std::string &text) {
                    std::ofstream stream(dir / name, std::ios::binary);
                    stream << text;
                    stream.flush();
                    if (!stream) projection_error = std::string("dump B write failed: ") + name;
                };
                write("B.txt", flat_text);
                write("B_legacy.txt", legacy_projection.out);
                projection_written = true;
                if (flat_text != legacy_projection.out && projection_error.empty()) {
                    const auto at =
                        std::mismatch(flat_text.begin(), flat_text.end(), legacy_projection.out.begin(), legacy_projection.out.end())
                            .first -
                        flat_text.begin();
                    projection_error = "dump B mismatch: flat records differ from the legacy extractor at byte " + std::to_string(at);
                }
            };
        }

        Clock::time_point requested{}, observed{};
        Clock::time_point release_poll{};
        std::vector<double> release_batches;
        release_batches.reserve(1024);
        int polls = 0;
        request.cancelled = [&] {
            if (measure && OfflineCancellation::phase == OfflineCancellation::Phase::EVENT_RELEASE) {
                const auto now = Clock::now();
                if (release_poll != Clock::time_point{}) release_batches.push_back(milliseconds(release_poll, now));
                release_poll = now;
            } else {
                release_poll = {};
            }
            if (requested != Clock::time_point{}) {
                if (observed == Clock::time_point{}) observed = Clock::now();
                return true;
            }
            if (static_cast<int>(OfflineCancellation::phase) == cancel_phase && cancel_phase && ++polls == cancel_poll) {
                requested = Clock::now();
                // The following poll observes the request, including engine work between polls.
            }
            return false;
        };
#ifdef ENGRAVE_PLAYBACK_COUNTING
        install_allocation_hooks();
        // Allocation data cover only export, including teardown, excluding import/bootstrap/capture.
        if (!measure) throw std::invalid_argument("counting probe requires --measure");
        test::AllocationMetrics::enabled = true;
#endif
        const auto start = Clock::now();
        const auto result = music::export_score_audio(context.audio(), master, request);
        const auto returned = Clock::now();
        test::AllocationMetrics::enabled = false;
        test::PlaybackCapture::track = {};
        test::PlaybackCapture::synth = nullptr;
        music::FlatPlaybackCapture::projection = {};
        music::FlatPlaybackCapture::dump_note = {};
        if (!measure) {
            a.flush();
            c.flush();
            if (!a || !c) throw std::runtime_error("dump write failed");
            if (!projection_error.empty()) throw std::runtime_error(projection_error);
            if (!projection_written) throw std::runtime_error("dump B was not captured");
        }
        std::ofstream stats(dir / "metrics.txt", std::ios::binary);
        stats.precision(12);
        stats << "export_ms " << milliseconds(start, returned) << '\n';
        if (!release_batches.empty()) {
            std::sort(release_batches.begin(), release_batches.end());
            stats << "release_batch_count " << release_batches.size() << "\nrelease_batch_p95_ms "
                  << release_batches[(release_batches.size() * 95 - 1) / 100] << "\nrelease_batch_max_ms " << release_batches.back()
                  << '\n';
        }
        if (cancel_phase) {
            if (result.failure != music::AudioRenderResult::Failure::CANCELLED || observed == Clock::time_point{} ||
                std::filesystem::exists(dir / "audio.wav"))
                throw std::runtime_error("cancellation did not complete at requested phase");
            stats << "request_to_observed_ms " << milliseconds(requested, observed) << "\nobserved_to_return_ms "
                  << milliseconds(observed, returned) << "\nrequest_to_return_ms " << milliseconds(requested, returned) << '\n';
        } else if (!result.ok)
            throw std::runtime_error(result.error);
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS counters{};
        if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
            throw std::runtime_error("GetProcessMemoryInfo failed");
        stats << "peak_working_set_bytes " << counters.PeakWorkingSetSize << '\n';
#endif
        const auto &t = OfflineTiming::current();
        const double values[] = {0, t.synth, t.render, t.encode, t.setup, t.teardown, t.create, t.fonts, t.flush, t.model, t.tracks, t.aux};
        for (unsigned i = 0; i < test::AllocationMetrics::phase_count; ++i) {
            stats << "seconds " << test::AllocationMetrics::names[i] << ' ' << values[i] << '\n';
            stats << "allocations " << test::AllocationMetrics::names[i];
            for (auto &n : test::AllocationMetrics::counts[i]) stats << ' ' << n.load();
            stats << '\n';
        }
        stats << "allocations total";
        for (unsigned op = 0; op < test::AllocationMetrics::OP_COUNT; ++op) {
            std::uint64_t total = 0;
            for (auto &phase : test::AllocationMetrics::counts) total += phase[op].load();
            stats << ' ' << total;
        }
        stats << '\n';
        std::ofstream meta(dir / "build.txt", std::ios::binary);
        meta << "cxx " << __cplusplus << "\nmode " << ENGRAVE_PLAYBACK_BUILD_MODE << '\n';
#ifdef NDEBUG
        meta << "NDEBUG 1\n";
#else
        meta << "NDEBUG 0\n";
#endif
#ifdef _WIN32
        meta << "runtime MD\nexceptions cxx\ncompiler_switch -std:c++23preview\n";
#endif
#ifdef _MSC_FULL_VER
        meta << "msvc " << _MSC_FULL_VER << '\n';
#endif
        meta << "rate " << rate << "\nframes " << result.frames << "\nchannels 2\nsample_format FLOAT32\n";
        if (!stats || !meta) throw std::runtime_error("report write failed");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
