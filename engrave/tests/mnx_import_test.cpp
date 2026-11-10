// SPDX-License-Identifier: GPL-3.0-only
// Existing upstream project examples retain the repository GPL-3.0-only licence.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/state.h"
#include "engrave/src/score/score_semantic.h"
#include "engraving/dom/masterscore.h"
#include "importexport/mnx/internal/export/mnxexporter.h"
#include "model/document.h"
#include <engrave/engrave.h>

namespace {
std::unique_ptr<engrave::Engine> engine;
std::filesystem::path data = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/importexport/mnx/tests/data/project_examples";
using Json = music::Json;

Json events(engrave::Document &doc) { return music::semantic_dump(engrave::DocumentAccess::master_score(doc)).at("events"); }

// The MSCX reader subdivides gap rests (including the historical 1/3-tick
// rounding in tupletHiddenRest). Compare sounding events with all their note,
// duration, tie, lyric and articulation semantics; byte/path equality below
// still compares the complete event dump, including every imported rest.
Json sounding_events(Json dump) {
    for (auto &staff : dump.get_array())
        for (auto &voice : staff.get_array()) {
            auto notes = music::make_array<Json>();
            for (const auto &event : voice.get_array())
                if (event.at("kind").get<std::string>() == "chord") music::array_push(notes, event);
            voice = std::move(notes);
        }
    return dump;
}

TEST(MnxImport, UpstreamSemanticReferences) {
    unsigned count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(data)) {
        if (entry.path().extension() != ".mnx") continue;
        ++count;
        SCOPED_TRACE(entry.path().filename().string());
        auto opened = engine->open(entry.path());
        ASSERT_TRUE(opened) << opened.error().message;
        auto reference = engine->open(data / (entry.path().stem().string() + "_ref.mscx"));
        ASSERT_TRUE(reference) << reference.error().message;
        if (entry.path().stem() == "tupletHiddenRest") {
            // This retained MSCX reference loses A4 at tick 1280 when the native
            // reader fills its leading tuplet gap. Use the MNX source's explicit
            // two 3:2 groups as the independent oracle, without changing that reader.
            const auto dump = events(**opened);
            Json actual = music::make_array<Json>();
            for (unsigned voice = 0; voice < 2; ++voice)
                for (const auto &event : dump[0][voice].get_array())
                    if (event.at("kind").get<std::string>() == "chord")
                        music::array_push(actual, music::json_array_of<Json>(voice, event.at("tick"), event.at("ticks"),
                                                                             event.at("notes")[0].at("pitch")));
            const Json expected =
                music::json_array_of<Json>(music::json_array_of<Json>(0, 320, 320, 74), music::json_array_of<Json>(0, 640, 320, 72),
                                           music::json_array_of<Json>(0, 960, 320, 72), music::json_array_of<Json>(0, 1280, 320, 69),
                                           music::json_array_of<Json>(0, 1600, 320, 67), music::json_array_of<Json>(1, 0, 320, 64),
                                           music::json_array_of<Json>(1, 320, 640, 65));
            EXPECT_TRUE(music::json_equal(actual, expected));
            EXPECT_EQ(dump[0][0][0].at("kind").get<std::string>(), "rest");
            EXPECT_EQ(dump[0][0][0].at("ticks").as<int>(), 320);
            EXPECT_EQ(dump[0][1].get_array().back().at("ticks").as<int>(), 960);
            EXPECT_TRUE(music::json_equal(dump[0][0][2].at("notes")[0].at("tieTo"), music::json_array_of<Json>(960, 0, 72)));
        } else {
            EXPECT_TRUE(music::json_equal(sounding_events(events(**opened)), sounding_events(events(**reference))));
        }
        // Byte input without a name exercises JSON content detection.
        std::ifstream in(entry.path(), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(in), {}};
        auto sniffed = engine->open(std::as_bytes(std::span(bytes)));
        ASSERT_TRUE(sniffed) << sniffed.error().message;
        EXPECT_TRUE(music::json_equal(events(**opened), events(**sniffed)));
    }
    EXPECT_EQ(count, 37u);
}

TEST(MnxImport, IndependentSemanticOracle) {
    auto opened = engine->open(data.parent_path() / "engine/two-notes.mnx");
    ASSERT_TRUE(opened) << opened.error().message;
    auto dump = events(**opened)[0][0];
    ASSERT_EQ(dump.size(), 2u);
    for (unsigned i = 0; i < 2; ++i) {
        EXPECT_EQ(dump[i].at("kind").get<std::string>(), "chord");
        EXPECT_EQ(dump[i].at("tick").as<int>(), static_cast<int>(i * 960));
        EXPECT_EQ(dump[i].at("ticks").as<int>(), 960);
        EXPECT_EQ(dump[i].at("notes")[0].at("pitch").as<int>(), i == 0 ? 60 : 62);
    }
}

// Optional directory for the exported MNX files; the engine gate also validates
// them with an independent draft 2020-12 schema validator.
std::filesystem::path mnx_out;

// Compare the importer-supported core independently of engraving/layout details:
// every chord with its pitches and every rest, each with its position and length.
Json core_content(engrave::Document &doc) {
    const auto dump = music::semantic_dump(engrave::DocumentAccess::master_score(doc));
    Json notes = music::make_array<Json>();
    Json rests = music::make_array<Json>();
    for (size_t staff = 0; staff < dump.at("events").size(); ++staff)
        for (size_t voice = 0; voice < dump.at("events")[staff].size(); ++voice)
            for (const auto &event : dump.at("events")[staff][voice].get_array()) {
                const Json position = music::json_array_of<Json>(staff, voice, event.at("tick"), event.at("ticks"));
                if (event.at("kind").get<std::string>() == "rest") {
                    music::array_push(rests, position);
                    continue;
                }
                if (event.at("kind").get<std::string>() != "chord") continue;
                Json pitches = music::make_array<Json>();
                for (const auto &note : event.at("notes").get_array()) music::array_push(pitches, note.at("pitch"));
                Json chord = position;
                music::array_push(chord, pitches);
                music::array_push(notes, chord);
            }
    Json result = music::make_object<Json>();
    result["parts"] = dump.at("parts").size();
    result["measures"] = dump.at("measures").size();
    result["notes"] = std::move(notes);
    result["rests"] = std::move(rests);
    return result;
}

TEST(MnxExport, SchemaAndCoreRoundtrip) {
    const auto references = data.parent_path() / "mscx_reference_examples";
    for (const char *fixture : {"hello-world", "two-bar-c-major-scale", "three-note-chord-and-half-rest", "dotted-notes", "grand-staff",
                                "parts", "multiple-voices", "tuplets"}) {
        SCOPED_TRACE(fixture);
        auto opened = engine->open(references / (std::string(fixture) + "_ref.mscx"));
        ASSERT_TRUE(opened) << opened.error().message;
        const auto before = core_content(**opened);
        ASSERT_FALSE(before.at("notes").empty());
        mu::iex::mnxio::MnxExporter exporter(engrave::DocumentAccess::master_score(**opened));
        const auto result = exporter.export_mnx();
        ASSERT_TRUE(result) << result.to_string();
        const std::string bytes = mu::iex::mnxio::model::write_document(exporter.document());
        // The written document must read back and pass the semantic rules.
        const auto reloaded = mu::iex::mnxio::model::load_document(bytes);
        ASSERT_TRUE(reloaded) << reloaded.error().message;
        if (!mnx_out.empty()) {
            std::ofstream out(mnx_out / (std::string(fixture) + ".mnx"), std::ios::binary);
            out << bytes;
            ASSERT_TRUE(out.good());
        }
        auto imported = engine->open(std::as_bytes(std::span(bytes)), {.format = engrave::Format::MNX});
        ASSERT_TRUE(imported) << imported.error().message;
        EXPECT_TRUE(music::json_equal(core_content(**imported), before));
    }
}

// Every project example, and a percussion score with notes its drumset does not define,
// re-export to a document that loads again.
TEST(MnxExport, ProjectExamplesExportLoadableDocuments) {
    std::vector<std::filesystem::path> inputs;
    for (const auto &entry : std::filesystem::directory_iterator(data))
        if (entry.path().extension() == ".mnx") inputs.push_back(entry.path());
    inputs.push_back(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/named_noteheads_perc.musicxml");
    unsigned count = 0;
    for (const auto &path : inputs) {
        SCOPED_TRACE(path.filename().string());
        auto opened = engine->open(path);
        ASSERT_TRUE(opened) << opened.error().message;
        mu::iex::mnxio::MnxExporter exporter(engrave::DocumentAccess::master_score(**opened));
        ASSERT_TRUE(exporter.export_mnx());
        const auto reloaded = mu::iex::mnxio::model::load_document(mu::iex::mnxio::model::write_document(exporter.document()));
        EXPECT_TRUE(reloaded) << (reloaded ? std::string() : reloaded.error().message);
        ++count;
    }
    EXPECT_GE(count, 30U);
}

// A tuplet MuseScore cannot represent is imported without it (it used to unbalance the importer).
TEST(MnxImport, UnrepresentableTuplet) {
    const std::string input =
        R"({"mnx":{"version":1},"global":{"measures":[{"time":{"count":4,"unit":4}}]},"parts":[{"measures":[{"sequences":[
        {"content":[{"type":"tuplet","inner":{"multiple":2,"duration":{"base":"quarter"}},"outer":{"multiple":1,"duration":{"base":"quarter","dots":5}},
        "content":[{"duration":{"base":"quarter"},"notes":[{"pitch":{"step":"C","octave":5}}]},{"duration":{"base":"quarter"},"notes":[{"pitch":{"step":"D","octave":5}}]}]},
        {"duration":{"base":"half"},"rest":{}},{"duration":{"base":"128th"},"rest":{}}]}]}]}]})";
    EXPECT_TRUE(engine->open(std::as_bytes(std::span(input)), {.format = engrave::Format::MNX}));
}

TEST(MnxImport, InvalidInputAndRecovery) {
    for (const std::string input :
         {"{", "{}", "{\"mnx\":{\"version\":27}}", "{\"mnx\":{\"version\":27},\"global\":{\"measures\":[]},\"parts\":[]}"}) {
        auto opened = engine->open(std::as_bytes(std::span(input)), {.format = engrave::Format::MNX});
        EXPECT_FALSE(opened);
        if (!opened) EXPECT_EQ(opened.error().code, engrave::ErrorCode::INVALID_INPUT);
    }
    EXPECT_TRUE(engine->open(data / "tupletSimple.mnx"));
}
} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    std::filesystem::path assets;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg.starts_with("--assets=")) assets = arg.substr(9);
        if (arg.starts_with("--mnx-out=")) mnx_out = arg.substr(10);
    }
    if (!mnx_out.empty()) std::filesystem::create_directories(mnx_out);
    auto created = engrave::Engine::create({.resources = assets});
    if (!created) {
        std::fprintf(stderr, "%s\n", created.error().message.c_str());
        return 1;
    }
    engine = std::move(*created);
    int result = RUN_ALL_TESTS();
    engine.reset();
    return result;
}
