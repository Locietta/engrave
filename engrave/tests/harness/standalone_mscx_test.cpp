#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "engrave/tests/core_test_env.h"
#include "engrave/tests/harness/harness.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/style/style.h"

namespace {

using engrave::Format;
using engrave::PositionKind;
using mu::engraving::MStyle;
using mu::engraving::Sid;

auto &g_corpus = engrave::test::core_env().golden_corpus;
auto &g_engine = engrave::test::core_env().engine;

std::unique_ptr<engrave::Document> open(std::string_view name) {
    const auto found = std::ranges::find(g_corpus, name, [](const std::filesystem::path &path) { return path.filename().string(); });
    EXPECT_NE(found, g_corpus.end()) << name << " is not in the corpus";
    if (found == g_corpus.end()) return nullptr;
    auto document = g_engine->open(*found);
    EXPECT_TRUE(document) << name << ": " << document.error().message;
    return document ? std::move(*document) : nullptr;
}

std::unique_ptr<engrave::Document> reopen(engrave::Document &document, Format format) {
    auto saved = document.export_score({.format = format});
    EXPECT_TRUE(saved) << saved.error().message;
    if (!saved) return nullptr;
    auto reopened = g_engine->open(*saved, {.format = format});
    EXPECT_TRUE(reopened) << reopened.error().message;
    return reopened ? std::move(*reopened) : nullptr;
}

const MStyle &style_of(engrave::Document &document) { return engrave::harness::master_of(document).style(); }

void expect_style(const MStyle &actual, const MStyle &expected) {
    for (int i = 0; i < static_cast<int>(Sid::STYLES); ++i) {
        const auto sid = static_cast<Sid>(i);
        EXPECT_TRUE(actual.value(sid) == expected.value(sid)) << MStyle::value_name(sid);
    }
}

void expect_same_layout(engrave::Document &actual_document, engrave::Document &expected_document) {
    const auto expected = expected_document.positions(PositionKind::SEGMENT);
    const auto actual = actual_document.positions(PositionKind::SEGMENT);
    ASSERT_TRUE(expected && actual);
    EXPECT_EQ(actual->page_width, expected->page_width);
    EXPECT_EQ(actual->page_height, expected->page_height);
    ASSERT_EQ(actual->elements.size(), expected->elements.size());
    for (std::size_t i = 0; i < actual->elements.size(); ++i) {
        const auto &a = actual->elements[i];
        const auto &e = expected->elements[i];
        EXPECT_EQ(a.page, e.page) << "segment " << i;
        EXPECT_EQ(a.x, e.x) << "segment " << i;
        EXPECT_EQ(a.y, e.y) << "segment " << i;
        EXPECT_EQ(a.sx, e.sx) << "segment " << i;
        EXPECT_EQ(a.sy, e.sy) << "segment " << i;
    }
    const auto actual_pages = actual_document.page_count();
    const auto expected_pages = expected_document.page_count();
    ASSERT_TRUE(actual_pages && expected_pages);
    EXPECT_EQ(*actual_pages, *expected_pages);
}

// Scores whose defaultsVersion predates the current defaults: an optimized style
// omits values that the reader then takes from the current defaults.
TEST(StandaloneMscx, LegacyDefaultsScoresReopenWithTheirStyleAndLayout) {
    for (const std::string_view name : {"lyrics.mscx", "repeat70.mscx", "layout_elements.mscx"}) {
        SCOPED_TRACE(name);
        auto source = open(name);
        ASSERT_TRUE(source);
        ASSERT_LT(style_of(*source).default_style_version(), 500);
        auto mscx = reopen(*source, Format::MSCX);
        auto mscz = reopen(*source, Format::MSCZ);
        ASSERT_TRUE(mscx && mscz);
        expect_style(style_of(*mscx), style_of(*source));
        expect_same_layout(*mscx, *mscz);
    }
}

TEST(StandaloneMscx, LegacyDefaultsExcerptReopensWithItsStyleAndLayout) {
    auto source = open("lyrics.mscx");
    ASSERT_TRUE(source);
    ASSERT_TRUE(source->edit({{engrave::GenerateExcerpts{{engrave::harness::find_id(*source, "Part")}}}}));
    const auto excerpt = engrave::harness::excerpt(*source, 0);
    const MStyle &excerpt_style = engrave::harness::master_of(*source).excerpts().front()->excerpt_score()->style();
    ASSERT_LT(excerpt_style.default_style_version(), 500);
    const auto reopen_excerpt = [&](Format format, engrave::CaptureOptions capture) -> std::unique_ptr<engrave::Document> {
        auto saved = source->export_score({.format = format, .score = excerpt, .capture = std::move(capture)});
        EXPECT_TRUE(saved) << saved.error().message;
        if (!saved) return nullptr;
        auto reopened = g_engine->open(*saved, {.format = format});
        EXPECT_TRUE(reopened) << reopened.error().message;
        return reopened ? std::move(*reopened) : nullptr;
    };
    auto mscz = reopen_excerpt(Format::MSCZ, {});
    ASSERT_TRUE(mscz);
    // A byte limit selects the writer that streams the excerpt's score XML directly.
    for (const bool limited : {false, true}) {
        SCOPED_TRACE(limited ? "byte limit" : "no limit");
        engrave::CaptureOptions capture;
        if (limited) capture.max_bytes = 64u * 1024 * 1024;
        auto mscx = reopen_excerpt(Format::MSCX, std::move(capture));
        ASSERT_TRUE(mscx);
        // Generated page sizes carry more digits than the writer keeps, so the archive
        // reopen is the reference for the excerpt's saved style.
        expect_style(style_of(*mscx), style_of(*mscz));
        expect_same_layout(*mscx, *mscz);
    }
}

} // namespace
