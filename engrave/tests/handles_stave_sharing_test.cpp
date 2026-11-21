#include "engrave/src/score/score_metadata.h"
#include "engrave/src/score/score_semantic.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/stavesharingchange.h"
#include "engraving/dom/systemlockindicator.h"
#include "handles_test_fixture.h"

using namespace handles_test;

TEST(StaveSharingOutput, S13NashvilleUsesMappedOriginSourceKey) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (int staff : {3, 4}) {
        const auto start = xml.find(std::format("<Staff id=\"{}\">", staff));
        const auto at = xml.find("<voice>", start) + std::string("<voice>").size();
        xml.insert(at, "<KeySig><concertKey>2</concertKey></KeySig>");
    }
    const auto origin = xml.find("<Staff id=\"3\">");
    const auto at = xml.find("<Chord>", origin);
    xml.insert(at, "<Harmony><harmonyType>2</harmonyType><harmonyInfo><root>16</root></harmonyInfo></Harmony>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    EXPECT_EQ(master.staff(0)->key(Fraction(0, 1)), Key::C);
    EXPECT_EQ(master.staff(2)->key(Fraction(0, 1)), Key::D);
    Harmony *source = nullptr;
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    for (auto *item : segment->annotations())
        if (item->is_harmony() && item->staff_idx() == 2) source = to_harmony(item);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->display_name(), u"1");
    const auto cold = native_captures(*document);
    expect_no_layout(master);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    Harmony *display = nullptr;
    for (auto *item : segment->rendered_annotations())
        if (item->is_harmony() && item->is_layout_copy() && item->staff_idx() == 0) display = to_harmony(item);
    ASSERT_NE(display, nullptr);
    String rendered;
    for (const auto *item : display->ldata()->render_item_list())
        if (const auto *text = dynamic_cast<const TextSegment *>(item)) rendered += text->text();
    EXPECT_EQ(rendered, u"1");
    EXPECT_EQ(display->display_text(), u"1");
    EXPECT_EQ(source->display_name(), u"1");
    EXPECT_EQ(native_captures(*document), cold);
}

TEST(StaveSharingOutput, UnisonRenderingPreservesAuthoredSourceAndHandles) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_EQ(master.shared_parts().size(), 1u);
    auto *part = master.shared_parts().front();
    ASSERT_EQ(part->origin_parts().size(), 2u);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    auto *authored_rest = segment->element(0);
    ASSERT_TRUE(authored_rest && authored_rest->is_rest());
    auto *origin = to_chord(segment->element(8))->up_note();
    EXPECT_EQ(origin->pitch(), 72);
    const auto objects = engrave::document_objects(master);
    const auto handles = master.eid_register()->size();
    const auto cold = source_native(master);
    EXPECT_NE(cold.find("<SharedPart"), std::string::npos);
    EXPECT_NE(cold.find("<sharedPart>A_A</sharedPart>"), std::string::npos);
    for (int cycle = 0; cycle < 2; ++cycle) {
        auto svg = document->render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(svg) << svg.error().message;
        ASSERT_EQ(svg->pages.size(), 1u);
        EXPECT_GT(svg->pages.front().size(), 1000u);
        auto *shown = segment->effective_element(0);
        ASSERT_TRUE(shown && shown->is_chord());
        EXPECT_EQ(to_chord(shown)->up_note()->pitch(), 72);
        EXPECT_GT(to_chord(shown)->up_note()->width(), 0.0);
        EXPECT_EQ(to_chord(shown)->up_note()->origin_items().size(), 2u);
        EXPECT_EQ(master.layout_copy(*origin), to_chord(shown)->up_note());
        EXPECT_FALSE(source_object(*shown));
        EXPECT_EQ(segment->element(0), authored_rest);
        EXPECT_EQ(part->track_map_at_tick(Fraction(0, 1)), (SharedTrackMap{{8, 0}, {12, 0}}));
        EXPECT_EQ(engrave::document_objects(master), objects);
        EXPECT_EQ(master.eid_register()->size(), handles);
        EXPECT_EQ(source_native(master), cold);
        ASSERT_FALSE(segment->rendered_annotations().empty());
        EXPECT_TRUE(segment->rendered_annotations().back()->is_stave_sharing_label());
        EXPECT_TRUE(segment->annotations().empty());
        size_t labels_drawn = 0;
        segment->scan_elements([&](auto *item) {
            if (item->is_stave_sharing_label()) ++labels_drawn;
        });
        EXPECT_EQ(labels_drawn, 1u);
        EXPECT_TRUE(master.systems().front()->staff(0)->show());
        EXPECT_FALSE(master.systems().front()->staff(2)->show());
        EXPECT_FALSE(master.systems().front()->staff(3)->show());
        master.layout_data_store()->clear();
        EXPECT_EQ(segment->effective_element(0), authored_rest);
        EXPECT_EQ(master.layout_copy(*origin), nullptr);
        EXPECT_EQ(source_native(master), cold);
    }
}

TEST(StaveSharingOutput, TiesAndSlursRemainOutsideSourceSpannerMembership) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure()->first(SegmentType::CHORD_REST);
    auto *second = first->next(SegmentType::CHORD_REST);
    for (track_idx_t track : {8u, 12u}) {
        auto *start = to_chord(first->element(track))->up_note();
        auto *end = to_chord(second->element(track))->up_note();
        end->set_pitch(72);
        end->set_tpc1(14);
        end->set_tpc2(14);
        auto *tie = Factory::create_tie(start);
        tie->set_note_span(start, end);
        master.add_element(tie);
        tie->assign_new_eid();
        auto *slur = Factory::create_slur(master.dummy());
        slur->set_track(track);
        slur->set_track2(track);
        slur->set_tick(first->tick());
        slur->set_tick2(second->tick());
        master.add_element(slur);
        slur->assign_new_eid();
    }
    const auto objects = engrave::document_objects(master);
    const auto spans = master.spanner_list();
    const auto cold = source_native(master);
    for (int cycle = 0; cycle < 2; ++cycle) {
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        const auto &shared = master.layout_data_store()->score_output().shared_spanners;
        EXPECT_EQ(std::ranges::count_if(shared, [](auto *sp) { return sp->is_tie(); }), 1);
        EXPECT_EQ(std::ranges::count_if(shared, [](auto *sp) { return sp->is_slur(); }), 1);
        for (auto *span : shared) {
            EXPECT_FALSE(source_object(*span));
            EXPECT_FALSE(span->spanner_segments().empty());
            ASSERT_NE(span->start_element(), nullptr);
            ASSERT_NE(span->end_element(), nullptr);
            EXPECT_FALSE(source_object(*span->start_element()));
            EXPECT_FALSE(source_object(*span->end_element()));
        }
        EXPECT_EQ(master.spanner_list(), spans);
        EXPECT_EQ(engrave::document_objects(master), objects);
        EXPECT_EQ(source_native(master), cold);
        master.layout_data_store()->clear();
    }
}

// Upstream #35281: every range layout re-added the shared nested tuplet to its shared
// parent, and each copy was autoplaced clear of the previous one.
TEST(StaveSharingOutput, NestedTupletRangeRelayoutMatchesFullLayout) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    const std::string authored = "<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig><Chord><durationType>half</durationType><Note><pitch>72</"
                                 "pitch><tpc>14</tpc></Note></Chord><Chord><durationType>half</durationType><Note><pitch>74</"
                                 "pitch><tpc>16</tpc></Note></Chord>";
    const std::string nested =
        "<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>"
        "<Chord><durationType>quarter</durationType><Note><pitch>72</pitch><tpc>14</tpc></Note></Chord>"
        "<Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>quarter</baseNote></Tuplet>"
        "<Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>eighth</baseNote></Tuplet>"
        "<Chord><durationType>eighth</durationType><Note><pitch>74</pitch><tpc>16</tpc></Note></Chord>"
        "<Rest><durationType>eighth</durationType></Rest><Rest><durationType>eighth</durationType></Rest><endTuplet/>"
        "<Rest><durationType>quarter</durationType></Rest><Rest><durationType>quarter</durationType></Rest><endTuplet/>"
        "<Chord><durationType>quarter</durationType><Note><pitch>81</pitch><tpc>17</tpc></Note></Chord>";
    for (int staff = 0; staff < 2; ++staff) {
        const auto at = xml.find(authored);
        ASSERT_NE(at, std::string::npos);
        xml.replace(at, authored.size(), nested);
    }
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto cold = source_native(master);
    auto *measure = master.first_measure();
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    engrave::Result<engrave::RenderOutput> range;
    for (int cycle = 0; cycle < 2; ++cycle) {
        master.invalidate_layout(measure->tick(), measure->end_tick());
        range = document->render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(range) << range.error().message;
        auto *shown = measure->find_segment_r(SegmentType::CHORD_REST, Fraction(1, 4))->effective_element(0);
        ASSERT_TRUE(shown && shown->is_chord());
        EXPECT_FALSE(source_object(*shown));
        Tuplet *inner = to_chord(shown)->tuplet();
        ASSERT_NE(inner, nullptr);
        Tuplet *outer = inner->tuplet();
        ASSERT_NE(outer, nullptr);
        EXPECT_EQ(inner->elements().size(), 3u);
        EXPECT_EQ(outer->elements().size(), 3u);
    }
    EXPECT_GT(master.layout_state.range_count, 0u);
    EXPECT_EQ(source_native(master), cold);
    master.layout_data_store()->clear();
    auto full = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(full) << full.error().message;
    EXPECT_EQ(range->pages, full->pages);
}

TEST(StaveSharingOutput, UnmatchedAuthoredRestsStayInSourceAfterSuppression) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    const std::string rest = "<Rest><durationType>measure</durationType></Rest>";
    xml.replace(xml.find(rest), rest.size(),
                "<Rest><durationType>quarter</durationType></Rest><Rest><durationType>quarter</durationType></"
                "Rest><Rest><durationType>half</durationType></Rest>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    const auto objects = engrave::document_objects(master);
    const auto cold = source_native(master);
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
    auto *quarter = master.first_measure()->find_segment_r(SegmentType::CHORD_REST, Fraction(1, 4));
    ASSERT_NE(quarter, nullptr);
    ASSERT_TRUE(quarter->element(0)->is_rest());
    EXPECT_EQ(quarter->effective_element(0), nullptr);
    EXPECT_EQ(engrave::document_objects(master), objects);
    EXPECT_EQ(source_native(master), cold);
}

TEST(StaveSharingOutput, PartialTiesAndBracketClonesKeepSourceLinksAndJumpPoints) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure()->first(SegmentType::CHORD_REST);
    auto *second = first->next(SegmentType::CHORD_REST);
    std::vector<std::pair<Note *, TieJumpPoint *>> points;
    std::vector<ChordBracket *> brackets;
    std::vector<Fingering *> fingerings;
    for (track_idx_t track : {8u, 12u}) {
        auto *chord = to_chord(first->element(track));
        auto *start = chord->up_note();
        auto *end = to_chord(second->element(track))->up_note();
        auto *outgoing = Factory::create_partial_tie(start);
        outgoing->set_start_note(start);
        master.add_element(outgoing);
        outgoing->assign_new_eid();
        auto *incoming = Factory::create_partial_tie(end);
        incoming->set_end_note(end);
        master.add_element(incoming);
        incoming->assign_new_eid();
        auto *point = new TieJumpPoint(end, false, 0, true);
        start->tie_jump_points()->add(point);
        points.emplace_back(start, point);
        auto *bracket = Factory::create_chord_bracket(chord);
        master.add_element(bracket);
        bracket->assign_new_eid();
        brackets.push_back(bracket);
        auto *finger = Factory::create_fingering(start);
        finger->set_xml_text(u"2");
        master.add_element(finger);
        finger->assign_new_eid();
        fingerings.push_back(finger);
    }
    const auto objects = engrave::document_objects(master);
    const auto cold = source_native(master);
    for (int cycle = 0; cycle < 2; ++cycle) {
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        const auto &spans = master.layout_data_store()->score_output().shared_spanners;
        EXPECT_EQ(std::ranges::count_if(spans, [](auto *sp) { return sp->is_partial_tie(); }), 2);
        for (auto *span : spans) {
            auto *partial = to_partial_tie(span);
            EXPECT_TRUE(partial->is_layout_copy());
            EXPECT_NE(partial->note(), nullptr);
        }
        for (const auto &[note, point] : points) {
            ASSERT_EQ(note->tie_jump_points()->size(), 1u);
            EXPECT_EQ(*note->tie_jump_points()->begin(), point);
        }
        for (auto *bracket : brackets) EXPECT_EQ(bracket->links(), nullptr);
        for (auto *finger : fingerings) {
            const auto *display = master.layout_copy(*finger);
            ASSERT_NE(display, nullptr);
            EXPECT_FALSE(source_object(*display));
            EXPECT_GT(display->width(), 0.0);
        }
        EXPECT_EQ(engrave::document_objects(master), objects);
        EXPECT_EQ(source_native(master), cold);
        master.layout_data_store()->clear();
    }
    for (const auto &[note, point] : points) note->tie_jump_points()->clear();
}

TEST(StaveSharingOutput, AuxiliaryRetirementInvalidatesEveryBorrowingPage) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("<Measure>") + 9, "<LayoutBreak><subtype>page</subtype></LayoutBreak>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    for (auto *measure = master.first_measure(); measure; measure = measure->next_measure()) {
        for (track_idx_t track : {8u, 12u}) {
            auto *slur = Factory::create_slur(master.dummy());
            slur->set_track(track);
            slur->set_track2(track);
            slur->set_tick(measure->tick());
            slur->set_tick2(measure->tick() + Fraction(1, 2));
            master.add_element(slur);
            slur->assign_new_eid();
        }
    }
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_EQ(master.pages().size(), 2u);
    for (auto *page : master.pages()) EXPECT_FALSE(page->items(muse::RectF(0, 0, 10000, 10000)).empty());
    master.layout_data_store()->retire_measure_borrowers(*master.first_measure());
    for (auto *page : master.pages()) EXPECT_TRUE(page->items(muse::RectF(0, 0, 10000, 10000)).empty());
    EXPECT_TRUE(master.layout_data_store()->score_output().shared_spanners.empty());
}

TEST(StaveSharingOutput, MultipleVoicesUseRenderedNotesWithoutChangingSourceVoiceFlags) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing-voices.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    const auto cold = source_native(master);
    EXPECT_FALSE(measure->has_voices(0));
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    auto *segment = measure->first(SegmentType::CHORD_REST);
    auto *upper = to_chord(segment->effective_element(0));
    auto *lower = to_chord(segment->effective_element(1));
    ASSERT_TRUE(upper && lower);
    EXPECT_EQ(master.shared_parts().front()->track_map_at_tick(Fraction(0, 1)), (SharedTrackMap{{12, 0}, {16, 1}, {20, 1}}));
    EXPECT_TRUE(upper->up());
    EXPECT_FALSE(lower->up());
    EXPECT_EQ(lower->notes().size(), 2u);
    EXPECT_TRUE(measure->has_rendered_voices(0));
    EXPECT_FALSE(measure->has_voices(0));
    EXPECT_EQ(source_native(master), cold);
    master.layout_data_store()->clear();
    EXPECT_FALSE(measure->has_voices(0));
    EXPECT_EQ(source_native(master), cold);
}

TEST(StaveSharingOutput, LocalSignatureProjectionLeavesSourceQueriesColdAndWarmIdentical) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *segment = master.first_measure()->first(SegmentType::TIME_SIG);
    auto *global = to_time_sig(segment->element(0));
    auto *local = to_time_sig(segment->element(8));
    ASSERT_TRUE(global && local);
    local->set_stretch(Fraction(3, 4));
    const auto clef = master.staff(0)->clef(Fraction(0, 1));
    const auto key = master.staff(0)->key(Fraction(0, 1));
    const auto cold = source_native(master);
    EXPECT_EQ(master.staff(0)->time_sig(Fraction(0, 1)), global);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(master.staff(0)->time_sig(Fraction(0, 1)), global);
    EXPECT_EQ(master.staff(0)->rendered_time_sig(Fraction(0, 1)), local);
    EXPECT_TRUE(master.systems().front()->staff(1)->show());
    EXPECT_EQ(master.staff(0)->clef(Fraction(0, 1)), clef);
    EXPECT_EQ(master.staff(0)->key(Fraction(0, 1)), key);
    EXPECT_EQ(source_native(master), cold);
}

TEST(StaveSharingOutput, SharedHairpinsSnapToOutputAndSuppressAuthoredSharedTrackSpanners) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    std::vector<Dynamic *> dynamics;
    for (track_idx_t track : {8u, 12u}) {
        auto *dynamic = Factory::create_dynamic(segment);
        dynamic->set_track(track);
        dynamic->set_dynamic_type(DynamicType::P);
        master.add_element(dynamic);
        dynamic->assign_new_eid();
        dynamics.push_back(dynamic);
    }
    Hairpin *authored_shared = nullptr;
    for (track_idx_t track : {0u, 8u, 12u}) {
        for (int half = 0; half < (track ? 2 : 1); ++half) {
            auto *hairpin = Factory::create_hairpin(master.dummy());
            hairpin->set_track(track);
            hairpin->set_track2(track);
            hairpin->set_tick(Fraction(half, 2));
            hairpin->set_tick2(Fraction(half + 1, 2));
            master.add_element(hairpin);
            hairpin->assign_new_eid();
            if (!track) authored_shared = hairpin;
        }
    }
    const auto cold = source_native(master);
    const auto source_spans = master.spanner_list();
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    for (auto *piece : authored_shared->spanner_segments()) EXPECT_EQ(piece->system(), nullptr);
    std::vector<Hairpin *> shared;
    for (auto *span : master.layout_data_store()->score_output().shared_spanners)
        if (span->is_hairpin()) shared.push_back(to_hairpin(span));
    ASSERT_EQ(shared.size(), 2u);
    std::sort(shared.begin(), shared.end(), [](auto *a, auto *b) { return a->tick() < b->tick(); });
    ASSERT_FALSE(shared.front()->segments_empty());
    ASSERT_FALSE(shared.back()->segments_empty());
    auto *before = to_hairpin_segment(shared.front()->front_segment())->find_element_to_snap_before(false);
    EXPECT_EQ(before, master.layout_copy(*dynamics.front()));
    EXPECT_EQ(to_hairpin_segment(shared.back()->front_segment())->find_element_to_snap_before(false), shared.front()->back_segment());
    EXPECT_EQ(master.spanner_list(), source_spans);
    EXPECT_EQ(source_native(master), cold);
}

TEST(StaveSharingOutput, MmrAnnotationGeometryResolvesToTheVisibleSharedCopy) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing-mmr.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    std::vector<Dynamic *> originals;
    for (auto *object : engrave::document_objects(master))
        if (object->is_dynamic()) originals.push_back(to_dynamic(object));
    ASSERT_EQ(originals.size(), 2u);
    const auto cold = source_native(master);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    auto *display = master.first_measure()->mm_rest();
    ASSERT_NE(display, nullptr);
    EXPECT_EQ(display->mm_rest_count(), 2);
    for (auto *dynamic : originals) {
        const auto *copy = master.layout_copy(*dynamic);
        ASSERT_NE(copy, nullptr);
        EXPECT_EQ(copy->track(), 0u);
        EXPECT_EQ(copy->find_measure(), display);
        EXPECT_FALSE(source_object(*copy));
        EXPECT_GT(copy->width(), 0.0);
    }
    EXPECT_EQ(source_native(master), cold);
}

TEST(StaveSharingOutput, EditedUnisonRangeMatchesFullLayout) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    auto *measure = master.first_measure()->next_measure();
    auto *note = to_chord(measure->first(SegmentType::CHORD_REST)->element(12))->up_note();
    note->set_pitch(71);
    note->set_tpc1(19);
    note->set_tpc2(19);
    master.invalidate_layout(measure->tick(), measure->end_tick());
    auto range = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(range) << range.error().message;
    EXPECT_GT(master.layout_state.range_count, 0u);
    auto *display = measure->first(SegmentType::CHORD_REST)->effective_element(0);
    ASSERT_TRUE(display && display->is_chord());
    std::set<int> pitches;
    for (auto *n : to_chord(display)->notes()) pitches.insert(n->pitch());
    EXPECT_EQ(pitches, (std::set<int>{71, 72}));
    const auto saved = source_native(master);
    master.layout_data_store()->clear();
    auto full = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(full) << full.error().message;
    EXPECT_EQ(range->pages, full->pages);
    EXPECT_EQ(source_native(master), saved);
}

TEST(StaveSharingOutput, UpstreamAuthoredVoiceCrossingSurvivesNativeRoundTrip) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing-12.mscz");
    ASSERT_TRUE(document);
    for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        const auto saved = document->export_score({.format = format});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto &master = master_of(**reopened);
        ASSERT_EQ(master.shared_parts().size(), 1u);
        auto *part = master.shared_parts().front();
        const Fraction tick(8, 4);
        const SharedTrackMap expected{{8, 1}, {12, 0}};
        const auto *state = part->exact_authored_state(tick);
        ASSERT_NE(state, nullptr);
        EXPECT_FALSE(state->is_reset);
        EXPECT_EQ(state->user_track_map, expected);
        EXPECT_EQ(master.style().style_s(Sid::STAVE_SHARING_CHANGE_MIN_DISTANCE), 0.5_sp);

        const auto cold = source_native(master);
        const auto objects = engrave::document_objects(master);
        const auto spanners = master.spanner_list();
        auto *segment = master.tick2measure(tick)->find_segment(SegmentType::CHORD_REST, tick);
        ASSERT_NE(segment, nullptr);
        for (int cycle = 0; cycle < 2; ++cycle) {
            auto rendered = (*reopened)->render({.format = engrave::RenderFormat::SVG});
            ASSERT_TRUE(rendered) << rendered.error().message;
            EXPECT_EQ(part->track_map_at_tick(tick), expected);
            auto *first_voice = segment->effective_element(0);
            auto *second_voice = segment->effective_element(1);
            ASSERT_TRUE(first_voice && first_voice->is_chord());
            ASSERT_TRUE(second_voice && second_voice->is_chord());
            EXPECT_EQ(to_chord(first_voice)->up_note()->pitch(), 74);
            EXPECT_EQ(to_chord(second_voice)->up_note()->pitch(), 77);

            const auto items = segment->rendered_annotations();
            const auto indicator = std::ranges::find_if(items, [](auto *item) { return item->is_stave_sharing_change(); });
            ASSERT_NE(indicator, items.end());
            EXPECT_TRUE((*indicator)->is_layout_copy());
            EXPECT_FALSE(source_object(**indicator));
            EXPECT_EQ(to_stave_sharing_change(*indicator)->icon_code(), char16_t(0xF499));
            EXPECT_EQ(source_native(master), cold);
            EXPECT_EQ(engrave::document_objects(master), objects);
            EXPECT_EQ(master.spanner_list(), spanners);
            master.layout_data_store()->clear();
        }
    }
}

TEST(StaveSharingOutput, ResetFillsVoiceOneWithoutReplacingTheSourceRest) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("</SharedPart>"),
               R"(<StaveSharingState tick="0/1"><trackMap origin="8" shared="4"/><trackMap origin="12" shared="4"/></StaveSharingState>)"
               R"(<StaveSharingState tick="1/2"><isReset>1</isReset></StaveSharingState>)");
    xml.insert(xml.find("</Style>"), "<staveSharingChangeMinDistance>1.25</staveSharingChangeMinDistance>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *part = master.shared_parts().front();
    const Fraction start_tick(0, 1);
    const Fraction reset_tick(1, 2);
    auto *measure = master.first_measure();
    auto *start = measure->find_segment(SegmentType::CHORD_REST, start_tick);
    auto *reset = measure->find_segment(SegmentType::CHORD_REST, reset_tick);
    ASSERT_NE(start, nullptr);
    ASSERT_NE(reset, nullptr);
    auto *source_rest = to_rest(start->element(0));
    ASSERT_TRUE(source_rest && source_rest->is_full_measure_rest());
    const auto cold = source_native(master);
    EXPECT_NE(cold.find("<isReset>1</isReset>"), std::string::npos);
    EXPECT_NE(cold.find("<staveSharingChangeMinDistance>1.25</staveSharingChangeMinDistance>"), std::string::npos);
    const auto rendered = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(rendered) << rendered.error().message;
    EXPECT_EQ(part->track_map_at_tick(start_tick), (SharedTrackMap{{8, 4}, {12, 4}}));
    EXPECT_EQ(part->track_map_at_tick(reset_tick), (SharedTrackMap{{8, 0}, {12, 0}}));
    auto *rest = start->effective_element(0);
    ASSERT_TRUE(rest && rest->is_rest());
    EXPECT_EQ(to_rest(rest)->actual_ticks(), reset_tick);
    EXPECT_EQ(start->element(0), source_rest);
    auto *chord = reset->effective_element(0);
    ASSERT_TRUE(chord && chord->is_chord());
    EXPECT_EQ(to_chord(chord)->up_note()->pitch(), 74);
    const auto items = reset->rendered_annotations();
    const auto indicator = std::ranges::find_if(items, [](auto *item) { return item->is_stave_sharing_change(); });
    ASSERT_NE(indicator, items.end());
    EXPECT_EQ(to_stave_sharing_change(*indicator)->icon_code(), char16_t(0xEF19));
    EXPECT_EQ(source_native(master), cold);
    EXPECT_EQ(cold.find("<StaveSharingChange>"), std::string::npos);

    auto reopened = open_text(cold);
    ASSERT_TRUE(reopened);
    auto &saved = master_of(*reopened);
    const auto *state = saved.shared_parts().front()->exact_authored_state(reset_tick);
    ASSERT_NE(state, nullptr);
    EXPECT_TRUE(state->is_reset);
    EXPECT_EQ(saved.style().style_s(Sid::STAVE_SHARING_CHANGE_MIN_DISTANCE), 1.25_sp);

    master.invalidate_layout(measure->tick(), measure->end_tick());
    auto range = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(range) << range.error().message;
    EXPECT_EQ(range->pages, rendered->pages);
    EXPECT_EQ(source_native(master), cold);
}

TEST(StaveSharingOutput, InvalidMapInLaterSystemFallsBackThere) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing-three-systems.mscx");
    xml.insert(xml.find("<Division>"), "<Style><enableStaveSharing>1</enableStaveSharing></Style>");
    xml.insert(xml.find("</SharedPart>"),
               R"(<StaveSharingState tick="0/1"><trackMap origin="12" shared="0"/><trackMap origin="16" shared="0"/>)"
               R"(<trackMap origin="20" shared="0"/></StaveSharingState>)");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto cold = source_native(master);
    auto rendered = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(rendered) << rendered.error().message;
    const auto *part = master.shared_parts().front();
    const auto *first_measure = master.first_measure();
    const auto *second_measure = first_measure->next_measure();
    ASSERT_NE(first_measure->system(), second_measure->system());
    EXPECT_EQ(part->track_map_at_tick(first_measure->tick()), (SharedTrackMap{{12, 0}, {16, 0}, {20, 0}}));
    EXPECT_EQ(part->track_map_at_tick(second_measure->tick()), (SharedTrackMap{{12, 0}, {16, 1}, {20, 4}}));

    const auto annotations = first_measure->first(SegmentType::CHORD_REST)->rendered_annotations();
    const auto indicator = std::ranges::find_if(annotations, [](auto *item) { return item->is_stave_sharing_change(); });
    ASSERT_NE(indicator, annotations.end());
    EXPECT_EQ(to_stave_sharing_change(*indicator)->icon_code(), char16_t(0xF3CE));
    EXPECT_EQ(source_native(master), cold);
}

TEST(StaveSharingOutput, DetachedIndicatorsAcceptDummyParents) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    std::unique_ptr<StaveSharingChange> change(Factory::create_stave_sharing_change(master.dummy()));
    change->set_track(0);
    EXPECT_EQ(change->shared_part(), nullptr);
    EXPECT_EQ(change->icon_code(), char16_t(0xF499));
    change->set_ownership_parent(segment);
    EXPECT_EQ(change->shared_part(), master.shared_parts().front());
    change->set_ownership_parent(master.dummy());
    EXPECT_EQ(change->tick(), Fraction(0, 1));
    EXPECT_EQ(change->shared_part(), nullptr);

    SystemLockIndicator lock(master.dummy(), nullptr);
    EXPECT_EQ(lock.system(), nullptr);
    EXPECT_EQ(lock.tick(), Fraction(0, 1));
}

TEST(StaveSharingOutput, InsertAndRemoveTimeMovesAuthoredStates) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing-12.mscz");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *part = master.shared_parts().front();
    const Fraction old_tick(2, 1);
    const Fraction shifted(9, 4);
    master.insert_time(Fraction(1, 1), Fraction(1, 4), {});
    EXPECT_EQ(part->exact_authored_state(old_tick), nullptr);
    const auto *state = part->exact_authored_state(shifted);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->user_track_map, (SharedTrackMap{{8, 1}, {12, 0}}));
    master.insert_time(old_tick, Fraction(-1, 2), {});
    EXPECT_TRUE(part->authored_states().empty());
}

TEST(StaveSharingOutput, StaffEditsRebaseAuthoredTracksAcrossCheckpointRestoreAndSave) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (int id = 4; id >= 1; --id) {
        const auto tag = std::format("<Staff id=\"{}\">", id);
        xml.replace(xml.find(tag), tag.size(), std::format("<Staff id=\"{}\">", id + 1));
    }
    xml.insert(xml.find("<SharedPart>"), "<Part><Staff/><Instrument id=\"flute\"/></Part>");
    xml.insert(xml.find("<Staff id=\"2\">"),
               "<Staff id=\"1\"><Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>"
               "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure></Staff>");
    xml.insert(xml.find("</SharedPart>"),
               R"(<StaveSharingState tick="0/1"><trackMap origin="12" shared="4"/><trackMap origin="16" shared="8"/></StaveSharingState>)");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    const Fraction tick(0, 1);
    const SharedTrackMap original{{12, 4}, {16, 8}};
    const auto check = [&](engrave::Document &doc, const SharedTrackMap &expected) {
        auto &master = master_of(doc);
        ASSERT_EQ(master.shared_parts().size(), 1u);
        const auto *state = master.shared_parts().front()->exact_authored_state(tick);
        ASSERT_NE(state, nullptr);
        EXPECT_EQ(state->user_track_map, expected);
    };
    const auto edit = [&](auto operation, const SharedTrackMap &before, const SharedTrackMap &after) {
        ASSERT_TRUE(document->checkpoint("before"));
        auto &master = master_of(*document);
        auto *manager = master.transaction_manager();
        manager->begin_transaction(muse::TranslatableString::untranslatable("change staff order"));
        operation(master);
        manager->end_transaction();
        engrave::assign_missing_ids(master);
        check(*document, after);
        auto reopened = open_text(source_native(master));
        ASSERT_TRUE(reopened);
        check(*reopened, after);
        ASSERT_TRUE(document->checkpoint("after"));
        ASSERT_TRUE(document->restore_checkpoint("before"));
        check(*document, before);
        ASSERT_TRUE(document->restore_checkpoint("after"));
        check(*document, after);
    };
    check(*document, original);
    const SharedTrackMap inserted{{16, 8}, {20, 12}};
    edit([](MasterScore &master) { master.undo_insert_staff(Factory::create_staff(master.parts().front()), 0); }, original, inserted);
    edit([](MasterScore &master) { master.cmd_remove_staff(0); }, inserted, original);
    edit(
        [](MasterScore &master) {
            std::vector<staff_idx_t> order{1, 2, 0, 4, 3};
            master.sort_staves(order);
        },
        original, {{12, 4}, {16, 0}});
    edit([](MasterScore &master) { master.cmd_remove_staff(1); }, {{12, 4}, {16, 0}}, {{12, 0}});
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    const auto *state = master_of(*document).shared_parts().front()->rendered_state_at_tick(tick);
    ASSERT_NE(state, nullptr);
    EXPECT_FALSE(state->is_user_map_valid);
    EXPECT_EQ(state->shared_track_map, (SharedTrackMap{{8, 0}, {12, 0}}));
}

TEST(StaveSharingOutput, InvalidAuthoredMapsKeepAllOriginNotesVisible) {
    using namespace mu::engraving;
    const auto source = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (const std::string_view entries : {
             "",
             R"(<trackMap origin="8" shared="4"/>)",
             R"(<trackMap origin="8" shared="4"/><trackMap origin="12" shared="4"/><trackMap origin="8" shared="4"/>)",
             R"(<trackMap origin="8" shared="4"/><trackMap origin="12" shared="4"/><trackMap origin="8" shared="0"/>)",
         }) {
        SCOPED_TRACE(entries);
        auto xml = source;
        xml.insert(xml.find("</SharedPart>"), std::format(R"(<StaveSharingState tick="0/1">{}</StaveSharingState>)", entries));
        auto document = open_text(xml);
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        const auto *state = master.shared_parts().front()->rendered_state_at_tick(Fraction(0, 1));
        ASSERT_NE(state, nullptr);
        EXPECT_FALSE(state->is_user_map_valid);
        EXPECT_EQ(state->shared_track_map, (SharedTrackMap{{8, 0}, {12, 0}}));
        auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
        const auto *rendered = to_chord(segment->effective_element(0))->up_note();
        EXPECT_EQ(rendered->pitch(), 72);
        for (const auto track : {8u, 12u}) EXPECT_EQ(master.layout_copy(*to_chord(segment->element(track))->up_note()), rendered);
    }
}

TEST(StaveSharingOutput, AuthoredMapsMustIncludeActiveSecondaryOriginVoices) {
    using namespace mu::engraving;
    const auto source = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (const bool complete : {false, true}) {
        auto xml = source;
        xml.insert(xml.find("</Measure>", xml.find("<Staff id=\"3\">")),
                   "<voice><Chord><durationType>whole</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord></voice>");
        xml.insert(
            xml.find("</SharedPart>"),
            std::format(
                R"(<StaveSharingState tick="0/1"><trackMap origin="8" shared="0"/>{}<trackMap origin="12" shared="4"/></StaveSharingState>)",
                complete ? R"(<trackMap origin="9" shared="1"/>)" : ""));
        auto document = open_text(xml);
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        const auto *state = master.shared_parts().front()->rendered_state_at_tick(Fraction(0, 1));
        ASSERT_NE(state, nullptr);
        EXPECT_EQ(state->is_user_map_valid, complete);
        if (complete) EXPECT_EQ(state->shared_track_map, (SharedTrackMap{{8, 0}, {9, 1}, {12, 4}}));
        auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
        auto *secondary = to_chord(segment->element(9))->up_note();
        const auto *copy = master.layout_copy(*secondary);
        ASSERT_NE(copy, nullptr);
        EXPECT_EQ(to_note(copy)->pitch(), 60);
        EXPECT_EQ(copy->track(), 1u);
    }
}

TEST(StaveSharingOutput, SplitUnisonKeepsBothTiesAcrossAuthoredBoundary) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure()->first(SegmentType::CHORD_REST);
    auto *second = first->next(SegmentType::CHORD_REST);
    auto *part = master.shared_parts().front();
    part->set_authored_state(first->tick(), {{{8, 0}, {12, 0}}, false});
    part->set_authored_state(second->tick(), {{{8, 1}, {12, 0}}, false});
    std::vector<Tie *> ties;
    for (const auto track : {8u, 12u}) {
        auto *start = to_chord(first->element(track))->up_note();
        auto *end = to_chord(second->element(track))->up_note();
        end->set_pitch(72);
        end->set_tpc1(14);
        end->set_tpc2(14);
        auto *tie = Factory::create_tie(start);
        tie->set_note_span(start, end);
        master.add_element(tie);
        tie->assign_new_eid();
        ties.push_back(tie);
    }
    for (int pass = 0; pass < 2; ++pass) {
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        EXPECT_FALSE(part->rendered_state_at_tick(first->tick())->is_user_map_valid);
        EXPECT_TRUE(part->rendered_state_at_tick(second->tick())->is_user_map_valid);
        for (size_t i = 0; i < ties.size(); ++i) {
            auto *start = to_chord(first->effective_element(i))->up_note();
            auto *end = to_chord(second->effective_element(1 - i))->up_note();
            const auto *display = to_tie(master.layout_copy(*ties[i]));
            ASSERT_NE(display, nullptr);
            EXPECT_EQ(display->start_note(), start);
            EXPECT_EQ(display->end_note(), end);
            EXPECT_EQ(start->tie_for(), display);
            EXPECT_EQ(end->tie_back(), display);
        }
        master.invalidate_layout(first->tick(), second->tick());
    }
}

TEST(StaveSharingOutput, CrossingSlurResolvesEachEndpointAndHairpinStaysOnStaff) {
    using namespace mu::engraving;
    const auto source = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (bool system_break : {false, true}) {
        SCOPED_TRACE(system_break);
        auto xml = source;
        if (system_break) xml.insert(xml.find("<voice>"), "<LayoutBreak><subtype>line</subtype></LayoutBreak>");
        auto document = open_text(xml);
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        auto *first = master.first_measure()->first(SegmentType::CHORD_REST);
        auto *second = first->next(SegmentType::CHORD_REST);
        if (system_break) {
            first = second;
            second = master.first_measure()->next_measure()->first(SegmentType::CHORD_REST);
        }
        auto *part = master.shared_parts().front();
        part->set_authored_state(Fraction(0, 1), {{{8, 0}, {12, 1}}, false});
        part->set_authored_state(second->tick(), {{{8, 1}, {12, 0}}, false});
        auto *slur = Factory::create_slur(master.dummy());
        auto *hairpin = Factory::create_hairpin(master.dummy());
        for (Spanner *span : {static_cast<Spanner *>(slur), static_cast<Spanner *>(hairpin)}) {
            span->set_track(8);
            span->set_track2(8);
            span->set_tick(first->tick());
            span->set_tick2(second->tick());
            master.add_element(span);
            span->assign_new_eid();
        }
        const auto cold = source_native(master);
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        EXPECT_EQ(first->system() != second->system(), system_break);
        const auto *shared_slur = to_slur(master.layout_copy(*slur));
        ASSERT_NE(shared_slur, nullptr);
        EXPECT_EQ(shared_slur->track(), 0u);
        EXPECT_EQ(shared_slur->track2(), 1u);
        EXPECT_EQ(shared_slur->start_element(), first->effective_element(0));
        EXPECT_EQ(shared_slur->end_element(), second->effective_element(1));
        const auto *shared_hairpin = to_hairpin(master.layout_copy(*hairpin));
        ASSERT_NE(shared_hairpin, nullptr);
        EXPECT_EQ(shared_hairpin->track(), 0u);
        EXPECT_EQ(shared_hairpin->track2(), 0u);
        EXPECT_EQ(shared_hairpin->voice_assignment(), VoiceAssignment::ALL_VOICE_IN_STAFF);
        EXPECT_FALSE(shared_hairpin->segments_empty());
        EXPECT_EQ(source_native(master), cold);
    }
}

TEST(StaveSharingOutput, SplitUnisonKeepsBothTiesAcrossAutomaticSystemMaps) {
    using namespace mu::engraving;
    const auto source = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (const bool authored : {false, true}) {
        SCOPED_TRACE(authored);
        auto xml = source;
        xml.insert(xml.find("<voice>"), "<LayoutBreak><subtype>line</subtype></LayoutBreak>");
        auto document = open_text(xml);
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        auto *first_measure = master.first_measure();
        auto *second_measure = first_measure->next_measure();
        auto *first = first_measure->first(SegmentType::CHORD_REST)->next(SegmentType::CHORD_REST);
        auto *second = second_measure->first(SegmentType::CHORD_REST);
        auto *part = master.shared_parts().front();
        if (authored) part->set_authored_state(first_measure->tick(), {{{8, 0}, {12, 0}}, false});
        auto *diverging = to_chord(second->next(SegmentType::CHORD_REST)->element(12))->up_note();
        diverging->set_pitch(76);
        diverging->set_tpc1(18);
        diverging->set_tpc2(18);

        std::vector<Tie *> ties;
        for (const auto track : {8u, 12u}) {
            auto *start = to_chord(first->element(track))->up_note();
            auto *end = to_chord(second->element(track))->up_note();
            end->set_pitch(74);
            end->set_tpc1(16);
            end->set_tpc2(16);
            auto *tie = Factory::create_tie(start);
            tie->set_note_span(start, end);
            master.add_element(tie);
            tie->assign_new_eid();
            ties.push_back(tie);
        }

        const auto cold = source_native(master);
        for (int pass = 0; pass < 2; ++pass) {
            ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
            ASSERT_NE(first->system(), second->system());
            const auto *state = part->rendered_state_at_tick(second->tick());
            ASSERT_NE(state, nullptr);
            EXPECT_FALSE(state->is_user_map_valid);
            const Tie *displays[2]{};
            for (size_t i = 0; i < ties.size(); ++i) {
                const auto *start = to_note(master.layout_copy(*ties[i]->start_note()));
                const auto *end = to_note(master.layout_copy(*ties[i]->end_note()));
                const auto *display = to_tie(master.layout_copy(*ties[i]));
                ASSERT_NE(start, nullptr);
                ASSERT_NE(end, nullptr);
                ASSERT_NE(display, nullptr);
                EXPECT_EQ(display->start_note(), start);
                EXPECT_EQ(display->end_note(), end);
                EXPECT_EQ(start->tie_for(), display);
                EXPECT_EQ(end->tie_back(), display);
                EXPECT_EQ(display->nsegments(), 2u);
                displays[i] = display;
            }
            EXPECT_NE(displays[0]->start_note(), displays[1]->start_note());
            EXPECT_NE(displays[0]->end_note(), displays[1]->end_note());
            EXPECT_EQ(source_native(master), cold);
            master.invalidate_layout(second->tick(), second_measure->end_tick());
        }
    }
}

TEST(StaveSharingOutput, AutomaticCrossSystemSpannersKeepTheirStartTrack) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("<voice>"), "<LayoutBreak><subtype>line</subtype></LayoutBreak>");
    xml.insert(xml.find("</Style>"), "<allowVoiceCrossing>0</allowVoiceCrossing>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure()->first(SegmentType::CHORD_REST)->next(SegmentType::CHORD_REST);
    auto *second = master.first_measure()->next_measure()->first(SegmentType::CHORD_REST)->next(SegmentType::CHORD_REST);
    auto *end = to_chord(second->element(12))->up_note();
    end->set_pitch(76);
    end->set_tpc1(18);
    end->set_tpc2(18);
    auto *slur = Factory::create_slur(master.dummy());
    auto *hairpin = Factory::create_hairpin(master.dummy());
    for (Spanner *span : {static_cast<Spanner *>(slur), static_cast<Spanner *>(hairpin)}) {
        span->set_track(12);
        span->set_track2(12);
        span->set_tick(first->tick());
        span->set_tick2(second->tick());
        master.add_element(span);
        span->assign_new_eid();
    }
    for (int pass = 0; pass < 2; ++pass) {
        const auto cold = source_native(master);
        const track_idx_t end_track = pass == 0 ? 4u : 1u;
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(first->system(), second->system());
        const auto *shared_slur = to_slur(master.layout_copy(*slur));
        ASSERT_NE(shared_slur, nullptr);
        EXPECT_EQ(shared_slur->track(), 1u);
        EXPECT_EQ(shared_slur->effective_track2(), end_track);
        EXPECT_EQ(shared_slur->start_element(), first->effective_element(1));
        EXPECT_EQ(shared_slur->end_element(), second->effective_element(end_track));
        const auto *shared_hairpin = to_hairpin(master.layout_copy(*hairpin));
        ASSERT_NE(shared_hairpin, nullptr);
        EXPECT_EQ(shared_hairpin->track(), 1u);
        EXPECT_EQ(shared_hairpin->effective_track2(), 1u);
        EXPECT_EQ(shared_hairpin->voice_assignment(),
                  pass == 0 ? VoiceAssignment::ALL_VOICE_IN_STAFF : VoiceAssignment::CURRENT_VOICE_ONLY);
        EXPECT_EQ(shared_hairpin->start_element(), first->effective_element(1));
        const auto *line_end = shared_hairpin->end_element();
        ASSERT_NE(line_end, nullptr);
        EXPECT_EQ(line_end->staff_idx(), 0u);
        EXPECT_EQ(line_end->tick(), second->tick());
        if (pass == 0) {
            EXPECT_TRUE(line_end->is_time_tick_anchor());
        } else {
            EXPECT_EQ(line_end, second->effective_element(1));
        }
        for (const Spanner *span : {static_cast<const Spanner *>(shared_slur), static_cast<const Spanner *>(shared_hairpin)}) {
            ASSERT_EQ(span->nsegments(), 2u);
            EXPECT_EQ(span->segment_at(0)->system(), first->system());
            EXPECT_EQ(span->segment_at(1)->system(), second->system());
        }
        EXPECT_EQ(source_native(master), cold);
        if (pass == 0) {
            end->set_pitch(74);
            end->set_tpc1(16);
            end->set_tpc2(16);
            master.invalidate_layout(second->tick(), second->measure()->end_tick());
        }
    }
}

TEST(StaveSharingOutput, RemovingSharedStaffKeepsIncompatibleOriginVoicesVisible) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("</Style>"), "<allowVoiceCrossing>1</allowVoiceCrossing>");
    const auto voice = [](int pitch, int tpc) {
        return std::format("<voice><Chord><durationType>whole</durationType><Note><pitch>{}</pitch><tpc>{}</tpc></Note></Chord></voice>",
                           pitch, tpc);
    };
    xml.insert(xml.find("</Measure>", xml.find("<Staff id=\"4\">")), voice(84, 14));
    xml.insert(xml.find("</Measure>", xml.find("<Staff id=\"3\">")), voice(77, 13) + voice(79, 15));
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    auto *segment = measure->first(SegmentType::CHORD_REST);
    auto *part = master.shared_parts().front();
    auto *fourth = to_chord(segment->element(12))->up_note();
    fourth->set_pitch(81);
    fourth->set_tpc1(17);
    fourth->set_tpc2(17);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_EQ(part->track_map_at_tick(measure->tick()).size(), 5u);

    auto *manager = master.transaction_manager();
    manager->begin_transaction(muse::TranslatableString::untranslatable("remove a shared staff"));
    master.cmd_remove_staff(1);
    manager->end_transaction();
    ASSERT_EQ(part->staves().size(), 1u);
    const auto cold = source_native(master);
    const auto objects = engrave::document_objects(master);
    for (int pass = 0; pass < 2; ++pass) {
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        EXPECT_FALSE(measure->system()->staff(0)->show());
        EXPECT_TRUE(measure->system()->staff(1)->show());
        EXPECT_TRUE(measure->system()->staff(2)->show());
        std::vector<int> visible_pitches;
        segment->scan_elements([&](const auto *item) {
            if (item->is_note()) visible_pitches.push_back(to_note(item)->pitch());
        });
        std::ranges::sort(visible_pitches);
        EXPECT_EQ(visible_pitches, (std::vector{72, 77, 79, 81, 84}));
        EXPECT_EQ(source_native(master), cold);
        EXPECT_EQ(engrave::document_objects(master), objects);
        master.invalidate_layout(measure->tick(), measure->end_tick());
    }

    fourth->set_pitch(70);
    fourth->set_tpc1(12);
    fourth->set_tpc2(12);
    master.invalidate_layout(measure->tick(), measure->end_tick());
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_TRUE(measure->system()->staff(0)->show());
    EXPECT_FALSE(measure->system()->staff(1)->show());
    EXPECT_FALSE(measure->system()->staff(2)->show());
    EXPECT_EQ(part->track_map_at_tick(measure->tick()).size(), 5u);
}

TEST(StaveSharingOutput, LateIncompatibleSystemRetiresOutputAndPreservesSourceQueries) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("<voice>"), "<LayoutBreak><subtype>line</subtype></LayoutBreak>");
    xml.insert(xml.find("</Style>"), "<allowVoiceCrossing>0</allowVoiceCrossing>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *part = master.shared_parts().front();
    auto *manager = master.transaction_manager();
    manager->begin_transaction(muse::TranslatableString::untranslatable("remove a shared staff"));
    master.cmd_remove_staff(1);
    manager->end_transaction();
    ASSERT_EQ(part->staves().size(), 1u);

    auto *first = master.first_measure();
    auto *second = first->next_measure();
    const auto upper_track = part->origin_parts().front()->track_range().start_track;
    const auto lower_track = part->origin_parts().back()->track_range().start_track;
    auto *first_segment = first->first(SegmentType::CHORD_REST);
    auto *later_segment = second->first(SegmentType::CHORD_REST)->next(SegmentType::CHORD_REST);
    auto *first_note = to_chord(first_segment->element(upper_track))->up_note();
    auto *later_note = to_chord(later_segment->element(lower_track))->up_note();
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_NE(first_note->shared_item(), nullptr);
    ASSERT_TRUE(first->system()->staff(0)->show());
    ASSERT_TRUE(second->system()->staff(0)->show());
    later_note->set_pitch(76);
    later_note->set_tpc1(18);
    later_note->set_tpc2(18);
    master.invalidate_layout(second->tick(), second->end_tick());

    for (bool suspended : {true, false}) {
        const auto semantic = music::semantic_dump(&master);
        const auto metadata = music::score_metadata(&master, std::vector<music::ScoreMetadataTrack>{});
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(first->system(), second->system());
        EXPECT_TRUE(music::json_equal(music::semantic_dump(&master), semantic));
        EXPECT_TRUE(music::json_equal(music::score_metadata(&master, std::vector<music::ScoreMetadataTrack>{}), metadata));
        for (auto *measure : {first, second}) {
            auto *system = measure->system();
            EXPECT_EQ(system->staff(0)->show(), !suspended);
            EXPECT_EQ(system->staff(track2staff(upper_track))->show(), suspended);
            EXPECT_EQ(system->staff(track2staff(lower_track))->show(), suspended);
            if (!suspended) continue;
            for (auto *segment = measure->first(); segment; segment = segment->next()) {
                EXPECT_TRUE(segment->ldata()->display_copies.empty());
                EXPECT_TRUE(segment->ldata()->display_annotations.empty());
                EXPECT_TRUE(segment->ldata()->suppressed_source_tracks.empty());
                EXPECT_TRUE(segment->ldata()->suppressed_annotations.empty());
                EXPECT_FALSE(segment->ldata()->suppressed);
                for (auto track : {upper_track, lower_track}) {
                    auto *item = segment->element(track);
                    if (!item || !item->is_chord()) continue;
                    EXPECT_EQ(segment->effective_element(track), item);
                    EXPECT_EQ(to_chord(item)->up_note()->shared_item(), nullptr);
                }
            }
        }
        if (!suspended) {
            auto *copy = first_note->shared_item();
            ASSERT_NE(copy, nullptr);
            EXPECT_TRUE(std::ranges::contains(copy->origin_items(), first_note));
            break;
        }
        later_note->set_pitch(74);
        later_note->set_tpc1(16);
        later_note->set_tpc2(16);
        master.invalidate_layout(second->tick(), second->end_tick());
    }
}

TEST(StaveSharingOutput, LateSlurSplitPublishesSegmentsOnAllThreePages) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (int id : {1, 2, 3, 4}) {
        const auto staff = xml.find(std::format(R"(<Staff id="{}">)", id));
        const auto first = xml.find("<Measure>", staff);
        const auto second = xml.find("<Measure>", xml.find("</Measure>", first));
        const auto end = xml.find("</Measure>", second) + std::string_view("</Measure>").size();
        xml.insert(end, xml.substr(second, end - second));
        if (id == 1) {
            const std::string page_break = "<LayoutBreak><subtype>page</subtype></LayoutBreak>";
            xml.insert(second + std::string_view("<Measure>").size(), page_break);
            xml.insert(first + std::string_view("<Measure>").size(), page_break);
        }
    }
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure();
    auto *second = first->next_measure();
    auto *third = second->next_measure();
    ASSERT_NE(third, nullptr);
    auto *start = first->first(SegmentType::CHORD_REST);
    auto *end = third->first(SegmentType::CHORD_REST)->next(SegmentType::CHORD_REST);
    auto *part = master.shared_parts().front();
    part->set_authored_state(first->tick(), {{{8, 0}, {12, 0}}, false});
    part->set_authored_state(third->tick(), {{{8, 0}, {12, 1}}, false});
    auto make_slur = [&](track_idx_t track) {
        auto *slur = Factory::create_slur(master.dummy());
        slur->set_track(track);
        slur->set_track2(track);
        slur->set_tick(start->tick());
        slur->set_tick2(end->tick());
        master.add_element(slur);
        slur->assign_new_eid();
        return slur;
    };
    auto *upper = make_slur(8);
    auto *lower = make_slur(12);
    for (int pass = 0; pass < 2; ++pass) {
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_EQ(master.pages().size(), 3u);
        const auto *a = to_slur(master.layout_copy(*upper));
        const auto *b = to_slur(master.layout_copy(*lower));
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        ASSERT_NE(a, b);
        for (const auto *copy : {a, b}) {
            ASSERT_EQ(copy->nsegments(), 3u);
            for (auto *measure : {first, second, third}) {
                const auto it = std::ranges::find(copy->spanner_segments(), measure->system(), &SpannerSegment::system);
                ASSERT_NE(it, copy->spanner_segments().end());
                EXPECT_GT((*it)->ldata()->bbox().width(), 0.0);
            }
        }
        master.invalidate_layout(third->tick(), third->end_tick());
    }
}

TEST(StaveSharingOutput, LateHairpinAssignmentRelayoutsEarlierSegment) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("<voice>"), "<LayoutBreak><subtype>page</subtype></LayoutBreak>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure();
    auto *second = first->next_measure();
    auto *start = first->first(SegmentType::CHORD_REST)->next(SegmentType::CHORD_REST);
    auto *end = second->first(SegmentType::CHORD_REST)->next(SegmentType::CHORD_REST);
    auto *part = master.shared_parts().front();
    part->set_authored_state(first->tick(), {{{8, 0}, {12, 1}}, false});
    auto *hairpin = Factory::create_hairpin(master.dummy());
    hairpin->set_track(8);
    hairpin->set_track2(8);
    hairpin->set_tick(start->tick());
    hairpin->set_tick2(end->tick());
    hairpin->set_property(Pid::CENTER_BETWEEN_STAVES, AutoOnOff::OFF);
    master.add_element(hairpin);
    hairpin->assign_new_eid();

    for (bool staff_wide : {true, false, true}) {
        part->set_authored_state(second->tick(), staff_wide ? AuthoredStaveSharingState{{{8, 1}, {12, 0}}, false} :
                                                              AuthoredStaveSharingState{{{8, 0}, {12, 1}}, false});
        master.invalidate_layout(second->tick(), second->end_tick());
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(first->system()->page(), second->system()->page());
        const auto *copy = to_hairpin(master.layout_copy(*hairpin));
        ASSERT_NE(copy, nullptr);
        EXPECT_EQ(copy->voice_assignment(), staff_wide ? VoiceAssignment::ALL_VOICE_IN_STAFF : VoiceAssignment::CURRENT_VOICE_ONLY);
        EXPECT_EQ(copy->placement(), staff_wide ? PlacementV::BELOW : PlacementV::ABOVE);
        ASSERT_EQ(copy->nsegments(), 2u);
        for (const auto *segment : copy->spanner_segments()) {
            EXPECT_EQ(segment->placement(), copy->placement());
            EXPECT_GT(segment->ldata()->bbox().width(), 0.0);
            if (staff_wide) {
                EXPECT_GT(segment->pos().y(), 0.0);
            } else {
                EXPECT_LT(segment->pos().y(), 0.0);
            }
        }
    }
}
