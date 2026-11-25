#include "engraving/dom/articulation.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/sharedpart.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/rendering/score/layoutdatastore.h"
#include "read_test_fixture.h"

using namespace read_test;

TEST(LayoutDataStore, RepeatedDiscardRebuildPreservesMusicOverridesAndPublishedGeometry) {
    for (const char *fixture : {"tests/data/inputs/golden/chordline_path.mscx", "tests/data/inputs/golden/layout_elements.mscx",
                                "tests/data/inputs/golden/tab_grace_prebend.mscx",
                                "src/engraving/tests/playback/playbackeventsrenderer_data/trill_line_on_tied_notes.mscx",
                                "src/engraving/tests/playback/playbackmodel_data/spanners/spanners.mscx"}) {
        SCOPED_TRACE(fixture);
        auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / fixture);
        ASSERT_TRUE(opened);
        expect_discard_rebuild_parity(**opened, master_of(**opened));
    }
}

TEST(LayoutDataStore, DiscardRebuildPreservesRichTextHarmonyAndExcerptOutput) {
    auto input = legacy_segment_fixture("");
    input.insert(input.find("<Chord>"), R"(<StaffText><text><b>Rich</b> <i>source</i><font size="14"/> text</text></StaffText>
<Harmony><harmonyInfo><root>14</root><name>7(b9)</name></harmonyInfo></Harmony>)");
    auto opened = g_engine->open(as_bytes(input));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    using namespace mu::engraving;
    auto &master = master_of(document);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_TRUE(set_child_override(*chord, {AuthoredChildRole::STEM}, Pid::OFFSET, PropertyValue(muse::PointF(0.25, -0.5)),
                                   PropertyFlags::NOSTYLE));
    ASSERT_FALSE(chord->authored_overrides.entries.empty());
    ASSERT_TRUE(document.page_count());
    Harmony *harmony = nullptr;
    for (auto *object : engrave::document_objects(master)) {
        if (object->is_harmony()) harmony = to_harmony(object);
    }
    ASSERT_NE(harmony, nullptr);
    ASSERT_EQ(harmony->chords().size(), 1u);
    ASSERT_EQ(harmony->chords().front()->root_tpc(), 14);
    ASSERT_FALSE(harmony->ldata()->render_item_list().empty());
    EXPECT_TRUE(std::ranges::any_of(harmony->ldata()->render_item_list(),
                                    [](const auto *item) { return dynamic_cast<const ChordSymbolParen *>(item) != nullptr; }));
    expect_discard_rebuild_parity(document, master_of(document), {}, true);
    const auto outline = document.outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
    const auto parts = document.outline();
    ASSERT_TRUE(parts);
    ASSERT_EQ(parts->excerpts.size(), 1u);
    const engrave::ScoreSel selection{parts->excerpts.front().id};
    auto selected = engrave::select_score(master_of(document), selection);
    ASSERT_TRUE(selected);
    expect_discard_rebuild_parity(document, **selected, selection);
}

TEST(LayoutDataStore, DiscardRetiresHiddenAndMmrSegmentViewsBeforeLedgerShells) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *measure = master.first_measure();
    auto *mmr = Factory::create_measure(&master);
    mmr->set_mm_rest_count(2);
    measure->set_mm_rest(mmr);
    std::unique_ptr<Segment> hidden(Factory::create_segment(measure, SegmentType::CHORD_REST, Fraction(0, 1)));
    hidden->set_visible(false);
    auto *display = Factory::create_segment(mmr, SegmentType::CHORD_REST, Fraction(0, 1));
    mmr->add_output_segment(display);
    std::unique_ptr<Chord> chord(Factory::create_chord(hidden.get()));
    chord->resize_ledger_lines_to(1);
    auto *ledger = chord->ldata()->ledger_lines.front();
    hidden->staff_shape(0).add(Shape(muse::RectF(1, 2, 3, 4), ledger));
    display->staff_shape(0).add(Shape(muse::RectF(1, 2, 3, 4), ledger));
    engrave::DocumentAccess::impl(**opened).discard_layout_output(master);
    EXPECT_EQ(measure->mm_rest(), nullptr);
    EXPECT_TRUE(hidden->staff_shape(0).empty());
    EXPECT_EQ(hidden->min_right(), 0.0);
    EXPECT_EQ(hidden->min_left(), 0.0);
    EXPECT_FALSE(chord->has_layout_data());
}

TEST(LayoutDataStore, LedgerOwnershipGrowthFailureDoesNotAttachAnUnownedShell) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    std::unique_ptr<Chord> chord(Factory::create_chord(master.dummy()));
    chord->resize_ledger_lines_to(1);
    const auto children = chord->children();
    const auto &lines = chord->ldata()->ledger_lines;
    const auto size = lines.size();
    EXPECT_THROW(chord->resize_ledger_lines_to(lines.max_size() + 1), std::length_error);
    EXPECT_EQ(chord->children(), children);
    EXPECT_EQ(lines.size(), size);
    engrave::DocumentAccess::impl(**opened).discard_layout_output(master);
    EXPECT_TRUE(chord->children().empty());
    chord->resize_ledger_lines_to(2);
    EXPECT_EQ(chord->ldata()->ledger_lines.size(), 2u);
}

TEST(LayoutDataStore, ClosedDispatchKeepsColdDefaultsSparseAndPreservesLegacyPayloadProvenance) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    std::unique_ptr<System> system(Factory::create_system(&master));
    auto bagpipe = std::make_unique<BagpipeEmbellishment>(master.dummy());
    std::unique_ptr<GuitarBend> bend(Factory::create_guitar_bend(master.dummy()));
    std::unique_ptr<TimeTickAnchor> anchor(Factory::create_time_tick_anchor(master.dummy()));
    std::unique_ptr<Parenthesis> paren(Factory::create_parenthesis(master.dummy()));
    auto &store = master.renderer()->layout_data_store(master);
    const auto before = store.size();
    EXPECT_TRUE(system->ldata()->group_brackets_width().empty());
    EXPECT_TRUE(system->ldata()->parts_with_group_name().empty());
    EXPECT_TRUE(bagpipe->ldata()->notes_data.empty());
    EXPECT_TRUE(bend->ldata()->dive_levels().empty());
    EXPECT_FALSE(anchor->ldata()->darker());
    EXPECT_TRUE(paren->ldata()->path.value().is_empty());
    const std::array<const EngravingItem *, 5> items{system.get(), bagpipe.get(), bend.get(), anchor.get(), paren.get()};
    for (const auto *item : items) EXPECT_FALSE(item->has_layout_data());
    EXPECT_EQ(store.size(), before);

    std::unique_ptr<MeasureRepeat> repeat(Factory::create_measure_repeat(master.dummy()));
    repeat->hack_to_rest_type();
    EXPECT_EQ(repeat->type(), ElementType::REST);
    const EngravingObject *object = repeat.get();
    EXPECT_EQ(item_cast<const MeasureRepeat *>(object), repeat.get());
    EXPECT_EQ(item_cast<const Rest *>(object), repeat.get());
    EXPECT_EQ(item_cast<const ChordRest *>(object), repeat.get());
    EXPECT_EQ(item_cast<const MMRest *>(object, CastMode::MAYBE_BAD), nullptr);
    EXPECT_EQ(item_cast<const TextBase *>(object, CastMode::MAYBE_BAD), nullptr);
    EXPECT_FALSE(repeat->has_layout_data());
    EXPECT_TRUE(repeat->ldata()->number_sym.empty());
    auto *payload = repeat->mutldata();
    payload->set_number_sym(12); // Must remain a full MeasureRepeat payload after the type hack.
    EXPECT_EQ(payload->number_sym.size(), 2u);
    EXPECT_EQ(store.find(*repeat), payload);
    std::unique_ptr<Rest> slice(Factory::copy_rest(*repeat));
    EXPECT_EQ(item_cast<const MeasureRepeat *>(static_cast<const EngravingObject *>(slice.get()), CastMode::MAYBE_BAD), nullptr);
    EXPECT_FALSE(slice->has_layout_data());
    EXPECT_EQ(slice->ldata()->sym.value(), SymId::REST_QUARTER);
    std::unique_ptr<MMRest> mm_rest(Factory::create_mm_rest(master.dummy()));
    mm_rest->mutldata()->rest_width = 4.5;
    std::unique_ptr<Rest> mm_slice(Factory::copy_rest(*mm_rest));
    mm_slice->hack_to_rest_type();
    EXPECT_FALSE(mm_slice->has_layout_data());
    EXPECT_EQ(mm_rest->ldata()->rest_width.value(), 4.5);

    std::unique_ptr<PartialLyricsLine> partial(Factory::create_partial_lyrics_line(master.dummy()));
    EXPECT_EQ(item_cast<const PartialLyricsLine *>(static_cast<const EngravingObject *>(partial.get())), partial.get());
    std::unique_ptr<LyricsLine> lyrics_slice(Factory::copy_lyrics_line(*partial));
    EXPECT_EQ(item_cast<const LyricsLine *>(static_cast<const EngravingObject *>(lyrics_slice.get())), lyrics_slice.get());
    EXPECT_EQ(item_cast<const PartialLyricsLine *>(static_cast<const EngravingObject *>(lyrics_slice.get()), CastMode::MAYBE_BAD), nullptr);

    std::unique_ptr<Beam> beam(Factory::create_beam(&master));
    std::unique_ptr<TremoloTwoChord> tremolo(Factory::create_tremolo_two_chord(master.dummy()));
    EXPECT_EQ(item_cast<const BeamBase *>(static_cast<const EngravingItem *>(beam.get())), beam.get());
    EXPECT_EQ(item_cast<const BeamBase *>(static_cast<const EngravingItem *>(tremolo.get())), tremolo.get());
    EXPECT_EQ(item_cast<const BeamBase *>(static_cast<const EngravingItem *>(slice.get()), CastMode::MAYBE_BAD), nullptr);

    // Direct base copy outside Factory must still report the constructed base's dynamic type.
    std::unique_ptr<Ornament> ornament(Factory::create_ornament(master.dummy()));
    Articulation direct_slice(*ornament);
    const EngravingObject *sliced_obj = &direct_slice;
    EXPECT_EQ(direct_slice.concrete_type(), ElementType::ARTICULATION);
    EXPECT_EQ(item_cast<const Articulation *>(sliced_obj), &direct_slice);
    EXPECT_EQ(item_cast<const Ornament *>(sliced_obj, CastMode::MAYBE_BAD), nullptr);
    EXPECT_EQ(item_cast<const Ornament *>(static_cast<const EngravingObject *>(ornament.get())), ornament.get());

    // SharedPart derives from Part with ElementType::SHARED_PART.
    auto shared_part = std::make_unique<SharedPart>(&master);
    const EngravingObject *part_obj = shared_part.get();
    EXPECT_EQ(item_cast<const Part *>(part_obj), shared_part.get());
    EXPECT_EQ(item_cast<const SharedPart *>(part_obj), shared_part.get());
}

TEST(LayoutDataStore, ItemCoordinatesKeepSegmentOffsetSemantics) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto *measure = master_of(**opened).first_measure();
    std::unique_ptr<Segment> segment(Factory::create_segment(measure, SegmentType::CHORD_REST, Fraction(0, 1)));
    std::unique_ptr<Chord> chord(Factory::create_chord(segment.get()));
    for (EngravingItem *item : std::array<EngravingItem *, 2>{segment.get(), chord.get()}) {
        item->set_pos(2.0, 3.0);
        item->set_offset(muse::PointF(5.0, 7.0));
        EXPECT_EQ(item->pos(), muse::PointF(7.0, 10.0));
        EXPECT_EQ(item->y(), 10.0);
    }
    EXPECT_EQ(static_cast<EngravingItem *>(segment.get())->x(), 2.0);
    EXPECT_EQ(chord->x(), 7.0);
}

TEST(LayoutDataStore, StablePayloadsAndNonAllocatingTypedReads) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    std::unique_ptr<Note> note(Factory::create_note(chord));
    auto &store = master.renderer()->layout_data_store(master);
    const auto before = store.size();
    EXPECT_FALSE(note->has_layout_data());
    EXPECT_EQ(note->ldata()->pos(LdAccess::MAYBE_NOTINITED), muse::PointF());
    EXPECT_EQ(store.size(), before);
    auto *payload = note->mutldata();
    payload->set_pos(2.5, -1.25);
    std::vector<std::unique_ptr<Note>> growth;
    for (int i = 0; i < 512; ++i) {
        growth.emplace_back(Factory::create_note(chord));
        growth.back()->mutldata()->set_pos(i, i + 1);
    }
    EXPECT_EQ(note->ldata(), payload);
    EXPECT_EQ(store.find(*note), payload);
    growth.clear();
    EXPECT_EQ(store.size(), before + 1);
    note.reset();
    EXPECT_EQ(store.size(), before);
    auto &impl = engrave::DocumentAccess::impl(document);
    impl.adopt_layout_output(master);
    EXPECT_TRUE(std::ranges::any_of(impl.layout_outputs, [&](const auto &output) { return output.get() == &store; }));
}

TEST(LayoutDataStore, TeardownClearsBorrowersBeforeItemDestruction) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    std::unique_ptr<Note> note(Factory::create_note(chord));
    {
        rendering::score::LayoutDataStore scratch(master);
        scratch.adopt(*note, std::make_unique<Note::LayoutData>());
        ASSERT_TRUE(note->has_layout_data());
        note->mutldata()->set_pos(3.25, 4.5);
    }
    EXPECT_FALSE(note->has_layout_data());
    EXPECT_EQ(note->ldata()->pos(LdAccess::MAYBE_NOTINITED), muse::PointF());
    // Destruction after its output owner has gone must not follow a stale borrow.
    note.reset();
}

TEST(LayoutDataStore, GeneratedExcerptOutputIsDocumentOwnedBeforeRendering) {
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto outline = document.outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
    auto &master = master_of(document);
    ASSERT_EQ(master.excerpts().size(), 1u);
    auto *part = master.excerpts().front()->excerpt_score();
    auto *store = part->layout_data_store();
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(part->layout_state.whole_count + part->layout_state.range_count, 0u);
    const auto &outputs = engrave::DocumentAccess::impl(document).layout_outputs;
    EXPECT_TRUE(std::ranges::any_of(outputs, [store](const auto &output) { return output.get() == store; }));
}

TEST(LayoutDataStore, RecursiveLedgerTeardownRetiresEveryPayload) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto &store = master.renderer()->layout_data_store(master);
    const auto before = store.size();
    std::unique_ptr<Chord> chord(Factory::create_chord(master.dummy()));
    auto *ledger = new LedgerLine(chord.get());
    chord->mutldata()->ledger_lines.push_back(ledger);
    ledger->mutldata()->set_pos(1.25, 2.5);
    chord.reset();
    EXPECT_EQ(store.size(), before);
}

TEST(LayoutDataStore, HarmonyOutputOwnsRenderedChildrenAndColdDestructionDoesNotAllocate) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto &store = master.renderer()->layout_data_store(master);
    const auto before = store.size();
    std::unique_ptr<Harmony> harmony(Factory::create_harmony(master.dummy()));
    EXPECT_FALSE(harmony->has_layout_data());
    // An allocate-then-erase destructor leaves size unchanged, so observe the
    // explicit allocation boundary while the cold item is being destroyed.
    size_t allocation_requests = 0;
    auto boundary = std::move(master.layout_boundaries->store);
    master.layout_boundaries->store = [&](Score &score) -> rendering::score::LayoutDataStore & {
        ++allocation_requests;
        return boundary(score);
    };
    harmony.reset();
    master.layout_boundaries->store = std::move(boundary);
    EXPECT_EQ(allocation_requests, 0u);
    EXPECT_EQ(store.size(), before);
    harmony.reset(Factory::create_harmony(master.dummy()));
    auto *output = harmony->mutldata();
    {
        HarmonyRenderItems abandoned;
        auto *paren = Factory::create_parenthesis(harmony.get());
        paren->mutldata()->set_pos(1.25, 2.5);
        abandoned.adopt(std::make_unique<ChordSymbolParen>(paren, false, 0.0, 0.0));
    }
    EXPECT_EQ(store.size(), before + 1);
    HarmonyRenderItems scratch;
    auto *paren = Factory::create_parenthesis(harmony.get());
    paren->mutldata()->set_pos(3.0, 4.0);
    auto *rendered = new ChordSymbolParen(paren, false, 0.0, 0.0);
    scratch.adopt(std::unique_ptr<HarmonyRenderItem>(rendered));
    output->render_item_list.append(std::move(scratch));
    EXPECT_TRUE(scratch.empty());
    ASSERT_EQ(output->render_item_list.value().size(), 1u);
    EXPECT_EQ(output->render_item_list.front(), rendered);
    store.erase(*harmony);
    EXPECT_EQ(store.size(), before);
    EXPECT_FALSE(harmony->has_layout_data());
    harmony.reset();
    EXPECT_EQ(store.size(), before);
    // The ordinary DOM-destruction order must also release live output-owned
    // children while the parent source shell is still available.
    harmony.reset(Factory::create_harmony(master.dummy()));
    paren = Factory::create_parenthesis(harmony.get());
    paren->mutldata()->set_pos(5.0, 6.0);
    harmony->mutldata()->render_item_list.adopt(std::make_unique<ChordSymbolParen>(paren, false, 0.0, 0.0));
    harmony.reset();
    EXPECT_EQ(store.size(), before);
}
