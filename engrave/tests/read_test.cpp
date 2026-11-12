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
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/score/json.h"
#include "engrave/src/score/score_metadata.h"
#include "engrave/src/score/score_positions.h"
#include "engrave/src/score/score_semantic.h"
#include "engrave/src/score/write470.h"
#include "engraving/automation/automationdata.h"
#include "engraving/automation/tempovalues.h"
#include "engraving/dom/accidental.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/groups.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/noteline.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/property.h"
#include "engraving/dom/rangelock.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spannermap.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/system.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tiejumppointlist.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/trill.h"
#include "engraving/editing/addremoveelement.h"
#include "engraving/editing/editproperty.h"
#include "engraving/editing/flip.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/infrastructure/eidregister.h"
#include "engraving/normalization/normalize.h"
#include "engraving/normalization/notes.h"
#include "engraving/playback/flatplaybackscore.h"
#include "engraving/playback/metaparsers/notearticulationsparser.h"
#include "engraving/rendering/score/chordlayout.h"
#include "engraving/rendering/score/layoutcontext.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "engraving/rendering/score/systemlayout.h"
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
    if (document) EXPECT_TRUE((*document)->page_count());
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

struct ItemLayoutSpy {
    static inline std::size_t count = 0;

    ItemLayoutSpy() {
        count = 0;
        mu::engraving::rendering::score::ScoreRenderer::layout_hook = [](const auto *) { ++count; };
    }

    ~ItemLayoutSpy() { mu::engraving::rendering::score::ScoreRenderer::layout_hook = nullptr; }
};

TEST(Read, PlaybackNoteheadUsesSourceSchemeWithoutDisplayData) {
    using namespace mu::engraving;
    using namespace muse::mpe;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    note->set_head_scheme(NoteHeadScheme::HEAD_SHAPE_NOTE_7_AIKIN);
    auto profile = std::make_shared<ArticulationsProfile>();
    ArticulationPattern pattern;
    pattern.emplace(0, ArticulationPatternSegment(ArrangementPattern(hundred_percent, 0),
                                                  PitchPattern(ArticulationMap::expected_size, ten_percent, 0),
                                                  ExpressionPattern(ArticulationMap::expected_size, ten_percent, 0)));
    profile->set_pattern(ArticulationType::TRIANGLE_UP_NOTE, pattern);
    RenderingContext context{.profile = profile};
    ArticulationMap fresh;
    NoteArticulationsParser::parse_note_head(note, context, fresh);
    EXPECT_TRUE(fresh.contains(ArticulationType::TRIANGLE_UP_NOTE));
    EXPECT_FALSE(note->has_layout_data());
    note->mutldata()->cached_notehead_sym.set_value(SymId::NOTEHEAD_X_BLACK);
    ArticulationMap stale;
    NoteArticulationsParser::parse_note_head(note, context, stale);
    EXPECT_TRUE(stale.contains(ArticulationType::TRIANGLE_UP_NOTE));
    EXPECT_FALSE(stale.contains(ArticulationType::CROSS_NOTE));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, HairpinEndpointUsesSourceVoiceAndPlacementBeforeLayout) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    std::unique_ptr<Hairpin> hairpin(Factory::create_hairpin(master.dummy()));
    hairpin->set_track(0);
    hairpin->set_tick(Fraction(0, 1));
    hairpin->set_ticks(Fraction(0, 1));
    hairpin->set_placement(PlacementV::BELOW);
    auto *dynamic = Factory::create_dynamic(segment);
    dynamic->set_track(0);
    dynamic->set_placement(PlacementV::BELOW);
    dynamic->set_dynamic_type(DynamicType::F);
    segment->add(dynamic);
    EXPECT_EQ(hairpin->find_end_dynamic_or_expression(PlacementV::BELOW, false, true), dynamic);
    EXPECT_EQ(hairpin->find_end_dynamic_or_expression(PlacementV::ABOVE, false, true), nullptr);
    EXPECT_FALSE(dynamic->has_layout_data());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, OutlineCountsAreAbsentUntilCurrentAndMetadataIgnoresDisplayPositions) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    for (const char *key : {"composer", "workTitle"}) {
        const auto begin = source.find(std::string("<metaTag name=\"") + key + "\">");
        source.erase(begin, source.find("</metaTag>", begin) + std::string("</metaTag>").size() - begin);
    }
    source.insert(source.rfind("<Staff id=\"1\">") + std::string("<Staff id=\"1\">").size(),
                  "<VBox>"
                  "<Text><style>Frame</style><fontSize>20</fontSize><text>First title</text></Text>"
                  "<Text><style>Frame</style><fontSize>20</fontSize><text>Second title</text></Text>"
                  "<Text><style>Frame</style><align>right,top</align><text>Source composer</text></Text>"
                  "</VBox>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    ItemLayoutSpy spy;
    auto initial = document.outline();
    ASSERT_TRUE(initial);
    EXPECT_EQ(initial->title, "First title");
    EXPECT_EQ(initial->composer, "Source composer");
    EXPECT_FALSE(initial->pages.has_value());
    int index = 0;
    for (auto *item : master.first()->el()) {
        item->set_pos(PointF(10000 - index * 1000, -1000 * index));
        ++index;
    }
    auto moved = document.outline();
    ASSERT_TRUE(moved);
    EXPECT_EQ(moved->title, initial->title);
    EXPECT_EQ(moved->composer, initial->composer);
    EXPECT_EQ(ItemLayoutSpy::count, 0u);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    auto count = document.page_count();
    ASSERT_TRUE(count);
    auto current = document.outline();
    ASSERT_TRUE(current);
    EXPECT_EQ(current->pages, *count);
    EXPECT_EQ(current->title, initial->title);
    const auto notes = document.query({.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(notes);
    ASSERT_FALSE(notes->elements.empty());
    ASSERT_TRUE(document.edit({{engrave::SetPitch{notes->elements.front().id, 67}}}));
    auto dirty = document.outline();
    ASSERT_TRUE(dirty);
    EXPECT_FALSE(dirty->pages.has_value());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
}

TEST(Read, GeometryAddressesAndFingeringDefaultsExplicitlyPrepareLayout) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    const auto at = source.find("</Measure>");
    source.insert(at, "<voice><Chord><durationType>whole</durationType><Note><pitch>48</pitch><tpc>14</tpc>"
                      "<Fingering><text>1</text></Fingering></Note></Chord></voice>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto fingers = document.query({.types = {"Fingering"}, .props = {"text"}});
    ASSERT_TRUE(fingers);
    ASSERT_EQ(fingers->elements.size(), 1u);
    const auto id = fingers->elements.front().id;
    ASSERT_TRUE(document.get(id, {"text"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    auto placement = document.get(id, {"placement"});
    ASSERT_TRUE(placement);
    ASSERT_EQ(placement->size(), 1u);
    EXPECT_TRUE(placement->front().is_default);
    const auto resolved = engrave::resolve(master, id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(to_fingering(resolved->object)->placement(), PlacementV::BELOW);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
    ASSERT_TRUE(document.get(engrave::Address{engrave::PageAddress{}}, {"visible"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
    const auto notes = document.query({.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(notes);
    ASSERT_TRUE(document.edit({{engrave::SetPitch{notes->elements.front().id, 67}}}));
    ASSERT_TRUE(document.get(engrave::Address{engrave::SystemAddress{}}, {"visible"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 2u);
    master.invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    auto query = document.query({.types = {"Fingering"}, .props = {"placement"}});
    ASSERT_TRUE(query);
    ASSERT_EQ(query->elements.size(), 1u);
    EXPECT_TRUE(query->elements.front().props.front().is_default);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 3u);
}

TEST(Read, SpannerSegmentAddressPreparesFreshDisplaySegments) {
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto spanners = master.spanner_list();
    ASSERT_FALSE(spanners.empty());
    const auto id = engrave::from_engine(spanners.front()->eid());
    ASSERT_TRUE(document.get(engrave::Address{engrave::SpannerSegmentAddress{id, 0}}, {"visible"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
    master.invalidate_layout(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(-1, 1));
    ASSERT_TRUE(document.get(engrave::Address{engrave::SpannerSegmentAddress{id, 0}}, {"visible"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 2u);
}

TEST(Read, SemanticDocumentBoundariesDeferAllLayoutUntilFirstRender) {
    using namespace mu::engraving;
    ItemLayoutSpy spy;
    auto native = g_engine->open(write470_fixture());
    ASSERT_TRUE(native);
    auto created = g_engine->create_document({.template_id = "Treble_Clef", .title = "deferred"});
    ASSERT_TRUE(created);
    auto mnx = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/importexport/mnx/tests/data/engine/two-notes.mnx");
    ASSERT_TRUE(mnx);
    for (auto *document : {native->get(), created->get(), mnx->get()}) {
        auto &master = master_of(*document);
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        EXPECT_TRUE(master.layout_state.dirty);
        EXPECT_TRUE(master.normalize_state.includes_hidden_staves);
        EXPECT_FALSE(master.normalize_state.dirty);
        ASSERT_TRUE(document->outline());
        ASSERT_TRUE(document->query({.types = {"Note"}}));
    }
    auto &document = **native;
    auto &master = master_of(document);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const auto id = engrave::from_engine(note->eid());
    EXPECT_FALSE(note->has_layout_data());
    for (int pitch : {62, 64, 65}) {
        ASSERT_TRUE(document.edit({{engrave::SetPitch{id, pitch}}}));
        ASSERT_TRUE(document.query({.types = {"Note"}}));
        ASSERT_TRUE(document.get(id, {"pitch"}));
        EXPECT_FALSE(note->has_layout_data());
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    }
    const auto outline = document.outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
    for (auto *score : master.score_list()) EXPECT_EQ(score->layout_state.whole_count + score->layout_state.range_count, 0u);
    EXPECT_EQ(ItemLayoutSpy::count, 0u);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
    ASSERT_TRUE(document.page_count());
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
}

TEST(Read, UndoRedoReplaysSourceWithoutLayout) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const int line = note->line();
    engrave::DocumentAccess::edit_hooks(document).keep_history = true;
    ASSERT_TRUE(document.edit({{engrave::SetPitch{engrave::from_engine(note->eid()), note->pitch() + 12}}}));
    master.undo_redo(true, nullptr);
    EXPECT_EQ(note->line(), line);
    master.undo_redo(false, nullptr);
    EXPECT_EQ(note->line(), line - 7);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    EXPECT_FALSE(master.normalize_state.dirty);
    EXPECT_FALSE(note->has_layout_data());
}

TEST(Read, TabTieAndBendFrettingIsFinalBeforeFirstLayout) {
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/tab_grace_prebend.mscx");
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    std::vector<std::tuple<mu::engraving::Note *, int, int>> fretting;
    for (auto *object : engrave::document_objects(master)) {
        if (object->is_note()) {
            auto *note = mu::engraving::to_note(object);
            fretting.emplace_back(note, note->fret(), note->string());
        }
    }
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    ASSERT_TRUE((*opened)->page_count());
    for (auto [note, fret, string] : fretting) {
        EXPECT_EQ(note->fret(), fret);
        EXPECT_EQ(note->string(), string);
    }
}

TEST(Read, HiddenStaffEditPublishesNormalizedLinesWithoutLayout) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const int line = note->line();
    const auto id = engrave::from_engine(note->eid());
    master.parts().front()->set_show(false);
    note->set_line(invalid_line);
    master.set_layout_all();
    ASSERT_TRUE(document.edit({{engrave::SetPitch{id, note->pitch() + 12}}}));
    EXPECT_EQ(note->line(), line - 7);
    EXPECT_TRUE(master.normalize_state.includes_hidden_staves);
    EXPECT_FALSE(master.normalize_state.dirty);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    EXPECT_FALSE(note->has_layout_data());
}

TEST(Read, ProductionBatchBarrierPublishesLinesBeforeTheNextStep) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const auto id = engrave::from_engine(note->eid());
    const int initial = note->line();
    bool checked = false;
    auto &hooks = engrave::DocumentAccess::edit_hooks(document);
    hooks.before_step = [&](std::size_t step) {
        if (step != 2) return;
        EXPECT_EQ(note->line(), initial - 7);
        EXPECT_FALSE(master.normalize_state.dirty);
        checked = true;
    };
    ASSERT_TRUE(document.edit({{engrave::SetPitch{id, note->pitch() + 12}, engrave::SetProperty{id, "visible", true}}}));
    hooks = {};
    EXPECT_TRUE(checked);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, DeferredEditSequenceMatchesEagerAndBatchedOutput) {
    std::vector<std::vector<std::byte>> expected_pages;
    std::string expected_semantics;
    for (int lane = 0; lane < 3; ++lane) {
        auto opened = g_engine->open(write470_fixture());
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *note =
            mu::engraving::to_chord(master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST)->element(0))->notes().front();
        const auto id = engrave::from_engine(note->eid());
        std::vector<engrave::Op> ops{engrave::SetPitch{id, 73, 21}, engrave::SetPitch{id, 64, 18}, engrave::SetPitch{id, 65, 13}};
        if (lane == 0) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        if (lane == 2) {
            ASSERT_TRUE(document.edit({ops}));
        } else {
            for (const auto &op : ops) {
                ASSERT_TRUE(document.edit({{op}}));
                if (lane == 0) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            }
        }
        const auto semantics = music::dump_json(music::semantic_dump(&master), 1);
        auto rendered = document.render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(rendered);
        if (lane == 0) {
            expected_pages = rendered->pages;
            expected_semantics = semantics;
        } else {
            EXPECT_EQ(semantics, expected_semantics);
            EXPECT_EQ(rendered->pages, expected_pages);
            EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
        }
    }
}

TEST(Read, BeamDetachmentDoesNotRenderItemsDuringEdit) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    const auto begin = source.find("<Chord>");
    const auto end = source.find("</Chord>", begin) + std::string("</Chord>").size();
    source.replace(begin, end - begin,
                   "<Chord><durationType>eighth</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>"
                   "<Chord><durationType>eighth</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>"
                   "<Rest><durationType>half</durationType><dots>1</dots></Rest>");
    auto document = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(document);
    ASSERT_TRUE((*document)->page_count());
    auto &master = master_of(**document);
    ChordRest *beamed = nullptr;
    for (Measure *m = master.first_measure(); m && !beamed; m = m->next_measure()) {
        for (const Segment &segment : m->segments()) {
            for (auto *item : segment.elist()) {
                if (item && item->is_chord_rest() && to_chord_rest(item)->beam()) {
                    beamed = to_chord_rest(item);
                    break;
                }
            }
            if (beamed) break;
        }
    }
    ASSERT_NE(beamed, nullptr);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    ItemLayoutSpy spy;
    master.start_cmd(muse::TranslatableString::untranslatable("detach beam"));
    beamed->remove_delete_beam(false);
    master.end_cmd(false, true);
    EXPECT_EQ(ItemLayoutSpy::count, 0u);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    EXPECT_TRUE(master.layout_state.dirty);
}

TEST(Read, InferredFlipExplicitlyRequestsGeometry) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    master.select(note, SelectType::SINGLE);
    master.start_cmd(muse::TranslatableString::untranslatable("inferred flip"));
    Flip::flip(master.transaction_manager()->current_or_dummy_transaction(), &master);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
    master.end_cmd(false, true);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
    EXPECT_TRUE(master.layout_state.dirty);
}

TEST(Read, DeletingSnappedItemDisconnectsTheSurvivingItem) {
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *segment = master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST);
    std::unique_ptr<mu::engraving::Dynamic> first(mu::engraving::Factory::create_dynamic(segment));
    auto *second = mu::engraving::Factory::create_dynamic(segment);
    first->mutldata()->connect_item_snapped_after(second);
    delete second;
    EXPECT_EQ(first->ldata()->item_snapped_after(), nullptr);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, ReplacingSnappingPartnersDisconnectsBothOldEnds) {
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *segment = master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST);
    for (bool before : {false, true}) {
        std::unique_ptr<mu::engraving::Dynamic> first(mu::engraving::Factory::create_dynamic(segment));
        std::unique_ptr<mu::engraving::Dynamic> second(mu::engraving::Factory::create_dynamic(segment));
        auto *shared = mu::engraving::Factory::create_dynamic(segment);
        if (before) {
            first->mutldata()->connect_item_snapped_before(shared);
            second->mutldata()->connect_item_snapped_before(shared);
            EXPECT_EQ(first->ldata()->item_snapped_before(), nullptr);
            EXPECT_EQ(shared->ldata()->item_snapped_after(), second.get());
        } else {
            first->mutldata()->connect_item_snapped_after(shared);
            second->mutldata()->connect_item_snapped_after(shared);
            EXPECT_EQ(first->ldata()->item_snapped_after(), nullptr);
            EXPECT_EQ(shared->ldata()->item_snapped_before(), second.get());
        }
        delete shared;
        EXPECT_EQ(second->ldata()->item_snapped_before(), nullptr);
        EXPECT_EQ(second->ldata()->item_snapped_after(), nullptr);
    }
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, FirstLayoutOfGraceArpeggiosMatchesReflow) {
    auto opened =
        g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/importexport/mnx/tests/data/project_examples/graceArps.mnx");
    ASSERT_TRUE(opened);
    auto first = (*opened)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(first);
    master_of(**opened).invalidate_layout(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(-1, 1));
    auto second = (*opened)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(second);
    EXPECT_EQ(first->pages, second->pages);
}

TEST(Read, UnlaidSnapChainStartsAtOwner) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    std::unique_ptr<mu::engraving::Chord> chord(mu::engraving::Factory::create_chord(master.dummy()));
    std::vector<mu::engraving::EngravingItem *> visited;
    chord->scan_connected_items_in_snapping_chain(nullptr, [&](auto *item) { visited.push_back(item); });
    EXPECT_EQ(visited, (std::vector<mu::engraving::EngravingItem *>{chord.get()}));
    EXPECT_FALSE(chord->has_layout_data());
}

TEST(Read, CenteringAcceptsUnlaidDynamic) {
    auto document = open_path(write470_fixture().parent_path() / "four_flutes.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *system = master.first_measure()->system();
    ASSERT_NE(system, nullptr);
    auto *segment = master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST);
    std::unique_ptr<mu::engraving::Dynamic> original(mu::engraving::Factory::create_dynamic(segment));
    original->set_track(mu::engraving::voices);
    auto *dynamic = original->clone();
    segment->add(dynamic);
    ASSERT_FALSE(dynamic->has_layout_data());
    system->staff(1)->set_show(false);
    mu::engraving::rendering::score::SystemLayout::center_elements_between_staves(system);
}

TEST(Read, LayoutResetDoesNotAllocateHiddenItems) {
    auto document = open_path(write470_fixture().parent_path() / "four_flutes.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *segment = master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST);
    std::unique_ptr<mu::engraving::Dynamic> original(mu::engraving::Factory::create_dynamic(segment));
    original->set_track(mu::engraving::voices);
    auto *dynamic = original->clone();
    segment->add(dynamic);
    ASSERT_FALSE(dynamic->has_layout_data());
    master.parts()[1]->set_show(false);
    master.do_layout();
    ASSERT_FALSE(master.first_measure()->system()->staff(1)->show());
    EXPECT_FALSE(dynamic->has_layout_data());
}

TEST(Read, NormalizeOnlyRepairsLinesWithoutLayoutAndPreservesGeometryDirtiness) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    auto *original = to_chord(segment->element(0));
    auto *replacement = Factory::copy_chord(*original);
    segment->remove(original);
    std::unique_ptr<Chord> removed(original);
    segment->add(replacement);
    auto *note = replacement->notes().front();
    const int expected = note->line();
    note->set_line(invalid_line);
    ASSERT_FALSE(replacement->has_layout_data());
    ASSERT_FALSE(note->has_layout_data());
    master.set_layout_all();
    const auto pending = master.layout_state;
    std::vector<EngravingItem *> unlaid;
    for (auto *object : engrave::document_objects(master)) {
        if (object->is_engraving_item() && !to_engraving_item(object)->has_layout_data()) unlaid.push_back(to_engraving_item(object));
    }
    normalize_score(master);
    for (auto *item : unlaid) EXPECT_FALSE(item->has_layout_data()) << item->type_name();
    EXPECT_EQ(note->line(), expected);
    EXPECT_FALSE(replacement->has_layout_data());
    EXPECT_FALSE(note->has_layout_data());
    EXPECT_FALSE(master.normalize_state.dirty);
    EXPECT_TRUE(master.layout_state.dirty);
    EXPECT_EQ(master.layout_state.start, pending.start);
    EXPECT_EQ(master.layout_state.end, pending.end);
    EXPECT_EQ(master.layout_state.whole_count, pending.whole_count);
    EXPECT_EQ(master.layout_state.range_count, pending.range_count);
    const auto generation = master.normalize_state.normalized_generation;
    normalize_score(master);
    EXPECT_EQ(master.normalize_state.normalized_generation, generation);
    EXPECT_FALSE(note->has_layout_data());
}

TEST(Read, ReplayNormalizationDoesNotRecordInsideAnOpenTransaction) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *ornament = Factory::create_ornament(chord);
    ornament->set_sym_id(SymId::ORNAMENT_TRILL);
    chord->add(ornament);
    master.do_layout();
    master.start_cmd(muse::TranslatableString::untranslatable("explicit replay scope"));
    auto *recording = master.transaction_manager()->current_transaction();
    master.set_layout_all();
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    const auto mark = master.undo_stack()->active_transaction()->commands().size();
    normalize_document(master, {.mode = NormalizeMode::REPLAY});
    EXPECT_EQ(master.transaction_manager()->current_transaction(), recording);
    EXPECT_EQ(master.undo_stack()->active_transaction()->commands().size(), mark);
    EXPECT_FALSE(master.normalize_state.dirty);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    master.end_cmd(true, true);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
}

TEST(Read, NormalizePersistsMidiRequestsAcrossCommandStateReset) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_FALSE(master.midi_mapping().empty());
    master.set_playback_score(nullptr);
    master.midi_mapping().clear();
    master.add_layout_flags(LayoutFlag::REBUILD_MIDI_MAPPING);
    master.cmd_state().reset();
    ASSERT_TRUE(master.normalize_state.rebuild_midi_mapping);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    normalize_document(master);
    EXPECT_FALSE(master.midi_mapping().empty());
    EXPECT_FALSE(master.normalize_state.rebuild_midi_mapping);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
}

TEST(Read, MutationBoundariesPublishNormalizedSourceAndAccidentalIds) {
    using namespace mu::engraving;
    auto native = open_path(write470_fixture());
    ASSERT_TRUE(native);
    auto created = g_engine->create_document({.template_id = "Treble_Clef", .title = "normalized create"});
    ASSERT_TRUE(created);
    auto mnx = open_path(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/importexport/mnx/tests/data/engine/two-notes.mnx");
    ASSERT_TRUE(mnx);
    for (auto *document : {native.get(), created->get(), mnx.get()}) {
        auto &master = master_of(*document);
        for (auto *score : master.score_list()) {
            EXPECT_FALSE(score->normalize_state.dirty);
            EXPECT_FALSE(score->normalize_state.running);
        }
        EXPECT_FALSE(master.repeat_list(false).empty());
    }
    auto &master = master_of(*native);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    static MasterScore *observed = nullptr;
    static bool saw_normalization = false;
    observed = &master;
    saw_normalization = false;
    Transaction::set_push_hook([](UndoableCommand *, bool appended) {
        if (appended && observed->normalize_state.running) {
            EXPECT_NE(observed->transaction_manager()->current_transaction(), nullptr);
            saw_normalization = true;
        }
    });
    auto result = native->edit({{engrave::SetPitch{engrave::from_engine(note->eid()), 61, 21}}});
    Transaction::set_push_hook(nullptr);
    ASSERT_TRUE(result);
    EXPECT_TRUE(saw_normalization);
    ASSERT_NE(note->accidental(), nullptr);
    EXPECT_TRUE(note->accidental()->eid().is_valid());
    const auto accidental = engrave::from_engine(note->accidental()->eid());
    EXPECT_NE(std::ranges::find(result->created, accidental), result->created.end());
    EXPECT_FALSE(master.normalize_state.dirty);
    const auto outline = native->outline();
    ASSERT_TRUE(outline);
    auto excerpts = native->edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}});
    ASSERT_TRUE(excerpts) << excerpts.error().message;
    EXPECT_GT(master.score_list().size(), 1u);
    for (auto *score : master.score_list()) EXPECT_FALSE(score->normalize_state.dirty);
}

TEST(Read, InactiveSemanticBarrierPlumbingRefreshesDependentTargetsWhenOptedIn) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const auto id = engrave::from_engine(note->eid());
    const int line = note->line();
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    std::optional<engrave::api::Target> target;
    auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
    EXPECT_FALSE(hooks.needs_normalized_state);
    hooks.needs_normalized_state = [](std::size_t step) { return step == 1; };
    std::vector<engrave::api::Step> steps;
    steps.push_back([&](engrave::api::EditContext &context) -> engrave::Result<void> {
        auto resolved = context.target(id);
        if (!resolved) return std::unexpected(resolved.error());
        target = *resolved;
        note->undo_change_property(Pid::PITCH, note->pitch() + 12);
        return {};
    });
    steps.push_back([&](engrave::api::EditContext &context) -> engrave::Result<void> {
        auto fresh = context.target(id);
        if (!fresh) return std::unexpected(fresh.error());
        EXPECT_GT(fresh->epoch, target->epoch);
        auto resolved = context.object(*target);
        if (!resolved) return std::unexpected(resolved.error());
        EXPECT_EQ(to_note(*resolved)->line(), line - 7);
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
        return {};
    });
    auto result = engrave::api::BatchGuard(*document).run(std::move(steps), engrave::api::Policy::TRANSACTION, {});
    hooks = {};
    ASSERT_TRUE(result);
}

TEST(Read, NormalizeRepairsNoteOwnedLinesAfterMeasureInsertion) {
    using namespace mu::engraving;
    for (const auto type : {ElementType::GLISSANDO, ElementType::GUITAR_BEND, ElementType::NOTELINE}) {
        auto document = open_path(write470_fixture().parent_path() / "ties_mismatched.mscx");
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        auto *measure = master.first_measure();
        auto *first = measure->first(SegmentType::CHORD_REST);
        auto *start = to_chord(first->element(0))->notes().front();
        auto *end = to_chord(first->next(SegmentType::CHORD_REST)->element(0))->notes().front();
        std::unique_ptr<Spanner> line;
        switch (type) {
            case ElementType::GLISSANDO: line.reset(Factory::create_glissando(start)); break;
            case ElementType::GUITAR_BEND: line.reset(Factory::create_guitar_bend(start)); break;
            case ElementType::NOTELINE: line.reset(Factory::create_note_line(start)); break;
            default: FAIL();
        }
        line->set_note_span(start, end);
        start->add(line.get());
        EXPECT_TRUE(master.spanner_map().empty());
        const Fraction old_tick = start->tick();
        master.start_cmd(muse::TranslatableString::untranslatable("insert before note-owned line"));
        master.insert_measure(measure);
        const auto count = master.layout_state.whole_count + master.layout_state.range_count;
        normalize_score(master);
        EXPECT_GT(start->tick(), old_tick);
        EXPECT_EQ(line->tick(), start->tick());
        EXPECT_EQ(line->tick2(), end->tick());
        EXPECT_FALSE(line->has_layout_data());
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
        start->remove(line.get());
        end->chord()->update_ends_note_anchored_line();
        master.end_cmd(false, true);
    }
}

TEST(Read, NormalizeRepairsGraceTieTicksAfterMeasureInsertion) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    auto *main = to_chord(measure->first(SegmentType::CHORD_REST)->element(0));
    auto *before = Factory::copy_chord(*main);
    auto *after = Factory::copy_chord(*main);
    before->set_note_type(NoteType::ACCIACCATURA);
    after->set_note_type(NoteType::GRACE8_AFTER);
    for (auto *grace : {before, after}) {
        grace->set_duration_type(DurationType::V_EIGHTH);
        grace->set_ticks(Fraction(1, 8));
    }
    main->add(before);
    main->add(after);
    auto *start = before->notes().front();
    std::unique_ptr<Tie> tie(Factory::create_tie(start));
    tie->set_end_note(after->notes().front());
    start->add(tie.get());
    const Fraction old_tick = tie->tick();
    master.start_cmd(muse::TranslatableString::untranslatable("insert before grace tie"));
    master.insert_measure(measure);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    normalize_score(master);
    EXPECT_GT(before->tick(), old_tick);
    EXPECT_EQ(tie->tick(), before->tick());
    EXPECT_FALSE(tie->has_layout_data());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    start->remove(tie.get());
    master.end_cmd(false, true);
}

TEST(Read, NormalizeCleansInvalidMelismaBeforeRepairingLyricTicks) {
    using namespace mu::engraving;
    for (const auto syllabic : {LyricsSyllabic::BEGIN, LyricsSyllabic::MIDDLE}) {
        auto document = open_path(write470_fixture().parent_path() / "ties_mismatched.mscx");
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
        auto *first = to_chord(segment->element(0));
        auto *second = to_chord(segment->next(SegmentType::CHORD_REST)->element(0));
        auto *lyrics = Factory::create_lyrics(first);
        lyrics->set_track(0);
        lyrics->set_syllabic(syllabic);
        lyrics->set_ticks(Fraction(1, 4));
        first->add(lyrics);
        auto *following = Factory::create_lyrics(second);
        following->set_track(0);
        second->add(following);
        auto *separator = Factory::create_lyrics_line(lyrics);
        separator->set_track(0);
        separator->set_track2(0);
        separator->set_start_element(first);
        separator->set_end_element(second);
        separator->set_ticks(Fraction(1, 4));
        lyrics->set_separator(separator);
        master.add_unmanaged_spanner(separator);
        lyrics->set_need_remove_invalid_segments();
        ASSERT_TRUE(lyrics->need_remove_invalid_segments());
        master.set_layout_all();
        const auto count = master.layout_state.whole_count + master.layout_state.range_count;
        normalize_score(master);
        EXPECT_EQ(lyrics->ticks(), Fraction(0, 1));
        EXPECT_EQ(lyrics->syllabic(), syllabic == LyricsSyllabic::BEGIN ? LyricsSyllabic::SINGLE : LyricsSyllabic::END);
        EXPECT_EQ(lyrics->separator(), nullptr);
        EXPECT_FALSE(lyrics->need_remove_invalid_segments());
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    }
}

TEST(Read, RejectedTransactionWithOrnamentKeepsUndoAndRedoHistory) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.start_cmd(muse::TranslatableString::untranslatable("append a measure"));
    master.insert_measure();
    master.end_cmd(false, true);
    master.undo_stack()->clear_history();
    ASSERT_EQ(master.undo_stack()->size(), 0u);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *ornament = Factory::create_ornament(chord);
    ornament->set_sym_id(SymId::ORNAMENT_TRILL);
    chord->add(ornament);
    master.do_layout();
    ASSERT_NE(ornament->note_above(), nullptr);
    auto *target = master.last_measure()->first(SegmentType::CHORD_REST)->element(0);
    master.start_cmd(muse::TranslatableString::untranslatable("visibility change"));
    target->undo_change_property(Pid::VISIBLE, false);
    master.end_cmd(false, true);
    auto *manager = master.transaction_manager();
    manager->undo_redo(true, nullptr);
    auto *stack = master.undo_stack();
    ASSERT_TRUE(stack->can_redo());
    const auto size = stack->size();
    const auto index = stack->current_index();
    auto *redo = stack->next();
    const int auxiliary_pitch = ornament->note_above()->pitch();
    master.start_cmd(muse::TranslatableString::untranslatable("rejected visibility change"));
    target->undo_change_property(Pid::VISIBLE, false);
    static std::size_t replay_pushes = 0;
    replay_pushes = 0;
    Transaction::set_push_hook([](UndoableCommand *, bool appended) {
        if (appended) ++replay_pushes;
    });
    master.end_cmd(true, true);
    Transaction::set_push_hook(nullptr);
    EXPECT_EQ(replay_pushes, 0u);
    EXPECT_EQ(stack->size(), size);
    EXPECT_EQ(stack->current_index(), index);
    EXPECT_TRUE(stack->can_redo());
    EXPECT_EQ(stack->next(), redo);
    EXPECT_FALSE(stack->has_active_transaction());
    EXPECT_EQ(manager->current_transaction(), nullptr);
    EXPECT_TRUE(target->visible());
    EXPECT_EQ(ornament->note_above()->pitch(), auxiliary_pitch);
    manager->undo_redo(false, nullptr);
    EXPECT_FALSE(target->visible());
}

std::string mmr_source(bool prefix_note = false) {
    auto source = read_text(write470_fixture());
    source.insert(source.find("</Style>"), "<createMultiMeasureRests>1</createMultiMeasureRests>");
    constexpr std::string_view rest = "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>";
    if (!prefix_note) {
        const auto begin = source.find("<Chord>");
        const auto end = source.find("</Chord>", begin) + std::string("</Chord>").size();
        source.replace(begin, end - begin, "<Rest><durationType>measure</durationType></Rest>");
    }
    source.insert(source.find("</Staff>", source.find("<Measure>")),
                  prefix_note ? std::string(rest) + std::string(rest) : std::string(rest));
    return source;
}

TEST(Read, MmrCopiesKeepSourceRepeatsTempoHarmonyAndBeamingGroups) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    source.insert(source.find("</Style>"), "<createMultiMeasureRests>1</createMultiMeasureRests>");
    const auto begin = source.find("<Chord>");
    const auto end = source.find("</Chord>", begin) + std::string("</Chord>").size();
    source.replace(begin, end - begin, "<Rest><durationType>measure</durationType></Rest>");
    source.insert(source.find("</Measure>") + std::string("</Measure>").size(),
                  "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *first = master.first_measure();
    auto *last = master.last_measure();
    auto *marker = Factory::create_marker(first);
    marker->set_track(0);
    marker->set_marker_type(MarkerType::SEGNO);
    first->add(marker);
    auto *jump = Factory::create_jump(last);
    jump->set_track(0);
    jump->set_jump_type(JumpType::DS);
    last->add(jump);
    auto *segment = first->first(SegmentType::CHORD_REST);
    auto *tempo = Factory::create_tempo_text(segment);
    tempo->set_track(0);
    tempo->set_tempo(BeatsPerSecond(1.0));
    tempo->set_follow_text(false);
    segment->add(tempo);
    auto *harmony = Factory::create_harmony(segment);
    harmony->set_track(0);
    harmony->set_harmony(u"C");
    segment->add(harmony);
    master.parts().front()->update_harmony_channels(false);
    master.rebuild_midi_mapping();
    auto *signature = master.staff(0)->time_sig(Fraction(0, 1));
    ASSERT_NE(signature, nullptr);
    Groups custom;
    custom.add_stop(240, DurationType::V_EIGHTH, BeamMode::BEGIN);
    signature->set_groups(custom);
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
    master.do_layout();
    auto *display = first->mm_rest();
    ASSERT_NE(display, nullptr);
    EXPECT_EQ(jump->find_measure(), last);
    EXPECT_EQ(marker->find_measure(), first);
    EXPECT_EQ(tempo->find_measure(), first);
    EXPECT_EQ(harmony->find_measure(), first);
    ASSERT_NE(master.layout_copy(*tempo), nullptr);
    EXPECT_NE(master.layout_copy(*tempo), tempo);
    EXPECT_EQ(master.layout_copy(*tempo)->find_measure(), display);
    EXPECT_TRUE(master.layout_copy(*tempo)->is_layout_copy());
    master.set_layout_all();
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    normalize_document(master);
    master.init_automation();
    EXPECT_EQ(master.repeat_list(true).ticks(), 2 * master.end_tick().ticks());
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(0).val, 1.0);
    EXPECT_EQ(master.staff(0)->time_sig(Fraction(0, 1)), signature);
    EXPECT_EQ(master.staff(0)->group(Fraction(0, 1)), custom);
    const auto annotations = segment->annotations();
    EXPECT_EQ(std::ranges::count(annotations, harmony), 1);
    EXPECT_EQ(std::ranges::count(annotations, tempo), 1);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    PlaybackScoreBuilder builder;
    using namespace muse::mpe;
    auto profile = std::make_shared<ArticulationsProfile>();
    ArticulationPattern pattern;
    pattern.emplace(0, ArticulationPatternSegment(ArrangementPattern(hundred_percent, 0),
                                                  PitchPattern(ArticulationMap::expected_size, ten_percent, 0),
                                                  ExpressionPattern(ArticulationMap::expected_size, ten_percent, 0)));
    profile->set_pattern(ArticulationType::STANDARD, pattern);
    builder.profiles.profiles.fill(profile);
    auto playback = builder.build(&master);
    std::size_t chord_notes = 0;
    for (const auto &[id, events] : playback.tracks) chord_notes += events.notes.size();
    EXPECT_GT(chord_notes, 0u); // The source contains rests only; these are chord-symbol events.
}

TEST(Read, MmrRenderKeepsSemanticQueryAndOutlineSourceIdentity) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(mmr_source()), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *first = master.first_measure();
    auto *segment = first->first(SegmentType::CHORD_REST);
    auto *tempo = Factory::create_tempo_text(segment);
    tempo->set_track(0);
    tempo->set_tempo(BeatsPerSecond(1.0));
    tempo->set_follow_text(false);
    tempo->set_xml_text(u"Andante");
    segment->add(tempo);
    master.set_layout_all();
    normalize_document(master);
    master.init_automation();
    engrave::assign_missing_ids(master);
    const auto id = engrave::from_engine(tempo->eid());
    const auto before = document.outline();
    ASSERT_TRUE(before);
    engrave::Query query;
    query.types = {"Tempo", "TimeSig", "Clef"};
    const auto before_items = document.query(query);
    ASSERT_TRUE(before_items);
    ASSERT_TRUE(document.page_count());
    ASSERT_NE(first->mm_rest(), nullptr);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    const auto after = document.outline();
    const auto after_items = document.query(query);
    ASSERT_TRUE(after);
    ASSERT_TRUE(after_items);
    ASSERT_EQ(after->signatures.size(), before->signatures.size());
    for (std::size_t i = 0; i < before->signatures.size(); ++i) {
        EXPECT_EQ(after->signatures[i].type, before->signatures[i].type);
        EXPECT_EQ(after->signatures[i].tick, before->signatures[i].tick);
        EXPECT_EQ(after->signatures[i].bpm, before->signatures[i].bpm);
    }
    ASSERT_EQ(after_items->elements.size(), before_items->elements.size());
    for (std::size_t i = 0; i < before_items->elements.size(); ++i) {
        const auto &a = before_items->elements[i];
        const auto &b = after_items->elements[i];
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.measure, b.measure);
        EXPECT_EQ(a.tick, b.tick);
    }
    EXPECT_EQ(tempo->ownership_parent(), segment);
    EXPECT_TRUE(document.get(id, {"text"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    query.layout_boxes = true;
    const auto boxes = document.query(query);
    ASSERT_TRUE(boxes);
    const auto found = std::ranges::find(boxes->elements, id, &engrave::ElementInfo::id);
    ASSERT_NE(found, boxes->elements.end());
    EXPECT_TRUE(found->box.has_value());
    ASSERT_NE(master.layout_copy(*tempo), nullptr);
    EXPECT_EQ(master.layout_copy(*tempo)->find_measure(), first->mm_rest());
    // Rebuild and dissolve overlays without deleting or registering source items.
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, false);
    master.set_layout_all();
    ASSERT_TRUE(document.page_count());
    EXPECT_EQ(master.layout_copy(*tempo), nullptr);
    EXPECT_EQ(tempo->ownership_parent(), segment);
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
    master.set_layout_all();
    ASSERT_TRUE(document.page_count());
    ASSERT_NE(master.layout_copy(*tempo), nullptr);
    segment->remove(tempo);
    delete tempo;
    master.set_layout_all();
    ASSERT_TRUE(document.page_count());
    EXPECT_TRUE(first->mm_rest()->first(SegmentType::CHORD_REST)->annotations().empty());
}

TEST(Read, InsertBeforeRenderedMmrKeepsTempoAndRepeatMarkersOnSourceMeasures) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(mmr_source()), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *first = master.first_measure();
    auto *last = master.last_measure();
    auto *marker = Factory::create_marker(first);
    marker->set_track(0);
    marker->set_marker_type(MarkerType::SEGNO);
    first->add(marker);
    auto *jump = Factory::create_jump(last);
    jump->set_track(0);
    jump->set_jump_type(JumpType::DS);
    last->add(jump);
    auto *segment = first->first(SegmentType::CHORD_REST);
    auto *tempo = Factory::create_tempo_text(segment);
    tempo->set_track(0);
    tempo->set_tempo(BeatsPerSecond(1.0));
    tempo->set_follow_text(false);
    segment->add(tempo);
    master.set_layout_all();
    normalize_document(master);
    master.init_automation();
    engrave::assign_missing_ids(master);
    ASSERT_TRUE(document.page_count());
    ASSERT_NE(first->mm_rest(), nullptr);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    ASSERT_TRUE(document.edit({{engrave::InsertMeasures{.before = engrave::Ref{engrave::from_engine(first->eid())}, .count = 1}}}));
    EXPECT_EQ(tempo->find_measure(), first);
    EXPECT_EQ(marker->find_measure(), first);
    EXPECT_EQ(jump->find_measure(), last);
    EXPECT_EQ(tempo->tick(), Fraction(1, 1));
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(0).val, 2.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(1920).val, 1.0);
    EXPECT_EQ(master.repeat_list(true).ticks(), 5 * whole_note_ticks);
    EXPECT_TRUE(document.get(engrave::from_engine(marker->eid()), {"text"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
}

TEST(Read, MmrBreathPauseSurvivesSourceLengthChangeWithoutLayout) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(mmr_source()), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *first = master.first_measure();
    auto *last = master.last_measure();
    auto *segment = last->get_segment_r(SegmentType::BREATH, last->ticks());
    auto *breath = Factory::create_breath(segment);
    breath->set_track(0);
    breath->set_pause(1.0);
    segment->add(breath);
    master.set_layout_all();
    normalize_document(master);
    master.init_automation();
    const auto before = master.tempo_timeline(false).utick2utime(master.end_tick().ticks());
    ASSERT_TRUE(document.page_count());
    ASSERT_NE(first->mm_rest(), nullptr);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    ASSERT_TRUE(document.edit(
        {{engrave::SetProperty{engrave::Ref{engrave::from_engine(first->eid())}, "timesigActual", engrave::Fraction{2, 1}}}}));
    EXPECT_EQ(breath->find_measure(), last);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).utick2utime(master.end_tick().ticks()), before + 2.0);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
}

TEST(Read, ExcerptClonesSourceAfterRenderedMmrBoundaryBecomesStale) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(mmr_source(true)), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *first = master.first_measure();
    auto *last = master.last_measure();
    auto *jump = Factory::create_jump(last);
    jump->set_track(0);
    jump->set_jump_type(JumpType::DC);
    last->add(jump);
    master.set_layout_all();
    normalize_document(master);
    master.init_automation();
    engrave::assign_missing_ids(master);
    ASSERT_TRUE(document.page_count());
    ASSERT_NE(first->next_measure()->mm_rest(), nullptr);
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    ASSERT_TRUE(document.edit(
        {{engrave::SetProperty{engrave::Ref{engrave::from_engine(first->eid())}, "timesigActual", engrave::Fraction{2, 1}}}}));
    const auto outline = document.outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
    ASSERT_EQ(master.excerpts().size(), 1u);
    auto *part_score = master.excerpts().front()->excerpt_score();
    ASSERT_NE(part_score, nullptr);
    EXPECT_EQ(part_score->end_tick(), master.end_tick());
    EXPECT_EQ(part_score->last_measure()->tick(), master.last_measure()->tick());
    for (auto *measure = part_score->first_measure(); measure; measure = measure->next_measure()) EXPECT_EQ(measure->mm_rest(), nullptr);
    const auto found = std::ranges::find_if(part_score->last_measure()->el(), [](const auto *item) { return item->is_jump(); });
    ASSERT_NE(found, part_score->last_measure()->el().end());
    EXPECT_EQ((*found)->tick(), jump->tick());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    EXPECT_EQ(part_score->layout_state.whole_count + part_score->layout_state.range_count, 0u);
}

TEST(Read, NativeMasterExportPreparesUnrenderedExcerpts) {
    for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        auto opened = g_engine->open(write470_fixture());
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        const auto outline = document.outline();
        ASSERT_TRUE(outline);
        ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
        ASSERT_EQ(master.excerpts().size(), 1u);
        auto *part = master.excerpts().front()->excerpt_score();
        ASSERT_NE(part, nullptr);
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        EXPECT_EQ(part->layout_state.whole_count + part->layout_state.range_count, 0u);
        ASSERT_TRUE(document.export_score({.format = format}));
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
        EXPECT_EQ(part->layout_state.whole_count + part->layout_state.range_count, 1u);
        EXPECT_GT(part->npages(), 0);
        ASSERT_TRUE(document.export_score({.format = format}));
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
        EXPECT_EQ(part->layout_state.whole_count + part->layout_state.range_count, 1u);
    }
}

TEST(Read, NativeLoadRestoresLegacyMmrSourceOwnershipBeforePlayback) {
    using namespace mu::engraving;
    const auto path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/mmr/legacy_source_ownership.mscx";
    auto opened = g_engine->open(path);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *first = master.first_measure();
    auto *last = master.last_measure();
    auto *segment = first->first(SegmentType::CHORD_REST);
    const auto tempo = std::ranges::find_if(segment->annotations(), [](const auto *item) { return item->is_tempo_text(); });
    ASSERT_NE(tempo, segment->annotations().end());
    EXPECT_EQ((*tempo)->find_measure(), first);
    const auto jump = std::ranges::find_if(last->el(), [](const auto *item) { return item->is_jump(); });
    ASSERT_NE(jump, last->el().end());
    auto *breaths = last->find_segment_r(SegmentType::BREATH, last->ticks());
    ASSERT_NE(breaths, nullptr);
    ASSERT_NE(breaths->element(0), nullptr);
    EXPECT_EQ(breaths->element(0)->find_measure(), last);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(0).val, 1.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).utick2utime(master.end_tick().ticks()), 9.0);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, NormalizeResolvesSpannerEndsAgainstRealMeasuresWithMmRests) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    source.insert(source.find("</Style>"), "<createMultiMeasureRests>1</createMultiMeasureRests>");
    const auto chord_begin = source.find("<Chord>");
    const auto chord_end = source.find("</Chord>", chord_begin) + std::string("</Chord>").size();
    source.replace(chord_begin, chord_end - chord_begin, "<Rest><durationType>measure</durationType></Rest>");
    source.insert(source.find("</Measure>") + std::string("</Measure>").size(),
                  "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    ASSERT_TRUE((*opened)->page_count());
    auto &master = master_of(**opened);
    ASSERT_NE(master.first_measure(), master.last_measure());
    ASSERT_NE(master.first_measure()->mm_rest(), nullptr);
    std::array<Spanner *, 2> spanners{Factory::create_trill(master.dummy()), Factory::create_ottava(master.dummy())};
    for (auto *spanner : spanners) {
        spanner->set_track(0);
        spanner->set_track2(0);
        spanner->set_tick(Fraction(0, 1));
        spanner->set_tick2(master.end_tick());
        master.add_spanner(spanner, false);
    }
    master.set_layout_all();
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    normalize_score(master);
    for (auto *spanner : spanners) {
        ASSERT_NE(spanner->end_element(), nullptr);
        EXPECT_EQ(spanner->end_element()->find_measure(), master.last_measure());
        EXPECT_FALSE(spanner->has_layout_data());
    }
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    const auto &repeats = master.repeat_list(false);
    ASSERT_EQ(repeats.size(), 1u);
    EXPECT_EQ(repeats.front()->first_measure(), master.first_measure());
    EXPECT_EQ(repeats.front()->last_measure(), master.last_measure());
    for (auto *measure : repeats.front()->measure_list()) EXPECT_FALSE(measure->is_mm_rest());
}

TEST(Read, NormalizeRepairsLyricDurationsAndTieTicksWithoutLayout) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *lyrics = Factory::create_lyrics(chord);
    lyrics->set_track(chord->track());
    lyrics->set_ticks(Fraction(2, 1));
    chord->add(lyrics);
    master.set_layout_all();
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    normalize_score(master);
    EXPECT_EQ(lyrics->ticks(), Fraction(0, 1));
    EXPECT_EQ(lyrics->separator(), nullptr);
    EXPECT_FALSE(lyrics->has_layout_data());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);

    auto tied = open_path(write470_fixture().parent_path() / "ties_mismatched.mscx");
    ASSERT_TRUE(tied);
    auto &tie_score = master_of(*tied);
    std::vector<Tie *> ties;
    for (auto *object : engrave::document_objects(tie_score)) {
        if (object->is_note()) {
            if (auto *tie = to_note(object)->tie_for()) {
                ties.push_back(tie);
                tie->set_tick(Fraction(10, 1));
            }
        }
    }
    ASSERT_FALSE(ties.empty());
    tie_score.set_layout_all();
    const auto tie_count = tie_score.layout_state.whole_count + tie_score.layout_state.range_count;
    normalize_score(tie_score);
    for (auto *tie : ties) EXPECT_EQ(tie->tick(), tie->start_note()->chord()->tick());
    EXPECT_EQ(tie_score.layout_state.whole_count + tie_score.layout_state.range_count, tie_count);
}

TEST(Read, NormalizeCreatesTimeAnchorsWithoutPreparingTheirGeometry) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *hairpin = Factory::create_hairpin(master.dummy());
    hairpin->set_track(0);
    hairpin->set_track2(0);
    hairpin->set_tick(Fraction(1, 4));
    hairpin->set_tick2(Fraction(3, 4));
    master.add_spanner(hairpin, false);
    master.set_layout_all();
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    normalize_score(master);
    ASSERT_NE(hairpin->start_element(), nullptr);
    ASSERT_NE(hairpin->end_element(), nullptr);
    EXPECT_EQ(hairpin->start_element()->tick(), Fraction(1, 4));
    EXPECT_EQ(hairpin->end_element()->tick(), Fraction(3, 4));
    EXPECT_FALSE(hairpin->has_layout_data());
    EXPECT_FALSE(hairpin->start_element()->has_layout_data());
    EXPECT_FALSE(hairpin->end_element()->has_layout_data());
    EXPECT_FALSE(hairpin->start_element()->find_measure()->find_segment(SegmentType::TIME_TICK, Fraction(1, 4))->has_layout_data());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
}

TEST(Read, ExplicitFullLayoutNormalizesLegacyDirectPitchChanges) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const int line = note->line();
    ASSERT_FALSE(master.normalize_state.dirty);
    note->set_pitch(note->pitch() + 12);
    master.do_layout();
    EXPECT_EQ(note->line(), line - 7);
}

TEST(Read, FullStaffSemanticNormalizationDoesNotLayOut) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture().parent_path() / "four_flutes.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.parts()[1]->set_show(false);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(voices));
    auto *note = chord->notes().front();
    const int expected = note->line();
    note->set_line(invalid_line);
    master.set_layout_all();
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    normalize_score(master, {.include_hidden_staves = false});
    EXPECT_EQ(note->line(), invalid_line);
    prepare_semantic_edit(master);
    EXPECT_EQ(note->line(), expected);
    EXPECT_TRUE(master.normalize_state.includes_hidden_staves);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
}

TEST(Read, SemanticNoteLineAndCrossMeasureUpdatesDoNotAllocateLayout) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *original = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    std::unique_ptr<Chord> chord(Factory::copy_chord(*original));
    auto *note = chord->notes().front();
    ASSERT_FALSE(note->has_layout_data());
    const auto count = master.layout_state.whole_count + master.layout_state.range_count;
    note->set_line(invalid_line);
    note->update_line();
    EXPECT_NE(note->line(), invalid_line);
    EXPECT_FALSE(note->has_layout_data());
    chord->set_cross_measure(CrossMeasure::FIRST);
    EXPECT_TRUE(normalize_cross_measure(*chord, false));
    EXPECT_EQ(chord->cross_measure(), CrossMeasure::UNKNOWN);
    EXPECT_FALSE(normalize_cross_measure(*chord, false));
    EXPECT_FALSE(chord->has_layout_data());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
}

TEST(Read, UnlaidElementsHaveImmutableTypedLayoutDefaults) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    std::unique_ptr<mu::engraving::Chord> first_owner(mu::engraving::Factory::create_chord(master.dummy()));
    auto &first = *first_owner;
    std::unique_ptr<mu::engraving::Chord> second_owner(mu::engraving::Factory::create_chord(master.dummy()));
    auto &second = *second_owner;
    const mu::engraving::Chord &query = first;
    const mu::engraving::EngravingItem &base_query = first;
    EXPECT_FALSE(first.has_layout_data());
    EXPECT_FALSE(second.has_layout_data());
    EXPECT_EQ(query.ldata(), second.ldata());
    EXPECT_EQ(base_query.ldata(), query.ldata());
    EXPECT_EQ(query.pos(), muse::PointF());
    EXPECT_TRUE(query.shape().empty());
    EXPECT_TRUE(query.ldata()->bbox().is_null());
    EXPECT_EQ(query.z(), mu::engraving::default_z(query.type()));
    // Reading a default must leave it unresolved; explicit copies preserve z separately.
    std::unique_ptr<mu::engraving::MeasureRepeat> z_probe(mu::engraving::Factory::create_measure_repeat(master.dummy()));
    EXPECT_EQ(z_probe->z(), mu::engraving::default_z(mu::engraving::ElementType::MEASURE_REPEAT));
    z_probe->hack_to_rest_type();
    EXPECT_EQ(z_probe->z(), mu::engraving::default_z(mu::engraving::ElementType::REST));
    EXPECT_EQ(query.get_property(mu::engraving::Pid::Z).to_int(), query.z());
    EXPECT_FALSE(first.has_layout_data());
    mu::engraving::EngravingItem::disconnect_all_origin_items(&first);
    EXPECT_FALSE(first.has_layout_data());
    auto *data = first.mutldata();
    EXPECT_TRUE(first.has_layout_data());
    EXPECT_NE(data, second.ldata());
    data->set_pos(3.0, 4.0);
    EXPECT_EQ(query.pos(), muse::PointF(3.0, 4.0));
    EXPECT_EQ(second.pos(), muse::PointF());
    EXPECT_FALSE(second.has_layout_data());
}

TEST(Read, MeasureRepeatCopiesPreserveZWhenConvertedToRest) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    for (const bool read_source : {false, true}) {
        for (const int explicit_z : {-1, 4321}) {
            std::unique_ptr<mu::engraving::MeasureRepeat> repeat(mu::engraving::Factory::create_measure_repeat(master.dummy()));
            repeat->set_z(explicit_z);
            const int expected = explicit_z == -1 ? mu::engraving::default_z(mu::engraving::ElementType::MEASURE_REPEAT) : explicit_z;
            if (read_source) {
                EXPECT_EQ(repeat->z(), expected);
            }
            std::unique_ptr<mu::engraving::Rest> rest(mu::engraving::Factory::copy_rest(*repeat));
            rest->hack_to_rest_type();
            EXPECT_EQ(rest->z(), expected);
            muse::ByteArray bytes;
            muse::io::Buffer buffer(&bytes);
            ASSERT_TRUE(buffer.open(muse::io::IODevice::WRITE_ONLY));
            {
                mu::engraving::XmlWriter xml(&buffer);
                mu::engraving::write::TWrite::write_property(rest.get(), xml, mu::engraving::Pid::Z);
                xml.flush();
            }
            const std::string serialized(reinterpret_cast<const char *>(bytes.const_data()), bytes.size());
            EXPECT_NE(serialized.find("<z>" + std::to_string(expected) + "</z>"), std::string::npos);
        }
    }
}

TEST(Read, GraceNoteQueryResultsAreIndependent) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    std::unique_ptr<mu::engraving::Chord> chord_owner(mu::engraving::Factory::create_chord(master.dummy()));
    auto &chord = *chord_owner;
    auto *grace = mu::engraving::Factory::create_chord(master.dummy());
    grace->set_note_type(mu::engraving::NoteType::ACCIACCATURA);
    chord.add(grace);
    const auto &all = chord.grace_notes_before();
    ASSERT_EQ(all.size(), 1);
    EXPECT_EQ(all.front(), grace);
    EXPECT_TRUE(chord.grace_notes_before(true).empty());
    EXPECT_EQ(all.size(), 1);
    EXPECT_TRUE(chord.grace_notes_before(true).empty());
    auto *after = mu::engraving::Factory::create_chord(master.dummy());
    after->set_note_type(mu::engraving::NoteType::GRACE8_AFTER);
    after->set_grace_index(1);
    chord.add(after);
    auto *last = mu::engraving::Factory::create_chord(master.dummy());
    last->set_note_type(mu::engraving::NoteType::GRACE16_AFTER);
    last->set_grace_index(2);
    chord.add(last);
    const auto &after_all = chord.grace_notes_after();
    ASSERT_EQ(after_all.size(), 2);
    EXPECT_EQ(after_all.front(), last);
    EXPECT_EQ(after_all.back(), after);
    EXPECT_EQ(after_all.at(1), after);
    const std::vector<mu::engraving::Chord *> owned = after_all;
    EXPECT_EQ(owned, (std::vector<mu::engraving::Chord *>{last, after}));
    EXPECT_TRUE(chord.grace_notes_after(true).empty());
    EXPECT_EQ(after_all.size(), 2);
    EXPECT_FALSE(chord.has_layout_data());
}

// Inspect ownership rather than scan_elements(), which omits layout-only groups.
size_t grace_group_count(const mu::engraving::EngravingObject *object) {
    size_t result = object->is_grace_notes_group() ? 1 : 0;
    for (const auto *child : object->children()) {
        result += grace_group_count(child);
    }
    if (object->is_segment()) {
        const auto *segment = mu::engraving::to_segment(object);
        for (mu::engraving::track_idx_t track = 0; track < segment->pre_appended_items().size(); ++track) {
            const auto *item = segment->pre_appended_item(track);
            EXPECT_TRUE(!item || !item->is_grace_notes_group());
        }
    }
    return result;
}

mu::engraving::Chord *add_test_grace(mu::engraving::Chord *chord, mu::engraving::NoteType type) {
    using namespace mu::engraving;
    Chord *grace = Factory::copy_chord(*chord);
    grace->set_note_type(type);
    grace->set_duration_type(DurationType::V_EIGHTH);
    grace->set_ticks(Fraction(1, 8));
    chord->add(grace);
    return grace;
}

TEST(Read, GraceGroupsAreOwnedByLayoutScope) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    Segment *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    Chord *chord = to_chord(segment->element(0));
    add_test_grace(chord, NoteType::ACCIACCATURA);
    EXPECT_EQ(grace_group_count(&master), 0);
    // A retained segment clone has elements but no attachment buffer.
    std::unique_ptr<Segment> retained(segment->clone());
    {
        LayoutContext context(&master);
        ChordLayout::append_grace_notes(chord, context);
        const GraceNotesGroup *group = context.grace_group(chord, false);
        ASSERT_NE(group, nullptr);
        EXPECT_EQ(group->layout_parent(), chord);
        EXPECT_NE(group->ownership_parent(), chord);
        ASSERT_EQ(segment->pre_appended_item(0), group);
        {
            LayoutContext nested(&master);
            nested.update_grace_group(chord, false);
        }
        EXPECT_EQ(segment->pre_appended_item(0), group);
    }
    EXPECT_EQ(grace_group_count(&master), 0);
    EXPECT_THROW(
        {
            LayoutContext context(&master);
            ChordLayout::append_grace_notes(chord, context);
            throw std::runtime_error("unwind layout");
        },
        std::runtime_error);
    EXPECT_EQ(grace_group_count(&master), 0);
    master.do_layout();
    EXPECT_EQ(grace_group_count(&master), 0);
    // No edit invalidation is needed between runs, even for a hidden staff.
    master.parts().front()->set_show(false);
    Chord *grace = chord->grace_notes().front();
    chord->remove(grace);
    delete grace;
    master.do_layout();
    EXPECT_EQ(grace_group_count(&master), 0);
}

TEST(Read, GraceGroupsDoNotSurviveRepeatedScoreLayout) {
    auto document = open_path(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/tab_grace_prebend.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    EXPECT_EQ(grace_group_count(&master), 0);
    auto baseline = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(baseline);
    for (int run = 0; run < 2; ++run) {
        master.do_layout();
        EXPECT_EQ(grace_group_count(&master), 0);
        auto rendered = document->render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(rendered);
        EXPECT_EQ(rendered->pages, baseline->pages);
    }
}

TEST(Read, GraceLayoutScopeClearsMovedSegmentAfterSourceDeletion) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
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
    auto opened = g_engine->open(as_bytes(source));
    ASSERT_TRUE(opened);
    auto document = std::move(*opened);
    auto &master = master_of(*document);
    Measure *measure = master.first_measure();
    Measure *next = measure->next_measure();
    ASSERT_NE(next, nullptr);
    Segment *source_segment = measure->first(SegmentType::CHORD_REST);
    Chord *chord = to_chord(source_segment->element(0));
    Chord *grace = add_test_grace(chord, NoteType::GRACE8_AFTER);
    ASSERT_EQ(chord->end_tick(), measure->end_tick());
    Segment *clef_segment = measure->get_segment(SegmentType::CLEF, measure->end_tick());
    if (!clef_segment->element(0)) {
        Clef *clef = Factory::create_clef(clef_segment);
        clef->set_track(0);
        clef_segment->add(clef);
    }
    {
        LayoutContext context(&master);
        ChordLayout::append_grace_notes(chord, context);
        ASSERT_EQ(context.grace_group(chord, true)->appended_segment(), clef_segment);
        ASSERT_EQ(clef_segment->pre_appended_item(0), context.grace_group(chord, true));
        ChangeSegmentParent move(clef_segment, next, Fraction(0, 1));
        move.redo();
        ASSERT_EQ(clef_segment->measure(), next);
        chord->remove(grace);
        delete grace;
        source_segment->remove(chord);
        delete chord;
    }
    EXPECT_EQ(clef_segment->pre_appended_item(0), nullptr);
    EXPECT_EQ(grace_group_count(&master), 0);
}

TEST(Read, GraceLayoutScopeSurvivesDeletedNonemptyMergedSegment) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    Measure *measure = master.first_measure();
    Chord *first = to_chord(measure->first(SegmentType::CHORD_REST)->element(0));
    first->set_duration_type(DurationType::V_HALF);
    first->set_ticks(Fraction(1, 2));
    Segment *segment = measure->get_segment(SegmentType::CHORD_REST, measure->tick() + Fraction(1, 2));
    Chord *second = Factory::copy_chord(*first);
    second->set_track(0);
    segment->add(second);
    Chord *after = add_test_grace(first, NoteType::GRACE8_AFTER);
    Chord *before = add_test_grace(second, NoteType::ACCIACCATURA);
    {
        LayoutContext context(&master);
        ChordLayout::append_grace_notes(first, context);
        ChordLayout::append_grace_notes(second, context);
        const GraceNotesGroup *merged = context.grace_group(first, true);
        ASSERT_EQ(segment->pre_appended_item(0), merged);
        ASSERT_EQ(merged->size(), 2);
        EXPECT_NE(std::find(merged->begin(), merged->end(), before), merged->end());
        measure->remove(segment);
        delete segment;
        // Group teardown must not dereference either the freed grace or segment.
    }
    EXPECT_EQ(grace_group_count(&master), 0);
    {
        LayoutContext context(&master);
        const GraceNotesGroup &fresh = context.update_grace_group(first, true);
        ASSERT_EQ(fresh.size(), 1);
        EXPECT_EQ(fresh.front(), after);
    }
    EXPECT_EQ(grace_group_count(&master), 0);
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

constexpr std::string_view repeat_edit_xml = R"(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time></attributes>
<barline location="left"><repeat direction="forward"/></barline>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note></measure>
<measure number="2"><note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
<barline location="right"><repeat direction="backward"/></barline></measure>
<measure number="3"><direction><sound tempo="60"/></direction>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note></measure>
</part></score-partwise>)";

TEST(Read, RepeatRepairStaysInTheActiveTransaction) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(repeat_edit_xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *manager = master.transaction_manager();
    auto *first = master.first_measure()->first(SegmentType::CHORD_REST)->element(0);
    auto *second_measure = master.first_measure()->next_measure();
    auto *second = second_measure->first(SegmentType::CHORD_REST)->element(0);
    auto *slur = Factory::create_slur(master.dummy());
    slur->set_start_element(first);
    slur->set_end_element(second);
    slur->set_track(0);
    slur->set_track2(0);
    slur->set_tick(first->tick());
    slur->set_ticks(second->tick() - first->tick());
    slur->set_incoming(true);
    manager->begin_transaction(muse::TranslatableString::untranslatable("partial slur setup"));
    master.undo_add_element(slur);
    manager->end_transaction();
    ASSERT_TRUE(slur->is_incoming());
    const auto history = master.undo_stack()->size();
    manager->begin_transaction(muse::TranslatableString::untranslatable("remove repeat"));
    master.undo(new ChangeProperty(second_measure, Pid::REPEAT_END, false));
    manager->end_transaction();
    EXPECT_FALSE(slur->is_incoming());
    EXPECT_FALSE(master.undo_stack()->has_active_transaction());
    EXPECT_EQ(master.undo_stack()->size(), history + 1);
    manager->undo_redo(true, nullptr);
    EXPECT_TRUE(second_measure->repeat_end());
    EXPECT_TRUE(slur->is_incoming());
    manager->undo_redo(false, nullptr);
    EXPECT_FALSE(second_measure->repeat_end());
    EXPECT_FALSE(slur->is_incoming());
}

TEST(Read, RepeatChangeAndTieInOneBatchMatchSeparateBatches) {
    using namespace mu::engraving;
    std::vector<std::tuple<int, bool, bool>> expected;
    std::vector<std::byte> expected_midi;
    for (bool combined : {false, true}) {
        auto opened = g_engine->open(as_bytes(repeat_edit_xml), {.format = engrave::Format::MUSIC_XML});
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *repeat_measure = master.first_measure()->next_measure();
        auto *note = to_chord(repeat_measure->first(SegmentType::CHORD_REST)->element(0))->up_note();
        const auto repeat_ref = engrave::Ref{engrave::from_engine(repeat_measure->eid())};
        const auto note_ref = engrave::Ref{engrave::from_engine(note->eid())};
        ASSERT_TRUE(document.edit({{engrave::SetProperty{repeat_ref, "repeatEnd", false}}}));
        if (combined) {
            ASSERT_TRUE(document.edit({{engrave::SetProperty{repeat_ref, "repeatEnd", true}, engrave::AddTie{note_ref}}}));
        } else {
            ASSERT_TRUE(document.edit({{engrave::SetProperty{repeat_ref, "repeatEnd", true}}}));
            ASSERT_TRUE(document.edit({{engrave::AddTie{note_ref}}}));
        }
        ASSERT_NE(note->tie_for(), nullptr);
        ASSERT_NE(note->tie_for()->tie_jump_points(), nullptr);
        std::vector<std::tuple<int, bool, bool>> actual;
        for (const auto *point : *note->tie_for()->tie_jump_points()) {
            actual.emplace_back(point->note()->tick().ticks(), point->active(), point->following_note());
        }
        ASSERT_EQ(actual.size(), 2u);
        EXPECT_NE(to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->up_note()->incoming_partial_tie(), nullptr);
        const auto midi = document.export_score({.format = engrave::Format::MIDI});
        ASSERT_TRUE(midi);
        if (!combined) {
            expected = actual;
            expected_midi = *midi;
        } else {
            EXPECT_EQ(actual, expected);
            EXPECT_EQ(*midi, expected_midi);
        }
        const auto saved = document.export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        auto &reloaded = master_of(**reopened);
        EXPECT_NE(to_chord(reloaded.first_measure()->first(SegmentType::CHORD_REST)->element(0))->up_note()->incoming_partial_tie(),
                  nullptr);
    }
}

TEST(Read, ActualMeasureLengthRefreshesFollowingTempo) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(repeat_edit_xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *measure = master.first_measure()->next_measure();
    auto *following = measure->next_measure();
    EXPECT_EQ(following->tick().ticks(), 3840);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(3839).val, 2.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(3840).val, 1.0);
    engrave::DocumentAccess::edit_hooks(document).keep_history = true;
    ASSERT_TRUE(document.edit(
        {{engrave::SetProperty{engrave::Ref{engrave::from_engine(measure->eid())}, "timesigActual", engrave::Fraction{1, 2}}}}));
    EXPECT_EQ(following->tick().ticks(), 2880);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(2879).val, 2.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(2880).val, 1.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).tempo(5760).val, 1.0);
    master.transaction_manager()->undo_redo(true, nullptr);
    EXPECT_EQ(following->tick().ticks(), 3840);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(2880).val, 2.0);
    master.transaction_manager()->undo_redo(false, nullptr);
    EXPECT_EQ(following->tick().ticks(), 2880);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(2880).val, 1.0);
}

TEST(Read, PlaybackFinalizationPublishesStableReadsAndReplay) {
    using namespace mu::engraving;
    constexpr std::string_view xml = R"(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time></attributes>
<direction><sound tempo="120"/></direction>
<barline location="left"><repeat direction="forward"/></barline>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
<barline location="right"><repeat direction="backward"/></barline></measure></part>
</score-partwise>)";
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *manager = master.transaction_manager();
    const auto before = music::dump_json(music::semantic_dump(&master), 1);
    const auto history = master.undo_stack()->size();
    const auto automation = master.automation_data()->dump();
    const auto count = layout_count(master);
    const auto *first = master.repeat_list(true).front();
    for (int i = 0; i < 4; ++i) {
        for (bool expand : {false, true}) {
            const auto &repeats = master.repeat_list(expand);
            const auto &tempo = master.tempo_timeline(expand);
            EXPECT_EQ(repeats.utick2tick(1920), expand ? 0 : 1920);
            EXPECT_DOUBLE_EQ(tempo.utick2utime(1920), 2.0);
            EXPECT_EQ(tempo.utime2utick(2.0), 1920);
            EXPECT_DOUBLE_EQ(tempo.tempo(0).val, 2.0);
        }
    }
    EXPECT_EQ(master.repeat_list(true).front(), first);
    EXPECT_EQ(master.automation_data()->dump(), automation);
    EXPECT_EQ(master.undo_stack()->size(), history);
    EXPECT_EQ(layout_count(master), count);
    EXPECT_EQ(music::dump_json(music::semantic_dump(&master), 1), before);

    manager->begin_transaction(muse::TranslatableString::untranslatable("repeat edit"));
    master.undo(new ChangeProperty(master.first_measure(), Pid::REPEAT_COUNT, 3));
    manager->end_transaction();
    EXPECT_EQ(master.repeat_list(true).ticks(), 5760);
    EXPECT_EQ(master.repeat_list(false).ticks(), 1920);
    EXPECT_EQ(master.undo_stack()->size(), history + 1);
    manager->undo_redo(true, nullptr);
    EXPECT_EQ(master.repeat_list(true).ticks(), 3840);
    manager->undo_redo(false, nullptr);
    EXPECT_EQ(master.repeat_list(true).ticks(), 5760);

    TempoText *mark = nullptr;
    master.scan_elements([&mark](EngravingItem *item) {
        if (item->is_tempo_text()) mark = to_tempo_text(item);
    });
    ASSERT_NE(mark, nullptr);
    const auto midi_before = document.export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(midi_before);
    manager->begin_transaction(muse::TranslatableString::untranslatable("tempo edit"));
    master.undo(new ChangeProperty(mark, Pid::TEMPO, BeatsPerSecond(1.0)));
    manager->end_transaction();
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).utick2utime(1920), 4.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).utick2utime(1920), 4.0);
    const auto midi_after = document.export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(midi_after);
    EXPECT_NE(*midi_after, *midi_before);
    manager->undo_redo(true, nullptr);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).utick2utime(1920), 2.0);
    manager->undo_redo(false, nullptr);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).utick2utime(1920), 4.0);
    ASSERT_TRUE(master.set_tempo_multiplier(BeatsPerSecond(2.0)));
    master.init_automation();
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).utick2utime(1920), 2.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).utick2utime(1920), 2.0);
    TempoTimeline override;
    override.rebuild(TempoValues{{0, 4.0}}, {});
    master.set_tempo_timeline_override(override);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).utick2utime(1920), 1.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).utick2utime(1920), 1.0);
    master.set_tempo_timeline_override(std::nullopt);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).utick2utime(1920), 2.0);
    AutomationPoint point;
    point.value.out_value = normalize_tempo(BeatsPerSecond(3.0));
    AutomationPointEdits edits{{480, AutomationPointEdit::SetPoint{point}}};
    master.edit_automation_points(tempo_key, edits, false);
    const auto authored = master.automation_data()->curve(tempo_key).at(480);
    master.init_automation();
    EXPECT_EQ(master.automation_data()->curve(tempo_key).at(480), authored);
    EXPECT_FALSE(master.automation_data()->curve(tempo_key).at(480).generated);
    const auto measures = document.query({.types = {"Measure"}});
    ASSERT_TRUE(measures);
    ASSERT_FALSE(measures->elements.empty());
    ASSERT_TRUE(document.edit({{engrave::InsertMeasures{.before = engrave::Ref{measures->elements.front().id}, .count = 1}}}));
    const auto &shifted = master.automation_data()->curve(tempo_key);
    ASSERT_TRUE(shifted.contains(2400));
    EXPECT_EQ(shifted.at(2400), authored);
}

TEST(Read, VoltaAndJumpReplayPublishesRepeatState) {
    using namespace mu::engraving;
    for (const auto &[type, property, replacement, fixture] :
         {std::tuple{"Volta", Pid::VOLTA_ENDING, PropertyValue::from_value(std::vector<int>{2}),
                     "tests/data/inputs/golden/repeat_with_2_voltas.mscx"},
          std::tuple{"Jump", Pid::JUMP_TO, PropertyValue(muse::String(u"invalid-target")),
                     "src/engraving/tests/playback/playbackmodel_data/da_capo_al_fine/da_capo_al_fine.mscx"}}) {
        auto document = open_path(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / fixture);
        ASSERT_TRUE(document) << type;
        auto &master = master_of(*document);
        EngravingItem *target = nullptr;
        master.scan_elements([&](EngravingItem *item) {
            if ((std::string_view(type) == "Volta" && item->is_volta()) || (std::string_view(type) == "Jump" && item->is_jump()))
                target = item;
        });
        if (std::string_view(type) == "Volta") {
            for (const auto &[tick, spanner] : master.spanner()) {
                if (spanner->is_volta() && (!target || spanner->tick() < target->tick())) target = spanner;
            }
        }
        ASSERT_NE(target, nullptr) << type;
        auto signature = [&master] {
            std::vector<int> result;
            for (const auto *segment : master.repeat_list(true)) {
                result.insert(result.end(), {segment->tick, segment->utick, segment->len(), segment->playback_count});
            }
            return result;
        };
        const auto before = signature();
        auto *manager = master.transaction_manager();
        manager->begin_transaction(muse::TranslatableString::untranslatable("repeat navigation edit"));
        master.undo(new ChangeProperty(target, property, replacement));
        manager->end_transaction();
        const auto after = signature();
        EXPECT_NE(after, before) << type;
        manager->undo_redo(true, nullptr);
        EXPECT_EQ(signature(), before) << type;
        manager->undo_redo(false, nullptr);
        EXPECT_EQ(signature(), after) << type;
        // An explicit independent rebuild must agree with the already published state.
        master.invalidate_repeat_list();
        master.update_repeat_list(false);
        EXPECT_EQ(signature(), after) << type;
    }
}

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
    EXPECT_EQ(after_edit, count + 1); // Edits preserve pending geometry until the boundary.
    ASSERT_TRUE(document->render({}));
    ASSERT_TRUE(document->page_count());
    EXPECT_EQ(layout_count(master), after_edit + 1);
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
    mu::engraving::invalidate_normalization(master, mu::engraving::Fraction(1, 4), mu::engraving::Fraction(1, 2));
    const auto normalized = master.normalize_state;
    auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
    hooks.before_step = [](std::size_t step) {
        if (step == 1) throw std::runtime_error("injected failure");
    };
    const auto revision = document->revision();
    EXPECT_FALSE(document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
    hooks = {};
    EXPECT_EQ(document->revision(), revision);
    EXPECT_EQ(master.layout_state.dirty, saved.dirty);
    EXPECT_EQ(master.layout_state.start, mu::engraving::Fraction(0, 1));
    EXPECT_LT(master.layout_state.end, mu::engraving::Fraction(0, 1));
    EXPECT_EQ(master.normalize_state.dirty, normalized.dirty);
    EXPECT_EQ(master.normalize_state.start, normalized.start);
    EXPECT_EQ(master.normalize_state.end, normalized.end);
    EXPECT_EQ(master.normalize_state.generation, normalized.generation);
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
    ASSERT_TRUE(document.page_count());
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

// Replacement keeps a full layout pending. Counter deltas are meaningful within
// each Score lifetime, not across project replacement.
TEST(Read, SnapshotReplacementPreservesRuntimeTempoSettings) {
    using namespace mu::engraving;
    for (const bool override_active : {false, true}) {
        auto document = open_path(write470_fixture());
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        ASSERT_TRUE(master.set_tempo_multiplier(BeatsPerSecond(2.0)));
        if (override_active) {
            TempoTimeline timeline;
            timeline.rebuild(TempoValues{{0, 3.0}}, {});
            timeline.set_tempo_multiplier(BeatsPerSecond(1.5));
            master.set_tempo_timeline_override(timeline);
        }
        const double before = master.tempo_timeline().utick2utime(1920);
        auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
        hooks.policy = engrave::api::Policy::SNAPSHOT;
        hooks.before_step = [](std::size_t) { throw std::runtime_error("snapshot tempo"); };
        ASSERT_FALSE(document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
        hooks = {};
        auto &restored = master_of(*document);
        EXPECT_EQ(restored.layout_state.whole_count + restored.layout_state.range_count, 0u);
        EXPECT_TRUE(restored.layout_state.dirty);
        EXPECT_DOUBLE_EQ(restored.tempo_timeline().utick2utime(1920), before);
        EXPECT_EQ(restored.runtime_tempo_state().override_timeline.has_value(), override_active);
        restored.set_tempo_timeline_override(std::nullopt);
        EXPECT_DOUBLE_EQ(restored.tempo_timeline().tempo_multiplier().val, 2.0);
    }
}

TEST(Read, SnapshotReplacementDefersMasterPendingLayout) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "chordline_path.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    for (const bool change_font : {false, true}) {
        SCOPED_TRACE(change_font);
        auto document = open_path(*it);
        ASSERT_TRUE(document);
        const auto outline = document->outline();
        ASSERT_TRUE(outline);
        ASSERT_GE(outline->parts.size(), 2u);
        auto *before = &master_of(*document);
        using mu::engraving::Fraction;
        for (int i = 0; i < 5; ++i) {
            before->invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
            ASSERT_TRUE(document->page_count());
        }
        const auto old_count = layout_count(*before);
        before->invalidate_layout(Fraction(1, 4), Fraction(1, 2));
        const auto revision = document->revision();
        const auto generation = engrave::DocumentAccess::impl(*document).engine->font_generation;
        auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
        hooks.before_step = [change_font](std::size_t step) {
            if (step != 1) return;
            if (change_font) ASSERT_TRUE(g_engine->add_font(g_assets / "fonts/petaluma/PetalumaScript.otf"));
            throw std::runtime_error("recover after generated excerpt");
        };
        EXPECT_FALSE(document->edit({{engrave::GenerateExcerpts{{outline->parts[1].id}}}}));
        hooks = {};
        auto &after = master_of(*document);
        EXPECT_NE(&after, before);
        EXPECT_EQ(document->revision(), revision);
        EXPECT_TRUE(after.layout_state.dirty);
        EXPECT_FALSE(after.normalize_state.dirty);
        EXPECT_FALSE(after.normalize_state.running);
        EXPECT_EQ(after.layout_state.revision, revision);
        EXPECT_EQ(after.layout_state.font_generation, generation + change_font);
        EXPECT_EQ(after.layout_state.last_start, Fraction(0, 1));
        EXPECT_EQ(after.layout_state.last_end, Fraction(-1, 1));
        const auto count = layout_count(after);
        EXPECT_EQ(count, 0u);
        EXPECT_LT(count, old_count); // Replacement diagnostics start with reload work.
        ASSERT_TRUE(document->page_count());
        ASSERT_TRUE(document->render({}));
        EXPECT_EQ(layout_count(after), count + 1);
    }
}

TEST(Read, SnapshotReplacementDefersExcerptPendingLayout) {
    for (const bool change_font : {false, true}) {
        SCOPED_TRACE(change_font);
        auto document = open_path(write470_fixture().parent_path() / "excerpts.mscx");
        ASSERT_TRUE(document);
        const auto outline = document->outline();
        ASSERT_TRUE(outline);
        ASSERT_FALSE(outline->excerpts.empty());
        const engrave::ScoreSel selection{outline->excerpts.front().id};
        auto &master = master_of(*document);
        auto selected = engrave::select_score(master, selection);
        ASSERT_TRUE(selected);
        auto *before = *selected;
        using mu::engraving::Fraction;
        ASSERT_TRUE(document->page_count());
        for (int i = 0; i < 5; ++i) {
            before->invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
            ASSERT_TRUE(document->page_count(selection));
        }
        const auto old_count = layout_count(*before);
        master.invalidate_layout(Fraction(1, 4), Fraction(1, 2));
        before->invalidate_layout(Fraction(3, 4), Fraction(1, 1));
        const auto generation = engrave::DocumentAccess::impl(*document).engine->font_generation;
        auto &hooks = engrave::DocumentAccess::edit_hooks(*document);
        hooks.policy = engrave::api::Policy::SNAPSHOT;
        hooks.before_step = [change_font](std::size_t) {
            if (change_font) ASSERT_TRUE(g_engine->add_font(g_assets / "fonts/petaluma/PetalumaScript.otf"));
            throw std::runtime_error("replace master and excerpt");
        };
        EXPECT_FALSE(document->edit({{engrave::SetStyle{selection, "staffDistance", 9.0}}}));
        hooks = {};
        selected = engrave::select_score(master_of(*document), selection);
        ASSERT_TRUE(selected);
        auto &after = **selected;
        EXPECT_NE(&after, before);
        EXPECT_TRUE(after.layout_state.dirty);
        EXPECT_FALSE(after.normalize_state.dirty);
        EXPECT_FALSE(after.normalize_state.running);
        EXPECT_TRUE(master_of(*document).layout_state.dirty);
        EXPECT_EQ(after.layout_state.revision, document->revision());
        EXPECT_EQ(after.layout_state.font_generation, generation + change_font);
        EXPECT_EQ(after.layout_state.last_start, Fraction(0, 1));
        EXPECT_EQ(after.layout_state.last_end, Fraction(-1, 1));
        const auto count = layout_count(after);
        EXPECT_EQ(count, 0u);
        EXPECT_LT(count, old_count);
        const auto master_count = layout_count(master_of(*document));
        ASSERT_TRUE(document->page_count(selection));
        ASSERT_TRUE(document->render({.score = selection}));
        ASSERT_TRUE(document->page_count());
        EXPECT_EQ(layout_count(after), count + 1);
        EXPECT_EQ(layout_count(master_of(*document)), master_count + 1);
    }
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
            // Placement includes system/staff centering and is a visual result.
            ASSERT_TRUE(document.page_count());
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
    ASSERT_TRUE((*opened)->page_count());
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
                    ASSERT_TRUE((*document)->page_count());
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
        ASSERT_TRUE((*document)->page_count());
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
