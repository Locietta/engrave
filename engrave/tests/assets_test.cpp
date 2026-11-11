// SPDX-License-Identifier: GPL-3.0-only
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>

#include <ft2build.h>

#include "engrave/src/score/json.h"
#include FT_FREETYPE_H

#include "draw/types/font.h"
#include "engrave/src/api/state.h"
#include "engrave/src/audio/engine_audio.h"
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/runtime/fonts_engine_proxy.h"
#include "engrave/src/runtime/runtime_assets.h"
#include "global/io/ifilesystem.h"
#include <engrave/engine.h>

namespace {
void expect(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

std::string read(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    expect(bool(f), "Cannot read font test input");
    return {std::istreambuf_iterator<char>(f), {}};
}

std::string utf8(const std::filesystem::path &p) {
    auto v = p.u8string();
    return {v.begin(), v.end()};
}

struct FontFixture {
    std::string bytes;
    std::string family;
    bool bold = false;
    bool italic = false;
};

FontFixture inspect_font(FT_Library lib, const std::filesystem::path &p) {
    FontFixture result;
    result.bytes = read(p);
    FT_Face face = nullptr;
    expect(FT_New_Memory_Face(lib, reinterpret_cast<const FT_Byte *>(result.bytes.data()), static_cast<FT_Long>(result.bytes.size()), 0,
                              &face) == 0,
           "Invalid font fixture");

    struct FaceGuard {
        FT_Face p;

        ~FaceGuard() {
            if (p) FT_Done_Face(p);
        }
    } guard{face};

    expect(face->num_faces == 1 && face->family_name, "Use a single-face font fixture");
    result.family = face->family_name;
    result.bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
    result.italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
    return result;
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc != 4 && argc != 5) throw std::invalid_argument("Usage: assets_test ASSETS UNREGISTERED_FONT OUTPUT_DIRECTORY [SOUNDFONT]");
        const auto out = std::filesystem::absolute(std::filesystem::u8path(argv[3]));
        std::filesystem::create_directories(out);
        FT_Library lib = nullptr;
        expect(FT_Init_FreeType(&lib) == 0, "Cannot initialize FreeType for test");

        struct LibGuard {
            FT_Library p;

            ~LibGuard() { FT_Done_FreeType(p); }
        } lg{lib};

        const auto fixture = inspect_font(lib, std::filesystem::u8path(argv[2]));
        const auto &bytes = fixture.bytes;
        const std::string family = fixture.family;
        const auto input = out / "mutable-input.ttf";
        {
            std::ofstream f(input, std::ios::binary);
            f.write(bytes.data(), bytes.size());
            expect(bool(f), "Cannot write test font");
        }
        engrave::EngineOptions options;
        options.resources = std::filesystem::u8path(argv[1]);
        auto created = engrave::Engine::create(options);
        expect(bool(created), "Cannot create public engine for asset tests");
        auto public_engine = std::move(*created);
        const auto startup_fonts = public_engine->runtime_fonts();
        expect(startup_fonts.empty(), "Bundled fonts appeared as runtime registrations");
        engrave::CreateOptions create;
        create.template_path = options.resources / "templates/01-General/01-Treble_Clef/01-Treble_Clef.mscx";
        auto opened = public_engine->create_document(create);
        expect(bool(opened), "Cannot create public document for asset tests");
        auto document = std::move(*opened);
        auto &context = engrave::DocumentAccess::context(*document);
        music::RuntimeAssets &assets = context.assets();
        music::RuntimeFontsEngine *engine = &assets.fonts_engine();
        muse::draw::Font font(muse::draw::Font::FontFamily(family), muse::draw::Font::Type::TEXT);
        font.set_point_size_f(12);
        const std::u32string sample = U"MMMWWWiii012345";
        const auto before = engine->horizontal_advance(font, sample); // Cache its missing-family fallback.
        const auto initial_count = assets.fonts().size();
        const auto info = assets.add_font(utf8(input));
        expect(info.family == family && info.path != utf8(input), "Font was not copied with correct family");
        expect(assets.fonts().size() == initial_count + 1, "Font registry count mismatch");
        const auto public_fonts = public_engine->runtime_fonts();
        expect(public_fonts.size() == initial_count + 1 && public_fonts.back().id == info.id && public_fonts.back().family == info.family &&
                   public_fonts.back().bold == info.bold && public_fonts.back().italic == info.italic &&
                   utf8(public_fonts.back().path) == info.path && public_fonts.back().content_fingerprint == info.content_hash,
               "Public font metadata does not match the active retained font");
        const auto after = engine->horizontal_advance(font, sample);
        expect(before > 0 && after > 0 && std::abs(after - before) > 0.01,
               "Cached fallback was not invalidated; choose an unregistered font");
        // A family that only ships a Medium/Bold face (FreeType flags it bold)
        // must still satisfy the regular lookup with that registered face.
        const bool bold_only_family = info.bold && !info.italic;
        double bold_advance = 0;
        if (bold_only_family) {
            muse::draw::Font bold_font(font);
            bold_font.set_bold(true);
            bold_advance = engine->horizontal_advance(bold_font, sample);
            expect(std::abs(after - bold_advance) < 0.0001, "Regular lookup of a bold-only family did not use the registered face");
        }
        const auto again = assets.add_font(utf8(input));
        expect(again.id == info.id && again.path == info.path, "Identical font was not deduplicated");
        expect(assets.fonts().size() == initial_count + 1, "Deduplication added a font");
        muse::draw::Font folded(muse::draw::Font::FontFamily(muse::String::from_utf8(family).to_lower()), muse::draw::Font::Type::TEXT);
        folded.set_point_size_f(12);
        expect(std::abs(engine->horizontal_advance(folded, sample) - after) < 0.0001, "Family lookup lost case-insensitivity");
        {
            std::ofstream f(input, std::ios::binary | std::ios::trunc);
            f << "invalid-font";
        }
        bool bad_rejected = false;
        try {
            assets.add_font(utf8(input));
        } catch (const std::exception &) {
            bad_rejected = true;
        }
        expect(bad_rejected && assets.fonts().size() == initial_count + 1, "Bad font changed registry");
        expect(read(std::filesystem::u8path(info.path)) == bytes, "Cached font changed with source file");
        expect(std::abs(engine->horizontal_advance(font, sample) - after) < 0.0001, "Bad input changed font engine state");
        bool thread_rejected = false, engine_thread_rejected = false;
        std::thread other([&] {
            try {
                assets.fonts();
            } catch (const std::exception &) {
                thread_rejected = true;
            }
            try {
                engine->height(font);
            } catch (const std::exception &) {
                engine_thread_rejected = true;
            }
        });
        other.join();
        expect(thread_rejected && engine_thread_rejected, "Font service accepted wrong thread");
        const auto active = assets.fonts();
        expect(active.back().content_hash == info.content_hash, "Font fingerprint changed");
        music::SortedJson result = {{"status", "passed"},
                                    {"family", family},
                                    {"id", info.id},
                                    {"contentHash", info.content_hash},
                                    {"cachedPath", info.path},
                                    {"before", before},
                                    {"after", after},
                                    {"bold", info.bold},
                                    {"boldOnlyFamily", bold_only_family},
                                    {"boldAdvance", bold_advance},
                                    {"sourceBytes", bytes.size()},
                                    {"immutableCopy", true},
                                    {"caseInsensitive", true},
                                    {"wrongThreadRejected", true}};
        if (argc == 5) {
            const auto sound_bytes = read(std::filesystem::u8path(argv[4]));
            const auto sound_input = out / "mutable-input.sf2";
            {
                std::ofstream f(sound_input, std::ios::binary);
                f.write(sound_bytes.data(), sound_bytes.size());
                expect(bool(f), "Cannot write SoundFont test fixture");
            }
            const auto sound = assets.freeze_sound_font(utf8(sound_input));
            const auto repeated = assets.freeze_sound_font(utf8(sound_input));
            expect(sound.path != utf8(sound_input) && repeated.path == sound.path, "SoundFont immutable copy/dedup failed");
            expect(read(std::filesystem::u8path(sound.path)) == sound_bytes, "SoundFont copy changed bytes");
            const auto cache_dir = std::filesystem::u8path(sound.path).parent_path();
            const auto file_count = std::distance(std::filesystem::directory_iterator(cache_dir), std::filesystem::directory_iterator{});
            {
                std::ofstream f(sound_input, std::ios::binary | std::ios::trunc);
                f << "invalid-soundfont";
            }
            bool rejected = false;
            try {
                assets.freeze_sound_font(utf8(sound_input));
            } catch (const std::exception &) {
                rejected = true;
            }
            expect(rejected, "Invalid SoundFont was published");
            expect(std::distance(std::filesystem::directory_iterator(cache_dir), std::filesystem::directory_iterator{}) == file_count,
                   "Rejected SoundFont leaked a cache file");
            expect(read(std::filesystem::u8path(sound.path)) == sound_bytes, "Source mutation altered retained SoundFont");
            bool wrong_thread = false;
            std::thread wrong([&] {
                try {
                    assets.freeze_sound_font(sound.path);
                } catch (const std::exception &) {
                    wrong_thread = true;
                }
            });
            wrong.join();
            expect(wrong_thread, "SoundFont accepted wrong owner thread");
            result["soundfont"] = music::SortedJson{{"cachedPath", sound.path},
                                                    {"contentHash", sound.content_hash},
                                                    {"sourceBytes", sound_bytes.size()},
                                                    {"invalidRejected", true},
                                                    {"immutableCopy", true}};
        }
        if (argc == 5) {
            auto &audio = context.audio();
            expect(!music::engine_audio_initialized(audio), "Audio started before selecting a SoundFont");
            const auto sound_input_value = out / "mutable-input.sf2";
            const auto sound_bytes_value = read(std::filesystem::u8path(argv[4]));
            {
                std::ofstream f(sound_input_value, std::ios::binary | std::ios::trunc);
                f.write(sound_bytes_value.data(), sound_bytes_value.size());
            }
            expect(bool(public_engine->set_soundfonts({sound_input_value})), "Cannot select valid SoundFont");
            const auto selection = public_engine->capabilities().soundfonts;
            expect(selection.size() == 1 && selection[0] != sound_input_value, "SoundFont selection was not retained");
            expect(!music::engine_audio_initialized(audio), "Selecting a SoundFont started audio services");
            {
                std::ofstream f(sound_input_value, std::ios::binary | std::ios::trunc);
                f.write("RIFF\x04\0\0\0sfbk", 12);
            }
            expect(read(selection[0]) == sound_bytes_value, "Retained SoundFont changed with its source");
            const auto invalid = public_engine->set_soundfonts({sound_input_value});
            expect(!invalid && invalid.error().code == engrave::ErrorCode::INVALID_INPUT, "Invalid SoundFont was accepted");
            expect(public_engine->capabilities().soundfonts == selection, "Invalid selection changed capabilities");
            expect(!music::engine_audio_initialized(audio), "Failed SoundFont selection started audio services");
            {
                std::ofstream f(sound_input_value, std::ios::binary | std::ios::trunc);
                f << "invalid-soundfont";
            }
            result["soundfont"]["publicSelectionDeferredAudio"] = true;
        }
        document.reset();
        auto lease = assets.retain();
        assets.shutdown();
        expect(std::filesystem::exists(std::filesystem::u8path(info.path)), "Outstanding lease lost its font file");
        if (result.contains("soundfont"))
            expect(std::filesystem::exists(std::filesystem::u8path(result["soundfont"]["cachedPath"].get<std::string>())),
                   "Outstanding lease lost its SoundFont file");
        lease.reset();
        expect(!std::filesystem::exists(std::filesystem::u8path(info.path)), "Font cache survived runtime shutdown");
        if (result.contains("soundfont"))
            expect(!std::filesystem::exists(std::filesystem::u8path(result["soundfont"]["cachedPath"].get<std::string>())),
                   "SoundFont cache survived runtime shutdown");
        public_engine.reset();
        expect(muse::io::file_system() == nullptr, "Engine teardown left the file system bound");
        result["contextTeardown"] = true;
        result["shutdownCleanup"] = true;
        result["leaseDeferredCleanup"] = true;
        std::ofstream report(out / "evidence.json");
        report << music::dump_json(result, 2);
        report.close();
        expect(bool(report), "Cannot write evidence");
        std::cout << music::dump_json(result) << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
