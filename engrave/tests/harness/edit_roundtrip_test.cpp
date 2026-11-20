#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/state.h"
#include "engrave/src/runtime/score_io.h"
#include "engrave/tests/core_test_env.h"
#include "engrave/tests/harness/harness.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/spanner.h"

namespace {

using engrave::ElementId;
using engrave::ElementInfo;
using engrave::Enum;
using engrave::Format;
using engrave::ResetProperty;
using engrave::SetPitch;
using engrave::SetProperty;
using engrave::harness::find_id;

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

std::string native_source(engrave::Document &document) {
    return music::export_native_score(engrave::DocumentAccess::context(document), engrave::DocumentAccess::project(document), "mscx",
                                      music::ExportOptions{.standalone_score = true, .transaction_snapshot = true}, {});
}

std::string token(engrave::Document &document, const ElementId &id, const std::string &name) {
    const auto read = document.get(id, {name});
    if (!read) return "error: " + read.error().message;
    if (read->size() != 1) return "error: no single value";
    const auto *value = std::get_if<Enum>(&read->front().value);
    return value ? value->token : "error: not an enum";
}

// What layout draws of a spanner: its first segment. A query boxes the spanner itself.
std::optional<engrave::LayoutBox> first_segment_box(engrave::Document &document, const ElementId &id) {
    const auto &source = static_cast<const mu::engraving::Spanner &>(engrave::harness::element(document, id));
    const auto *copy = engrave::harness::master_of(document).layout_copy(source);
    const auto *segment = static_cast<const mu::engraving::Spanner &>(copy ? *copy : source).front_segment();
    if (!segment) return std::nullopt;
    const auto box = segment->page_bounding_rect(mu::engraving::LdAccess::MAYBE_NOTINITED);
    return engrave::LayoutBox{0, box.x(), box.y(), box.width(), box.height()};
}

// The staff-0 elements of the given types with their page boxes.
std::vector<ElementInfo> laid_out(engrave::Document &document, std::vector<std::string> types) {
    types.push_back("Note");
    auto result = document.query({.types = std::move(types), .staves = {0}, .props = {"placement"}, .layout_boxes = true});
    EXPECT_TRUE(result) << result.error().message;
    if (!result) return {};
    for (auto &element : result->elements)
        if (element.type == "HairPin") element.box = first_segment_box(document, element.id);
    return std::move(result->elements);
}

enum struct Side { ABOVE, BELOW, NEITHER };

// Where an element sits against the notes of its own measure.
Side side_of_notes(const std::vector<ElementInfo> &elements, const ElementId &id) {
    const auto item = std::ranges::find(elements, id, &ElementInfo::id);
    if (item == elements.end() || !item->box) return Side::NEITHER;
    double top = std::numeric_limits<double>::max();
    double bottom = std::numeric_limits<double>::lowest();
    for (const auto &element : elements) {
        if (element.type != "Note" || element.measure_index != item->measure_index || !element.box) continue;
        top = std::min(top, element.box->y);
        bottom = std::max(bottom, element.box->y + element.box->height);
    }
    if (top > bottom) return Side::NEITHER;
    if (item->box->y + item->box->height <= top) return Side::ABOVE;
    if (item->box->y >= bottom) return Side::BELOW;
    return Side::NEITHER;
}

TEST(VoicePlacement, PlacementEditMovesDirectionItemsAboveAndSurvivesSaving) {
    auto document = open("layout_elements.mscx");
    ASSERT_TRUE(document);
    const std::vector<std::string> types{"Dynamic", "Expression", "HairPin"};
    std::vector<ElementId> ids;
    for (const auto &type : types) ids.push_back(find_id(*document, type));
    const auto before = native_source(*document);
    const auto source_layout = laid_out(*document, types);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        SCOPED_TRACE(types[i]);
        EXPECT_EQ(token(*document, ids[i], "placement"), "below");
        EXPECT_EQ(side_of_notes(source_layout, ids[i]), Side::BELOW);
    }
    ASSERT_TRUE(document->checkpoint("source"));

    engrave::Batch above;
    for (const auto &id : ids) above.ops.push_back(SetProperty{id, "placement", Enum{"above"}});
    const auto edited = document->edit(above);
    ASSERT_TRUE(edited) << edited.error().message;
    EXPECT_FALSE(edited->rejection);
    EXPECT_EQ(edited->revision, 2u);
    const auto edited_expression = document->get(ids[1], {"placement"});
    ASSERT_TRUE(edited_expression);
    EXPECT_FALSE(edited_expression->front().styled);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        SCOPED_TRACE(types[i]);
        EXPECT_EQ(token(*document, ids[i], "placement"), "above");
        EXPECT_EQ(token(*document, ids[i], "direction"), "up");
    }
    const auto edited_layout = laid_out(*document, types);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        SCOPED_TRACE(types[i]);
        EXPECT_EQ(token(*document, ids[i], "placement"), "above");
        EXPECT_EQ(side_of_notes(edited_layout, ids[i]), Side::ABOVE);
    }

    for (const Format format : {Format::MSCX, Format::MSCZ}) {
        SCOPED_TRACE(format == Format::MSCX ? "mscx" : "mscz");
        auto reopened = reopen(*document, format);
        ASSERT_TRUE(reopened);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            SCOPED_TRACE(types[i]);
            EXPECT_EQ(token(*reopened, ids[i], "placement"), "above");
        }
        const auto reopened_layout = laid_out(*reopened, types);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            SCOPED_TRACE(types[i]);
            EXPECT_EQ(side_of_notes(reopened_layout, ids[i]), Side::ABOVE);
        }
    }

    ASSERT_TRUE(document->restore_checkpoint("source"));
    for (std::size_t i = 0; i < ids.size(); ++i) {
        SCOPED_TRACE(types[i]);
        EXPECT_EQ(token(*document, ids[i], "placement"), "below");
        EXPECT_EQ(token(*document, ids[i], "direction"), "auto");
    }
    EXPECT_EQ(native_source(*document), before);

    ASSERT_TRUE(document->edit(above));
    engrave::Batch reset;
    for (const auto &id : ids) reset.ops.push_back(ResetProperty{id, "placement"});
    ASSERT_TRUE(document->edit(reset));
    const auto reset_expression = document->get(ids[1], {"placement"});
    ASSERT_TRUE(reset_expression);
    EXPECT_TRUE(reset_expression->front().styled);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        SCOPED_TRACE(types[i]);
        EXPECT_EQ(token(*document, ids[i], "direction"), "auto");
    }
    EXPECT_EQ(native_source(*document), before);
    const auto reset_layout = laid_out(*document, types);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        SCOPED_TRACE(types[i]);
        EXPECT_EQ(token(*document, ids[i], "placement"), "below");
        EXPECT_EQ(side_of_notes(reset_layout, ids[i]), Side::BELOW);
    }
}

// Two notes, G_G and J_J, both pitch 60, joined by tie H_H; ties_mismatched.mscx
// has J_J at pitch 62.
std::unique_ptr<engrave::Document> open_ties(std::string_view name) {
    auto document = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/write470" / name);
    EXPECT_TRUE(document) << name << ": " << document.error().message;
    return document ? std::move(*document) : nullptr;
}

ElementId id(std::string_view text) {
    const auto parsed = engrave::element_id_from_string(text);
    if (!parsed) throw std::invalid_argument(std::string(text) + " is not an element id");
    return *parsed;
}

TEST(VoicePlacement, CreationAboveSurvivesLayoutAndNativeRoundTrip) {
    auto document = open_ties("ties.mscx");
    ASSERT_TRUE(document);
    const engrave::Address at = engrave::SegmentAddress{id("C_C"), 0, 0, {0, 1}, "ChordRest"};
    const auto result = document->edit({{engrave::Add{at, "Dynamic", {{"DYNAMIC_TYPE", Enum{"ff"}}, {"placement", Enum{"above"}}}},
                                         engrave::Add{at, "Expression", {{"text", std::string("dolce")}, {"placement", Enum{"above"}}}},
                                         engrave::AddSpanner{"HairPin", id("F_F"), id("I_I"), {{"placement", Enum{"above"}}}}}});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    ASSERT_EQ(result->applied, 3u);
    std::vector<ElementId> ids;
    for (const auto &operation : result->ops) {
        ASSERT_EQ(operation.created.size(), 1u);
        ASSERT_TRUE(operation.created.front());
        ids.push_back(*operation.created.front());
        EXPECT_EQ(token(*document, ids.back(), "direction"), "up");
    }

    const auto verify = [&ids](engrave::Document &score) {
        const auto layout = laid_out(score, {"Dynamic", "Expression", "HairPin"});
        for (const ElementId &element_id : ids) {
            const auto found = std::ranges::find(layout, element_id, &ElementInfo::id);
            ASSERT_NE(found, layout.end());
            SCOPED_TRACE(found->type);
            ASSERT_EQ(found->props.size(), 1u);
            EXPECT_EQ(std::get<Enum>(found->props.front().value).token, "above");
            EXPECT_FALSE(found->props.front().styled);
            EXPECT_EQ(side_of_notes(layout, element_id), Side::ABOVE);
        }
    };
    verify(*document);
    for (const Format format : {Format::MSCX, Format::MSCZ}) {
        SCOPED_TRACE(format == Format::MSCX ? "mscx" : "mscz");
        auto reopened = reopen(*document, format);
        ASSERT_TRUE(reopened);
        verify(*reopened);
    }
}

TEST(VoicePlacement, AutomaticPlacementReadsStyleAndRepeatedResetIsANoOp) {
    auto document = open("layout_elements.mscx");
    ASSERT_TRUE(document);
    const auto dynamic = find_id(*document, "Dynamic");
    const auto styled = document->edit({{engrave::SetStyle{{}, "dynamicsHairpinVoiceBasedPlacement", Enum{"up"}}}});
    ASSERT_TRUE(styled) << styled.error().message;
    ASSERT_FALSE(styled->rejection) << styled->rejection->error.message;
    EXPECT_EQ(token(*document, dynamic, "direction"), "auto");
    EXPECT_EQ(token(*document, dynamic, "placement"), "above");
    const auto layout = laid_out(*document, {"Dynamic"});
    EXPECT_EQ(side_of_notes(layout, dynamic), Side::ABOVE);
    EXPECT_EQ(token(*document, dynamic, "placement"), "above");

    for (int i = 0; i < 2; ++i) {
        const auto reset = document->edit({{ResetProperty{dynamic, "placement"}}});
        ASSERT_TRUE(reset) << reset.error().message;
        EXPECT_EQ(reset->applied, 1u);
        EXPECT_EQ(reset->revision, styled->revision);
        EXPECT_EQ(token(*document, dynamic, "placement"), "above");
    }
}

std::vector<std::string> tie_warnings(const engrave::EditResult &result) {
    std::vector<std::string> warnings;
    for (const auto &diagnostic : result.diagnostics)
        if (diagnostic.severity == engrave::Severity::WARNING && diagnostic.message.contains("different pitches"))
            warnings.push_back(diagnostic.message);
    return warnings;
}

TEST(MismatchedTie, PitchEditOfOneTiedNoteCommitsAndWarns) {
    auto document = open_ties("ties.mscx");
    ASSERT_TRUE(document);
    const auto result = document->edit({{SetPitch{id("G_G"), 62, 16}}});
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result->applied, 1u);
    EXPECT_FALSE(result->rejection);
    EXPECT_EQ(std::get<std::int64_t>(engrave::harness::property(*document, id("G_G"), "pitch")), 62);
    const auto warnings = tie_warnings(*result);
    ASSERT_EQ(warnings.size(), 1u);
    for (const auto *named : {"H_H", "G_G", "J_J"}) EXPECT_TRUE(warnings.front().contains(named)) << warnings.front();
}

TEST(MismatchedTie, PitchEditOfBothTiedNotesInOneBatchDoesNotWarn) {
    auto document = open_ties("ties.mscx");
    ASSERT_TRUE(document);
    const auto result = document->edit({{SetPitch{id("G_G"), 62, 16}, SetPitch{id("J_J"), 62, 16}}});
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result->applied, 2u);
    EXPECT_TRUE(tie_warnings(*result).empty());
}

TEST(MismatchedTie, NoOpPitchEditOfMismatchedTieDoesNotWarn) {
    auto document = open_ties("ties_mismatched.mscx");
    ASSERT_TRUE(document);
    const auto revision = document->revision();
    const auto result = document->edit({{SetPitch{id("G_G"), 60, 14}}});
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result->applied, 1u);
    EXPECT_EQ(result->revision, revision);
    EXPECT_TRUE(result->diagnostics.empty());
}

TEST(MismatchedTie, EditsThatLeaveNoMismatchedTieDoNotWarn) {
    {
        auto document = open_ties("ties_mismatched.mscx");
        ASSERT_TRUE(document);
        const auto recolored = document->edit({{SetProperty{id("G_G"), "color", engrave::Color{255, 0, 0}}}});
        ASSERT_TRUE(recolored) << recolored.error().message;
        EXPECT_EQ(recolored->revision, 2u);
        EXPECT_TRUE(recolored->diagnostics.empty());
        const auto repaired = document->edit({{SetPitch{id("J_J"), 60, 14}}});
        ASSERT_TRUE(repaired) << repaired.error().message;
        EXPECT_EQ(repaired->revision, 3u);
        EXPECT_TRUE(tie_warnings(*repaired).empty());
    }
    auto document = open_ties("ties.mscx");
    ASSERT_TRUE(document);
    const auto recolored = document->edit({{SetProperty{id("J_J"), "color", engrave::Color{0, 0, 255}}}});
    ASSERT_TRUE(recolored) << recolored.error().message;
    EXPECT_TRUE(recolored->diagnostics.empty());
}

} // namespace
