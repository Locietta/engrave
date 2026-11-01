// SPDX-License-Identifier: GPL-3.0-only
// Failure injection for the offline writer, with recovery through the public API.

#include "output_fixture.h"
#include "engrave/src/api/state.h"
#include "engrave/src/audio/engine_audio.h"
#include "audio/common/audioerrors.h"
#include "audio/engine/internal/export/abstractaudioencoder.h"
#include "global/io/iodevice.h"

#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

class FailingFinalizer final : public muse::audio::encode::AbstractAudioEncoder {
public:
    FailingFinalizer(const muse::audio::SoundTrackFormat& format, muse::io::IODevice& device,
                     bool failEncode, int& ended)
        : AbstractAudioEncoder(format), device_(device), failEncode_(failEncode), ended_(ended) {}

    bool begin(muse::audio::samples_t) override { return true; }
    size_t encode(muse::audio::samples_t samples, const float*) override {
        const std::uint8_t marker = 42;
        if (!wrote_) {
            wrote_ = true;
            if (device_.write(&marker, 1) != 1) return 0;
        }
        return failEncode_ ? 0 : static_cast<size_t>(samples);
    }
    muse::Ret end() override {
        ++ended_;
        return muse::audio::make_ret(muse::audio::Err::ErrorEncode, "injected finalization failure");
    }

private:
    muse::io::IODevice& device_;
    bool failEncode_;
    int& ended_;
    bool wrote_ = false;
};

// Sets cancellation inside encode/end, then attempts two distinct device writes.
// The second write must be rejected immediately, before control returns to writer.
class CancellingEncoder final : public muse::audio::encode::AbstractAudioEncoder {
public:
    CancellingEncoder(const muse::audio::SoundTrackFormat& format, muse::io::IODevice& device,
                      bool& cancelled, bool final, int& encoded, int& writes)
        : AbstractAudioEncoder(format), device_(device), cancelled_(cancelled), final_(final),
          encoded_(encoded), writes_(writes) {}
    bool begin(muse::audio::samples_t) override { return true; }
    size_t encode(muse::audio::samples_t samples, const float*) override {
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
    muse::io::IODevice& device_;
    bool& cancelled_;
    bool final_;
    int& encoded_;
    int& writes_;
};

TEST(AudioInternal, CancellationAtSynthesisWriteAndFinalFlushRemovesFileAndRecovers) {
    auto opened = music::test::g_engine->open(music::test::corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened);
    auto& document = **opened;
    auto render = [&] {
        std::vector<std::byte> bytes;
        auto result = document.render_audio({}, [&](std::span<const std::byte> chunk) {
            bytes.insert(bytes.end(), chunk.begin(), chunk.end());
        });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto baseline = render();
    ASSERT_FALSE(baseline.empty());
    auto& audio = engrave::DocumentAccess::context(document).audio();
    auto* master = engrave::DocumentAccess::master_score(document);
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto output = std::filesystem::temp_directory_path()
        / ("engrave-cancel-internal-" + std::to_string(process) + ".wav");
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
        request.encoder_factory = [&](const muse::audio::SoundTrackFormat& format, muse::io::IODevice& device) {
            return std::make_unique<CancellingEncoder>(format, device, cancelled, phase == 2, encoded, writes);
        };
        if (phase == 0) {
            music::set_engine_audio_midi_event_hook(audio, [&](const auto&, uint64_t) { cancelled = true; });
        }
        if (phase == 3) {
            EXPECT_THROW(music::export_score_audio(audio, master, request), std::runtime_error);
        } else {
            const auto result = music::export_score_audio(audio, master, request);
            EXPECT_FALSE(result.ok);
            EXPECT_EQ(result.failure, music::AudioRenderResult::Failure::Cancelled);
        }
        music::set_engine_audio_midi_event_hook(audio, {});
        EXPECT_FALSE(std::filesystem::exists(output));
        EXPECT_GT(polls, 0);
        if (phase == 0) {
            EXPECT_TRUE(cancelled); // event handled inside synthesis block
            EXPECT_EQ(encoded, 0); // stopped before that block reached encoder
        } else {
            EXPECT_EQ(writes, 1); // second write in the same encoder call rejected
            if (phase != 2) EXPECT_EQ(encoded, 2);
            else EXPECT_GT(encoded, 2); // reached final encoder flush
        }
        EXPECT_EQ(render(), baseline);
    }
    // Public adapter must preserve Cancelled and publish no bytes.
    engrave::AudioOptions options;
    int polls = 0, sinks = 0;
    options.cancelled = [&] { return ++polls == 100; };
    auto result = document.render_audio(options, [&](auto) { ++sinks; });
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, engrave::ErrorCode::Cancelled);
    EXPECT_EQ(polls, 100);
    EXPECT_EQ(sinks, 0);
    const auto& state = *engrave::DocumentAccess::impl(document).engine;
    const auto intermediate = state.cache_dir / ("engrave-audio-"
        + std::to_string(
#ifdef _WIN32
            _getpid()
#else
            getpid()
#endif
        ) + "-" + std::to_string(state.audio_render_index) + ".wav");
    EXPECT_FALSE(std::filesystem::exists(intermediate));
    EXPECT_EQ(render(), baseline);
}

TEST(AudioInternal, FinalizationFailureReportsFirstErrorAndNextRenderRecovers) {
    const auto path = music::test::corpus_file("ghost_note.mscx");
    ASSERT_FALSE(path.empty());
    auto opened = music::test::g_engine->open(path);
    ASSERT_TRUE(opened) << opened.error().message;
    auto& document = **opened;
    auto render = [&document] {
        std::vector<std::byte> bytes;
        const auto result = document.render_audio({}, [&bytes](std::span<const std::byte> chunk) {
            bytes.insert(bytes.end(), chunk.begin(), chunk.end());
        });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto expected = render();
    ASSERT_FALSE(expected.empty());

    auto& audio = engrave::DocumentAccess::context(document).audio();
    auto* master = engrave::DocumentAccess::master_score(document);
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto output = std::filesystem::temp_directory_path()
                        / ("engrave-finalization-test-" + std::to_string(process) + ".wav");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove(path, ec); }
    } cleanup{output};

    for (const bool failEncode : { false, true }) {
        int ended = 0;
        music::AudioRenderRequest request;
        const auto outputUtf8 = output.generic_u8string();
        request.output_path.assign(outputUtf8.begin(), outputUtf8.end());
        request.encoder_factory = [failEncode, &ended](const muse::audio::SoundTrackFormat& format,
                                                         muse::io::IODevice& device) {
            return std::make_unique<FailingFinalizer>(format, device, failEncode, ended);
        };
        const auto failed = music::export_score_audio(audio, master, request);
        EXPECT_FALSE(failed.ok);
        EXPECT_EQ(failed.failure, music::AudioRenderResult::Failure::EngineFailure);
        EXPECT_EQ(ended, 1);
        EXPECT_GT(std::filesystem::file_size(output), 0u);
        if (failEncode) {
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
    auto& document = **opened;
    auto render = [&document] {
        std::vector<std::byte> bytes;
        const auto result = document.render_audio({}, [&bytes](std::span<const std::byte> chunk) {
            bytes.insert(bytes.end(), chunk.begin(), chunk.end());
        });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto original = render();
    ASSERT_FALSE(original.empty());
    auto& audio = engrave::DocumentAccess::context(document).audio();
    auto* master = engrave::DocumentAccess::master_score(document);
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto output = std::filesystem::temp_directory_path()
                        / ("engrave-budget-test-" + std::to_string(process) + ".wav");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove(path, ec); }
    } cleanup{output};
    music::AudioRenderRequest request;
    const auto outputUtf8 = output.generic_u8string();
    request.output_path.assign(outputUtf8.begin(), outputUtf8.end());
    request.max_bytes = 64;
    for (const auto format : {music::EngineAudioFormat::Wav, music::EngineAudioFormat::Flac,
                              music::EngineAudioFormat::Ogg, music::EngineAudioFormat::Mp3,
                              music::EngineAudioFormat::Aac}) {
        request.format = format;
        const auto failed = music::export_score_audio(audio, master, request);
        EXPECT_FALSE(failed.ok);
        EXPECT_EQ(failed.failure, music::AudioRenderResult::Failure::ResourceLimit);
        EXPECT_FALSE(std::filesystem::exists(output));
    }
    EXPECT_EQ(render(), original);
}

TEST(AudioInternal, SoundFontRefreshWaitsForNextRender) {
    using namespace music::test;
    if (g_soundfonts.size() < 2) GTEST_SKIP() << "pass two different SoundFonts";
    auto opened = g_engine->open(corpus_file("ghost_note.mscx"));
    ASSERT_TRUE(opened) << opened.error().message;
    auto& document = **opened;
    ASSERT_TRUE(g_engine->set_soundfonts({g_soundfonts[0]}));
    auto render = [&document] {
        std::vector<std::byte> bytes;
        const auto result = document.render_audio({}, [&bytes](std::span<const std::byte> chunk) {
            bytes.insert(bytes.end(), chunk.begin(), chunk.end());
        });
        EXPECT_TRUE(result);
        return bytes;
    };
    const auto first = render();
    ASSERT_FALSE(first.empty());
    auto& audio = engrave::DocumentAccess::context(document).audio();
    const auto originalUri = music::engine_soundfont_uri(audio);
    ASSERT_FALSE(originalUri.empty());

    ASSERT_TRUE(g_engine->set_soundfonts({g_soundfonts[1]}));
    EXPECT_EQ(music::engine_soundfont_uri(audio), originalUri);
    const auto selection = g_engine->capabilities().soundfonts;
    ASSERT_EQ(selection.size(), 1u);
    EXPECT_NE(selection[0], g_soundfonts[1]);
    const auto second = render();
    ASSERT_FALSE(second.empty());
    EXPECT_NE(second, first);
    EXPECT_NE(music::engine_soundfont_uri(audio), originalUri);
    ASSERT_TRUE(g_engine->set_soundfonts(g_soundfonts));
}

} // namespace
