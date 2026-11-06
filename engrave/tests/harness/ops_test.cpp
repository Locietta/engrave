// The edit API (Document::edit), the snapshot round trip (S1) and the evidence of
// every registered OpCase (harness.h).
//
//   engrave_ops_test --assets=<resources> --corpus=<file or dir>... [--update-evidence]
//
// --update-evidence rewrites op_evidence.json from this run instead of comparing
// against it. The engine hands out random ids, as in production: the sequential ids
// of deterministic_ids collide with the ids of a score read back, so a round trip
// under them repairs ids that production never would.

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/tests/harness/harness.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/masterscore.h"
#include "engraving/editing/editproperty.h"
#include "engraving/editing/editstyle.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/style/style.h"

namespace {

using engrave::Batch;
using engrave::Color;
using engrave::ElementId;
using engrave::ErrorCode;
using engrave::ResetProperty;
using engrave::SetProperty;
using engrave::SetStyle;
using engrave::harness::find_id;
using engrave::harness::property;

const std::filesystem::path g_evidence = ENGRAVE_TEST_SOURCE_ROOT "/engrave/tests/harness/op_evidence.json";
std::vector<std::filesystem::path> g_corpus;
bool g_update_evidence = false;
std::unique_ptr<engrave::Engine> g_engine;

std::filesystem::path fixture(std::string_view name) {
    for (const auto &path : g_corpus) {
        if (path.filename() == name) return path;
    }
    return {};
}

std::unique_ptr<engrave::Document> open(std::string_view name) {
    const std::filesystem::path path = fixture(name);
    EXPECT_FALSE(path.empty()) << name << " is not in the corpus";
    auto document = g_engine->open(path);
    EXPECT_TRUE(document) << name << ": " << document.error().message;
    return document ? std::move(*document) : nullptr;
}

// --- Document::edit -----------------------------------------------------------

constexpr std::string_view fixture_name = "chordline_path.mscx";

TEST(Edit, CommitAdvancesTheRevisionAndClearsTheHistory) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const ElementId note = find_id(*document, "Note");
    const auto result = document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}, 1);
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result->revision, 2u);
    EXPECT_EQ(document->revision(), 2u);
    ASSERT_EQ(result->ops.size(), 1u);
    EXPECT_TRUE(result->ops[0].created.empty());
    EXPECT_TRUE(result->created.empty() && result->removed.empty() && result->replaced.empty());
    EXPECT_EQ(property(*document, note, "color"), engrave::Value(Color{255, 0, 0}));
    EXPECT_EQ(engrave::harness::master_of(*document).undo_stack()->size(), 0u);
}

TEST(Edit, ABatchThatChangesNothingKeepsTheRevision) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const ElementId note = find_id(*document, "Note");
    EXPECT_EQ(document->edit({})->revision, 1u);
    const auto same = document->edit({{SetProperty{note, "color", property(*document, note, "color")}}});
    ASSERT_TRUE(same) << same.error().message;
    EXPECT_EQ(same->revision, 1u);
}

TEST(Edit, AStaleRevisionConflicts) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const ElementId note = find_id(*document, "Note");
    const auto result = document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}, 7);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::REVISION_CONFLICT);
    EXPECT_EQ(document->revision(), 1u);
}

// Every error names the operation and leaves the score as it was, whether the
// operation fails while it is prepared or while it runs after others changed the score.
TEST(Edit, AFailingOperationRollsTheBatchBack) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const ElementId note = find_id(*document, "Note");
    const engrave::Value color = property(*document, note, "color");
    const SetProperty red{note, "color", Color{255, 0, 0}};

    struct Bad {
        engrave::Op op;
        ErrorCode code;
    };

    const Bad bad[] = {
        {SetProperty{note, "noSuchProperty", 1.0}, ErrorCode::INVALID_ARGUMENT},
        {SetProperty{note, "color", std::int64_t{3}}, ErrorCode::INVALID_ARGUMENT},
        {SetProperty{note, "pitch", std::int64_t{60}}, ErrorCode::INVALID_ARGUMENT},         // structural only
        {SetProperty{note, "beamMode", engrave::Enum{"auto"}}, ErrorCode::INVALID_ARGUMENT}, // not a Note property
        // An integer that does not fit the engine's int is not narrowed silently.
        {SetProperty{note, "fret", std::int64_t{4294967297LL}}, ErrorCode::INVALID_ARGUMENT},
        {SetStyle{{}, "measureNumberInterval", std::int64_t{4294967297LL}}, ErrorCode::INVALID_ARGUMENT},
        {SetProperty{ElementId{1, 2}, "color", Color{}}, ErrorCode::NOT_FOUND},
        {SetProperty{engrave::OpRef{1, 0}, "color", Color{}}, ErrorCode::INVALID_ARGUMENT},
        {ResetProperty{note, "noSuchProperty"}, ErrorCode::INVALID_ARGUMENT},
        {SetStyle{{}, "noSuchStyle", 1.0}, ErrorCode::INVALID_ARGUMENT},
        {SetStyle{{}, "staffDistance", true}, ErrorCode::INVALID_ARGUMENT},
        {SetStyle{{ElementId{1, 2}}, "staffDistance", 9.0}, ErrorCode::NOT_FOUND},
    };
    for (const Bad &entry : bad) {
        const auto result = document->edit({{red, entry.op}});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, entry.code) << result.error().message;
        EXPECT_EQ(result.error().op_index, 1u) << result.error().message;
        EXPECT_EQ(document->revision(), 1u);
        EXPECT_EQ(property(*document, note, "color"), color) << "the first operation was not rolled back";
    }
}

TEST(Edit, APoisonedDocumentRefusesWork) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
    hooks.policy = engrave::api::Policy::SNAPSHOT;
    hooks.fail_restore = true;
    hooks.before_step = [](std::size_t) { throw std::runtime_error("injected"); };
    const Batch batch{{SetStyle{{}, "staffDistance", 9.0}}};
    EXPECT_ANY_THROW((void) document->edit(batch));
    hooks = {};
    const auto expect_poisoned = [](const auto &result) {
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::POISONED);
        EXPECT_EQ(result.error().message, "an earlier edit left the document unusable");
    };
    // Invalid inputs would normally produce other errors. Poison takes priority,
    // and no audio sink may be called after the failed restore.
    const engrave::ScoreSel missing{ElementId{0x1234, 0x5678}};
    expect_poisoned(document->edit(batch, 999));
    expect_poisoned(document->export_score({}));
    expect_poisoned(document->outline(missing));
    expect_poisoned(document->query({.score = missing}));
    expect_poisoned(document->get(engrave::OpRef{0, 0}));
    expect_poisoned(document->page_count(missing));
    expect_poisoned(document->render({.score = missing}));
    expect_poisoned(document->positions(engrave::PositionKind::MEASURE, missing));
    expect_poisoned(document->positions(engrave::PositionKind::SEGMENT, missing, {}));
    expect_poisoned(document->audio_tracks(missing));
    bool called = false;
    const auto sink = [&called](std::span<const std::byte>) { called = true; };
    expect_poisoned(document->render_audio({.score = missing}, sink));
    EXPECT_FALSE(called);
}

// --- S1: the snapshot round trip ------------------------------------------------

// What the engine's writer and reader do not keep symmetric, on the first round trip
// of these corpus files; the second one is stable.
const std::map<std::string, std::string> s1_asymmetries = {
    // The importer gives a title an inline <font size>; read back, it is a size property.
    {"drums_import.musicxml", "files"},
    // The excerpt's page style is written out in full once it was read back.
    {"linked_parts.mscx", "files"},
};

std::string changed_fields(const engrave::harness::Fingerprint &before, const engrave::harness::Fingerprint &after) {
    std::string fields;
    for (const auto &field : engrave::harness::differences(before, after, {"layout", "externals", "objects", "registered"}))
        fields += (fields.empty() ? "" : " ") + field;
    return fields;
}

// A snapshot restore reads back what the writer wrote. Through the guard's own
// restore, the write of the restored score has to be the write it came from, byte
// for byte, with the same meaning and the same ids. Layout and the MIDI mapping are
// the harness's to judge per operation (C3/C4): a score the engine did not write
// itself is laid out differently once it is read back as a current one.
TEST(Snapshot, S1RestoreIsIdempotent) {
    // A throw, not a returned error: the guard unwinds a clean failure and only
    // restores the snapshot when the engine threw (see BatchGuard::roll_back), and
    // this test is about the restore.
    const engrave::api::Step fail = [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("restore"); };
    for (const auto &path : g_corpus) {
        auto document = g_engine->open(path);
        if (!document) continue;
        const std::string name = path.filename().string();
        const auto restore = [&document, &fail] {
            EXPECT_FALSE(engrave::api::BatchGuard(**document).run({fail}, engrave::api::Policy::SNAPSHOT, {}));
            return engrave::harness::fingerprint(**document);
        };
        const auto written = engrave::harness::fingerprint(**document);
        const auto once = restore();
        const auto twice = restore();
        const auto known = s1_asymmetries.find(name);
        const std::string expected = known == s1_asymmetries.end() ? "" : known->second;
        EXPECT_EQ(changed_fields(written, once), expected) << name << ": " << engrave::harness::explain(written, once);
        EXPECT_EQ(changed_fields(once, twice), "") << name << ": " << engrave::harness::explain(once, twice);
    }
}

// A batch whose rollback policy is Snapshot can fail with an error the operations
// themselves return (a stale handle, an engine refusal), not only with a throw. The
// score then has to come back as it was, layout included: reloading the snapshot is
// what moves it (S1 above), so the unwind is what the guard keeps. The validation's
// rollback probe found this batch; its 39 changed note boxes are the regression.
TEST(Edit, AFailedMixedBatchKeepsTheLayout) {
    auto document = open("layout_elements.mscx");
    ASSERT_TRUE(document);
    engrave::Query query;
    query.types = {"Note"};
    query.layout_boxes = true;
    const auto boxes = [&query](engrave::Document &doc) {
        std::map<engrave::ElementId, engrave::LayoutBox> found;
        const auto result = doc.query(query);
        EXPECT_TRUE(result) << result.error().message;
        if (!result) return found;
        for (const auto &item : result->elements) {
            if (item.box) found.emplace(item.id, *item.box);
        }
        return found;
    };
    const auto before = boxes(*document);
    ASSERT_GE(before.size(), 40u);
    const auto svg_before = document->render({.format = engrave::RenderFormat::SVG, .first_page = 0, .last_page = 0});
    ASSERT_TRUE(svg_before) << svg_before.error().message;
    ASSERT_FALSE(svg_before->pages.empty());

    const engrave::ElementId note = engrave::harness::find_id(*document, "Note");
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    const engrave::ElementId measure = outline->measures.front().id;
    const engrave::Batch batch{{engrave::Remove{note}, engrave::Remove{note}, engrave::SetLayoutBreak{measure, "line", false}}};
    const auto result = document->edit(batch);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::STALE_HANDLE);
    EXPECT_EQ(result.error().op_index, 1u);
    EXPECT_EQ(document->revision(), 1u);

    const auto after = boxes(*document);
    ASSERT_EQ(after.size(), before.size());
    for (const auto &[id, box] : before) {
        const auto found = after.find(id);
        ASSERT_NE(found, after.end()) << engrave::to_string(id) << " is gone after the rollback";
        EXPECT_EQ(found->second.page, box.page) << engrave::to_string(id);
        EXPECT_EQ(found->second.x, box.x) << engrave::to_string(id);
        EXPECT_EQ(found->second.y, box.y) << engrave::to_string(id);
        EXPECT_EQ(found->second.width, box.width) << engrave::to_string(id);
        EXPECT_EQ(found->second.height, box.height) << engrave::to_string(id);
    }
    const auto svg_after = document->render({.format = engrave::RenderFormat::SVG, .first_page = 0, .last_page = 0});
    ASSERT_TRUE(svg_after) << svg_after.error().message;
    ASSERT_FALSE(svg_after->pages.empty());
    EXPECT_TRUE(svg_before->pages[0] == svg_after->pages[0]) << "the rollback changed the first page";
}

// Native PATH commands use the same BatchGuard as public property batches;
// DRAW_PATH intentionally remains opaque in the public value codec.
using mu::engraving::ChordLine;
using mu::engraving::Pid;
using mu::engraving::PropertyValue;
using muse::draw::PainterPath;

ChordLine &chord_line(engrave::Document &document) {
    return *mu::engraving::to_chord_line(&engrave::harness::element(document, find_id(document, "ChordLine")));
}

void seed_controls(ChordLine &line) {
    line.read_source_offsets({muse::PointF(0.25, -0.5), muse::PointF(0.5, 0.25), muse::PointF(-0.75, 0.5), muse::PointF(1.0, -0.25)}, 1.0);
    line.set_modified(true);
}

PainterPath edited_path() {
    PainterPath path;
    path.move_to(1.0, 2.0);
    path.cubic_to(3.0, 5.0, 7.0, 11.0, 13.0, 17.0);
    path.line_to(19.0, 23.0); // Preserve a legacy path beyond an o1-o4 cubic.
    return path;
}

void expect_chord_line(const ChordLine &line, const ChordLine::AppearanceState &expected) {
    const auto actual = line.appearance_state();
    EXPECT_EQ(actual.linked, expected.linked);
    EXPECT_EQ(actual.offsets, expected.offsets);
    EXPECT_EQ(actual.modified, expected.modified);
    EXPECT_EQ(actual.length_x, expected.length_x);
    EXPECT_EQ(actual.length_y, expected.length_y);
    EXPECT_EQ(actual.controls, expected.controls);
    EXPECT_EQ(actual.flags, expected.flags);
    for (size_t i = 0; i < ChordLine::appearance_properties.size(); ++i) {
        // Offset/default paths are layout caches. Only legacy edited PATH is source.
        if (ChordLine::appearance_properties[i] == Pid::PATH && (expected.offsets || !expected.modified)) continue;
        EXPECT_EQ(actual.values[i], expected.values[i]);
    }
}

engrave::Result<engrave::EditResult> chord_line_batch(engrave::Document &document, std::vector<engrave::api::Step> steps) {
    engrave::DocumentAccess::edit_hooks(document).keep_history = true;
    return engrave::api::BatchGuard(document).run(std::move(steps), engrave::api::Policy::TRANSACTION, std::nullopt);
}

engrave::api::Step set_line(ChordLine &line, Pid pid, PropertyValue value) {
    return [&line, pid, value](engrave::api::EditContext &) -> engrave::Result<void> {
        line.undo_change_property(pid, value);
        return {};
    };
}

TEST(ChordLineEdit, PathAssignmentResetUndoRedoAndMode) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto source = line.appearance_state();
    auto *stack = engrave::harness::master_of(*document).undo_stack();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(line, Pid::PATH, PropertyValue::from_value(edited_path()))}));
    EXPECT_FALSE(line.has_source_offsets());
    EXPECT_TRUE(line.modified());
    const auto legacy = line.appearance_state();
    stack->undo(nullptr);
    expect_chord_line(line, source);
    stack->redo();
    expect_chord_line(line, legacy);

    ASSERT_TRUE(chord_line_batch(*document, {[&line](engrave::api::EditContext &) -> engrave::Result<void> {
        line.undo_reset_property(Pid::PATH);
        return {};
    }}));
    EXPECT_FALSE(line.modified());
    EXPECT_FALSE(line.has_source_offsets());
    stack->undo(nullptr);
    expect_chord_line(line, legacy);
    stack->undo(nullptr);
    expect_chord_line(line, source);

    ASSERT_TRUE(chord_line_batch(*document, {[&line](engrave::api::EditContext &) -> engrave::Result<void> {
        line.undo_reset_property(Pid::PATH);
        return {};
    }}));
    EXPECT_TRUE(line.has_source_offsets());
    EXPECT_FALSE(line.modified());
    const std::array<muse::PointF, 4> zero{};
    EXPECT_EQ(line.source_offsets(), zero);
    const auto reset = line.appearance_state();
    stack->undo(nullptr);
    expect_chord_line(line, source);
    stack->redo();
    expect_chord_line(line, reset);
    stack->undo(nullptr);

    ASSERT_TRUE(chord_line_batch(*document,
                                 {set_line(line, Pid::SIZE_SPATIUM_DEPENDENT, !line.get_property(Pid::SIZE_SPATIUM_DEPENDENT).to_bool())}));
    EXPECT_EQ(line.source_offsets(), source.controls);
    const auto mode = line.appearance_state();
    stack->undo(nullptr);
    expect_chord_line(line, source);
    stack->redo();
    expect_chord_line(line, mode);
}

TEST(ChordLineEdit, OrdinaryScaleCallbacksAndInverseKeepSources) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto before = line.appearance_state();
    line.spatium_changed(2.0, 3.0);
    for (size_t i = 0; i < 4; ++i) EXPECT_EQ(line.source_offsets()[i], before.controls[i] * 1.5);
    line.spatium_changed(3.0, 2.0);
    expect_chord_line(line, before);
    line.local_spatium_changed(2.0, 4.0);
    for (size_t i = 0; i < 4; ++i) EXPECT_EQ(line.source_offsets()[i], before.controls[i] * 2.0);
    line.local_spatium_changed(4.0, 2.0);
    expect_chord_line(line, before);
}

TEST(ChordLineEdit, PathEditAndOffsetResetSurviveNativeReopen) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    seed_controls(line);
    ASSERT_TRUE(chord_line_batch(*document, {set_line(line, Pid::PATH, PropertyValue::from_value(edited_path()))}));
    const auto expected = edited_path();
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        SCOPED_TRACE(format == engrave::Format::MSCX ? "MSCX" : "MSCZ");
        const auto saved = document->export_score({.format = format, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        const auto &restored = chord_line(**reopened);
        EXPECT_FALSE(restored.has_source_offsets());
        EXPECT_TRUE(restored.modified());
        const auto actual = restored.get_property(Pid::PATH).value<PainterPath>();
        ASSERT_EQ(actual.element_count(), expected.element_count());
        for (size_t i = 0; i < expected.element_count(); ++i) {
            EXPECT_EQ(actual.element_at(i).type, expected.element_at(i).type);
            // The legacy Path writer stores decimal coordinates at its existing precision.
            EXPECT_NEAR(actual.element_at(i).x, expected.element_at(i).x, 1e-4);
            EXPECT_NEAR(actual.element_at(i).y, expected.element_at(i).y, 1e-4);
        }
    }
    engrave::harness::master_of(*document).undo_stack()->undo(nullptr);
    ASSERT_TRUE(chord_line_batch(*document, {[&line](engrave::api::EditContext &) -> engrave::Result<void> {
        line.undo_reset_property(Pid::PATH);
        return {};
    }}));
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        SCOPED_TRACE(format == engrave::Format::MSCX ? "MSCX" : "MSCZ");
        const auto saved = document->export_score({.format = format, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        const auto &restored = chord_line(**reopened);
        EXPECT_TRUE(restored.has_source_offsets());
        EXPECT_EQ(restored.source_offsets(), (std::array<muse::PointF, 4>{}));
    }
}

TEST(ChordLineEdit, StyleScaleUndoRedoUsesExistingCommand) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    seed_controls(line);
    auto &score = engrave::harness::master_of(*document);
    const auto before = line.appearance_state();
    const double old_spatium = score.style().spatium();
    ASSERT_TRUE(chord_line_batch(*document, {[&score, old_spatium](engrave::api::EditContext &) -> engrave::Result<void> {
        score.undo(new mu::engraving::ChangeStyleValues(&score, {{mu::engraving::Sid::SPATIUM, old_spatium * 2.0}}));
        return {};
    }}));
    for (size_t i = 0; i < 4; ++i) EXPECT_EQ(line.source_offsets()[i], before.controls[i] * 2.0);
    const auto scaled = line.appearance_state();
    score.undo_stack()->undo(nullptr);
    expect_chord_line(line, before);
    score.undo_stack()->redo();
    expect_chord_line(line, scaled);
}

TEST(ChordLineEdit, EmptyAssignmentIsNotReset) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto before = line.appearance_state();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(line, Pid::PATH, PropertyValue::from_value(PainterPath()))}));
    EXPECT_FALSE(line.has_source_offsets());
    EXPECT_TRUE(line.modified());
    EXPECT_TRUE(line.get_property(Pid::PATH).value<PainterPath>().is_empty());
    engrave::harness::master_of(*document).undo_stack()->undo(nullptr);
    expect_chord_line(line, before);
}

TEST(ChordLineEdit, EmptyCachedPathAssignmentSwitchesRepresentation) {
    const auto source = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/chordline-offsets/offsets-500.mscx";
    auto opened = g_engine->open(source);
    ASSERT_TRUE(opened) << opened.error().message;
    auto document = std::move(*opened);
    auto &line = chord_line(*document);
    ASSERT_TRUE(line.has_source_offsets());
    line.set_property(Pid::CHORD_LINE_WAVY, true);
    engrave::harness::master_of(*document).do_layout();
    ASSERT_TRUE(line.get_property(Pid::PATH).value<PainterPath>().is_empty());
    const auto before = line.appearance_state();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(line, Pid::PATH, PropertyValue::from_value(PainterPath()))}));
    EXPECT_FALSE(line.has_source_offsets());
    EXPECT_TRUE(line.modified());
    engrave::harness::master_of(*document).undo_stack()->undo(nullptr);
    expect_chord_line(line, before);
}

TEST(ChordLineEdit, DefaultPathResetKeepsPartLinked) {
    const auto source = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/chordline-offsets/offsets-500.mscx";
    auto opened = g_engine->open(source);
    ASSERT_TRUE(opened) << opened.error().message;
    auto document = std::move(*opened);
    auto &master_line = chord_line(*document);
    ASSERT_FALSE(master_line.has_default_path());
    const auto initial_revision = document->revision();
    ASSERT_TRUE(document->edit({{ResetProperty{find_id(*document, "ChordLine"), "path"}}}));
    EXPECT_EQ(document->revision(), initial_revision + 1);
    ASSERT_TRUE(master_line.has_default_path());
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    ChordLine *part_line = nullptr;
    for (auto *linked : master_line.link_list_for_property_propagation()) {
        if (linked->is_chord_line() && !linked->score()->is_master()) {
            part_line = mu::engraving::to_chord_line(linked);
            break;
        }
    }
    ASSERT_NE(part_line, nullptr);
    ASSERT_TRUE(part_line->has_default_path());
    ASSERT_TRUE(part_line->is_appearance_linked_to_master());
    const auto revision = document->revision();
    ASSERT_TRUE(document->edit({{ResetProperty{engrave::from_engine(part_line->eid()), "path"}}}));
    EXPECT_EQ(document->revision(), revision);
    EXPECT_TRUE(part_line->is_appearance_linked_to_master());
    ASSERT_TRUE(chord_line_batch(*document, {set_line(master_line, Pid::VISIBLE, false)}));
    EXPECT_FALSE(part_line->get_property(Pid::VISIBLE).to_bool());
}

TEST(ChordLineEdit, LinkedAppearanceEditRelinkAndUndo) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    seed_controls(chord_line(*document));
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    auto &master_line = chord_line(*document);
    ChordLine *part_line = nullptr;
    for (auto *linked : master_line.link_list_for_property_propagation()) {
        if (linked->is_chord_line() && !linked->score()->is_master()) {
            part_line = mu::engraving::to_chord_line(linked);
            break;
        }
    }
    ASSERT_NE(part_line, nullptr);
    const auto master_before = master_line.appearance_state();
    const auto part_before = part_line->appearance_state();
    auto *stack = engrave::harness::master_of(*document).undo_stack();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(*part_line, Pid::PATH, PropertyValue::from_value(edited_path()))}));
    EXPECT_FALSE(part_line->is_appearance_linked_to_master());
    EXPECT_FALSE(part_line->has_source_offsets());
    expect_chord_line(master_line, master_before);
    const auto independent = part_line->appearance_state();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(*part_line, Pid::APPEARANCE_LINKED_TO_MASTER, true)}));
    expect_chord_line(*part_line, master_before);
    stack->undo(nullptr);
    expect_chord_line(*part_line, independent);
    stack->redo();
    expect_chord_line(*part_line, master_before);
    stack->undo(nullptr);
    stack->undo(nullptr);
    expect_chord_line(*part_line, part_before);
    expect_chord_line(master_line, master_before);

    // A failed batch must restore the score. Layout may add MM-rest bookkeeping
    // to the private undo stack after rollback, so it is not an exact-history test.
    stack->redo();
    expect_chord_line(*part_line, independent);
    const auto revision = document->revision();
    EXPECT_FALSE(chord_line_batch(
        *document, {set_line(*part_line, Pid::APPEARANCE_LINKED_TO_MASTER, true),
                    [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("injected after relink"); }}));
    expect_chord_line(*part_line, independent);
    expect_chord_line(master_line, master_before);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(stack->has_active_transaction());
}

TEST(ChordLineEdit, FailedBatchRestoresSourceAndLegacyRepresentations) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    auto *stack = engrave::harness::master_of(*document).undo_stack();
    for (bool offsets : {true, false}) {
        if (offsets)
            seed_controls(line);
        else
            line.set_property(Pid::PATH, PropertyValue::from_value(edited_path()));
        const auto before = line.appearance_state();
        const auto revision = document->revision();
        const auto fail = [](engrave::api::EditContext &) -> engrave::Result<void> {
            throw std::runtime_error("injected after ChordLine edits");
        };
        auto result = chord_line_batch(*document, {set_line(line, Pid::SIZE_SPATIUM_DEPENDENT, !line.size_is_spatium_dependent()),
                                                   [&line](engrave::api::EditContext &) -> engrave::Result<void> {
                                                       line.undo_reset_property(Pid::PATH);
                                                       return {};
                                                   },
                                                   set_line(line, Pid::PATH, PropertyValue::from_value(PainterPath())), fail});
        EXPECT_FALSE(result);
        expect_chord_line(line, before);
        EXPECT_EQ(document->revision(), revision);
        EXPECT_EQ(stack->size(), 0u);
        EXPECT_FALSE(stack->has_active_transaction());
    }
}

// --- the evidence ---------------------------------------------------------------

std::string read_text(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

TEST(Evidence, EveryCaseMatchesTheCheckedInRow) {
    std::vector<engrave::harness::EvidenceRow> rows;
    for (const auto &op_case : engrave::harness::registered_cases()) {
        const std::filesystem::path path = fixture(op_case.fixture);
        ASSERT_FALSE(path.empty()) << op_case.name << ": " << op_case.fixture << " is not in the corpus";

        rows.push_back(engrave::harness::run_case(*g_engine, op_case, path));
    }
    const std::string current = engrave::harness::to_json(rows).dump(2) + "\n";
    if (g_update_evidence) {
        std::ofstream(g_evidence, std::ios::binary) << current;
        return;
    }
    const auto checked_in = nlohmann::ordered_json::parse(read_text(g_evidence), nullptr, false);
    ASSERT_FALSE(checked_in.is_discarded()) << g_evidence.string() << " is not JSON";
    const auto now = nlohmann::ordered_json::parse(current);
    EXPECT_EQ(now["verdicts"], checked_in["verdicts"]);
    for (const auto &row : now["cases"]) {
        const auto found = std::ranges::find_if(checked_in["cases"], [&row](const auto &old) { return old["case"] == row["case"]; });
        if (found == checked_in["cases"].end())
            ADD_FAILURE() << row["case"] << " has no checked-in row";
        else
            EXPECT_EQ(row, *found) << "rerun with --update-evidence once the change is understood";
    }
}

// --- corpus -----------------------------------------------------------------------

void collect(const std::filesystem::path &path) {
    static const std::set<std::string> extensions = {".mscx", ".mscz", ".musicxml", ".mxl", ".mid", ".midi",
                                                     ".gp",   ".gp3",  ".gp4",      ".gp5", ".gpx", ".ptb"};
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::vector<std::filesystem::path> entries;
        for (const auto &entry : std::filesystem::directory_iterator(path, ec)) entries.push_back(entry.path());
        std::ranges::sort(entries);
        for (const auto &entry : entries) collect(entry);
    } else if (std::filesystem::is_regular_file(path, ec) && extensions.contains(path.extension().string())) {
        g_corpus.push_back(path);
    }
}

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    std::filesystem::path assets;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument.starts_with("--assets="))
            assets = std::filesystem::path(argument.substr(9));
        else if (argument.starts_with("--corpus="))
            collect(std::filesystem::path(argument.substr(9)));
        else if (argument == "--update-evidence")
            g_update_evidence = true;
    }
    if (g_corpus.empty()) {
        std::fprintf(stderr, "no corpus: pass --corpus=<file or directory>\n");
        return 1;
    }
    auto engine = engrave::Engine::create({.resources = assets});
    if (!engine) {
        std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", engine.error().message.c_str());
        return 1;
    }
    g_engine = std::move(*engine);
    const int result = RUN_ALL_TESTS();
    g_engine.reset();
    return result;
}
