#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "engraving/dom/accidental.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/sharedpart.h"
#include "engraving/dom/tapping.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/rendering/score/beamlayout.h"
#include "engraving/rendering/score/chordlayout.h"
#include "engraving/rendering/score/headerfooterlayout.h"
#include "engraving/rendering/score/layoutcontext.h"
#include "engraving/rendering/score/layoutdatastore.h"
#include "engraving/rendering/score/passresetlayoutdata.h"
#include "engraving/rendering/score/tlayout.h"
#include "global/slab_pool.h"
#include "read_test_fixture.h"

using namespace read_test;

namespace {
struct alignas(8) SlabValue {
    std::size_t value;
};

struct alignas(8) ThrowingSlabValue {
    explicit ThrowingSlabValue(bool fail) {
        if (fail) throw std::runtime_error("slot construction failed");
    }
};

struct ReentrantSlabValue;
using ReentrantSlabPool = muse::SlabPool<ReentrantSlabValue, int, 16>;

struct ReentrantSlabState {
    ReentrantSlabPool *pool;
    int owner = 0;
    unsigned destroyed = 0;
    std::uint32_t first_spawned_slot = 0;
};

struct ReentrantSlabValue {
    ReentrantSlabState *state;
    bool spawn;

    ~ReentrantSlabValue() {
        ++state->destroyed;
        if (!spawn) return;
        state->first_spawned_slot = state->pool->emplace(&state->owner, state, false).first;
        for (int i = 1; i < 256; ++i) state->pool->emplace(&state->owner, state, false);
    }
};
} // namespace

TEST(LayoutDataSlabPool, GrowthKeepsPayloadsStableAndVisitsOnlyLiveSlots) {
    std::array<std::size_t, 300> owners{};
    std::array<SlabValue *, owners.size()> payloads{};
    muse::SlabPool<SlabValue, std::size_t, 64> pool;
    for (std::size_t i = 0; i < owners.size(); ++i) {
        owners[i] = i;
        const auto [slot, data] = pool.emplace(&owners[i]);
        ASSERT_EQ(slot, i);
        EXPECT_EQ(data->value, 0u);
        data->value = i + 1000;
        payloads[i] = data;
    }
    EXPECT_EQ(pool.live_count(), owners.size());

    auto *dying = pool.detach(0);
    EXPECT_EQ(dying, payloads[0]);
    EXPECT_EQ(pool.live_count(), owners.size() - 1);
    EXPECT_FALSE(pool.release_all_chunks());
    auto [fresh_slot, fresh] = pool.emplace(&owners[0], 1000u);
    EXPECT_EQ(fresh_slot, owners.size());
    EXPECT_NE(fresh, dying);
    pool.finish_destroy(0, dying);
    auto [reused_slot, reused] = pool.emplace(&owners[0], 1000u);
    EXPECT_EQ(reused_slot, 0u);
    EXPECT_EQ(reused, payloads[0]);
    pool.finish_destroy(fresh_slot, pool.detach(fresh_slot));

    std::size_t visited = 0;
    pool.for_each_live([&](std::size_t &owner, SlabValue &data) {
        ASSERT_LT(owner, owners.size());
        EXPECT_EQ(&owner, &owners[owner]);
        EXPECT_EQ(&data, payloads[owner]);
        EXPECT_EQ(data.value, owner + 1000);
        ++visited;
    });
    EXPECT_EQ(visited, owners.size());
    for (std::uint32_t slot = 0; slot < owners.size(); ++slot) pool.finish_destroy(slot, pool.detach(slot));
    EXPECT_TRUE(pool.empty());
    EXPECT_TRUE(pool.release_all_chunks());
    const auto [first_slot, first] = pool.emplace(&owners[0]);
    EXPECT_EQ(first_slot, 0u);
    EXPECT_EQ(first->value, 0u);
}

TEST(LayoutDataSlabPool, FailedConstructionLeavesNoLiveSlotAndPreservesReuse) {
    int owner = 0;
    muse::SlabPool<ThrowingSlabValue, int, 16> pool;
    EXPECT_THROW(pool.emplace(&owner, true), std::runtime_error);
    EXPECT_TRUE(pool.empty());
    const auto [first_slot, first] = pool.emplace(&owner, false);
    EXPECT_EQ(first_slot, 0u);
    const auto [second_slot, second] = pool.emplace(&owner, false);
    EXPECT_EQ(second_slot, 1u);
    pool.finish_destroy(second_slot, pool.detach(second_slot));
    EXPECT_THROW(pool.emplace(&owner, true), std::runtime_error);
    EXPECT_EQ(pool.live_count(), 1u);
    const auto [reused_slot, reused] = pool.emplace(&owner, false);
    EXPECT_EQ(reused_slot, second_slot);
    EXPECT_EQ(reused, second);
    EXPECT_EQ(pool.detach(first_slot), first);
    pool.finish_destroy(first_slot, first);
}

TEST(LayoutDataSlabPool, FinishDestroyCanGrowPoolBeforePublishingDetachedSlot) {
    ReentrantSlabState state{};
    {
        ReentrantSlabPool pool;
        state.pool = &pool;
        const auto slot = pool.emplace(&state.owner, &state, true).first;
        pool.finish_destroy(slot, pool.detach(slot));
        EXPECT_EQ(state.destroyed, 1u);
        EXPECT_EQ(state.first_spawned_slot, 1u);
        EXPECT_EQ(pool.live_count(), 256u);
        EXPECT_EQ(pool.emplace(&state.owner, &state, false).first, slot);
    }
    EXPECT_EQ(state.destroyed, 258u);
}

TEST(LayoutDataSlabPool, DestructorDrainsSlotsEmplacedByPayloadDestructors) {
    ReentrantSlabState state{};
    {
        ReentrantSlabPool pool;
        state.pool = &pool;
        for (int i = 0; i < 64; ++i) pool.emplace(&state.owner, &state, i == 63);
    }
    EXPECT_EQ(state.first_spawned_slot, 62u);
    EXPECT_EQ(state.destroyed, 320u);
}

TEST(LayoutDataSlabPool, VisitsSkipEmptyLeadingChunkAndKeepTheHighBitLive) {
    std::array<std::uint32_t, 130> owners{};
    muse::SlabPool<SlabValue, std::uint32_t, 64> pool;
    for (std::uint32_t i = 0; i < owners.size(); ++i) {
        owners[i] = i;
        pool.emplace(&owners[i], i + 1000u);
    }
    for (std::uint32_t i = 0; i < 63; ++i) pool.finish_destroy(i, pool.detach(i));
    std::vector<std::uint32_t> chunk0_tail;
    pool.for_each_live([&](std::uint32_t &owner, SlabValue &) {
        if (owner < 64) chunk0_tail.push_back(owner);
    });
    EXPECT_EQ(chunk0_tail, (std::vector<std::uint32_t>{63}));
    pool.finish_destroy(63, pool.detach(63));
    for (std::uint32_t i = 65; i < 127; ++i) pool.finish_destroy(i, pool.detach(i));

    const std::vector<std::uint32_t> expected{64, 127, 128, 129};
    std::vector<std::uint32_t> visited;
    pool.for_each_live([&](std::uint32_t &owner, SlabValue &data) {
        EXPECT_EQ(data.value, owner + 1000u);
        visited.push_back(owner);
    });
    EXPECT_EQ(visited, expected);
    visited.clear();
    bool released_in_final_callback = false;
    pool.drain_owners([&](std::uint32_t &owner) {
        visited.push_back(owner);
        pool.finish_destroy(owner, pool.detach(owner));
        if (pool.empty()) released_in_final_callback = pool.release_all_chunks();
    });
    EXPECT_EQ(visited, expected);
    EXPECT_TRUE(released_in_final_callback);
    EXPECT_TRUE(pool.empty());
}

TEST(LayoutDataSlabPool, ReleaseAllChunksDuringConstructionIsRejected) {
    struct ReleasingSlabValue;
    using Pool = muse::SlabPool<ReleasingSlabValue, int, 16>;

    struct alignas(8) ReleasingSlabValue {
        std::size_t tag = 0;

        ReleasingSlabValue(Pool &pool, bool &released_during_ctor, bool fail, std::size_t t) : tag(t) {
            released_during_ctor = pool.release_all_chunks();
            if (fail) throw std::runtime_error("ctor failed after release attempt");
        }
    };

    int owner = 0;
    Pool pool;
    bool released = true;
    EXPECT_THROW(pool.emplace(&owner, pool, released, true, 11u), std::runtime_error);
    EXPECT_FALSE(released);
    EXPECT_TRUE(pool.empty());
    EXPECT_TRUE(pool.release_all_chunks());

    released = true;
    const auto [slot, ptr] = pool.emplace(&owner, pool, released, false, 42u);
    EXPECT_FALSE(released);
    EXPECT_EQ(slot, 0u);
    EXPECT_EQ(ptr->tag, 42u);
    EXPECT_FALSE(pool.release_all_chunks());
    pool.finish_destroy(slot, pool.detach(slot));
    EXPECT_TRUE(pool.release_all_chunks());
}

TEST(LayoutDataStore, ResetPassClearsArticulationFamilyOverrides) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_NE(chord, nullptr);
    const std::array<Articulation *, 3> articulations{Factory::create_articulation(chord), Factory::create_ornament(chord),
                                                      Factory::create_tapping(chord)};
    for (auto *articulation : articulations) {
        chord->add(articulation);
        auto *data = articulation->mutldata();
        data->up.set_value(false);
        data->sym_id.set_value(SymId::ARTIC_STACCATO_ABOVE);
        data->set_bbox(0, 0, 5, 5);
    }

    LayoutContext ctx(&master);
    ctx.mut_state().set_is_layout_all(true);
    PassResetLayoutData{}.run(&master, ctx);

    for (const auto *articulation : articulations) {
        SCOPED_TRACE(static_cast<int>(articulation->type()));
        EXPECT_FALSE(articulation->ldata()->up.has_value());
        EXPECT_EQ(articulation->ldata()->up.value(LdAccess::MAYBE_NOTINITED), true);
        EXPECT_FALSE(articulation->ldata()->sym_id.has_value());
        EXPECT_EQ(articulation->ldata()->sym_id.value(LdAccess::MAYBE_NOTINITED), SymId::NO_SYM);
        EXPECT_FALSE(articulation->ldata()->is_set_shape());
        EXPECT_FALSE(articulation->ldata()->is_valid());
    }
}

TEST(LayoutDataStore, AccidentalValidityRequiresBothShapeAndSymbols) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    std::unique_ptr<Accidental> accidental(Factory::create_accidental(chord->up_note()));
    accidental->set_accidental_type(AccidentalType::SHARP);
    accidental->set_bracket(AccidentalBracket::PARENTHESIS);
    auto *data = accidental->mutldata();
    EXPECT_FALSE(data->is_valid());
    data->set_bbox(0, 0, 5, 5);
    EXPECT_FALSE(data->is_valid());

    LayoutContext ctx(&master);
    TLayout::layout_item(accidental.get(), ctx);
    if (data->syms.size() == 1) {
        EXPECT_EQ(data->syms.front().sym, SymId::ACCIDENTAL_SHARP_PARENS);
    } else {
        ASSERT_EQ(data->syms.size(), 3u);
        EXPECT_EQ(data->syms[0].sym, SymId::ACCIDENTAL_PARENS_LEFT);
        EXPECT_EQ(data->syms[1].sym, SymId::ACCIDENTAL_SHARP);
        EXPECT_EQ(data->syms[2].sym, SymId::ACCIDENTAL_PARENS_RIGHT);
    }
    EXPECT_TRUE(data->is_valid());
    data->clear_shape();
    EXPECT_FALSE(data->is_valid());
}

TEST(LayoutDataStore, BeamAndTremoloValidityDependsOnBeamType) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    std::unique_ptr<Beam> beam(Factory::create_beam(&master));
    std::unique_ptr<TremoloTwoChord> tremolo(Factory::create_tremolo_two_chord(master.dummy()));
    const auto verify = [](BeamBase &item, BeamType type) {
        EXPECT_FALSE(item.ldata()->is_valid());
        auto *data = item.mutldata();
        data->set_bbox(0, 0, 5, 5);
        EXPECT_FALSE(data->is_valid());
        data->clear_shape();
        data->beam_type = type;
        EXPECT_TRUE(item.ldata()->is_valid());
    };
    verify(*beam, BeamType::BEAM);
    verify(*tremolo, BeamType::TREMOLO);

    LayoutContext ctx(&master);
    const RectF sentinel_bbox(0, 0, 5, 5);
    beam->mutldata()->set_bbox(sentinel_bbox);
    BeamLayout::layout_if_need(beam.get(), ctx);
    EXPECT_EQ(beam->ldata()->bbox(), sentinel_bbox);
    beam->mutldata()->beam_type = BeamType::INVALID;
    BeamLayout::layout_if_need(beam.get(), ctx);
    EXPECT_NE(beam->ldata()->bbox(), sentinel_bbox);

    master.ensure_layout();
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    const auto anchor_y = std::max(chord->up_note()->page_pos().y(), chord->down_note()->page_pos().y()) + 100 * chord->spatium();
    tremolo->set_start_anchor({1, anchor_y});
    tremolo->set_end_anchor({2, anchor_y});
    EXPECT_FALSE(ChordLayout::is_chord_pos_below_trem(chord, tremolo.get()));
    tremolo->mutldata()->beam_type = BeamType::INVALID;
    EXPECT_TRUE(ChordLayout::is_chord_pos_below_trem(chord, tremolo.get()));
}

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
    EXPECT_EQ(repeat->type(), ElementType::MEASURE_REPEAT);
    const EngravingObject *object = repeat.get();
    EXPECT_EQ(item_cast<const MeasureRepeat *>(object), repeat.get());
    EXPECT_EQ(item_cast<const Rest *>(object), repeat.get());
    EXPECT_EQ(item_cast<const ChordRest *>(object), repeat.get());
    EXPECT_EQ(item_cast<const MMRest *>(object, CastMode::MAYBE_BAD), nullptr);
    EXPECT_EQ(item_cast<const TextBase *>(object, CastMode::MAYBE_BAD), nullptr);
    EXPECT_FALSE(repeat->has_layout_data());
    EXPECT_TRUE(repeat->ldata()->number_sym.empty());
    auto *payload = repeat->mutldata();
    payload->set_number_sym(12);
    EXPECT_EQ(payload->number_sym.size(), 2u);
    EXPECT_EQ(store.find(*repeat), payload);
    std::unique_ptr<Rest> slice(Factory::copy_rest(*repeat));
    EXPECT_EQ(slice->type(), ElementType::REST);
    EXPECT_EQ(item_cast<const MeasureRepeat *>(static_cast<const EngravingObject *>(slice.get()), CastMode::MAYBE_BAD), nullptr);
    EXPECT_FALSE(slice->has_layout_data());
    EXPECT_EQ(slice->ldata()->sym.value(), SymId::REST_QUARTER);
    std::unique_ptr<MMRest> mm_rest(Factory::create_mm_rest(master.dummy()));
    mm_rest->mutldata()->rest_width = 4.5;
    std::unique_ptr<Rest> mm_slice(Factory::copy_rest(*mm_rest));
    EXPECT_EQ(mm_slice->type(), ElementType::REST);
    EXPECT_EQ(item_cast<const MMRest *>(static_cast<const EngravingObject *>(mm_slice.get()), CastMode::MAYBE_BAD), nullptr);
    EXPECT_FALSE(mm_slice->has_layout_data());
    EXPECT_EQ(mm_rest->ldata()->rest_width.value(), 4.5);

    std::unique_ptr<PartialLyricsLine> partial(Factory::create_partial_lyrics_line(master.dummy()));
    EXPECT_EQ(item_cast<const PartialLyricsLine *>(static_cast<const EngravingObject *>(partial.get())), partial.get());
    std::unique_ptr<LyricsLine> lyrics_slice(Factory::copy_lyrics_line(*partial));
    EXPECT_EQ(lyrics_slice->type(), ElementType::LYRICSLINE);
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
    EXPECT_EQ(direct_slice.type(), ElementType::ARTICULATION);
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

TEST(LayoutDataStore, EmptyMacrosRetireBorrowersOnlyForPublishedHeadersAndFooters) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    master.style().set(Sid::SHOW_HEADER, true);
    master.style().set(Sid::SHOW_FOOTER, true);
    master.style().set(Sid::HEADER_FIRST_PAGE, true);
    master.style().set(Sid::FOOTER_FIRST_PAGE, true);
    master.style().set(Sid::HEADER_ODD_EVEN, false);
    master.style().set(Sid::FOOTER_ODD_EVEN, false);
    master.style().set(Sid::ODD_HEADER_C, muse::String(u"Header"));
    master.style().set(Sid::ODD_FOOTER_C, muse::String(u"Footer"));
    master.set_layout_all();
    master.ensure_layout();

    LayoutContext ctx(&master);
    auto *page = master.pages().front();
    auto *header = page->header_text(1);
    auto *footer = page->footer_text(1);
    ASSERT_NE(header, nullptr);
    ASSERT_NE(footer, nullptr);
    const auto items = page->items(page->page_bounding_rect());
    ASSERT_NE(std::find(items.begin(), items.end(), header), items.end());
    ASSERT_NE(std::find(items.begin(), items.end(), footer), items.end());

    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    auto &shape = segment->staff_shape(0);
    const auto shape_size = shape.size();
    shape.add(muse::RectF(1, 1, 1, 1), header);
    shape.add(muse::RectF(2, 2, 1, 1), footer);

    const auto empty_macro = muse::String(u"$:missingHeaderFooterTag:");
    master.style().set(Sid::ODD_HEADER_C, empty_macro);
    master.style().set(Sid::ODD_FOOTER_C, empty_macro);
    auto &store = master.renderer()->layout_data_store(master);
    const auto after_removal = store.size() - 2;
    HeaderFooterLayout::layout_header_footer(ctx, page);
    EXPECT_EQ(page->header_text(1), nullptr);
    EXPECT_EQ(page->footer_text(1), nullptr);
    EXPECT_EQ(shape.size(), shape_size);
    EXPECT_EQ(store.size(), after_removal);
    const auto after_items = page->items(page->page_bounding_rect());
    EXPECT_EQ(std::find(after_items.begin(), after_items.end(), header), after_items.end());
    EXPECT_EQ(std::find(after_items.begin(), after_items.end(), footer), after_items.end());

    // Re-running empty macros disposes unpublished scratch text cleanly without leaking slab slots.
    HeaderFooterLayout::layout_header_footer(ctx, page);
    EXPECT_EQ(page->header_text(1), nullptr);
    EXPECT_EQ(page->footer_text(1), nullptr);
    EXPECT_EQ(store.size(), after_removal);
    EXPECT_FALSE(page->items(page->page_bounding_rect()).empty());
}

TEST(LayoutDataStore, CrossStaffGraceRefreshClearsDestinationStaffAfterGraceScopeEnds) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    constexpr std::string_view two_staff_mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff id="1"/><Staff id="2"/><Instrument id="piano"><trackName>Piano</trackName></Instrument></Part>
<Staff id="1"><Measure><voice>
  <Chord><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
  <Chord><durationType>quarter</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
</voice></Measure></Staff>
<Staff id="2"><Measure><voice>
  <Chord><durationType>quarter</durationType><Note><pitch>48</pitch><tpc>14</tpc></Note></Chord>
  <Chord><durationType>quarter</durationType><Note><pitch>50</pitch><tpc>16</tpc></Note></Chord>
</voice></Measure></Staff>
</Score></museScore>)";
    auto opened = g_engine->open(as_bytes(two_staff_mscx));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    ASSERT_EQ(master.nstaves(), 2u);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    ASSERT_NE(segment, nullptr);
    auto *main = to_chord(segment->element(4));
    ASSERT_NE(main, nullptr);
    auto *grace = add_test_grace(main, NoteType::ACCIACCATURA);
    grace->set_staff_move(-1);
    ASSERT_EQ(grace->staff_idx(), 1u);
    ASSERT_EQ(grace->v_staff_idx(), 0u);

    master.set_layout_all();
    master.ensure_layout();
    ASSERT_FALSE(segment->has_pre_appended_items());
    ASSERT_TRUE(segment->has_pre_appended_shapes());

    const auto contains_grace_note = [](const Shape &shape, const Chord *target) {
        const auto *note = target->up_note();
        return std::ranges::any_of(shape.elements(), [&](const ShapeElement &el) { return el.item() == note || el.item() == target; });
    };
    ASSERT_TRUE(contains_grace_note(segment->staff_shape(0), grace));

    // Refresh after the LayoutContext has already cleared pre_appended_items_:
    // destination staff 0 must also be rebuilt instead of retaining stale grace geometry.
    ChordLayout::refresh_shape(grace);
    EXPECT_FALSE(contains_grace_note(segment->staff_shape(0), grace));
    EXPECT_FALSE(segment->has_pre_appended_shapes());
}

TEST(LayoutDataStore, ClonedSegmentInitializesPreAppendedSlotsAndUpdatesCountOnStaffRemoval) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    constexpr std::string_view two_staff_mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff id="1"/><Staff id="2"/><Instrument id="piano"><trackName>Piano</trackName></Instrument></Part>
<Staff id="1"><Measure><voice>
  <Chord><durationType>whole</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
</voice></Measure></Staff>
<Staff id="2"><Measure><voice>
  <Chord><durationType>whole</durationType><Note><pitch>48</pitch><tpc>14</tpc></Note></Chord>
</voice></Measure></Staff>
</Score></museScore>)";
    auto opened = g_engine->open(as_bytes(two_staff_mscx));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    ASSERT_NE(segment, nullptr);
    auto *chord = to_chord(segment->element(0));
    ASSERT_NE(chord, nullptr);
    add_test_grace(chord, NoteType::ACCIACCATURA);

    std::unique_ptr<Segment> cloned;
    {
        LayoutContext ctx(&master);
        ChordLayout::append_grace_notes(chord, ctx);
        ASSERT_TRUE(segment->has_pre_appended_items());
        cloned.reset(segment->clone());
    }
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->pre_appended_items().size(), cloned->elist().size());
    EXPECT_FALSE(cloned->has_pre_appended_items());

    cloned->pre_append(chord, 0);
    EXPECT_TRUE(cloned->has_pre_appended_items());
    cloned->pre_append(chord, 0);
    EXPECT_TRUE(cloned->has_pre_appended_items());
    cloned->clear_pre_appended(0);
    EXPECT_FALSE(cloned->has_pre_appended_items());

    cloned->pre_append(chord, 0);
    ASSERT_TRUE(cloned->has_pre_appended_items());
    cloned->remove_staff(0);
    EXPECT_FALSE(cloned->has_pre_appended_items());
    EXPECT_EQ(cloned->pre_appended_items().size(), cloned->elist().size());
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
        scratch.mut_data(*note);
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

TEST(LayoutDataStore, RecursiveMeasureTeardownPreservesLiveSiblingAndReusesSlots) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    rendering::score::LayoutDataStore scratch(master);
    std::unique_ptr<Measure> outer(Factory::create_measure(&master));
    std::unique_ptr<Measure> sibling(Factory::create_measure(&master));
    auto *mm_rest = Factory::create_measure(&master);
    const auto *outer_data = scratch.mut_data(*outer);
    const auto *mm_rest_data = scratch.mut_data(*mm_rest);
    const auto *sibling_data = scratch.mut_data(*sibling);
    outer->set_mm_rest(mm_rest);
    sibling->mutldata()->set_pos(3.0, 4.0);
    ASSERT_EQ(scratch.size(), 3u);

    scratch.erase(*outer);
    EXPECT_FALSE(outer->has_layout_data());
    EXPECT_EQ(scratch.find(*outer), nullptr);
    EXPECT_EQ(scratch.size(), 1u);
    EXPECT_EQ(scratch.find(*sibling), sibling_data);
    EXPECT_EQ(sibling->ldata()->pos(), muse::PointF(3.0, 4.0));

    std::unique_ptr<Measure> replacement(Factory::create_measure(&master));
    EXPECT_EQ(scratch.mut_data(*outer), outer_data);
    EXPECT_EQ(scratch.mut_data(*replacement), mm_rest_data);
    EXPECT_EQ(outer->mm_rest(), nullptr);
    EXPECT_EQ(scratch.size(), 3u);
    scratch.clear();
    EXPECT_EQ(scratch.size(), 0u);
    EXPECT_FALSE(sibling->has_layout_data());
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

TEST(LayoutDataStore, LdFieldAndItemPresenceTrackStateUniformly) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;

    ld_field<double> scalar = {"scalar", 1.5};
    EXPECT_FALSE(scalar.has_value());
    EXPECT_DOUBLE_EQ(scalar.value(LdAccess::MAYBE_NOTINITED), 1.5);
    scalar = 4.25;
    EXPECT_TRUE(scalar.has_value());
    EXPECT_DOUBLE_EQ(scalar.value(), 4.25);
    scalar.reset();
    EXPECT_FALSE(scalar.has_value());
    EXPECT_DOUBLE_EQ(scalar.value(LdAccess::MAYBE_NOTINITED), 1.5);

    ld_field<Shape> shape_field = {"shape", Shape()};
    EXPECT_FALSE(shape_field.has_value());
    shape_field.mut_value().add(RectF(0, 0, 2, 3));
    EXPECT_TRUE(shape_field.has_value());
    EXPECT_FALSE(shape_field.value().empty());
    shape_field.reset();
    EXPECT_FALSE(shape_field.has_value());
    EXPECT_TRUE(shape_field.value(LdAccess::MAYBE_NOTINITED).empty());

    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_NE(chord, nullptr);

    // Creating an Articulation initializes its element style and marks layout dirty;
    // dirty layout data falls back to authored direction_ even when up is set.
    std::unique_ptr<Articulation> art(Factory::create_articulation(chord));
    art->set_direction(DirectionV::DOWN);
    {
        auto *pre_ld = art->mutldata();
        pre_ld->up = true;
        ASSERT_FALSE(art->is_layout_data_current());
        EXPECT_FALSE(art->up());
        master.layout_data_store()->erase(*art);
    }

    master.ensure_layout();

    auto *art_ld = art->mutldata();
    ASSERT_TRUE(art->is_layout_data_current());
    EXPECT_FALSE(art_ld->is_set_pos());
    EXPECT_FALSE(art_ld->is_set_shape());
    EXPECT_FALSE(art_ld->is_set_bbox());
    EXPECT_FALSE(art_ld->is_set_mask());
    EXPECT_FALSE(art_ld->up.has_value());
    // Unset ldata()->up must fall back to authored direction_ instead of reading the default true.
    EXPECT_FALSE(art->up());

    art_ld->set_pos(PointF(2.0, 3.0));
    art_ld->set_bbox(0.0, 0.0, 4.0, 5.0);
    art_ld->set_mask(Shape(RectF(0.0, 0.0, 1.0, 1.0)));
    art_ld->up = true;
    EXPECT_TRUE(art_ld->is_set_pos());
    EXPECT_TRUE(art_ld->is_set_shape());
    EXPECT_TRUE(art_ld->is_set_mask());
    EXPECT_TRUE(art->up());

    art_ld->reset();
    EXPECT_TRUE(art_ld->is_set_pos());
    EXPECT_FALSE(art_ld->is_set_shape());
    EXPECT_FALSE(art_ld->is_set_mask());
    EXPECT_FALSE(art_ld->up.has_value());
    EXPECT_TRUE(art_ld->up.value(LdAccess::MAYBE_NOTINITED));
    EXPECT_FALSE(art->up());

    std::unique_ptr<Harmony> harmony(Factory::create_harmony(master.dummy()));
    auto *harm_ld = harmony->mutldata();
    auto *paren = Factory::create_parenthesis(harmony.get());
    harm_ld->render_item_list.adopt(std::make_unique<ChordSymbolParen>(paren, false, 0.0, 0.0));
    EXPECT_FALSE(harm_ld->baseline.has_value());
    EXPECT_DOUBLE_EQ(harmony->base_line(), static_cast<const TextBase *>(harmony.get())->base_line());
    harm_ld->baseline = 12.5;
    EXPECT_TRUE(harm_ld->baseline.has_value());
    EXPECT_DOUBLE_EQ(harmony->base_line(), 12.5);
}

TEST(LayoutDataStore, SingleOwnerChildrenAndBorrowerFastPathStaySound) {
    using namespace mu::engraving;
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/slurs.mscz");
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));

    // No layout-owned child may appear in any source object's children_ vector.
    const auto verify_source_children = [&](auto &&self, const EngravingObject &owner) -> void {
        for (const auto *child : owner.children()) {
            EXPECT_FALSE(child->is_layout_owned()) << owner.type_name() << " -> " << child->type_name();
            self(self, *child);
        }
    };
    verify_source_children(verify_source_children, master);
    verify_source_children(verify_source_children, *master.dummy());

    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_NE(chord, nullptr);
    EXPECT_TRUE(chord->stem() == nullptr || chord->stem()->is_layout_owned());
    EXPECT_TRUE(std::ranges::none_of(chord->children(), [](const auto *c) { return c->is_layout_owned(); }));

    // Unborrowed items skip borrower sweeps; borrowed items are pruned cleanly from foreign shapes & masks.
    auto &store = *master.layout_data_store();
    std::unique_ptr<Articulation> unborrowed(Factory::create_articulation(master.dummy()));
    EXPECT_FALSE(unborrowed->is_borrowed_in_layout());
    unborrowed->mutldata()->set_bbox(0.0, 0.0, 2.0, 2.0);
    EXPECT_FALSE(unborrowed->is_borrowed_in_layout());
    unborrowed.reset();

    std::unique_ptr<Articulation> borrowed_shape(Factory::create_articulation(master.dummy()));
    std::unique_ptr<Articulation> borrowed_mask(Factory::create_articulation(master.dummy()));
    std::unique_ptr<Articulation> holder(Factory::create_articulation(master.dummy()));
    Shape composite(Shape::Type::COMPOSITE);
    composite.add(RectF(0.0, 0.0, 3.0, 3.0), borrowed_shape.get());
    holder->mutldata()->set_shape(composite);
    holder->mutldata()->set_mask(Shape(RectF(1.0, 1.0, 2.0, 2.0), borrowed_mask.get()));
    EXPECT_TRUE(borrowed_shape->is_borrowed_in_layout());
    EXPECT_TRUE(borrowed_mask->is_borrowed_in_layout());

    borrowed_shape.reset();
    borrowed_mask.reset();
    EXPECT_TRUE(holder->ldata()->shape().empty());
    EXPECT_TRUE(holder->ldata()->mask().empty());

    // HashMap layout_copies_ tracks registration, remapping, and retirement accurately.
    std::unique_ptr<Articulation> src(Factory::create_articulation(master.dummy()));
    std::unique_ptr<Articulation> copy1(Factory::create_articulation(master.dummy()));
    std::unique_ptr<Articulation> copy2(Factory::create_articulation(master.dummy()));
    store.register_layout_copy(*src, *copy1);
    EXPECT_EQ(store.layout_copy(*src), copy1.get());
    EXPECT_TRUE(copy1->is_layout_owned());
    store.register_layout_copy(*copy1, *copy2);
    EXPECT_EQ(store.layout_copy(*src), copy2.get());
    EXPECT_EQ(store.layout_copy(*copy1), copy2.get());
    store.retire_layout_copy(*copy2);
    EXPECT_EQ(store.layout_copy(*src), nullptr);
    EXPECT_EQ(store.layout_copy(*copy1), nullptr);

    // LayoutContext teardown must clear grace-group pre-append links on layout-owned output segments.
    auto *measure = master.first_measure();
    ASSERT_NE(measure, nullptr);
    auto *output_seg = Factory::create_segment(measure, SegmentType::END_BAR_LINE, measure->ticks());
    measure->add_output_segment(output_seg);
    ASSERT_TRUE(output_seg->is_layout_owned());
    {
        rendering::score::LayoutContext ctx(&master);
        auto &group = ctx.update_grace_group(chord, true);
        output_seg->pre_append(&group, 0);
        ASSERT_EQ(output_seg->pre_appended_item(0), &group);
    }
    EXPECT_EQ(output_seg->pre_appended_item(0), nullptr);
}
