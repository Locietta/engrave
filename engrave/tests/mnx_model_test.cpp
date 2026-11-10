// SPDX-License-Identifier: GPL-3.0-only
// The MNX document model: every MNX test input reads, round-trips and indexes; reading rejects
// schema violations with a located error, and indexing rejects semantic ones.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "model/document.h"
#include "model/index.h"
#include "model/layout.h"

namespace model = mu::iex::mnxio::model;

namespace {

const std::filesystem::path data = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/importexport/mnx/tests/data";

std::string read_file(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

std::vector<std::filesystem::path> inputs() {
    std::vector<std::filesystem::path> out;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(data))
        if (entry.path().extension() == ".mnx") out.push_back(entry.path());
    return out;
}

// The smallest valid document; tests splice their case into it.
constexpr std::string_view minimal = R"({
  "mnx": {"version": 1},
  "global": {"measures": [{"time": {"count": 4, "unit": 4}}]},
  "parts": [{"measures": [{"sequences": [{"content": [%]}]}]}]
})";

std::string with_content(std::string_view content) {
    std::string text(minimal);
    text.replace(text.find('%'), 1, content);
    return text;
}

// Expects the read to fail and returns the message.
std::string rejection(std::string_view text) {
    auto result = model::read_document(text);
    EXPECT_FALSE(result) << text;
    return result ? std::string() : result.error().message;
}

} // namespace

TEST(MnxModel, TestInputsReadAndRoundTrip) {
    const auto files = inputs();
    ASSERT_GE(files.size(), 30U);
    for (const auto &path : files) {
        SCOPED_TRACE(path.string());
        const auto first = model::read_document(read_file(path));
        ASSERT_TRUE(first) << first.error().message;
        const std::string written = model::write_document(*first);
        const auto second = model::read_document(written);
        ASSERT_TRUE(second) << second.error().message;
        EXPECT_EQ(model::write_document(*second), written);
    }
}

TEST(MnxModel, ReadsTheMinimalDocument) {
    const auto root = model::read_document(with_content(R"({"duration": {"base": "whole"}, "rest": {}})"));
    ASSERT_TRUE(root) << root.error().message;
    ASSERT_EQ(root->parts.size(), 1U);
    const auto &content = root->parts[0].measures[0].sequences[0].content;
    ASSERT_EQ(content.size(), 1U);
    const auto *event = std::get_if<model::Event>(&content[0]);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->duration.base, model::NoteValueBase::WHOLE);
    EXPECT_TRUE(event->rest.has_value());
    EXPECT_EQ(root->global.measures[0].time->unit, model::TimeSignatureUnit::QUARTER);
}

TEST(MnxModel, ContentAlternativesFollowTheirType) {
    // An element without "type" is an event even when its keys also fit a space.
    const auto root = model::read_document(with_content(R"({"duration": {"base": "half"}},
        {"type": "space", "duration": [1, 4]},
        {"type": "event", "duration": {"base": "quarter"}, "rest": {}},
        {"type": "grace", "content": [{"type": "event", "duration": {"base": "eighth"}, "notes": [{"pitch": {"step": "C", "octave": 4}}]}]},
        {"type": "tuplet", "inner": {"duration": {"base": "eighth"}, "multiple": 3}, "outer": {"duration": {"base": "quarter"}, "multiple": 1},
         "content": [{"duration": {"base": "eighth"}, "rest": {}}]})"));
    ASSERT_TRUE(root) << root.error().message;
    const auto &content = root->parts[0].measures[0].sequences[0].content;
    ASSERT_EQ(content.size(), 5U);
    EXPECT_TRUE(std::holds_alternative<model::Event>(content[0]));
    EXPECT_TRUE(std::holds_alternative<model::Space>(content[1]));
    EXPECT_EQ(std::get<model::Space>(content[1]).duration, (model::Fraction{1, 4}));
    EXPECT_TRUE(std::holds_alternative<model::Event>(content[2]));
    EXPECT_TRUE(std::holds_alternative<model::Grace>(content[3]));
    EXPECT_TRUE(std::holds_alternative<model::Tuplet>(content[4]));
    EXPECT_TRUE(std::holds_alternative<model::Event>(std::get<model::Tuplet>(content[4]).content[0]));
}

TEST(MnxModel, RejectsSchemaViolations) {
    const std::string rest = R"({"duration": {"base": "whole"}, "rest": {}})";
    // Missing required member, unknown member, wrong type, unknown enum value.
    EXPECT_NE(rejection(R"({"mnx": {"version": 1}, "parts": []})").find("missing_key"), std::string::npos);
    EXPECT_NE(rejection(with_content(R"({"duration": {"base": "whole"}, "rest": {}, "colour": "red"})")).find("unknown_key"),
              std::string::npos);
    rejection(with_content(R"({"duration": {"base": "whole", "dots": "1"}, "rest": {}})"));
    rejection(with_content(R"({"duration": {"base": "sixteenth"}, "rest": {}})"));
    // Literal tags and unknown alternatives.
    rejection(with_content(R"({"type": "chord", "duration": {"base": "whole"}})"));
    rejection(with_content(R"({"type": "space", "duration": {"base": "whole"}})"));
    // Patterns and integer enums.
    EXPECT_NE(rejection(with_content(R"({"id": "has space", "duration": {"base": "whole"}, "rest": {}})")).find("id must be"),
              std::string::npos);
    rejection(with_content(R"({"id": "", "duration": {"base": "whole"}, "rest": {}})"));
    std::string time_unit(minimal);
    time_unit.replace(time_unit.find("\"unit\": 4"), 9, "\"unit\": 3");
    EXPECT_NE(rejection(time_unit).find("TimeSignatureUnit"), std::string::npos);
    // Integers must be integers in range.
    rejection(with_content(R"({"duration": {"base": "whole", "dots": 1.0}, "rest": {}})"));
    rejection(with_content(R"({"duration": {"base": "whole", "dots": 18446744073709551615}, "rest": {}})"));
    // Nothing may follow the document.
    rejection(with_content(rest) + " {}");
}

TEST(MnxModel, AcceptsTheDocumentedLeniencies) {
    // A byte order mark, null for an absent optional member, and arbitrary comments and
    // vendor extensions.
    EXPECT_TRUE(model::read_document("\xEF\xBB\xBF" + with_content(R"({"duration": {"base": "whole"}, "rest": {}})")));
    EXPECT_TRUE(model::read_document(with_content(R"({"duration": {"base": "whole"}, "rest": {}, "staff": null})")));
    EXPECT_TRUE(
        model::read_document(with_content(R"({"_c": "note", "_x": {"v": {"a": [1]}}, "duration": {"base": "whole"}, "rest": {}})")));
}

TEST(MnxModel, ErrorsAreLocated) {
    // The error points at the offending line even inside nested content.
    const std::string text = with_content(R"({"duration": {"base": "whole"}, "rest": {}},
      {"type": "tuplet", "inner": {"duration": {"base": "eighth"}, "multiple": 3}, "outer": {"duration": {"base": "quarter"}, "multiple": 1},
       "content": [{"duration": {"base": "eighth"}, "rest": {}, "bogus": 1}]})");
    const auto result = model::read_document(text);
    ASSERT_FALSE(result);
    // glaze reports an unknown key at its first character.
    EXPECT_EQ(text.substr(result.error().offset, 5), "bogus") << result.error().message;
    EXPECT_TRUE(result.error().message.starts_with("6:")) << result.error().message;
    EXPECT_NE(result.error().message.find("unknown_key"), std::string::npos) << result.error().message;
}

namespace {

model::Root parse(std::string_view text) {
    auto root = model::read_document(text);
    EXPECT_TRUE(root) << root.error().message;
    return root ? *std::move(root) : model::Root{};
}

// Expects indexing to fail and returns the error pointers.
std::vector<std::string> semantic_errors(std::string_view text) {
    const auto root = parse(text);
    const auto indexed = model::build_index(root);
    EXPECT_FALSE(indexed) << text;
    std::vector<std::string> pointers;
    if (!indexed)
        for (const auto &d : indexed.error())
            if (d.severity == model::Severity::ERROR) pointers.push_back(d.pointer);
    return pointers;
}

bool contains(const std::vector<std::string> &pointers, std::string_view pointer) {
    return std::ranges::find(pointers, pointer) != pointers.end();
}

} // namespace

TEST(MnxIndex, TestInputsIndexWithoutErrors) {
    for (const auto &path : inputs()) {
        SCOPED_TRACE(path.string());
        const auto root = model::read_document(read_file(path));
        ASSERT_TRUE(root);
        const auto indexed = model::build_index(*root);
        if (!indexed) {
            for (const auto &d : indexed.error()) ADD_FAILURE() << d.pointer << ": " << d.message;
        }
    }
}

TEST(MnxIndex, EventTimingFollowsTupletsAndGraces) {
    const auto root = parse(with_content(R"(
        {"type": "grace", "content": [
            {"id": "g1", "duration": {"base": "16th"}, "notes": [{"pitch": {"step": "D", "octave": 4}}]},
            {"id": "g2", "duration": {"base": "16th"}, "notes": [{"pitch": {"step": "E", "octave": 4}}]}]},
        {"id": "q", "duration": {"base": "quarter"}, "notes": [{"pitch": {"step": "C", "octave": 4}}]},
        {"type": "tuplet", "inner": {"duration": {"base": "eighth"}, "multiple": 3}, "outer": {"duration": {"base": "quarter"}, "multiple": 1},
         "content": [{"id": "t1", "duration": {"base": "eighth"}, "rest": {}}, {"id": "t2", "duration": {"base": "eighth"}, "rest": {}},
                     {"id": "t3", "duration": {"base": "eighth"}, "rest": {}}]},
        {"type": "space", "duration": [1, 4]},
        {"id": "last", "duration": {"base": "quarter"}, "rest": {}})"));
    const auto indexed = model::build_index(root);
    ASSERT_TRUE(indexed);
    const auto &index = indexed->index;
    const auto event = [&](std::string_view id) { return *index.find_event(id); };
    EXPECT_EQ(event("g1").grace_index, 2U);
    EXPECT_EQ(event("g2").grace_index, 1U);
    EXPECT_EQ(event("g2").container, model::EventContainer::GRACE);
    EXPECT_EQ(event("g2").duration, model::Rational(0));
    EXPECT_EQ(event("q").start, model::Rational(0));
    EXPECT_EQ(event("q").grace_index, 0U);
    EXPECT_EQ(event("t1").start, model::Rational(1, 4));
    EXPECT_EQ(event("t2").start, model::Rational(1, 3));
    EXPECT_EQ(event("t3").duration, model::Rational(1, 12));
    EXPECT_EQ(event("t3").container, model::EventContainer::TUPLET);
    EXPECT_EQ(event("last").start, model::Rational(3, 4));
    EXPECT_EQ(event("last").pointer, "/parts/0/measures/0/sequences/0/content/4");
    // Only the missing clef is reported, as a warning.
    ASSERT_EQ(indexed->warnings.size(), 1U);
    EXPECT_EQ(indexed->warnings[0].severity, model::Severity::WARNING);
}

TEST(MnxIndex, RejectsSemanticErrors) {
    const std::string note = R"("notes": [{"pitch": {"step": "C", "octave": 4}}])";
    // Content longer than the bar, and a tuplet whose content overflows its inner value.
    EXPECT_TRUE(contains(
        semantic_errors(with_content(R"({"duration": {"base": "whole"}, "rest": {}}, {"duration": {"base": "quarter"}, "rest": {}})")),
        "/parts/0/measures/0/sequences/0"));
    EXPECT_TRUE(contains(semantic_errors(with_content(R"({"type": "tuplet", "inner": {"duration": {"base": "eighth"}, "multiple": 3},
        "outer": {"duration": {"base": "quarter"}, "multiple": 1}, "content": [{"duration": {"base": "half"}, "rest": {}}]})")),
                         "/parts/0/measures/0/sequences/0/content/0"));
    // Duplicate ids, a staff the part does not have, and a tie to a missing note.
    EXPECT_TRUE(
        contains(semantic_errors(with_content(
                     R"({"id": "a", "duration": {"base": "half"}, "rest": {}}, {"id": "a", "duration": {"base": "half"}, "rest": {}})")),
                 "/parts/0/measures/0/sequences/0/content/1"));
    EXPECT_TRUE(contains(semantic_errors(with_content(R"({"duration": {"base": "whole"}, "staff": 2, "rest": {}})")),
                         "/parts/0/measures/0/sequences/0/content/0/staff"));
    EXPECT_TRUE(contains(
        semantic_errors(with_content(
            R"({"duration": {"base": "whole"}, "notes": [{"pitch": {"step": "C", "octave": 4}, "ties": [{"target": "nowhere"}]}]})")),
        "/parts/0/measures/0/sequences/0/content/0/notes/0/ties/0/target"));
    // An event that is neither a rest nor has notes, and a zero denominator.
    EXPECT_TRUE(contains(semantic_errors(with_content(R"({"duration": {"base": "whole"}})")), "/parts/0/measures/0/sequences/0/content/0"));
    EXPECT_TRUE(contains(semantic_errors(with_content(R"({"type": "space", "duration": [1, 0]})")),
                         "/parts/0/measures/0/sequences/0/content/0/duration"));
    // A beam naming an event that does not exist.
    std::string beam(minimal);
    beam.replace(beam.find("\"sequences\""), 0, R"("beams": [{"events": ["x", "y"]}], )");
    beam.replace(beam.find('%'), 1, R"({"id": "x", "duration": {"base": "eighth"}, )" + note + "}");
    EXPECT_TRUE(contains(semantic_errors(beam), "/parts/0/measures/0/beams/0"));
}

TEST(MnxIndex, RejectsValuesTheImporterCannotHandle) {
    // A fraction has exactly two elements.
    rejection(with_content(R"({"type": "space", "duration": [1]})"));
    // A sub-beam names only events of its parent beam.
    std::string beams(minimal);
    beams.replace(beams.find("\"sequences\""), 0, R"("beams": [{"events": ["a", "b"], "beams": [{"events": ["c"]}]}], )");
    beams.replace(beams.find('%'), 1,
                  R"({"id": "a", "duration": {"base": "16th"}, "rest": {}}, {"id": "b", "duration": {"base": "16th"}, "rest": {}},
        {"id": "c", "duration": {"base": "16th"}, "rest": {}}, {"duration": {"base": "half"}, "rest": {}},
        {"duration": {"base": "eighth"}, "rest": {}}, {"duration": {"base": "16th"}, "rest": {}})");
    EXPECT_TRUE(contains(semantic_errors(beams), "/parts/0/measures/0/beams/0/beams/0"));
    // Durations whose sum cannot be represented.
    EXPECT_FALSE(
        semantic_errors(with_content(R"({"type": "space", "duration": [1, 2147483647]}, {"type": "space", "duration": [1, 2147483629]},
        {"type": "space", "duration": [1, 2147483587]}, {"duration": {"base": "quarter"}, "rest": {}})"))
            .empty());
    // Absurd staff counts, octaves and diatonic deltas, and a tempo at an invalid position.
    std::string staves(minimal);
    staves.replace(staves.find("\"measures\": [{\"sequences\""), 0, R"("staves": 10000000, )");
    staves.replace(staves.find('%'), 1, R"({"duration": {"base": "whole"}, "rest": {}})");
    EXPECT_TRUE(contains(semantic_errors(staves), "/parts/0/staves"));
    EXPECT_TRUE(contains(
        semantic_errors(with_content(R"({"duration": {"base": "whole"}, "notes": [{"pitch": {"step": "C", "octave": 2147483647}}]})")),
        "/parts/0/measures/0/sequences/0/content/0/notes/0/pitch/octave"));
    EXPECT_TRUE(contains(
        semantic_errors(with_content(
            R"({"duration": {"base": "whole"}, "notes": [{"pitch": {"step": "C", "octave": 4}, "written": {"diatonicDelta": 2147483647}}]})")),
        "/parts/0/measures/0/sequences/0/content/0/notes/0/written/diatonicDelta"));
    std::string tempo(minimal);
    tempo.replace(tempo.find("\"time\""), 0,
                  R"("tempos": [{"bpm": 120, "value": {"base": "quarter"}, "location": {"fraction": [1, 0]}}], )");
    tempo.replace(tempo.find('%'), 1, R"({"duration": {"base": "whole"}, "rest": {}})");
    EXPECT_TRUE(contains(semantic_errors(tempo), "/global/measures/0/tempos/0/location/fraction"));
}

TEST(MnxIndex, OttavaShiftsCoverTheirSpan) {
    const auto root = parse(R"({
      "mnx": {"version": 1},
      "global": {"measures": [{"id": "m1", "time": {"count": 2, "unit": 4}}, {"id": "m2"}]},
      "parts": [{"measures": [
        {"clefs": [{"clef": {"sign": "G", "staffPosition": -2}}],
         "ottavas": [{"value": 1, "position": {"fraction": [1, 4]}, "end": {"measure": "m1", "position": {"fraction": [2, 4]}}}],
         "sequences": [{"content": [{"id": "a", "duration": {"base": "quarter"}, "notes": [{"pitch": {"step": "C", "octave": 5}}]},
                                    {"id": "b", "duration": {"base": "quarter"}, "notes": [{"pitch": {"step": "C", "octave": 5}}]}]}]},
        {"sequences": [{"content": [{"id": "c", "duration": {"base": "half"}, "notes": [{"pitch": {"step": "C", "octave": 5}}]}]}]}]}]
    })");
    const auto indexed = model::build_index(root);
    ASSERT_TRUE(indexed) << indexed.error().front().message;
    EXPECT_EQ(indexed->index.find_event("a")->ottava_shift, 0);
    EXPECT_EQ(indexed->index.find_event("b")->ottava_shift, -1);
    EXPECT_EQ(indexed->index.find_event("c")->ottava_shift, 0);
    EXPECT_EQ(model::current_time(root, indexed->index, 1)->count, 2); // inherited from m1
}

TEST(MnxLayout, SpansOfNestedGroupsAndDefaults) {
    const auto root = parse(R"({
      "mnx": {"version": 1},
      "global": {"measures": [{}]},
      "parts": [{"id": "P1", "name": "Flute", "measures": [{"sequences": []}]},
                {"id": "P2", "name": "Piano", "staves": 2, "measures": [{"sequences": []}]}],
      "layouts": [{"id": "full", "content": [
        {"type": "staff", "sources": [{"part": "P1"}]},
        {"type": "group", "symbol": "brace", "content": [
          {"type": "staff", "sources": [{"part": "P2", "staff": 1}]},
          {"type": "staff", "sources": [{"part": "P2", "staff": 2}]}]}]}]
    })");
    const auto *layout = model::full_score_layout(root);
    ASSERT_NE(layout, nullptr);
    const auto spans = model::layout_spans(*layout);
    ASSERT_EQ(spans.size(), 4U);
    EXPECT_EQ(spans[0].kind, model::LayoutSpan::Kind::STAFF);
    EXPECT_EQ(spans[1].kind, model::LayoutSpan::Kind::GROUP); // the group sorts before its first staff
    EXPECT_EQ(spans[1].start, 1U);
    EXPECT_EQ(spans[1].end, 2U);
    EXPECT_EQ(spans[1].depth, 0U);                                // a root-level group
    EXPECT_EQ(spans[1].barline, model::BarlineOverride::UNIFIED); // instrument style, one part
    EXPECT_EQ(spans[2].depth, 2U);
    const auto defaults = model::default_layout_spans(root);
    ASSERT_EQ(defaults.size(), 4U);
    EXPECT_EQ(defaults[0].label, "Flute");
    EXPECT_EQ(defaults[1].symbol, model::StaffSymbol::BRACE);
    EXPECT_FALSE(defaults[2].label.has_value());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
