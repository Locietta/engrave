#include "engraving/dom/dummyparent.h"
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
