// SPDX-License-Identifier: GPL-3.0-only
// Failure injection for the offline writer, with recovery through the public API.

#include <algorithm>
#include <atomic>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <xmmintrin.h>
#endif

#include "audio/thirdparty/fluidsynth/vorbis_decode.h"
#include <gtest/gtest.h>

#include "audio/common/audioerrors.h"
#include "audio/engine/internal/audioengineconfiguration.h"
#include "audio/engine/internal/export/audioencoder.h"
#include "audio/engine/internal/nodes/fluidtracksource.h"
#include "audio/engine/internal/offlinecancellation.h"
#include "audio/engine/internal/offlinerendersession.h"
#include "audio/engine/internal/offlinetiming.h"
#include "audio/engine/internal/synthesizers/fluidsynth/fluidsequencer.h"
#include "audio/engine/internal/workerpool.h"
#include "engrave/src/api/state.h"
#include "engrave/src/audio/engine_audio.h"
#include "engrave/src/runtime/resource_filesystem.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "global/btree.h"
#include "global/io/iodevice.h"
#include "mpe/articulationprofilebundle.h"
#include "output_fixture.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

// Sets the thread count the way render_audio passes EngineOptions::audio_threads.
void set_audio_threads(engrave::Document &document, int threads) {
    auto &engine = *engrave::DocumentAccess::impl(document).engine;
    engine.audio_threads = threads;
    music::EngineAudioSetup setup;
    setup.sample_rate = engine.audio_sample_rate;
    for (const auto &soundfont : engine.soundfonts) setup.soundfonts.push_back(engrave::utf8(soundfont));
    setup.audio_threads = threads;
    ASSERT_TRUE(music::initialize_engine_audio(engrave::DocumentAccess::context(document).audio(), setup));
}

struct AutomaticAudioThreads {
    engrave::Document &document;

    ~AutomaticAudioThreads() { set_audio_threads(document, 0); }
};
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
struct TestFpState {
    static constexpr std::uint32_t k_denormal_mask = (1U << 15) | (1U << 6);
    static constexpr std::uint32_t k_rounding_bit = 1U << 13;

    static std::uint32_t read() { return _mm_getcsr(); }

    static void write(std::uint32_t state) { _mm_setcsr(state); }
};
#elif defined(__aarch64__)
struct TestFpState {
    static constexpr std::uint64_t k_denormal_mask = 1ULL << 24;
    static constexpr std::uint64_t k_rounding_bit = 1ULL << 22;

    static std::uint64_t read() {
        std::uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        return fpcr;
    }

    static void write(std::uint64_t fpcr) { asm volatile("msr fpcr, %0" : : "r"(fpcr)); }
};
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
TEST(AudioInternal, RenderAudioRestoresCallerDenormalState) {
    using Fp = TestFpState;

    struct RestoreFpState {
        const decltype(Fp::read()) state = Fp::read();

        ~RestoreFpState() { Fp::write(state); }
    } restore;

    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    ASSERT_TRUE(document.render_audio({}, [](std::span<const std::byte>) {}));
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto output = std::filesystem::temp_directory_path() / ("engrave-denormal-" + std::to_string(process) + ".wav");
    for (const auto entry_mode : {decltype(Fp::read()){0}, Fp::k_denormal_mask}) {
        SCOPED_TRACE(entry_mode);
        music::AudioRenderRequest request;
        const auto utf8 = output.generic_u8string();
        request.output_path.assign(utf8.begin(), utf8.end());
        const auto entry = (restore.state & ~Fp::k_denormal_mask) | entry_mode;
        const auto callback_rounding = (entry ^ Fp::k_rounding_bit) & Fp::k_rounding_bit;
        Fp::write(entry);
        int polls = 0;
        request.cancelled = [&] {
            const auto current = Fp::read();
            if (polls < 2) {
                EXPECT_EQ(current & Fp::k_denormal_mask, entry_mode);
                EXPECT_EQ(current & Fp::k_rounding_bit, polls == 0 ? entry & Fp::k_rounding_bit : callback_rounding);
            }
            ++polls;
            Fp::write((current & ~(Fp::k_denormal_mask | Fp::k_rounding_bit)) | callback_rounding);
            return false;
        };
        int observed_events = 0;
        music::set_engine_audio_midi_event_hook(audio, [&](const auto &, std::uint64_t) {
            EXPECT_GT(polls, 0);
            const auto current = Fp::read();
            EXPECT_EQ(current & Fp::k_denormal_mask, Fp::k_denormal_mask);
            EXPECT_EQ(current & Fp::k_rounding_bit, callback_rounding);
            ++observed_events;
        });
        const auto rendered = music::export_score_audio(audio, master, request);
        const auto current = Fp::read();
        Fp::write(restore.state);
        music::set_engine_audio_midi_event_hook(audio, {});
        std::filesystem::remove(output);
        EXPECT_TRUE(rendered.ok) << rendered.error;
        EXPECT_GT(polls, 1);
        EXPECT_GT(observed_events, 0);
        EXPECT_EQ(current & Fp::k_denormal_mask, entry_mode);
        EXPECT_EQ(current & Fp::k_rounding_bit, callback_rounding);
    }
}
#endif

struct FailingFinalizer final {
    FailingFinalizer(const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device, bool fail_encode, int &ended)
        : device_(device), fail_encode_(fail_encode), ended_(ended) {}

    bool begin(muse::audio::samples_t) { return true; }

    size_t encode(muse::audio::samples_t samples, const float *) {
        const std::uint8_t marker = 42;
        if (!wrote_) {
            wrote_ = true;
            if (device_.write(&marker, 1) != 1) return 0;
        }
        return fail_encode_ ? 0 : static_cast<size_t>(samples);
    }

    muse::Ret end() {
        ++ended_;
        return muse::audio::make_ret(muse::audio::Err::ERROR_ENCODE, "injected finalization failure");
    }

private:
    muse::io::IODevice &device_;
    bool fail_encode_;
    int &ended_;
    bool wrote_ = false;
};

// Sets cancellation inside encode/end, then attempts two distinct device writes.
// The second write must be rejected immediately, before control returns to writer.
struct CancellingEncoder final {
    CancellingEncoder(const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device, bool &cancelled, bool final, int &encoded,
                      int &writes)
        : device_(device), cancelled_(cancelled), final_(final), encoded_(encoded), writes_(writes) {}

    bool begin(muse::audio::samples_t) { return true; }

    size_t encode(muse::audio::samples_t samples, const float *) {
        ++encoded_;
        if (!final_ && encoded_ == 2) inject();
        return samples;
    }

    muse::Ret end() {
        if (final_) inject();
        return muse::make_ok();
    }
private:
    void inject() {
        const uint8_t marker = 42;
        EXPECT_EQ(device_.write(&marker, 1), 1u);
        ++writes_;
        cancelled_ = true;
        EXPECT_EQ(device_.write(&marker, 1), 0u);
    }

    muse::io::IODevice &device_;
    bool &cancelled_;
    bool final_;
    int &encoded_;
    int &writes_;
};

// Owner-thread write granularity: one audio thread keeps encoding, and so every device write, on the
// caller. AsyncEncodeCancellationDrainsAndRecovers covers the encoder thread.
TEST(AudioInternal, CancellationAtSynthesisWriteAndFinalFlushRemovesFileAndRecovers) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread count under test";
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};
    set_audio_threads(document, 1);
    auto render = [&] {
        std::vector<std::byte> bytes;
        auto result =
            document.render_audio({}, [&](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto baseline = render();
    ASSERT_FALSE(baseline.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto output = std::filesystem::temp_directory_path() / ("engrave-cancel-internal-" + std::to_string(process) + ".wav");
    for (int phase = 0; phase < 4; ++phase) {
        bool cancelled = false;
        int encoded = 0, writes = 0, polls = 0;
        music::AudioRenderRequest request;
        const auto utf8 = output.generic_u8string();
        request.output_path.assign(utf8.begin(), utf8.end());
        request.cancelled = [&] {
            ++polls;
            if (phase == 3 && cancelled) throw std::runtime_error("cancel callback");
            return cancelled;
        };
        request.encoder_factory = [&](const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device) {
            return std::make_unique<CancellingEncoder>(format, device, cancelled, phase == 2, encoded, writes);
        };
        if (phase == 0) {
            music::set_engine_audio_midi_event_hook(audio, [&](const auto &, uint64_t) { cancelled = true; });
        }
        if (phase == 3) {
            EXPECT_THROW(music::export_score_audio(audio, master, request), std::runtime_error);
        } else {
            const auto result = music::export_score_audio(audio, master, request);
            EXPECT_FALSE(result.ok);
            EXPECT_EQ(result.failure, music::AudioRenderResult::Failure::CANCELLED);
        }
        music::set_engine_audio_midi_event_hook(audio, {});
        EXPECT_FALSE(std::filesystem::exists(output));
        EXPECT_GT(polls, 0);
        if (phase == 0) {
            EXPECT_TRUE(cancelled); // event handled inside synthesis block
            EXPECT_EQ(encoded, 0);  // stopped before that block reached encoder
        } else {
            EXPECT_EQ(writes, 1); // second write in the same encoder call rejected
            if (phase != 2)
                EXPECT_EQ(encoded, 2);
            else
                EXPECT_GT(encoded, 2); // reached final encoder flush
        }
        EXPECT_EQ(render(), baseline);
    }
    // Public adapter must preserve Cancelled and publish no bytes.
    engrave::AudioOptions options;
    int polls = 0, sinks = 0;
    options.cancelled = [&] { return ++polls == 100; };
    auto result = document.render_audio(options, [&](auto) { ++sinks; });
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, engrave::ErrorCode::CANCELLED);
    EXPECT_EQ(polls, 100);
    EXPECT_EQ(sinks, 0);
    const auto &state = *engrave::DocumentAccess::impl(document).engine;
    const auto intermediate = state.cache_dir / ("engrave-audio-" +
                                                 std::to_string(
#ifdef _WIN32
                                                     _getpid()
#else
                                                     getpid()
#endif
                                                         ) +
                                                 "-" + std::to_string(state.audio_render_index) + ".wav");
    EXPECT_FALSE(std::filesystem::exists(intermediate));
    EXPECT_EQ(render(), baseline);
}

TEST(AudioInternal, FinalizationFailureReportsFirstErrorAndNextRenderRecovers) {
    const auto path = music::test::corpus_file("ghost_note.mscx");
    ASSERT_FALSE(path.empty());
    auto opened = music::test::g_engine->open(path);
    ASSERT_TRUE(opened) << opened.error().message;
    auto &document = **opened;
    auto render = [&document] {
        std::vector<std::byte> bytes;
        const auto result = document.render_audio(
            {}, [&bytes](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto expected = render();
    ASSERT_FALSE(expected.empty());

    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto output = std::filesystem::temp_directory_path() / ("engrave-finalization-test-" + std::to_string(process) + ".wav");

    struct Cleanup {
        std::filesystem::path path;

        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{output};

    for (const bool fail_encode : {false, true}) {
        int ended = 0;
        music::AudioRenderRequest request;
        const auto output_utf8 = output.generic_u8string();
        request.output_path.assign(output_utf8.begin(), output_utf8.end());
        request.encoder_factory = [fail_encode, &ended](const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device) {
            return std::make_unique<FailingFinalizer>(format, device, fail_encode, ended);
        };
        const auto failed = music::export_score_audio(audio, master, request);
        EXPECT_FALSE(failed.ok);
        EXPECT_EQ(failed.failure, music::AudioRenderResult::Failure::ENGINE_FAILURE);
        EXPECT_EQ(ended, 1);
        EXPECT_GT(std::filesystem::file_size(output), 0u);
        if (fail_encode) {
            EXPECT_EQ(failed.error.find("injected finalization failure"), std::string::npos)
                << "a later finalization failure replaced the encode failure";
        } else {
            EXPECT_NE(failed.error.find("injected finalization failure"), std::string::npos);
        }
        EXPECT_EQ(render(), expected);
    }

    // Factories must hand over sole ownership: raw and shared pointers do not convert.
    static_assert(std::is_convertible_v<std::unique_ptr<FailingFinalizer>, muse::audio::encode::UniqueEncoder>);
    static_assert(!std::is_convertible_v<FailingFinalizer *, muse::audio::encode::UniqueEncoder>);
    static_assert(!std::is_convertible_v<std::shared_ptr<FailingFinalizer>, muse::audio::encode::UniqueEncoder>);
    // A null owner is an empty encoder: the export fails cleanly instead of dispatching through nullptr.
    music::AudioRenderRequest request;
    const auto output_utf8 = output.generic_u8string();
    request.output_path.assign(output_utf8.begin(), output_utf8.end());
    request.encoder_factory = [](const muse::audio::SoundTrackFormat &, muse::io::IODevice &) {
        return std::unique_ptr<FailingFinalizer>{};
    };
    EXPECT_FALSE(music::export_score_audio(audio, master, request).ok);
    EXPECT_EQ(render(), expected);
}

TEST(AudioInternal, BudgetFailureRemovesTheIntermediateAndAllowsRecovery) {
    const auto path = music::test::corpus_file("ghost_note.mscx");
    ASSERT_FALSE(path.empty());
    auto opened = music::test::g_engine->open(path);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto render = [&document] {
        std::vector<std::byte> bytes;
        const auto result = document.render_audio(
            {}, [&bytes](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto original = render();
    ASSERT_FALSE(original.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto output = std::filesystem::temp_directory_path() / ("engrave-budget-test-" + std::to_string(process) + ".wav");

    struct Cleanup {
        std::filesystem::path path;

        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{output};

    music::AudioRenderRequest request;
    const auto output_utf8 = output.generic_u8string();
    request.output_path.assign(output_utf8.begin(), output_utf8.end());
    request.max_bytes = 64;
    for (const auto format :
         {music::EngineAudioFormat::WAV, music::EngineAudioFormat::FLAC, music::EngineAudioFormat::OGG, music::EngineAudioFormat::MP3}) {
        request.format = format;
        const auto failed = music::export_score_audio(audio, master, request);
        EXPECT_FALSE(failed.ok);
        EXPECT_EQ(failed.failure, music::AudioRenderResult::Failure::RESOURCE_LIMIT);
        EXPECT_FALSE(std::filesystem::exists(output));
    }
    EXPECT_EQ(render(), original);
}

TEST(AudioInternal, SoundFontRefreshWaitsForNextRender) {
    using namespace music::test;
    if (g_soundfonts.size() < 2) GTEST_SKIP() << "pass two different SoundFonts";
    auto opened = g_engine->open(corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened) << opened.error().message;
    auto &document = **opened;
    ASSERT_TRUE(g_engine->set_soundfonts({g_soundfonts[0]}));
    auto render = [&document] {
        std::vector<std::byte> bytes;
        const auto result = document.render_audio(
            {}, [&bytes](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto first = render();
    ASSERT_FALSE(first.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    const auto original_uri = music::engine_soundfont_uri(audio);
    ASSERT_FALSE(original_uri.empty());

    ASSERT_TRUE(g_engine->set_soundfonts({g_soundfonts[1]}));
    EXPECT_EQ(music::engine_soundfont_uri(audio), original_uri);
    const auto selection = g_engine->capabilities().soundfonts;
    ASSERT_EQ(selection.size(), 1u);
    EXPECT_NE(selection[0], g_soundfonts[1]);
    const auto second = render();
    ASSERT_FALSE(second.empty());
    EXPECT_NE(second, first);
    EXPECT_NE(music::engine_soundfont_uri(audio), original_uri);
    EXPECT_EQ(render(), second);
    ASSERT_TRUE(g_engine->set_soundfonts({g_soundfonts[0]}));
    EXPECT_EQ(render(), first);
    EXPECT_EQ(music::engine_soundfont_uri(audio), original_uri);
    ASSERT_TRUE(g_engine->set_soundfonts(g_soundfonts));
}

} // namespace

// Real driver/graph teardown is included. Phase requests occur synchronously on
// the owner thread; the separate delayed case samples arbitrary operation work.
TEST(AudioInternal, CancellationGranularityAndRecovery) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread count under test";
    auto opened = music::test::g_engine->open(music::test::corpus_file("repeat70.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    // The test encoders write and poll on the caller's thread.
    const AutomaticAudioThreads restore{document};
    set_audio_threads(document, 1);
    const auto render = [&] {
        std::vector<std::byte> bytes;
        EXPECT_TRUE(document.render_audio({}, [&](auto chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); }));
        return bytes;
    };
    const auto baseline = render();
    ASSERT_FALSE(baseline.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);

    struct LargeWriter final {
    private:
        muse::io::IODevice &device;
        bool final;
        bool &armed;
        std::vector<uint8_t> bytes = std::vector<uint8_t>(1024 * 1024, 42);
    public:
        LargeWriter(const muse::audio::SoundTrackFormat &f, muse::io::IODevice &d, bool end, bool &a) : device(d), final(end), armed(a) {}

        bool begin(muse::audio::samples_t) { return true; }

        size_t encode(muse::audio::samples_t n, const float *) {
            if (!final) {
                armed = true;
                return device.write(bytes.data(), bytes.size()) ? n : 0;
            }
            return n;
        }

        muse::Ret end() {
            armed = true;
            return device.write(bytes.data(), bytes.size()) ? muse::make_ok() : muse::Ret(false);
        }
    };

    const auto path = std::filesystem::temp_directory_path() / "engrave-granularity.wav";
    for (int phase = 0; phase < 4; ++phase) {
        bool armed = false, cancelled = false, started_signal = false;
        int write_polls = 0;
        std::latch work_started{1};
        std::latch signal_sent{1};
        std::atomic<bool> external{false};
        music::AudioRenderRequest request;
        const auto utf8 = path.generic_u8string();
        request.output_path.assign(utf8.begin(), utf8.end());
        request.cancelled = [&] {
            if (armed && ++write_polls == 3) cancelled = true;
            if (phase == 3 && !started_signal &&
                muse::audio::OfflineCancellation::phase == muse::audio::OfflineCancellation::Phase::SYNTHESIS) {
                started_signal = true;
                work_started.count_down();
                signal_sent.wait();
                return false;
            }
            return cancelled || external.load();
        };
        if (phase == 0)
            music::set_engine_audio_midi_event_hook(audio, [&](const auto &, uint64_t) {
                if (!cancelled && muse::audio::OfflineCancellation::phase == muse::audio::OfflineCancellation::Phase::SYNTHESIS) {
                    cancelled = true;
                }
            });
        if (phase == 1 || phase == 2)
            request.encoder_factory = [&](const auto &f, auto &d) { return std::make_unique<LargeWriter>(f, d, phase == 2, armed); };
        std::thread transport;
        if (phase == 3)
            transport = std::thread([&] {
                work_started.wait();
                external.store(true);
                signal_sent.count_down();
            });
        const auto result = music::export_score_audio(audio, master, request);
        if (transport.joinable()) {
            if (!started_signal) work_started.count_down();
            transport.join();
        }
        music::set_engine_audio_midi_event_hook(audio, {});
        if (phase == 3) ASSERT_TRUE(started_signal);
        ASSERT_EQ(result.failure, music::AudioRenderResult::Failure::CANCELLED) << "phase=" << phase;
        ASSERT_FALSE(std::filesystem::exists(path)) << "phase=" << phase;
        EXPECT_EQ(render(), baseline) << "cancellation poisoned the SoundFont cache (phase=" << phase << ")";
    }
}

#ifdef ENGRAVE_PLAYBACK_HARNESS
TEST(AudioInternal, FlatPlanConstructionFailureDestroysTracksAndRecovers) {
    using muse::audio::engine::OfflineRenderSession;
    using muse::audio::synth::FluidTrack;
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto render = [&] {
        std::vector<std::byte> bytes;
        auto result =
            document.render_audio({}, [&](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto baseline = render();
    ASSERT_FALSE(baseline.empty());
    ASSERT_EQ(FluidTrack::live_instances(), 0);
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    music::AudioRenderRequest request;
    request.output_path = (std::filesystem::temp_directory_path() / "engrave-plan-fault.wav").generic_string();
    for (int count : {0, 1}) {
        struct Reset {
            ~Reset() { OfflineRenderSession::fail_after_tracks = -1; }
        } reset;

        OfflineRenderSession::fail_after_tracks = count;
        EXPECT_THROW(music::export_score_audio(audio, master, request), std::bad_alloc);
        EXPECT_EQ(FluidTrack::live_instances(), 0);
        OfflineRenderSession::fail_after_tracks = -1;
        EXPECT_EQ(render(), baseline);
        EXPECT_EQ(FluidTrack::live_instances(), 0);
    }
}
#endif

TEST(AudioInternal, FailedEventSourceDisarmsBorrow) {
    using namespace muse;
    using namespace muse::audio;
    using namespace muse::audio::engine;

    // AudioResources installs engine-lifetime C bridges. Isolate this deliberately
    // failing lower-level owner from the suite's live engine and cached fonts.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    ASSERT_EXIT(
        {
            music::test::g_engine.reset();
            const auto check = [] {
                AudioResources resources;
                AudioEngineConfiguration config;
                const auto events = std::make_unique<mpe::TrackEvents>();
                FluidTrackSource node(resources, config.render_settings(), {44100, 4096, 2}, {}, *events);
                node.set_output_spec({44100, 4096, 2});
                // No resource resolves: the first apply fails after consuming the borrow.
                node.apply_input_params({});
                if (node.initialized()) return 1;
                // Make the default resolvable while the events stay alive. A borrow still
                // armed after the failure would now initialize; a disarmed one cannot reload.
                const auto default_id = config.default_audio_input_params().resource_meta.id;
                AudioResources::SoundFontResource font;
                font.path = io::PathT(music::test::g_soundfonts.front().string());
                resources.resources.insert_or_assign(default_id, std::move(font));
                node.apply_input_params({});
                if (node.initialized()) return 2;
                // Positive control: a fresh borrow with the same resources does initialize.
                FluidTrackSource fresh(resources, config.render_settings(), {44100, 4096, 2}, {}, *events);
                fresh.set_output_spec({44100, 4096, 2});
                fresh.apply_input_params({});
                return fresh.initialized() ? 0 : 3;
            };
            std::exit(check());
        },
        ::testing::ExitedWithCode(0), "");
}

// Multi-part dense synthetic score covering cancellation and recovery across all 5 phases.
TEST(AudioInternal, DenseScoreCancellationAndRecovery) {
    const auto owner_thread = std::this_thread::get_id();
    const std::string prefix = "engrave-dense-cancel-" + std::to_string(
#ifdef _WIN32
                                                             _getpid()
#else
                                                             getpid()
#endif
                                                         );
    const auto xml = std::filesystem::temp_directory_path() / (prefix + ".musicxml");
    {
        std::ofstream out(xml);
        out << "<?xml version=\"1.0\"?><score-partwise version=\"4.0\"><part-list>";
        for (int p = 1; p <= 4; ++p)
            out << "<score-part id=\"P" << p << "\"><part-name>Part " << p << "</part-name><midi-instrument id=\"I" << p
                << "\"><midi-channel>" << p << "</midi-channel><midi-program>1</midi-program></midi-instrument></score-part>";
        out << "</part-list>";
        for (int p = 1; p <= 4; ++p) {
            out << "<part id=\"P" << p << "\">";
            for (int m = 1; m <= 32; ++m) {
                out << "<measure number=\"" << m << "\">";
                if (m == 1)
                    out << "<attributes><divisions>480</divisions><time><beats>4</beats>"
                           "<beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes>"
                           "<direction><direction-type><metronome><beat-unit>quarter</beat-unit>"
                           "<per-minute>300</per-minute></metronome></direction-type><sound tempo=\"300\"/></direction>";
                for (char step : std::string("CDEFGABCCDEFGABC"))
                    out << "<note><pitch><step>" << step
                        << "</step><octave>4</octave></pitch>"
                           "<duration>120</duration><type>16th</type></note>";
                out << "</measure>";
            }
            out << "</part>";
        }
        out << "</score-partwise>";
    }
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto render = [&] {
        std::vector<std::byte> bytes;
        EXPECT_TRUE(document.render_audio({}, [&](auto chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); }));
        return bytes;
    };
    const auto baseline = render();
    ASSERT_FALSE(baseline.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    const auto path = std::filesystem::temp_directory_path() / (prefix + ".wav");
    using Phase = muse::audio::OfflineCancellation::Phase;
    const char *names[] = {"before-synthesis", "playback-model", "synthesis", "encode", "event-release"};
    for (int phase = 0; phase < 5; ++phase) {
        bool cancelled = false;
        bool observed = false;
        bool model_started = false;
        int model_polls = 0, stage_polls = 0;
        std::latch model_start{1};
        std::atomic<bool> external{false};
        std::thread transport;
        if (phase == 1)
            transport = std::thread([&] {
                model_start.wait();
                external.store(true);
            });
        music::AudioRenderRequest request;
        const auto utf8 = path.generic_u8string();
        request.output_path.assign(utf8.begin(), utf8.end());
        request.cancelled = [&] {
            EXPECT_EQ(std::this_thread::get_id(), owner_thread);
            const auto current_phase = muse::audio::OfflineCancellation::phase;
            if (!cancelled && ((phase == 3 && current_phase == Phase::ENCODING) || (phase == 0 && current_phase == Phase::SEQUENCER)) &&
                ++stage_polls == 1) {
                cancelled = true;
                return false;
            }
            if (!cancelled && phase == 4 && current_phase == Phase::EVENT_RELEASE) {
                cancelled = true;
                return false;
            }
            if (phase == 1 && muse::audio::OfflineCancellation::phase == Phase::PLAYBACK_MODEL && ++model_polls == 500 && !model_started) {
                model_started = true;
                model_start.count_down();
            }
            if (external.load() && !cancelled) {
                EXPECT_EQ(muse::audio::OfflineCancellation::phase, Phase::PLAYBACK_MODEL);
                cancelled = true;
            }
            if (cancelled) observed = true;
            return cancelled;
        };
        if (phase == 2)
            music::set_engine_audio_midi_event_hook(audio, [&](const auto &, uint64_t) {
                if (muse::audio::OfflineCancellation::phase == Phase::SYNTHESIS) cancelled = true;
            });
        const auto result = music::export_score_audio(audio, master, request);
        if (transport.joinable()) {
            if (!model_started) model_start.count_down();
            transport.join();
            ASSERT_TRUE(model_started);
        }
        music::set_engine_audio_midi_event_hook(audio, {});
        ASSERT_TRUE(cancelled) << names[phase];
        ASSERT_TRUE(observed) << names[phase];
        ASSERT_EQ(result.failure, music::AudioRenderResult::Failure::CANCELLED) << names[phase];
        ASSERT_FALSE(std::filesystem::exists(path)) << names[phase];
        EXPECT_EQ(render(), baseline) << names[phase];
    }
    // Exercise error translation and publication through the public API too.
    engrave::AudioOptions options;
    int model_polls = 0, sinks = 0;
    options.cancelled = [&] { return muse::audio::OfflineCancellation::phase == Phase::PLAYBACK_MODEL && ++model_polls == 500; };
    const auto result = document.render_audio(options, [&](auto) { ++sinks; });
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, engrave::ErrorCode::CANCELLED);
    EXPECT_EQ(sinks, 0);
    const auto &state = *engrave::DocumentAccess::impl(document).engine;
    const auto intermediate = state.cache_dir / ("engrave-audio-" +
                                                 std::to_string(
#ifdef _WIN32
                                                     _getpid()
#else
                                                     getpid()
#endif
                                                         ) +
                                                 "-" + std::to_string(state.audio_render_index) + ".wav");
    EXPECT_FALSE(std::filesystem::exists(intermediate));
    model_polls = 0;
    options.cancelled = [&]() -> bool {
        if (muse::audio::OfflineCancellation::phase == Phase::PLAYBACK_MODEL && ++model_polls == 500)
            throw std::runtime_error("model cancellation callback");
        return false;
    };
    EXPECT_THROW(std::ignore = document.render_audio(options, [](auto) {}), std::runtime_error);
    EXPECT_EQ(render(), baseline);
}

TEST(AudioInternal, CancellationDuringPublicationCleansUpAndRecovers) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("repeat70.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto render = [&] {
        std::vector<std::byte> bytes;
        EXPECT_TRUE(document.render_audio({}, [&](auto chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); }));
        return bytes;
    };
    const auto baseline = render();
    ASSERT_GT(baseline.size(), 1u << 20);
    bool cancel = false;
    int sinks = 0;
    engrave::AudioOptions options;
    const auto owner = std::this_thread::get_id();
    options.cancelled = [&] {
        EXPECT_EQ(std::this_thread::get_id(), owner);
        return cancel;
    };
    const auto result = document.render_audio(options, [&](auto) {
        ++sinks;
        cancel = true;
    });
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, engrave::ErrorCode::CANCELLED);
    EXPECT_EQ(sinks, 1);
    const auto &state = *engrave::DocumentAccess::impl(document).engine;
    const auto intermediate = state.cache_dir / ("engrave-audio-" +
                                                 std::to_string(
#ifdef _WIN32
                                                     _getpid()
#else
                                                     getpid()
#endif
                                                         ) +
                                                 "-" + std::to_string(state.audio_render_index) + ".wav");
    EXPECT_FALSE(std::filesystem::exists(intermediate));
    EXPECT_EQ(render(), baseline);
}

TEST(AudioInternal, MidiTimelineSlicesAndLimits) {
    using namespace muse::audio::engine;
    using namespace muse::audio::synth;
    using muse::midi::Event;

    struct ExposedSequencer : FluidSequencer {
        using FluidSequencer::chunk_plan_;
        using FluidSequencer::main_stream_events_;
    };

    // 1. checked_grow enforces uint32_t bounds and throws StoreLimit.
    EXPECT_EQ(checked_grow(0, 0), 0u);
    EXPECT_EQ(checked_grow(10, 20), 30u);
    EXPECT_THROW(std::ignore = checked_grow(static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1, 0), StoreLimit);
    EXPECT_THROW(std::ignore = checked_grow(std::numeric_limits<std::uint32_t>::max(), 1), StoreLimit);

    // 2. ChannelMap::init enforces k_fluid_channel_count (16) even in release builds.
    {
        ChannelMap channels;
        muse::mpe::PlaybackSetupData setup;
        EXPECT_THROW(channels.init(setup, std::nullopt, k_fluid_channel_count + 1), std::invalid_argument);
    }

    // 3. MidiTimeline + FluidSequencer slice planning, continuation merge, <= boundary, seek, and buffer reuse.
    ExposedSequencer seq;
    seq.load(muse::mpe::TrackEvents{});

    auto make_note = [](Event::Opcode opcode, std::uint32_t note, std::uint32_t vel) {
        Event ev(opcode, Event::MessageType::CHANNEL_VOICE10);
        ev.set_channel(0);
        ev.set_note(note);
        ev.set_velocity7(vel);
        return ev;
    };
    const Event ev0 = make_note(Event::Opcode::NOTE_ON, 60, 100);
    const Event ev1 = make_note(Event::Opcode::NOTE_OFF, 60, 0);
    const Event ev2 = make_note(Event::Opcode::NOTE_ON, 62, 100);
    const Event ev3 = make_note(Event::Opcode::NOTE_OFF, 62, 0);

    emplace_checked(seq.main_stream_events_.events, 0ULL, ev0);
    emplace_checked(seq.main_stream_events_.events, 0ULL, ev1);
    emplace_checked(seq.main_stream_events_.events, 100ULL, ev2);
    emplace_checked(seq.main_stream_events_.events, 250ULL, ev3);

    // Inactive: returns 1 empty continuation slice bound to TimelineStream::OFF.
    {
        const auto &plan = seq.move_playback_forward(50);
        ASSERT_EQ(plan.size(), 1u);
        EXPECT_EQ(plan[0].at, 0ULL);
        EXPECT_EQ(plan[0].count, 0u);
        EXPECT_EQ(plan[0].stream, TimelineStream::OFF);
        EXPECT_TRUE(seq.slice_events(plan[0]).empty());
    }

    // Active: continuation slice at 0 merges with timestamp 0 group; <= boundary includes 100.
    seq.set_active(true);
    seq.set_playback_position(0);
    {
        const auto &plan = seq.move_playback_forward(100);
        ASSERT_EQ(plan.size(), 2u);
        EXPECT_EQ(plan[0].at, 0ULL);
        EXPECT_EQ(plan[0].first, 0u);
        EXPECT_EQ(plan[0].count, 2u);
        EXPECT_EQ(plan[0].stream, TimelineStream::MAIN);
        EXPECT_EQ(plan[1].at, 100ULL);
        EXPECT_EQ(plan[1].first, 2u);
        EXPECT_EQ(plan[1].count, 1u);
        EXPECT_EQ(plan[1].stream, TimelineStream::MAIN);

        // Switching is_active_ while a plan is live keeps slices bound to their originating stream (MAIN).
        seq.set_active(false);
        const auto span0 = seq.slice_events(plan[0]);
        ASSERT_EQ(span0.size(), 2u);
        EXPECT_EQ(span0[0].event, ev0);
        EXPECT_EQ(span0[1].event, ev1);
        seq.set_active(true);
    }

    const auto reused_cap = seq.chunk_plan_.capacity();
    // Next chunk [100..200]: no events in (100..200], returns empty continuation slice at 100.
    {
        const auto &plan = seq.move_playback_forward(100);
        ASSERT_EQ(plan.size(), 1u);
        EXPECT_EQ(plan[0].at, 100ULL);
        EXPECT_EQ(plan[0].count, 0u);
        EXPECT_TRUE(seq.slice_events(plan[0]).empty());
        EXPECT_GE(seq.chunk_plan_.capacity(), reused_cap);
    }

    // Next chunk [200..250]: empty continuation at 200 + event at 250.
    {
        const auto &plan = seq.move_playback_forward(50);
        ASSERT_EQ(plan.size(), 2u);
        EXPECT_EQ(plan[0].at, 200ULL);
        EXPECT_EQ(plan[0].count, 0u);
        EXPECT_EQ(plan[1].at, 250ULL);
        EXPECT_EQ(plan[1].first, 3u);
        EXPECT_EQ(plan[1].count, 1u);
    }

    // Seek to 100 invalidates any live slice plan and positions cursor at index 2 (timestamp 100).
    {
        const ChunkSlice stale_slice = seq.chunk_plan_[1];
        seq.set_playback_position(100);
        EXPECT_TRUE(seq.chunk_plan_.empty());
        EXPECT_THROW(std::ignore = seq.slice_events(stale_slice), StoreLimit);
    }
    EXPECT_EQ(seq.main_stream_events_.cursor, 2u);
    {
        const auto &plan = seq.move_playback_forward(50);
        ASSERT_EQ(plan.size(), 1u);
        EXPECT_EQ(plan[0].at, 100ULL);
        EXPECT_EQ(plan[0].first, 2u);
        EXPECT_EQ(plan[0].count, 1u);
    }

    // flush_offstream and load also invalidate any live slice plan.
    {
        const ChunkSlice stale_slice = seq.chunk_plan_[0];
        seq.flush_offstream();
        EXPECT_THROW(std::ignore = seq.slice_events(stale_slice), StoreLimit);
    }
}

TEST(AudioInternal, BoundedStableSortAndDedupCancellation) {
    using namespace muse::audio::engine;
    using namespace muse::audio::synth;
    using muse::midi::Event;

    struct ExposedSequencer : FluidSequencer {
        using FluidSequencer::bounded_stable_sort;
        using FluidSequencer::dedup_timeline;
        using FluidSequencer::k_sort_block_size;
        using FluidSequencer::main_stream_events_;
        using FluidSequencer::next_sort_block_end;
    };

    // 0. Block-sort offset arithmetic near the uint32 limit terminates without wrapping (tested without allocating).
    {
        constexpr std::uint32_t max_u32 = std::numeric_limits<std::uint32_t>::max();
        constexpr std::uint32_t last_aligned = max_u32 - (max_u32 % ExposedSequencer::k_sort_block_size); // 4,294,959,104
        for (const std::uint32_t total : {last_aligned + 1u, last_aligned + 4096u, max_u32 - 1u, max_u32}) {
            std::uint32_t offset = last_aligned - ExposedSequencer::k_sort_block_size;
            int steps = 0;
            while (offset < total) {
                const std::uint32_t next = ExposedSequencer::next_sort_block_end(offset, total);
                ASSERT_GT(next, offset);
                ASSERT_LE(next, total);
                offset = next;
                ASSERT_LE(++steps, 2);
            }
            EXPECT_EQ(offset, total);
            EXPECT_EQ(steps, 2);
        }
    }

    const auto make_unsorted_timeline = [](std::uint32_t count) {
        MidiTimeline timeline;
        reserve_checked(timeline.events, count);
        for (std::uint32_t i = 0; i < count; ++i) {
            Event ev(Event::Opcode::NOTE_ON, Event::MessageType::CHANNEL_VOICE10);
            ev.set_channel(static_cast<muse::midi::channel_t>(i % 16));
            ev.set_note(static_cast<std::uint8_t>(i % 128));
            ev.set_velocity7(static_cast<std::uint8_t>((i / 128) % 128));
            const auto ts = static_cast<muse::mpe::timestamp_t>((count - i) % 97);
            emplace_checked(timeline.events, TimedMidiEvent{ts, ev});
        }
        timeline.cursor = 7;
        return timeline;
    };

    // 1. Cancellation between 8,192-event block sorts (20,000 events -> 3 blocks; poll 2 fires before block 1).
    {
        auto timeline = make_unsorted_timeline(20000);
        int polls = 0;
        const auto poll_fn = [&] { return ++polls == 2; };
        muse::audio::OfflineCancellation scope(poll_fn);
        EXPECT_FALSE(ExposedSequencer::bounded_stable_sort(timeline));
        EXPECT_EQ(polls, 2);
        EXPECT_TRUE(timeline.empty());
        EXPECT_EQ(timeline.cursor, 0u);
    }

    // 2. Cancellation during the 8,192-output merge pass (20,000 events: poll 1 at entry, polls 2-3 before blocks 1-2,
    //    poll 4 after block sorts, poll 5 after scratch reserve, poll 6 after 8,192 merged outputs).
    {
        auto timeline = make_unsorted_timeline(20000);
        int polls = 0;
        const auto poll_fn = [&] { return ++polls == 6; };
        muse::audio::OfflineCancellation scope(poll_fn);
        EXPECT_FALSE(ExposedSequencer::bounded_stable_sort(timeline));
        EXPECT_EQ(polls, 6);
        EXPECT_TRUE(timeline.empty());
        EXPECT_EQ(timeline.cursor, 0u);
    }

    // 3. Cancellation during dedup_timeline at the 4,096-event stride (6,000 events: poll 1 at entry, poll 2 at 4,096).
    {
        auto timeline = make_unsorted_timeline(6000);
        ASSERT_TRUE(ExposedSequencer::bounded_stable_sort(timeline));
        timeline.cursor = 3;

        int polls = 0;
        const auto poll_fn = [&] { return ++polls == 2; };
        muse::audio::OfflineCancellation scope(poll_fn);
        EXPECT_FALSE(ExposedSequencer::dedup_timeline(timeline));
        EXPECT_EQ(polls, 2);
        EXPECT_TRUE(timeline.empty());
        EXPECT_EQ(timeline.cursor, 0u);
    }

    // 4. Direct resample_curve early exit: returning false stops evaluating remaining steps and curve segments.
    {
        muse::mpe::AutomationCurve<muse::mpe::timestamp_t> curve;
        curve[0] = muse::mpe::AutomationPoint{muse::mpe::AutomationPoint::ArrivalFromPrevious{}, muse::real_t(0.1)};
        curve[30000ULL * 100000ULL] =
            muse::mpe::AutomationPoint{muse::mpe::AutomationPoint::ExplicitArrival{muse::real_t(0.9), {}}, muse::real_t(0.9)};
        curve[30000ULL * 200000ULL] =
            muse::mpe::AutomationPoint{muse::mpe::AutomationPoint::ExplicitArrival{muse::real_t(0.2), {}}, muse::real_t(0.2)};
        std::size_t samples = 0;
        muse::mpe::resample_curve(curve, muse::mpe::timestamp_t{30000},
                                  [&](muse::mpe::timestamp_t, muse::real_t) -> bool { return ++samples < 4096; });
        EXPECT_EQ(samples, 4096u);
    }

    // 5. End-to-end FluidSequencer::load cancellation during a long dynamic curve stops resampling at the first
    //    positive 4,096-sample poll and discards the timeline.
    {
        muse::mpe::TrackEvents track;
        muse::mpe::FreezeProgress progress;
        muse::mpe::PlaybackSetupData setup;
        setup.supports_single_note_dynamics = true;

        const auto level = muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::MF);
        muse::mpe::emit_note(track, 0, {0, 1000, 0, 0, 0, level}, {}, {}, progress);

        // Non-flat curve spanning 100,000 steps of 30,000 us (~24.4x the 4,096-sample poll stride), plus a second layer.
        muse::mpe::DynamicAutomationMap curve0;
        curve0[0] = muse::mpe::AutomationPoint{muse::mpe::AutomationPoint::ArrivalFromPrevious{}, muse::real_t(0.1)};
        curve0[30000ULL * 100000ULL] =
            muse::mpe::AutomationPoint{muse::mpe::AutomationPoint::ExplicitArrival{muse::real_t(0.9), {}}, muse::real_t(0.9)};
        muse::mpe::DynamicAutomationMap curve1;
        curve1[0] = muse::mpe::AutomationPoint{muse::mpe::AutomationPoint::ArrivalFromPrevious{}, muse::real_t(0.2)};
        curve1[30000ULL * 50000ULL] =
            muse::mpe::AutomationPoint{muse::mpe::AutomationPoint::ExplicitArrival{muse::real_t(0.8), {}}, muse::real_t(0.8)};

        muse::mpe::DynamicAutomationLayers dynamics;
        dynamics.emplace(0, std::make_shared<const muse::mpe::DynamicAutomationMap>(std::move(curve0)));
        dynamics.emplace(1, std::make_shared<const muse::mpe::DynamicAutomationMap>(std::move(curve1)));

        muse::mpe::freeze_dynamics(track, dynamics, progress);
        ExposedSequencer seq;
        seq.init(setup, muse::midi::Program{0, 0}, true, k_fluid_channel_count);
        seq.main_stream_events_.cursor = 5;

        // Poll 1: note event in add_playback_events.
        // Poll 2: layer 0 entry in add_dynamic_events.
        // Poll 3: first 4,096-sample poll inside resample_curve. If resample_curve did not stop, it would poll 24 more times.
        int polls = 0;
        const auto cancel_in_curve = [&] { return ++polls >= 3; };
        {
            muse::audio::OfflineCancellation scope(cancel_in_curve);
            seq.load(track);
        }
        EXPECT_EQ(polls, 3);
        EXPECT_TRUE(seq.main_stream_events_.empty());
        EXPECT_EQ(seq.main_stream_events_.cursor, 0u);
    }

    // 6. End-to-end FluidSequencer::load cancellation during sort/merge/dedup leaves a cleared timeline and recovers.
    {
        muse::mpe::TrackEvents track;
        muse::mpe::FreezeProgress progress;
        muse::mpe::PlaybackSetupData setup;
        setup.supports_single_note_dynamics = true;
        for (int i = 0; i < 5000; ++i) {
            const muse::mpe::timestamp_t timestamp = (5000 - i) * 10;
            const auto pitch = (i % 24) * muse::mpe::pitch_level_step;
            const auto level = muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::MF);
            muse::mpe::emit_note(track, timestamp, {timestamp, 100, 0, 0, pitch, level}, {}, {}, progress);
        }

        ExposedSequencer seq;
        seq.init(setup, muse::midi::Program{0, 0}, true, k_fluid_channel_count);

        // 5,000 notes emit 10,000 NOTE_ON/NOTE_OFF events (> 8,192 block size).
        // Cancel on poll 5004 (inside bounded_stable_sort after the 5,000 note polls).
        int polls = 0;
        const auto cancel_in_sort = [&] { return ++polls == 5004; };
        {
            muse::audio::OfflineCancellation scope(cancel_in_sort);
            seq.load(track);
        }
        EXPECT_EQ(polls, 5004);
        EXPECT_TRUE(seq.main_stream_events_.empty());
        EXPECT_EQ(seq.main_stream_events_.cursor, 0u);

        // Subsequent non-cancelled load recovers cleanly.
        seq.load(track);
        EXPECT_EQ(seq.main_stream_events_.events.size(), 10000u);
        EXPECT_EQ(seq.main_stream_events_.cursor, 0u);
    }
}

TEST(AudioInternal, MidiTimelineDedupAndEqualTimeOrder) {
    using namespace muse::audio::engine;
    using namespace muse::audio::synth;
    using muse::midi::Event;

    struct ExposedSequencer : FluidSequencer {
        using FluidSequencer::bounded_stable_sort;
        using FluidSequencer::dedup_timeline;
        using FluidSequencer::main_stream_events_;
    };

    // 1. Multi-block bounded_stable_sort (20,000 events across 3 blocks) matches std::stable_sort bit-for-bit,
    //    preserving equal-timestamp emission order across block boundaries.
    {
        MidiTimeline actual;
        std::vector<TimedMidiEvent> expected;
        constexpr std::uint32_t total = 20000;
        reserve_checked(actual.events, total);
        expected.reserve(total);

        for (std::uint32_t i = 0; i < total; ++i) {
            Event ev(Event::Opcode::NOTE_ON, Event::MessageType::CHANNEL_VOICE20);
            ev.set_channel(static_cast<muse::midi::channel_t>(i % 16));
            ev.set_note(static_cast<std::uint8_t>(i % 128));
            ev.set_velocity16(static_cast<std::uint16_t>(i & 0xFFFFu));
            const auto ts = static_cast<muse::mpe::timestamp_t>((i * 37) % 53);
            emplace_checked(actual.events, TimedMidiEvent{ts, ev});
            expected.push_back(TimedMidiEvent{ts, ev});
        }

        ASSERT_TRUE(ExposedSequencer::bounded_stable_sort(actual));
        std::stable_sort(expected.begin(), expected.end(),
                         [](const TimedMidiEvent &lhs, const TimedMidiEvent &rhs) { return lhs.timestamp < rhs.timestamp; });

        ASSERT_EQ(actual.events.size(), expected.size());
        for (std::uint32_t i = 0; i < total; ++i) {
            EXPECT_EQ(actual.events[i].timestamp, expected[i].timestamp);
            EXPECT_EQ(actual.events[i].event, expected[i].event);
        }
    }

    // 2. Per-bucket CC deduplication and equal-time emission order:
    //    - Eligible CCs (sustain, sostenuto, modwheel) deduplicate on exact (opcode, channel, controller, value)
    //      within a timestamp bucket while keeping first-seen position relative to other events.
    //    - Eligible CCs with different channel, controller, value, or in a later timestamp bucket are kept.
    //    - Pitch bends and late dynamics (expression_controller) are NEVER deduplicated.
    {
        auto make_cc = [](muse::midi::channel_t ch, int ctrl, std::uint32_t val) {
            Event cc(Event::Opcode::CONTROL_CHANGE, Event::MessageType::CHANNEL_VOICE10);
            cc.set_channel(ch);
            cc.set_index(ctrl);
            cc.set_data(val);
            return cc;
        };
        auto make_bend = [](muse::midi::channel_t ch, std::uint32_t val) {
            Event bend(Event::Opcode::PITCH_BEND, Event::MessageType::CHANNEL_VOICE10);
            bend.set_channel(ch);
            bend.set_data(val);
            return bend;
        };
        auto make_note = [](Event::Opcode op, muse::midi::channel_t ch, std::uint8_t note) {
            Event ev(op, Event::MessageType::CHANNEL_VOICE20);
            ev.set_channel(ch);
            ev.set_note(note);
            ev.set_velocity16(32000);
            return ev;
        };

        const Event note_on_a = make_note(Event::Opcode::NOTE_ON, 0, 60);
        const Event sustain_on_ch0 = make_cc(0, muse::midi::sustain_pedal_controller, 127);
        const Event bend_center = make_bend(0, 8192);
        const Event sustain_on_ch1 = make_cc(1, muse::midi::sustain_pedal_controller, 127);
        const Event sustain_off_ch0 = make_cc(0, muse::midi::sustain_pedal_controller, 0);
        const Event sostenuto_on_ch0 = make_cc(0, muse::midi::sostenuto_pedal_controller, 127);
        const Event expr_ch0_a = make_cc(0, muse::midi::expression_controller, 96);
        const Event expr_ch0_b = make_cc(0, muse::midi::expression_controller, 96);
        const Event note_on_b = make_note(Event::Opcode::NOTE_ON, 0, 64);

        MidiTimeline timeline;
        // Emit out of timestamp order across t=200 and t=100 to exercise stable sort + per-bucket dedup together.
        emplace_checked(timeline.events, TimedMidiEvent{200, sustain_on_ch0});   // bucket 200: kept (fresh bucket)
        emplace_checked(timeline.events, TimedMidiEvent{100, note_on_a});        // bucket 100 #0: kept
        emplace_checked(timeline.events, TimedMidiEvent{100, sustain_on_ch0});   // bucket 100 #1: kept (first sustain ch0=127)
        emplace_checked(timeline.events, TimedMidiEvent{100, bend_center});      // bucket 100 #2: kept
        emplace_checked(timeline.events, TimedMidiEvent{100, sustain_on_ch0});   // bucket 100: duplicate sustain ch0=127 -> dropped
        emplace_checked(timeline.events, TimedMidiEvent{100, bend_center});      // bucket 100 #3: duplicate pitch bend -> KEPT
        emplace_checked(timeline.events, TimedMidiEvent{100, sustain_on_ch1});   // bucket 100 #4: different channel -> kept
        emplace_checked(timeline.events, TimedMidiEvent{100, sustain_off_ch0});  // bucket 100 #5: different value -> kept
        emplace_checked(timeline.events, TimedMidiEvent{100, sostenuto_on_ch0}); // bucket 100 #6: different controller -> kept
        emplace_checked(timeline.events, TimedMidiEvent{100, sostenuto_on_ch0}); // bucket 100: duplicate sostenuto -> dropped
        emplace_checked(timeline.events, TimedMidiEvent{100, note_on_b});        // bucket 100 #7: kept
        emplace_checked(timeline.events, TimedMidiEvent{100, expr_ch0_a});       // bucket 100 #8: late dynamic -> kept
        emplace_checked(timeline.events, TimedMidiEvent{100, expr_ch0_b});       // bucket 100 #9: duplicate late dynamic -> KEPT
        emplace_checked(timeline.events, TimedMidiEvent{200, sustain_on_ch0});   // bucket 200: duplicate in bucket 200 -> dropped

        ASSERT_TRUE(ExposedSequencer::bounded_stable_sort(timeline));
        ASSERT_TRUE(ExposedSequencer::dedup_timeline(timeline));

        const std::vector<TimedMidiEvent> expected{
            {100, note_on_a},      {100, sustain_on_ch0},  {100, bend_center},      {100, bend_center},
            {100, sustain_on_ch1}, {100, sustain_off_ch0}, {100, sostenuto_on_ch0}, {100, note_on_b},
            {100, expr_ch0_a},     {100, expr_ch0_b},      {200, sustain_on_ch0},
        };
        ASSERT_EQ(timeline.events.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(timeline.events[i].timestamp, expected[i].timestamp) << "index " << i;
            EXPECT_EQ(timeline.events[i].event, expected[i].event) << "index " << i;
        }
    }
}

TEST(AudioInternal, MeasurelessScorePreservesThreeSecondSilentTail) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto *master = engrave::DocumentAccess::master_score(document);
    master->delete_measures(master->first_measure(), master->last_measure());
    ASSERT_EQ(master->first_measure(), nullptr);
    engrave::AudioOptions options;
    options.sample_rate = 44100;
    std::vector<std::byte> bytes;
    const auto result =
        document.render_audio(options, [&](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
    ASSERT_TRUE(result);
    // RIFF chunks may include metadata; inspect the data chunk, not the header size.
    std::size_t offset = 12;
    const auto u32 = [&](std::size_t at) {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= std::to_integer<std::uint32_t>(bytes[at + i]) << (8 * i);
        return value;
    };
    while (offset + 8 <= bytes.size()) {
        const auto size = u32(offset + 4);
        if (std::memcmp(bytes.data() + offset, "data", 4) == 0) {
            ASSERT_EQ(size, 132300u * 2 * sizeof(float));
            ASSERT_LE(offset + 8 + size, bytes.size());
            EXPECT_TRUE(
                std::all_of(bytes.begin() + offset + 8, bytes.begin() + offset + 8 + size, [](std::byte b) { return b == std::byte{0}; }));
            return;
        }
        offset += 8 + size + (size & 1);
    }
    FAIL() << "missing PCM data";
}

TEST(AudioInternal, CancelsInsideFlatSequencerPreprocessing) {
    struct Exposed : muse::audio::synth::FluidSequencer {
        using FluidSequencer::main_stream_events_;
        using FluidSequencer::dynamic_layers_;
        using FluidSequencer::dynamic_points_;
    };

    for (int pass = 0; pass < 4; ++pass) {
        muse::mpe::TrackEvents events;
        if (pass < 2) {
            for (int i = 0; i < 10000; ++i) {
                muse::mpe::NoteRecord note;
                note.group_timestamp = pass == 0 ? i : 10000 - i;
                muse::mpe::emplace_checked(events.notes, note);
            }
        } else if (pass == 2) {
            events.dynamic_layers.resize(10000);
        } else {
            events.dynamic_points.resize(10000);
        }
        Exposed seq;
        seq.init({}, muse::midi::Program{0, 0}, true, muse::audio::synth::k_fluid_channel_count);
        seq.main_stream_events_.cursor = 5;
        int polls = 0;
        const auto cancel = [&] {
            EXPECT_EQ(muse::audio::OfflineCancellation::phase, muse::audio::OfflineCancellation::Phase::SEQUENCER);
            ++polls;
            // Dynamics are still being copied, and notes have not started MIDI lowering.
            if (pass == 2) EXPECT_EQ(seq.dynamic_layers_.size(), 4095u);
            if (pass == 3) EXPECT_EQ(seq.dynamic_points_.size(), 4095u);
            EXPECT_TRUE(seq.main_stream_events_.empty());
            return true;
        };
        {
            muse::audio::OfflineCancellation scope(cancel);
            seq.load(events);
        }
        EXPECT_EQ(polls, 1);
        EXPECT_TRUE(seq.main_stream_events_.empty());
        EXPECT_EQ(seq.main_stream_events_.cursor, 0u);
    }
}

// Uncancelled WAV of a corpus score rendered by the probe in a fresh process.
static std::optional<std::vector<std::byte>> fresh_process_wav(std::string_view score) {
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto path = std::filesystem::temp_directory_path() / ("engrave-fresh-reference-" + std::to_string(process) + ".wav");
    std::vector<std::string> arguments{"--assets=" + music::test::g_assets.string()};
    for (const auto &font : music::test::g_soundfonts) arguments.push_back("--soundfont=" + font.string());
    arguments.push_back("--audio=" + music::test::corpus_file(score).string());
    arguments.push_back("--format=wav");
    arguments.push_back("--out=" + path.string());
    if (music::test::run_probe(arguments) != 0) return std::nullopt;
    std::vector<std::byte> bytes;
    {
        std::ifstream in(path, std::ios::binary);
        const std::vector<char> raw{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        bytes.resize(raw.size());
        std::memcpy(bytes.data(), raw.data(), raw.size());
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return bytes;
}

// A re-executed death-test subprocess starts with a fresh engine, its own
// runtime cache directory and no rendered synths. Cancel at the first note-on
// FluidSynth receives (a live voice), recover twice on the same SF3 cache,
// compare against an uncancelled fresh-process render, and explicitly tear
// down the engine, checking the decoder bridge is detached.
TEST(AudioInternal, FirstRenderCancelThenRepeatedRenderAndTeardown) {
    // The default "fast" style forks on POSIX: the child would inherit the warm
    // parent engine and clear the parent's retained cache directory on teardown.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    ASSERT_EXIT(([] {
                    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
                    if (!opened) std::exit(11);
                    auto &document = **opened;
                    auto &audio = engrave::DocumentAccess::context(document).audio();
                    bool cancelled = false;
                    music::set_engine_audio_midi_event_hook(audio, [&](const muse::midi::Event &event, uint64_t) {
                        // Startup flushes send all-notes-off/controllers first; wait for a score note.
                        if (event.opcode() == muse::midi::Event::Opcode::NOTE_ON) cancelled = true;
                    });
                    engrave::AudioOptions options;
                    options.cancelled = [&] { return cancelled; };
                    int published = 0;
                    auto failed = document.render_audio(options, [&](auto) { ++published; });
                    if (failed || failed.error().code != engrave::ErrorCode::CANCELLED || published || !cancelled) std::exit(12);
                    music::set_engine_audio_midi_event_hook(audio, {});
                    auto render = [&] {
                        std::vector<std::byte> bytes;
                        if (!document.render_audio({}, [&](auto chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); }))
                            std::exit(13);
                        return bytes;
                    };
                    auto first = render();
                    if (first.empty() || render() != first) std::exit(14);
                    const auto reference = fresh_process_wav("ghost_note.mscx");
                    if (!reference) std::exit(17);
                    if (reference->empty() || *reference != first) std::exit(18);
                    opened->reset();
                    music::test::g_engine.reset();
                    if (muse::audio::fluid::FluidVorbisDecoder::decoder) std::exit(15);
                    std::exit(0);
                }()),
                ::testing::ExitedWithCode(0), "");
}

#ifdef ENGRAVE_PLAYBACK_HARNESS
TEST(AudioInternal, ReverbPartialSignalAllocationDestroysAndRecovers) {
    using muse::audio::fx::ReverbProcessor;

    struct Disarm {
        ~Disarm() { ReverbProcessor::fail_signal_allocation = -1; }
    } disarm;

    muse::audio::OutputSpec spec;
    spec.sample_rate = 44100;
    spec.audio_channel_count = 2;
    spec.samples_per_channel = 4096;
    for (int allocation = 0; allocation < 3; ++allocation) {
        ReverbProcessor::fail_signal_allocation = allocation;
        {
            ReverbProcessor effect({});
            EXPECT_THROW(effect.init(spec), std::bad_alloc);
        }
        ReverbProcessor effect({});
        EXPECT_THROW(effect.init(spec), std::bad_alloc);
        ReverbProcessor::fail_signal_allocation = -1;
        EXPECT_NO_THROW(effect.init(spec));
        // Exercise destruction of old buffers when a larger block is requested.
        spec.samples_per_channel *= 2;
        ReverbProcessor::fail_signal_allocation = allocation;
        EXPECT_THROW(effect.init(spec), std::bad_alloc);
        ReverbProcessor::fail_signal_allocation = -1;
        EXPECT_NO_THROW(effect.init(spec));
        // A resize that fails inside process() must not leave a retry of the same
        // block size dispatching through null signal buffers.
        const auto larger = spec.samples_per_channel * 2;
        std::vector<float> block(static_cast<size_t>(larger) * spec.audio_channel_count, 0.f);
        ReverbProcessor::fail_signal_allocation = allocation;
        EXPECT_THROW(effect.process(block.data(), larger, 0), std::bad_alloc);
        ReverbProcessor::fail_signal_allocation = -1;
        EXPECT_NO_THROW(effect.process(block.data(), larger, 0));
    }
}
#endif

namespace {
struct ProfileResourceFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
                                 ("engrave-profile-repair-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    muse::io::NativeFileSystem files{root / "assets"};
    muse::mpe::ArticulationProfilesRepository repository{files};

    ProfileResourceFixture() { std::filesystem::create_directories(root / "assets/mpe/resources"); }

    ~ProfileResourceFixture() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    void provide(const char *family) {
        std::ofstream file(root / "assets/mpe/resources" / (std::string("general_") + family + "_articulations_profile.json"));
        file << R"({"supportedFamilies":[],"patterns":{}})";
        ASSERT_TRUE(file.good());
    }

    muse::mpe::ArticulationProfileBundle bundle() {
        muse::mpe::ArticulationProfileBundle result;
        result.repository = &repository;
        return result;
    }
};
} // namespace

TEST(AudioInternal, UnusedProfileCanBeRepairedBeforeNextBundleLookup) {
    // Bundle/repository seam: no existing repository failure-injection seam exists.
    ProfileResourceFixture resources;
    resources.provide("keyboard");
    const auto first = resources.bundle();
    ASSERT_TRUE(first.default_profile(muse::mpe::ArticulationFamily::KEYBOARDS));
    // STRINGS was absent during the first score's profile lookups.
    resources.provide("strings");
    const auto second = resources.bundle();
    ASSERT_TRUE(second.default_profile(muse::mpe::ArticulationFamily::STRINGS));
    EXPECT_EQ(first.default_profile(muse::mpe::ArticulationFamily::STRINGS),
              second.default_profile(muse::mpe::ArticulationFamily::STRINGS));
}

TEST(AudioInternal, LookedUpProfileFailureRemainsCachedAfterRepair) {
    ProfileResourceFixture resources;
    const auto first = resources.bundle();
    EXPECT_FALSE(first.default_profile(muse::mpe::ArticulationFamily::STRINGS));
    resources.provide("strings");
    EXPECT_FALSE(first.default_profile(muse::mpe::ArticulationFamily::STRINGS));
    const auto second = resources.bundle();
    EXPECT_FALSE(second.default_profile(muse::mpe::ArticulationFamily::STRINGS));
    // The repaired file really is readable; only the default-family cache retains null.
    EXPECT_TRUE(resources.repository.load_profile(muse::io::PathT(":/mpe/resources/general_strings_articulations_profile.json")));
}

TEST(AudioInternal, ArticulationProfileJsonKeepsValidFieldsWhenSomeAreMalformed) {
    using namespace muse::mpe;
    ProfileResourceFixture resources;
    const auto path = resources.root / "assets/mpe/resources/general_strings_articulations_profile.json";
    const muse::io::PathT resource(":/mpe/resources/general_strings_articulations_profile.json");
    std::ofstream(path) << "{";
    EXPECT_FALSE(resources.repository.load_profile(resource));

    std::ofstream(path) << R"({
        "supportedFamilies":["StringsArticulation",false],
        "patterns":{"Staccato":[
            {"patternPosition":-1.5,"arrangementPattern":{"durationFactor":5123.5,"timestampOffset":"bad"},
             "pitchPattern":{"pitchOffsets":[{"offsetPosition":2.5,"offsetValue":-4.5},{"offsetPosition":3,"offsetValue":5}]},
             "expressionPattern":{"dynamicOffsets":false}},
            {"patternPosition":-2,"arrangementPattern":{"durationFactor":1}}
        ]}
    })";
    const auto profile = resources.repository.load_profile(resource);
    ASSERT_TRUE(profile);
    EXPECT_EQ(profile->supported_families, (std::vector{ArticulationFamily::STRINGS, ArticulationFamily::UNDEFINED}));
    const auto &pattern = profile->pattern(ArticulationType::STACCATO);
    ASSERT_EQ(pattern.size(), 1u);
    ASSERT_TRUE(pattern.contains(-2));
    const auto &segment = pattern.at(-2);
    EXPECT_EQ(segment.arrangement_pattern.duration_factor_value, 5124);
    EXPECT_EQ(segment.arrangement_pattern.timestamp_offset_value, 0);
    ASSERT_TRUE(segment.pitch_pattern.pitch_offset_map.contains(3));
    EXPECT_EQ(segment.pitch_pattern.pitch_offset_map.at(3), -5);
    EXPECT_TRUE(segment.expression_pattern.dynamic_offset_map.empty());
}

TEST(AudioInternal, ArticulationProfileSaveWritesSchemaAndReloads) {
    using namespace muse::mpe;
    ProfileResourceFixture resources;
    const auto path = resources.root / "saved.json";
    const muse::io::PathT output(path.string());
    ArticulationPatternSegment segment;
    segment.arrangement_pattern = {7000, -1200};
    segment.pitch_pattern.pitch_offset_map.emplace(0, -50);
    segment.pitch_pattern.pitch_offset_map.emplace(10000, 75);
    segment.expression_pattern.dynamic_offset_map.emplace(2500, -100);
    auto profile = resources.repository.create_new();
    profile->supported_families = {ArticulationFamily::STRINGS};
    profile->set_pattern(ArticulationType::STACCATO, {{1250, segment}});
    resources.repository.save_profile(output, profile);

    std::ifstream saved(path, std::ios::binary);
    ASSERT_TRUE(saved.is_open());
    const std::string actual{std::istreambuf_iterator<char>(saved), std::istreambuf_iterator<char>()};
    EXPECT_EQ(actual, R"({
  "patterns": {
    "Staccato": [
      {
        "arrangementPattern": {
          "durationFactor": 7000,
          "timestampOffset": -1200
        },
        "expressionPattern": {
          "dynamicOffsets": [
            {
              "offsetPosition": 2500,
              "offsetValue": -100
            }
          ]
        },
        "patternPosition": 1250,
        "pitchPattern": {
          "pitchOffsets": [
            {
              "offsetPosition": 0,
              "offsetValue": -50
            },
            {
              "offsetPosition": 10000,
              "offsetValue": 75
            }
          ]
        }
      }
    ]
  },
  "supportedFamilies": [
    "StringsArticulation"
  ]
}
)");

    const auto reloaded = resources.repository.load_profile(output);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->supported_families, (std::vector{ArticulationFamily::STRINGS}));
    const auto &pattern = reloaded->pattern(ArticulationType::STACCATO);
    ASSERT_EQ(pattern.size(), 1u);
    ASSERT_TRUE(pattern.contains(1250));
    EXPECT_EQ(pattern.at(1250), segment);
}

TEST(AudioInternal, AmplitudeValuePointHandlesIntMinWithoutOverflow) {
    using namespace muse::mpe;
    ExpressionCurve curve;
    curve.emplace(0, std::numeric_limits<dynamic_level_t>::min());
    curve.emplace(5000, std::numeric_limits<dynamic_level_t>::max());
    const auto [pos, level] = curve.amplitude_value_point();
    EXPECT_EQ(pos, 0);
    EXPECT_EQ(level, std::numeric_limits<dynamic_level_t>::min());
}

TEST(AudioInternal, EmitsIntMinExpressionOffsetFromProfile) {
    using namespace muse::mpe;
    ProfileResourceFixture resources;
    const auto path = resources.root / "assets/mpe/resources/general_strings_articulations_profile.json";
    std::ofstream(path) << R"({
        "patterns":{"Staccato":[{
            "patternPosition":0,
            "arrangementPattern":{"durationFactor":10000},
            "expressionPattern":{"dynamicOffsets":[{"offsetPosition":0,"offsetValue":-2147483648}]}
        }]}
    })";
    const auto profile = resources.repository.load_profile(muse::io::PathT(":/mpe/resources/general_strings_articulations_profile.json"));
    ASSERT_TRUE(profile);
    const auto &pattern = profile->pattern_ptr(ArticulationType::STACCATO);
    ASSERT_TRUE(pattern);

    constexpr duration_t duration = 100'000;
    ArticulationMap applied;
    applied.emplace(ArticulationType::STACCATO,
                    ArticulationAppliedData({ArticulationType::STACCATO, pattern, 0, duration}, 0, hundred_percent));
    applied.pre_calculate_average_data();

    for (const auto dynamic : {DynamicType::NATURAL, DynamicType::PPPPPPPPP}) {
        TrackEvents track;
        FreezeProgress progress;
        emit_note(track, 0, {0, duration, 0, 0, pitch_level(PitchClass::C, 4), dynamic_level_from_type(dynamic), 2.0}, applied, {},
                  progress);
        ASSERT_EQ(track.notes.size(), 1u);
        const auto curve = track.expression_curve(track.notes.front());
        ASSERT_EQ(curve.size(), 1u);
        EXPECT_EQ(curve.front().at, 0);
        EXPECT_EQ(curve.front().value, std::numeric_limits<dynamic_level_t>::min());
    }
}

TEST(AudioInternal, EmitsAverageOfCombinedArticulationExpressionOffsets) {
    using namespace muse::mpe;
    const auto make_pattern = [](dynamic_level_t offset) {
        ArticulationPatternSegment segment;
        segment.arrangement_pattern.duration_factor_value = hundred_percent;
        segment.expression_pattern.dynamic_offset_map.emplace(0, offset);
        return std::make_shared<const ArticulationPattern>(ArticulationPattern{{0, std::move(segment)}});
    };

    constexpr duration_t duration = 100'000;
    constexpr dynamic_level_t expected_average = -1073741825;
    ArticulationMap applied;
    applied.emplace(
        ArticulationType::STACCATO,
        ArticulationAppliedData({ArticulationType::STACCATO, make_pattern(std::numeric_limits<dynamic_level_t>::min()), 0, duration}, 0,
                                hundred_percent));
    applied.emplace(ArticulationType::MARCATO,
                    ArticulationAppliedData({ArticulationType::MARCATO, make_pattern(-2), 0, duration}, 0, hundred_percent));
    applied.update_occupied_range(ArticulationType::STACCATO, 0, hundred_percent / 2);

    EXPECT_EQ(applied.average_max_amplitude_level(), expected_average);
    EXPECT_EQ(applied.average_dynamic_offset_map().at(0), expected_average);
    TrackEvents track;
    FreezeProgress progress;
    emit_note(track, 0, {0, duration, 0, 0, pitch_level(PitchClass::C, 4), dynamic_level_from_type(DynamicType::NATURAL), 2.0}, applied, {},
              progress);
    ASSERT_EQ(track.notes.size(), 1u);
    const auto curve = track.expression_curve(track.notes.front());
    ASSERT_EQ(curve.size(), 11u);
    EXPECT_EQ(curve.front().at, 0);
    EXPECT_EQ(curve.front().value, expected_average);
    for (std::size_t i = 1; i < curve.size(); ++i) {
        EXPECT_EQ(curve[i].at, static_cast<duration_percentage_t>(i) * ten_percent);
        EXPECT_EQ(curve[i].value, 0);
    }
}

TEST(AudioInternal, DynamicAutomationLayersCompareByValue) {
    using namespace muse::mpe;
    auto make_layer = [](double out_val) {
        auto map = std::make_shared<DynamicAutomationMap>();
        map->emplace(0, AutomationPoint{.in_value = AutomationPoint::ArrivalFromPrevious{}, .out_value = muse::real_t(out_val)});
        return map;
    };

    DynamicAutomationLayers a{{0, make_layer(0.5)}};
    DynamicAutomationLayers b{{0, make_layer(0.5)}};
    DynamicAutomationLayers c{{0, make_layer(0.75)}};
    EXPECT_NE(a.at(0).get(), b.at(0).get());
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(AudioInternal, WorkerPoolRunsEachTaskOnceStopsAndRethrows) {
    using namespace std::chrono_literals;
    muse::audio::engine::WorkerPool pool;
    const auto owner = std::this_thread::get_id();
    std::vector<std::atomic<int>> runs(200);
    std::mutex mutex;
    muse::BTreeSet<std::thread::id> threads;
    const auto count_task = [&](std::size_t index) {
        runs[index].fetch_add(1);
        const std::scoped_lock lock(mutex);
        threads.insert(std::this_thread::get_id());
    };
    const auto never = [] { return false; };
    const auto check_counts = [&] {
        for (std::size_t i = 0; i < runs.size(); ++i) EXPECT_EQ(runs[i].exchange(0), 1) << i;
    };
    EXPECT_TRUE(pool.run(4, runs.size(), count_task, never));
    check_counts();
    EXPECT_FALSE(threads.contains(owner));
    EXPECT_LE(threads.size(), 4u);
    EXPECT_EQ(pool.started_threads(), 4u);

    // A task in flight runs to its own cancellation check; run() returns only after it does.
    std::atomic<int> started{0}, finished{0};
    const auto until_stopped = [&](std::size_t) {
        started.fetch_add(1);
        while (!pool.stopping()) std::this_thread::sleep_for(1ms);
        finished.fetch_add(1);
    };
    std::vector<std::chrono::steady_clock::time_point> polls;
    const auto stop_on_twentieth = [&] {
        EXPECT_EQ(std::this_thread::get_id(), owner);
        polls.push_back(std::chrono::steady_clock::now());
        return polls.size() == 20;
    };
    const auto begin = std::chrono::steady_clock::now();
    EXPECT_FALSE(pool.run(4, 100, until_stopped, stop_on_twentieth));
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    EXPECT_EQ(polls.size(), 20u);
    EXPECT_EQ(started.load(), finished.load());
    EXPECT_GE(started.load(), 1);
    EXPECT_LE(started.load(), 4);
    // Report observed poll cadence without asserting a tight wall-clock threshold
    // that can flake under load or when Windows falls back to scheduler-tick waits.
    std::vector<std::chrono::steady_clock::duration> gaps;
    for (std::size_t i = 1; i < polls.size(); ++i) gaps.push_back(polls[i] - polls[i - 1]);
    std::ranges::sort(gaps);
    std::printf("worker pool: 20 polls in %.3f ms, median gap %.3f ms, widest %.3f ms\n",
                std::chrono::duration<double, std::milli>(elapsed).count(),
                std::chrono::duration<double, std::milli>(gaps[gaps.size() / 2]).count(),
                std::chrono::duration<double, std::milli>(gaps.back()).count());

    const auto throw_fifth = [](std::size_t index) {
        if (index == 5) throw std::runtime_error("task");
    };
    EXPECT_THROW(pool.run(3, 50, throw_fifth, never), std::runtime_error);
    EXPECT_TRUE(pool.stopping());
    const auto throwing_poll = []() -> bool { throw std::logic_error("poll"); };
    EXPECT_THROW(pool.run(2, 10, until_stopped, throwing_poll), std::logic_error);
    EXPECT_EQ(started.load(), finished.load());

    threads.clear();
    EXPECT_TRUE(pool.run(4, runs.size(), count_task, never));
    check_counts();
    EXPECT_FALSE(pool.stopping());
    EXPECT_EQ(pool.started_threads(), 4u);

    // A started block runs while the coordinator is free; finish() completes it as run() would.
    ASSERT_TRUE(pool.start(4, runs.size(), count_task));
    EXPECT_TRUE(pool.in_flight());
    EXPECT_TRUE(pool.finish(never));
    EXPECT_FALSE(pool.in_flight());
    check_counts();
    // Starting a second block while one is in flight is rejected without disturbing the running block.
    std::latch entered{1}, release{1};
    std::atomic<int> started_gated{0}, completed{0};
    const auto gated_task = [&](std::size_t) {
        if (started_gated.fetch_add(1) == 0) entered.count_down();
        release.wait();
        completed.fetch_add(1);
    };
    ASSERT_TRUE(pool.start(2, 10, gated_task));
    entered.wait();
    EXPECT_THROW(pool.start(2, 10, count_task), std::logic_error);
    EXPECT_TRUE(pool.in_flight());
    EXPECT_FALSE(pool.stopping());
    release.count_down();
    EXPECT_TRUE(pool.finish(never));
    EXPECT_FALSE(pool.in_flight());
    EXPECT_EQ(completed.load(), 10);

    // abandon() stops a started block, returns once its running tasks have, and drops task errors.
    started = 0;
    finished = 0;
    std::latch abandon_entered{1};
    const auto until_stopped_notified = [&](std::size_t) {
        if (started.fetch_add(1) == 0) abandon_entered.count_down();
        while (!pool.stopping()) std::this_thread::sleep_for(1ms);
        finished.fetch_add(1);
    };
    ASSERT_TRUE(pool.start(4, 100, until_stopped_notified));
    abandon_entered.wait();
    pool.abandon();
    EXPECT_FALSE(pool.in_flight());
    EXPECT_EQ(started.load(), finished.load());
    EXPECT_LE(started.load(), 4);
    ASSERT_TRUE(pool.start(2, 10, throw_fifth));
    pool.abandon();

    // Cancellation and throwing poll callbacks take precedence over concurrent worker exceptions.
    // Wait until the worker error has set stopping() before calling finish() so the race is deterministic.
    const auto throw_first = [](std::size_t index) {
        if (index == 0) throw std::runtime_error("worker race");
    };
    const auto always_stop = [] { return true; };
    ASSERT_TRUE(pool.start(2, 10, throw_first));
    while (!pool.stopping()) std::this_thread::sleep_for(1ms);
    EXPECT_FALSE(pool.finish(always_stop));
    ASSERT_TRUE(pool.start(2, 10, throw_first));
    while (!pool.stopping()) std::this_thread::sleep_for(1ms);
    EXPECT_THROW(pool.finish(throwing_poll), std::logic_error);

    EXPECT_TRUE(pool.run(4, runs.size(), count_task, never));
    check_counts();
    EXPECT_EQ(pool.started_threads(), 4u);
}

namespace {
// Six parts on distinct programs, `chord` notes on every 16th at 300 BPM and a
// new dynamic each bar, so tracks differ in cost and timbre.
std::filesystem::path write_parallel_score(const std::string &stem, int chord, int measures) {
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto path = std::filesystem::temp_directory_path() / (stem + "-" + std::to_string(process) + ".musicxml");
    std::ofstream out(path);
    const int programs[] = {1, 41, 74, 57, 34, 49};
    const char *dynamics[][2] = {{"pp", "36"}, {"mf", "80"}, {"ff", "112"}, {"mp", "64"}};
    const std::string steps = "CDEFGAB";
    out << "<?xml version=\"1.0\"?><score-partwise version=\"4.0\"><part-list>";
    for (int p = 1; p <= 6; ++p)
        out << "<score-part id=\"P" << p << "\"><part-name>Part " << p << "</part-name><midi-instrument id=\"I" << p << "\"><midi-channel>"
            << p << "</midi-channel><midi-program>" << programs[p - 1] << "</midi-program></midi-instrument></score-part>";
    out << "</part-list>";
    for (int p = 1; p <= 6; ++p) {
        out << "<part id=\"P" << p << "\">";
        for (int m = 1; m <= measures; ++m) {
            out << "<measure number=\"" << m << "\">";
            if (m == 1)
                out << "<attributes><divisions>480</divisions><time><beats>4</beats><beat-type>4</beat-type></time>"
                       "<clef><sign>G</sign><line>2</line></clef></attributes><direction><direction-type><metronome>"
                       "<beat-unit>quarter</beat-unit><per-minute>300</per-minute></metronome></direction-type>"
                       "<sound tempo=\"300\"/></direction>";
            const auto &dynamic = dynamics[(m + p) % 4];
            out << "<direction><direction-type><dynamics><" << dynamic[0] << "/></dynamics></direction-type><sound dynamics=\""
                << dynamic[1] << "\"/></direction>";
            for (int s = 0; s < 16; ++s) {
                for (int c = 0; c < chord; ++c) {
                    const int degree = s + 2 * c;
                    out << "<note>" << (c ? "<chord/>" : "") << "<pitch><step>" << steps[degree % 7] << "</step><octave>"
                        << std::min(3 + p % 3 + degree / 7, 7) << "</octave></pitch><duration>120</duration><type>16th</type></note>";
                }
            }
            out << "</measure>";
        }
        out << "</part>";
    }
    out << "</score-partwise>";
    return path;
}

std::vector<std::byte> render_bytes(engrave::Document &document, engrave::AudioFormat format) {
    std::vector<std::byte> bytes;
    engrave::AudioOptions options;
    options.format = format;
    EXPECT_TRUE(document.render_audio(options, [&](auto chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); }));
    return bytes;
}
} // namespace

TEST(AudioInternal, ParallelSynthesisIsBitIdenticalAcrossThreadCounts) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    const auto xml = write_parallel_score("engrave-parallel-identity", 3, 4);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};
    for (const auto format : {engrave::AudioFormat::WAV, engrave::AudioFormat::OPUS}) {
        set_audio_threads(document, 1);
        const auto serial = render_bytes(document, format);
        ASSERT_FALSE(serial.empty());
        for (const int threads : {2, 3, 4}) {
            set_audio_threads(document, threads);
            EXPECT_TRUE(render_bytes(document, format) == serial) << "threads=" << threads << " format=" << static_cast<int>(format);
        }
    }
    // Four threads did start, so the comparisons above covered the fan-out.
    EXPECT_GE(music::engine_audio_worker_threads(engrave::DocumentAccess::context(document).audio()), 4u);
    set_audio_threads(document, 4);
    const auto reference = render_bytes(document, engrave::AudioFormat::WAV);
    for (int repeat = 0; repeat < 50; ++repeat) ASSERT_TRUE(render_bytes(document, engrave::AudioFormat::WAV) == reference) << repeat;
}

TEST(AudioInternal, ParallelSynthesisCancellationUnwindsAndReusesWorkers) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    using Phase = muse::audio::OfflineCancellation::Phase;
    const auto owner = std::this_thread::get_id();
    // Dense chords keep each block busy for longer than the coordinator's poll interval.
    const auto xml = write_parallel_score("engrave-parallel-cancel", 6, 2);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};
    set_audio_threads(document, 4);
    const auto baseline = render_bytes(document, engrave::AudioFormat::WAV);
    ASSERT_FALSE(baseline.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    const auto workers = music::engine_audio_worker_threads(audio);
    ASSERT_GE(workers, 4u);
    const auto path = std::filesystem::temp_directory_path() / "engrave-parallel-cancel.wav";
    for (const bool throws : {false, true}) {
        int synthesis_polls = 0;
        bool fired = false;
        music::AudioRenderRequest request;
        const auto utf8 = path.generic_u8string();
        request.output_path.assign(utf8.begin(), utf8.end());
        request.cancelled = [&]() -> bool {
            EXPECT_EQ(std::this_thread::get_id(), owner);
            if (muse::audio::OfflineCancellation::phase != Phase::SYNTHESIS || ++synthesis_polls < 3) return false;
            fired = true;
            if (throws) throw std::runtime_error("synthesis cancellation callback");
            return true;
        };
        if (throws)
            EXPECT_THROW(music::export_score_audio(audio, master, request), std::runtime_error);
        else
            EXPECT_EQ(music::export_score_audio(audio, master, request).failure, music::AudioRenderResult::Failure::CANCELLED);
        EXPECT_TRUE(fired) << "throws=" << throws;
        EXPECT_FALSE(std::filesystem::exists(path)) << "throws=" << throws;
        EXPECT_EQ(music::engine_audio_worker_threads(audio), workers) << "throws=" << throws;
        EXPECT_TRUE(render_bytes(document, engrave::AudioFormat::WAV) == baseline) << "throws=" << throws;
    }
}

TEST(AudioInternal, MidiHookKeepsSynthesisSerialAndOrdered) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    const auto owner = std::this_thread::get_id();
    const auto xml = write_parallel_score("engrave-parallel-hook", 2, 2);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    const auto path = std::filesystem::temp_directory_path() / "engrave-parallel-hook.wav";
    using Captured = std::tuple<std::uint32_t, std::uint32_t, std::uint64_t>;
    const auto capture = [&](int threads) {
        set_audio_threads(document, threads);
        std::vector<Captured> events;
        music::set_engine_audio_midi_event_hook(audio, [&](const muse::midi::Event &event, std::uint64_t frame) {
            EXPECT_EQ(std::this_thread::get_id(), owner);
            const auto *words = event.midi20_words();
            events.emplace_back(words[0], words[1], frame);
        });
        music::AudioRenderRequest request;
        const auto utf8 = path.generic_u8string();
        request.output_path.assign(utf8.begin(), utf8.end());
        const auto result = music::export_score_audio(audio, master, request);
        music::set_engine_audio_midi_event_hook(audio, {});
        EXPECT_TRUE(result.ok) << result.error;
        std::ifstream in(path, std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        in.close();
        std::filesystem::remove(path);
        return std::pair{events, bytes};
    };
    const auto serial = capture(1);
    ASSERT_FALSE(serial.first.empty());
    const auto configured = capture(4);
    EXPECT_TRUE(configured.first == serial.first);
    EXPECT_TRUE(configured.second == serial.second);
}

TEST(AudioInternal, ParallelSynthesisMatchesSerialAcrossRoundingModesAndSrandSeeds) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";

    struct RestoreRounding {
        int saved = std::fegetround();

        ~RestoreRounding() {
            if (saved >= 0) std::fesetround(saved);
        }
    } restore_rounding;

    const auto xml = write_parallel_score("engrave-parallel-fp-mode", 6, 2);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};

    // Prime the persistent worker pool under FE_TONEAREST, then verify both
    // FE_TOWARDZERO and returning to FE_TONEAREST match serial output bit-for-bit.
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
    set_audio_threads(document, 4);
    ASSERT_FALSE(render_bytes(document, engrave::AudioFormat::WAV).empty());
    ASSERT_GE(music::engine_audio_worker_threads(engrave::DocumentAccess::context(document).audio()), 4u);

    for (const int mode : {FE_TOWARDZERO, FE_TONEAREST}) {
        ASSERT_EQ(std::fesetround(mode), 0);
        set_audio_threads(document, 1);
        const auto serial = render_bytes(document, engrave::AudioFormat::WAV);
        ASSERT_FALSE(serial.empty());
        set_audio_threads(document, 4);
        EXPECT_TRUE(render_bytes(document, engrave::AudioFormat::WAV) == serial) << "mode=" << mode;
    }

    // Mutating rounding mode inside the cancellation callback during synthesis
    // must keep serial and parallel output bit-identical while preserving the
    // callback's rounding change on return.
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    const auto path = std::filesystem::temp_directory_path() / "engrave-parallel-mid-round.wav";
    const auto render_with_mid_synthesis_flip = [&](int threads) {
        EXPECT_EQ(std::fesetround(FE_TONEAREST), 0);
        set_audio_threads(document, threads);
        bool flipped = false;
        music::AudioRenderRequest request;
        const auto utf8 = path.generic_u8string();
        request.output_path.assign(utf8.begin(), utf8.end());
        request.cancelled = [&]() -> bool {
            if (!flipped && muse::audio::OfflineCancellation::phase == muse::audio::OfflineCancellation::Phase::SYNTHESIS) {
                flipped = true;
                EXPECT_EQ(std::fesetround(FE_TOWARDZERO), 0);
            } else if (flipped) {
                EXPECT_EQ(std::fegetround(), FE_TOWARDZERO);
            }
            return false;
        };
        const auto res = music::export_score_audio(audio, master, request);
        EXPECT_TRUE(res.ok) << res.error;
        EXPECT_TRUE(flipped) << "threads=" << threads;
        EXPECT_EQ(std::fegetround(), FE_TOWARDZERO) << "threads=" << threads;
        std::ifstream in(path, std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        in.close();
        std::filesystem::remove(path);
        return bytes;
    };
    const auto mid_serial = render_with_mid_synthesis_flip(1);
    ASSERT_FALSE(mid_serial.empty());
    const auto mid_parallel = render_with_mid_synthesis_flip(4);
    EXPECT_TRUE(mid_serial == mid_parallel);
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);

    // Removing std::srand(0) must leave Opus encoding independent of process rand() state.
    set_audio_threads(document, 4);
    std::srand(1u);
    const auto opus_a = render_bytes(document, engrave::AudioFormat::OPUS);
    ASSERT_FALSE(opus_a.empty());
    std::srand(0x1234567u);
    const auto opus_b = render_bytes(document, engrave::AudioFormat::OPUS);
    EXPECT_TRUE(opus_a == opus_b);
}

TEST(AudioInternal, SingleInstrumentWithMutedMetronomeStartsNoWorkers) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    if (music::test::g_soundfonts.empty()) GTEST_SKIP() << "no SoundFont configured";
    // Isolate a fresh Engine so an earlier parallel test's live worker pool cannot mask thread starts.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    ASSERT_EXIT(
        {
            music::test::g_engine.reset();
            const auto check = [] {
                engrave::EngineOptions options;
                options.resources = music::test::g_assets;
                options.soundfonts = {music::test::g_soundfonts.front()};
                options.audio_threads = 4;
                auto engine = engrave::Engine::create(options);
                if (!engine) return 1;

                constexpr std::string_view one_part = R"(<?xml version="1.0"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
  <part id="P1">
    <measure number="1">
      <attributes><divisions>1</divisions><time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes>
      <note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
    </measure>
  </part>
</score-partwise>)";
                engrave::OpenOptions open_options;
                open_options.format = engrave::Format::MUSIC_XML;
                auto opened = (*engine)->open(std::as_bytes(std::span(one_part.data(), one_part.size())), open_options);
                if (!opened) return 2;
                auto &document = **opened;
                std::size_t total_bytes = 0;
                engrave::AudioOptions audio_options;
                audio_options.format = engrave::AudioFormat::WAV;
                if (!document.render_audio(audio_options, [&](auto chunk) { total_bytes += chunk.size(); })) return 3;
                if (total_bytes == 0) return 4;
                return music::engine_audio_worker_threads(engrave::DocumentAccess::context(document).audio()) == 0u ? 0 : 5;
            };
            std::exit(check());
        },
        ::testing::ExitedWithCode(0), "");
}

namespace {
// Records which threads drive one encoder and injects a failure at its third block.
struct EncoderRecord {
    enum class Mode { OK, BEGIN_FAILS, ENCODE_FAILS, END_FAILS, ENCODE_THROWS };
    Mode mode = Mode::OK;
    bool end_throws = false;
    bool throw_on_short_write = false;
    // Encoder-thread writes are read only after export_score_audio has joined that thread.
    muse::BTreeSet<std::thread::id> threads;
    int begun = 0, encoded = 0, ended = 0;
    // Read by the caller's cancellation callback while the encoder runs.
    std::atomic<int> blocks{0};
    bool hold_encode = false;
    std::mutex gate_mutex;
    std::condition_variable gate_cv;
    bool released = false;

    void wait_if_held() {
        if (!hold_encode) return;
        std::unique_lock lock(gate_mutex);
        gate_cv.wait(lock, [this] { return released; });
    }

    void release() {
        {
            std::lock_guard lock(gate_mutex);
            released = true;
        }
        gate_cv.notify_all();
    }
};

struct RecordingEncoder final {
    RecordingEncoder(const muse::audio::SoundTrackFormat &, muse::io::IODevice &device, EncoderRecord &record)
        : device_(device), record_(record) {
        note();
    }

    bool begin(muse::audio::samples_t) {
        note();
        ++record_.begun;
        return record_.mode != EncoderRecord::Mode::BEGIN_FAILS;
    }

    size_t encode(muse::audio::samples_t samples, const float *input) {
        note();
        ++record_.encoded;
        record_.wait_if_held();
        if (record_.encoded == 3 && record_.mode == EncoderRecord::Mode::ENCODE_FAILS) return 0;
        if (record_.encoded == 3 && record_.mode == EncoderRecord::Mode::ENCODE_THROWS) throw std::runtime_error("encoder threw");
        const auto bytes = static_cast<size_t>(samples) * 2 * sizeof(float);
        if (device_.write(reinterpret_cast<const std::uint8_t *>(input), bytes) != bytes) {
            if (record_.throw_on_short_write) throw std::runtime_error("short write");
            return 0;
        }
        record_.blocks.fetch_add(1);
        return samples;
    }

    muse::Ret end() {
        note();
        ++record_.ended;
        if (record_.end_throws) throw std::logic_error("end threw");
        if (record_.mode == EncoderRecord::Mode::END_FAILS)
            return muse::audio::make_ret(muse::audio::Err::ERROR_ENCODE, "injected end failure");
        return muse::make_ok();
    }
private:
    void note() { record_.threads.insert(std::this_thread::get_id()); }

    muse::io::IODevice &device_;
    EncoderRecord &record_;
};

std::filesystem::path temp_output(const std::string &stem, const char *extension) {
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    return std::filesystem::temp_directory_path() / (stem + "-" + std::to_string(process) + extension);
}

music::AudioRenderRequest request_for(const std::filesystem::path &path) {
    music::AudioRenderRequest request;
    const auto utf8 = path.generic_u8string();
    request.output_path.assign(utf8.begin(), utf8.end());
    return request;
}

std::vector<char> read_file(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::byte> render_bytes(engrave::Document &document, engrave::AudioFormat format, int bit_depth) {
    std::vector<std::byte> bytes;
    engrave::AudioOptions options;
    options.format = format;
    options.bit_depth = bit_depth;
    EXPECT_TRUE(document.render_audio(options, [&](auto chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); }));
    return bytes;
}
} // namespace

// The encoder thread and the overlapped mix must not change a byte of any container.
TEST(AudioInternal, AsyncEncodeAndOverlapMatchSerialForEveryFormat) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    const auto xml = write_parallel_score("engrave-async-formats", 3, 6);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};

    const struct {
        engrave::AudioFormat format;
        int bit_depth;
    } cases[] = {{engrave::AudioFormat::WAV, 16},  {engrave::AudioFormat::WAV, 24},  {engrave::AudioFormat::WAV, 32},
                 {engrave::AudioFormat::FLAC, 16}, {engrave::AudioFormat::FLAC, 24}, {engrave::AudioFormat::OPUS, 0},
                 {engrave::AudioFormat::MP3, 0}};

    for (const auto &c : cases) {
        set_audio_threads(document, 1);
        const auto serial = render_bytes(document, c.format, c.bit_depth);
        ASSERT_FALSE(serial.empty());
        set_audio_threads(document, 4);
        EXPECT_TRUE(render_bytes(document, c.format, c.bit_depth) == serial)
            << "format=" << static_cast<int>(c.format) << " bit_depth=" << c.bit_depth;
    }
    EXPECT_GE(music::engine_audio_worker_threads(engrave::DocumentAccess::context(document).audio()), 4u);
}

// Integer WAV samples are the float render converted by the PCM rule
// round(clamp(x, -1, 1) * (2^(bits-1) - 1)), little-endian, after a 46-byte header.
TEST(AudioInternal, IntegerWavSamplesAreTheConvertedFloatRender) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    constexpr std::size_t header = 46;
    const auto float_wav = render_bytes(document, engrave::AudioFormat::WAV, 32);
    ASSERT_GT(float_wav.size(), header);
    const std::size_t samples = (float_wav.size() - header) / 4;
    for (const int bits : {16, 24}) {
        const auto pcm = render_bytes(document, engrave::AudioFormat::WAV, bits);
        const std::size_t width = static_cast<std::size_t>(bits / 8);
        ASSERT_EQ(pcm.size(), header + samples * width) << bits;
        const float scale = static_cast<float>((1 << (bits - 1)) - 1);
        std::size_t mismatches = 0;
        for (std::size_t i = 0; i < samples; ++i) {
            float value;
            std::memcpy(&value, float_wav.data() + header + i * 4, 4);
            const auto expected = static_cast<std::int32_t>(std::round(std::clamp(value, -1.0f, 1.0f) * scale));
            std::uint32_t actual = 0;
            for (std::size_t b = 0; b < width; ++b) actual |= std::to_integer<std::uint32_t>(pcm[header + i * width + b]) << (8 * b);
            const std::uint32_t mask = (1u << bits) - 1;
            if (actual != (static_cast<std::uint32_t>(expected) & mask)) ++mismatches;
        }
        EXPECT_EQ(mismatches, 0u) << bits << "-bit samples differ from the converted float render";
    }
}

TEST(AudioInternal, AsyncEncoderStaysOnOneThreadAndReportsFailures) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    using Mode = EncoderRecord::Mode;
    const auto owner = std::this_thread::get_id();
    const auto xml = write_parallel_score("engrave-async-failures", 2, 4);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    const auto path = temp_output("engrave-async-failures", ".wav");
    set_audio_threads(document, 4);
    const auto baseline = render_bytes(document, engrave::AudioFormat::WAV, 0);
    for (const int threads : {1, 4}) {
        set_audio_threads(document, threads);
        for (const Mode mode : {Mode::OK, Mode::BEGIN_FAILS, Mode::ENCODE_FAILS, Mode::END_FAILS, Mode::ENCODE_THROWS}) {
            SCOPED_TRACE(testing::Message() << "threads=" << threads << " mode=" << static_cast<int>(mode));
            EncoderRecord record;
            record.mode = mode;
            auto request = request_for(path);
            request.encoder_factory = [&record](const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device) {
                return std::make_unique<RecordingEncoder>(format, device, record);
            };
            int polls = 0;
            request.cancelled = [&] {
                EXPECT_EQ(std::this_thread::get_id(), owner);
                ++polls;
                return false;
            };
            if (mode == Mode::ENCODE_THROWS) {
                EXPECT_THROW(music::export_score_audio(audio, master, request), std::runtime_error);
            } else {
                const auto result = music::export_score_audio(audio, master, request);
                EXPECT_EQ(result.ok, mode == Mode::OK) << result.error;
                if (mode != Mode::OK) EXPECT_EQ(result.failure, music::AudioRenderResult::Failure::ENGINE_FAILURE);
                EXPECT_EQ(result.error.find("injected end failure") != std::string::npos, mode == Mode::END_FAILS) << result.error;
            }
            ASSERT_EQ(record.threads.size(), 1u) << "the encoder migrated between threads";
            EXPECT_EQ(*record.threads.begin() == owner, threads == 1);
            EXPECT_GT(polls, 0);
            EXPECT_EQ(record.begun, 1);
            if (mode == Mode::BEGIN_FAILS) EXPECT_EQ(record.encoded, 0);
            if (mode == Mode::ENCODE_FAILS || mode == Mode::ENCODE_THROWS) EXPECT_EQ(record.encoded, 3);
            EXPECT_EQ(record.ended, mode == Mode::BEGIN_FAILS ? 0 : 1);
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
        // A throwing finalizer must not terminate or mask a prior encode failure/exception.
        for (const Mode mode : {Mode::ENCODE_FAILS, Mode::ENCODE_THROWS}) {
            SCOPED_TRACE(testing::Message() << "throwing end threads=" << threads << " mode=" << static_cast<int>(mode));
            EncoderRecord record;
            record.mode = mode;
            record.end_throws = true;
            auto request = request_for(path);
            request.encoder_factory = [&record](const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device) {
                return std::make_unique<RecordingEncoder>(format, device, record);
            };
            if (mode == Mode::ENCODE_THROWS) {
                EXPECT_THROW(music::export_score_audio(audio, master, request), std::runtime_error);
            } else {
                const auto result = music::export_score_audio(audio, master, request);
                EXPECT_FALSE(result.ok);
                EXPECT_EQ(result.failure, music::AudioRenderResult::Failure::ENGINE_FAILURE);
            }
            EXPECT_EQ(record.ended, 1);
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
        // An encoder exception after an over-budget write still reports RESOURCE_LIMIT and deletes the file.
        {
            EncoderRecord record;
            record.throw_on_short_write = true;
            auto request = request_for(path);
            request.max_bytes = 1024;
            request.encoder_factory = [&record](const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device) {
                return std::make_unique<RecordingEncoder>(format, device, record);
            };
            const auto result = music::export_score_audio(audio, master, request);
            EXPECT_FALSE(result.ok);
            EXPECT_EQ(result.failure, music::AudioRenderResult::Failure::RESOURCE_LIMIT) << result.error;
            EXPECT_FALSE(std::filesystem::exists(path));
        }
        // A factory that returns no encoder fails cleanly.
        auto request = request_for(path);
        request.encoder_factory = [](const muse::audio::SoundTrackFormat &, muse::io::IODevice &) {
            return std::unique_ptr<RecordingEncoder>{};
        };
        const auto result = music::export_score_audio(audio, master, request);
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.failure, music::AudioRenderResult::Failure::ENGINE_FAILURE);
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    set_audio_threads(document, 4);
    EXPECT_TRUE(render_bytes(document, engrave::AudioFormat::WAV, 0) == baseline);
}

TEST(AudioInternal, AsyncEncodeCancellationDrainsAndRecovers) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    using Phase = muse::audio::OfflineCancellation::Phase;
    const auto owner = std::this_thread::get_id();
    const auto xml = write_parallel_score("engrave-async-cancel", 6, 3);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};
    set_audio_threads(document, 4);
    const auto baseline = render_bytes(document, engrave::AudioFormat::WAV, 0);
    ASSERT_FALSE(baseline.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    const auto workers = music::engine_audio_worker_threads(audio);
    ASSERT_GE(workers, 4u);
    const auto path = temp_output("engrave-async-cancel", ".wav");
    // 0: waiting on a slow encoder; 1: the same, the callback throws; 2: waiting on the
    // prefetched block's synthesis after the encoder has taken some blocks.
    for (int scenario = 0; scenario < 3; ++scenario) {
        SCOPED_TRACE(scenario);
        EncoderRecord record;
        if (scenario < 2) record.hold_encode = true;

        struct ReleaseOnExit {
            EncoderRecord &record;

            ~ReleaseOnExit() { record.release(); }
        } release_guard{record};

        auto request = request_for(path);
        request.encoder_factory = [&record](const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device) {
            return std::make_unique<RecordingEncoder>(format, device, record);
        };
        int phase_polls = 0;
        bool fired = false;
        request.cancelled = [&]() -> bool {
            EXPECT_EQ(std::this_thread::get_id(), owner);
            const auto phase = muse::audio::OfflineCancellation::phase;
            const bool armed = scenario < 2 ? phase == Phase::ENCODING : phase == Phase::SYNTHESIS && record.blocks.load() >= 3;
            if (fired || !armed || ++phase_polls < 3) return fired;
            fired = true;
            record.release();
            if (scenario == 1) throw std::runtime_error("encode-wait cancellation callback");
            return true;
        };
        if (scenario == 1)
            EXPECT_THROW(music::export_score_audio(audio, master, request), std::runtime_error);
        else
            EXPECT_EQ(music::export_score_audio(audio, master, request).failure, music::AudioRenderResult::Failure::CANCELLED);
        EXPECT_TRUE(fired);
        EXPECT_FALSE(std::filesystem::exists(path));
        ASSERT_EQ(record.threads.size(), 1u);
        EXPECT_NE(*record.threads.begin(), owner);
        EXPECT_EQ(record.ended, 0) << "a cancelled stream was ended";
        EXPECT_EQ(music::engine_audio_worker_threads(audio), workers);
        EXPECT_TRUE(render_bytes(document, engrave::AudioFormat::WAV, 0) == baseline);
    }
}

// The byte limits hold for writes from the encoder thread, including the header rewrites in end().
TEST(AudioInternal, AsyncEncodeEnforcesByteLimitsIncludingHeaderRewrites) {
    if (std::getenv("ENGRAVE_AUDIO_THREADS")) GTEST_SKIP() << "ENGRAVE_AUDIO_THREADS overrides the thread counts under test";
    const auto xml = write_parallel_score("engrave-async-limits", 2, 4);
    auto opened = music::test::g_engine->open(xml);
    std::filesystem::remove(xml);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const AutomaticAudioThreads restore{document};
    auto &audio = engrave::DocumentAccess::context(document).audio();
    auto *master = engrave::DocumentAccess::master_score(document);
    for (const int threads : {1, 4}) {
        set_audio_threads(document, threads);
        for (const auto format : {music::EngineAudioFormat::WAV, music::EngineAudioFormat::FLAC}) {
            SCOPED_TRACE(testing::Message() << "threads=" << threads << " format=" << static_cast<int>(format));
            const auto path = temp_output("engrave-async-limits", format == music::EngineAudioFormat::WAV ? ".wav" : ".flac");
            auto unlimited = request_for(path);
            unlimited.format = format;
            const auto full = music::export_score_audio(audio, master, unlimited);
            ASSERT_TRUE(full.ok) << full.error;
            const auto expected = read_file(path);
            const std::uint64_t size = expected.size();
            ASSERT_GT(size, 4096u);
            const auto expect_limit = [&](std::optional<std::uint64_t> max_bytes, std::optional<std::uint64_t> max_write_bytes) {
                auto request = request_for(path);
                request.format = format;
                request.max_bytes = max_bytes;
                request.max_write_bytes = max_write_bytes;
                const auto result = music::export_score_audio(audio, master, request);
                EXPECT_FALSE(result.ok);
                EXPECT_EQ(result.failure, music::AudioRenderResult::Failure::RESOURCE_LIMIT) << result.error;
                EXPECT_FALSE(std::filesystem::exists(path));
            };
            expect_limit(size / 2, std::nullopt);
            expect_limit(std::nullopt, size / 2);
            // Every byte of the file fits, but its header rewrite does not.
            expect_limit(std::nullopt, size);
            auto roomy = request_for(path);
            roomy.format = format;
            roomy.max_bytes = size;
            roomy.max_write_bytes = size + 4096;
            const auto fits = music::export_score_audio(audio, master, roomy);
            EXPECT_TRUE(fits.ok) << fits.error;
            EXPECT_TRUE(read_file(path) == expected);
            std::filesystem::remove(path);
        }
    }
}

namespace {
using namespace std::chrono_literals;

std::vector<char> render_chars(engrave::Document &document, const engrave::AudioOptions &options) {
    std::vector<char> bytes;
    const auto result = document.render_audio(options, [&](std::span<const std::byte> chunk) {
        const auto *data = reinterpret_cast<const char *>(chunk.data());
        bytes.insert(bytes.end(), data, data + chunk.size());
    });
    EXPECT_TRUE(result) << (result ? std::string() : result.error().message);
    return bytes;
}

// False after a minute, so a broken background render fails the test instead of hanging it.
bool wait_for(const std::atomic<bool> &flag) {
    const auto deadline = std::chrono::steady_clock::now() + 60s;
    while (!flag.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

struct RemoveFile {
    std::filesystem::path path;

    ~RemoveFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

const char *extension_of(engrave::AudioFormat format) {
    switch (format) {
        case engrave::AudioFormat::WAV: return ".wav";
        case engrave::AudioFormat::FLAC: return ".flac";
        case engrave::AudioFormat::OPUS: return ".ogg";
        case engrave::AudioFormat::MP3: return ".mp3";
    }
    return ".audio";
}

using RunResult = std::optional<engrave::Result<engrave::AudioRenderResult>>;

std::string run_error(const RunResult &result) {
    if (!result) return "the run did not return";
    return *result ? std::string() : (*result).error().message;
}
} // namespace

TEST(AudioInternal, PreparedAudioRunOnAnotherThreadMatchesRenderAudio) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    for (const auto format :
         {engrave::AudioFormat::WAV, engrave::AudioFormat::FLAC, engrave::AudioFormat::OPUS, engrave::AudioFormat::MP3}) {
        SCOPED_TRACE(engrave::to_string(format));
        engrave::AudioOptions options;
        options.format = format;
        const auto baseline = render_chars(document, options);
        ASSERT_FALSE(baseline.empty());
        const RemoveFile output{temp_output("engrave-prepared-thread", extension_of(format))};
        auto prepared = document.prepare_audio(options);
        ASSERT_TRUE(prepared) << prepared.error().message;
        EXPECT_EQ(prepared->source_revision(), document.revision());
        RunResult result;
        std::thread([&] { result = prepared->run(output.path); }).join();
        ASSERT_TRUE(result && *result) << run_error(result);
        const auto written = read_file(output.path);
        EXPECT_TRUE(written == baseline);
        EXPECT_EQ((*result)->bytes, written.size());
        EXPECT_GT((*result)->frames, 0u);
        EXPECT_GT((*result)->seconds, 3.0);
    }
}

// The owner queries, edits, draws, opens another score and finally destroys the
// document the render came from: first while the run thread synthesizes, then while
// that thread is held at a poll mid-render (the encoder thread keeps draining).
TEST(AudioInternal, PreparedAudioOutlivesEditsAndItsDocument) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("repeat70.mscx"));
    ASSERT_TRUE(opened);
    const auto baseline = render_chars(**opened, {});
    ASSERT_GT(baseline.size(), 1u << 20);
    const RemoveFile output{temp_output("engrave-prepared-edit", ".wav")};
    auto prepared = (*opened)->prepare_audio({});
    ASSERT_TRUE(prepared);
    const std::uint64_t revision = (*opened)->revision();
    EXPECT_EQ(prepared->source_revision(), revision);

    std::atomic<bool> started{false}, paused{false}, resume{false}, done{false};
    std::atomic<int> foreign_polls{0};
    RunResult result;
    std::uint64_t source_revision = 0;
    std::thread runner([&, held = std::move(*prepared)]() mutable {
        const auto self = std::this_thread::get_id();
        int polls = 0;
        result = held.run(output.path, [&] {
            if (std::this_thread::get_id() != self) ++foreign_polls;
            if (++polls == 1) started.store(true, std::memory_order_release);
            if (polls == 40) {
                paused.store(true, std::memory_order_release);
                wait_for(resume);
            }
            return false;
        });
        source_revision = held.source_revision();
        // Destroyed off the owner thread: the graph is gone before the engine is free again.
        { const auto finished = std::move(held); }
        done.store(true, std::memory_order_release);
    });

    const auto owner_work = [&] {
        ASSERT_TRUE(wait_for(started));
        const auto notes = (*opened)->query(engrave::Query{.types = {"Note"}, .limit = 1});
        ASSERT_TRUE(notes);
        ASSERT_FALSE(notes->elements.empty());
        const auto edited = (*opened)->edit(engrave::Batch{{engrave::SetPitch{notes->elements.front().id, 85, std::nullopt}}});
        ASSERT_TRUE(edited) << edited.error().message;
        EXPECT_GT((*opened)->revision(), revision);
        EXPECT_FALSE(done.load()) << "the render finished before the owner's edits";
        ASSERT_TRUE(wait_for(paused));
        engrave::RenderOptions png;
        png.format = engrave::RenderFormat::PNG;
        png.last_page = 0;
        EXPECT_TRUE((*opened)->render(png));
        auto other = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
        EXPECT_TRUE(other);
    };
    owner_work();
    opened->reset();
    resume.store(true, std::memory_order_release);
    runner.join();

    ASSERT_TRUE(result && *result) << run_error(result);
    EXPECT_EQ(source_revision, revision);
    EXPECT_EQ(foreign_polls.load(), 0);
    EXPECT_TRUE(read_file(output.path) == baseline);
    auto reopened = music::test::g_engine->open(music::test::corpus_file("repeat70.mscx"));
    ASSERT_TRUE(reopened);
    EXPECT_TRUE(render_chars(**reopened, {}) == baseline);
}

TEST(AudioInternal, SetSoundFontsDuringBackgroundRun) {
    using namespace music::test;
    auto opened = g_engine->open(corpus_file("repeat70.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto baseline = render_chars(document, {});
    ASSERT_FALSE(baseline.empty());
    const std::vector<std::filesystem::path> replacement{g_soundfonts.size() >= 2 ? g_soundfonts[1] : g_soundfonts[0]};
    const RemoveFile output{temp_output("engrave-prepared-fonts", ".wav")};
    {
        auto prepared = document.prepare_audio({});
        ASSERT_TRUE(prepared);
        std::atomic<bool> started{false}, paused{false}, resume{false}, done{false};
        RunResult result;
        std::thread runner([held = std::move(*prepared), &output, &started, &paused, &resume, &done, &result]() mutable {
            int polls = 0;
            result = held.run(output.path, [&] {
                if (++polls == 1) started.store(true, std::memory_order_release);
                if (polls == 40) {
                    paused.store(true, std::memory_order_release);
                    wait_for(resume);
                }
                return false;
            });
            // Destroy on the run thread so teardown overlaps owner-thread set_soundfonts.
            { const auto finished = std::move(held); }
            done.store(true, std::memory_order_release);
        });
        const auto owner_work = [&] {
            ASSERT_TRUE(wait_for(started));
            // Each call freezes and fully parses its SoundFonts (fresh FluidSynth settings,
            // loader and font): first while the render synthesizes from the cached fonts,
            // then while it is held mid-render and during background teardown.
            int rounds = 0;
            while ((!paused.load(std::memory_order_acquire) && !done.load(std::memory_order_acquire)) || rounds < 2) {
                EXPECT_TRUE(g_engine->set_soundfonts(replacement));
                EXPECT_TRUE(g_engine->set_soundfonts(g_soundfonts));
                ++rounds;
            }
            EXPECT_TRUE(g_engine->set_soundfonts(replacement));
            EXPECT_FALSE(done.load()) << "the render ended before its 40th poll";
            const auto refused = document.prepare_audio({});
            ASSERT_FALSE(refused);
            EXPECT_EQ(refused.error().code, engrave::ErrorCode::RESOURCE_LIMIT);
            resume.store(true, std::memory_order_release);
            while (!done.load(std::memory_order_acquire)) EXPECT_TRUE(g_engine->set_soundfonts(replacement));
        };
        owner_work();
        runner.join();
        ASSERT_TRUE(result && *result) << run_error(result);
        EXPECT_TRUE(read_file(output.path) == baseline);
    }
    {
        // The next render refreshes the engine's fonts to the new selection.
        auto next = document.prepare_audio({});
        ASSERT_TRUE(next) << next.error().message;
        ASSERT_TRUE(next->run(output.path));
        if (g_soundfonts.size() >= 2) EXPECT_FALSE(read_file(output.path) == baseline);
    }
    ASSERT_TRUE(g_engine->set_soundfonts(g_soundfonts));
    EXPECT_TRUE(render_chars(document, {}) == baseline);
}

TEST(AudioInternal, PreparedAudioHoldsTheEngineUntilDestroyed) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto baseline = render_chars(document, {});
    ASSERT_FALSE(baseline.empty());
    auto &audio = engrave::DocumentAccess::context(document).audio();
    const auto &engine = *engrave::DocumentAccess::impl(document).engine;
    const RemoveFile output{temp_output("engrave-prepared-lane", ".wav")};
    {
        auto first = document.prepare_audio({});
        ASSERT_TRUE(first);
        EXPECT_TRUE(music::engine_audio_render_active(audio));
        const auto second = document.prepare_audio({});
        ASSERT_FALSE(second);
        EXPECT_EQ(second.error().code, engrave::ErrorCode::RESOURCE_LIMIT);
        const auto rendered = document.render_audio({}, [](auto) { ADD_FAILURE() << "a refused render reached the sink"; });
        ASSERT_FALSE(rendered);
        EXPECT_EQ(rendered.error().code, engrave::ErrorCode::RESOURCE_LIMIT);
        // The driver refuses a second render and a setup change by itself.
        const auto driver = music::prepare_score_audio(audio, engrave::DocumentAccess::master_score(document), {});
        ASSERT_FALSE(driver);
        EXPECT_EQ(driver.error().failure, music::AudioRenderResult::Failure::RESOURCE_LIMIT);
        music::EngineAudioSetup setup;
        setup.sample_rate = engine.audio_sample_rate;
        setup.audio_threads = engine.audio_threads;
        for (const auto &soundfont : engine.soundfonts) setup.soundfonts.push_back(engrave::utf8(soundfont));
        std::string error;
        EXPECT_FALSE(music::initialize_engine_audio(audio, setup, &error));
        EXPECT_NE(error.find("active"), std::string::npos) << error;

        engrave::PreparedAudio moved = std::move(*first);
        const auto from_moved = first->run(output.path);
        ASSERT_FALSE(from_moved);
        EXPECT_EQ(from_moved.error().code, engrave::ErrorCode::INVALID_ARGUMENT);
        EXPECT_FALSE(std::filesystem::exists(output.path));
        const auto no_path = moved.run({});
        ASSERT_FALSE(no_path);
        EXPECT_EQ(no_path.error().code, engrave::ErrorCode::INVALID_ARGUMENT);
        EXPECT_TRUE(music::engine_audio_render_active(audio));
    }
    // Destroying a render that never ran frees the engine.
    EXPECT_FALSE(music::engine_audio_render_active(audio));
    {
        auto prepared = document.prepare_audio({});
        ASSERT_TRUE(prepared);
        ASSERT_TRUE(prepared->run(output.path));
        EXPECT_TRUE(read_file(output.path) == baseline);
        const auto again = prepared->run(output.path);
        ASSERT_FALSE(again);
        EXPECT_EQ(again.error().code, engrave::ErrorCode::INVALID_ARGUMENT);
        EXPECT_TRUE(read_file(output.path) == baseline);
        const auto refused = document.prepare_audio({});
        ASSERT_FALSE(refused);
        EXPECT_EQ(refused.error().code, engrave::ErrorCode::RESOURCE_LIMIT);
    }
    {
        // A missing parent directory is an I/O failure, and the render is spent.
        auto prepared = document.prepare_audio({});
        ASSERT_TRUE(prepared);
        const auto missing = prepared->run(temp_output("engrave-missing-dir", "") / "out.wav");
        ASSERT_FALSE(missing);
        EXPECT_EQ(missing.error().code, engrave::ErrorCode::IO);
        const auto spent = prepared->run(output.path);
        ASSERT_FALSE(spent);
        EXPECT_EQ(spent.error().code, engrave::ErrorCode::INVALID_ARGUMENT);
    }
    EXPECT_FALSE(music::engine_audio_render_active(audio));
    EXPECT_TRUE(render_chars(document, {}) == baseline);

    // Synchronous render_audio must preserve a value-captured mutable cancellation
    // counter across setup and run, and must release the lane before calling the sink.
    {
        std::vector<int> seen_counts;
        bool saw_setup_phase = false;
        bool saw_run_phase = false;
        engrave::AudioOptions stateful;
        stateful.cancelled = [count = 0, &seen_counts, &saw_setup_phase, &saw_run_phase]() mutable {
            const int current = ++count;
            seen_counts.push_back(current);
            const auto phase = muse::audio::OfflineCancellation::phase;
            if (phase == muse::audio::OfflineCancellation::Phase::PLAYBACK_MODEL ||
                phase == muse::audio::OfflineCancellation::Phase::SEQUENCER ||
                phase == muse::audio::OfflineCancellation::Phase::EVENT_RELEASE) {
                saw_setup_phase = true;
            } else if (phase == muse::audio::OfflineCancellation::Phase::SYNTHESIS ||
                       phase == muse::audio::OfflineCancellation::Phase::ENCODING) {
                saw_run_phase = true;
            }
            return false;
        };
        ASSERT_TRUE(document.render_audio(stateful, [](auto) {}));
        ASSERT_TRUE(saw_setup_phase);
        ASSERT_TRUE(saw_run_phase);
        ASSERT_GT(seen_counts.size(), 2u);
        for (std::size_t i = 0; i < seen_counts.size(); ++i) {
            EXPECT_EQ(seen_counts[i], static_cast<int>(i + 1)) << "counter reset at poll " << i;
        }

        // Re-entering render_audio from the synchronous ByteSink must succeed because the lane is already released.
        bool nested_ran = false;
        const auto outer = document.render_audio({}, [&](std::span<const std::byte>) {
            if (nested_ran) return;
            nested_ran = true;
            std::size_t nested_bytes = 0;
            const auto inner = document.render_audio({}, [&](std::span<const std::byte> chunk) { nested_bytes += chunk.size(); });
            EXPECT_TRUE(inner) << (inner ? "" : inner.error().message);
            EXPECT_EQ(nested_bytes, baseline.size());
        });
        ASSERT_TRUE(outer) << outer.error().message;
        EXPECT_TRUE(nested_ran);
    }
}

TEST(AudioInternal, BackgroundRunCancellationStaysOnTheRunThread) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("repeat70.mscx"));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto baseline = render_chars(document, {});
    ASSERT_FALSE(baseline.empty());
    const RemoveFile output{temp_output("engrave-prepared-cancel", ".wav")};
    for (const bool throws : {false, true}) {
        SCOPED_TRACE(throws ? "throwing callback" : "cancelling callback");
        auto prepared = document.prepare_audio({});
        ASSERT_TRUE(prepared);
        std::atomic<int> foreign_polls{0};
        RunResult result;
        bool threw = false;
        std::thread([&] {
            const auto self = std::this_thread::get_id();
            int polls = 0;
            const auto cancelled = [&]() -> bool {
                if (std::this_thread::get_id() != self) ++foreign_polls;
                if (++polls < 30) return false;
                if (throws) throw std::runtime_error("run cancellation callback");
                return true;
            };
            try {
                result = prepared->run(output.path, cancelled);
            } catch (const std::runtime_error &) {
                threw = true;
            }
        }).join();
        EXPECT_EQ(foreign_polls.load(), 0);
        EXPECT_EQ(threw, throws);
        if (!throws) {
            ASSERT_TRUE(result);
            ASSERT_FALSE(*result);
            EXPECT_EQ((*result).error().code, engrave::ErrorCode::CANCELLED);
        }
        EXPECT_FALSE(std::filesystem::exists(output.path));
    }
    EXPECT_TRUE(render_chars(document, {}) == baseline);
}