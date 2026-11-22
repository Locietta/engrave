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
#include "engrave/tests/core_test_env.h"
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
#include "engraving/editing/editstaffbrackets.h"
#include "engraving/editing/editstyle.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/style/style.h"
#include "global/io/buffer.h"

namespace engrave::api {
extern void (*after_batch_step_for_test)(std::size_t);
} // namespace engrave::api

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

auto &g_corpus = engrave::test::core_env().golden_corpus;
auto &g_engine = engrave::test::core_env().engine;

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

constexpr std::string_view fixture_name = "chordline_path.mscx";

std::string native_source(engrave::Document &document) {
    return music::export_native_score(engrave::DocumentAccess::context(document), engrave::DocumentAccess::project(document), "mscx",
                                      music::ExportOptions{.standalone_score = true, .transaction_snapshot = true}, {});
}

TEST(Checkpoint, RestoresSourceAndIdsBeforeNativeSave) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto before = native_source(*document);
    std::vector<ElementId> ids;
    for (const auto &entry : engrave::live_ids(engrave::harness::master_of(*document))) ids.push_back(entry.id);
    ASSERT_TRUE(document->checkpoint("before"));
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE(document->edit({{engrave::InsertMeasures{outline->measures.front().id, 2}}}));
    ASSERT_TRUE(document->restore_checkpoint("before"));
    EXPECT_EQ(native_source(*document), before);
    std::vector<ElementId> restored_ids;
    for (const auto &entry : engrave::live_ids(engrave::harness::master_of(*document))) restored_ids.push_back(entry.id);
    EXPECT_EQ(restored_ids, ids);
    for (const auto *score : engrave::harness::master_of(*document).score_list())
        EXPECT_EQ(score->layout_state.whole_count + score->layout_state.range_count, 0u);
    auto saved = document->export_score({.format = engrave::Format::MSCZ});
    ASSERT_TRUE(saved) << saved.error().message;
    auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto restored_outline = (*reopened)->outline();
    ASSERT_TRUE(restored_outline);
    EXPECT_EQ(restored_outline->measures.size(), outline->measures.size());
}

TEST(Checkpoint, RestoresGoldenSourceBytesExactly) {
    for (const auto &path : g_corpus) {
        SCOPED_TRACE(path.filename().string());
        auto opened = g_engine->open(path);
        ASSERT_TRUE(opened) << opened.error().message;
        auto &document = **opened;
        const auto before = native_source(document);
        ASSERT_TRUE(document.checkpoint("source"));
        ASSERT_TRUE(document.restore_checkpoint("source"));
        const auto after = native_source(document);
        if (before != after) {
            const auto output = std::filesystem::path(".runtime/checkpoint-diffs") / path.filename();
            std::filesystem::create_directories(output);
            std::ofstream(output / "before.mscx", std::ios::binary) << before;
            std::ofstream(output / "after.mscx", std::ios::binary) << after;
        }
        EXPECT_TRUE(before == after);
    }
}

TEST(Checkpoint, LimitRefusalKeepsExistingCheckpoint) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto before = native_source(*document);
    ASSERT_TRUE(document->checkpoint("before"));
    auto &state = engrave::DocumentAccess::impl(*document);
    state.checkpoint_byte_limit = 1;
    const auto rejected = document->checkpoint("before");
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().code, ErrorCode::RESOURCE_LIMIT);
    const auto note = find_id(*document, "Note");
    ASSERT_TRUE(document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}));
    ASSERT_TRUE(document->restore_checkpoint("before"));
    EXPECT_EQ(native_source(*document), before);
}

TEST(Checkpoint, RestoresRuntimeTempoForPlayback) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    mu::engraving::TempoTimeline timeline;
    timeline.rebuild(mu::engraving::TempoValues{{0, 3.0}}, {});
    timeline.set_tempo_multiplier(mu::engraving::BeatsPerSecond(1.5));
    master.set_tempo_timeline_override(timeline);
    ASSERT_TRUE(document->checkpoint("tempo"));
    master.set_tempo_timeline_override(std::nullopt);
    ASSERT_TRUE(document->restore_checkpoint("tempo"));
    // Four quarter notes at 4.5 beats per second last eight ninths of a second.
    EXPECT_DOUBLE_EQ(engrave::harness::master_of(*document).tempo_timeline().utick2utime(1920), 8.0 / 9.0);
}

TEST(Checkpoint, InternalRestoreRecoversRevisionWithoutNamedStorage) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto &state = engrave::DocumentAccess::impl(*document);
    state.checkpoint_byte_limit = 0;
    const auto before = native_source(*document);
    const auto revision = document->revision();
    const auto snapshot = state.capture_snapshot();
    const auto note = find_id(*document, "Note");
    ASSERT_TRUE(document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}));
    ASSERT_GT(document->revision(), revision);
    ASSERT_TRUE(state.restore_snapshot(snapshot, false));
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(native_source(*document), before);
    const auto missing = document->restore_checkpoint("before");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::NOT_FOUND);
}

TEST(Checkpoint, PreservesSubToleranceAuthoredOffset) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto note = find_id(*document, "Note");
    ASSERT_TRUE(document->edit({{SetProperty{note, "offset", engrave::Point{0.00005, -0.00003}}}}));
    const auto before = native_source(*document);
    ASSERT_TRUE(document->checkpoint("small offset"));
    ASSERT_TRUE(document->edit({{SetProperty{note, "offset", engrave::Point{2.0, 3.0}}}}));
    ASSERT_TRUE(document->restore_checkpoint("small offset"));
    EXPECT_EQ(std::get<engrave::Point>(property(*document, note, "offset")), (engrave::Point{0.00005, -0.00003}));
    EXPECT_EQ(native_source(*document), before);
}

TEST(Checkpoint, RestoresSnapshotAbovePublicImportLimit) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    std::string payload(16 * 1024 * 1024, ' ');
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::uint32_t random = 0x9e3779b9;
    for (char &value : payload) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        value = alphabet[random & 63];
    }
    auto &master = engrave::harness::master_of(*document);
    master.meta_tags()[u"large metadata"] = muse::String::from_std_string(payload);
    ASSERT_TRUE(document->checkpoint("large"));
    ASSERT_GT(engrave::DocumentAccess::impl(*document).checkpoints.front().snapshot.bytes.size(), 8u * 1024 * 1024);
    master.meta_tags()[u"large metadata"] = u"changed";
    ASSERT_TRUE(document->restore_checkpoint("large"));
    EXPECT_TRUE(engrave::harness::master_of(*document).meta_tags().at(u"large metadata").to_std_string() == payload);
}

TEST(Edit, RepeatedKeySignatureKeepsRevisionAndSource) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    const auto first = outline->measures.front().id;
    ASSERT_TRUE(document->edit({{engrave::SetKeySig{first, 2, {}}}}));
    const auto before = native_source(*document);
    const auto revision = document->revision();
    ASSERT_TRUE(document->edit({{engrave::SetKeySig{first, 2, {}}}}));
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(native_source(*document), before);
}

TEST(Edit, TupletAcrossNewBarRejectsAndCommitsPrefix) {
    auto document = open("hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *measure = master.first_measure()->next_measure();
    ASSERT_NE(measure, nullptr);
    const auto tick = measure->tick();
    auto *chord = master.find_cr(tick, 0);
    ASSERT_NE(chord, nullptr);
    const auto chord_id = engrave::from_engine(chord->eid());
    ASSERT_TRUE(document->edit({{engrave::AddTuplet{chord_id, chord_id, 3, 2}}}));
    chord = master.find_cr(tick, 0);
    auto *start = master.find_cr(master.first_measure()->tick(), 0);
    ASSERT_NE(start, nullptr);
    ASSERT_NE(chord, nullptr);
    const auto added =
        document->edit({{engrave::AddSpanner{"Slur", engrave::from_engine(start->eid()), engrave::from_engine(chord->eid()), {}}}});
    ASSERT_TRUE(added) << added.error().message;
    const auto slur_id = added->ops.front().created.front();
    ASSERT_TRUE(slur_id);
    ASSERT_TRUE(document->edit({{engrave::GenerateExcerpts{{find_id(*document, "Part")}}}}));
    const auto measure_id = engrave::from_engine(measure->eid());
    const auto note = find_id(*document, "Note");
    ASSERT_TRUE(document->checkpoint("prefix"));
    ASSERT_TRUE(document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}));
    const auto prefix = native_source(*document);
    ASSERT_TRUE(document->restore_checkpoint("prefix"));
    const auto result = document->edit({{SetProperty{note, "color", Color{255, 0, 0}}, engrave::SetTimeSig{measure_id, {3, 16}},
                                         SetProperty{note, "color", Color{0, 0, 255}}}});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_TRUE(result->rejection);
    EXPECT_EQ(result->applied, 1u);
    EXPECT_EQ(result->rejection->op_index, 1u);
    EXPECT_EQ(result->rejection->error.code, ErrorCode::EDIT_REJECTED);
    EXPECT_FALSE(result->rejection->error.poisoned);
    EXPECT_EQ(native_source(*document), prefix);
    EXPECT_TRUE(document->page_count());
    ASSERT_TRUE(document->edit({{SetProperty{note, "color", Color{0, 255, 0}}}}));
}

TEST(Edit, RemovingTimeSigRejectsTupletAcrossRestoredBarBoundary) {
    for (bool at_end : {false, true}) {
        auto document = open("hairpins_and_repeats.mscx");
        ASSERT_TRUE(document);
        auto &master = engrave::harness::master_of(*document);
        ASSERT_TRUE(document->edit({{engrave::SetTimeSig{engrave::from_engine(master.first_measure()->eid()), {3, 16}}}}));
        auto *measure = master.first_measure()->next_measure()->next_measure();
        ASSERT_NE(measure, nullptr);
        const auto changed = document->edit({{engrave::SetTimeSig{engrave::from_engine(measure->eid()), {4, 4}}}});
        ASSERT_TRUE(changed);
        ASSERT_FALSE(changed->rejection);
        measure = master.first_measure()->next_measure()->next_measure();
        const engrave::SegmentAddress at{
            engrave::from_engine(measure->eid()), 0, 0, {measure->tick().numerator(), measure->tick().denominator()}, "ChordRest"};
        const auto entered = document->edit({{engrave::EnterNotes{at, {1, 2}, {{60}}}}});
        ASSERT_TRUE(entered) << entered.error().message;
        ASSERT_FALSE(entered->rejection);
        auto *chord = master.find_cr(measure->tick(), 0);
        ASSERT_NE(chord, nullptr);
        const auto chord_id = engrave::from_engine(chord->eid());
        const auto tuplet = document->edit({{engrave::AddTuplet{chord_id, chord_id, 3, 2}}});
        ASSERT_TRUE(tuplet);
        ASSERT_FALSE(tuplet->rejection);
        auto *signature = measure->first(mu::engraving::SegmentType::TIME_SIG)->element(0);
        if (at_end) {
            master.start_cmd(muse::TranslatableString::untranslatable("end-of-measure fixture"));
            auto *previous = measure->prev_measure();
            master.apply_segment_parent_change(mu::engraving::to_time_sig(signature)->segment(), previous, previous->ticks());
            master.end_cmd();
        }
        const auto signature_id = engrave::from_engine(signature->eid());
        const auto note = find_id(*document, "Note");
        ASSERT_TRUE(document->checkpoint("prefix"));
        ASSERT_TRUE(document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}));
        const auto prefix = native_source(*document);
        ASSERT_TRUE(document->restore_checkpoint("prefix"));
        const auto result = document->edit({{SetProperty{note, "color", Color{255, 0, 0}}, engrave::Remove{signature_id}}});
        ASSERT_TRUE(result) << result.error().message;
        ASSERT_TRUE(result->rejection);
        EXPECT_EQ(result->applied, 1u);
        EXPECT_EQ(result->rejection->op_index, 1u);
        EXPECT_EQ(result->rejection->error.code, ErrorCode::EDIT_REJECTED);
        EXPECT_FALSE(result->rejection->error.poisoned);
        EXPECT_EQ(native_source(*document), prefix);
        ASSERT_TRUE(document->edit({{SetProperty{note, "color", Color{0, 255, 0}}}}));
    }
}

TEST(Edit, NonrepresentableRewritePoisonsAndCheckpointRestoresExactBytes) {
    auto document = open("hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = engrave::harness::master_of(*document);
    auto *measure = master.first_measure()->next_measure();
    const auto measure_id = engrave::from_engine(measure->eid());
    auto *start = master.find_cr(master.first_measure()->tick(), 0);
    auto *end = master.find_cr(measure->tick(), 0);
    ASSERT_TRUE(start && end);
    const auto added =
        document->edit({{engrave::AddSpanner{"Slur", engrave::from_engine(start->eid()), engrave::from_engine(end->eid()), {}}}});
    ASSERT_TRUE(added);
    const auto slur_id = added->ops.front().created.front();
    ASSERT_TRUE(slur_id);
    const auto note = find_id(*document, "Note");
    const auto before = native_source(*document);
    ASSERT_TRUE(document->checkpoint("before"));
    const auto failed = document->edit({{SetProperty{note, "color", Color{255, 0, 0}}, engrave::SetTimeSig{measure_id, {1, 5}}}});
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().poisoned);
    EXPECT_EQ(failed.error().op_index, 1u);
    EXPECT_EQ(failed.error().applied_ops, (std::vector<std::size_t>{0}));
    const auto blocked = document->edit({});
    ASSERT_FALSE(blocked);
    EXPECT_EQ(blocked.error().code, ErrorCode::POISONED);
    const auto save = document->export_score({.format = engrave::Format::MSCZ});
    ASSERT_FALSE(save);
    EXPECT_EQ(save.error().code, ErrorCode::POISONED);
    const auto surviving = engrave::resolve(master, *slur_id);
    ASSERT_TRUE(surviving);
    EXPECT_EQ(mu::engraving::to_spanner(surviving->object)->end_element(), nullptr);
    EXPECT_TRUE(document->query({.types = {"Slur"}}));
    EXPECT_TRUE(document->page_count());
    ASSERT_TRUE(document->restore_checkpoint("before"));
    EXPECT_EQ(native_source(*document), before);
    ASSERT_TRUE(document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}));
}

TEST(Edit, StaticMeasureErrorsAreRejectedBeforeEarlierWrites) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto before = native_source(*document);
    const auto note = find_id(*document, "Note");
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    const auto first = outline->measures.front().id;
    const auto last = outline->measures.back().id;
    for (const engrave::Op bad : {engrave::Op{engrave::InsertMeasures{note, 1}}, engrave::Op{engrave::RemoveMeasures{note, first}},
                                  engrave::Op{engrave::RemoveMeasures{first, note}}, engrave::Op{engrave::RemoveMeasures{last, first}}}) {
        const auto result = document->edit({{SetProperty{note, "color", Color{255, 0, 0}}, bad}});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(result.error().op_index, 1u);
        EXPECT_EQ(native_source(*document), before);
    }
}

TEST(Edit, CommitAdvancesTheRevision) {
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

TEST(Edit, PreflightFailureKeepsNativeBytes) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const ElementId note = find_id(*document, "Note");
    const auto before = native_source(*document);
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
        EXPECT_EQ(native_source(*document), before);
        EXPECT_FALSE(result.error().poisoned);
        EXPECT_TRUE(result.error().applied_ops.empty());
    }
}

TEST(Edit, LaterExcerptTargetIsCheckedBeforeAnyExcerptIsCreated) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto part = find_id(*document, "Part");
    const auto note = find_id(*document, "Note");
    const auto before = native_source(*document);
    const auto result = document->edit({{engrave::GenerateExcerpts{{part, note}}}});
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->rejection);
    EXPECT_EQ(result->applied, 0u);
    EXPECT_EQ(result->rejection->op_index, 0u);
    EXPECT_EQ(result->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(native_source(*document), before);
    EXPECT_TRUE(document->edit({{SetProperty{note, "color", Color{255, 0, 0}}}}));
}

TEST(Edit, MultinoteEntryAcrossBarlineLeavesNoPartialChord) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    auto *measure = engrave::harness::master_of(*document).first_measure();
    const engrave::SegmentAddress at{engrave::from_engine(measure->eid()), 0, 0, {0, 1}, "ChordRest"};
    const auto before = native_source(*document);
    const auto result = document->edit({{engrave::EnterNotes{at, {4, 1}, {{60}, {64}}}}});
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->rejection);
    EXPECT_EQ(result->applied, 0u);
    EXPECT_EQ(result->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(native_source(*document), before);
}

TEST(Edit, NoteEntryRejectsAStaffRemovedByThePrefix) {
    auto made = g_engine->create_document({.template_id = "Treble_Clef"});
    ASSERT_TRUE(made);
    auto &document = **made;
    auto &master = engrave::harness::master_of(document);
    const auto part = engrave::from_engine(master.parts().front()->eid());
    ASSERT_TRUE(document.edit({{engrave::AddStaff{part}}}));
    const auto staff = engrave::from_engine(master.staff(1)->eid());
    const engrave::SegmentAddress at{engrave::from_engine(master.first_measure()->eid()), 1, 0, {0, 1}, "ChordRest"};
    const auto result = document.edit({{engrave::RemoveStaff{staff}, engrave::EnterNotes{at, {1, 4}, {{60}}}}});
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->rejection);
    EXPECT_EQ(result->applied, 1u);
    EXPECT_EQ(result->rejection->op_index, 1u);
    EXPECT_EQ(result->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(master.nstaves(), 1u);
    EXPECT_FALSE(engrave::resolve(master, staff));
    EXPECT_TRUE(master.first_measure()->is_full_measure_rest());
    EXPECT_TRUE(document.edit({}));
}

TEST(Edit, SplitNotationRejectsBeforeReplacingTheMeasureRest) {
    auto made = g_engine->create_document({.template_id = "Treble_Clef"});
    ASSERT_TRUE(made);
    auto &document = **made;
    auto &master = engrave::harness::master_of(document);
    const auto rest = engrave::from_engine(master.find_cr({0, 1}, 0)->eid());
    const engrave::SegmentAddress at{engrave::from_engine(master.first_measure()->eid()), 0, 0, {0, 1}, "ChordRest"};
    const auto result = document.edit({{SetProperty{rest, "color", Color{255, 0, 0}}, engrave::EnterNotes{at, {5, 16}, {{60}, {64}}}}});
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->rejection);
    EXPECT_EQ(result->applied, 1u);
    EXPECT_EQ(result->rejection->op_index, 1u);
    EXPECT_EQ(result->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_TRUE(master.first_measure()->is_full_measure_rest());
    EXPECT_EQ(property(document, rest, "color"), (engrave::Value{Color{255, 0, 0}}));
    EXPECT_TRUE(document.edit({}));
}

TEST(Edit, MultinoteEntryUsesTheTupletsActualDurationAtTheBarline) {
    auto made = g_engine->create_document({.template_id = "Treble_Clef"});
    ASSERT_TRUE(made);
    auto &document = **made;
    auto &master = engrave::harness::master_of(document);
    const auto measure = engrave::from_engine(master.first_measure()->eid());
    const engrave::SegmentAddress at{measure, 0, 0, {0, 1}, "ChordRest"};
    ASSERT_TRUE(document.edit({{engrave::EnterNotes{at, {3, 4}, {{60}}}}}));
    const auto rest = engrave::from_engine(master.find_cr({3, 4}, 0)->eid());
    ASSERT_TRUE(document.edit({{engrave::AddTuplet{rest, rest, 3, 2}}}));
    const engrave::SegmentAddress inside{measure, 0, 0, {5, 6}, "ChordRest"};
    const auto result = document.edit({{engrave::EnterNotes{inside, {1, 4}, {{60}, {64}}}}});
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_FALSE(result->rejection);
    const auto *entered = master.find_cr({5, 6}, 0);
    ASSERT_TRUE(entered && entered->is_chord());
    EXPECT_EQ(entered->ticks(), (mu::engraving::Fraction{1, 4}));
    EXPECT_EQ(entered->actual_ticks(), (mu::engraving::Fraction{1, 6}));
    std::vector<int> pitches;
    for (const auto *note : mu::engraving::to_chord(entered)->notes()) {
        pitches.push_back(note->pitch());
        EXPECT_EQ(note->tie_for(), nullptr);
    }
    EXPECT_EQ(pitches, (std::vector<int>{60, 64}));
}

TEST(Edit, RejectionCommitsOnlyTheCompletedPrefix) {
    auto document = open(fixture_name);
    ASSERT_TRUE(document);
    const auto note = find_id(*document, "Note");
    const auto measures = document->outline()->measures.size();
    const auto result =
        document->edit({{engrave::InsertMeasures{{}, 1}, engrave::Remove{note}, engrave::Remove{note}, engrave::InsertMeasures{{}, 2}}});
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result->applied, 2u);
    ASSERT_TRUE(result->rejection);
    EXPECT_EQ(result->rejection->op_index, 2u);
    EXPECT_EQ(result->rejection->error.code, ErrorCode::STALE_HANDLE);
    EXPECT_FALSE(result->rejection->error.poisoned);
    EXPECT_EQ(document->revision(), 2u);
    EXPECT_EQ(document->outline()->measures.size(), measures + 1);
    EXPECT_FALSE(engrave::resolve(engrave::harness::master_of(*document), note));
    ASSERT_EQ(result->ops.size(), 2u);
    ASSERT_FALSE(result->ops[0].created.empty());
    ASSERT_TRUE(result->ops[0].created[0]);
    const auto inserted = engrave::from_engine(engrave::harness::master_of(*document).last_measure()->eid());
    EXPECT_EQ(*result->ops[0].created[0], inserted);
    EXPECT_NE(std::ranges::find(result->created, inserted), result->created.end());
    const auto next = find_id(*document, "Note");
    ASSERT_TRUE(document->edit({{SetProperty{next, "color", Color{255, 0, 0}}}}));
    EXPECT_EQ(property(*document, next, "color"), (engrave::Value{Color{255, 0, 0}}));
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
    return engrave::api::BatchGuard(document).run(std::move(steps));
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

TEST(ChordLineEdit, StyleScalePreservesAuthoredPaths) {
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

TEST(Operations, CommittedCasesKeepTheirBehaviorAndNativeInterop) {
    for (const auto &op_case : engrave::harness::registered_cases()) {
        SCOPED_TRACE(op_case.name);
        const auto path = fixture(op_case.fixture);
        ASSERT_FALSE(path.empty());
        EXPECT_EQ(engrave::harness::run_case(*g_engine, op_case, path), "");
    }
}

// --- corpus -----------------------------------------------------------------------

} // namespace
