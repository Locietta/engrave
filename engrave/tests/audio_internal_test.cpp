// SPDX-License-Identifier: GPL-3.0-only
// Failure injection for the offline writer, with recovery through the public API.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <latch>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "audio/common/audioerrors.h"
#include "audio/engine/internal/export/abstractaudioencoder.h"
#include "audio/engine/internal/nodes/eventaudionode.h"
#include "audio/engine/internal/offlinecancellation.h"
#include "audio/engine/internal/offlineeventrelease.h"
#include "audio/engine/internal/offlinetiming.h"
#include "engrave/src/api/state.h"
#include "engrave/src/audio/engine_audio.h"
#include "global/io/iodevice.h"
#include "output_fixture.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

struct FailingFinalizer final : muse::audio::encode::AbstractAudioEncoder {
    FailingFinalizer(const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device, bool fail_encode, int &ended)
        : AbstractAudioEncoder(format), device_(device), fail_encode_(fail_encode), ended_(ended) {}

    bool begin(muse::audio::samples_t) override { return true; }

    size_t encode(muse::audio::samples_t samples, const float *) override {
        const std::uint8_t marker = 42;
        if (!wrote_) {
            wrote_ = true;
            if (device_.write(&marker, 1) != 1) return 0;
        }
        return fail_encode_ ? 0 : static_cast<size_t>(samples);
    }

    muse::Ret end() override {
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
struct CancellingEncoder final : muse::audio::encode::AbstractAudioEncoder {
    CancellingEncoder(const muse::audio::SoundTrackFormat &format, muse::io::IODevice &device, bool &cancelled, bool final, int &encoded,
                      int &writes)
        : AbstractAudioEncoder(format), device_(device), cancelled_(cancelled), final_(final), encoded_(encoded), writes_(writes) {}

    bool begin(muse::audio::samples_t) override { return true; }

    size_t encode(muse::audio::samples_t samples, const float *) override {
        ++encoded_;
        if (!final_ && encoded_ == 2) inject();
        return samples;
    }

    muse::Ret end() override {
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

TEST(AudioInternal, CancellationAtSynthesisWriteAndFinalFlushRemovesFileAndRecovers) {
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
    for (const auto format : {music::EngineAudioFormat::WAV, music::EngineAudioFormat::FLAC, music::EngineAudioFormat::OGG,
                              music::EngineAudioFormat::MP3, music::EngineAudioFormat::AAC}) {
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
    ASSERT_TRUE(g_engine->set_soundfonts(g_soundfonts));
}

} // namespace

// Real driver/graph teardown is included. Phase requests occur synchronously on
// the owner thread; the separate delayed case samples arbitrary operation work.
TEST(AudioInternal, CancellationLatencyDistribution) {
    using Clock = std::chrono::steady_clock;
    auto opened = music::test::g_engine->open(music::test::corpus_file("repeat70.mscx"));
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

    struct LargeWriter final : muse::audio::encode::AbstractAudioEncoder {
    private:
        muse::io::IODevice &device;
        bool final;
        bool &armed;
        std::vector<uint8_t> bytes = std::vector<uint8_t>(1024 * 1024, 42);
    public:
        LargeWriter(const muse::audio::SoundTrackFormat &f, muse::io::IODevice &d, bool end, bool &a)
            : AbstractAudioEncoder(f), device(d), final(end), armed(a) {}

        bool begin(muse::audio::samples_t) override { return true; }

        size_t encode(muse::audio::samples_t n, const float *) override {
            if (!final) {
                armed = true;
                return device.write(bytes.data(), bytes.size()) ? n : 0;
            }
            return n;
        }

        muse::Ret end() override {
            armed = true;
            return device.write(bytes.data(), bytes.size()) ? muse::make_ok() : muse::Ret(false);
        }
    };

    const auto path = std::filesystem::temp_directory_path() / "engrave-granularity.wav";
    for (int phase = 0; phase < 4; ++phase) {
        std::vector<double> times;
        for (int trial = 0; trial < 50; ++trial) {
            bool armed = false, cancelled = false;
            int write_polls = 0;
            std::atomic<bool> external{false};
            Clock::time_point requested;
            music::AudioRenderRequest request;
            const auto utf8 = path.generic_u8string();
            request.output_path.assign(utf8.begin(), utf8.end());
            request.cancelled = [&] {
                if (armed && ++write_polls == 3) {
                    requested = Clock::now();
                    cancelled = true;
                }
                return cancelled || external.load();
            };
            if (phase == 0)
                music::set_engine_audio_midi_event_hook(audio, [&](const auto &, uint64_t) {
                    if (!cancelled && muse::audio::OfflineCancellation::phase == muse::audio::OfflineCancellation::Phase::SYNTHESIS) {
                        requested = Clock::now();
                        cancelled = true;
                    }
                });
            if (phase == 1 || phase == 2)
                request.encoder_factory = [&](const auto &f, auto &d) { return std::make_unique<LargeWriter>(f, d, phase == 2, armed); };
            std::thread transport;
            if (phase == 3)
                transport = std::thread([&] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    requested = Clock::now();
                    external.store(true);
                });
            const auto result = music::export_score_audio(audio, master, request);
            const auto returned = Clock::now();
            if (transport.joinable()) transport.join();
            music::set_engine_audio_midi_event_hook(audio, {});
            ASSERT_EQ(result.failure, music::AudioRenderResult::Failure::CANCELLED);
            ASSERT_FALSE(std::filesystem::exists(path));
            times.push_back(std::chrono::duration<double, std::milli>(returned - requested).count());
        }
        std::sort(times.begin(), times.end());
        std::fprintf(stderr, "cancel_ms phase=%d n=50 p50=%.6f p95=%.6f max=%.6f\n", phase, times[24], times[47], times.back());
        EXPECT_LT(times[47], 50.0);
        EXPECT_LT(times.back(), 100.0);
        EXPECT_EQ(render(), baseline) << "cancellation poisoned the SoundFont cache";
    }
}

TEST(AudioInternal, EventReleaseOwnershipAndBatches) {
    using namespace muse::audio;
    constexpr auto batch_events = event_release_batch_cost / event_release_base_cost;
    muse::mpe::PlaybackEventsMap events;
    events[0].resize(3 * batch_events);
    const auto alias = events;
    EXPECT_FALSE(events.unique());
    EXPECT_TRUE(release_event_batches(events, [] {
        ADD_FAILURE() << "Aliased events must not be touched";
        return true;
    }));
    EXPECT_FALSE(events.unique());
    EXPECT_EQ(events.cbegin()->second.size(), 3 * batch_events);
    // Drop the alias by replacing the source instead; verify the original is intact.
    events = {};
    EXPECT_TRUE(events.unique());
    events[0].resize(3 * batch_events);
    events[1].resize(batch_events - 1);
    int polls = 0;
    EXPECT_FALSE(release_event_batches(events, [&] { return ++polls == 2; }));
    EXPECT_FALSE(events.contains(1));
    EXPECT_EQ(events.at(0).size(), 3 * batch_events);
    EXPECT_FALSE(release_event_batches(events, [&] { return ++polls == 4; }));
    EXPECT_EQ(events.at(0).size(), 2 * batch_events);
    EXPECT_TRUE(release_event_batches(events, [] { return false; }));
    EXPECT_TRUE(events.empty());
    EXPECT_EQ(alias.at(0).size(), 3 * batch_events);
}

TEST(AudioInternal, EventReleaseLargeCurves) {
    using namespace muse::audio;
    using namespace muse::mpe;
    using Clock = std::chrono::steady_clock;
    const auto fill = [](PlaybackEventsMap &events, int points) {
        ExpressionContext expression;
        for (int i = 0; i < points; ++i) expression.expression_curve.emplace(i, i);
        events[0].emplace_back(NoteEvent(ArrangementContext{}, PitchContext{}, std::move(expression)));
    };
    double maximum = 0;
    for (int trial = 0; trial < 32; ++trial) {
        PlaybackEventsMap events;
        fill(events, static_cast<int>(event_release_batch_cost - event_release_base_cost));
        EXPECT_EQ(event_release_cost(events.cbegin()->second.back()), event_release_batch_cost);
        Clock::time_point previous;
        EXPECT_TRUE(release_event_batches(events, [&] {
            const auto now = Clock::now();
            if (previous != Clock::time_point{})
                maximum = std::max(maximum, std::chrono::duration<double, std::milli>(now - previous).count());
            previous = now;
            return false;
        }));
        EXPECT_TRUE(events.empty());
    }
    PlaybackEventsMap oversized;
    fill(oversized, 262144);
    const auto &event = std::get<NoteEvent>(oversized.cbegin()->second.back());
    ASSERT_TRUE(event.expression_ctx().expression_curve.unique());
    ASSERT_GT(event_release_cost(oversized.cbegin()->second.back()), event_release_batch_cost);
    const auto start = Clock::now();
    EXPECT_TRUE(release_event_batches(oversized, [] { return false; }));
    const double fallback_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    // An oversized event remains intact for ordinary teardown, never popped in a batch.
    EXPECT_EQ(oversized.size(), 1);
    EXPECT_EQ(event.expression_ctx().expression_curve.size(), 262144);
    EXPECT_LE(maximum, 0.5);
    EXPECT_LE(fallback_ms, 0.5);
    std::fprintf(stderr, "release_cost_ms budget=%zu eligible_points=%zu n=32 max=%.6f oversized_points=262144 fallback=%.6f\n",
                 event_release_batch_cost, event_release_batch_cost - event_release_base_cost, maximum, fallback_ms);
    // The same graph hidden in an articulation's applied segment or its meta
    // pattern must also force fallback; neither is covered by top-level sizes.
    ArticulationAppliedData art;
    art.applied_pattern_segment.expression_pattern.dynamic_offset_map = event.expression_ctx().expression_curve;
    ExpressionContext nested;
    nested.articulations.emplace(ArticulationType::STANDARD, art);
    EXPECT_GT(event_release_cost(NoteEvent(ArrangementContext{}, PitchContext{}, std::move(nested))), event_release_batch_cost);
    art.meta.pattern.emplace(0, art.applied_pattern_segment);
    art.applied_pattern_segment = {};
    ExpressionContext pattern;
    pattern.articulations.emplace(ArticulationType::STANDARD, art);
    EXPECT_GT(event_release_cost(NoteEvent(ArrangementContext{}, PitchContext{}, std::move(pattern))), event_release_batch_cost);
}

TEST(AudioInternal, FailedEventSourceDisarmsBorrow) {
    using namespace muse;
    using namespace muse::audio;
    using namespace muse::audio::engine;

    struct FailingFactory : IAudioFactory {
        mutable int calls = 0;
        bool throws = false;

        AudioResourceMetaList available_input_resources() const override { return {}; }

        SoundPresetList available_sound_presets(const AudioResourceMeta &) const override { return {}; }

        AudioResourceMetaList available_output_resources() const override { return {}; }

        RetVal<synth::ISynthesizerPtr> make_synth(TrackId, const AudioInputParams &, const PlaybackSetupData &) const override {
            ++calls;
            if (throws) throw std::runtime_error("injected setup failure");
            return RetVal<synth::ISynthesizerPtr>::make_ret(Err::INVALID_SYNTH);
        }

        RetVal<synth::ISynthesizerPtr> make_default_synth(TrackId) const override {
            ++calls;
            return RetVal<synth::ISynthesizerPtr>::make_ret(Err::INVALID_SYNTH);
        }

        void clear_synth_sources() override {}

        RetVal<AudioSourceNodePtr> make_event_source(TrackId, const mpe::PlaybackData &, const AudioInputParams &) const override {
            return {};
        }

        FxChainPtr make_master_fx_chain(const AudioFxChain &) const override { return nullptr; }

        FxChainPtr make_track_fx_chain(TrackId, const AudioFxChain &) const override { return nullptr; }

        void clear_all_fx() override {}
    };

    for (bool throws : {false, true}) {
        FailingFactory factory;
        factory.throws = throws;
        auto data = std::make_unique<mpe::PlaybackData>();
        EventAudioNode node(factory, 1, *data);
        node.set_output_spec({44100, 4096, 2});
        if (throws)
            EXPECT_THROW(node.apply_input_params({}), std::runtime_error);
        else
            node.apply_input_params({});
        EXPECT_FALSE(node.initialized());
        const auto calls = factory.calls;
        data.reset();
        node.apply_input_params({});
        EXPECT_EQ(factory.calls, calls);
    }
}

// Same 12 x 256 x 16 input shape as the dense-score probe, generated without a desktop.
TEST(AudioInternal, DenseScoreCancellationLatencyAndRecovery) {
    using Clock = std::chrono::steady_clock;
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
        for (int p = 1; p <= 12; ++p)
            out << "<score-part id=\"P" << p << "\"><part-name>Part " << p << "</part-name><midi-instrument id=\"I" << p
                << "\"><midi-channel>" << p << "</midi-channel><midi-program>1</midi-program></midi-instrument></score-part>";
        out << "</part-list>";
        for (int p = 1; p <= 12; ++p) {
            out << "<part id=\"P" << p << "\">";
            for (int m = 1; m <= 256; ++m) {
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
        std::vector<double> responsiveness, cleanup;
        for (int trial = 0; trial < 50; ++trial) {
            bool cancelled = false;
            bool model_started = false;
            int model_polls = 0, stage_polls = 0;
            std::latch model_start{1};
            std::atomic<bool> external{false};
            Clock::time_point requested, observed;
            std::thread transport;
            if (phase == 1)
                transport = std::thread([&] {
                    model_start.wait();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1 + trial % 5));
                    requested = Clock::now();
                    external.store(true);
                });
            const auto cancel = [&] {
                if (!cancelled) {
                    requested = Clock::now();
                    cancelled = true;
                }
            };
            music::AudioRenderRequest request;
            const auto utf8 = path.generic_u8string();
            request.output_path.assign(utf8.begin(), utf8.end());
            request.cancelled = [&] {
                EXPECT_EQ(std::this_thread::get_id(), owner_thread);
                // Arm at an existing stage marker, but return false from this poll.
                // Only the NEXT poll may observe the request: the measured span
                // includes engine work between polls, rather than callback cost.
                const auto current_phase = muse::audio::OfflineCancellation::phase;
                if (!cancelled &&
                    ((phase == 3 && current_phase == Phase::ENCODING) || (phase == 0 && current_phase == Phase::SEQUENCER) ||
                     (phase == 4 && current_phase == Phase::EVENT_RELEASE)) &&
                    ++stage_polls == (phase == 4 ? 1 + trial * 8 : 1 + trial * 3)) {
                    cancel();
                    return false;
                }
                if (phase == 1 && muse::audio::OfflineCancellation::phase == Phase::PLAYBACK_MODEL &&
                    ++model_polls == 2000 + trial * 3000 && !model_started) {
                    model_started = true;
                    model_start.count_down();
                }
                if (external.load() && !cancelled) {
                    EXPECT_EQ(muse::audio::OfflineCancellation::phase, Phase::PLAYBACK_MODEL);
                    cancelled = true;
                }
                // First positive poll is the existing observable cancellation boundary.
                // Everything after it includes synchronous graph/event cleanup.
                if (cancelled && observed == Clock::time_point{}) observed = Clock::now();
                return cancelled;
            };
            if (phase == 2)
                music::set_engine_audio_midi_event_hook(audio, [&](const auto &, uint64_t) {
                    if (muse::audio::OfflineCancellation::phase == Phase::SYNTHESIS) cancel();
                });
            const auto result = music::export_score_audio(audio, master, request);
            const auto returned = Clock::now();
            if (transport.joinable()) {
                if (!model_started) model_start.count_down();
                transport.join();
                ASSERT_TRUE(model_started);
            }
            music::set_engine_audio_midi_event_hook(audio, {});
            ASSERT_TRUE(cancelled);
            ASSERT_EQ(result.failure, music::AudioRenderResult::Failure::CANCELLED);
            ASSERT_FALSE(std::filesystem::exists(path));
            ASSERT_NE(observed, Clock::time_point{});
            ASSERT_LE(requested, observed);
            ASSERT_LE(observed, returned);
            responsiveness.push_back(std::chrono::duration<double, std::milli>(observed - requested).count());
            cleanup.push_back(std::chrono::duration<double, std::milli>(returned - observed).count());
            std::fprintf(stderr, "dense_cancel_sample phase=%s trial=%d request_to_observed_ms=%.6f observed_to_return_ms=%.6f\n",
                         names[phase], trial, responsiveness.back(), cleanup.back());
        }
        const auto report = [&](const char *measurement, auto &times, double p95_limit, double max_limit) {
            std::sort(times.begin(), times.end());
            std::fprintf(stderr, "dense_cancel_ms phase=%s measurement=%s n=50 p50=%.6f p95=%.6f max=%.6f\n", names[phase], measurement,
                         times[24], times[47], times.back());
            EXPECT_LE(times[47], p95_limit) << names[phase] << " " << measurement;
            EXPECT_LE(times.back(), max_limit) << names[phase] << " " << measurement;
        };
        // Idle Windows/Linux observations are well below a millisecond; leave
        // scheduling margin while keeping responsiveness independent of teardown.
        report("request_to_observed", responsiveness, 5.0, 10.0);
        // Idle Windows request-to-return peaks near 46 ms. Cleanup owns
        // almost all of that span, so allow 50 ms p95 and 100 ms worst case.
        report("observed_to_return", cleanup, 50.0, 100.0);
        EXPECT_EQ(render(), baseline);
    }
    // Exercise error translation and publication through the public API too.
    engrave::AudioOptions options;
    int model_polls = 0, sinks = 0;
    options.cancelled = [&] { return muse::audio::OfflineCancellation::phase == Phase::PLAYBACK_MODEL && ++model_polls == 3000; };
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
        if (muse::audio::OfflineCancellation::phase == Phase::PLAYBACK_MODEL && ++model_polls == 3000)
            throw std::runtime_error("model cancellation callback");
        return false;
    };
    EXPECT_THROW(std::ignore = document.render_audio(options, [](auto) {}), std::runtime_error);
    EXPECT_EQ(render(), baseline);
}

TEST(AudioInternal, WarmGraphRebuildTiming) {
    if (!muse::audio::OfflineTiming::enabled()) GTEST_SKIP() << "set ENGRAVE_AUDIO_TIMING=1";
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    std::vector<double> setup, teardown, create, fonts, flush;
    for (int i = 0; i < 51; ++i) {
        ASSERT_TRUE((*opened)->render_audio({}, [](auto) {}));
        const auto &t = muse::audio::OfflineTiming::current();
        if (i == 0) continue;
        setup.push_back(t.setup * 1000);
        teardown.push_back(t.teardown * 1000);
        create.push_back(t.create * 1000);
        fonts.push_back(t.fonts * 1000);
        flush.push_back(t.flush * 1000);
    }
    const auto report = [](const char *phase, auto values) {
        std::sort(values.begin(), values.end());
        std::fprintf(stderr, "warm_ms phase=%s n=50 p50=%.6f p95=%.6f max=%.6f\n", phase, values[24], values[47], values.back());
    };
    report("setup", setup);
    report("teardown", teardown);
    report("create", create);
    report("fonts", fonts);
    report("flush", flush);
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
