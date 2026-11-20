#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
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
    auto result = document.query({.types = std::move(types), .staves = {0}, .layout_boxes = true});
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

} // namespace
