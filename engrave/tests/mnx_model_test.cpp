// SPDX-License-Identifier: GPL-3.0-only
// The MNX document model: every MNX test input reads and round-trips, and reading rejects
// schema violations with a located error.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "model/document.h"

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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
