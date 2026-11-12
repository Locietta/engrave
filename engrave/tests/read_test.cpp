// The read API: outline(), query() and get(), against the score code, which walks
// the same score independently.
//
//   engrave_read_test --assets=<resources> [--corpus=<file or dir>]...
//
// The corpus test compares a query with the semantic dump (engrave/src/score/
// score_semantic.cpp).  The comparison is exact and covers the element kinds both
// walks report: for every score of a document, master and excerpts,
//   - each chord, rest and measure repeat of every staff and voice, with its tick and
//     length in ticks;
//   - each note of those chords, with its pitch;
//   - each time signature, key signature and clef, with its tick, staff and value.
// A query over the same types must give one row per dump row with the same fields and
// no row of its own.  What the dump reports and a query does not is out of scope:
// annotations, spanners, frame elements, and everything layout builds.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/score/json.h"
#include "engrave/src/score/score_metadata.h"
#include "engrave/src/score/score_positions.h"
#include "engrave/src/score/score_semantic.h"
#include "engrave/src/score/write470.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/property.h"
#include "engraving/dom/rangelock.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/infrastructure/eidregister.h"
#include "engraving/rw/write/twrite.h"
#include "engraving/rw/write/writecontext.h"
#include "engraving/rw/xmlwriter.h"
#include "engraving/types/constants.h"
#include "global/io/buffer.h"
#include "importexport/musicxml/internal/musicxmlconfiguration.h"
#include "serialization/xmlstreamreader.h"
#include <engrave/engrave.h>
#include <engrave/testing/write470.h>

namespace {

using engrave::ElementId;
using engrave::ErrorCode;
using engrave::PropertyRead;
using mu::engraving::MasterScore;

// A whole note in the engine's ticks, which is what a Fraction in the API counts.
constexpr std::int64_t whole_note_ticks = 4 * mu::engraving::Constants::division;

std::filesystem::path g_assets;
std::vector<std::filesystem::path> g_corpus;
std::unique_ptr<engrave::Engine> g_engine;

// --- text, bytes and documents -------------------------------------------

std::string read_text(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::byte> as_bytes(std::string_view text) {
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    return {data, data + text.size()};
}

std::unique_ptr<engrave::Document> open_path(const std::filesystem::path &path) {
    auto document = g_engine->open(path);
    EXPECT_TRUE(document) << path.string() << ": " << document.error().message;
    return document ? std::move(*document) : nullptr;
}

MasterScore &master_of(engrave::Document &document) {
    MasterScore *master = engrave::DocumentAccess::master_score(document);
    EXPECT_NE(master, nullptr);
    return *master;
}

std::int64_t ticks_of(const engrave::Fraction &value) { return value.num * whole_note_ticks / value.den; }

std::filesystem::path write470_fixture() {
    return std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/write470/minimal.mscx";
}

// Palette-only elements are ignored by both supported native readers.
TEST(Read, UnknownActionIconIsSkipped) {
    for (const std::string version : {"4.60", "5.00"}) {
        std::string fixture = read_text(write470_fixture());
        fixture.replace(fixture.find("4.70"), 4, version);
        const auto voice = fixture.find("<voice>") + std::string("<voice>").size();
        fixture.insert(voice, "<ActionIcon><action>insert-vbox</action></ActionIcon>");
        auto document = g_engine->open(as_bytes(fixture));
        ASSERT_TRUE(document) << document.error().message;
        auto saved = (*document)->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved) << saved.error().message;
        const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
        EXPECT_EQ(xml.find("<ActionIcon>"), std::string::npos);
    }
}

TEST(Read, NativeViewModesRenderAsPagesAndRoundTrip) {
    auto baseline = g_engine->open(as_bytes(read_text(write470_fixture())));
    ASSERT_TRUE(baseline);
    auto expected = (*baseline)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(expected);
    auto baseline_saved = (*baseline)->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(baseline_saved);
    auto baseline_reopened = g_engine->open(*baseline_saved);
    ASSERT_TRUE(baseline_reopened);
    auto expected_reopened = (*baseline_reopened)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(expected_reopened);
    for (const std::string version : {"4.60", "5.00"}) {
        for (const std::string mode : {"line", "system"}) {
            std::string fixture = read_text(write470_fixture());
            fixture.replace(fixture.find("4.70"), 4, version);
            fixture.insert(fixture.find("<Score>") + 7, "<layoutMode>" + mode + "</layoutMode>");
            auto document = g_engine->open(as_bytes(fixture));
            ASSERT_TRUE(document) << document.error().message;
            auto rendered = (*document)->render({.format = engrave::RenderFormat::SVG});
            ASSERT_TRUE(rendered) << rendered.error().message;
            EXPECT_EQ(rendered->pages, expected->pages);
            auto saved = (*document)->export_score({.format = engrave::Format::MSCX});
            ASSERT_TRUE(saved);
            const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
            EXPECT_NE(xml.find("<layoutMode>" + mode + "</layoutMode>"), std::string::npos);
            auto reopened = g_engine->open(*saved);
            ASSERT_TRUE(reopened);
            auto again = (*reopened)->render({.format = engrave::RenderFormat::SVG});
            ASSERT_TRUE(again);
            EXPECT_EQ(again->pages, expected_reopened->pages);
        }
    }
}

TEST(Read, InstrumentLabelNumberFlags500) {
    const auto path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/upstream-label/show-number-short-500.mscx";
    const std::string fixture = read_text(path);
    constexpr std::string_view short_tag = "<showNumberShort>0</showNumberShort>";
    const auto position = fixture.find(short_tag);
    ASSERT_NE(position, std::string::npos);

    const auto check_flags = [](engrave::Document &document, bool long_flag, bool short_flag) {
        const auto &parts = master_of(document).parts();
        ASSERT_EQ(parts.size(), 1u);
        const auto *instrument = parts.front()->instrument();
        ASSERT_NE(instrument, nullptr);
        const auto &label = instrument->instrument_label();
        EXPECT_EQ(label.number(), 1);
        EXPECT_EQ(label.show_number_long(), long_flag);
        EXPECT_EQ(label.show_number_short(), short_flag);
    };
    const auto check_roundtrip = [&](const std::string &xml, bool long_flag, bool short_flag) {
        auto document = g_engine->open(as_bytes(xml), {.format = engrave::Format::MSCX});
        ASSERT_TRUE(document) << document.error().message;
        check_flags(**document, long_flag, short_flag);
        for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            SCOPED_TRACE(format == engrave::Format::MSCX ? "MSCX" : "MSCZ");
            const auto saved = (*document)->export_score({.format = format, .native_version = "5.00"});
            ASSERT_TRUE(saved) << saved.error().message;
            auto reopened = g_engine->open(*saved, {.format = format});
            ASSERT_TRUE(reopened) << reopened.error().message;
            check_flags(**reopened, long_flag, short_flag);
        }
    };

    // The reported file omits showNumberLong: its default must stay true.
    {
        SCOPED_TRACE("short only; long defaults to true");
        check_roundtrip(fixture, true, false);
    }
    for (bool long_false : {false, true}) {
        SCOPED_TRACE(long_false ? "short omitted; long false" : "both flags omitted");
        std::string xml = fixture;
        xml.replace(position, short_tag.size(), long_false ? "<showNumberLong>0</showNumberLong>" : "");
        check_roundtrip(xml, !long_false, true);
    }
    for (bool long_flag : {false, true}) {
        for (bool short_flag : {false, true}) {
            for (bool short_first : {false, true}) {
                SCOPED_TRACE(std::format("long={}, short={}, short_first={}", long_flag, short_flag, short_first));
                const auto long_xml = std::format("<showNumberLong>{}</showNumberLong>", int(long_flag));
                const auto short_xml = std::format("<showNumberShort>{}</showNumberShort>", int(short_flag));
                std::string xml = fixture;
                xml.replace(position, short_tag.size(), short_first ? short_xml + long_xml : long_xml + short_xml);
                check_roundtrip(xml, long_flag, short_flag);
            }
        }
    }
}

TEST(Export470, SelectedExcerptIsStandaloneAndBounded) {
    auto document = open_path(write470_fixture().parent_path() / "excerpts.mscx");
    ASSERT_TRUE(document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    ASSERT_FALSE(outline->excerpts.empty());
    const engrave::ScoreSel selection{outline->excerpts.front().id};
    auto selected = engrave::select_score(master_of(*document), selection);
    ASSERT_TRUE(selected);
    const auto expected = music::semantic_dump(&master_of(*document))["excerpts"][0]["score"];
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        auto default_bytes = document->export_score({.format = format, .score = selection});
        auto explicit_bytes = document->export_score({.format = format, .score = selection, .native_version = "5.00"});
        ASSERT_TRUE(default_bytes);
        ASSERT_TRUE(explicit_bytes);
        EXPECT_EQ(*default_bytes, *explicit_bytes);
        engrave::ExportOptions options{.format = format, .score = selection, .native_version = "4.70"};
        auto result = document->export_score_with_warnings(options);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_TRUE(result->warnings.empty());
        auto reopened = g_engine->open(result->bytes, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto actual = music::semantic_dump(&master_of(**reopened));
        for (const char *key : {"parts", "measures", "events"}) EXPECT_TRUE(music::json_equal(actual[key], expected[key])) << key;
        EXPECT_TRUE(actual["excerpts"].empty());
        options.capture.max_bytes = result->bytes.size() - 1;
        const auto limited = document->export_score_with_warnings(options);
        ASSERT_FALSE(limited);
        EXPECT_EQ(limited.error().code, ErrorCode::RESOURCE_LIMIT);
    }
    // Selection still goes through the structural guard.
    ASSERT_TRUE(document->edit({{engrave::SetStyle{selection, "staffDistance", 9.0}}}));
    auto refused = document->export_score_with_warnings({.score = selection, .native_version = "4.70"});
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code, ErrorCode::UNSUPPORTED);
    ASSERT_FALSE(refused.error().export_warnings.empty());
    EXPECT_EQ(refused.error().export_warnings.front().code, "B470_UNSUPPORTED_FEATURE");
}

TEST(Export470, WarningIdentifiersAreStableAndDistinct) {
    using namespace engrave::export_warning_code;
    const std::array<std::string_view, 8> actual{shared_part,           stave_sharing_label,   page_lock,
                                                 group_bracket,         instrument_group_name, invalid_tie,
                                                 unsupported_construct, unsupported_target};
    const std::array<std::string_view, 8> expected{"B470_SHARED_PART_DROPPED",
                                                   "B470_STAVE_SHARING_LABEL_DROPPED",
                                                   "B470_PAGE_LOCK_DROPPED",
                                                   "B470_GROUP_BRACKET_DROPPED",
                                                   "B470_INSTRUMENT_GROUP_NAME_DROPPED",
                                                   "B470_INVALID_TIE",
                                                   "B470_UNSUPPORTED_FEATURE",
                                                   "B470_UNSUPPORTED_TARGET"};
    EXPECT_EQ(actual, expected);
}

TEST(Export470, SelectsVersionsAndPreservesTheDefaultWriter) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        auto tick = std::time(nullptr) / 2;
        auto original = document->export_score({.format = format});
        auto explicit_500 = document->export_score_with_warnings({.format = format, .native_version = "5.00"});
        // ZIP timestamps have two-second precision, as in the existing capture tests.
        for (int attempt = 0; attempt < 2 && format == engrave::Format::MSCZ && tick != std::time(nullptr) / 2; ++attempt) {
            tick = std::time(nullptr) / 2;
            original = document->export_score({.format = format});
            explicit_500 = document->export_score_with_warnings({.format = format, .native_version = "5.00"});
        }
        ASSERT_TRUE(original);
        ASSERT_TRUE(explicit_500);
        EXPECT_TRUE(explicit_500->warnings.empty());
        EXPECT_EQ(*original, explicit_500->bytes);
        const auto invalid = document->export_score_with_warnings({.format = format, .native_version = "4.71"});
        ASSERT_FALSE(invalid);
        EXPECT_EQ(invalid.error().code, ErrorCode::UNSUPPORTED);
        EXPECT_NE(invalid.error().message.find("4.71"), std::string::npos);
    }
    const auto non_native = document->export_score_with_warnings({.format = engrave::Format::MUSIC_XML, .native_version = "4.70"});
    ASSERT_FALSE(non_native);
    EXPECT_EQ(non_native.error().code, ErrorCode::UNSUPPORTED);
}

// The view mode is a score-level tag that 4.70 writes the same way; the engine lays
// out pages regardless, but keeps the tag so a MuseScore user gets the view back.
TEST(Export470, KeepsTheViewMode) {
    for (const std::string mode : {"line", "system"}) {
        std::string fixture = read_text(write470_fixture());
        fixture.insert(fixture.find("<Score>") + 7, "<layoutMode>" + mode + "</layoutMode>");
        auto document = g_engine->open(as_bytes(fixture));
        ASSERT_TRUE(document) << document.error().message;
        const auto xml = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        if (!xml) {
            for (const auto &w : xml.error().export_warnings) ADD_FAILURE() << w.code << ": " << w.message;
            FAIL() << mode << ": " << xml.error().message;
        }
        EXPECT_TRUE(xml->warnings.empty());
        const std::string text(reinterpret_cast<const char *>(xml->bytes.data()), xml->bytes.size());
        EXPECT_NE(text.find("<layoutMode>" + mode + "</layoutMode>"), std::string::npos);
    }
}

TEST(Export470, ConvertsInstrumentNamesWithoutWarnings) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    const auto revision = document->revision();
    const auto xml = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    if (!xml) {
        for (const auto &w : xml.error().export_warnings) ADD_FAILURE() << w.code << ": " << w.message;
        FAIL() << xml.error().message;
    }
    EXPECT_TRUE(xml->warnings.empty());
    const std::string text(reinterpret_cast<const char *>(xml->bytes.data()), xml->bytes.size());
    EXPECT_NE(text.find("version=\"4.70\""), std::string::npos);
    EXPECT_EQ(text.find("<InstrumentLabel>"), std::string::npos);
    EXPECT_NE(text.find("<longName>Piano</longName>"), std::string::npos);
    EXPECT_NE(text.find("<shortName>Pno.</shortName>"), std::string::npos);
    const auto archive = document->export_score_with_warnings({.format = engrave::Format::MSCZ, .native_version = "4.70"});
    if (!archive) FAIL() << archive.error().message;
    EXPECT_TRUE(archive->warnings.empty());
    const auto reopened = g_engine->open(archive->bytes, {.format = engrave::Format::MSCZ});
    ASSERT_TRUE(reopened);
    EXPECT_EQ(document->revision(), revision);
    const auto legacy = document->export_score({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(legacy);
    const auto bounded =
        document->export_score_with_warnings({.format = engrave::Format::MSCX,
                                              .capture = {.max_bytes = xml->bytes.size() + 1, .max_working_bytes = 16 * 1024 * 1024},
                                              .native_version = "4.70"});
    ASSERT_TRUE(bounded);
    EXPECT_EQ(bounded->bytes, xml->bytes);
    EXPECT_TRUE(bounded->warnings.empty());
    const auto too_small = document->export_score_with_warnings(
        {.format = engrave::Format::MSCX, .capture = {.max_bytes = 1024, .max_working_bytes = 16 * 1024 * 1024}, .native_version = "4.70"});
    ASSERT_FALSE(too_small);
    EXPECT_EQ(too_small.error().code, ErrorCode::RESOURCE_LIMIT);
    const auto stopped = document->export_score_with_warnings(
        {.format = engrave::Format::MSCZ, .capture = {.cancelled = [] { return true; }}, .native_version = "4.70"});
    ASSERT_FALSE(stopped);
    EXPECT_EQ(stopped.error().code, ErrorCode::CANCELLED);
}

TEST(Export470, InterruptsConversionAndSerializationAndRecovers) {
    using Progress = engrave::testing::Write470Progress;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    const auto baseline = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(baseline);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        for (auto stage : {Progress::Stage::CONVERTING, Progress::Stage::SERIALIZING}) {
            Progress observed;
            const auto result =
                document->export_score_with_warnings({.format = format,
                                                      .capture = {.cancelled =
                                                                      [&] {
                                                                          const auto progress = engrave::testing::write470_progress();
                                                                          if (progress.stage == stage && progress.units >= 2) {
                                                                              observed = progress;
                                                                              return true;
                                                                          }
                                                                          return false;
                                                                      }},
                                                      .native_version = "4.70"});
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error().code, ErrorCode::CANCELLED);
            EXPECT_EQ(observed.stage, stage);
            EXPECT_GE(observed.units, 2u);
            EXPECT_EQ(engrave::testing::write470_progress().stage, Progress::Stage::NONE);
        }
        // Determine a working allowance reached *inside* serialization, then
        // replay with that allowance. Earlier phases fit; the next growth fails.
        std::uint64_t allowance = 0;
        const auto measured = document->export_score_with_warnings(
            {.format = format,
             .capture = {.cancelled =
                             [&] {
                                 const auto p = engrave::testing::write470_progress();
                                 if (!allowance && p.stage == Progress::Stage::SERIALIZING && p.units >= 2) allowance = p.working_bytes;
                                 return false;
                             },
                         .max_working_bytes = 128 * 1024 * 1024},
             .native_version = "4.70"});
        ASSERT_TRUE(measured);
        ASSERT_GT(allowance, 0u);
        Progress last;
        const auto limited = document->export_score_with_warnings({.format = format,
                                                                   .capture = {.cancelled =
                                                                                   [&] {
                                                                                       last = engrave::testing::write470_progress();
                                                                                       return false;
                                                                                   },
                                                                               .max_working_bytes = allowance},
                                                                   .native_version = "4.70"});
        ASSERT_FALSE(limited);
        EXPECT_EQ(limited.error().code, ErrorCode::RESOURCE_LIMIT);
        EXPECT_EQ(last.stage, Progress::Stage::SERIALIZING);
        EXPECT_GE(last.units, 2u);
        const auto recovery = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(recovery);
        EXPECT_EQ(recovery->bytes, baseline->bytes);
    }
}

TEST(Export470, ConvertsPageLocksWithStableObjectWarnings) {
    auto source = read_text(write470_fixture());
    const auto start = source.find("<Measure>");
    const auto end = source.find("</Measure>", start);
    ASSERT_NE(start, std::string::npos);
    ASSERT_NE(end, std::string::npos);
    std::string second = source.substr(start, end + std::string_view("</Measure>").size() - start);
    for (const auto &[from, to] : {std::pair{"C_C", "H_H"}, {"D_D", "I_I"}, {"E_E", "J_J"}, {"F_F", "K_K"}, {"G_G", "L_L"}}) {
        const auto pos = second.find(from);
        ASSERT_NE(pos, std::string::npos);
        second.replace(pos, std::char_traits<char>::length(from), to);
    }
    source.insert(end + std::string_view("</Measure>").size(), second);
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto document = std::move(*opened);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    ASSERT_NE(measure, nullptr);
    auto *next = measure->next_measure();
    ASSERT_NE(next, nullptr);
    const auto id = engrave::from_engine(measure->eid());
    const auto next_id = engrave::from_engine(next->eid());
    mu::engraving::RangeLock lock(measure, measure);
    mu::engraving::RangeLock next_lock(next, next);
    master.add_page_lock(&lock);
    master.add_page_lock(&next_lock);

    struct RemoveLock {
        MasterScore &score;
        const mu::engraving::RangeLock *first;
        const mu::engraving::RangeLock *second;

        ~RemoveLock() {
            score.remove_page_lock(first);
            score.remove_page_lock(second);
        }
    } remove_lock{master, &lock, &next_lock};

    for (int repeat = 0; repeat != 2; ++repeat) {
        const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCZ, .native_version = "4.70"});
        ASSERT_TRUE(result);
        ASSERT_EQ(result->warnings.size(), 2u);
        EXPECT_EQ(result->warnings[0].code, "B470_PAGE_LOCK_DROPPED");
        ASSERT_TRUE(result->warnings[0].object);
        EXPECT_EQ(result->warnings[0].object->id, engrave::to_string(id) + ".." + engrave::to_string(id));
        EXPECT_EQ(result->warnings[1].code, "B470_PAGE_LOCK_DROPPED");
        ASSERT_TRUE(result->warnings[1].object);
        EXPECT_EQ(result->warnings[1].object->id, engrave::to_string(next_id) + ".." + engrave::to_string(next_id));
        EXPECT_EQ(master.page_locks()->all_locks().size(), 2u);
    }
}

TEST(Export470, RefusesANonNeutralFiveOnlyStyle) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto score_id = engrave::from_engine(master.eid());
    master.style().set(mu::engraving::Sid::MIN_SYSTEM_DISTANCE, mu::engraving::Spatium(99.0));
    const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::UNSUPPORTED);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    const auto &warning = result.error().export_warnings.front();
    EXPECT_EQ(warning.code, "B470_UNSUPPORTED_FEATURE");
    EXPECT_NE(result.error().message.find("minSystemDistance"), std::string::npos);
    ASSERT_TRUE(warning.object);
    EXPECT_EQ(warning.object->id, engrave::to_string(score_id));
    EXPECT_DOUBLE_EQ(master.style().style_s(mu::engraving::Sid::MIN_SYSTEM_DISTANCE).val(), 99.0);
}

TEST(Export470, DeduplicatesUnmappedStylesWithoutPublishing) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.style().set(mu::engraving::Sid::MIN_SYSTEM_DISTANCE, mu::engraving::Spatium(99.0));
    master.style().set(mu::engraving::Sid::MAX_SYSTEM_DISTANCE, mu::engraving::Spatium(100.0));
    const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::UNSUPPORTED);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    EXPECT_EQ(result.error().export_warnings.front().code, "B470_UNSUPPORTED_FEATURE");
    EXPECT_NE(result.error().message.find("minSystemDistance"), std::string::npos);
    EXPECT_NE(result.error().message.find("maxSystemDistance"), std::string::npos);
}

TEST(Export470, StaveLabelsSurviveDefaultExportAndReopen) {
    auto document = open_path(write470_fixture().parent_path() / "stave_labels.source.mscx");
    ASSERT_TRUE(document);
    for (int pass = 0; pass < 2; ++pass) {
        auto saved = document->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        const std::string text(reinterpret_cast<const char *>(saved->data()), saved->size());
        for (auto value : {"<StaveSharingLabel>", "<eid>H_H</eid>", "<eid>I_I</eid>", "Players 1", "Players 2"})
            EXPECT_NE(text.find(value), std::string::npos) << value;
        const auto downgraded = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(downgraded);
        const auto &warnings = downgraded->warnings;
        ASSERT_EQ(warnings.size(), 2u);
        const std::string legacy(reinterpret_cast<const char *>(downgraded->bytes.data()), downgraded->bytes.size());
        EXPECT_EQ(legacy.find("StaveSharingLabel"), std::string::npos);
        EXPECT_EQ(warnings[0].code, "B470_STAVE_SHARING_LABEL_DROPPED");
        EXPECT_EQ(warnings[1].code, "B470_STAVE_SHARING_LABEL_DROPPED");
        EXPECT_EQ(warnings[0].object->id, "H_H");
        EXPECT_EQ(warnings[1].object->id, "I_I");
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        document = std::move(*reopened);
    }
}

TEST(Export470, PitchEditsKeepPairedTiesAndRefuseOneEnd) {
    auto document = open_path(write470_fixture().parent_path() / "ties.mscx");
    ASSERT_TRUE(document);
    const auto first = engrave::element_id_from_string("G_G");
    const auto last = engrave::element_id_from_string("J_J");
    ASSERT_TRUE(first);
    ASSERT_TRUE(last);
    ASSERT_TRUE(document->edit({{engrave::SetPitch{*first, 62, 16}}}));
    auto rejected = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(rejected);
    ASSERT_EQ(rejected.error().export_warnings.size(), 1u);
    EXPECT_EQ(rejected.error().export_warnings[0].code, "B470_INVALID_TIE");
    ASSERT_TRUE(document->edit({{engrave::SetPitch{*last, 62, 16}}}));
    auto paired = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(paired) << paired.error().message;
    EXPECT_TRUE(paired->warnings.empty());
}

TEST(Export470, ConvertsConnectionsAndRefusesUnequalPitches) {
    for (auto name : {"ties", "ties_paired_edit", "spanners", "brackets"}) {
        auto document = open_path(write470_fixture().parent_path() / (std::string(name) + ".mscx"));
        ASSERT_TRUE(document);
        const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        if (!result)
            for (const auto &warning : result.error().export_warnings) ADD_FAILURE() << warning.message;
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_TRUE(result->warnings.empty());
    }
    auto document = open_path(write470_fixture().parent_path() / "ties_mismatched.mscx");
    ASSERT_TRUE(document);
    const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    EXPECT_EQ(result.error().export_warnings[0].code, "B470_INVALID_TIE");
    EXPECT_EQ(result.error().export_warnings[0].object->id, "H_H");
}

TEST(Export470, HiddenPartTiesAreRefusedInBothFormats) {
    auto document = open_path(write470_fixture().parent_path() / "ties_hidden.mscx");
    ASSERT_TRUE(document);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().export_warnings.size(), 1u);
        EXPECT_EQ(result.error().export_warnings[0].code, "B470_INVALID_TIE");
        EXPECT_EQ(result.error().export_warnings[0].object, (engrave::ExportWarningObject{"tie", "H_H"}));
    }
}

struct WarningIdentity : testing::TestWithParam<std::optional<engrave::ExportWarningObject>> {};

TEST_P(WarningIdentity, InvalidArgumentWithoutArtifact) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        engrave::ExportWarning warning{"B470_UNSUPPORTED_FEATURE", "test invalid identity", std::nullopt};
        warning.object = GetParam();
        engrave::write470::set_warning_identity_test_fault(std::move(warning));
        const auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        // The failed Result has no ExportOutput/bytes alternative: no artifact
        // can be passed to the host for publication, nor an anonymous warning.
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_TRUE(result.error().export_warnings.empty());
        // Injection is consumed once; the same document still exports normally.
        EXPECT_TRUE(document->export_score_with_warnings({.format = format, .native_version = "4.70"}));
    }
}

INSTANTIATE_TEST_SUITE_P(
    Malformed, WarningIdentity,
    testing::Values(std::nullopt, engrave::ExportWarningObject{"", "H_H"}, engrave::ExportWarningObject{"tie", ""},
                    engrave::ExportWarningObject{"tie", engrave::to_string(engrave::ElementId{UINT64_MAX, UINT64_MAX})},
                    engrave::ExportWarningObject{"page_lock", "A.."}, engrave::ExportWarningObject{"group_bracket", "B:"},
                    engrave::ExportWarningObject{"group_bracket", ":1"}, engrave::ExportWarningObject{"tie", "not-an-identity"},
                    engrave::ExportWarningObject{"tie", "locator:score:0/measure:0/tick:0/track:0/tieFor"},
                    engrave::ExportWarningObject{"tie", "locator:garbage"},
                    engrave::ExportWarningObject{"tie", "locator:score:0/measure:0/tick:0/track:0/note:/tieFor"},
                    engrave::ExportWarningObject{"tie", "locator:score:0/measure:0/tick:0/track:0/note:0/tieFor/extra"}));

TEST(Export470, MissingTieIdNamesTheStartNote) {
    auto source = read_text(write470_fixture().parent_path() / "ties_mismatched.mscx");
    source.erase(source.find("<eid>H_H</eid>"), std::string("<eid>H_H</eid>").size());
    auto document = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(document);
    auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    EXPECT_EQ(result.error().export_warnings[0].object, (engrave::ExportWarningObject{"note", "G_G"}));
    // Exercise the explicit non-EID locator when no owner is addressable.
    auto &master = master_of(**document);
    for (auto *object : engrave::document_objects(master))
        if (object->eid().is_valid()) master.eid_register()->remove_item(object);
    auto first = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    auto second = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(first);
    ASSERT_FALSE(second);
    EXPECT_EQ(first.error().export_warnings[0].object, second.error().export_warnings[0].object);
    EXPECT_EQ(first.error().export_warnings[0].object->id, "locator:score:0/measure:0/tick:0/track:0/note:0/tieFor");
}

TEST(Export470, HiddenExcerptAndMasterStavesAreBothChecked) {
    auto document = open_path(write470_fixture().parent_path() / "excerpts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    std::size_t ties = 0;
    for (auto *object : engrave::document_objects(master)) {
        if (!object->is_note()) continue;
        auto *tie = static_cast<mu::engraving::Note *>(object)->tie_for();
        if (!tie || !tie->end_note()) continue;
        ++ties;
        tie->end_note()->set_pitch(tie->start_note()->pitch() + 2);
        tie->start_note()->staff()->set_visible(false);
    }
    ASSERT_EQ(ties, 4u);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().export_warnings.size(), ties);
        for (const auto &warning : result.error().export_warnings) EXPECT_EQ(warning.code, "B470_INVALID_TIE");
    }
}

TEST(Export470, EnabledFluteGroupingIsWarnedAndReopensDisabled) {
    auto document = open_path(write470_fixture().parent_path() / "four_flutes.mscx");
    ASSERT_TRUE(document);
    ASSERT_TRUE(master_of(*document).style().style_b(mu::engraving::Sid::WINDS_NAME_BY_GROUP));
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_TRUE(result) << result.error().message;
        ASSERT_EQ(result->warnings.size(), 4u);
        for (const auto &warning : result->warnings) EXPECT_EQ(warning.code, "B470_INSTRUMENT_GROUP_NAME_DROPPED");
        auto reopened = g_engine->open(result->bytes, {.format = format});
        ASSERT_TRUE(reopened);
        EXPECT_FALSE(master_of(**reopened).style().style_b(mu::engraving::Sid::WINDS_NAME_BY_GROUP));
        auto bytes_only = document->export_score({.format = format, .native_version = "4.70"});
        EXPECT_FALSE(bytes_only);
    }
}

TEST(Export470, PartNumericIdsSurviveButPartEidsHaveNoLegacyField) {
    auto document = open_path(write470_fixture().parent_path() / "shared_parts.source.mscx");
    ASSERT_TRUE(document);
    auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(result);
    const std::string xml(reinterpret_cast<const char *>(result->bytes.data()), result->bytes.size());
    for (const auto *id : {"1", "2", "3", "4"}) EXPECT_NE(xml.find(std::string("<Part id=\"") + id + "\">"), std::string::npos);
    for (const auto *id : {"P_P", "Q_Q", "R_R", "S_S"}) EXPECT_EQ(xml.find(std::string("<eid>") + id + "</eid>"), std::string::npos);
    auto reopened = g_engine->open(result->bytes, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    EXPECT_EQ(master_of(**reopened).parts().size(), 4u);
}

/// Checks the error a call must fail with, without reading .error() of a success.
template <typename T>
void expect_code(const engrave::Result<T> &result, ErrorCode expected, std::string_view what) {
    if (result) {
        ADD_FAILURE() << what << ": the call succeeded";
        return;
    }
    EXPECT_EQ(result.error().code, expected) << what << ": " << result.error().message;
}

// --- transactions --------------------------------------------------------

/// One transaction on the score, so that an engine command may run (the batch API is
/// S6's; a read test drives the primitive the same way the handle tests do).
struct Command {
    explicit Command(MasterScore &score) : score_(score) {
        mu::engraving::MScore::set_error(mu::engraving::MsError::MS_NO_ERROR);
        score_.start_cmd(muse::TranslatableString::untranslatable("engrave read test"));
    }

    ~Command() {
        if (open_) score_.end_cmd(false, true);
    }

    Command(const Command &) = delete;
    Command &operator=(const Command &) = delete;

    void commit() {
        score_.end_cmd(false, true);
        open_ = false;
        engrave::assign_missing_ids(score_);
        score_.undo_stack()->clear_history();
    }

private:
    MasterScore &score_;
    bool open_ = true;
};

// --- rows of the oracle comparison ---------------------------------------

// One element of the comparison, as a line so that a difference prints readably.
using Row = std::string;

const char *type_name(const music::Json &kind) {
    const std::string value = kind.get<std::string>();
    if (value == "chord") return "Chord";
    if (value == "rest") return "Rest";
    if (value == "measureRepeat") return "MeasureRepeat";
    return "unknown";
}

Row event_row(std::string_view type, int tick, std::size_t staff, std::size_t voice, int ticks) {
    return std::format("{} tick={} staff={} voice={} ticks={}", type, tick, staff, voice, ticks);
}

Row note_row(int tick, std::size_t staff, std::size_t voice, int pitch) {
    return std::format("Note tick={} staff={} voice={} pitch={}", tick, staff, voice, pitch);
}

std::string value_text(const music::Json &value) { return value.is_string() ? value.get<std::string>() : music::dump_json(value); }

// The rows the semantic dump reports for one score body.
void oracle_rows(const music::Json &body, std::multiset<Row> &rows) {
    const music::Json &events = body.at("events");
    for (std::size_t staff = 0; staff < events.size(); ++staff) {
        for (std::size_t voice = 0; voice < events[staff].size(); ++voice) {
            for (const music::Json &item : events[staff][voice].get_array()) {
                const int tick = item.at("tick").as<int>();
                const int ticks = item.at("ticks").as<int>();
                rows.insert(event_row(type_name(item.at("kind")), tick, staff, voice, ticks));
                if (!item.contains("notes")) continue;
                for (const music::Json &note : item.at("notes").get_array()) {
                    rows.insert(note_row(tick, staff, voice, note.at("pitch").as<int>()));
                }
            }
        }
    }
    for (const music::Json &signature : body.at("signatures").get_array()) {
        const std::string type = signature.at("type").get<std::string>();
        const int tick = signature.at("tick").as<int>();
        const std::size_t staff = signature.at("staff").as<std::size_t>();
        if (type == "Clef") {
            // The clef token depends on the score's concert pitch setting; the tick and
            // the staff are what the walk has to agree on.
            rows.insert(std::format("Clef tick={} staff={}", tick, staff));
            continue;
        }
        rows.insert(std::format("{} tick={} staff={} value={}", type, tick, staff, value_text(signature.at("value"))));
    }
}

const PropertyRead *property(const engrave::ElementInfo &element, std::string_view name) {
    for (const PropertyRead &read : element.props) {
        if (read.name == name) return &read;
    }
    return nullptr;
}

std::int64_t int_property(const engrave::ElementInfo &element, std::string_view name) {
    const PropertyRead *read = property(element, name);
    EXPECT_NE(read, nullptr) << element.type << " " << engrave::to_string(element.id) << " has no " << name;
    if (!read || !std::holds_alternative<std::int64_t>(read->value)) return -1;
    return std::get<std::int64_t>(read->value);
}

// The rows a query reports, in the same form as oracle_rows.
void query_rows(const std::vector<engrave::ElementInfo> &elements, std::multiset<Row> &rows) {
    for (const engrave::ElementInfo &element : elements) {
        const auto tick = static_cast<int>(ticks_of(element.tick));
        const auto staff = static_cast<std::size_t>(element.staff);
        const auto voice = static_cast<std::size_t>(element.voice);
        if (element.type == "Note") {
            rows.insert(note_row(tick, staff, voice, static_cast<int>(int_property(element, "pitch"))));
        } else if (element.type == "Chord" || element.type == "Rest" || element.type == "MeasureRepeat") {
            rows.insert(event_row(element.type, tick, staff, voice, static_cast<int>(ticks_of(element.duration))));
        } else if (element.type == "Clef") {
            rows.insert(std::format("Clef tick={} staff={}", tick, staff));
        } else if (element.type == "KeySig") {
            rows.insert(std::format("KeySig tick={} staff={} value={}", tick, staff, int_property(element, "concertKey")));
        } else if (element.type == "TimeSig") {
            const PropertyRead *read = property(element, "timesig");
            EXPECT_NE(read, nullptr) << "a TimeSig has no timesig";
            const auto &sig = std::get<engrave::Fraction>(read->value);
            rows.insert(std::format("TimeSig tick={} staff={} value={}/{}", tick, staff, sig.num, sig.den));
        } else if (element.type == "Clef") {
            rows.insert(std::format("Clef tick={} staff={}", tick, staff));
        } else {
            ADD_FAILURE() << "the comparison covers no " << element.type;
        }
    }
}

/// Compares two row sets, reporting the rows whose counts differ rather than the whole
/// set.
void expect_same_rows(const std::multiset<Row> &expected, const std::multiset<Row> &actual, std::string_view what) {
    std::map<Row, int> wanted;
    for (const Row &row : expected) ++wanted[row];
    std::map<Row, int> got;
    for (const Row &row : actual) ++got[row];
    int reported = 0;
    for (const auto &[row, count] : wanted) {
        const auto found = got.find(row);
        const int mine = found == got.end() ? 0 : found->second;
        if (mine == count) continue;
        if (reported++ < 20) {
            ADD_FAILURE() << what << ": " << row << " appears " << mine << " time(s) in the query, " << count << " in the oracle";
        }
    }
    for (const auto &[row, count] : got) {
        if (wanted.contains(row)) continue;
        if (reported++ < 20) ADD_FAILURE() << what << ": the query has " << row << " " << count << " time(s), the oracle none";
    }
    if (reported > 20) ADD_FAILURE() << what << ": " << reported - 20 << " more row(s) differ";
}

// --- reading a whole score ----------------------------------------------

constexpr std::string_view compared_types[] = {"Chord", "Rest", "MeasureRepeat", "Note", "Clef", "KeySig", "TimeSig"};
constexpr std::string_view compared_props[] = {"pitch", "timesig", "concertKey"};

std::vector<std::string> names(std::span<const std::string_view> values) {
    std::vector<std::string> result;
    for (std::string_view value : values) result.emplace_back(value);
    return result;
}

/// Every element of the comparison's types in one score, asked for measure window by
/// measure window so that a large score stays under the result cap, and page by page
/// so that paging is part of every corpus run.
std::vector<engrave::ElementInfo> read_all(engrave::Document &document, const engrave::ScoreSel &score, std::size_t measures) {
    std::vector<engrave::ElementInfo> elements;
    for (std::size_t first = 0; first < measures; first += engrave::max_query_measures) {
        engrave::Query request;
        request.score = score;
        request.types = names(compared_types);
        request.props = names(compared_props);
        request.first_measure = static_cast<std::int32_t>(first);
        request.measure_count = static_cast<std::int32_t>(std::min(engrave::max_query_measures, measures - first));
        for (std::size_t cursor = 0;;) {
            request.cursor = cursor;
            auto page = document.query(request);
            if (!page) {
                ADD_FAILURE() << "query: " << page.error().message;
                return elements;
            }
            elements.insert(elements.end(), std::make_move_iterator(page->elements.begin()), std::make_move_iterator(page->elements.end()));
            if (!page->next_cursor) break;
            cursor = *page->next_cursor;
        }
    }
    return elements;
}

// --- corpus choice -------------------------------------------------------

std::filesystem::path corpus_file(std::initializer_list<std::string_view> extensions) {
    for (const auto &path : g_corpus) {
        if (extensions.size() == 0) return path;
        for (std::string_view extension : extensions) {
            if (path.extension() == extension) return path;
        }
    }
    return {};
}

/// The first corpus score that has an element of a type.
std::unique_ptr<engrave::Document> open_with(std::string_view type) {
    for (const auto &path : g_corpus) {
        auto document = g_engine->open(path);
        if (!document) continue;
        engrave::Query request;
        request.types = {std::string(type)};
        const auto found = (*document)->query(request);
        if (found && !found->elements.empty()) return std::move(*document);
    }
    return nullptr;
}

// --- the tests -----------------------------------------------------------

std::uint64_t layout_count(const mu::engraving::Score &score) { return score.layout_state.whole_count + score.layout_state.range_count; }

TEST(Read, GeometryBoundaryReusesFreshLayout) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.invalidate_layout(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(-1, 1));
    const auto count = layout_count(master);
    ASSERT_TRUE(document->render({}));
    EXPECT_EQ(layout_count(master), count + 1);
    for (int i = 0; i < 2; ++i) {
        ASSERT_TRUE(document->render({}));
        ASSERT_TRUE(document->page_count());
        ASSERT_TRUE(document->positions(engrave::PositionKind::MEASURE));
        ASSERT_TRUE(document->query({.types = {"Note"}, .layout_boxes = true}));
        ASSERT_TRUE(document->export_score({.format = engrave::Format::MSCX}));
    }
    EXPECT_EQ(layout_count(master), count + 1);
    ASSERT_TRUE(document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
    const auto after_edit = layout_count(master);
    EXPECT_EQ(after_edit, count + 2); // S1 retains one eager transaction layout.
    ASSERT_TRUE(document->render({}));
    ASSERT_TRUE(document->page_count());
    EXPECT_EQ(layout_count(master), after_edit);
}

TEST(Read, DirtyLayoutRangeSurvivesCommandReset) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_TRUE(document->page_count());
    using mu::engraving::Fraction;
    master.set_layout(Fraction(1, 4), 0);
    master.cmd_state().reset();
    master.set_layout(Fraction(3, 4), 0);
    master.cmd_state().reset();
    EXPECT_TRUE(master.layout_state.dirty);
    EXPECT_EQ(master.layout_state.start, Fraction(1, 4));
    EXPECT_EQ(master.layout_state.end, Fraction(3, 4));
    const auto count = layout_count(master);
    ASSERT_TRUE(document->page_count());
    EXPECT_EQ(layout_count(master), count + 1);
    EXPECT_FALSE(master.layout_state.dirty);
    ASSERT_TRUE(document->page_count());
    EXPECT_EQ(layout_count(master), count + 1);
}

TEST(Read, FailedBatchRestoresPendingLayout) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.invalidate_layout(mu::engraving::Fraction(1, 4), mu::engraving::Fraction(1, 2));
    const auto saved = master.layout_state;
    auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
    hooks.before_step = [](std::size_t step) {
        if (step == 1) throw std::runtime_error("injected failure");
    };
    const auto revision = document->revision();
    EXPECT_FALSE(document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
    hooks = {};
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(master.layout_state.dirty, saved.dirty);
    EXPECT_EQ(master.layout_state.start, saved.start);
    EXPECT_EQ(master.layout_state.end, saved.end);
    const auto count = layout_count(master);
    ASSERT_TRUE(document->page_count());
    EXPECT_EQ(layout_count(master), count + 1);
    ASSERT_TRUE(document->page_count());
    EXPECT_EQ(layout_count(master), count + 1);
}

TEST(Read, LayoutRangeIncludesEveryOverlappingSpanner) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    using mu::engraving::Fraction;
    // Neither endpoint of the union belongs to every spanner. The previous
    // loop discarded the earlier interval's extension on every iteration.
    for (const auto &range : {std::pair{Fraction(0, 1), Fraction(3, 4)}, std::pair{Fraction(1, 4), Fraction(1, 1)}}) {
        auto *hairpin = mu::engraving::Factory::create_hairpin(master.dummy());
        hairpin->set_track(0);
        hairpin->set_tick(range.first);
        hairpin->set_tick2(range.second);
        master.add_spanner(hairpin);
    }
    master.layout_state.dirty = false; // Isolate expansion from insertion dirtiness.
    master.do_layout_range(Fraction(1, 2), Fraction(5, 8));
    EXPECT_EQ(master.layout_state.last_start, Fraction(0, 1));
    EXPECT_GE(master.layout_state.last_end, Fraction(1, 1));
}

TEST(Read, FontRefreshWaitsForGeometryBoundary) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    std::string xml = read_text(*it);
    const auto frame = xml.find("<height>10</height>");
    ASSERT_NE(frame, std::string::npos);
    xml.insert(frame, "<Text><style>title</style><text><font face=\"Petaluma Script\"/>MMMWWWiii012345</text></Text>");
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto *title = master_of(document).get_text(mu::engraving::TextStyleType::TITLE);
    ASSERT_NE(title, nullptr);
    const double before = title->ldata()->bbox().width();
    ASSERT_GT(before, 0.0);
    const auto revision = document.revision();
    ASSERT_TRUE(g_engine->add_font(g_assets / "fonts/petaluma/PetalumaScript.otf"));
    EXPECT_EQ(document.revision(), revision);
    auto &master = master_of(document);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    ASSERT_TRUE(document.edit({}));
    ASSERT_TRUE(engrave::api::BatchGuard(document).run({[](engrave::api::EditContext &) -> engrave::Result<void> { return {}; }},
                                                       engrave::api::Policy::SNAPSHOT, {}));
    ASSERT_TRUE(document.outline());
    const auto notes = document.query({.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(notes);
    ASSERT_FALSE(notes->elements.empty());
    ASSERT_TRUE(document.get(notes->elements.front().id));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    EXPECT_EQ(title->ldata()->bbox().width(), before);
    ASSERT_TRUE(document.render({}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count + 1);
    EXPECT_NE(title->ldata()->bbox().width(), before);
    EXPECT_EQ(document.revision(), revision);
}

TEST(Read, PositionsExpandRepeatsInRawPageUnitsWithoutEditing) {
    constexpr std::string_view xml = R"(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef>
</attributes><barline location="left"><repeat direction="forward"/></barline>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
<barline location="right"><repeat direction="backward"/></barline></measure></part>
</score-partwise>)";
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto revision = document.revision();
    for (const auto kind : {engrave::PositionKind::MEASURE, engrave::PositionKind::SEGMENT}) {
        const auto positions = document.positions(kind);
        ASSERT_TRUE(positions);
        ASSERT_EQ(positions->elements.size(), 1u);
        ASSERT_EQ(positions->events.size(), 2u);
        EXPECT_EQ(positions->elements[0].id, 0);
        EXPECT_EQ(positions->elements[0].page, 0);
        EXPECT_EQ(positions->events[0].elid, 0);
        EXPECT_EQ(positions->events[0].position, 0);
        EXPECT_EQ(positions->events[1].elid, 0);
        EXPECT_EQ(positions->events[1].position, 2000);
        EXPECT_GT(positions->page_width, 9000.0);
        EXPECT_LT(positions->page_width, 11000.0);
        EXPECT_GT(positions->page_height, 12000.0);
        EXPECT_GT(positions->elements[0].sy, 0.0);
        if (kind == engrave::PositionKind::SEGMENT) {
            EXPECT_EQ(positions->elements[0].x, std::trunc(positions->elements[0].x));
            EXPECT_EQ(positions->elements[0].y, std::trunc(positions->elements[0].y));
        }
    }
    EXPECT_EQ(document.revision(), revision);
}

TEST(Read, PositionsUsePageLocalCoordinatesOnLaterPages) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "slurs.mscz"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto pages = document->page_count();
    ASSERT_TRUE(pages);
    ASSERT_GE(*pages, 2);
    const auto positions = document->positions(engrave::PositionKind::MEASURE);
    ASSERT_TRUE(positions);
    EXPECT_NEAR(positions->page_width, 9921.264, 1.0);
    EXPECT_NEAR(positions->page_height, 1417.32, 1.0);
    const auto second_page = std::ranges::find_if(positions->elements, [](const auto &e) { return e.page == 1; });
    ASSERT_NE(second_page, positions->elements.end());
    EXPECT_NEAR(second_page->x, 708.6612, 1.0);
    EXPECT_NEAR(second_page->y, 578.6674, 1.0);
    EXPECT_LT(second_page->y, positions->page_height);
    for (const auto &element : positions->elements) {
        EXPECT_GE(element.page, 0);
        EXPECT_LT(element.page, *pages);
    }
    EXPECT_EQ(document->revision(), 1u);
}

TEST(Read, PositionCaptureLimitsAndCancellation) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    for (const auto kind : {engrave::PositionKind::MEASURE, engrave::PositionKind::SEGMENT}) {
        const auto original = document->positions(kind);
        ASSERT_TRUE(original);
        ASSERT_FALSE(original->elements.empty());
        const auto size = engrave::position_payload_bytes(*original);
        ASSERT_GT(size, 0u);
        const auto rejected = document->positions(kind, {}, {.max_bytes = size - 1});
        expect_code(rejected, ErrorCode::RESOURCE_LIMIT, "position payload limit");
        const auto bounded = document->positions(kind, {}, {.max_bytes = size + 1});
        ASSERT_TRUE(bounded);
        EXPECT_EQ(bounded->page_width, original->page_width);
        EXPECT_EQ(bounded->page_height, original->page_height);
        ASSERT_EQ(bounded->elements.size(), original->elements.size());
        ASSERT_EQ(bounded->events.size(), original->events.size());
        for (std::size_t i = 0; i < bounded->elements.size(); ++i) {
            const auto &a = bounded->elements[i];
            const auto &b = original->elements[i];
            EXPECT_EQ(a.id, b.id);
            EXPECT_EQ(a.page, b.page);
            EXPECT_DOUBLE_EQ(a.x, b.x);
            EXPECT_DOUBLE_EQ(a.y, b.y);
            EXPECT_DOUBLE_EQ(a.sx, b.sx);
            EXPECT_DOUBLE_EQ(a.sy, b.sy);
        }
        for (std::size_t i = 0; i < bounded->events.size(); ++i) {
            EXPECT_EQ(bounded->events[i].elid, original->events[i].elid);
            EXPECT_EQ(bounded->events[i].position, original->events[i].position);
        }
        int checks = 0;
        const auto cancelled = document->positions(kind, {}, {.cancelled = [&] { return ++checks == 3; }});
        expect_code(cancelled, ErrorCode::CANCELLED, "cancelled position capture");
        EXPECT_EQ(checks, 3);
    }
}

TEST(Read, ExcerptPositionsMatchTheLegacyWriterAndSelectionErrors) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    ASSERT_FALSE(outline->excerpts.empty());
    const engrave::ScoreSel excerpt{outline->excerpts.front().id};
    const auto revision = document->revision();
    for (const auto kind : {engrave::PositionKind::MEASURE, engrave::PositionKind::SEGMENT}) {
        const auto positions = document->positions(kind, excerpt);
        ASSERT_TRUE(positions);
        ASSERT_FALSE(positions->elements.empty());
        const auto pages = document->page_count(excerpt);
        ASSERT_TRUE(pages);
        auto selected = engrave::select_score(master_of(*document), excerpt);
        ASSERT_TRUE(selected);
        const auto legacy = music::score_positions(*selected, kind == engrave::PositionKind::SEGMENT);
        EXPECT_DOUBLE_EQ(positions->page_width, legacy.at("pageSize").at("width").as<double>());
        EXPECT_DOUBLE_EQ(positions->page_height, legacy.at("pageSize").at("height").as<double>());
        EXPECT_NEAR(positions->page_width, 9921.2598, 0.1);
        EXPECT_NEAR(positions->page_height, 14031.4961, 0.1);
        ASSERT_EQ(positions->elements.size(), legacy.at("elements").size());
        ASSERT_EQ(positions->events.size(), legacy.at("events").size());
        for (std::size_t i = 0; i < positions->elements.size(); ++i) {
            const auto &element = positions->elements[i];
            const auto &expected = legacy.at("elements")[i];
            EXPECT_EQ(element.id, static_cast<int>(i));
            EXPECT_GE(element.page, 0);
            EXPECT_LT(element.page, *pages);
            EXPECT_DOUBLE_EQ(element.x, expected.at("x").as<double>());
            EXPECT_DOUBLE_EQ(element.y, expected.at("y").as<double>());
            EXPECT_DOUBLE_EQ(element.sx, expected.at("sx").as<double>());
            EXPECT_DOUBLE_EQ(element.sy, expected.at("sy").as<double>());
            EXPECT_EQ(element.page, expected.at("page").as<int>());
            if (kind == engrave::PositionKind::SEGMENT) {
                EXPECT_EQ(element.x, std::trunc(element.x));
                EXPECT_EQ(element.y, std::trunc(element.y));
            }
        }
        for (std::size_t i = 0; i < positions->events.size(); ++i) {
            EXPECT_EQ(positions->events[i].elid, legacy.at("events")[i].at("elid").as<int>());
            EXPECT_EQ(positions->events[i].position, legacy.at("events")[i].at("position").as<int>());
        }
    }
    EXPECT_EQ(document->revision(), revision);
    const ElementId missing{0x1234, 0x5678};
    expect_code(document->positions(engrave::PositionKind::MEASURE, {missing}), ErrorCode::NOT_FOUND, "missing score");
    const auto notes = document->query({.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(notes);
    ASSERT_FALSE(notes->elements.empty());
    expect_code(document->positions(engrave::PositionKind::MEASURE, {notes->elements.front().id}), ErrorCode::INVALID_ARGUMENT,
                "note is not a score");
    expect_code(document->positions(static_cast<engrave::PositionKind>(99)), ErrorCode::INVALID_ARGUMENT, "unknown kind");

    // Keep the removed score alive in the engine's id registry while checking
    // that it is no longer reachable from the document, then restore ownership.
    MasterScore &master = master_of(*document);
    auto *removed = master.excerpts().front();
    master.remove_excerpt(removed);
    expect_code(document->outline(excerpt), ErrorCode::STALE_HANDLE, "stale outline");
    expect_code(document->positions(engrave::PositionKind::MEASURE, excerpt), ErrorCode::STALE_HANDLE, "stale score");
    master.add_excerpt(removed);
}

TEST(Read, PositionsRefuseAPoisonedDocument) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
    hooks.policy = engrave::api::Policy::SNAPSHOT;
    hooks.fail_restore = true;
    hooks.before_step = [](std::size_t) { throw std::runtime_error("injected"); };
    EXPECT_ANY_THROW(std::ignore = document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
    hooks = {};
    expect_code(document->positions(engrave::PositionKind::MEASURE), ErrorCode::POISONED, "poisoned document");
}

TEST(Read, ExcerptExportsReopenAsIndependentScores) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "chordline_path.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto full = document->outline();
    ASSERT_TRUE(full);
    ASSERT_GE(full->parts.size(), 2u);
    const engrave::Batch create{{engrave::GenerateExcerpts{{full->parts[1].id}}}};
    ASSERT_TRUE(document->edit(create));
    const auto with_excerpt = document->outline();
    ASSERT_TRUE(with_excerpt);
    ASSERT_EQ(with_excerpt->excerpts.size(), 1u);
    const engrave::ScoreSel selection{with_excerpt->excerpts.front().id};
    ASSERT_TRUE(document->edit({{engrave::SetStyle{selection, "staffDistance", 9.0}}}));
    const auto source = document->outline(selection);
    ASSERT_TRUE(source);
    ASSERT_EQ(source->parts.size(), 1u);
    const auto source_notes = document->query({.score = selection, .types = {"Note"}, .props = {"pitch"}});
    ASSERT_TRUE(source_notes);
    ASSERT_FALSE(source_notes->elements.empty());
    const auto pitches = [](const engrave::QueryResult &notes) {
        std::vector<std::int64_t> result;
        for (const auto &note : notes.elements) {
            if (!note.props.empty()) result.push_back(std::get<std::int64_t>(note.props.front().value));
        }
        std::ranges::sort(result);
        return result;
    };
    const auto revision = document->revision();
    for (const auto format :
         {engrave::Format::MSCX, engrave::Format::MSCZ, engrave::Format::MUSIC_XML, engrave::Format::MXL, engrave::Format::MIDI}) {
        SCOPED_TRACE(engrave::to_string(format));
        const auto exported = document->export_score({.format = format, .score = selection});
        ASSERT_TRUE(exported);
        if (format == engrave::Format::MSCX) {
            const std::string xml(reinterpret_cast<const char *>(exported->data()), exported->size());
            EXPECT_EQ(xml.find("<linkedTo>"), std::string::npos) << "this one-staff excerpt must not refer to master-score elements";
            const auto bounded =
                document->export_score({.format = format, .score = selection, .capture = {.max_bytes = exported->size() + 1}});
            ASSERT_TRUE(bounded);
            EXPECT_EQ(*bounded, *exported);
            expect_code(document->export_score({.format = format, .score = selection, .capture = {.max_bytes = exported->size() - 1}}),
                        ErrorCode::RESOURCE_LIMIT, "standalone excerpt output limit");
        }
        const auto reopened = g_engine->open(*exported, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        const auto part = (*reopened)->outline();
        ASSERT_TRUE(part);
        EXPECT_EQ(part->parts.size(), 1u);
        EXPECT_TRUE(part->excerpts.empty());
        if (format != engrave::Format::MIDI) {
            EXPECT_EQ(part->parts.front().name, source->parts.front().name);
            EXPECT_EQ(part->measure_count, source->measure_count);
            EXPECT_EQ(part->title, source->title);
        }
        const auto notes = (*reopened)->query({.types = {"Note"}, .props = {"pitch"}});
        ASSERT_TRUE(notes);
        EXPECT_EQ(pitches(*notes), pitches(*source_notes));
        if (format == engrave::Format::MSCX || format == engrave::Format::MSCZ) {
            EXPECT_DOUBLE_EQ(master_of(**reopened).style().style_s(mu::engraving::Sid::STAFF_DISTANCE).val(), 9.0);
            EXPECT_EQ(master_of(**reopened).meta_tag(u"workTitle"), master_of(*document).meta_tag(u"workTitle"));
            EXPECT_EQ(master_of(**reopened).meta_tag(u"partName").to_std_string(), with_excerpt->excerpts.front().name);
        }
    }
    EXPECT_EQ(document->revision(), revision);
    const ElementId missing{0x1234, 0x5678};
    expect_code(document->export_score({.format = engrave::Format::MSCZ, .score = {missing}}), ErrorCode::NOT_FOUND,
                "missing excerpt export");
    const auto master_notes = document->query({.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(master_notes);
    ASSERT_FALSE(master_notes->elements.empty());
    expect_code(document->export_score({.format = engrave::Format::MUSIC_XML, .score = {master_notes->elements.front().id}}),
                ErrorCode::INVALID_ARGUMENT, "note is not an excerpt");
    auto *removed = master_of(*document).excerpts().front();
    master_of(*document).remove_excerpt(removed);
    expect_code(document->export_score({.format = engrave::Format::MSCZ, .score = selection}), ErrorCode::STALE_HANDLE,
                "stale excerpt export");
    master_of(*document).add_excerpt(removed);
}

TEST(Read, MscxMasterExportRetainsStyle) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto original = document->outline();
    ASSERT_TRUE(original);
    ASSERT_FALSE(original->excerpts.empty());
    ASSERT_TRUE(document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
    const auto exported = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(exported);
    const auto reopened = g_engine->open(*exported, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    EXPECT_DOUBLE_EQ(master_of(**reopened).style().style_s(mu::engraving::Sid::STAFF_DISTANCE).val(), 9.0);
    const auto outline = (*reopened)->outline();
    ASSERT_TRUE(outline);
    ASSERT_EQ(outline->excerpts.size(), original->excerpts.size());
    EXPECT_EQ(outline->excerpts.front().name, original->excerpts.front().name);
}

TEST(Read, ScoreCaptureLimitsAndCancellation) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ, engrave::Format::MUSIC_XML, engrave::Format::MXL}) {
        SCOPED_TRACE(engrave::to_string(format));
        auto tick = std::time(nullptr) / 2;
        auto original = document->export_score({.format = format});
        ASSERT_TRUE(original);
        ASSERT_FALSE(original->empty());
        auto bounded = document->export_score({.format = format, .capture = {.max_bytes = original->size() + 1}});
        // ZIP stores the current time in two-second units. Retry a pair that
        // happened to straddle that boundary, so identical writers compare
        // exactly without masking any archive bytes.
        for (int attempt = 0;
             attempt < 2 && (format == engrave::Format::MSCZ || format == engrave::Format::MXL) && tick != std::time(nullptr) / 2;
             ++attempt) {
            tick = std::time(nullptr) / 2;
            original = document->export_score({.format = format});
            ASSERT_TRUE(original);
            bounded = document->export_score({.format = format, .capture = {.max_bytes = original->size() + 1}});
        }
        ASSERT_TRUE(bounded) << bounded.error().message;
        EXPECT_EQ(*bounded, *original);
        const auto rejected = document->export_score({.format = format, .capture = {.max_bytes = original->size() - 1}});
        expect_code(rejected, ErrorCode::RESOURCE_LIMIT, "score output limit");
        const auto workspace = document->export_score({.format = format, .capture = {.max_working_bytes = 64}});
        expect_code(workspace, ErrorCode::RESOURCE_LIMIT, "score workspace limit");
        int checks = 0;
        const auto cancelled = document->export_score({.format = format, .capture = {.cancelled = [&] { return ++checks == 20; }}});
        expect_code(cancelled, ErrorCode::CANCELLED, "cancelled score capture");
        EXPECT_EQ(checks, 20);
    }
    expect_code(document->export_score({.format = engrave::Format::MIDI, .capture = {.max_bytes = 64}}), ErrorCode::UNSUPPORTED,
                "bounded MIDI capture");
    int checks = 0;
    EXPECT_THROW(std::ignore =
                     document->export_score({.format = engrave::Format::MSCZ, .capture = {.cancelled = [&]() -> bool {
                                                                                  if (++checks == 20)
                                                                                      throw std::runtime_error("capture callback failure");
                                                                                  return false;
                                                                              }}}),
                 std::runtime_error);
    EXPECT_EQ(checks, 20);
    EXPECT_TRUE(document->export_score({.format = engrave::Format::MSCZ}));
}

TEST(Read, LargeScoreCaptureStopsBeforeMaterializingTheOutput) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    master_of(*document).set_meta_tag(u"workTitle", muse::String::from_std_string(std::string(512 * 1024, 'x')));
    const auto rejected = document->export_score({.format = engrave::Format::MSCX, .capture = {.max_bytes = 1024}});
    expect_code(rejected, ErrorCode::RESOURCE_LIMIT, "large score output limit");
    const auto success = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(success);
    ASSERT_GT(success->size(), 512u * 1024u);
    const auto bounded = document->export_score({.format = engrave::Format::MSCX, .capture = {.max_bytes = success->size() + 1}});
    ASSERT_TRUE(bounded);
    EXPECT_EQ(*bounded, *success);
    auto tick = std::time(nullptr) / 2;
    auto archive = document->export_score({.format = engrave::Format::MSCZ});
    ASSERT_TRUE(archive);
    auto bounded_archive = document->export_score(
        {.format = engrave::Format::MSCZ, .capture = {.max_bytes = archive->size() + 1, .max_working_bytes = 16 * 1024 * 1024}});
    for (int attempt = 0; attempt < 2 && tick != std::time(nullptr) / 2; ++attempt) {
        tick = std::time(nullptr) / 2;
        archive = document->export_score({.format = engrave::Format::MSCZ});
        ASSERT_TRUE(archive);
        bounded_archive = document->export_score(
            {.format = engrave::Format::MSCZ, .capture = {.max_bytes = archive->size() + 1, .max_working_bytes = 16 * 1024 * 1024}});
    }
    ASSERT_TRUE(bounded_archive);
    EXPECT_EQ(*bounded_archive, *archive);
}

TEST(Read, StandaloneExcerptKeepsLinksBetweenItsOwnStaves) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto full = document->outline();
    ASSERT_TRUE(full);
    ASSERT_FALSE(full->excerpts.empty());
    const engrave::ScoreSel selection{full->excerpts.front().id};
    const auto exported = document->export_score({.format = engrave::Format::MSCX, .score = selection});
    ASSERT_TRUE(exported);
    const std::string xml(reinterpret_cast<const char *>(exported->data()), exported->size());
    std::set<std::string> seen;
    std::size_t cursor = 0, links = 0;
    while (cursor < xml.size()) {
        const auto eid = xml.find("<eid>", cursor);
        const auto link = xml.find("<linkedTo>", cursor);
        if (eid == std::string::npos && link == std::string::npos) break;
        if (eid < link) {
            const auto end = xml.find("</eid>", eid);
            ASSERT_NE(end, std::string::npos);
            seen.insert(xml.substr(eid + 5, end - eid - 5));
            cursor = end + 6;
        } else {
            const auto end = xml.find("</linkedTo>", link);
            ASSERT_NE(end, std::string::npos);
            EXPECT_TRUE(seen.contains(xml.substr(link + 10, end - link - 10))) << "a standalone link points outside the score or forward";
            ++links;
            cursor = end + 11;
        }
    }
    ASSERT_GT(links, 0u);
    const auto reopened = g_engine->open(*exported, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    const auto outline = (*reopened)->outline();
    ASSERT_TRUE(outline);
    ASSERT_EQ(outline->parts.size(), 1u);
    EXPECT_EQ(outline->parts.front().staves.size(), 2u);
    const auto notes = (*reopened)->query({.types = {"Note"}});
    ASSERT_TRUE(notes);
    EXPECT_TRUE(std::ranges::any_of(notes->elements, [](const auto &note) { return note.links.size() >= 2; }));
}

TEST(Read, MatchesTheSemanticOracle) {
    ASSERT_FALSE(g_corpus.empty());
    int scores = 0;
    std::size_t rows = 0;
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        const music::Json dump = music::semantic_dump(&master_of(*document));

        const auto outline = document->outline();
        ASSERT_TRUE(outline) << outline.error().message;
        const std::size_t measures = outline->measures.size();

        std::multiset<Row> expected;
        oracle_rows(dump, expected);
        std::multiset<Row> actual;
        query_rows(read_all(*document, {}, measures), actual);
        expect_same_rows(expected, actual, "the master score");
        rows += expected.size();
        ++scores;

        // The excerpts are separate scores with ids of their own; the dump lists each
        // one's body under its name, in master order.  Two excerpts of one part name are
        // told apart by their order, which both lists keep.
        std::size_t next = 0;
        for (const music::Json &listed : dump.at("excerpts").get_array()) {
            if (!listed.contains("score")) continue;
            const std::string name = listed.at("name").get<std::string>();
            while (next < outline->excerpts.size() && outline->excerpts[next].name != name) ++next;
            if (next == outline->excerpts.size()) {
                ADD_FAILURE() << "the outline has no excerpt named '" << name << "'";
                continue;
            }
            const engrave::ExcerptInfo &excerpt = outline->excerpts[next++];
            const auto excerpt_outline = document->outline(engrave::ScoreSel{excerpt.id});
            ASSERT_TRUE(excerpt_outline) << excerpt_outline.error().message;
            std::multiset<Row> excerpt_expected;
            oracle_rows(listed.at("score"), excerpt_expected);
            std::multiset<Row> excerpt_actual;
            query_rows(read_all(*document, engrave::ScoreSel{excerpt.id}, excerpt_outline->measures.size()), excerpt_actual);
            expect_same_rows(excerpt_expected, excerpt_actual, "the excerpt '" + excerpt.name + "'");
            rows += excerpt_expected.size();
        }
    }
    ASSERT_GT(scores, 0) << "no corpus score opens";
    std::printf("corpus: %d score(s), %zu compared row(s)\n", scores, rows);
    ASSERT_GT(rows, 0u) << "the corpus comparison saw no element";
}

TEST(Read, OutlineDescribesTheScore) {
    ASSERT_FALSE(g_corpus.empty());
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        MasterScore &master = master_of(*document);
        const auto metadata = music::score_metadata(&master, std::vector<music::ScoreMetadataTrack>{});
        const auto outline = document->outline();
        ASSERT_TRUE(outline) << outline.error().message;

        // The metadata fields are the engine's own derivation.
        EXPECT_EQ(outline->title, metadata.at("title").get<std::string>());
        EXPECT_EQ(outline->subtitle, metadata.at("subtitle").get<std::string>());
        EXPECT_EQ(outline->composer, metadata.at("composer").get<std::string>());
        EXPECT_EQ(outline->poet, metadata.at("poet").get<std::string>());
        EXPECT_EQ(outline->timesig, metadata.at("timesig").get<std::string>());
        EXPECT_EQ(outline->mscore_version, metadata.at("mscoreVersion").get<std::string>());
        EXPECT_EQ(outline->file_version, metadata.at("fileVersion").as<int>());
        EXPECT_EQ(outline->pages, metadata.at("pages").as<int>());
        EXPECT_EQ(outline->measure_count, metadata.at("measures").as<int>());
        EXPECT_EQ(outline->key, metadata.at("keysig").as<int>());
        EXPECT_EQ(outline->duration, metadata.at("duration").as<double>());

        // Parts and staves are named as the metadata names them, and every id it hands
        // out resolves.
        const auto &parts = metadata.at("parts");
        ASSERT_EQ(outline->parts.size(), parts.size());
        for (std::size_t i = 0; i < outline->parts.size(); ++i) {
            const engrave::PartInfo &part = outline->parts[i];
            EXPECT_EQ(part.name, parts[i].at("name").get<std::string>());
            EXPECT_EQ(part.instrument_id, parts[i].at("instrumentId").get<std::string>());
            EXPECT_TRUE(document->get(part.id).has_value()) << "the part id does not resolve";
            ASSERT_FALSE(part.staves.empty());
            for (const engrave::StaffInfo &staff : part.staves) {
                EXPECT_TRUE(document->get(staff.id).has_value()) << "the staff id does not resolve";
                EXPECT_EQ(staff.part, static_cast<std::int32_t>(i));
                EXPECT_GE(staff.lines, 0);
            }
        }

        // The measures are the score's own list, in order, and each id names the
        // measure at that index.
        ASSERT_EQ(outline->measures.size(), master.nmeasures());
        for (std::size_t i = 0; i < outline->measures.size(); ++i) {
            const engrave::MeasureInfo &measure = outline->measures[i];
            EXPECT_EQ(measure.index, static_cast<std::int32_t>(i));
            EXPECT_GT(measure.duration.num, 0);
            if (i > 0) EXPECT_LE(ticks_of(outline->measures[i - 1].tick), ticks_of(measure.tick));
            const auto read = document->get(measure.id);
            ASSERT_TRUE(read) << "measure " << i << ": " << read.error().message;
        }

        // The signatures are the dump's, which walks them independently: same type,
        // tick and staff.  The dump's clefs are left out: a clef change is an element
        // of its own that a query reports, and the outline covers time, key and tempo.
        std::multiset<std::string> expected;
        for (const music::Json &signature : music::semantic_dump(&master).at("signatures").get_array()) {
            const std::string type = signature.at("type").get<std::string>();
            if (type == "Clef") continue;
            expected.insert(
                std::format("{} tick={} staff={}", type, signature.at("tick").as<int>(), signature.at("staff").as<std::size_t>()));
        }
        std::multiset<std::string> actual;
        for (const engrave::SignatureInfo &signature : outline->signatures) {
            if (signature.type == "Tempo") continue;
            actual.insert(std::format("{} tick={} staff={}", signature.type, static_cast<int>(ticks_of(signature.tick)), signature.staff));
        }
        EXPECT_EQ(expected, actual) << "the outline's signatures are not the dump's";

        // The tempo markings are the dump's annotations of that type.
        std::multiset<std::string> expected_tempi;
        for (const music::Json &annotation : music::semantic_dump(&master).at("annotations").get_array()) {
            if (annotation.at("type").get<std::string>() != "Tempo") continue;
            expected_tempi.insert(std::format("tick={} bpm={} text={}", annotation.at("tick").as<int>(), annotation.at("bpm").as<double>(),
                                              annotation.at("text").get<std::string>()));
        }
        std::multiset<std::string> actual_tempi;
        for (const engrave::SignatureInfo &signature : outline->signatures) {
            if (signature.type != "Tempo") continue;
            actual_tempi.insert(
                std::format("tick={} bpm={} text={}", static_cast<int>(ticks_of(signature.tick)), signature.bpm, signature.text));
        }
        EXPECT_EQ(expected_tempi, actual_tempi) << "the outline's tempo markings are not the dump's";
    }
}

TEST(Read, FiltersAndPagingAgree) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto measures = static_cast<std::size_t>(outline->measures.size());
    ASSERT_GT(measures, 1u);

    // Every element of the score, in one call.
    engrave::Query all;
    all.limit = engrave::max_query_results;
    const auto full = document->query(all);
    ASSERT_TRUE(full) << full.error().message;
    ASSERT_EQ(full->total, full->elements.size());
    ASSERT_GT(full->total, 20u) << "the corpus score is too small to page";
    EXPECT_FALSE(full->next_cursor);

    const auto ids = [](const std::vector<engrave::ElementInfo> &elements) {
        std::vector<std::string> result;
        for (const auto &element : elements) result.push_back(engrave::to_string(element.id));
        return result;
    };
    const std::vector<std::string> order = ids(full->elements);

    // The same query twice is the same answer, in the same order.
    const auto again = document->query(all);
    ASSERT_TRUE(again) << again.error().message;
    EXPECT_EQ(ids(again->elements), order) << "a repeated query reordered its results";

    // Page by page, the results concatenate to the one call's, in the same order.
    engrave::Query paged;
    paged.limit = 7;
    std::vector<engrave::ElementInfo> pages;
    std::size_t calls = 0;
    for (std::size_t cursor = 0;;) {
        paged.cursor = cursor;
        const auto page = document->query(paged);
        ASSERT_TRUE(page) << page.error().message;
        EXPECT_EQ(page->total, full->total) << "paging changed the total";
        pages.insert(pages.end(), page->elements.begin(), page->elements.end());
        if (!page->next_cursor) break;
        EXPECT_EQ(*page->next_cursor, pages.size()) << "the next cursor is not where the page ended";
        cursor = *page->next_cursor;
        if (++calls > full->total) ADD_FAILURE() << "paging does not end";
    }
    EXPECT_EQ(ids(pages), order) << "the pages are not the whole query";

    // A type filter selects exactly the rows of that type.
    engrave::Query typed;
    typed.types = {"Note"};
    typed.limit = engrave::max_query_results;
    const auto notes = document->query(typed);
    ASSERT_TRUE(notes) << notes.error().message;
    std::size_t notes_in_full = 0;
    for (const auto &element : full->elements) notes_in_full += element.type == "Note";
    EXPECT_EQ(notes->total, notes_in_full);
    for (const auto &element : notes->elements) EXPECT_EQ(element.type, "Note");

    // Adjacent measure windows tile the score.
    std::vector<std::string> tiled;
    for (std::size_t first = 0; first < measures; ++first) {
        engrave::Query window;
        window.first_measure = static_cast<std::int32_t>(first);
        window.measure_count = 1;
        window.limit = engrave::max_query_results;
        const auto part = document->query(window);
        ASSERT_TRUE(part) << part.error().message;
        for (const auto &element : part->elements) {
            EXPECT_EQ(element.measure_index, static_cast<std::int32_t>(first));
            tiled.push_back(engrave::to_string(element.id));
        }
    }
    // Every element the whole-score query reported for a measure is in exactly one
    // window; the score and its parts and staves are in none.
    std::vector<std::string> in_measures;
    for (const auto &element : full->elements) {
        if (element.measure_index >= 0) in_measures.push_back(engrave::to_string(element.id));
    }
    std::ranges::sort(in_measures);
    std::ranges::sort(tiled);
    EXPECT_EQ(tiled, in_measures) << "the measure windows do not tile the score";

    // A staff or a voice filter keeps the elements of that staff or voice and drops
    // the rest.
    if (master_of(*document).nstaves() > 1) {
        engrave::Query staff;
        staff.staves = {1};
        staff.limit = engrave::max_query_results;
        const auto on_staff = document->query(staff);
        ASSERT_TRUE(on_staff) << on_staff.error().message;
        std::size_t expected = 0;
        for (const auto &element : full->elements) expected += element.staff == 1;
        EXPECT_EQ(on_staff->total, expected);
        for (const auto &element : on_staff->elements) EXPECT_EQ(element.staff, 1);
    }
    engrave::Query voice;
    voice.voices = {2};
    voice.limit = engrave::max_query_results;
    const auto in_voice = document->query(voice);
    ASSERT_TRUE(in_voice) << in_voice.error().message;
    for (const auto &element : in_voice->elements) EXPECT_EQ(element.voice, 2);

    // A tick range keeps the elements that start inside it.
    engrave::Query ticks;
    ticks.from_tick = engrave::Fraction{1, 4};
    ticks.to_tick = engrave::Fraction{1, 2};
    ticks.limit = engrave::max_query_results;
    const auto in_ticks = document->query(ticks);
    ASSERT_TRUE(in_ticks) << in_ticks.error().message;
    std::size_t expected = 0;
    for (const auto &element : full->elements) {
        const double start = static_cast<double>(element.tick.num) / static_cast<double>(element.tick.den);
        expected += start >= 0.25 && start < 0.5;
    }
    EXPECT_EQ(in_ticks->total, expected);
}

TEST(Read, ExtremeTickRangesAndMeasureWindows) {
    const auto fixture = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(fixture, g_corpus.end());
    auto document = open_path(*fixture);
    ASSERT_TRUE(document);
    const auto max = std::numeric_limits<std::int64_t>::max();
    const auto min = std::numeric_limits<std::int64_t>::min();

    engrave::Query request;
    request.types = {"Note"};
    const auto all = document->query(request);
    ASSERT_TRUE(all);
    ASSERT_GT(all->total, 0u);

    // Zero with an extreme denominator must select the same notes as zero/1.
    request.from_tick = engrave::Fraction{0, max};
    const auto zero = document->query(request);
    ASSERT_TRUE(zero);
    EXPECT_EQ(zero->total, all->total);
    request.from_tick = engrave::Fraction{min, max};
    request.to_tick = engrave::Fraction{max, 1};
    const auto wide = document->query(request);
    ASSERT_TRUE(wide);
    EXPECT_EQ(wide->total, all->total);
    request.from_tick = engrave::Fraction{max, 1};
    request.to_tick.reset();
    const auto beyond = document->query(request);
    ASSERT_TRUE(beyond);
    EXPECT_EQ(beyond->total, 0u);

    // Bounds within one unit in the last place of 1 straddle exactly the
    // notes at tick 1, even though cross products would require 128 bits.
    request.from_tick = engrave::Fraction{max - 1, max};
    request.to_tick = engrave::Fraction{max, max - 1};
    const auto narrow = document->query(request);
    ASSERT_TRUE(narrow);
    const auto at_one = std::ranges::count_if(all->elements, [](const auto &element) { return element.tick.num == element.tick.den; });
    ASSERT_GT(at_one, 0);
    EXPECT_EQ(narrow->total, static_cast<std::size_t>(at_one));
    std::swap(request.from_tick, request.to_tick);
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "reversed extreme bounds");

    request.from_tick = engrave::Fraction{1, 2};
    request.to_tick.reset();
    const auto reduced = document->query(request);
    ASSERT_TRUE(reduced);
    request.from_tick = engrave::Fraction{(max - 1) / 2, max - 1};
    const auto unreduced = document->query(request);
    ASSERT_TRUE(unreduced);
    EXPECT_EQ(unreduced->total, reduced->total);

    for (const std::int64_t denominator : {std::int64_t{0}, std::int64_t{-1}, min}) {
        request.from_tick = engrave::Fraction{0, denominator};
        expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "invalid lower denominator");
        request.from_tick.reset();
        request.to_tick = engrave::Fraction{0, denominator};
        expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "invalid upper denominator");
        request.to_tick.reset();
    }
    const auto max32 = std::numeric_limits<std::int32_t>::max();
    const auto min32 = std::numeric_limits<std::int32_t>::min();
    for (const auto [first, count] :
         {std::pair{max32, std::int32_t{1}}, std::pair{max32, max32}, std::pair{min32, min32}, std::pair{std::int32_t{0}, min32}}) {
        request.first_measure = first;
        request.measure_count = count;
        expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "out-of-range measure window");
    }
}

TEST(Read, FractionPropertyDefaultsKeepWrittenTerms) {
    const auto fixture = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(fixture, g_corpus.end());
    auto document = open_path(*fixture);
    ASSERT_TRUE(document);
    engrave::Query request;
    request.types = {"TimeSig"};
    const auto signatures = document->query(request);
    ASSERT_TRUE(signatures);
    ASSERT_FALSE(signatures->elements.empty());
    const auto id = signatures->elements.front().id;
    const auto found = engrave::resolve(master_of(*document), id);
    ASSERT_TRUE(found);
    auto *signature = static_cast<mu::engraving::TimeSig *>(found->object);
    signature->set_sig(mu::engraving::Fraction(2, 2));
    const auto written = document->get(id, {"timesig"});
    ASSERT_TRUE(written);
    ASSERT_EQ(written->size(), 1u);
    const auto actual = std::get<engrave::Fraction>(written->front().value);
    EXPECT_EQ(actual.num, 2);
    EXPECT_EQ(actual.den, 2);
    EXPECT_FALSE(written->front().is_default); // the default is written as 4/4

    signature->set_sig(mu::engraving::Fraction(4, 4));
    const auto default_value = document->get(id, {"timesig"});
    ASSERT_TRUE(default_value);
    ASSERT_EQ(default_value->size(), 1u);
    EXPECT_TRUE(default_value->front().is_default);
}

TEST(Read, LimitsAreEnforced) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto measures = static_cast<std::int32_t>(outline->measures.size());

    engrave::Query request;
    request.limit = 0;
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a limit of 0");
    request.limit = engrave::max_query_results + 1;
    expect_code(document->query(request), ErrorCode::RESOURCE_LIMIT, "a limit over the cap");
    request.limit = engrave::max_query_results;
    request.types = {"NoSuchType"};
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "an unknown type");
    request.types = {};
    request.props = {"noSuchProperty"};
    expect_code(document->query(request), ErrorCode::NOT_FOUND, "an unknown property name");
    request.props = {};
    request.staves = {99};
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a staff the score has not");
    request.staves = {};
    request.voices = {4};
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a voice over 3");
    request.voices = {};
    request.first_measure = 0;
    request.measure_count = static_cast<std::int32_t>(engrave::max_query_measures) + 1;
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a window over the measure cap");
    request.measure_count = measures + 1;
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a window past the last measure");
    request.measure_count.reset();
    request.first_measure = measures + 1;
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a window that starts past the end");
    request.first_measure.reset();
    request.from_tick = engrave::Fraction{1, 2};
    request.to_tick = engrave::Fraction{1, 4};
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a reversed tick range");
    request.from_tick.reset();
    request.to_tick.reset();
    request.cursor = 100000;
    expect_code(document->query(request), ErrorCode::INVALID_ARGUMENT, "a cursor past the end");
    request.cursor = 0;

    // A score with more elements than one query may report is a ResourceLimit, not a
    // partial answer.  Measures are appended through the engine's own command; the
    // batch API that would do this for a client is S6's.
    {
        Command command(master);
        for (std::size_t i = 0; i <= engrave::max_query_results; ++i) {
            if (!master.Score::insert_measure(mu::engraving::ElementType::MEASURE, nullptr)) {
                GTEST_SKIP() << "the engine did not append a measure";
            }
        }
        command.commit();
    }
    engrave::Query everything;
    everything.types = {"Measure"};
    const auto too_many = document->query(everything);
    ASSERT_FALSE(too_many) << "a query over " << everything.types.front() << " elements succeeded";
    EXPECT_EQ(too_many.error().code, ErrorCode::RESOURCE_LIMIT);
    EXPECT_FALSE(too_many.error().message.empty());

    // The same score narrowed to a measure window fits again.
    everything.first_measure = 0;
    everything.measure_count = 16;
    const auto window = document->query(everything);
    ASSERT_TRUE(window) << window.error().message;
    EXPECT_LE(window->total, 16u);
}

TEST(Read, GetReadsProperties) {
    auto document = open_with("Dynamic");
    ASSERT_TRUE(document) << "no corpus score has a dynamic";
    MasterScore &master = master_of(*document);

    // A note: the properties the catalogue lists for its type, and pitch among them.
    engrave::Query notes;
    notes.types = {"Note"};
    const auto found = document->query(notes);
    ASSERT_TRUE(found) << found.error().message;
    ASSERT_FALSE(found->elements.empty()) << "no note in the corpus score";
    const ElementId note_id = found->elements.front().id;

    const auto all = document->get(note_id);
    ASSERT_TRUE(all) << all.error().message;
    std::set<std::string> names;
    for (const PropertyRead &read : *all) names.insert(read.name);
    EXPECT_EQ(names.size(), all->size()) << "a property is reported twice";
    EXPECT_TRUE(names.contains("pitch"));
    const auto pitch = document->get(note_id, {"pitch"});
    ASSERT_TRUE(pitch) << pitch.error().message;
    ASSERT_EQ(pitch->size(), 1u);
    EXPECT_EQ((*pitch)[0].name, "pitch");
    EXPECT_TRUE(std::holds_alternative<std::int64_t>((*pitch)[0].value));
    EXPECT_FALSE((*pitch)[0].styled);
    EXPECT_FALSE((*pitch)[0].is_default) << "pitch has no default to be";

    // A dynamic: minDistance is a style's value until the element sets one.
    engrave::Query dynamics;
    dynamics.types = {"Dynamic"};
    const auto dynamic_found = document->query(dynamics);
    ASSERT_TRUE(dynamic_found) << dynamic_found.error().message;
    ASSERT_FALSE(dynamic_found->elements.empty()) << "no dynamic in the corpus score";
    const ElementId dynamic_id = dynamic_found->elements.front().id;
    {
        // Whatever the file holds, start from the style's value.
        Command command(master);
        const auto dynamic = engrave::resolve(master, dynamic_id);
        ASSERT_TRUE(dynamic) << dynamic.error().message;
        dynamic->object->undo_reset_property(mu::engraving::Pid::MIN_DISTANCE);
        command.commit();
    }
    auto styled = document->get(dynamic_id, {"minDistance"});
    ASSERT_TRUE(styled) << styled.error().message;
    ASSERT_EQ(styled->size(), 1u);
    EXPECT_TRUE((*styled)[0].styled) << "minDistance is not reported as styled";
    EXPECT_TRUE((*styled)[0].is_default) << "an untouched style value is not the default";

    {
        Command command(master);
        const auto dynamic = engrave::resolve(master, dynamic_id);
        ASSERT_TRUE(dynamic) << dynamic.error().message;
        dynamic->object->undo_change_property(mu::engraving::Pid::MIN_DISTANCE, mu::engraving::PropertyValue::from_value(2.5),
                                              mu::engraving::PropertyFlags::UNSTYLED);
        command.commit();
    }
    auto changed = document->get(dynamic_id, {"minDistance"});
    ASSERT_TRUE(changed) << changed.error().message;
    EXPECT_FALSE((*changed)[0].styled) << "a value the element sets is still reported as styled";
    EXPECT_FALSE((*changed)[0].is_default) << "a value the element sets is reported as the default";
    EXPECT_EQ(std::get<double>((*changed)[0].value), 2.5);

    {
        Command command(master);
        const auto dynamic = engrave::resolve(master, dynamic_id);
        ASSERT_TRUE(dynamic) << dynamic.error().message;
        dynamic->object->undo_reset_property(mu::engraving::Pid::MIN_DISTANCE);
        command.commit();
    }
    auto reset = document->get(dynamic_id, {"minDistance"});
    ASSERT_TRUE(reset) << reset.error().message;
    EXPECT_TRUE((*reset)[0].styled) << "a reset property did not go back to its style";
    EXPECT_TRUE((*reset)[0].is_default) << "a reset property is not the default again";

    // A property name the catalogue does not know, and one the type does not have.
    const auto unknown = document->get(note_id, {"noSuchProperty"});
    expect_code(unknown, ErrorCode::NOT_FOUND, "an unknown property name");
    const auto foreign = document->get(dynamic_id, {"concertClefType"});
    expect_code(foreign, ErrorCode::INVALID_ARGUMENT, "a property of another type");
    // A reference only a batch can use.
    expect_code(document->get(engrave::OpRef{}), ErrorCode::INVALID_ARGUMENT, "an OpRef");
}

TEST(Read, LayoutBoxesComeFromTheLayout) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    ASSERT_GT(outline->pages, 0);

    engrave::Query request;
    request.types = {"Measure"};
    request.layout_boxes = true;
    const auto measures = document->query(request);
    ASSERT_TRUE(measures) << measures.error().message;
    ASSERT_EQ(measures->total, outline->measures.size());

    std::int32_t page = 0;
    for (const auto &measure : measures->elements) {
        ASSERT_TRUE(measure.box) << "a laid-out measure has no box";
        EXPECT_GE(measure.box->page, page) << "the pages of the measures go backwards";
        page = measure.box->page;
        EXPECT_LT(measure.box->page, outline->pages);
        EXPECT_GT(measure.box->width, 0.0);
        EXPECT_GE(measure.box->height, 0.0);
        EXPECT_GE(measure.box->x, 0.0);
        EXPECT_GE(measure.box->y, 0.0);
    }

    // Without the flag, no box is filled in.
    engrave::Query plain;
    plain.types = {"Measure"};
    const auto bare = document->query(plain);
    ASSERT_TRUE(bare) << bare.error().message;
    for (const auto &measure : bare->elements) EXPECT_FALSE(measure.box);
}

TEST(Read, ErrorsForStaleAndMissingReferences) {
    const auto path = corpus_file({".mscx"});
    ASSERT_FALSE(path.empty());
    auto document = open_path(path);
    ASSERT_TRUE(document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline) << outline.error().message;

    engrave::Query notes;
    notes.types = {"Note"};
    const auto found = document->query(notes);
    ASSERT_TRUE(found) << found.error().message;
    ASSERT_FALSE(found->elements.empty());
    const ElementId note_id = found->elements.front().id;

    // An id that names nothing is NotFound, in get() and in a measure window alike.
    const ElementId missing{0x1234, 0x5678};
    expect_code(document->get(missing), ErrorCode::NOT_FOUND, "an id nothing has");
    engrave::Query window;
    window.from_measure = missing;
    expect_code(document->query(window), ErrorCode::NOT_FOUND, "a measure id nothing has");
    // A selection has to name a score.
    engrave::Query wrong_score;
    wrong_score.score = engrave::ScoreSel{note_id};
    expect_code(document->query(wrong_score), ErrorCode::INVALID_ARGUMENT, "a selection that names a note");
    wrong_score.score = engrave::ScoreSel{missing};
    expect_code(document->query(wrong_score), ErrorCode::NOT_FOUND, "a selection nothing has");
    expect_code(document->outline(engrave::ScoreSel{missing}), ErrorCode::NOT_FOUND, "an outline of nothing");
    expect_code(document->outline(engrave::ScoreSel{note_id}), ErrorCode::INVALID_ARGUMENT, "an outline of a note");
    // A measure window names a measure of this score.
    engrave::Query not_a_measure;
    not_a_measure.from_measure = note_id;
    expect_code(document->query(not_a_measure), ErrorCode::INVALID_ARGUMENT, "a window that names a note");
    engrave::Query both_forms;
    both_forms.first_measure = 0;
    both_forms.from_measure = outline->measures.front().id;
    expect_code(document->query(both_forms), ErrorCode::INVALID_ARGUMENT, "a window named twice");

    // A page, a system and a segment that the layout does not have are stale.
    expect_code(document->get(engrave::Address{engrave::PageAddress{.score = {}, .index = 9999}}), ErrorCode::STALE_HANDLE,
                "a page past the end");
    expect_code(document->get(engrave::Address{engrave::SystemAddress{.score = {}, .page = 9999, .index = 0}}), ErrorCode::STALE_HANDLE,
                "a system on no page");
    expect_code(document->get(engrave::Address{engrave::SegmentAddress{
                    .measure = outline->measures.front().id, .tick = engrave::Fraction{1000, 1}, .kind = "ChordRest"}}),
                ErrorCode::STALE_HANDLE, "a segment at no tick");
    // The first page and the first measure's own segment do resolve.
    const auto first_page = document->get(engrave::Address{engrave::PageAddress{.score = {}, .index = 0}});
    ASSERT_TRUE(first_page) << first_page.error().message;
    EXPECT_FALSE(first_page->empty());
    const auto first_segment = document->get(engrave::Address{
        engrave::SegmentAddress{.measure = outline->measures.front().id, .tick = engrave::Fraction{0, 1}, .kind = "ChordRest"}});
    ASSERT_TRUE(first_segment) << first_segment.error().message;

    // An id of a document that is gone names nothing in another one.
    const auto other = corpus_file({".musicxml"});
    ASSERT_FALSE(other.empty());
    auto second = open_path(other);
    ASSERT_TRUE(second);
    expect_code(second->get(note_id), ErrorCode::NOT_FOUND, "an id of another document");
}

// A leading omitted rest must remain part of the triplet. Otherwise measure
// repair starts its duration at tick 320 and overwrites the A4 at tick 1280.
TEST(Read, HiddenLeadingTupletRestPreservesFollowingNotes) {
    constexpr std::string_view fixture = R"mscx(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice>
<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>quarter</baseNote></Tuplet>
<location><fractions>1/6</fractions></location>
<Chord><durationType>quarter</durationType><Note><pitch>74</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>72</pitch><tpc>14</tpc></Note></Chord>
<endTuplet/>
<Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>quarter</baseNote></Tuplet>
<Chord><durationType>quarter</durationType><Note><pitch>72</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>69</pitch><tpc>17</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>67</pitch><tpc>15</tpc></Note></Chord>
<endTuplet/>
</voice></Measure></Staff></Score></museScore>
)mscx";
    for (const std::string version : {"5.00", "4.60"}) {
        SCOPED_TRACE(version);
        std::string xml(fixture);
        ASSERT_FALSE(xml.empty());
        xml.replace(xml.find("5.00"), 4, version);
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document) << document.error().message;
        engrave::Query query;
        query.types = {"Note"};
        query.props = {"pitch"};
        const auto notes = (*document)->query(query);
        ASSERT_TRUE(notes);
        std::multiset<Row> actual;
        query_rows(notes->elements, actual);
        const std::multiset<Row> expected = {note_row(320, 0, 0, 74), note_row(640, 0, 0, 72), note_row(960, 0, 0, 72),
                                             note_row(1280, 0, 0, 69), note_row(1600, 0, 0, 67)};
        EXPECT_EQ(actual, expected);
        query.types = {"Chord"};
        const auto chords = (*document)->query(query);
        ASSERT_TRUE(chords);
        ASSERT_EQ(chords->elements.size(), 5u);
        for (const auto &chord : chords->elements) EXPECT_EQ(ticks_of(chord.duration), 320);
        query.types = {"Rest"};
        const auto rests = (*document)->query(query);
        ASSERT_TRUE(rests);
        ASSERT_EQ(rests->elements.size(), 1u);
        EXPECT_EQ(ticks_of(rests->elements.front().tick), 0);
        EXPECT_EQ(ticks_of(rests->elements.front().duration), 320);
        const auto saved = (*document)->export_score({.format = engrave::Format::MSCZ});
        ASSERT_TRUE(saved);
        const auto reopened = g_engine->open(*saved);
        ASSERT_TRUE(reopened);
        query.types = {"Note"};
        const auto roundtrip = (*reopened)->query(query);
        ASSERT_TRUE(roundtrip);
        actual.clear();
        query_rows(roundtrip->elements, actual);
        EXPECT_EQ(actual, expected);
    }
}

std::string dynamics_musicxml(std::string_view direction, std::string_view notations, std::string_view note_attributes = "") {
    return std::format(R"xml(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef>
</attributes>{}<note {}><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration>
<type>whole</type><notations>{}</notations></note></measure></part></score-partwise>)xml",
                       direction, note_attributes, notations);
}

std::vector<mu::engraving::Dynamic *> imported_dynamics(engrave::Document &document) {
    engrave::Query query;
    query.types = {"Dynamic"};
    const auto result = document.query(query);
    EXPECT_TRUE(result);
    std::vector<mu::engraving::Dynamic *> dynamics;
    if (!result) return dynamics;
    for (const auto &row : result->elements) {
        const auto element = engrave::resolve(master_of(document), row.id);
        EXPECT_TRUE(element);
        if (element) dynamics.push_back(static_cast<mu::engraving::Dynamic *>(element->object));
    }
    return dynamics;
}

// Parse the exported document before reopening it: successful import alone does
// not establish that literal text was serialized as well-formed XML.
std::unique_ptr<engrave::Document> dynamics_native_roundtrip(engrave::Document &document, std::string_view expected_text = "",
                                                             engrave::Format format = engrave::Format::MSCX) {
    const auto saved = document.export_score({.format = format});
    EXPECT_TRUE(saved);
    if (!saved) return nullptr;
    if (format == engrave::Format::MSCX && !expected_text.empty()) {
        const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
        EXPECT_NE(xml.find(std::format("<text>{}</text>", expected_text)), std::string::npos);
    }
    if (format == engrave::Format::MSCX) {
        const muse::ByteArray bytes(reinterpret_cast<const char *>(saved->data()), saved->size());
        muse::XmlStreamReader reader(bytes);
        while (!reader.at_end()) reader.read_next();
        EXPECT_FALSE(reader.is_error()) << reader.error_string().to_std_string();
    }
    auto reopened = g_engine->open(*saved, {.format = format});
    EXPECT_TRUE(reopened);
    return reopened ? std::move(*reopened) : nullptr;
}

TEST(Read, MusicXmlDynamicsVelocityIsBoundedBeforeConversion) {
    struct Case {
        const char *value;
        int direction;
        int note;
    };

    const std::string canceled = "0." + std::string(1000, '0') + "5e1001";
    const Case cases[] = {
        {"0", 96, 1},
        {"-10", 96, 1},
        {"5", 5, 5},
        {"50", 45, 45},
        {"100", 90, 90},
        {"110", 99, 99},
        {"+5e1", 45, 45},
        {" 50 ", 45, 45},
        {".5", 96, 1},
        {"0.6", 1, 1},
        {"50.6", 46, 46},
        {"200", 127, 127},
        {"10000000000", 127, 127},
        {"1e308", 127, 127},
        {"-1e308", 96, 1},
        {"garbage", 96, 1},
        {"50garbage", 96, 1},
        {"", 96, 1},
        {"NaN", 96, 1},
        {"INF", 96, 1},
        {"-INF", 96, 1},
        {"1e999", 127, 127},
        {"-1e999", 96, 1},
        {"1e-999", 96, 1},
        {"1e9999999999999999999", 127, 127},
        {"1e-9999999999999999999", 96, 1},
        {"1e+", 96, 1},
        {"+-1", 96, 1},
        {"5 0", 96, 1},
        {"&#x9;50&#xA;", 45, 45},
        {canceled.c_str(), 5, 5},
    };
    for (const auto &c : cases) {
        SCOPED_TRACE(c.value);
        const auto direction = std::format(
            R"(<direction><direction-type><dynamics><f/></dynamics></direction-type><sound dynamics="{}"/></direction>)", c.value);
        auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "", std::format("dynamics=\"{}\"", c.value))),
                                     {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        const auto check = [&](engrave::Document &document) {
            const auto dynamics = imported_dynamics(document);
            ASSERT_EQ(dynamics.size(), 1u);
            EXPECT_EQ(dynamics.front()->get_property(mu::engraving::Pid::VELOCITY).to_int(), c.direction);
            engrave::Query query;
            query.types = {"Note"};
            const auto notes = document.query(query);
            ASSERT_TRUE(notes);
            ASSERT_EQ(notes->elements.size(), 1u);
            const auto note = engrave::resolve(master_of(document), notes->elements.front().id);
            ASSERT_TRUE(note);
            EXPECT_EQ(static_cast<mu::engraving::Note *>(note->object)->user_velocity(), c.note);
        };
        check(**opened);
        for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            auto reopened = dynamics_native_roundtrip(**opened, "", format);
            ASSERT_TRUE(reopened);
            check(*reopened);
        }
    }
    const auto plain = g_engine->open(as_bytes(dynamics_musicxml("", "")), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(plain);
    engrave::Query query;
    query.types = {"Note"};
    const auto notes = (*plain)->query(query);
    ASSERT_TRUE(notes);
    ASSERT_EQ(notes->elements.size(), 1u);
    const auto note = engrave::resolve(master_of(**plain), notes->elements.front().id);
    ASSERT_TRUE(note);
    EXPECT_EQ(static_cast<mu::engraving::Note *>(note->object)->user_velocity(), 0);

    const std::string long_number(100000, '9');
    const auto direction = std::format(
        R"(<direction><direction-type><dynamics><f/></dynamics></direction-type><sound dynamics="{}"/></direction>)", long_number);
    auto large = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(large);
    const auto dynamics = imported_dynamics(**large);
    ASSERT_EQ(dynamics.size(), 1u);
    EXPECT_EQ(dynamics.front()->velocity(), 127);
}

TEST(Read, MusicXmlDynamicsContainersAndLiteralSafety) {
    using mu::engraving::DynamicType;

    struct Case {
        const char *children;
        const char *xml;
        DynamicType type;
    };

    const Case cases[] = {
        {"<ff/><pp/>", "<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>", DynamicType::OTHER},
        {"<f/><p/>", "<sym>dynamicForte</sym><sym>dynamicPiano</sym>", DynamicType::FP},
        {"<other-dynamics>A &amp; <![CDATA[B < C]]><!--split--> tail</other-dynamics>", "A &amp; B &lt; C tail", DynamicType::OTHER},
        {"<other-dynamics> f </other-dynamics>", " <sym>dynamicForte</sym> ", DynamicType::F},
        {"<other-dynamics>s<![CDATA[f]]>z</other-dynamics>", "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>",
         DynamicType::SFZ},
        {"<other-dynamics>sempre f</other-dynamics>", "sempre <sym>dynamicForte</sym>", DynamicType::F},
        {"<other-dynamics>sempre f &lt;subito&gt;</other-dynamics>", "sempre <sym>dynamicForte</sym> &lt;subito&gt;", DynamicType::F},
        {"<other-dynamics>&lt;subito&gt; f</other-dynamics>", "&lt;subito&gt; <sym>dynamicForte</sym>", DynamicType::F},
        {"<other-dynamics>p&lt;sym&gt;f&lt;/sym&gt;</other-dynamics>", "<sym>dynamicPiano</sym>&lt;sym&gt;f&lt;/sym&gt;", DynamicType::P},
        {"<other-dynamics>&lt;subito&gt; f &lt;dolce&gt;</other-dynamics>", "&lt;subito&gt; <sym>dynamicForte</sym> &lt;dolce&gt;",
         DynamicType::F},
        {"<other-dynamics>dolce &lt; f</other-dynamics>", "dolce &lt; <sym>dynamicForte</sym>", DynamicType::F},
        {"<other-dynamics>&lt;b&gt;&lt;b&gt;x&lt;/b&gt; f &lt;/b&gt; p</other-dynamics>",
         "&lt;b&gt;&lt;b&gt;x&lt;/b&gt; f &lt;/b&gt; <sym>dynamicPiano</sym>", DynamicType::P},
        {"<other-dynamics>~LITERAL_0_0~ &lt;x/&gt; ~LITERAL_1_0~ &lt;y/&gt; f</other-dynamics>",
         "~LITERAL_0_0~ &lt;x/&gt; ~LITERAL_1_0~ &lt;y/&gt; <sym>dynamicForte</sym>", DynamicType::F},
        {"<sf/><mp/>", "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicMezzo</sym><sym>dynamicPiano</sym>",
         DynamicType::OTHER},
        {"<sfz/>", "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>", DynamicType::SFZ},
        {"<sf/><p/>", "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicPiano</sym>", DynamicType::SFP},
        {"<f/>", "<sym>dynamicForte</sym>", DynamicType::F},
        {"<other-dynamics smufl=\"dynamicForteSuffix\">fallback</other-dynamics>", "fallback", DynamicType::OTHER},
        {"<other-dynamics smufl=\"dynamicForte&#x100;\">fallback</other-dynamics>", "fallback", DynamicType::OTHER},
        {"<other-dynamics smufl=\"unknownSymbol\">A &amp; B</other-dynamics>", "A &amp; B", DynamicType::OTHER},
        {"<other-dynamics smufl=\"ornamentPrallMordent\">fallback</other-dynamics>", "fallback", DynamicType::OTHER},
        {"<other-dynamics smufl=\"noteheadBlack\"/>", "<sym>noteheadBlack</sym>", DynamicType::OTHER},
        {"<other-dynamics smufl=\"dynamicForte\"/>", "<sym>dynamicForte</sym>", DynamicType::F},
        {"<other-dynamics smufl=\"dynamicSforzato\"/>", "<sym>dynamicSforzato</sym>", DynamicType::OTHER},
        {"<other-dynamics>sfz</other-dynamics>", "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>", DynamicType::SFZ},
        {"<other-dynamics>A &amp; B &lt; C</other-dynamics>", "A &amp; B &lt; C", DynamicType::OTHER},
        {"<mystery/>", "mystery", DynamicType::OTHER},
        {"<other-dynamics>&lt;sym&gt;dynamicForte&lt;/sym&gt;</other-dynamics>", "&lt;sym&gt;dynamicForte&lt;/sym&gt;", DynamicType::OTHER},
        {"<other-dynamics>&lt;sym&gt;f&lt;/sym&gt;</other-dynamics>", "&lt;sym&gt;f&lt;/sym&gt;", DynamicType::OTHER},
        {"<p/><other-dynamics>&lt;sym&gt;f&lt;/sym&gt;</other-dynamics>", "<sym>dynamicPiano</sym>&lt;sym&gt;f&lt;/sym&gt;",
         DynamicType::P},
        {"<other-dynamics>&lt;b&gt;loud&lt;/b&gt; &amp; prose</other-dynamics>", "&lt;b&gt;loud&lt;/b&gt; &amp; prose", DynamicType::OTHER},
        {"<p/><other-dynamics> prose</other-dynamics>", "<sym>dynamicPiano</sym> prose", DynamicType::P},
        {"<other-dynamics smufl=\"bad&lt;/sym&gt;&lt;b&gt;\">A &amp; B</other-dynamics>", "A &amp; B", DynamicType::OTHER},
    };
    for (bool notation : {false, true}) {
        for (const auto &c : cases) {
            SCOPED_TRACE(std::format("notation={}, {}", notation, c.children));
            const auto container = std::format("<dynamics>{}</dynamics>", c.children);
            const auto direction = std::format("<direction><direction-type>{}</direction-type></direction>", container);
            const auto xml = dynamics_musicxml(notation ? "" : direction, notation ? container : "");
            auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
            ASSERT_TRUE(opened);
            const auto check = [&](engrave::Document &document, bool reopened) {
                const auto dynamics = imported_dynamics(document);
                ASSERT_EQ(dynamics.size(), 1u);
                // The native XML reader discards whitespace-only nodes around
                // embedded symbols; import itself preserves the source text.
                const bool native_whitespace = reopened && std::string_view(c.children) == "<other-dynamics> f </other-dynamics>";
                EXPECT_EQ(dynamics.front()->xml_text().to_std_string(), native_whitespace ? "<sym>dynamicForte</sym>" : c.xml);
                EXPECT_EQ(dynamics.front()->dynamic_type(), c.type);
                if (c.type == DynamicType::OTHER) EXPECT_EQ(dynamics.front()->velocity(), -1);
                EXPECT_TRUE(dynamics.front()->visible());
            };
            check(**opened, false);
            for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
                auto reopened = dynamics_native_roundtrip(**opened, c.type == DynamicType::OTHER ? c.xml : "", format);
                ASSERT_TRUE(reopened);
                check(*reopened, true);
            }
        }
    }
}

TEST(Read, MusicXmlManyLiteralSpansPreserveText) {
    std::string literal;
    for (int i = 0; i < 2000; ++i) {
        literal += "&lt;b/&gt; x ";
    }
    const auto direction =
        "<direction><direction-type><dynamics><other-dynamics>" + literal + "f</other-dynamics></dynamics></direction-type></direction>";
    auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    const auto dynamics = imported_dynamics(**opened);
    ASSERT_EQ(dynamics.size(), 1u);
    EXPECT_EQ(dynamics.front()->dynamic_type(), mu::engraving::DynamicType::F);
    EXPECT_EQ(dynamics.front()->xml_text().to_std_string(), literal + "<sym>dynamicForte</sym>");
}

TEST(Read, MusicXmlDynamicsSeparateContainersKeepStyleAndTiming) {
    using namespace mu::engraving;
    for (bool notation : {false, true}) {
        SCOPED_TRACE(notation);
        const std::string containers = R"(<dynamics color="#FF0000" placement="above" enclosure="circle" print-object="no"><p/></dynamics>
<dynamics color="#0000FF" placement="below" enclosure="rectangle"><f/></dynamics><dynamics/>)";
        const auto direction =
            std::format(R"(<direction placement="below"><direction-type>{}</direction-type><offset>1</offset></direction>)", containers);
        auto opened = g_engine->open(as_bytes(dynamics_musicxml(notation ? "" : direction, notation ? containers : "")),
                                     {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        const auto check = [&](engrave::Document &document) {
            const auto dynamics = imported_dynamics(document);
            ASSERT_EQ(dynamics.size(), 2u);
            std::set<DynamicType> types;
            for (const auto *dynamic : dynamics) {
                types.insert(dynamic->dynamic_type());
                const bool piano = dynamic->dynamic_type() == DynamicType::P;
                EXPECT_TRUE(dynamic->visible());
                EXPECT_EQ(dynamic->color(), muse::Color::from_string(piano ? "#FF0000" : "#0000FF"));
                EXPECT_EQ(dynamic->placement(), notation && piano ? PlacementV::ABOVE : PlacementV::BELOW);
                EXPECT_EQ(dynamic->frame_type(), piano ? FrameType::CIRCLE : FrameType::RECTANGLE);
                EXPECT_EQ(dynamic->tick().ticks(), notation ? 0 : Constants::division);
            }
            EXPECT_EQ(types, (std::set<DynamicType>{DynamicType::P, DynamicType::F}));
        };
        check(**opened);
        for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            auto reopened = dynamics_native_roundtrip(**opened, "", format);
            ASSERT_TRUE(reopened);
            check(*reopened);
        }
    }
}

TEST(Read, MusicXmlDynamicsKeepPerContainerInferredPlacement) {
    const auto xml = dynamics_musicxml(R"(<direction><direction-type>
<dynamics default-y="30"><p/></dynamics><dynamics default-y="-30"><f/></dynamics>
<dynamics/></direction-type></direction>)",
                                       "");
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    const auto dynamics = imported_dynamics(**opened);
    ASSERT_EQ(dynamics.size(), 2u);
    for (const auto *dynamic : dynamics) {
        EXPECT_EQ(dynamic->placement(), dynamic->dynamic_type() == mu::engraving::DynamicType::P ? mu::engraving::PlacementV::ABOVE :
                                                                                                   mu::engraving::PlacementV::BELOW);
    }
}

TEST(Read, MusicXmlSingleDynamicsKeepNativeBehavior) {
    using mu::engraving::DynamicType;

    struct Case {
        const char *tag;
        DynamicType type;
        int velocity;
        int change;
    };

    const Case cases[] = {
        {"pppppp", DynamicType::PPPPPP, 1, 0}, {"ppppp", DynamicType::PPPPP, 5, 0},
        {"pppp", DynamicType::PPPP, 10, 0},    {"ppp", DynamicType::PPP, 16, 0},
        {"pp", DynamicType::PP, 33, 0},        {"p", DynamicType::P, 49, 0},
        {"mp", DynamicType::MP, 64, 0},        {"mf", DynamicType::MF, 80, 0},
        {"f", DynamicType::F, 96, 0},          {"ff", DynamicType::FF, 112, 0},
        {"fff", DynamicType::FFF, 126, 0},     {"ffff", DynamicType::FFFF, 127, 0},
        {"fffff", DynamicType::FFFFF, 127, 0}, {"ffffff", DynamicType::FFFFFF, 127, 0},
        {"fp", DynamicType::FP, 96, -47},      {"pf", DynamicType::PF, 49, 47},
        {"sf", DynamicType::SF, 112, -18},     {"sfz", DynamicType::SFZ, 112, -18},
        {"sff", DynamicType::SFF, 126, -18},   {"sffz", DynamicType::SFFZ, 126, -18},
        {"sfff", DynamicType::SFFF, 127, -18}, {"sfffz", DynamicType::SFFFZ, 127, -18},
        {"sfp", DynamicType::SFP, 112, -47},   {"sfpp", DynamicType::SFPP, 112, -79},
        {"rfz", DynamicType::RFZ, 112, -18},   {"rf", DynamicType::RF, 112, -18},
        {"fz", DynamicType::FZ, 112, -18},     {"m", DynamicType::M, 96, -16},
        {"r", DynamicType::R, 112, -18},       {"s", DynamicType::S, 112, -18},
        {"z", DynamicType::Z, 80, 0},          {"n", DynamicType::N, 49, -48},
    };
    for (bool notation : {false, true}) {
        for (const auto &c : cases) {
            SCOPED_TRACE(std::format("notation={}, tag={}", notation, c.tag));
            const auto container = std::format("<dynamics><{}/></dynamics>", c.tag);
            const auto direction = std::format("<direction><direction-type>{}</direction-type></direction>", container);
            auto opened = g_engine->open(as_bytes(dynamics_musicxml(notation ? "" : direction, notation ? container : "")),
                                         {.format = engrave::Format::MUSIC_XML});
            ASSERT_TRUE(opened);
            const auto dynamics = imported_dynamics(**opened);
            ASSERT_EQ(dynamics.size(), 1u);
            EXPECT_EQ(dynamics.front()->dynamic_type(), c.type);
            EXPECT_EQ(dynamics.front()->velocity(), c.velocity);
            EXPECT_EQ(dynamics.front()->change_in_velocity(), c.change);
        }
    }
}

TEST(Read, MusicXmlDynamicsHyphenRangeKeepsExistingBehavior) {
    auto seed = g_engine->open(as_bytes(dynamics_musicxml("", "")), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(seed);
    auto &config = const_cast<mu::iex::musicxml::MusicXmlConfiguration &>(engrave::DocumentAccess::context(**seed).music_xml());

    struct RestoreInference {
        mu::iex::musicxml::MusicXmlConfiguration &config;
        bool previous;

        ~RestoreInference() { config.set_infer_text_type_override(previous); }
    } restore{config, config.infer_text_type()};

    config.set_infer_text_type_override(true);
    const auto xml = dynamics_musicxml(R"(<direction><direction-type><dynamics><mp/></dynamics>
<words>-</words><dynamics><f/></dynamics></direction-type></direction>)",
                                       "");
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    const auto dynamics = imported_dynamics(**opened);
    ASSERT_EQ(dynamics.size(), 1u);
    EXPECT_EQ(dynamics.front()->xml_text().to_std_string(),
              "<sym>dynamicMezzo</sym><sym>dynamicPiano</sym><sym>dynamicCombinedSeparatorHyphen</sym><sym>dynamicForte</sym>");
    const auto words = dynamics_musicxml("<direction><direction-type><words>mf</words></direction-type></direction>", "");
    auto inferred = g_engine->open(as_bytes(words), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(inferred);
    const auto word_dynamics = imported_dynamics(**inferred);
    ASSERT_EQ(word_dynamics.size(), 1u);
    EXPECT_EQ(word_dynamics.front()->dynamic_type(), mu::engraving::DynamicType::MF);
    EXPECT_EQ(word_dynamics.front()->velocity(), 80);
}

TEST(Read, MusicXmlGroupedDynamicsSoundOverrideAndReimport) {
    using mu::engraving::DynamicType;
    for (const std::string children : {"<ff/><pp/>", "<f/><p/>"}) {
        for (bool sound : {false, true}) {
            const auto direction = std::format("<direction><direction-type><dynamics>{}</dynamics></direction-type>{}</direction>",
                                               children, sound ? "<sound dynamics=\"50\"/>" : "");
            auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
            ASSERT_TRUE(opened);
            const auto check = [&](engrave::Document &doc) {
                const auto dynamics = imported_dynamics(doc);
                ASSERT_EQ(dynamics.size(), 1u);
                const bool other = children == "<ff/><pp/>";
                EXPECT_EQ(dynamics.front()->dynamic_type(), other ? DynamicType::OTHER : DynamicType::FP);
                EXPECT_EQ(dynamics.front()->velocity(), sound ? 45 : (other ? -1 : 96));
            };
            check(**opened);
            for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
                auto reopened = dynamics_native_roundtrip(**opened, "", format);
                ASSERT_TRUE(reopened);
                check(*reopened);
            }
            const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
            ASSERT_TRUE(saved);
            auto reopened = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
            ASSERT_TRUE(reopened);
            check(**reopened);
        }
    }
}

TEST(Read, MusicXmlLiteralDynamicsExportReimport) {
    for (const std::string literal : {"A &amp; B &lt; C", "&lt;sym&gt;dynamicForte&lt;/sym&gt;", "&lt;sym&gt;f&lt;/sym&gt;"}) {
        const auto direction = std::format("<direction><direction-type><dynamics><other-dynamics>{}</other-dynamics></dynamics>"
                                           "</direction-type></direction>",
                                           literal);
        auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(saved);
        const muse::ByteArray bytes(reinterpret_cast<const char *>(saved->data()), saved->size());
        muse::XmlStreamReader reader(bytes);
        while (!reader.at_end()) reader.read_next();
        EXPECT_FALSE(reader.is_error());
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(reopened);
        const auto dynamics = imported_dynamics(**reopened);
        ASSERT_EQ(dynamics.size(), 1u);
        EXPECT_EQ(dynamics.front()->xml_text().to_std_string(), literal);
    }
}

TEST(Read, MusicXmlOrdinarySingleDynamicsSerializeAsStandardTags) {
    using mu::engraving::DynamicType;
    for (bool notation : {false, true}) {
        for (const std::string tag : {"f", "fp"}) {
            const auto container = std::format("<dynamics><{}/></dynamics>", tag);
            const auto direction = std::format("<direction><direction-type>{}</direction-type></direction>", container);
            auto opened = g_engine->open(as_bytes(dynamics_musicxml(notation ? "" : direction, notation ? container : "")),
                                         {.format = engrave::Format::MUSIC_XML});
            ASSERT_TRUE(opened);
            const auto check = [&](engrave::Document &doc) {
                const auto dynamics = imported_dynamics(doc);
                ASSERT_EQ(dynamics.size(), 1u);
                EXPECT_EQ(dynamics.front()->dynamic_type(), tag == "f" ? DynamicType::F : DynamicType::FP);
                EXPECT_FALSE(dynamics.front()->has_custom_text());
            };
            check(**opened);
            for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
                auto reopened = dynamics_native_roundtrip(**opened, "", format);
                ASSERT_TRUE(reopened);
                check(*reopened);
            }
            const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
            ASSERT_TRUE(saved);
            const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
            EXPECT_NE(xml.find(std::format("<{}/>", tag)), std::string::npos);
        }
    }
}

TEST(Read, MusicXmlSingleSmuflDynamicsExportReimport) {
    for (const std::string name : {"dynamicSforzato", "noteheadBlack", "dynamicForte"}) {
        const auto direction = std::format("<direction><direction-type><dynamics><other-dynamics smufl=\"{}\"/>"
                                           "</dynamics></direction-type></direction>",
                                           name);
        auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(reopened);
        const auto dynamics = imported_dynamics(**reopened);
        ASSERT_EQ(dynamics.size(), 1u);
        EXPECT_EQ(dynamics.front()->xml_text().to_std_string(), std::format("<sym>{}</sym>", name));
    }
}

TEST(Read, MusicXmlGroupedSmuflDynamicsExportReimport) {
    for (const std::string children :
         {"<f/><other-dynamics smufl=\"dynamicSforzato\"/><p/>",
          "<other-dynamics>sempre </other-dynamics><other-dynamics smufl=\"dynamicSforzato\"/><p/>",
          "<other-dynamics>sempre f</other-dynamics><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<other-dynamics>sempre f</other-dynamics><p/><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<other-dynamics>sempre </other-dynamics><f/><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<p/><other-dynamics> subito </other-dynamics><f/><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<other-dynamics>f </other-dynamics><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<other-dynamics>f &#x9;</other-dynamics><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<other-dynamics>f &#13;</other-dynamics><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<other-dynamics>sempre &#13; f</other-dynamics><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<other-dynamics>&lt;sym&gt;f&lt;/sym&gt; &amp; </other-dynamics><other-dynamics smufl=\"dynamicSforzato\"/>"}) {
        SCOPED_TRACE(children);
        const auto direction = std::format("<direction><direction-type><dynamics>{}</dynamics></direction-type></direction>", children);
        auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        const auto original = imported_dynamics(**opened);
        ASSERT_EQ(original.size(), 1u);
        const auto text = original.front()->xml_text();
        const auto type = original.front()->dynamic_type();
        const auto velocity = original.front()->velocity();
        const auto velocity_change = original.front()->change_in_velocity();
        if (children.starts_with("<other-dynamics>sempre f</other-dynamics>")) {
            EXPECT_EQ(type, mu::engraving::DynamicType::F);
        } else if (children.starts_with("<other-dynamics>sempre </other-dynamics><f/>")) {
            EXPECT_EQ(type, mu::engraving::DynamicType::OTHER);
        } else if (children.starts_with("<p/><other-dynamics> subito ")) {
            EXPECT_EQ(type, mu::engraving::DynamicType::P);
        }
        const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(saved);
        const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
        EXPECT_NE(xml.find("smufl=\"dynamicSforzato\""), std::string::npos);
        if (children.starts_with("<f/>")) {
            EXPECT_NE(xml.find("<f/>"), std::string::npos);
            EXPECT_NE(xml.find("<p/>"), std::string::npos);
            EXPECT_EQ(xml.find("smufl=\"dynamicForte\""), std::string::npos);
            EXPECT_EQ(xml.find("smufl=\"dynamicPiano\""), std::string::npos);
        }
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(reopened);
        const auto dynamics = imported_dynamics(**reopened);
        ASSERT_EQ(dynamics.size(), 1u);
        EXPECT_EQ(dynamics.front()->xml_text(), text);
        EXPECT_EQ(dynamics.front()->dynamic_type(), type);
        EXPECT_EQ(dynamics.front()->velocity(), velocity);
        EXPECT_EQ(dynamics.front()->change_in_velocity(), velocity_change);
    }
}

TEST(Read, MusicXmlGroupedProseRetainsTypeWithoutNamedSymbol) {
    const auto direction =
        R"(<direction><direction-type><dynamics><other-dynamics>sempre f</other-dynamics><p/></dynamics></direction-type></direction>)";
    auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    const auto original = imported_dynamics(**opened);
    ASSERT_EQ(original.size(), 1u);
    EXPECT_EQ(original.front()->dynamic_type(), mu::engraving::DynamicType::F);
    const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(reopened);
    const auto dynamics = imported_dynamics(**reopened);
    ASSERT_EQ(dynamics.size(), 1u);
    EXPECT_EQ(dynamics.front()->dynamic_type(), original.front()->dynamic_type());
    EXPECT_EQ(dynamics.front()->xml_text(), original.front()->xml_text());
    EXPECT_EQ(dynamics.front()->velocity(), original.front()->velocity());
    EXPECT_EQ(dynamics.front()->change_in_velocity(), original.front()->change_in_velocity());
}

TEST(Read, MusicXmlGroupedWhitespaceRetainsType) {
    for (const std::string children :
         {"<other-dynamics> sf</other-dynamics><p/>", "<other-dynamics> sf</other-dynamics><p/><other-dynamics smufl=\"dynamicSforzato\"/>",
          "<f/><other-dynamics>&#32;</other-dynamics><p/>"}) {
        SCOPED_TRACE(children);
        const auto direction = std::format("<direction><direction-type><dynamics>{}</dynamics></direction-type></direction>", children);
        auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        const auto original = imported_dynamics(**opened);
        ASSERT_EQ(original.size(), 1u);
        const auto expected = children.starts_with("<f/>") ? mu::engraving::DynamicType::F : mu::engraving::DynamicType::SF;
        EXPECT_EQ(original.front()->dynamic_type(), expected);
        const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(reopened);
        const auto dynamics = imported_dynamics(**reopened);
        ASSERT_EQ(dynamics.size(), 1u);
        EXPECT_EQ(dynamics.front()->dynamic_type(), original.front()->dynamic_type());
        EXPECT_EQ(dynamics.front()->xml_text(), original.front()->xml_text());
        EXPECT_EQ(dynamics.front()->velocity(), original.front()->velocity());
        EXPECT_EQ(dynamics.front()->change_in_velocity(), original.front()->change_in_velocity());
    }
}

TEST(Read, MusicXmlSingleOtherDynamicLettersExportText) {
    for (const std::string letter : {"m", "r", "s", "z"}) {
        const auto direction = std::format(
            "<direction><direction-type><dynamics><other-dynamics>{}</other-dynamics></dynamics></direction-type></direction>", letter);
        auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        const auto original = imported_dynamics(**opened);
        ASSERT_EQ(original.size(), 1u);
        const auto type = original.front()->dynamic_type();
        const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(saved);
        const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
        EXPECT_NE(xml.find(std::format("<other-dynamics>{}</other-dynamics>", letter)), std::string::npos);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(reopened);
        const auto dynamics = imported_dynamics(**reopened);
        ASSERT_EQ(dynamics.size(), 1u);
        EXPECT_EQ(dynamics.front()->dynamic_type(), type);
        EXPECT_EQ(dynamics.front()->xml_text(), original.front()->xml_text());
    }
}

TEST(Read, MusicXmlEngineLocalSymbolIsNotExportedAsSmufl) {
    const auto direction = R"(<direction><direction-type><dynamics><f/></dynamics></direction-type></direction>)";
    auto opened = g_engine->open(as_bytes(dynamics_musicxml(direction, "")), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    const auto dynamics = imported_dynamics(**opened);
    ASSERT_EQ(dynamics.size(), 1u);
    dynamics.front()->set_dynamic_type(mu::engraving::DynamicType::OTHER);
    dynamics.front()->set_xml_text(u"<sym>ornamentPrallMordent</sym>");
    const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(saved);
    const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
    EXPECT_EQ(xml.find("smufl=\"ornamentPrallMordent\""), std::string::npos);
}

TEST(Read, MusicXmlNotationDynamicsKeepExistingVisibility) {
    auto xml = dynamics_musicxml("", "<dynamics><f/><p/></dynamics>");
    xml.replace(xml.find("<notations>"), std::string("<notations>").size(), "<notations print-object=\"no\">");
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    const auto dynamics = imported_dynamics(**opened);
    ASSERT_EQ(dynamics.size(), 1u);
    EXPECT_TRUE(dynamics.front()->visible());
    EXPECT_EQ(dynamics.front()->dynamic_type(), mu::engraving::DynamicType::FP);
}

// --- corpus discovery ----------------------------------------------------

bool looks_like_score(const std::filesystem::path &path) {
    static const std::set<std::string> extensions = {".mscx", ".mscz", ".musicxml", ".mxl", ".mid", ".midi",
                                                     ".gp",   ".gpx",  ".gp3",      ".gp4", ".gp5", ".ptb"};
    return extensions.contains(path.extension().string());
}

void collect(const std::filesystem::path &path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::directory_iterator(path, ec)) files.push_back(entry.path());
        std::ranges::sort(files);
        for (const auto &file : files) collect(file);
        return;
    }
    if (std::filesystem::is_regular_file(path, ec) && looks_like_score(path)) g_corpus.push_back(path);
}

} // namespace

namespace {
std::string chordline_offset_fixture() {
    return read_text(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/chordline-offsets/offsets-500.mscx");
}

std::vector<mu::engraving::ChordLine *> offset_test_lines(mu::engraving::Score &score) {
    std::vector<mu::engraving::ChordLine *> lines;
    score.scan_elements([&](mu::engraving::EngravingItem *item) {
        if (item->is_chord_line()) lines.push_back(mu::engraving::to_chord_line(item));
    });
    return lines;
}

std::string offset_test_write(const mu::engraving::ChordLine *line) {
    muse::ByteArray bytes;
    muse::io::Buffer buffer(&bytes);
    buffer.open(muse::io::IODevice::WRITE_ONLY);
    {
        mu::engraving::XmlWriter xml(&buffer);
        mu::engraving::write::WriteContext ctx(line->score());
        mu::engraving::write::TWrite::write(line, xml, ctx);
    }
    return {reinterpret_cast<const char *>(bytes.const_data()), bytes.size()};
}

TEST(Read, ChordLineOffsetsEachAndCombined500) {
    const auto fixture = chordline_offset_fixture();
    ASSERT_FALSE(fixture.empty());
    const auto begin = fixture.find("<o1");
    const auto end = fixture.find("<subtype>", begin);
    ASSERT_NE(begin, std::string::npos);
    ASSERT_NE(end, std::string::npos);
    const std::array<muse::PointF, 4> input = {muse::PointF(0.25, -0.5), muse::PointF(0.5, 0.25), muse::PointF(-0.25, 0.75),
                                               muse::PointF(1.0, 0.5)};
    // Zero, each independently, and all together. Exercise every orientation,
    // subtype ordering, and normal/small chord scaling.
    for (int mask : {0, 1, 2, 4, 8, 15}) {
        for (const std::string type : {"1", "2", "3", "4"}) {
            for (bool small : {false, true}) {
                for (bool before : {false, true}) {
                    SCOPED_TRACE(std::format("mask={} type={} small={} before={}", mask, type, small, before));
                    std::string tags;
                    for (size_t i = 0; i < 4; ++i) {
                        if (mask & (1 << i)) tags += std::format("<o{} x=\"{}\" y=\"{}\"/>", i + 1, input[i].x(), input[i].y());
                    }
                    auto xml = fixture;
                    xml.replace(begin, end - begin, "");
                    xml.replace(xml.find("<subtype>1</subtype>"), 20, "<subtype>" + type + "</subtype>");
                    const auto insertion = before ? xml.find("<subtype>") : xml.find("</ChordLine>");
                    xml.insert(insertion, tags);
                    if (small) xml.insert(xml.find("<durationType>"), "<small>1</small>");
                    auto document = g_engine->open(as_bytes(xml));
                    ASSERT_TRUE(document) << document.error().message;
                    auto &master = master_of(**document);
                    auto lines = offset_test_lines(master);
                    ASSERT_EQ(lines.size(), 1u);
                    auto *line = lines[0];
                    EXPECT_EQ(line->has_source_offsets(), mask != 0);
                    const double unit = line->offset_is_spatium_dependent() ? master.style().spatium() : mu::engraving::dpmm;
                    const double sp = line->spatium(), mag = line->chord()->intrinsic_mag();
                    if (small) EXPECT_LT(mag, 1.0);
                    const double x = (type == "3" || type == "4") ? -1.2 : 1.2;
                    const double y = (type == "1" || type == "4") ? 1.0 : -1.0;
                    const std::array<muse::PointF, 4> defaults =
                        x < 0 ?
                            std::array<muse::PointF, 4>{muse::PointF(), muse::PointF(0, y / 2), muse::PointF(x / 2, y),
                                                        muse::PointF(x, y)} :
                            std::array<muse::PointF, 4>{muse::PointF(), muse::PointF(x / 2, 0), muse::PointF(x, y / 2), muse::PointF(x, y)};
                    ASSERT_EQ(line->ldata()->path.element_count(), 4u);
                    for (size_t i = 0; i < 4; ++i) {
                        const auto offset = (mask & (1 << i)) ? input[i] : muse::PointF();
                        EXPECT_NEAR(line->source_offsets()[i].x(), offset.x() * unit, 1e-6);
                        EXPECT_NEAR(line->source_offsets()[i].y(), offset.y() * unit, 1e-6);
                        const auto &p = line->ldata()->path.element_at(i);
                        EXPECT_NEAR(p.x, (defaults[i].x() * sp + offset.x() * unit) * mag, 1e-6);
                        EXPECT_NEAR(p.y, (defaults[i].y() * sp + offset.y() * unit) * mag, 1e-6);
                    }
                    std::unique_ptr<mu::engraving::ChordLine> copy(line->clone());
                    EXPECT_EQ(copy->has_source_offsets(), mask != 0);
                    EXPECT_EQ(copy->source_offsets(), line->source_offsets());
                    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
                        auto saved = (*document)->export_score({.format = format, .native_version = "5.00"});
                        ASSERT_TRUE(saved) << saved.error().message;
                        auto reopened = g_engine->open(*saved, {.format = format});
                        ASSERT_TRUE(reopened) << reopened.error().message;
                        auto roundtrip = offset_test_lines(master_of(**reopened));
                        ASSERT_EQ(roundtrip.size(), 1u);
                        EXPECT_EQ(roundtrip[0]->has_source_offsets(), mask != 0);
                        for (size_t i = 0; i < 4; ++i) {
                            EXPECT_NEAR(roundtrip[0]->source_offsets()[i].x(), line->source_offsets()[i].x(), 1e-6);
                            EXPECT_NEAR(roundtrip[0]->source_offsets()[i].y(), line->source_offsets()[i].y(), 1e-6);
                        }
                    }
                }
            }
        }
    }
}

TEST(Read, ChordLineOffsetsDormantControlsWriteWithoutLayout) {
    for (const std::string mode : {"", "<straight>1</straight>", "<wavy>1</wavy>"}) {
        auto xml = chordline_offset_fixture();
        xml.insert(xml.find("</ChordLine>"), mode);
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document);
        auto lines = offset_test_lines(master_of(**document));
        ASSERT_EQ(lines.size(), 1u);
        auto *line = lines[0];
        const auto source = line->source_offsets();
        if (line->is_straight()) {
            ASSERT_EQ(line->ldata()->path.element_count(), 2u);
            const double mag = line->chord()->intrinsic_mag();
            EXPECT_NEAR(line->ldata()->path.element_at(0).x, source[0].x() * mag, 1e-6);
            EXPECT_NEAR(line->ldata()->path.element_at(1).x, (1.2 * line->spatium() + source[3].x()) * mag, 1e-6);
        } else if (line->is_wavy()) {
            EXPECT_EQ(line->ldata()->path.element_count(), 0u);
        }
        // Serialize a clone with no layout path. Call the writer directly, so
        // an export helper cannot silently repair layout before this assertion.
        std::unique_ptr<mu::engraving::ChordLine> copy(line->clone());
        copy->mutldata()->path = muse::draw::PainterPath();
        const auto written = offset_test_write(copy.get());
        EXPECT_EQ(written.find("<Path"), std::string::npos);
        for (const char *tag : {"<o1", "<o2", "<o3", "<o4"}) EXPECT_NE(written.find(tag), std::string::npos);
        auto reload_xml = chordline_offset_fixture();
        const auto start = reload_xml.find("<ChordLine>");
        reload_xml.replace(start, reload_xml.find("</ChordLine>", start) + 12 - start, written);
        auto reopened = g_engine->open(as_bytes(reload_xml));
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto reloaded = offset_test_lines(master_of(**reopened));
        ASSERT_EQ(reloaded.size(), 1u);
        for (size_t i = 0; i < 4; ++i) {
            EXPECT_NEAR(reloaded[0]->source_offsets()[i].x(), source[i].x(), 1e-6);
            EXPECT_NEAR(reloaded[0]->source_offsets()[i].y(), source[i].y(), 1e-6);
        }
        EXPECT_EQ(reloaded[0]->is_straight(), line->is_straight());
        EXPECT_EQ(reloaded[0]->is_wavy(), line->is_wavy());
    }
}

TEST(Read, ChordLineOffsetMillimetreUnitsSurvivePropertyOrderingAndReopen) {
    for (bool before : {false, true}) {
        auto xml = chordline_offset_fixture();
        xml.insert(before ? xml.find("<o1") : xml.find("</ChordLine>"), "<sizeIsSpatiumDependent>0</sizeIsSpatiumDependent>");
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document);
        auto lines = offset_test_lines(master_of(**document));
        ASSERT_EQ(lines.size(), 1u);
        ASSERT_FALSE(lines[0]->offset_is_spatium_dependent());
        EXPECT_NEAR(lines[0]->source_offsets()[3].x(), mu::engraving::dpmm, 1e-6);
        const auto source = lines[0]->source_offsets();
        for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            auto saved = (*document)->export_score({.format = format, .native_version = "5.00"});
            ASSERT_TRUE(saved);
            auto reopened = g_engine->open(*saved, {.format = format});
            ASSERT_TRUE(reopened);
            auto again = offset_test_lines(master_of(**reopened));
            ASSERT_EQ(again.size(), 1u);
            EXPECT_FALSE(again[0]->offset_is_spatium_dependent());
            for (size_t i = 0; i < 4; ++i) {
                EXPECT_NEAR(again[0]->source_offsets()[i].x(), source[i].x(), 1e-6);
                EXPECT_NEAR(again[0]->source_offsets()[i].y(), source[i].y(), 1e-6);
            }
        }
    }
}

TEST(Read, ChordLineMixedSourcesRejectInBothOrders) {
    const std::string path = "<Path><Element type=\"0\" x=\"0\" y=\"0\"/><Element type=\"1\" x=\"1.5\" y=\"2.5\"/><Element type=\"1\" "
                             "x=\"3\" y=\"2.5\"/></Path>";
    for (bool first : {false, true}) {
        auto xml = chordline_offset_fixture();
        xml.insert(first ? xml.find("<o1") : xml.find("</ChordLine>"), path);
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_FALSE(document);
        EXPECT_EQ(document.error().code, ErrorCode::INVALID_INPUT);
        EXPECT_NE(document.error().message.find("ChordLine"), std::string::npos);
    }
}

TEST(Read, ChordLinePhysicalOffsetsFollowBothSpatiumCallbacks) {
    for (bool independent : {false, true}) {
        auto xml = chordline_offset_fixture();
        if (independent) xml.insert(xml.find("</ChordLine>"), "<sizeIsSpatiumDependent>0</sizeIsSpatiumDependent>");
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document);
        auto lines = offset_test_lines(master_of(**document));
        ASSERT_EQ(lines.size(), 1u);
        auto *line = lines[0];
        const auto original = line->source_offsets();
        master_of(**document).spatium_changed(1.0, 1.25);
        line->staff()->set_local_spatium(1.0, 0.8, mu::engraving::Fraction(0, 1));
        for (size_t i = 0; i < 4; ++i) {
            EXPECT_NEAR(line->source_offsets()[i].x(), original[i].x(), 1e-9);
            EXPECT_NEAR(line->source_offsets()[i].y(), original[i].y(), 1e-9);
        }
        line->staff()->set_local_spatium(1.0, 0.75, mu::engraving::Fraction(0, 1));
        for (size_t i = 0; i < 4; ++i) {
            EXPECT_NEAR(line->source_offsets()[i].x(), original[i].x() * 0.75, 1e-9);
            EXPECT_NEAR(line->source_offsets()[i].y(), original[i].y() * 0.75, 1e-9);
        }
        auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved);
        ASSERT_TRUE(reopened);
        auto again = offset_test_lines(master_of(**reopened));
        ASSERT_EQ(again.size(), 1u);
        for (size_t i = 0; i < 4; ++i) {
            EXPECT_NEAR(again[0]->source_offsets()[i].x(), original[i].x() * 0.75, 1e-6);
            EXPECT_NEAR(again[0]->source_offsets()[i].y(), original[i].y() * 0.75, 1e-6);
        }
    }
}

TEST(Read, ChordLineZeroMissingAndMalformedOffsets) {
    for (const std::string tags : {"", "<o1/>", "<o4 x=\"0\" y=\"0\"/>"}) {
        auto xml = chordline_offset_fixture();
        const auto begin = xml.find("<o1");
        xml.replace(begin, xml.find("<subtype>", begin) - begin, tags);
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document);
        auto lines = offset_test_lines(master_of(**document));
        ASSERT_EQ(lines.size(), 1u);
        EXPECT_EQ(lines[0]->has_source_offsets(), !tags.empty());
        EXPECT_EQ(lines[0]->source_offsets(), (std::array<muse::PointF, 4>{}));
        auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved);
        ASSERT_TRUE(reopened);
        auto again = offset_test_lines(master_of(**reopened));
        ASSERT_EQ(again.size(), 1u);
        EXPECT_EQ(again[0]->has_source_offsets(), !tags.empty());
    }
    for (const std::string tags : {"<o1 x=\"nan\"/>", "<o1 x=\"1junk\"/>", "<o1 x=\"1e999\"/>", "<o1/><o1/>", "<o1><unexpected/></o1>",
                                   "<Path><Element type=\"2\" x=\"1\" y=\"1\"/></Path>", "<Path/><o1/>", "<o1/><Path/>"}) {
        SCOPED_TRACE(tags);
        auto xml = chordline_offset_fixture();
        const auto begin = xml.find("<o1");
        xml.replace(begin, xml.find("<subtype>", begin) - begin, tags);
        auto document = g_engine->open(as_bytes(xml));
        EXPECT_FALSE(document);
    }
}

TEST(Read, ChordLineActualGoldenTypedGeometry) {
    // Run with --corpus=<host>/fixtures/golden/chordline_path.mscx.
    const auto it = std::ranges::find_if(g_corpus, [](const auto &p) { return p.filename() == "chordline_path.mscx"; });
    ASSERT_NE(it, g_corpus.end()) << "actual golden fixture must be supplied with --corpus";
    auto doc = open_path(*it);
    ASSERT_TRUE(doc);
    auto lines = offset_test_lines(master_of(*doc));
    ASSERT_FALSE(lines.empty());
    auto edited = std::ranges::find_if(lines, [](const auto *l) { return l->modified(); });
    ASSERT_NE(edited, lines.end());
    auto *line = *edited;
    EXPECT_FALSE(line->has_source_offsets());
    const auto path = line->ldata()->path;
    ASSERT_EQ(path.element_count(), 3u);
    EXPECT_EQ(path.element_at(0).type, muse::draw::PainterPath::ElementType::MOVE_TO_ELEMENT);
    for (size_t i = 1; i < 3; ++i) {
        EXPECT_EQ(path.element_at(i).type, muse::draw::PainterPath::ElementType::LINE_TO_ELEMENT);
        EXPECT_NEAR(path.element_at(i).x, 1.5 * i * line->spatium(), 1e-6);
        EXPECT_NEAR(path.element_at(i).y, 2.5 * line->spatium(), 1e-6);
    }
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        auto saved = doc->export_score({.format = format, .native_version = "5.00"});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = format});
        ASSERT_TRUE(reopened);
        auto again = offset_test_lines(master_of(**reopened));
        auto found = std::ranges::find_if(again, [](const auto *l) { return l->modified(); });
        ASSERT_NE(found, again.end());
        ASSERT_EQ((*found)->ldata()->path.element_count(), path.element_count());
        for (size_t i = 0; i < path.element_count(); ++i) {
            const auto &p = (*found)->ldata()->path.element_at(i);
            EXPECT_EQ(p.type, path.element_at(i).type);
            EXPECT_NEAR(p.x, path.element_at(i).x, 1e-6);
            EXPECT_NEAR(p.y, path.element_at(i).y, 1e-6);
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument.starts_with("--assets=")) {
            g_assets = std::filesystem::path(argument.substr(9));
        } else if (argument.starts_with("--corpus=")) {
            collect(std::filesystem::path(argument.substr(9)));
        }
    }
    if (g_corpus.empty()) {
        for (const auto &candidate : {std::filesystem::path("fixtures/golden"), std::filesystem::path("../fixtures/golden"),
                                      std::filesystem::path("../../fixtures/golden")}) {
            if (std::filesystem::is_directory(candidate)) {
                collect(candidate);
                break;
            }
        }
    }
    if (g_corpus.empty()) {
        std::fprintf(stderr, "no corpus: pass --corpus=<file or directory>\n");
        return 1;
    }

    auto engine = engrave::Engine::create({.resources = g_assets});
    if (!engine) {
        std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", engine.error().message.c_str());
        return 1;
    }
    g_engine = std::move(*engine);
    const int result = RUN_ALL_TESTS();
    g_engine.reset();
    return result;
}
