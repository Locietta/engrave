#include "engrave/src/score/score_metadata.h"
#include "engrave/src/score/score_semantic.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/jianpuoctavedot.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/stavesharingchange.h"
#include "engraving/dom/systemlockindicator.h"
#include "engraving/editing/edittimesig.h"
#include "engraving/rendering/score/layoutcontext.h"
#include "engraving/rendering/score/pagelayout.h"
#include "engraving/rendering/score/scorepageviewlayout.h"
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

// A shared staff shows its mapped origins' notes, so a Jianpu digit and its octave dots
// count from the key of the origin they came from at each map, not from the shared
// staff's own C.
TEST(StaveSharingOutput, SharedJianpuDigitsCountFromTheMappedOriginKey) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    constexpr std::string_view jianpu = R"(<StaffType group="jianpu"><name>jianpu</name><lines>0</lines></StaffType>)";
    for (const std::string_view eid : {"<eid>B_B</eid>", "<eid>C_C</eid>"}) xml.insert(xml.find(eid) + eid.size(), jianpu);
    // D major on the first origin; the same signature numbered as E minor ("6=E") on the second.
    const std::pair<int, std::string_view> keys[] = {
        {3, "<KeySig><concertKey>2</concertKey></KeySig>"},
        {4, "<KeySig><concertKey>2</concertKey><jianpuNumbering>custom</jianpuNumbering><jianpuTonicKey>E</jianpuTonicKey>"
            "<jianpuTonicMode>minor</jianpuTonicMode></KeySig>"},
    };
    for (const auto &[staff, keysig] : keys) {
        const auto start = xml.find(std::format("<Staff id=\"{}\">", staff));
        xml.insert(xml.find("<voice>", start) + std::string_view("<voice>").size(), keysig);
    }
    // Every measure plays D5 then E5.
    for (const auto &[from, to] : {std::pair{"<pitch>74</pitch><tpc>16", "<pitch>76</pitch><tpc>18"},
                                   std::pair{"<pitch>72</pitch><tpc>14", "<pitch>74</pitch><tpc>16"}}) {
        for (size_t pos = xml.find(from); pos != std::string::npos; pos = xml.find(from, pos + std::string_view(to).size()))
            xml.replace(pos, std::string_view(from).size(), to);
    }
    // The first origin on the upper shared staff, then on the lower one.
    xml.insert(xml.find("</SharedPart>"),
               R"(<StaveSharingState tick="0/1"><trackMap origin="8" shared="0"/><trackMap origin="12" shared="4"/></StaveSharingState>)"
               R"(<StaveSharingState tick="1/1"><trackMap origin="8" shared="4"/><trackMap origin="12" shared="0"/></StaveSharingState>)");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_TRUE(master.staff(0)->is_jianpu_staff(Fraction(0, 1)));
    ASSERT_EQ(master.staff(0)->key(Fraction(0, 1)), Key::C);
    const auto cold = source_native(master);

    struct Expected {
        Fraction tick;
        track_idx_t track;
        std::string_view digit;
        size_t dots_above;
    };

    // Under D major D5 is "1" and E5 "2", two octaves above D3; numbered from E minor
    // they are "5" and "6", one octave above the "6" of E3. In C they would be "2" and "3".
    const Expected expected[] = {
        {{0, 1}, 0, "1", 2}, {{1, 2}, 0, "2", 2}, {{0, 1}, 4, "5", 1}, {{1, 2}, 4, "6", 1},
        {{1, 1}, 0, "5", 1}, {{3, 2}, 0, "6", 1}, {{1, 1}, 4, "1", 2}, {{3, 2}, 4, "2", 2},
    };
    for (int cycle = 0; cycle < 2; ++cycle) {
        SCOPED_TRACE(cycle);
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        const auto *part = master.shared_parts().front();
        for (const Fraction tick : {Fraction(0, 1), Fraction(1, 1)}) {
            ASSERT_NE(part->rendered_state_at_tick(tick), nullptr);
            EXPECT_TRUE(part->rendered_state_at_tick(tick)->is_user_map_valid) << tick.ticks();
        }
        for (const auto &[tick, track, digit, dots_above] : expected) {
            SCOPED_TRACE(std::format("tick {} on track {}", tick.ticks(), track));
            auto *segment = master.tick2segment(tick, true, SegmentType::CHORD_REST);
            ASSERT_NE(segment, nullptr);
            auto *shown = segment->effective_element(track);
            ASSERT_TRUE(shown && shown->is_chord() && shown->is_layout_copy());
            const Note *note = to_chord(shown)->up_note();
            EXPECT_EQ(note->jianpu_digit().to_std_string(), digit);
            EXPECT_TRUE(note->jianpu_accidental().is_empty());
            ASSERT_EQ(note->jianpu_octave_dots().size(), dots_above);
            for (const auto *dot : note->jianpu_octave_dots()) EXPECT_TRUE(dot->ldata()->above);
        }
        EXPECT_EQ(source_native(master), cold);
        master.layout_data_store()->clear();
    }
}

// A shared Jianpu unison shows one digit for all its origins, so origins share a voice only
// where their keys number alike. Here the keys differ from the first measure on, before
// the range of the later map, and each origin keeps the digits of its own key throughout.
TEST(StaveSharingOutput, SharedJianpuUnisonsKeepEachOriginsInheritedKey) {
    using namespace mu::engraving;
    for (const bool authored : {true, false}) {
        SCOPED_TRACE(authored ? "authored unison map from the second measure" : "automatic map on a second system");
        auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
        constexpr std::string_view jianpu = R"(<StaffType group="jianpu"><name>jianpu</name><lines>0</lines></StaffType>)";
        for (const std::string_view eid : {"<eid>B_B</eid>", "<eid>C_C</eid>"}) xml.insert(xml.find(eid) + eid.size(), jianpu);
        // D major on the first origin from its first measure; the second origin stays in C.
        const auto first_origin = xml.find("<Staff id=\"3\">");
        xml.insert(xml.find("<voice>", first_origin) + std::string_view("<voice>").size(), "<KeySig><concertKey>2</concertKey></KeySig>");
        // Both origins play D5 then E5 in every measure.
        for (const auto &[from, to] : {std::pair{"<pitch>74</pitch><tpc>16", "<pitch>76</pitch><tpc>18"},
                                       std::pair{"<pitch>72</pitch><tpc>14", "<pitch>74</pitch><tpc>16"}}) {
            for (size_t pos = xml.find(from); pos != std::string::npos; pos = xml.find(from, pos + std::string_view(to).size()))
                xml.replace(pos, std::string_view(from).size(), to);
        }
        if (authored) {
            xml.insert(xml.find("</SharedPart>"),
                       R"(<StaveSharingState tick="1/1"><trackMap origin="8" shared="0"/><trackMap origin="12" shared="0"/>)"
                       R"(</StaveSharingState>)");
        } else {
            xml.insert(xml.find("<voice>"), "<LayoutBreak><subtype>line</subtype></LayoutBreak>");
        }
        auto document = open_text(xml);
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        ASSERT_EQ(master.staff(2)->key(Fraction(1, 1)), Key::D);
        ASSERT_EQ(master.staff(3)->key(Fraction(1, 1)), Key::C);
        const auto cold = source_native(master);
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        const auto *part = master.shared_parts().front();
        if (authored) {
            const auto *state = part->rendered_state_at_tick(Fraction(1, 1));
            ASSERT_NE(state, nullptr);
            EXPECT_FALSE(state->is_user_map_valid);
        } else {
            ASSERT_NE(master.first_measure()->system(), master.first_measure()->next_measure()->system());
        }

        // D5 and E5 are "1" and "2" in D major, "2" and "3" in C.
        const std::pair<track_idx_t, std::array<std::string_view, 2>> digits[] = {{8, {"1", "2"}}, {12, {"2", "3"}}};
        for (const Fraction tick : {Fraction(0, 1), Fraction(1, 2), Fraction(1, 1), Fraction(3, 2)}) {
            auto *segment = master.tick2segment(tick, true, SegmentType::CHORD_REST);
            ASSERT_NE(segment, nullptr);
            const size_t half = tick.ticks() % Fraction(1, 1).ticks() == 0 ? 0 : 1;
            for (const auto &[track, expected] : digits) {
                SCOPED_TRACE(std::format("tick {} from track {}", tick.ticks(), track));
                const EngravingItem *origin = segment->element(track);
                ASSERT_TRUE(origin && origin->is_chord());
                const EngravingItem *shown = to_chord(origin)->up_note()->shared_item();
                ASSERT_TRUE(shown && shown->is_note());
                EXPECT_EQ(shown->origin_items().size(), 1u);
                EXPECT_EQ(to_note(shown)->jianpu_digit().to_std_string(), expected[half]);
            }
        }
        EXPECT_EQ(source_native(master), cold);
    }
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

TEST(StaveSharingOutput, HiddenSharedPartDoesNotBlockAnUnrelatedMultiMeasureRest) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("<Staff id=\"1\">"), "<Part><Staff/><Instrument id=\"flute\"/></Part>");
    constexpr std::string_view rest = "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>";
    xml.insert(xml.find("</Score>"), std::format("<Staff id=\"5\">{}{}</Staff>", rest, rest));
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *shared = master.shared_parts().front();
    shared->set_show(false);
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
    expect_no_layout(master);
    ASSERT_TRUE(shared->enabled());
    ASSERT_FALSE(shared->show());
    ASSERT_FALSE(master.staff(2)->layout_show());
    ASSERT_TRUE(master.staff(4)->layout_show());
    auto *first = master.first_measure();
    ASSERT_TRUE(first->first(SegmentType::CHORD_REST)->element(8)->is_chord());

    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    auto *display = first->mm_rest();
    ASSERT_NE(display, nullptr);
    EXPECT_EQ(display->mm_rest_count(), 2);
    EXPECT_EQ(first->next_measure()->covering_mm_rest_or_this(), display);
    const auto *rest_item = display->first(SegmentType::CHORD_REST)->effective_element(16);
    ASSERT_NE(rest_item, nullptr);
    EXPECT_TRUE(rest_item->is_mm_rest());
    EXPECT_TRUE(display->system()->staff(4)->show());
}

TEST(StaveSharingOutput, OriginDynamicStartsANewMultiMeasureRestOnColdLayout) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing-mmr.mscx");
    constexpr std::string_view rest = "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>";
    for (int staff = 1; staff <= 4; ++staff) {
        const auto staff_start = xml.find(std::format("<Staff id=\"{}\">", staff));
        xml.insert(xml.find("</Staff>", staff_start), rest);
        if (staff >= 3) {
            const auto first_end = xml.find("</Measure>", staff_start);
            const auto second_voice = xml.find("<voice>", first_end) + std::string_view("<voice>").size();
            xml.insert(second_voice, "<Dynamic><subtype>f</subtype></Dynamic>");
        }
    }
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.style().set(Sid::MIN_EMPTY_MEASURES, 2);
    expect_no_layout(master);
    ASSERT_FALSE(master.staff(2)->layout_show());
    auto *first = master.first_measure();
    auto *second = first->next_measure();
    auto *segment = second->first(SegmentType::CHORD_REST);
    ASSERT_EQ(segment->annotations().size(), 2u);
    auto *dynamic = segment->annotations().front();
    ASSERT_TRUE(dynamic->is_dynamic());
    ASSERT_EQ(dynamic->track(), 8u);

    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(first->covering_mm_rest_or_this(), first);
    auto *display = second->mm_rest();
    ASSERT_NE(display, nullptr);
    EXPECT_EQ(display->mm_rest_count(), 2);
    EXPECT_EQ(second->next_measure()->covering_mm_rest_or_this(), display);
    const auto annotations = display->first(SegmentType::CHORD_REST)->rendered_annotations();
    const auto copy = std::ranges::find_if(annotations, [](const auto *item) { return item->is_dynamic() && item->track() < 8; });
    ASSERT_NE(copy, annotations.end());
    EXPECT_EQ(to_dynamic(*copy)->dynamic_type(), DynamicType::F);
    EXPECT_GT((*copy)->width(), 0.0);
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

static void expect_stave_sharing_after_meter_rewrite(mu::engraving::Fraction meter, bool stop_at_second_measure,
                                                     const std::vector<mu::engraving::Fraction> &rewritten_lengths,
                                                     mu::engraving::Fraction end_tick,
                                                     const mu::engraving::AuthoredStaveSharingStates &rewritten_master_states,
                                                     const mu::engraving::AuthoredStaveSharingStates &rewritten_excerpt_states) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    if (stop_at_second_measure) {
        // The explicit second signature stops the rewrite after the first 4/4 measure.
        for (int staff = 1; staff <= 4; ++staff) {
            const auto first_end = xml.find("</Measure>", xml.find(std::format("<Staff id=\"{}\">", staff)));
            constexpr std::string_view measure_voice = "<Measure><voice>";
            xml.insert(xml.find(measure_voice, first_end) + measure_voice.size(), "<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>");
        }
    }
    const auto score_end = xml.find("</Score>");
    const auto part_start = xml.find("<SharedPart>");
    auto excerpt = xml.substr(part_start, score_end - part_start);
    for (char id = 'A'; id <= 'G'; ++id) {
        const auto old_eid = std::format("{}_{}", id, id);
        const auto new_eid = std::format("{}_{}", id, static_cast<char>(id + ('a' - 'A')));
        for (size_t pos = excerpt.find(old_eid); pos != std::string::npos; pos = excerpt.find(old_eid, pos + new_eid.size())) {
            excerpt.replace(pos, old_eid.size(), new_eid);
        }
        if (std::string_view("BCEG").find(id) != std::string_view::npos) {
            const auto eid_end = excerpt.find("</eid>", excerpt.find(new_eid)) + std::string_view("</eid>").size();
            excerpt.insert(eid_end, std::format("<linkedTo>{}</linkedTo>", old_eid));
        }
    }
    xml.insert(score_end, "<Score><name>Shared flutes</name><Division>480</Division>" + excerpt + "</Score>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);

    const AuthoredStaveSharingStates master_states{
        {Fraction(0, 1), {{{8, 0}, {12, 1}}, false}},
        {Fraction(1, 1), {{}, true}},
        {Fraction(5, 4), {{{8, 1}, {12, 0}}, false}},
    };
    const AuthoredStaveSharingStates excerpt_states{
        {Fraction(0, 1), {{{8, 4}, {12, 5}}, false}},
        {Fraction(3, 4), {{}, true}},
        {Fraction(1, 1), {{{8, 5}, {12, 4}}, false}},
        {Fraction(5, 4), {{}, true}},
    };
    auto &master = master_of(*document);
    ASSERT_EQ(master.excerpts().size(), 1u);
    auto *excerpt_score = master.excerpts().front()->excerpt_score();
    ASSERT_EQ(master.shared_parts().size(), 1u);
    ASSERT_EQ(excerpt_score->shared_parts().size(), 1u);
    for (const auto &[tick, state] : master_states) master.shared_parts().front()->set_authored_state(tick, state);
    for (const auto &[tick, state] : excerpt_states) excerpt_score->shared_parts().front()->set_authored_state(tick, state);

    const auto expect_score = [&](engrave::Document &doc, const std::vector<Fraction> &bar_lengths, Fraction expected_end_tick,
                                  const AuthoredStaveSharingStates &expected_master, const AuthoredStaveSharingStates &expected_excerpt) {
        auto &score = master_of(doc);
        ASSERT_EQ(score.excerpts().size(), 1u);
        const auto expect_part = [&](Score &view, const AuthoredStaveSharingStates &expected) {
            ASSERT_EQ(view.shared_parts().size(), 1u);
            EXPECT_EQ(view.shared_parts().front()->authored_states(), expected);
            std::vector<Fraction> actual_lengths;
            for (auto *measure = view.first_measure(); measure; measure = measure->next_measure()) {
                actual_lengths.push_back(measure->ticks());
            }
            EXPECT_EQ(actual_lengths, bar_lengths);
            EXPECT_EQ(view.end_tick(), expected_end_tick);
        };
        expect_part(score, expected_master);
        expect_part(*score.excerpts().front()->excerpt_score(), expected_excerpt);
    };

    const auto expect_original_score = [&] { expect_score(*document, {{1, 1}, {1, 1}}, {2, 1}, master_states, excerpt_states); };
    const auto expect_rewritten_score = [&](engrave::Document &doc) {
        expect_score(doc, rewritten_lengths, end_tick, rewritten_master_states, rewritten_excerpt_states);
    };
    expect_original_score();
    ASSERT_TRUE(document->checkpoint("before rewrite"));
    auto *manager = master.transaction_manager();
    manager->begin_transaction(muse::TranslatableString::untranslatable("rewrite meter"));
    auto *first = master.first_measure();
    auto *signature = Factory::create_time_sig(first->find_segment(SegmentType::TIME_SIG, first->tick()));
    signature->set_sig(meter);
    EditTimeSig::add_time_sig(manager->current_or_dummy_transaction(), &master, first, 2, signature, false);
    manager->end_transaction();
    engrave::assign_missing_ids(master);
    expect_rewritten_score(*document);

    ASSERT_TRUE(document->checkpoint("after rewrite"));
    ASSERT_TRUE(document->restore_checkpoint("before rewrite"));
    expect_original_score();
    ASSERT_TRUE(document->restore_checkpoint("after rewrite"));
    expect_rewritten_score(*document);

    const auto saved = document->export_score({.format = engrave::Format::MSCZ});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened);
    expect_rewritten_score(**reopened);
}

TEST(StaveSharingOutput, UnpaddedMeterRewritePreservesAuthoredStateTicks) {
    expect_stave_sharing_after_meter_rewrite(
        {2, 4}, false, {{1, 2}, {1, 2}, {1, 2}, {1, 2}}, {2, 1},
        {{{0, 1}, {{{8, 0}, {12, 1}}, false}}, {{1, 1}, {{}, true}}, {{5, 4}, {{{8, 1}, {12, 0}}, false}}},
        {{{0, 1}, {{{8, 4}, {12, 5}}, false}}, {{3, 4}, {{}, true}}, {{1, 1}, {{{8, 5}, {12, 4}}, false}}, {{5, 4}, {{}, true}}});
}

TEST(StaveSharingOutput, PaddedMeterRewriteShiftsAuthoredStateTicks) {
    expect_stave_sharing_after_meter_rewrite(
        {3, 4}, true, {{3, 4}, {3, 4}, {1, 1}}, {5, 2},
        {{{0, 1}, {{{8, 0}, {12, 1}}, false}}, {{3, 2}, {{}, true}}, {{7, 4}, {{{8, 1}, {12, 0}}, false}}},
        {{{0, 1}, {{{8, 4}, {12, 5}}, false}}, {{3, 4}, {{}, true}}, {{3, 2}, {{{8, 5}, {12, 4}}, false}}, {{7, 4}, {{}, true}}});
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
    auto *manager = master.transaction_manager();
    manager->begin_transaction(muse::TranslatableString::untranslatable("remove a shared staff"));
    master.cmd_remove_staff(1);
    manager->end_transaction();
    ASSERT_EQ(part->staves().size(), 1u);
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
    master.style().set(Sid::MIN_EMPTY_MEASURES, 1);
    auto *second = measure->next_measure();
    ASSERT_NE(second, nullptr);
    expect_no_layout(master);
    ASSERT_TRUE(master.staff(0)->layout_show());
    ASSERT_FALSE(master.staff(1)->layout_show());
    ASSERT_FALSE(master.staff(2)->layout_show());
    const auto cold = source_native(master);
    const auto objects = engrave::document_objects(master);
    for (int pass = 0; pass < 2; ++pass) {
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        std::vector<int> visible_pitches;
        for (auto *bar : {measure, second}) {
            EXPECT_EQ(bar->mm_rest(), nullptr);
            EXPECT_EQ(bar->covering_mm_rest_or_this(), bar);
            ASSERT_NE(bar->system(), nullptr);
            EXPECT_FALSE(bar->system()->staff(0)->show());
            EXPECT_TRUE(bar->system()->staff(1)->show());
            EXPECT_TRUE(bar->system()->staff(2)->show());
            for (auto *chord_rest = bar->first(SegmentType::CHORD_REST); chord_rest;
                 chord_rest = chord_rest->next(SegmentType::CHORD_REST)) {
                chord_rest->scan_elements([&](const auto *item) {
                    if (item->is_note()) visible_pitches.push_back(to_note(item)->pitch());
                });
            }
        }
        std::ranges::sort(visible_pitches);
        EXPECT_EQ(visible_pitches, (std::vector{72, 72, 72, 74, 74, 74, 74, 77, 79, 81, 84}));
        EXPECT_EQ(source_native(master), cold);
        EXPECT_EQ(engrave::document_objects(master), objects);
        master.invalidate_layout(measure->tick(), measure->end_tick());
    }

    fourth->set_pitch(70);
    fourth->set_tpc1(12);
    fourth->set_tpc2(12);
    master.invalidate_layout(measure->tick(), measure->end_tick());
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(measure->covering_mm_rest_or_this(), measure);
    EXPECT_EQ(second->covering_mm_rest_or_this(), second);
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
    for (const auto &[from, to] : {std::pair{"id=\"flute\"", "id=\"soprano\""}, std::pair{"wind.flutes.flute", "voice.soprano"},
                                   std::pair{"<pitch>72</pitch><tpc>14", "<pitch>69</pitch><tpc>17"}}) {
        for (size_t pos = xml.find(from); pos != std::string::npos; pos = xml.find(from, pos + std::string_view(to).size())) {
            xml.replace(pos, std::string_view(from).size(), to);
        }
    }
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
    auto *dynamic = Factory::create_dynamic(first_segment);
    dynamic->set_track(upper_track);
    dynamic->set_dynamic_type(DynamicType::P);
    dynamic->set_property(Pid::PLACEMENT, PlacementV::BELOW);
    dynamic->set_property(Pid::CENTER_BETWEEN_STAVES, AutoOnOff::ON);
    first_segment->add(dynamic);
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
            EXPECT_GT(system->height(), 0.0);
            const auto &upper_box = system->staff(track2staff(upper_track))->bbox();
            const auto &lower_box = system->staff(track2staff(lower_track))->bbox();
            if (suspended) {
                EXPECT_GT(upper_box.height(), 0.0);
                EXPECT_GT(lower_box.height(), 0.0);
                EXPECT_GT(lower_box.top(), upper_box.bottom());
                EXPECT_EQ(system->next_visible_staff(track2staff(upper_track)), track2staff(lower_track));
                EXPECT_EQ(system->prev_visible_staff(track2staff(lower_track)), track2staff(upper_track));
                if (measure == first) {
                    ASSERT_TRUE(dynamic->has_layout_data());
                    const auto *layout = dynamic->ldata();
                    const double center = upper_box.top() + layout->pos().y() + layout->bbox().center().y();
                    EXPECT_NEAR(center, (upper_box.bottom() + lower_box.top()) * 0.5, 0.1 * dynamic->spatium());
                }
            } else {
                EXPECT_GT(system->staff(0)->bbox().height(), 0.0);
                EXPECT_TRUE(upper_box.is_empty());
                EXPECT_TRUE(lower_box.is_empty());
            }
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
        for (uint32_t i = 0; i < 3; ++i) {
            slur->authored_overrides.set({{AuthoredChildRole::SPANNER_SEGMENT, i},
                                          Pid::SLUR_UOFF1,
                                          PointF(0.25 * (i + 1), 0.0),
                                          PropertyFlags::NOSTYLE,
                                          AuthoredOverrideUnit::SPATIUM});
        }
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
            const std::array measures{first, second, third};
            for (size_t i = 0; i < measures.size(); ++i) {
                auto *segment = to_slur_segment(copy->spanner_segments()[i]);
                EXPECT_EQ(segment->system(), measures[i]->system());
                EXPECT_EQ(segment->authored_child.ordinal, i);
                EXPECT_GT(segment->ldata()->bbox().width(), 0.0);
                EXPECT_NEAR(segment->ups(Grip::START).off.x(), 0.25 * (i + 1) * segment->spatium(), 1e-6);
                const auto &path = segment->ldata()->path();
                ASSERT_FALSE(path.is_empty());
                EXPECT_NEAR(path.element_at(0).x, segment->ups(Grip::START).pos().x(), 1e-6);
            }
        }
        master.invalidate_layout(third->tick(), third->end_tick());
    }
}

TEST(StaveSharingOutput, LateTextLineExtensionRepairsEarlierSystemSegments) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    for (int id = 1; id <= 4; ++id) {
        const auto staff = xml.find(std::format(R"(<Staff id="{}">)", id));
        const auto second = xml.find("<Measure>", xml.find("</Measure>", staff));
        const auto end = xml.find("</Measure>", second) + std::string_view("</Measure>").size();
        const auto bar = xml.substr(second, end - second);
        xml.insert(end, bar + bar);
        if (id == 1) {
            auto pos = staff;
            for (int i = 0; i < 3; ++i) {
                pos = xml.find("<Measure>", pos) + std::string_view("<Measure>").size();
                xml.insert(pos, "<LayoutBreak><subtype>page</subtype></LayoutBreak>");
            }
        }
    }
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure();
    auto *second = first->next_measure();
    auto *third = second->next_measure();
    auto *fourth = third->next_measure();
    ASSERT_NE(fourth, nullptr);

    auto *line = Factory::create_text_line(master.dummy());
    line->set_track(8);
    line->set_tick(first->tick());
    line->set_tick2(third->tick() + Fraction(1, 2));
    master.add_element(line);
    line->assign_new_eid();
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));

    using namespace mu::engraving::rendering::score;
    line->set_tick2(fourth->tick() + Fraction(1, 2));
    auto *retained_system = first->system();
    // A range pass can regenerate the line without laying out the retained first page.
    master.cmd_state().lock();
    master.layout_data_store()->retire_shared_auxiliary_output();
    LayoutContext ctx(&master);
    ScorePageViewLayout::layout_page_view(&master, ctx, third->tick(), fourth->end_tick());
    const auto *partial = to_text_line(master.layout_copy(*line));
    EXPECT_NE(partial, nullptr);
    if (partial) EXPECT_FALSE(std::ranges::contains(partial->spanner_segments(), retained_system, &SpannerSegment::system));
    PageLayout::repair_shared_spanners(master);
    master.cmd_state().unlock();

    EXPECT_EQ(first->system(), retained_system);
    ASSERT_EQ(master.pages().size(), 4u);
    const auto *copy = to_text_line(master.layout_copy(*line));
    ASSERT_NE(copy, nullptr);
    const std::array measures{first, second, third, fourth};
    ASSERT_EQ(copy->nsegments(), measures.size());
    for (size_t i = 0; i < measures.size(); ++i) {
        const auto *segment = copy->spanner_segments()[i];
        EXPECT_EQ(segment->system(), measures[i]->system());
        EXPECT_EQ(segment->authored_child.ordinal, i);
        EXPECT_GT(segment->ldata()->bbox().width(), 0.0);
    }
}

TEST(StaveSharingOutput, LateHairpinAssignmentRelayoutsEarlierSegment) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/stave-sharing.mscx");
    xml.insert(xml.find("<voice>"), "<LayoutBreak><subtype>page</subtype></LayoutBreak>");
    xml.insert(xml.find("</Style>"), "<hairpinHeight>16</hairpinHeight>");
    xml.insert(xml.find("<Staff id=\"1\">"), R"(<Part><Staff/><Instrument id="flute"><trackName>Flute</trackName>
<instrumentId>wind.flutes.flute</instrumentId></Instrument></Part>)");
    xml.insert(xml.find("</Score>"), R"(<Staff id="5">
<Measure><voice>
<Chord><durationType>half</durationType><Note><pitch>84</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>half</durationType><Note><pitch>84</pitch><tpc>14</tpc></Note></Chord>
</voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
</Staff>)");
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
                if (segment->system() == first->system()) {
                    const auto *lower_staff = first->system()->staff(4);
                    ASSERT_TRUE(lower_staff->show());
                    const auto bounds = segment->ldata()->bbox().translated(segment->pos());
                    EXPECT_LT(bounds.bottom(), lower_staff->bbox().top());
                    const auto *nearby_note = to_chord(start->element(staff2track(4)))->up_note();
                    EXPECT_LT(segment->page_pos().y() + segment->ldata()->bbox().bottom(),
                              nearby_note->page_pos().y() + nearby_note->ldata()->bbox().top());
                }
            } else {
                EXPECT_LT(segment->pos().y(), 0.0);
            }
        }
    }
}
