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
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/tests/harness/harness.h"
#include "engraving/automation/automationdata.h"
#include "engraving/automation/tempovalues.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/chordlist.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mmrest.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/notedot.h"
#include "engraving/dom/parenthesis.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/sharedpart.h"
#include "engraving/dom/spacer.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/spannermap.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafflines.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stem.h"
#include "engraving/dom/system.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/editexcerpt.h"
#include "engraving/editing/editproperty.h"
#include "engraving/editing/editstaffbrackets.h"
#include "engraving/editing/editstyle.h"
#include "engraving/editing/transaction/mmrestchange.h"
#include "engraving/editing/transaction/mutationjournal.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/style/style.h"
#include "global/io/buffer.h"

namespace {

using engrave::Batch;
using music::dump_json;
using music::json_equal;
using music::parse_json;
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
    EXPECT_EQ(engrave::harness::master_of(*document).mutation_journal()->size(), 0u);
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

TEST(JournalPolicy, GenericTimingAndBreakRemovalsUseSnapshotRecovery) {
    for (const std::string_view type : {"Measure", "TimeSig", "LayoutBreak"}) {
        SCOPED_TRACE(type);
        auto document = open(fixture_name);
        ASSERT_TRUE(document);
        if (type == "LayoutBreak") {
            const auto outline = document->outline();
            ASSERT_TRUE(outline);
            ASSERT_TRUE(document->edit({{engrave::SetLayoutBreak{outline->measures.front().id, "line", true}}}));
        }
        const auto target = find_id(*document, type);
        const auto revision = document->revision();
        const auto *project = engrave::DocumentAccess::project(*document).get();
        std::vector<ElementId> ids;
        for (const auto &entry : engrave::live_ids(engrave::harness::master_of(*document))) ids.push_back(entry.id);
        bool removed = false;
        engrave::DocumentAccess::edit_hooks(*document).before_step = [&](std::size_t step) {
            if (step == 1) {
                removed = true;
                mu::engraving::MScore::set_error(mu::engraving::MsError::NO_NOTE_SELECTED);
            }
        };
        const auto result = document->edit({{engrave::Remove{target}}});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::EDIT_REJECTED);
        EXPECT_TRUE(removed);
        EXPECT_NE(engrave::DocumentAccess::project(*document).get(), project);
        auto &restored = engrave::harness::master_of(*document);
        std::vector<ElementId> restored_ids;
        for (const auto &entry : engrave::live_ids(restored)) restored_ids.push_back(entry.id);
        EXPECT_EQ(restored_ids, ids);
        EXPECT_TRUE(engrave::resolve(restored, target));
        EXPECT_EQ(document->revision(), revision);
        EXPECT_FALSE(restored.mutation_journal()->has_active_transaction());
        EXPECT_EQ(restored.layout_state.whole_count + restored.layout_state.range_count, 0u);
    }
}

TEST(JournalPolicy, OrdinaryNoteRemovalKeepsJournalOnlyRecovery) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto target = find_id(*document, "Note");
    const auto revision = document->revision();
    auto &master = engrave::harness::master_of(*document);
    const auto before = engrave::resolve(master, target);
    ASSERT_TRUE(before);
    const auto handle = master.eid_register()->handle_of(before->object);
    const auto *project = engrave::DocumentAccess::project(*document).get();
    auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
    hooks.fail_restore = true;
    hooks.before_step = [](std::size_t step) {
        if (step == 1) throw std::runtime_error("reject after note removal");
    };
    const auto result = document->edit({{engrave::Remove{target}}});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::EDIT_REJECTED);
    EXPECT_EQ(engrave::DocumentAccess::project(*document).get(), project);
    const auto restored = engrave::resolve(master, target);
    ASSERT_TRUE(restored);
    EXPECT_EQ(restored->object, before->object);
    EXPECT_EQ(master.eid_register()->live_item(handle), before->object);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalPolicy, RejectedTimeSignatureRewriteRejectsTheWholeBatch) {
    using namespace mu::engraving;
    auto document = open("layout_elements.mscx");
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    ASSERT_GT(master.nstaves(), 1u);
    auto *local = master.staff(1)->time_sig(Fraction(0, 1));
    ASSERT_NE(local, nullptr);
    local->set_stretch(Fraction(3, 4));
    auto *measure = master.first_measure()->next_measure();
    ASSERT_NE(measure, nullptr);
    auto *segment = measure->undo_get_segment(SegmentType::TIME_SIG, measure->tick());
    auto *signature = Factory::create_time_sig(segment);
    signature->set_track(0);
    signature->set_sig(Fraction(4, 4), TimeSigType::NORMAL);
    master.record_element_add(signature);
    engrave::assign_missing_ids(master);
    const auto segment_handle = master.eid_register()->handle_of(segment);
    // Before replacing a failed batch's project, the domain unwind must return
    // detached source ownership to the old score so teardown can destroy it.
    master.start_cmd(muse::TranslatableString::untranslatable("refused time signature removal"));
    EXPECT_THROW(master.delete_item(signature), std::runtime_error);
    EXPECT_EQ(master.eid_register()->live_item(segment_handle), segment);
    EXPECT_EQ(measure->find_segment(SegmentType::TIME_SIG, measure->tick()), segment);
    master.end_cmd(true);
    const auto signature_id = engrave::from_engine(signature->eid());
    const auto local_id = engrave::from_engine(local->eid());
    const auto note = find_id(*document, "Note");
    const auto color = property(*document, note, "color");
    const auto revision = document->revision();
    const auto *project = engrave::DocumentAccess::project(*document).get();
    const auto result = document->edit({{SetProperty{note, "color", Color{17, 29, 43}}, engrave::Remove{signature_id}}});
    ASSERT_FALSE(result) << "a refused rewrite must not report a successful batch";
    EXPECT_EQ(result.error().code, ErrorCode::EDIT_REJECTED);
    EXPECT_EQ(result.error().op_index, 1u);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(property(*document, note, "color"), color);
    EXPECT_NE(engrave::DocumentAccess::project(*document).get(), project);
    auto &restored = engrave::harness::master_of(*document);
    EXPECT_TRUE(engrave::resolve(restored, signature_id));
    const auto restored_local = engrave::resolve(restored, local_id);
    ASSERT_TRUE(restored_local);
    EXPECT_EQ(to_time_sig(restored_local->object)->stretch(), Fraction(3, 4));
    EXPECT_FALSE(restored.mutation_journal()->has_active_transaction());
    EXPECT_EQ(restored.layout_state.whole_count + restored.layout_state.range_count, 0u);
}

TEST(Edit, APoisonedDocumentRefusesWork) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
    hooks.policy = engrave::api::Policy::SNAPSHOT;
    hooks.fail_restore = true;
    hooks.before_step = [](std::size_t) { throw std::runtime_error("injected"); };
    const Batch batch{{SetStyle{{}, "staffDistance", 9.0}}};
    EXPECT_ANY_THROW(std::ignore = document->edit(batch));
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
    EXPECT_EQ(actual.legacy_path, expected.legacy_path);
    EXPECT_EQ(actual.flags, expected.flags);
    for (size_t i = 0; i < ChordLine::appearance_properties.size(); ++i) {
        // Offset/default paths are layout caches. Only legacy edited PATH is source.
        if (ChordLine::appearance_properties[i] == Pid::PATH && (expected.offsets || !expected.modified)) continue;
        EXPECT_EQ(actual.values[i], expected.values[i]);
    }
}

engrave::Result<engrave::EditResult> chord_line_batch(engrave::Document &document, std::vector<engrave::api::Step> steps) {
    return engrave::api::BatchGuard(document).run(std::move(steps), engrave::api::Policy::TRANSACTION, std::nullopt);
}

engrave::api::Step set_line(ChordLine &line, Pid pid, PropertyValue value) {
    return [&line, pid, value](engrave::api::EditContext &) -> engrave::Result<void> {
        line.undo_change_property(pid, value);
        return {};
    };
}

TEST(ChordLineEdit, PathAssignmentResetAndMode) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto source = line.appearance_state();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(line, Pid::PATH, PropertyValue::from_value(edited_path()))}));
    EXPECT_FALSE(line.has_source_offsets());
    EXPECT_TRUE(line.modified());
    ASSERT_TRUE(chord_line_batch(*document, {[&line](engrave::api::EditContext &) -> engrave::Result<void> {
        line.undo_reset_property(Pid::PATH);
        return {};
    }}));
    EXPECT_FALSE(line.modified());
    EXPECT_FALSE(line.has_source_offsets());
    seed_controls(line);
    ASSERT_TRUE(chord_line_batch(*document, {[&line](engrave::api::EditContext &) -> engrave::Result<void> {
        line.undo_reset_property(Pid::PATH);
        return {};
    }}));
    EXPECT_TRUE(line.has_source_offsets());
    EXPECT_FALSE(line.modified());
    const std::array<muse::PointF, 4> zero{};
    EXPECT_EQ(line.source_offsets(), zero);
    seed_controls(line);
    ASSERT_TRUE(chord_line_batch(*document,
                                 {set_line(line, Pid::SIZE_SPATIUM_DEPENDENT, !line.get_property(Pid::SIZE_SPATIUM_DEPENDENT).to_bool())}));
    EXPECT_EQ(line.source_offsets(), source.controls);
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
    seed_controls(line);
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

TEST(ChordLineEdit, StyleScaleUsesSourceJournal) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    seed_controls(line);
    auto &score = engrave::harness::master_of(*document);
    const auto before = line.appearance_state();
    const double old_spatium = score.style().spatium();
    ASSERT_TRUE(chord_line_batch(*document, {[&score, old_spatium](engrave::api::EditContext &) -> engrave::Result<void> {
        score.undo_change_style_val(mu::engraving::Sid::SPATIUM, old_spatium * 2.0);
        return {};
    }}));
    for (size_t i = 0; i < 4; ++i) EXPECT_EQ(line.source_offsets()[i], before.controls[i] * 2.0);
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

TEST(ChordLineEdit, LinkedAppearanceEditRelinkAndRollback) {
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
    auto *stack = engrave::harness::master_of(*document).mutation_journal();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(*part_line, Pid::PATH, PropertyValue::from_value(edited_path()))}));
    EXPECT_FALSE(part_line->is_appearance_linked_to_master());
    EXPECT_FALSE(part_line->has_source_offsets());
    expect_chord_line(master_line, master_before);
    const auto independent = part_line->appearance_state();
    ASSERT_TRUE(chord_line_batch(*document, {set_line(*part_line, Pid::APPEARANCE_LINKED_TO_MASTER, true)}));
    expect_chord_line(*part_line, master_before);
    ASSERT_TRUE(chord_line_batch(*document, {set_line(*part_line, Pid::PATH, PropertyValue::from_value(edited_path()))}));
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

TEST(ChordLineEdit, LegacyPathSurvivesDisplayCacheReplacement) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    ASSERT_TRUE(chord_line_batch(*document, {set_line(line, Pid::PATH, PropertyValue::from_value(edited_path()))}));
    const auto before = line.appearance_state();
    const auto path = line.legacy_absolute_path();
    line.mutldata()->path = PainterPath();
    EXPECT_EQ(line.get_property(Pid::PATH), PropertyValue::from_value(path));
    expect_chord_line(line, before);
    std::unique_ptr<ChordLine> copy(line.clone());
    EXPECT_EQ(copy->legacy_absolute_path(), path);
    EXPECT_EQ(copy->get_property(Pid::PATH), PropertyValue::from_value(path));
}

TEST(ChordLineEdit, FailedBatchRestoresSourceAndLegacyRepresentations) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &line = chord_line(*document);
    auto *stack = engrave::harness::master_of(*document).mutation_journal();
    for (bool offsets : {true, false}) {
        if (offsets)
            seed_controls(line);
        else
            line.set_property(Pid::PATH, PropertyValue::from_value(edited_path()));
        const auto before = line.appearance_state();
        const auto revision = document->revision();
        const auto native500 = document->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        const auto native470 = document->export_score({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(native500);
        ASSERT_FALSE(native470);
        EXPECT_NE(native470.error().message.find("ChordLine"), std::string::npos);
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
        const auto restored500 = document->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        const auto restored470 = document->export_score({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(restored500);
        ASSERT_FALSE(restored470);
        EXPECT_EQ(*restored500, *native500);
        EXPECT_EQ(restored470.error().message, native470.error().message);
        expect_chord_line(line, before);
        EXPECT_EQ(document->revision(), revision);
        EXPECT_EQ(stack->size(), 0u);
        EXPECT_FALSE(stack->has_active_transaction());
    }
}

TEST(JournalDerived, BracketMoveAndRemovalRollbackRestoresOwnerValues) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto level = master.brackets(0).size();
    EditStaffBrackets::set_bracket_type(&master, 0, level, BracketType::NORMAL);
    auto *item = master.brackets(0).back();
    const auto handle = master.eid_register()->enroll(*item);
    const auto revision = document->revision();
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        item->undo_change_property(Pid::BRACKET_COLUMN, static_cast<int>(level + 3));
        EXPECT_EQ(item->column(), level + 3);
        item->undo_change_property(Pid::SYSTEM_BRACKET, static_cast<int>(BracketType::NO_BRACKET));
        EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->derived_entries(), 2u);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject bracket moves/removal"});
    }});
    EXPECT_FALSE(result);
    ASSERT_EQ(master.brackets(0).size(), level + 1);
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
    EXPECT_EQ(master.brackets(0).back()->column(), level);
    EXPECT_EQ(master.brackets(0).back()->bracket_type(), BracketType::NORMAL);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(JournalDerived, CommittedBracketRemovalDisposesDisplayObject) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    EditStaffBrackets::set_bracket_type(&master, 0, master.brackets(0).size(), BracketType::NORMAL);
    auto *item = master.brackets(0).back();
    const auto handle = master.eid_register()->enroll(*item);
    engrave::DocumentAccess::edit_hooks(*document).before_cleanup = [&] {
        EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
    };
    ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        item->undo_change_property(Pid::SYSTEM_BRACKET, static_cast<int>(BracketType::NO_BRACKET));
        return {};
    }}));
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalDerived, BracketInverseSurvivesReplacement) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto level = master.brackets(0).size();
    EditStaffBrackets::set_bracket_type(&master, 0, level, BracketType::NORMAL);
    auto *item = master.brackets(0).back();
    item->set_bracket_span(5);
    const auto revision = document->revision();
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        item->undo_change_property(Pid::BRACKET_SPAN, 3);
        master.record_bracket_remove(master.staff(0), level);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject bracket replacement"});
    }}));
    ASSERT_EQ(master.brackets(0).size(), level + 1);
    EXPECT_EQ(master.brackets(0).back()->bracket_span(), 5u);
    EXPECT_EQ(master.brackets(0).back()->bracket_type(), BracketType::NORMAL);
    EXPECT_EQ(document->revision(), revision);
}

TEST(JournalDerived, BracketRollbackPreservesNonIndexColumns) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto level = master.brackets(0).size();
    EditStaffBrackets::set_bracket_type(&master, 0, level, BracketType::NORMAL);
    auto *item = master.brackets(0).back();
    item->set_column(level + 7);
    item->set_bracket_span(5);
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        item->undo_change_property(Pid::BRACKET_SPAN, 3);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject non-index bracket span"});
    }}));
    EXPECT_EQ(item->column(), level + 7);
    EXPECT_EQ(item->bracket_span(), 5u);
}

TEST(JournalDerived, BracketRemovalThenExcerptRemovalHasOneOwner) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    auto &master = engrave::harness::master_of(*document);
    ASSERT_FALSE(master.excerpts().empty());
    auto *excerpt = master.excerpts().front();
    auto *score = excerpt->excerpt_score();
    ASSERT_NE(score, nullptr);
    EditStaffBrackets::set_bracket_type(score, 0, score->brackets(0).size(), BracketType::NORMAL);
    auto *item = score->brackets(0).back();
    const auto handle = master.eid_register()->enroll(*item);
    ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        item->undo_change_property(Pid::SYSTEM_BRACKET, static_cast<int>(BracketType::NO_BRACKET));
        master.record_excerpt(excerpt, false);
        return {};
    }}));
    EXPECT_TRUE(master.excerpts().empty());
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
}

TEST(JournalDerived, TextLineDestinationOnlyPropertiesRollback) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *line = Factory::create_text_line(master.dummy());
    line->set_track(0);
    line->set_system_flag(true);
    line->init_style();
    master.add_spanner(line, false);
    line->set_property(Pid::BEGIN_LINE_ARROW_HEIGHT, Spatium(9.25));
    const auto before = line->get_property(Pid::BEGIN_LINE_ARROW_HEIGHT);
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        line->undo_change_property(Pid::SYSTEM_FLAG, false, PropertyFlags::NOSTYLE);
        EXPECT_NE(line->get_property(Pid::BEGIN_LINE_ARROW_HEIGHT), before);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject destination-only styled values"});
    }}));
    EXPECT_TRUE(line->system_flag());
    EXPECT_EQ(line->get_property(Pid::BEGIN_LINE_ARROW_HEIGHT), before);
}

TEST(JournalDerived, TextLineSystemRollbackRestoresStyledOverrides) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *line = Factory::create_text_line(master.dummy());
    line->set_track(0);
    master.add_spanner(line, false);
    line->set_property(Pid::BEGIN_FONT_FACE, muse::String(u"journal authored font"));
    line->set_property_flags(Pid::BEGIN_FONT_FACE, PropertyFlags::UNSTYLED);
    const auto before = line->get_property(Pid::BEGIN_FONT_FACE);
    const auto revision = document->revision();
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        line->undo_change_property(Pid::SYSTEM_FLAG, true, PropertyFlags::NOSTYLE);
        EXPECT_TRUE(line->system_flag());
        EXPECT_NE(line->get_property(Pid::BEGIN_FONT_FACE), before);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->derived_entries(), 1u);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject text-line system conversion"});
    }});
    EXPECT_FALSE(result);
    EXPECT_FALSE(line->system_flag());
    EXPECT_EQ(line->get_property(Pid::BEGIN_FONT_FACE), before);
    EXPECT_EQ(line->property_flags(Pid::BEGIN_FONT_FACE), PropertyFlags::UNSTYLED);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

// --- JournalCompound -------------------------------------------------------------

TEST(JournalCompound, NestedLinkAndCreatedAddFailure) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto peer_id = find_id(*document, "Text");
    auto &peer = *to_text_base(&engrave::harness::element(*document, peer_id));
    const auto original_peers = peer.link_list();
    const auto revision = document->revision();
    const auto whole_count_before = master.layout_state.whole_count;
    const auto range_count_before = master.layout_state.range_count;

    auto *created = Factory::create_text(master.first_measure());
    ASSERT_NE(created, nullptr);
    created->set_track(0);
    created->set_xml_text("<b>authored nested</b>");

    SourceHandle created_handle{};
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_link(created, &peer);
        master.record_element_add(created);
        created_handle = master.eid_register()->enroll(*created);
        EXPECT_TRUE(created_handle.is_valid());
        const auto &batch = *master.mutation_journal()->active_transaction();
        EXPECT_GE(batch.compound_entries(), 2u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject nested link and add"});
    }});
    EXPECT_FALSE(result);
    EXPECT_EQ(peer.link_list(), original_peers);
    EXPECT_EQ(master.eid_register()->item_from_handle(created_handle), nullptr);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    EXPECT_EQ(master.layout_state.whole_count, whole_count_before);
    EXPECT_EQ(master.layout_state.range_count, range_count_before);
}

TEST(JournalCompound, EarlierValueInverseReferencesRemovedObject) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto text_id = find_id(*document, "Text");
    auto &text = *to_text_base(&engrave::harness::element(*document, text_id));
    const auto color_before = text.get_property(Pid::COLOR);
    const auto text_before = text.xml_text();
    const auto revision = document->revision();
    const auto handle = master.eid_register()->enroll(text);

    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        text.undo_change_property(Pid::COLOR, PropertyValue(muse::draw::Color{12, 34, 56}));
        EXPECT_EQ(text.get_property(Pid::COLOR), PropertyValue(muse::draw::Color{12, 34, 56}));
        master.record_element_remove(&text);
        const auto &batch = *master.mutation_journal()->active_transaction();
        EXPECT_GE(batch.property_entries(), 1u);
        EXPECT_GE(batch.compound_entries(), 1u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject color change and remove"});
    }});
    EXPECT_FALSE(result);
    const auto resolved = engrave::resolve(master, text_id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->object, &text);
    EXPECT_EQ(master.eid_register()->live_item(handle), &text);
    EXPECT_EQ(text.get_property(Pid::COLOR), color_before);
    EXPECT_EQ(text.xml_text(), text_before);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(JournalCompound, RemoveAndReaddSameAuthoredTextLifetime) {
    using namespace mu::engraving;
    for (bool reject : {false, true}) {
        SCOPED_TRACE(reject);
        auto document = open(fixture_name);
        ASSERT_TRUE(document);
        auto &master = engrave::harness::master_of(*document);
        const auto id = find_id(*document, "Text");
        auto &item = *to_text_base(&engrave::harness::element(*document, id));
        auto *saved_parent = item.ownership_parent_item();
        ASSERT_NE(saved_parent, nullptr);
        const auto handle = master.eid_register()->enroll(item);
        const auto revision = document->revision();

        const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            master.record_element_remove(&item);
            item.set_ownership_parent(saved_parent);
            master.record_element_add(&item);
            const auto &batch = *master.mutation_journal()->active_transaction();
            EXPECT_GE(batch.compound_entries(), 2u);
            EXPECT_EQ(batch.legacy_entries(), 0u);
            if (reject) {
                return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject reattached element"});
            }
            return {};
        }});
        EXPECT_EQ(result.has_value(), !reject);
        EXPECT_EQ(master.eid_register()->item_from_handle(handle), &item);
        EXPECT_EQ(master.eid_register()->live_item(handle), &item);
        const auto resolved = engrave::resolve(master, id);
        ASSERT_TRUE(resolved);
        EXPECT_EQ(resolved->object, &item);
        EXPECT_EQ(item.ownership_parent_item(), saved_parent);
        EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
        if (reject) {
            EXPECT_EQ(document->revision(), revision);
        } else {
            EXPECT_EQ(document->revision(), revision + 1u);
        }
    }
}

TEST(JournalCompound, UnlinkExactGroupAndOrderRollback) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *first_measure = master.first_measure();
    ASSERT_NE(first_measure, nullptr);

    auto *a = Factory::create_text(first_measure);
    ASSERT_NE(a, nullptr);
    a->set_track(0);
    a->set_xml_text("<b>link a</b>");
    master.record_element_add(a);

    auto *b = Factory::create_text(first_measure);
    ASSERT_NE(b, nullptr);
    b->set_track(0);
    b->set_xml_text("<b>link b</b>");
    master.record_element_add(b);

    auto *c = Factory::create_text(first_measure);
    ASSERT_NE(c, nullptr);
    c->set_track(0);
    c->set_xml_text("<b>link c</b>");
    master.record_element_add(c);

    a->link_to(b);
    c->link_to(b);

    auto *saved_group = b->links();
    ASSERT_NE(saved_group, nullptr);
    EXPECT_EQ(a->links(), saved_group);
    EXPECT_EQ(c->links(), saved_group);
    const std::vector<EngravingObject *> ordered_members(saved_group->begin(), saved_group->end());
    ASSERT_EQ(ordered_members.size(), 3u);
    const auto revision = document->revision();

    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_unlink(a);
        master.record_unlink(b);
        const auto &batch = *master.mutation_journal()->active_transaction();
        EXPECT_GE(batch.compound_entries(), 2u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject unlink batch"});
    }});
    EXPECT_FALSE(result);
    EXPECT_EQ(a->links(), saved_group);
    EXPECT_EQ(b->links(), saved_group);
    EXPECT_EQ(c->links(), saved_group);
    const std::vector<EngravingObject *> rolled_back_members(saved_group->begin(), saved_group->end());
    EXPECT_EQ(rolled_back_members, ordered_members);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(JournalCompound, LaterDirectLinkRetainsGroupAfterRecordedLinkRollback) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *measure = master.first_measure();
    ASSERT_NE(measure, nullptr);
    auto make_text = [&](const muse::String &value) {
        auto *text = Factory::create_text(measure);
        text->set_track(0);
        text->set_xml_text(value);
        master.record_element_add(text);
        return text;
    };
    auto *recorded = make_text(u"recorded");
    auto *peer = make_text(u"peer");
    auto *direct = make_text(u"direct");
    LinkedObjects *group = nullptr;

    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_link(recorded, peer);
        direct->link_to(peer);
        group = peer->links();
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject recorded link"});
    }});
    EXPECT_FALSE(result);
    EXPECT_EQ(recorded->links(), nullptr);
    ASSERT_NE(peer->links(), nullptr);
    EXPECT_EQ(peer->links(), group);
    EXPECT_EQ(direct->links(), group);
    // The direct member is outside this inverse. Its live group must remain DOM-owned.
    const std::vector<EngravingObject *> members(group->begin(), group->end());
    EXPECT_EQ(members, (std::vector<EngravingObject *>{peer, direct}));
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(JournalCompound, ReparentItemAndSegmentRollback) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto *master = &engrave::harness::master_of(*document);
    auto *first = master->first_measure();
    ASSERT_NE(first, nullptr);
    auto *next = first->next_measure();
    if (!next) {
        document = open("coverage.musicxml");
        ASSERT_TRUE(document);
        master = &engrave::harness::master_of(*document);
        first = master->first_measure();
        ASSERT_NE(first, nullptr);
        next = first->next_measure();
        ASSERT_NE(next, nullptr);
    }

    auto *original_segment = Factory::create_segment(first, SegmentType::TIME_TICK, Fraction(1, 8));
    ASSERT_NE(original_segment, nullptr);
    master->record_element_add(original_segment);

    auto *staff_text = Factory::create_staff_text(original_segment);
    ASSERT_NE(staff_text, nullptr);
    staff_text->set_track(0);
    staff_text->set_xml_text("<b>authored staff text</b>");
    master->record_element_add(staff_text);
    engrave::assign_missing_ids(*master);

    auto *target_segment = first->first(SegmentType::CHORD_REST);
    ASSERT_NE(target_segment, nullptr);
    ASSERT_NE(target_segment, original_segment);

    const auto item_owner = staff_text->ownership_parent_item();
    const auto segment_owner = original_segment->measure();
    const auto local_tick = original_segment->rtick();
    const auto item_handle = master->eid_register()->enroll(*staff_text);
    const auto segment_handle = master->eid_register()->enroll(*original_segment);
    const auto item_id = engrave::from_engine(staff_text->eid());
    const auto segment_id = engrave::from_engine(original_segment->eid());
    const auto revision = document->revision();

    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master->record_parent_change(staff_text, target_segment, 0);
        master->record_segment_parent_change(original_segment, next, Fraction(1, 4));
        const auto &batch = *master->mutation_journal()->active_transaction();
        EXPECT_GE(batch.compound_entries(), 2u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject reparent"});
    }});
    EXPECT_FALSE(result);

    EXPECT_EQ(staff_text->ownership_parent_item(), item_owner);
    EXPECT_EQ(original_segment->measure(), segment_owner);
    EXPECT_EQ(original_segment->rtick(), local_tick);
    EXPECT_EQ(master->eid_register()->live_item(item_handle), staff_text);
    EXPECT_EQ(master->eid_register()->live_item(segment_handle), original_segment);
    if (staff_text->eid().is_valid()) {
        const auto resolved = engrave::resolve(*master, item_id);
        ASSERT_TRUE(resolved);
        EXPECT_EQ(resolved->object, staff_text);
    }
    if (original_segment->eid().is_valid()) {
        const auto resolved = engrave::resolve(*master, segment_id);
        ASSERT_TRUE(resolved);
        EXPECT_EQ(resolved->object, original_segment);
    }
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master->mutation_journal()->has_active_transaction());
}

TEST(JournalCompound, ReplaceThenRejectRestoresOriginalAndLeavesReplacementInactive) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto old_id = find_id(*document, "Text");
    auto &old_text = *to_text_base(&engrave::harness::element(*document, old_id));
    const auto old_xml = old_text.xml_text();
    auto *parent = old_text.ownership_parent_item();
    ASSERT_NE(parent, nullptr);
    const auto track = old_text.track();
    const auto old_handle = master.eid_register()->enroll(old_text);
    const auto revision = document->revision();

    auto *replacement = Factory::create_text(parent);
    ASSERT_NE(replacement, nullptr);
    replacement->set_track(track);
    replacement->set_xml_text("<b>replacement authored text</b>");
    const auto replacement_handle = master.eid_register()->enroll(*replacement);

    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_element_replace(&old_text, replacement);
        const auto &batch = *master.mutation_journal()->active_transaction();
        EXPECT_GE(batch.compound_entries(), 1u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject element replacement"});
    }});
    EXPECT_FALSE(result);

    EXPECT_EQ(master.eid_register()->item_from_handle(old_handle), &old_text);
    EXPECT_EQ(master.eid_register()->live_item(old_handle), &old_text);
    const auto resolved = engrave::resolve(master, old_id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->object, &old_text);
    EXPECT_EQ(old_text.ownership_parent_item(), parent);
    EXPECT_EQ(old_text.xml_text(), old_xml);
    EXPECT_EQ(master.eid_register()->live_item(replacement_handle), nullptr);
    EXPECT_FALSE(master.eid_register()->published(replacement_handle));
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(JournalTiming, FailedChainReplacementRestoresOriginalSourceLifetimes) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *original = master.first_measure();
    auto *tail = master.last();
    ASSERT_NE(original, nullptr);
    ASSERT_NE(tail, nullptr);
    const auto original_id = engrave::from_engine(original->eid());
    const auto revision = document->revision();
    std::vector<MeasureBase *> before;
    for (auto *item = master.first(); item; item = item->next()) before.push_back(item);

    auto *first = Factory::create_measure(&master);
    auto *last = Factory::create_measure(&master);
    first->set_timesig(original->timesig());
    first->set_ticks(original->ticks());
    first->set_tick(master.last_measure()->end_tick());
    last->set_timesig(original->timesig());
    last->set_ticks(original->ticks());
    last->set_tick(first->end_tick());
    first->set_prev(tail);
    first->set_next(last);
    last->set_prev(first);
    last->set_next(nullptr);
    const auto first_handle = master.eid_register()->enroll(*first);
    const auto last_handle = master.eid_register()->enroll(*last);
    const auto whole = master.layout_state.whole_count;
    const auto range = master.layout_state.range_count;

    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_measures(first, last, true, false);
        master.record_measures(original, original, false, false);
        const auto &journal = *master.mutation_journal()->active_transaction();
        EXPECT_EQ(journal.timing_entries(), 2u);
        EXPECT_EQ(journal.legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject measure chain replacement"});
    }});
    EXPECT_FALSE(result);
    std::vector<MeasureBase *> restored;
    for (auto *item = master.first(); item; item = item->next()) restored.push_back(item);
    EXPECT_EQ(restored, before);
    const auto resolved = engrave::resolve(master, original_id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->object, original);
    EXPECT_EQ(master.eid_register()->live_item(first_handle), nullptr);
    EXPECT_EQ(master.eid_register()->live_item(last_handle), nullptr);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    EXPECT_EQ(master.layout_state.whole_count, whole);
    EXPECT_EQ(master.layout_state.range_count, range);
}

TEST(JournalPart, ReattachedPartRestoresLabelsAndOriginalInstrumentLifetime) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *part = master.parts().front();
    auto *instrument = part->instrument();
    const auto long_name = part->long_name(Fraction(0, 1));
    const auto short_name = part->short_name(Fraction(0, 1));
    const auto handle = master.eid_register()->enroll(*part);
    const auto id = engrave::from_engine(part->eid());
    const auto revision = document->revision();
    auto replacement = std::make_unique<Instrument>(*instrument);
    replacement->set_id(u"journal-test");
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_part_label(Fraction(0, 1), part, u"changed long", true);
        master.record_part_label(Fraction(0, 1), part, u"changed short", false);
        master.record_part_instrument(part, std::move(replacement));
        master.record_part_membership(part, 0, false);
        master.record_part_membership(part, 0, true);
        const auto &journal = *master.mutation_journal()->active_transaction();
        EXPECT_EQ(journal.part_entries(), 5u);
        EXPECT_EQ(journal.legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject part changes"});
    }}));
    EXPECT_EQ(master.parts().front(), part);
    EXPECT_EQ(part->instrument(), instrument);
    EXPECT_EQ(part->long_name(Fraction(0, 1)), long_name);
    EXPECT_EQ(part->short_name(Fraction(0, 1)), short_name);
    const auto resolved = engrave::resolve(master, id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->object, part);
    EXPECT_EQ(master.eid_register()->live_item(handle), part);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalPart, ReattachedExcerptHasOneOwnerOnCommitAndRollback) {
    using namespace mu::engraving;
    for (bool reject : {false, true}) {
        SCOPED_TRACE(reject);
        auto document = open(fixture_name);
        ASSERT_TRUE(document);
        ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
        auto &master = engrave::harness::master_of(*document);
        auto *excerpt = master.excerpts().front();
        auto *score = excerpt->excerpt_score();
        const auto handle = master.eid_register()->enroll(*score);
        const auto id = engrave::from_engine(score->eid());
        const auto revision = document->revision();
        const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            master.record_excerpt(excerpt, false);
            master.record_excerpt(excerpt, true);
            EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
            if (reject) return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject excerpt reattach"});
            return {};
        }});
        EXPECT_EQ(result.has_value(), !reject);
        ASSERT_EQ(master.excerpts().size(), 1u);
        EXPECT_EQ(master.excerpts().front(), excerpt);
        EXPECT_EQ(master.eid_register()->live_item(handle), score);
        const auto resolved = engrave::resolve(master, id);
        ASSERT_TRUE(resolved);
        EXPECT_EQ(resolved->object, score);
        EXPECT_EQ(document->revision(), revision + (reject ? 0u : 1u));
        ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            master.record_excerpt(excerpt, false);
            return {};
        }}));
        EXPECT_TRUE(master.excerpts().empty());
        EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    }
}

TEST(JournalPart, DestroyedRetainedExcerptDoesNotDoubleDeleteOnSnapshotRecovery) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    const auto revision = document->revision();
    const auto result = engrave::api::BatchGuard(*document).run(
        {[](auto &context) -> engrave::Result<void> {
            auto &master = context.master();
            auto *excerpt = master.excerpts().front();
            master.record_excerpt(excerpt, false);
            delete excerpt;
            return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject after excerpt destruction"});
        }},
        engrave::api::Policy::SNAPSHOT, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(document->revision(), revision);
    auto &restored = engrave::harness::master_of(*document);
    EXPECT_EQ(restored.excerpts().size(), 1u);
    EXPECT_FALSE(restored.mutation_journal()->has_active_transaction());
    EXPECT_EQ(restored.layout_state.whole_count + restored.layout_state.range_count, 0u);
}

TEST(JournalPart, SharedPartMembershipSupportsRollbackAndCommittedDisposal) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto original = master.parts();
    auto *shared = new SharedPart(&master);
    const auto handle = master.eid_register()->enroll(*shared);
    {
        JournalBatch journal;
        journal.change_part_membership(shared, master.parts().size(), true);
        journal.unwind();
    }
    EXPECT_EQ(master.parts(), original);
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
    shared = new SharedPart(&master);
    const auto removed = master.eid_register()->enroll(*shared);
    master.insert_part(shared, master.parts().size());
    {
        JournalBatch journal;
        journal.change_part_membership(shared, master.parts().size() - 1, false);
        journal.cleanup(true);
    }
    EXPECT_EQ(master.parts(), original);
    EXPECT_EQ(master.eid_register()->item_from_handle(removed), nullptr);
}

TEST(JournalPart, UnjournaledExcerptRemovalDestroysItsScore) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    auto &master = engrave::harness::master_of(*document);
    auto *excerpt = master.excerpts().front();
    const auto handle = master.eid_register()->enroll(*excerpt->excerpt_score());
    ASSERT_FALSE(master.mutation_journal()->has_active_transaction());
    master.delete_excerpt(excerpt);
    EXPECT_TRUE(master.excerpts().empty());
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
}

TEST(JournalPart, DetachedExcerptOwnsItsReattachedPartUntilScoreDestruction) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    auto &master = engrave::harness::master_of(*document);
    auto *excerpt = master.excerpts().front();
    auto *part = excerpt->excerpt_score()->parts().front();
    const auto handle = master.eid_register()->enroll(*part);
    ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_part_membership(part, 0, false);
        master.record_part_membership(part, 0, true);
        master.record_excerpt(excerpt, false);
        return {};
    }}));
    EXPECT_TRUE(master.excerpts().empty());
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
}

TEST(JournalStaff, RemovedStaffRestoresAuthoredBracketRosterAndIdentity) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *staff = master.staff(0);
    const auto handle = staff->source_handle();
    const auto level = master.brackets(0).size() + 1;
    EditStaffBrackets::fill_brackets(&master, 0, level);
    auto &bracket = *master.brackets(0).back();
    bracket.set_bracket_type(BracketType::GROUP);
    bracket.set_bracket_span(3);
    bracket.set_column(level + 7);
    bracket.set_visible(false);
    bracket.label().set_long_name(u"authored group");
    bracket.label().set_short_name(u"AG");
    bracket.set_property(Pid::GROUP_BRACKET_SHOW_TEXT, false);
    bracket.set_property(Pid::GROUP_BRACKET_SHOW_BRACKET, false);
    const auto before = capture_brackets(*staff);
    master.add_system_object_staff(staff);
    const auto systems = master.system_object_staves();
    const auto revision = document->revision();
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_staff_membership(staff, muse::nidx, false);
        EXPECT_EQ(master.eid_register()->live_item(handle), nullptr);
        master.record_staff_membership(staff, 0, true);
        auto &journal = *master.mutation_journal()->active_transaction();
        EXPECT_EQ(journal.staff_entries(), 2u);
        EXPECT_EQ(journal.legacy_entries(), 0u);
        EXPECT_TRUE(journal.changes_info().changed_objects.at(staff).contains(CommandType::REMOVE_STAFF));
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject staff reattachment"});
    }}));
    EXPECT_EQ(master.staff(0), staff);
    EXPECT_EQ(master.eid_register()->live_item(handle), staff);
    EXPECT_EQ(capture_brackets(*staff), before);
    EXPECT_EQ(master.system_object_staves(), systems);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalStaff, RejectedNewStaffIsDisposedAfterItsInverses) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto before = master.staves();
    auto *part = master.parts().front();
    auto *staff = Factory::create_staff(part);
    staff->set_part(part);
    const auto handle = master.eid_register()->enroll(*staff);
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_staff_membership(staff, part->nstaves(), true);
        staff->undo_change_property(Pid::COLOR, muse::draw::Color(111, 122, 133));
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject new staff"});
    }}));
    EXPECT_EQ(master.staves(), before);
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
}

TEST(JournalStaff, SystemObjectMembershipRestoresOriginalOrder) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *part = master.parts().front();
    auto *first = master.staff(0);
    auto *second = Factory::create_staff(part);
    auto *third = Factory::create_staff(part);
    second->set_part(part);
    third->set_part(part);
    master.undo_insert_staff(second, part->nstaves(), false);
    master.undo_insert_staff(third, part->nstaves(), false);
    master.clear_system_object_staves();
    master.add_system_object_staff(third);
    master.add_system_object_staff(second);
    const auto before = master.system_object_staves();
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_system_staff(third, false);
        master.record_system_staff(first, true);
        master.record_system_staff(second, false);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->staff_entries(), 3u);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject system staff changes"});
    }}));
    EXPECT_EQ(master.system_object_staves(), before);
}

TEST(JournalStaff, SortAndMeasureOptionRollbackRetainSourceOrder) {
    using namespace mu::engraving;
    auto document = open("lyrics.mscx");
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    ASSERT_GE(master.nstaves(), 2u);
    const auto before = master.staves();
    const auto parts = master.parts();
    auto *measure = master.first_measure();
    const auto hidden = measure->hide_staff_if_empty(0);
    std::vector<staff_idx_t> order;
    for (size_t i = 0; i < master.nstaves(); ++i) order.push_back(i);
    std::swap(order[0], order[1]);
    const auto revision = document->revision();
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_staff_hide(measure, 0, AutoOnOff::ON);
        master.record_staff_order(order);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->staff_entries(), 2u);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject staff sort"});
    }}));
    EXPECT_EQ(master.staves(), before);
    EXPECT_EQ(master.parts(), parts);
    EXPECT_EQ(measure->hide_staff_if_empty(0), hidden);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalStaff, DetachedExcerptKeepsOwnershipOfReattachedStaff) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    auto &master = engrave::harness::master_of(*document);
    auto *excerpt = master.excerpts().front();
    auto *score = excerpt->excerpt_score();
    auto *staff = score->staff(0);
    const auto handle = staff->source_handle();
    ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_staff_membership(staff, muse::nidx, false);
        master.record_staff_membership(staff, 0, true);
        master.record_excerpt(excerpt, false);
        return {};
    }}));
    EXPECT_TRUE(master.excerpts().empty());
    EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
}

TEST(JournalStaffRows, RemovedRowKeepsAuthoredValuesAndSpacerIdentityForEarlierInverse) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &measure = *master.first_measure();
    auto *row = measure.mstaves().at(0);
    ASSERT_NE(row, nullptr);
    row->set_visible(false);
    row->set_stemless(true);
    row->set_hide_if_empty(AutoOnOff::OFF);
    row->set_measure_repeat_count(7);
    auto *spacer = Factory::create_spacer(&measure);
    spacer->set_track(0);
    spacer->set_spacer_type(SpacerType::UP);
    spacer->set_gap(Spatium(3.25));
    master.record_element_add(spacer);
    const auto handle = master.eid_register()->enroll(*spacer);
    const auto color = spacer->color();
    const auto gap = spacer->gap();
    ASSERT_EQ(master.eid_register()->live_item(handle), spacer);
    {
        JournalBatch journal;
        journal.change_property(*spacer, Pid::COLOR, muse::draw::Color(111, 122, 133), PropertyFlags::NOSTYLE, false);
        journal.change_mstaff(&measure, row, 0, false);
        EXPECT_EQ(master.eid_register()->live_item(handle), nullptr);
        EXPECT_EQ(journal.staff_entries(), 1u);
        EXPECT_EQ(journal.legacy_entries(), 0u);
        EXPECT_TRUE(journal.changes_info().changed_objects.at(&measure).contains(CommandType::REMOVE_M_STAFF));
        journal.unwind();
    }
    EXPECT_EQ(measure.mstaves().at(0), row);
    EXPECT_EQ(row->vspacer_up(), spacer);
    EXPECT_FALSE(row->visible());
    EXPECT_TRUE(row->stemless());
    EXPECT_EQ(row->hide_if_empty(), AutoOnOff::OFF);
    EXPECT_EQ(row->measure_repeat_count(), 7);
    EXPECT_EQ(spacer->color(), color);
    EXPECT_EQ(spacer->gap(), gap);
    EXPECT_EQ(master.eid_register()->live_item(handle), spacer);
}

TEST(JournalStaffRows, ReattachedRowHasOneOwnerOnCommitAndRollback) {
    using namespace mu::engraving;
    for (bool reject : {false, true}) {
        SCOPED_TRACE(reject);
        auto document = open(fixture_name);
        ASSERT_TRUE(document);
        auto &master = engrave::harness::master_of(*document);
        auto &measure = *master.first_measure();
        auto *row = measure.mstaves().at(0);
        const auto lines = master.eid_register()->enroll(*row->lines());
        {
            JournalBatch journal;
            journal.change_mstaff(&measure, row, 0, false);
            journal.change_mstaff(&measure, row, 0, true);
            if (reject)
                journal.unwind();
            else
                journal.cleanup(true);
        }
        EXPECT_EQ(measure.mstaves().at(0), row);
        EXPECT_EQ(master.eid_register()->item_from_handle(lines), row->lines());
    }
}

TEST(JournalStaffRows, NewRowLivesThroughInversesAndIsDisposedAfterRollback) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &measure = *master.first_measure();
    const auto before = measure.mstaves();
    auto row = std::make_unique<MStaff>();
    row->set_lines(Factory::create_staff_lines(&measure));
    auto *spacer = Factory::create_spacer(&measure);
    row->set_vspacer_up(spacer);
    const auto line_handle = master.eid_register()->enroll(*row->lines());
    const auto spacer_handle = master.eid_register()->enroll(*spacer);
    auto *part = master.parts().front();
    auto *staff = Factory::create_staff(part);
    staff->set_part(part);
    {
        JournalBatch journal;
        journal.change_staff_membership(staff, part->nstaves(), true);
        journal.change_staff_tracks(&measure, before.size(), before.size() + 1, true);
        journal.add_mstaff(&measure, std::move(row), before.size());
        journal.change_property(*spacer, Pid::COLOR, muse::draw::Color(111, 122, 133), PropertyFlags::NOSTYLE, false);
        EXPECT_EQ(journal.legacy_entries(), 0u);
        journal.unwind();
        EXPECT_EQ(measure.mstaves(), before);
        EXPECT_NE(master.eid_register()->item_from_handle(line_handle), nullptr);
        EXPECT_EQ(master.eid_register()->item_from_handle(spacer_handle), spacer);
        EXPECT_EQ(master.eid_register()->live_item(spacer_handle), nullptr);
    }
    EXPECT_EQ(master.eid_register()->item_from_handle(line_handle), nullptr);
    EXPECT_EQ(master.eid_register()->item_from_handle(spacer_handle), nullptr);
}

TEST(JournalStaffRows, RejectedOwnershipTransferDisposesNewRowBeforeArming) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &measure = *master.first_measure();
    const auto before = measure.mstaves();
    auto row = std::make_unique<MStaff>();
    row->set_lines(Factory::create_staff_lines(&measure));
    const auto lines = master.eid_register()->enroll(*row->lines());
    JournalBatch journal;
    EXPECT_THROW(journal.add_mstaff(&measure, std::move(row), before.size() + 1), std::logic_error);
    EXPECT_EQ(journal.entry_count(), 0u);
    EXPECT_EQ(measure.mstaves(), before);
    EXPECT_EQ(master.eid_register()->item_from_handle(lines), nullptr);
}

TEST(JournalStaffRows, DetachedRowIsDisposedBeforeItsExcerpt) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    auto &master = engrave::harness::master_of(*document);
    auto *excerpt = master.excerpts().front();
    auto *measure = excerpt->excerpt_score()->first_measure();
    auto *row = measure->mstaves().at(0);
    const auto lines = master.eid_register()->enroll(*row->lines());
    ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_mstaff(measure, row, 0, false);
        master.record_excerpt(excerpt, false);
        return {};
    }}));
    EXPECT_TRUE(master.excerpts().empty());
    EXPECT_EQ(master.eid_register()->item_from_handle(lines), nullptr);
}

TEST(JournalStaffRows, ExcerptDestructionInvalidatesAttachedRowsAndDetachedChildLifetimes) {
    using namespace mu::engraving;
    for (bool reattach : {false, true}) {
        SCOPED_TRACE(reattach);
        auto document = open(fixture_name);
        ASSERT_TRUE(document);
        ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
        const auto revision = document->revision();
        const auto result = engrave::api::BatchGuard(*document).run(
            {[reattach](auto &context) -> engrave::Result<void> {
                auto &master = context.master();
                auto *excerpt = master.excerpts().front();
                auto *measure = excerpt->excerpt_score()->first_measure();
                auto *row = measure->mstaves().at(0);
                master.record_mstaff(measure, row, 0, false);
                if (reattach) master.record_mstaff(measure, row, 0, true);
                master.record_excerpt(excerpt, false);
                delete excerpt;
                return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject after row owner destruction"});
            }},
            engrave::api::Policy::SNAPSHOT, std::nullopt);
        EXPECT_FALSE(result);
        EXPECT_EQ(document->revision(), revision);
        auto &restored = engrave::harness::master_of(*document);
        ASSERT_EQ(restored.excerpts().size(), 1u);
        EXPECT_NE(restored.excerpts().front()->excerpt_score()->first_measure()->mstaves().at(0), nullptr);
        EXPECT_FALSE(restored.mutation_journal()->has_active_transaction());
        EXPECT_EQ(restored.layout_state.whole_count + restored.layout_state.range_count, 0u);
    }
}

TEST(JournalStaffRows, UnjournaledRemovalDisposesOwnedSpacerAndDisplayChildren) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &measure = *master.first_measure();
    auto *row = measure.mstaves().at(0);
    auto *spacer = Factory::create_spacer(&measure);
    spacer->set_track(0);
    master.record_element_add(spacer);
    const auto spacer_handle = master.eid_register()->enroll(*spacer);
    const auto lines = master.eid_register()->enroll(*row->lines());
    ASSERT_FALSE(master.mutation_journal()->has_active_transaction());
    master.record_mstaff(&measure, row, 0, false);
    EXPECT_EQ(master.eid_register()->item_from_handle(spacer_handle), nullptr);
    EXPECT_EQ(master.eid_register()->item_from_handle(lines), nullptr);
}

TEST(JournalStaffRows, BracketRosterAndNamesSurviveNestedRemovalAndStaffRollback) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *staff = master.staff(0);
    const auto level = master.brackets(0).size();
    EditStaffBrackets::fill_brackets(&master, 0, level);
    auto *item = master.brackets(0).back();
    item->set_bracket_type(BracketType::GROUP);
    item->set_bracket_span(2);
    item->label().set_long_name(u"retained authored group");
    item->label().set_short_name(u"RAG");
    item->set_visible(false);
    item->set_property(Pid::GROUP_BRACKET_SHOW_TEXT, false);
    const auto before = capture_brackets(*staff);
    const auto revision = document->revision();
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.record_bracket_remove(staff, level);
        master.record_bracket_add(staff, level + 2, BracketType::NORMAL, 3);
        master.brackets(0).back()->undo_change_property(Pid::BRACKET_COLUMN, static_cast<int>(level + 3));
        master.record_staff_membership(staff, muse::nidx, false);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject bracket roster and staff removal"});
    }}));
    EXPECT_EQ(master.staff(0), staff);
    EXPECT_EQ(capture_brackets(*staff), before);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalStaffRows, TrackInsertionRollbackRestoresAuthoredElementOrderAndTracks) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &measure = *master.first_measure();
    auto *part = master.parts().front();
    auto *staff = Factory::create_staff(part);
    staff->set_part(part);

    struct Before {
        Segment *segment;
        std::vector<EngravingItem *> elements;
        std::vector<std::pair<EngravingItem *, track_idx_t>> tracks;
    };

    std::vector<Before> before;
    for (auto *segment = measure.first(); segment; segment = segment->next()) {
        Before saved{segment, {segment->elist().begin(), segment->elist().end()}, {}};
        for (auto *item : segment->elist())
            if (item) saved.tracks.emplace_back(item, item->track());
        for (auto *item : segment->annotations()) saved.tracks.emplace_back(item, item->track());
        before.push_back(std::move(saved));
    }
    {
        JournalBatch journal;
        journal.change_staff_membership(staff, 0, true);
        journal.change_staff_tracks(&measure, 0, 1, true);
        EXPECT_EQ(journal.legacy_entries(), 0u);
        EXPECT_TRUE(journal.changes_info().changed_objects.at(&measure).contains(CommandType::INSERT_STAVES));
        journal.unwind();
    }
    for (const auto &saved : before) {
        EXPECT_EQ(std::vector<EngravingItem *>(saved.segment->elist().begin(), saved.segment->elist().end()), saved.elements);
        for (const auto &[item, track] : saved.tracks) EXPECT_EQ(item->track(), track);
    }
}

std::unique_ptr<engrave::Document> open_mm_rest_document() {
    const auto path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/mmr/legacy_source_ownership.mscx";
    auto result = g_engine->open(path);
    EXPECT_TRUE(result) << result.error().message;
    if (!result) return {};
    auto document = std::move(*result);
    engrave::harness::master_of(*document).ensure_layout();
    return document;
}

mu::engraving::Measure *source_with_mm_rest(mu::engraving::MasterScore &master) {
    for (auto *measure = master.first_measure(); measure; measure = measure->next_measure())
        if (measure->mm_rest()) return measure;
    return nullptr;
}

TEST(JournalMMRestLinks, RejectionRestoresAssociationLinksAndAuthoredNumberState) {
    using namespace mu::engraving;
    auto document = open_mm_rest_document();
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *source = source_with_mm_rest(master);
    ASSERT_NE(source, nullptr);
    auto *overlay = source->mm_rest();
    auto *segment = overlay->first(SegmentType::CHORD_REST);
    ASSERT_NE(segment, nullptr);
    auto *rest = to_mm_rest(segment->element(0));
    ASSERT_NE(rest, nullptr);
    rest->set_property(Pid::MMREST_NUMBER_OFFSET, Spatium(2.5));
    rest->set_property(Pid::MMREST_NUMBER_VISIBLE, false);
    const auto offset = rest->get_property(Pid::MMREST_NUMBER_OFFSET);
    const auto visible = rest->get_property(Pid::MMREST_NUMBER_VISIBLE);
    const auto next = overlay->next();
    const auto previous = overlay->prev();
    const auto layout_count = master.layout_state.whole_count + master.layout_state.range_count;
    const auto revision = document->revision();
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        record_mm_rest_association(*source, nullptr);
        record_mm_rest_next(*overlay, source);
        record_mm_rest_previous(*overlay, source);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->mm_rest_entries(), 3u);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject changed display links"});
    }});
    ASSERT_FALSE(result);
    EXPECT_EQ(source->mm_rest(), overlay);
    EXPECT_EQ(overlay->next(), next);
    EXPECT_EQ(overlay->prev(), previous);
    EXPECT_EQ(rest->get_property(Pid::MMREST_NUMBER_OFFSET), offset);
    EXPECT_EQ(rest->get_property(Pid::MMREST_NUMBER_VISIBLE), visible);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, layout_count);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    master.ensure_layout();
    EXPECT_EQ(source->mm_rest(), overlay);
    EXPECT_EQ(rest->get_property(Pid::MMREST_NUMBER_OFFSET), offset);
    EXPECT_EQ(rest->get_property(Pid::MMREST_NUMBER_VISIBLE), visible);
}

TEST(JournalMMRestLinks, RejectionRestoresAbsentAssociation) {
    using namespace mu::engraving;
    auto document = open_mm_rest_document();
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *source = source_with_mm_rest(master);
    ASSERT_NE(source, nullptr);
    auto *other = source->next_measure();
    while (other && other->mm_rest()) other = other->next_measure();
    ASSERT_NE(other, nullptr);
    auto *overlay = source->mm_rest();
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        record_mm_rest_association(*source, nullptr);
        record_mm_rest_association(*other, overlay);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->mm_rest_entries(), 2u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject moved display association"});
    }});
    ASSERT_FALSE(result);
    EXPECT_EQ(source->mm_rest(), overlay);
    EXPECT_EQ(other->mm_rest(), nullptr);
}

TEST(JournalMMRestLinks, FailureBeforeAndAfterMutationKeepsOriginalAssociation) {
    using namespace mu::engraving;
    for (const bool after : {false, true}) {
        SCOPED_TRACE(after);
        auto document = open_mm_rest_document();
        ASSERT_TRUE(document);
        auto &master = engrave::harness::master_of(*document);
        auto *source = source_with_mm_rest(master);
        ASSERT_NE(source, nullptr);
        auto *overlay = source->mm_rest();

        struct HookReset {
            ~HookReset() { Transaction::set_push_hook(nullptr); }
        } reset;

        const Transaction::PushHook before_hook = [](UndoableCommand *token, bool appended) {
            if (appended) return;
            delete token;
            throw std::runtime_error("before MMR link recording");
        };
        const Transaction::PushHook after_hook = [](UndoableCommand *, bool appended) {
            if (appended) throw std::runtime_error("after MMR link mutation");
        };
        Transaction::set_push_hook(after ? after_hook : before_hook);
        const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            record_mm_rest_association(*source, nullptr);
            return {};
        }});
        ASSERT_FALSE(result);
        EXPECT_EQ(source->mm_rest(), overlay);
        EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    }
}

std::unique_ptr<engrave::Document> open_mm_rest_creation_document() {
    constexpr std::string_view source = R"(<museScore version="5.00"><Score>
<Style><createMultiMeasureRests>1</createMultiMeasureRests></Style>
<Part id="1"><Staff><StaffType group="pitched"/></Staff><Instrument id="piano">
<trackName>Piano</trackName><instrumentId>keyboard.piano</instrumentId><Channel><program value="0"/></Channel>
</Instrument></Part><Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>measure</durationType><duration>4/4</duration></Rest></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType><duration>4/4</duration></Rest></voice></Measure>
</Staff></Score></museScore>)";
    auto result = g_engine->open(std::as_bytes(std::span(source.data(), source.size())));
    EXPECT_TRUE(result) << result.error().message;
    return result ? std::move(*result) : nullptr;
}

void complete_test_mm_rest(mu::engraving::MasterScore &master, mu::engraving::MMRestCreation &creation) {
    using namespace mu::engraving;
    auto *overlay = creation.get();
    overlay->set_mm_rest_count(1);
    auto *segment = overlay->undo_get_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
    auto *rest = Factory::create_mm_rest(segment);
    rest->set_track(0);
    master.record_element_add(rest);
    creation.complete();
}

TEST(JournalMMRestCreation, FailureWithoutBatchRetiresIncompleteOverlayBeforeSnapshotSave) {
    using namespace mu::engraving;
    auto document = open_mm_rest_creation_document();
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *source = master.first_measure();
    ASSERT_EQ(source->mm_rest(), nullptr);
    SourceHandle created;
    EXPECT_THROW(
        {
            auto creation = begin_mm_rest_creation(*source, source->ticks());
            created = master.eid_register()->handle_of(creation.get());
            EXPECT_EQ(source->mm_rest(), creation.get());
            EXPECT_EQ(creation.get()->first(), nullptr);
            throw std::bad_alloc();
        },
        std::bad_alloc);
    EXPECT_EQ(source->mm_rest(), nullptr);
    EXPECT_EQ(master.eid_register()->item_from_handle(created), nullptr);
    const auto id = engrave::from_engine(source->eid());
    ASSERT_TRUE(document->edit({{engrave::SetLayoutBreak{id, "line", true}}}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalMMRestCreation, FailedBatchRetiresCompletedOverlayAndCachedSystemReference) {
    using namespace mu::engraving;
    auto document = open_mm_rest_creation_document();
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *source = master.first_measure();
    auto *system = Factory::create_system(&master);
    master.systems().push_back(system);
    SourceHandle created;
    const auto revision = document->revision();
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        auto creation = begin_mm_rest_creation(*source, source->ticks());
        created = master.eid_register()->handle_of(creation.get());
        complete_test_mm_rest(master, creation);
        system->append_measure(creation.get());
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject completed display topology"});
    }});
    ASSERT_FALSE(result);
    EXPECT_EQ(source->mm_rest(), nullptr);
    EXPECT_EQ(master.eid_register()->item_from_handle(created), nullptr);
    EXPECT_TRUE(system->measures().empty());
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    const auto id = engrave::from_engine(source->eid());
    ASSERT_TRUE(document->edit({{engrave::SetLayoutBreak{id, "line", true}}}));
}

TEST(JournalMMRestCreation, PostAttachmentFailureRestoresAbsenceBeforeStepCatchesIt) {
    using namespace mu::engraving;
    auto document = open_mm_rest_creation_document();
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    static Measure *hook_source = nullptr;
    static SourceHandle created;
    hook_source = master.first_measure();
    created = {};

    struct HookReset {
        ~HookReset() { Transaction::set_push_hook(nullptr); }
    } reset;

    Transaction::set_push_hook([](UndoableCommand *, bool appended) {
        if (!appended) return;
        auto *overlay = hook_source->mm_rest();
        EXPECT_NE(overlay, nullptr);
        EXPECT_EQ(overlay->first(), nullptr);
        created = hook_source->master_score()->eid_register()->handle_of(overlay);
        throw std::runtime_error("after attachment before first segment");
    });
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        EXPECT_THROW(std::ignore = begin_mm_rest_creation(*hook_source, hook_source->ticks()), std::runtime_error);
        EXPECT_EQ(hook_source->mm_rest(), nullptr);
        Transaction::set_push_hook(nullptr);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject caught layout failure"});
    }});
    ASSERT_FALSE(result);
    EXPECT_EQ(hook_source->mm_rest(), nullptr);
    EXPECT_EQ(master.eid_register()->item_from_handle(created), nullptr);
    const auto id = engrave::from_engine(hook_source->eid());
    ASSERT_TRUE(document->edit({{engrave::SetLayoutBreak{id, "line", true}}}));
    hook_source = nullptr;
}

TEST(JournalMMRestCreation, CompleteBatchTransfersOverlayToItsOwningScore) {
    using namespace mu::engraving;
    auto document = open_mm_rest_creation_document();
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *source = master.first_measure();
    SourceHandle created;
    ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        auto creation = begin_mm_rest_creation(*source, source->ticks());
        created = master.eid_register()->handle_of(creation.get());
        complete_test_mm_rest(master, creation);
        return {};
    }}));
    EXPECT_NE(source->mm_rest(), nullptr);
    EXPECT_EQ(master.eid_register()->item_from_handle(created), source->mm_rest());
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    const auto exported = document->export_score({.format = engrave::Format::MSCZ});
    ASSERT_TRUE(exported) << exported.error().message;
}

TEST(JournalMMRestCreation, DetachedExcerptOwnsAttachedOverlayUntilItsScoreIsDisposed) {
    using namespace mu::engraving;
    for (bool reject : {false, true}) {
        SCOPED_TRACE(reject);
        auto document = open_mm_rest_creation_document();
        ASSERT_TRUE(document);
        ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
        auto &master = engrave::harness::master_of(*document);
        auto *excerpt = master.excerpts().front();
        auto *source = excerpt->excerpt_score()->first_measure();
        ASSERT_EQ(source->mm_rest(), nullptr);
        SourceHandle created;
        const auto source_handle = master.eid_register()->handle_of(source);
        const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            auto creation = begin_mm_rest_creation(*source, source->ticks());
            created = master.eid_register()->handle_of(creation.get());
            complete_test_mm_rest(master, creation);
            master.record_excerpt(excerpt, false);
            if (reject) return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject detached excerpt overlay"});
            return {};
        }});
        EXPECT_EQ(result.has_value(), !reject);
        EXPECT_EQ(master.eid_register()->item_from_handle(created), nullptr);
        if (reject) {
            ASSERT_EQ(master.excerpts().size(), 1u);
            EXPECT_EQ(master.excerpts().front(), excerpt);
            EXPECT_EQ(master.eid_register()->item_from_handle(source_handle), source);
            EXPECT_EQ(source->mm_rest(), nullptr);
        } else {
            EXPECT_TRUE(master.excerpts().empty());
            EXPECT_EQ(master.eid_register()->item_from_handle(source_handle), nullptr);
        }
        EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    }
}

TEST(JournalMMRestCreation, SnapshotAbandonmentToleratesAlreadyDestroyedOverlayOwner) {
    using namespace mu::engraving;
    auto document = open_mm_rest_creation_document();
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    const auto revision = document->revision();
    const auto result = engrave::api::BatchGuard(*document).run(
        {[](auto &context) -> engrave::Result<void> {
            auto &master = context.master();
            auto *excerpt = master.excerpts().front();
            auto *source = excerpt->excerpt_score()->first_measure();
            auto creation = begin_mm_rest_creation(*source, source->ticks());
            const auto handle = master.eid_register()->handle_of(creation.get());
            complete_test_mm_rest(master, creation);
            master.record_excerpt(excerpt, false);
            delete excerpt;
            EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
            return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject destroyed overlay owner"});
        }},
        engrave::api::Policy::SNAPSHOT, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(document->revision(), revision);
    auto &restored = engrave::harness::master_of(*document);
    ASSERT_EQ(restored.excerpts().size(), 1u);
    EXPECT_EQ(restored.excerpts().front()->excerpt_score()->first_measure()->mm_rest(), nullptr);
    EXPECT_FALSE(restored.mutation_journal()->has_active_transaction());
}

TEST(JournalMMRestCreation, CanceledOverlayRetiresBeforeLateExcerptChildDestruction) {
    using namespace mu::engraving;
    auto document = open_mm_rest_creation_document();
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    const auto revision = document->revision();
    const auto result = engrave::api::BatchGuard(*document).run(
        {[](auto &context) -> engrave::Result<void> {
            auto &master = context.master();
            auto *excerpt = master.excerpts().front();
            auto *source = excerpt->excerpt_score()->first_measure();
            SourceHandle handle;
            {
                auto creation = begin_mm_rest_creation(*source, source->ticks());
                handle = master.eid_register()->handle_of(creation.get());
                // Cancel before a first segment exists, retaining the lifetime
                // in the arena so its earlier inverses can still be replayed.
            }
            EXPECT_EQ(source->mm_rest(), nullptr);
            EXPECT_NE(master.eid_register()->item_from_handle(handle), nullptr);
            master.record_excerpt(excerpt, false);
            delete excerpt;
            EXPECT_EQ(master.eid_register()->item_from_handle(handle), nullptr);
            return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject canceled overlay owner"});
        }},
        engrave::api::Policy::SNAPSHOT, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(document->revision(), revision);
    auto &restored = engrave::harness::master_of(*document);
    ASSERT_EQ(restored.excerpts().size(), 1u);
    EXPECT_EQ(restored.excerpts().front()->excerpt_score()->first_measure()->mm_rest(), nullptr);
    EXPECT_FALSE(restored.mutation_journal()->has_active_transaction());
}

TEST(JournalAutomation, SourceSetupRejectsActiveBatchAndPreservesEarlierEditsOnFailure) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    master.init_automation();
    AutomationPoint initial;
    initial.value.out_value = normalize_tempo(BeatsPerSecond(2.0));
    AutomationPointEdits baseline{{0, AutomationPointEdit::SetPoint{initial}}};
    master.apply_automation_points(tempo_key, baseline);
    const auto resolved = engrave::resolve(master, find_id(*document, "Note"));
    ASSERT_TRUE(resolved);
    auto *note = to_note(resolved->object);
    const auto color = note->color();
    const auto curves = master.automation_data()->dump();
    const auto revision = document->revision();
    AutomationPoint point;
    point.value.out_value = normalize_tempo(BeatsPerSecond(3.0));
    AutomationPointEdits edits{{480, AutomationPointEdit::SetPoint{point}}};
    const auto result = chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        note->undo_change_property(Pid::COLOR, muse::draw::Color(111, 122, 133));
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        master.apply_automation_points(tempo_key, edits);
        return {};
    }});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::EDIT_REJECTED);
    EXPECT_EQ(note->color(), color);
    EXPECT_EQ(master.automation_data()->dump(), curves);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalWholeStyle, ChordSnapshotOwnsItsRenderActions) {
    using namespace mu::engraving;
    ChordList source;
    source.render_list_root.push_back(std::make_shared<RenderActionMoveScaled>(1.25, -0.5));
    source.render_list_function.push_back(std::make_shared<RenderActionMoveTextWidth>(muse::String(u"root")));
    ChordDescription description(900001);
    description.names.push_back(u"journal-custom");
    description.render_list.push_back(std::make_shared<RenderActionSet>(muse::String(u"journal-custom")));
    description.render_list_stacked = description.render_list;
    source.emplace(description.id, std::move(description));
    source.set_custom_chord_list(true);
    auto copy = source.owned_copy();
    EXPECT_NE(copy.render_list_root.front().get(), source.render_list_root.front().get());
    EXPECT_NE(copy.render_list_function.front().get(), source.render_list_function.front().get());
    const auto *original = source.description(900001);
    const auto *cloned = copy.description(900001);
    ASSERT_NE(original, nullptr);
    ASSERT_NE(cloned, nullptr);
    EXPECT_NE(cloned->render_list.front().get(), original->render_list.front().get());
    EXPECT_NE(cloned->render_list_stacked.front().get(), original->render_list_stacked.front().get());
    EXPECT_TRUE(static_cast<const RenderActionMove &>(*copy.render_list_root.front()).scaled());
    EXPECT_FALSE(static_cast<const RenderActionSet &>(*copy.render_list_function.front()).render_text());
    source.unload();
    EXPECT_TRUE(copy.custom_chord_list());
    EXPECT_EQ(copy.description(900001)->names.front(), muse::String(u"journal-custom"));
}

TEST(JournalWholeStyle, RejectedStyleRestoresEmbeddedCustomChordDefinitions) {
    using namespace mu::engraving;
    auto seed = open(fixture_name);
    ASSERT_TRUE(seed);
    auto &original = engrave::harness::master_of(*seed);
    ChordDescription custom(900001);
    custom.names.push_back(u"journal-custom");
    custom.xml_kind = u"major";
    custom.chord = HChord(0, 4, 7);
    custom.render_list.push_back(std::make_shared<RenderActionSet>(muse::String(u"journal-custom")));
    original.chord_list()->emplace(custom.id, std::move(custom));
    original.chord_list()->set_custom_chord_list(true);
    const auto archive = seed->export_score({.format = engrave::Format::MSCZ});
    ASSERT_TRUE(archive);
    const auto serialize = [](const ChordList &list) {
        auto buffer = muse::io::Buffer::opened(muse::io::IODevice::WRITE_ONLY);
        EXPECT_TRUE(list.write(&buffer));
        return buffer.data();
    };
    for (bool whole : {false, true}) {
        SCOPED_TRACE(whole);
        auto opened = g_engine->open(*archive, {.format = engrave::Format::MSCZ});
        ASSERT_TRUE(opened);
        auto document = std::move(*opened);
        auto &master = engrave::harness::master_of(*document);
        ASSERT_TRUE(master.chord_list()->custom_chord_list());
        ASSERT_NE(master.chord_list()->description(900001), nullptr);
        const auto before = serialize(*master.chord_list());
        const auto revision = document->revision();
        EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            if (whole) {
                MStyle requested = master.style();
                requested.set(Sid::STAFF_DISTANCE, Spatium(master.style().style_s(Sid::STAFF_DISTANCE).val() + 0.75));
                master.transaction_manager()->current_or_dummy_transaction().replace_style(master, requested);
            } else {
                master.undo_change_style_val(Sid::CHORD_EXTENSION_MAG, master.style().style_d(Sid::CHORD_EXTENSION_MAG) + 0.25);
            }
            EXPECT_EQ(master.chord_list()->description(900001), nullptr);
            return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject after replacing embedded chord definitions"});
        }}));
        EXPECT_TRUE(master.chord_list()->custom_chord_list());
        EXPECT_NE(master.chord_list()->description(900001), nullptr);
        EXPECT_EQ(serialize(*master.chord_list()), before);
        EXPECT_EQ(document->revision(), revision);
        const auto saved = document->export_score({.format = engrave::Format::MSCZ});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCZ});
        ASSERT_TRUE(reopened);
        EXPECT_NE(engrave::harness::master_of(**reopened).chord_list()->description(900001), nullptr);
    }
}

TEST(JournalWholeStyle, SpatiumRollbackSurvivesRebuiltMmrDisplayCopies) {
    using namespace mu::engraving;
    const auto path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/write470/minimal.mscx";
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file);
    std::string source((std::istreambuf_iterator<char>(file)), {});
    source.insert(source.find("</Style>"), "<createMultiMeasureRests>1</createMultiMeasureRests>");
    const auto chord_begin = source.find("<Chord>");
    const auto chord_end = source.find("</Chord>", chord_begin) + std::string("</Chord>").size();
    source.replace(chord_begin, chord_end - chord_begin, "<Rest><durationType>measure</durationType></Rest>");
    source.insert(source.find("</Staff>", source.find("<Measure>")),
                  "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>");
    for (bool whole : {false, true}) {
        SCOPED_TRACE(whole);
        auto opened = g_engine->open(std::as_bytes(std::span(source.data(), source.size())), {.format = engrave::Format::MSCX});
        ASSERT_TRUE(opened);
        auto document = std::move(*opened);
        auto &master = engrave::harness::master_of(*document);
        auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
        ASSERT_NE(segment, nullptr);
        auto *tempo = Factory::create_tempo_text(segment);
        tempo->set_track(0);
        tempo->set_offset(muse::PointF(0.5, 0.75));
        segment->add(tempo);
        engrave::assign_missing_ids(master);
        master.do_layout();
        auto *copy = const_cast<EngravingItem *>(master.layout_copy(*tempo));
        ASSERT_NE(copy, nullptr);
        ASSERT_FALSE(copy->offset().is_null());
        const auto old_copy = master.eid_register()->enroll(*copy);
        const auto offset = tempo->offset();
        const double spatium = master.style().spatium();
        const auto revision = document->revision();
        EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            if (whole) {
                MStyle requested = master.style();
                requested.set_spatium(spatium * 2.0);
                master.transaction_manager()->current_or_dummy_transaction().replace_style(master, requested);
            } else {
                master.undo_change_style_val(Sid::SPATIUM, spatium * 2.0);
            }
            engrave::DocumentAccess::impl(*document).ensure_layout(master);
            EXPECT_EQ(master.eid_register()->item_from_handle(old_copy), nullptr);
            return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject after rebuilding MMR display copies"});
        }}));
        EXPECT_FALSE(engrave::DocumentAccess::impl(*document).poisoned);
        EXPECT_EQ(tempo->offset(), offset);
        EXPECT_DOUBLE_EQ(master.style().spatium(), spatium);
        EXPECT_EQ(document->revision(), revision);
        EXPECT_TRUE(document->export_score({.format = engrave::Format::MSCX}));
    }
}

TEST(JournalWholeStyle, LoaderUsesOwnedReplacementAndPreservesOverlapSemantics) {
    using namespace mu::engraving;
    for (bool overlap : {false, true}) {
        SCOPED_TRACE(overlap);
        auto document = open(fixture_name);
        ASSERT_TRUE(document);
        auto &master = engrave::harness::master_of(*document);
        ASSERT_TRUE(master.style().is_default(Sid::SMALL_NOTE_MAG));
        const auto default_mag = master.style().style_v(Sid::SMALL_NOTE_MAG);
        const double custom_mag = default_mag.to_double() + 0.25;
        master.style().set(Sid::SMALL_NOTE_MAG, custom_mag);
        MStyle requested = master.style();
        requested.set(Sid::SMALL_NOTE_MAG, default_mag);
        const auto distance = Spatium(master.style().style_s(Sid::STAFF_DISTANCE).val() + 0.75);
        requested.set(Sid::STAFF_DISTANCE, distance);
        muse::io::Buffer output;
        ASSERT_TRUE(output.open(muse::io::IODevice::WRITE_ONLY));
        ASSERT_TRUE(requested.write(&output));
        auto bytes = output.data();
        output.close();
        muse::io::Buffer input(&bytes);
        bool observed = false;
        engrave::DocumentAccess::edit_hooks(*document).before_cleanup = [&] {
            observed = true;
            const auto &batch = *master.mutation_journal()->active_transaction();
            EXPECT_EQ(batch.style_replacement_entries(), 1u);
            EXPECT_EQ(batch.legacy_entries(), 0u);
            EXPECT_FALSE(batch.structural_since(0));
            const auto changes = batch.changes_info();
            EXPECT_TRUE(changes.changed_style_id_set.contains(Sid::STAFF_DISTANCE));
            EXPECT_EQ(changes.changed_style_id_set.contains(Sid::SMALL_NOTE_MAG), !overlap);
        };
        ASSERT_TRUE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
            if (!EditStyle::load_style(master.transaction_manager()->current_or_dummy_transaction(), &master, input, false, overlap))
                return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "style loader failed"});
            return {};
        }}));
        EXPECT_TRUE(observed);
        EXPECT_EQ(master.style().style_s(Sid::STAFF_DISTANCE), distance);
        EXPECT_DOUBLE_EQ(master.style().style_d(Sid::SMALL_NOTE_MAG), overlap ? custom_mag : default_mag.to_double());
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    }
}

TEST(JournalWholeStyle, RejectionRestoresStyleAndAuthoredOverridesAfterNestedConcertPitch) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto appearance = line.appearance_state();
    const auto before = master.style();
    const auto revision = document->revision();
    MStyle requested = before;
    requested.set_spatium(before.spatium() * 2.0);
    requested.set(Sid::CONCERT_PITCH, !before.style_b(Sid::CONCERT_PITCH));
    requested.set(Sid::STAFF_DISTANCE, Spatium(before.style_s(Sid::STAFF_DISTANCE).val() + 0.75));
    EXPECT_FALSE(chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
        master.transaction_manager()->current_or_dummy_transaction().replace_style(master, requested);
        for (size_t i = 0; i < appearance.controls.size(); ++i) EXPECT_EQ(line.source_offsets()[i], appearance.controls[i] * 2.0);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->style_replacement_entries(), 1u);
        EXPECT_GE(master.mutation_journal()->active_transaction()->style_entries(), 1u);
        return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject whole style after nested callbacks"});
    }}));
    for (int i = 0; i < static_cast<int>(Sid::STYLES); ++i) {
        const auto id = static_cast<Sid>(i);
        EXPECT_EQ(master.style().style_v(id), before.style_v(id)) << i;
    }
    expect_chord_line(line, appearance);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalWholeStyle, PartialSpatiumCallbackFailureRestoresVisitedAndUntouchedSource) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto appearance = line.appearance_state();
    auto *tremolo = Factory::create_tremolo_single_chord(line.chord());
    tremolo->set_tremolo_type(TremoloType::R8);
    tremolo->set_offset(muse::PointF(0.25, 0.75));
    line.chord()->add(tremolo);
    engrave::assign_missing_ids(master);
    const auto offset = tremolo->offset();
    const auto before = master.style();
    const auto revision = document->revision();
    MStyle requested = before;
    requested.set_spatium(before.spatium() * 2.0);
    static const EngravingItem *fail_after = nullptr;
    static const ChordLine *untouched = nullptr;
    static std::array<muse::PointF, 4> controls;
    static bool injected = false;
    fail_after = tremolo;
    untouched = &line;
    controls = appearance.controls;
    injected = false;

    struct ResetHook {
        ~ResetHook() { set_spatium_item_hook(nullptr); }
    } reset_hook;

    set_spatium_item_hook([](const auto *item) {
        if (item == fail_after) {
            EXPECT_FALSE(injected);
            EXPECT_EQ(untouched->source_offsets(), controls);
            injected = true;
            throw std::bad_alloc();
        }
    });
    EXPECT_THROW((void) chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
                     master.transaction_manager()->current_or_dummy_transaction().replace_style(master, requested);
                     return {};
                 }}),
                 std::bad_alloc);
    EXPECT_TRUE(injected);
    for (int i = 0; i < static_cast<int>(Sid::STYLES); ++i) {
        const auto id = static_cast<Sid>(i);
        EXPECT_EQ(master.style().style_v(id), before.style_v(id)) << i;
    }
    expect_chord_line(line, appearance);
    EXPECT_EQ(tremolo->offset(), offset);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(JournalTextFormat, ApiFormattingUsesOwnedPayloads) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto id = find_id(*document, "Text");
    auto &text = *to_text_base(&engrave::harness::element(*document, id));
    text.set_xml_text("<b>authored</b> text <sym>accidentalSharp</sym>");
    bool observed = false;
    engrave::DocumentAccess::edit_hooks(*document).before_cleanup = [&] {
        observed = true;
        const auto &batch = *master.mutation_journal()->active_transaction();
        EXPECT_EQ(batch.text_format_entries(), 4u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
    };
    ASSERT_TRUE(document->edit({{SetProperty{id, "FONT_SIZE", 14.25}, SetProperty{id, "family", std::string("DejaVu Sans")},
                                 SetProperty{id, "fontStyle", std::int64_t(1)}, SetProperty{id, "TEXT_SCRIPT_ALIGN", std::int64_t(1)}}}));
    EXPECT_TRUE(observed);
    EXPECT_DOUBLE_EQ(text.get_property(Pid::FONT_SIZE).to_double(), 14.25);
    EXPECT_EQ(text.get_property(Pid::FONT_FACE).value<muse::String>(), muse::String(u"DejaVu Sans"));
    EXPECT_TRUE(text.xml_text().contains(u"<sym>accidentalSharp</sym>"));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalTextFormat, RejectionRestoresMarkupBaseFormatFlagsAndSelection) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto id = find_id(*document, "Text");
    auto &text = *to_text_base(&engrave::harness::element(*document, id));
    text.set_xml_text("<b>authored</b> text <sym>accidentalSharp</sym>");
    auto &cursor = *text.cursor();
    cursor.format()->set_font_family(muse::String(u"journal base font"));
    cursor.format()->set_font_size(18.25);
    cursor.format()->set_style(FontStyle::ITALIC);
    cursor.set_row(0);
    cursor.set_column(2);
    cursor.set_select_line(0);
    cursor.set_select_column(1);
    const auto format = *cursor.format();
    const auto markup = text.xml_text();
    constexpr std::array ids{Pid::FONT_SIZE, Pid::FONT_FACE, Pid::FONT_STYLE, Pid::TEXT_SCRIPT_ALIGN};
    std::array<PropertyFlags, ids.size()> flags;
    for (size_t i = 0; i < ids.size(); ++i) flags[i] = text.property_flags(ids[i]);
    const auto revision = document->revision();
    engrave::DocumentAccess::edit_hooks(*document).before_cleanup = [&] {
        EXPECT_EQ(master.mutation_journal()->active_transaction()->text_format_entries(), 4u);
        EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
        throw std::runtime_error("reject source formatting");
    };
    EXPECT_FALSE(document->edit({{SetProperty{id, "FONT_SIZE", 14.25}, SetProperty{id, "family", std::string("DejaVu Sans")},
                                  SetProperty{id, "fontStyle", std::int64_t(1)}, SetProperty{id, "TEXT_SCRIPT_ALIGN", std::int64_t(1)}}}));
    EXPECT_EQ(text.xml_text(), markup);
    EXPECT_EQ(*cursor.format(), format);
    EXPECT_EQ(cursor.row(), 0u);
    EXPECT_EQ(cursor.column(), 2u);
    EXPECT_EQ(cursor.select_line(), 0u);
    EXPECT_EQ(cursor.select_column(), 1u);
    for (size_t i = 0; i < ids.size(); ++i) EXPECT_EQ(text.property_flags(ids[i]), flags[i]);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalValues, PlainPropertiesTextAndStylesUseOwnedPayloads) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto note = find_id(*document, "Note");
    const auto text = find_id(*document, "Text");
    const double distance = master.style().style_v(mu::engraving::Sid::STAFF_DISTANCE).to_double() + 1.0;
    bool observed = false;
    engrave::DocumentAccess::edit_hooks(*document).before_cleanup = [&] {
        observed = true;
        const auto &batch = *master.mutation_journal()->active_transaction();
        EXPECT_GE(batch.property_entries(), 2u);
        EXPECT_EQ(batch.style_entries(), 1u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
    };
    ASSERT_TRUE(
        document->edit({{SetProperty{note, "color", Color{12, 34, 56}}, SetProperty{text, "text", std::string("<b>journal value</b>")},
                         SetStyle{{}, "staffDistance", distance}}}));
    EXPECT_TRUE(observed);
    EXPECT_EQ(property(*document, note, "color"), engrave::Value(Color{12, 34, 56}));
    EXPECT_NE(std::get<std::string>(property(*document, text, "text")).find("journal value"), std::string::npos);
    EXPECT_DOUBLE_EQ(master.style().style_v(mu::engraving::Sid::STAFF_DISTANCE).to_double(), distance);
    EXPECT_EQ(master.mutation_journal()->size(), 0u);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(JournalValues, RejectedPropertyTextAndStyleBatchRestoresOwnedValues) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    const auto note = find_id(*document, "Note");
    const auto text = find_id(*document, "Text");
    const auto color_before = property(*document, note, "color");
    const auto text_before = property(*document, text, "text");
    const double distance = master.style().style_v(mu::engraving::Sid::STAFF_DISTANCE).to_double();
    const auto revision = document->revision();
    engrave::DocumentAccess::edit_hooks(*document).before_cleanup = [&] {
        const auto &batch = *master.mutation_journal()->active_transaction();
        EXPECT_GE(batch.property_entries(), 2u);
        EXPECT_EQ(batch.style_entries(), 1u);
        EXPECT_EQ(batch.legacy_entries(), 0u);
        throw std::runtime_error("reject owned source values");
    };
    EXPECT_FALSE(
        document->edit({{SetProperty{note, "color", Color{12, 34, 56}}, SetProperty{text, "text", std::string("<i>replacement markup</i>")},
                         SetStyle{{}, "staffDistance", distance + 1.0}}}));
    EXPECT_EQ(property(*document, note, "color"), color_before);
    EXPECT_EQ(property(*document, text, "text"), text_before);
    EXPECT_DOUBLE_EQ(master.style().style_v(mu::engraving::Sid::STAFF_DISTANCE).to_double(), distance);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(JournalValues, StyleRollbackPreservesAuthoredChordLineControls) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto source = line.appearance_state();
    const double spatium = master.style().spatium();
    const auto result =
        chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
                                         master.undo_change_style_val(mu::engraving::Sid::SPATIUM, spatium * 2.0);
                                         return {};
                                     },
                                     [&](auto &) -> engrave::Result<void> {
                                         for (size_t i = 0; i < source.controls.size(); ++i)
                                             EXPECT_EQ(line.source_offsets()[i], source.controls[i] * 2.0);
                                         EXPECT_EQ(master.mutation_journal()->active_transaction()->style_entries(), 1u);
                                         EXPECT_EQ(master.mutation_journal()->active_transaction()->legacy_entries(), 0u);
                                         return std::unexpected(engrave::Error{ErrorCode::EDIT_REJECTED, "reject after source scaling"});
                                     }});
    EXPECT_FALSE(result);
    expect_chord_line(line, source);
    EXPECT_DOUBLE_EQ(master.style().spatium(), spatium);
}

TEST(JournalValues, PartialSpatiumFailureRestoresVisitedAndUntouchedSources) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto &line = chord_line(*document);
    seed_controls(line);
    const auto source = line.appearance_state();
    auto *tremolo = mu::engraving::Factory::create_tremolo_single_chord(line.chord());
    tremolo->set_tremolo_type(mu::engraving::TremoloType::R8);
    tremolo->set_offset(muse::PointF(0.25, 0.75));
    line.chord()->add(tremolo);
    engrave::assign_missing_ids(master);
    const auto offset = tremolo->offset();
    const auto revision = document->revision();
    const double spatium = master.style().spatium();
    static const mu::engraving::EngravingItem *fail_after = nullptr;
    static const ChordLine *untouched = nullptr;
    static std::array<muse::PointF, 4> controls;
    static bool injected = false;
    fail_after = tremolo;
    untouched = &line;
    controls = source.controls;
    injected = false;

    struct ResetHook {
        ~ResetHook() { mu::engraving::set_spatium_item_hook(nullptr); }
    } reset_hook;

    mu::engraving::set_spatium_item_hook([](const auto *item) {
        if (item == fail_after) {
            EXPECT_FALSE(injected);
            EXPECT_EQ(untouched->source_offsets(), controls);
            injected = true;
            throw std::bad_alloc();
        }
    });
    EXPECT_THROW((void) chord_line_batch(*document, {[&](auto &) -> engrave::Result<void> {
                     master.undo_change_style_val(mu::engraving::Sid::SPATIUM, spatium * 2.0);
                     return {};
                 }}),
                 std::bad_alloc);
    EXPECT_TRUE(injected);
    expect_chord_line(line, source);
    EXPECT_EQ(tremolo->offset(), offset);
    EXPECT_DOUBLE_EQ(master.style().spatium(), spatium);
    EXPECT_EQ(document->revision(), revision);
    EXPECT_FALSE(master.mutation_journal()->has_active_transaction());
}

TEST(AuthoredOverrides, SortedOwningValuesAndAbsentRollback) {
    mu::engraving::AuthoredOverrideTable table;
    const mu::engraving::AuthoredChildKey a{mu::engraving::AuthoredChildRole::DOT, 0};
    const mu::engraving::AuthoredChildKey b{mu::engraving::AuthoredChildRole::DOT, 3};
    table.set({b, Pid::COLOR, muse::draw::Color(10, 20, 30), mu::engraving::PropertyFlags::NOSTYLE});
    table.set({a, Pid::OFFSET, muse::PointF(0, 0), mu::engraving::PropertyFlags::UNSTYLED});
    ASSERT_EQ(table.entries.size(), 2u);
    EXPECT_EQ(table.entries.front().child, a);
    auto copy = table;
    table.erase(a, Pid::OFFSET);
    ASSERT_NE(copy.find(a, Pid::OFFSET), nullptr);
    EXPECT_EQ(copy.find(a, Pid::OFFSET)->flags, mu::engraving::PropertyFlags::UNSTYLED);

    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto *note = chord_line(*document).chord()->notes().front();
    std::unique_ptr<mu::engraving::NoteDot> dot(mu::engraving::Factory::create_note_dot(note));
    mu::engraving::bind_authored_child(*dot, a);
    const auto before = note->authored_overrides.entries.size();
    const auto carriers = note->authored_overrides.carriers.size();
    auto result = engrave::api::BatchGuard(*document).run(
        {[&](engrave::api::EditContext &) -> engrave::Result<void> {
             dot->undo_change_property(Pid::OFFSET, muse::PointF(0, 0));
             EXPECT_NE(note->authored_overrides.find(a, Pid::OFFSET), nullptr);
             EXPECT_EQ(engrave::harness::master_of(*document).mutation_journal()->active_transaction()->authored_override_entries(), 1u);
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after explicit zero"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(note->authored_overrides.entries.size(), before);
    EXPECT_EQ(note->authored_overrides.carriers.size(), carriers);
    EXPECT_EQ(note->authored_overrides.find(a, Pid::OFFSET), nullptr);
}

TEST(AuthoredOverrides, JournalResetAndRejectedAppearancePreserveSourceRecords) {
    using namespace mu::engraving;
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto *note = chord_line(*document).chord()->notes().front();
    std::unique_ptr<NoteDot> dot(Factory::create_note_dot(note));
    const AuthoredChildKey key{AuthoredChildRole::DOT, 2};
    bind_authored_child(*dot, key);
    dot->set_property(Pid::OFFSET, muse::PointF(0, 0));
    dot->set_property_flags(Pid::OFFSET, PropertyFlags::UNSTYLED);
    const auto before = note->authored_overrides;
    auto result = engrave::api::BatchGuard(*document).run(
        {[&](engrave::api::EditContext &) -> engrave::Result<void> {
             dot->undo_reset_property(Pid::OFFSET);
             EXPECT_EQ(note->authored_overrides.find(key, Pid::OFFSET), nullptr);
             dot->undo_change_property(Pid::COLOR, muse::draw::Color(12, 34, 56));
             EXPECT_EQ(engrave::harness::master_of(*document).mutation_journal()->active_transaction()->authored_override_entries(), 2u);
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after reset and appearance"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    const auto &after = note->authored_overrides;
    ASSERT_EQ(after.entries.size(), before.entries.size());
    for (size_t i = 0; i < before.entries.size(); ++i) {
        EXPECT_EQ(after.entries[i].child, before.entries[i].child);
        EXPECT_EQ(after.entries[i].pid, before.entries[i].pid);
        EXPECT_EQ(after.entries[i].value, before.entries[i].value);
        EXPECT_EQ(after.entries[i].flags, before.entries[i].flags);
        EXPECT_EQ(after.entries[i].unit, before.entries[i].unit);
    }
    ASSERT_EQ(after.carriers.size(), before.carriers.size());
    for (size_t i = 0; i < before.carriers.size(); ++i) {
        EXPECT_EQ(after.carriers[i].child, before.carriers[i].child);
        EXPECT_EQ(after.carriers[i].type, before.carriers[i].type);
        EXPECT_EQ(after.carriers[i].legacy_eid, before.carriers[i].legacy_eid);
        EXPECT_EQ(after.carriers[i].linked_to, before.carriers[i].linked_to);
        EXPECT_EQ(after.carriers[i].track, before.carriers[i].track);
    }
}

TEST(SpannerSegmentEdit, FailedBatchRestoresLegacyOffset2Baseline) {
    auto document = open("hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    const auto &spanners = engrave::harness::master_of(*document).spanner_map().map();
    ASSERT_FALSE(spanners.empty());
    auto *spanner = spanners.begin()->second;
    ASSERT_FALSE(spanner->spanner_segments().empty());
    auto *segment = spanner->spanner_segments().front();
    const auto before = segment->get_property(Pid::OFFSET2);
    const auto flags = segment->property_flags(Pid::OFFSET2);
    const auto source = spanner->authored_overrides.entries;
    auto result = engrave::api::BatchGuard(*document).run(
        {[segment](engrave::api::EditContext &) -> engrave::Result<void> {
             segment->undo_change_property(Pid::OFFSET2, muse::PointF(1.0, -2.0));
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after legacy segment offset2"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(segment->get_property(Pid::OFFSET2), before);
    EXPECT_EQ(segment->property_flags(Pid::OFFSET2), flags);
    ASSERT_EQ(spanner->authored_overrides.entries.size(), source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        EXPECT_EQ(spanner->authored_overrides.entries[i].value, source[i].value);
        EXPECT_EQ(spanner->authored_overrides.entries[i].flags, source[i].flags);
    }
}

TEST(TremoloOverrideEdit, FailedBatchRestoresManualEndpointsAndPresence) {
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) /
                                 "src/engraving/tests/playback/playbackeventsrenderer_data/two_chords_tremolo/two_chords_tremolo.mscx");
    ASSERT_TRUE(opened) << opened.error().message;
    auto &document = **opened;
    mu::engraving::TremoloTwoChord *tremolo = nullptr;
    engrave::harness::master_of(document).scan_elements([&](mu::engraving::EngravingItem *item) {
        if (item->type() == mu::engraving::ElementType::TREMOLO_TWOCHORD) tremolo = static_cast<mu::engraving::TremoloTwoChord *>(item);
    });
    ASSERT_NE(tremolo, nullptr);
    const auto before = tremolo->get_property(Pid::BEAM_POS);
    const bool modified = tremolo->user_modified();
    auto result = engrave::api::BatchGuard(document).run(
        {[tremolo](engrave::api::EditContext &) -> engrave::Result<void> {
             tremolo->undo_change_property(Pid::USER_MODIFIED, true);
             tremolo->undo_change_property(Pid::BEAM_POS, PropertyValue::from_value(muse::PairF(3.25, 4.5)));
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after tremolo endpoints"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(tremolo->get_property(Pid::BEAM_POS), before);
    EXPECT_EQ(tremolo->user_modified(), modified);
}

TEST(ParenthesisGroupEdit, FailedBatchRestoresAuthoredAppearanceBaseline) {
    auto document = open("ghost_note.mscx");
    ASSERT_TRUE(document);
    mu::engraving::Parenthesis *left = nullptr;
    engrave::harness::master_of(*document).scan_elements([&](mu::engraving::EngravingItem *item) {
        if (left || !item->is_note()) return;
        const auto &groups = mu::engraving::to_note(item)->chord()->note_parentheses();
        if (!groups.empty()) left = groups.front()->left_paren();
    });
    ASSERT_NE(left, nullptr);
    const auto before = left->get_property(Pid::OFFSET);
    const auto flags = left->property_flags(Pid::OFFSET);
    auto result = engrave::api::BatchGuard(*document).run(
        {[left](engrave::api::EditContext &) -> engrave::Result<void> {
             left->undo_change_property(Pid::OFFSET, muse::PointF(1.0, -2.0));
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after parenthesis appearance"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(left->get_property(Pid::OFFSET), before);
    EXPECT_EQ(left->property_flags(Pid::OFFSET), flags);
}

TEST(StemOverrideEdit, FailedBatchRestoresOwnerLength) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->page_count());
    auto &chord = *chord_line(*document).chord();
    auto *stem = chord.stem();
    ASSERT_NE(stem, nullptr);
    const auto before = chord.stem_user_length();
    const auto revision = document->revision();
    auto result = engrave::api::BatchGuard(*document).run(
        {[stem](engrave::api::EditContext &) -> engrave::Result<void> {
             stem->undo_change_property(Pid::USER_LEN, mu::engraving::Spatium(3.25));
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after stem length"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(chord.stem_user_length(), before);
    EXPECT_EQ(stem->user_length(), before);
    EXPECT_EQ(document->revision(), revision);
}

TEST(TupletNumberEdit, FailedBatchRestoresStyleWithoutNumberDisplay) {
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) /
                                 "src/importexport/mnx/tests/data/project_examples/tupletSimple.mnx");
    ASSERT_TRUE(opened) << opened.error().message;
    auto &document = **opened;
    auto &tuplet = *mu::engraving::to_tuplet(&engrave::harness::element(document, find_id(document, "Tuplet")));
    delete tuplet.number();
    tuplet.set_number(nullptr);
    const auto before = tuplet.get_property(Pid::FONT_SIZE);
    const auto flags = tuplet.property_flags(Pid::FONT_SIZE);
    const auto revision = document.revision();
    auto result = engrave::api::BatchGuard(document).run(
        {[&tuplet](engrave::api::EditContext &) -> engrave::Result<void> {
             tuplet.undo_change_property(Pid::FONT_SIZE, 19.0);
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after tuplet number style"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(tuplet.get_property(Pid::FONT_SIZE), before);
    EXPECT_EQ(tuplet.property_flags(Pid::FONT_SIZE), flags);
    EXPECT_EQ(tuplet.number(), nullptr);
    EXPECT_EQ(document.revision(), revision);
}

TEST(TupletNumberEdit, FailedBatchRestoresTextEntryAndAbsence) {
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) /
                                 "src/importexport/mnx/tests/data/project_examples/tupletSimple.mnx");
    ASSERT_TRUE(opened) << opened.error().message;
    auto &document = **opened;
    ASSERT_TRUE(document.page_count());
    auto &tuplet = *mu::engraving::to_tuplet(&engrave::harness::element(document, find_id(document, "Tuplet")));
    auto *number = tuplet.number();
    ASSERT_NE(number, nullptr);
    number->set_property(Pid::OFFSET, muse::PointF(0.25, -0.5));
    const auto before = tuplet.authored_overrides;
    const auto unit_policy = tuplet.get_property(Pid::SIZE_SPATIUM_DEPENDENT);
    const auto font_size = tuplet.get_property(Pid::FONT_SIZE);
    const auto font_flags = tuplet.property_flags(Pid::FONT_SIZE);
    const auto text = number->get_property(Pid::TEXT);
    auto result = engrave::api::BatchGuard(document).run(
        {[number](engrave::api::EditContext &) -> engrave::Result<void> {
             number->undo_change_property(Pid::TEXT, muse::String(u"changed"));
             number->undo_change_property(Pid::SIZE_SPATIUM_DEPENDENT, false);
             number->undo_change_property(Pid::FONT_SIZE, 19.25, mu::engraving::PropertyFlags::UNSTYLED);
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after number text"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(number->get_property(Pid::TEXT), text);
    EXPECT_EQ(tuplet.authored_overrides.entries.size(), before.entries.size());
    EXPECT_EQ(tuplet.authored_overrides.carriers.size(), before.carriers.size());
    EXPECT_EQ(tuplet.get_property(Pid::SIZE_SPATIUM_DEPENDENT), unit_policy);
    EXPECT_EQ(tuplet.get_property(Pid::FONT_SIZE), font_size);
    EXPECT_EQ(tuplet.property_flags(Pid::FONT_SIZE), font_flags);
    EXPECT_EQ(number->get_property(Pid::FONT_SIZE), font_size);
    ASSERT_EQ(tuplet.authored_overrides.entries.size(), before.entries.size());
    for (size_t i = 0; i < before.entries.size(); ++i) {
        EXPECT_EQ(tuplet.authored_overrides.entries[i].unit, before.entries[i].unit);
        EXPECT_EQ(tuplet.authored_overrides.entries[i].value, before.entries[i].value);
    }
}

TEST(BeamFragmentEdit, FailedBatchRestoresZeroAbsenceAndCompoundChanges) {
    using namespace mu::engraving;
    auto document = open("layout_elements.mscx");
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->page_count());
    Beam *beam = nullptr;
    engrave::harness::master_of(*document).scan_elements([&](EngravingItem *item) {
        if (item->is_note() && to_note(item)->chord()->beam()) beam = to_note(item)->chord()->beam();
    });
    ASSERT_NE(beam, nullptr);
    const auto attempt = [&](bool compound) {
        const auto before = beam->authored_overrides;
        const auto generated = beam->generated();
        const auto no_slope = beam->no_slope();
        auto result = engrave::api::BatchGuard(*document).run(
            {[beam, compound](engrave::api::EditContext &) -> engrave::Result<void> {
                 beam->undo_change_property(Pid::BEAM_POS, PropertyValue::from_value(muse::PairF(0, 0)));
                 EXPECT_TRUE(beam->user_modified());
                 if (compound) {
                     beam->undo_change_property(Pid::BEAM_NO_SLOPE, true, PropertyFlags::UNSTYLED);
                     beam->undo_change_property(Pid::USER_MODIFIED, false);
                 }
                 return {};
             },
             [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after beam override"); }},
            engrave::api::Policy::TRANSACTION, std::nullopt);
        EXPECT_FALSE(result);
        EXPECT_EQ(beam->generated(), generated);
        EXPECT_EQ(beam->no_slope(), no_slope);
        ASSERT_EQ(beam->authored_overrides.entries.size(), before.entries.size());
        for (size_t i = 0; i < before.entries.size(); ++i) EXPECT_EQ(beam->authored_overrides.entries[i].value, before.entries[i].value);
    };
    attempt(false);
    beam->set_beam_pos(muse::PairF(1.25, 2.5));
    attempt(true);
}

TEST(TremoloOverrideEdit, FirstZeroPinRollsBackToAbsence) {
    using namespace mu::engraving;
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) /
                                 "src/engraving/tests/playback/playbackeventsrenderer_data/two_chords_tremolo/two_chords_tremolo.mscx");
    ASSERT_TRUE(opened) << opened.error().message;
    auto &document = **opened;
    TremoloTwoChord *tremolo = nullptr;
    engrave::harness::master_of(document).scan_elements([&](EngravingItem *item) {
        if (item->type() == ElementType::TREMOLO_TWOCHORD) tremolo = static_cast<TremoloTwoChord *>(item);
    });
    ASSERT_NE(tremolo, nullptr);
    const auto before = tremolo->authored_overrides.entries.size();
    auto result = engrave::api::BatchGuard(document).run(
        {[tremolo](engrave::api::EditContext &) -> engrave::Result<void> {
             tremolo->undo_change_property(Pid::BEAM_POS, PropertyValue::from_value(muse::PairF(0, 0)));
             EXPECT_TRUE(tremolo->user_modified());
             return {};
         },
         [](engrave::api::EditContext &) -> engrave::Result<void> { throw std::runtime_error("after zero tremolo pin"); }},
        engrave::api::Policy::TRANSACTION, std::nullopt);
    EXPECT_FALSE(result);
    EXPECT_EQ(tremolo->authored_overrides.entries.size(), before);
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
    const std::string current = dump_json(engrave::harness::to_json(rows), 2) + "\n";
    if (g_update_evidence) {
        std::ofstream(g_evidence, std::ios::binary) << current;
        return;
    }
    const auto checked_in = parse_json<engrave::harness::Json>(read_text(g_evidence));
    ASSERT_TRUE(checked_in) << g_evidence.string() << " is not JSON";
    const auto now = parse_json<engrave::harness::Json>(current);
    ASSERT_TRUE(now);
    EXPECT_TRUE(json_equal(now->at("verdicts"), checked_in->at("verdicts")));
    const auto &now_cases = now->at("cases").get_array();
    const auto &old_cases = checked_in->at("cases").get_array();
    for (const auto &row : now_cases) {
        const auto found = std::ranges::find_if(old_cases, [&row](const auto &old) { return json_equal(old.at("case"), row.at("case")); });
        if (found == old_cases.end())
            ADD_FAILURE() << dump_json(row.at("case")) << " has no checked-in row";
        else
            EXPECT_TRUE(json_equal(row, *found)) << "actual: " << dump_json(row) << "\nexpected: " << dump_json(*found);
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
