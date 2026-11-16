#include "engraving/dom/dummyparent.h"
#include "engraving/dom/guitarbend.h"
#include "read_test_fixture.h"

using namespace read_test;

TEST(LayoutBounds, ConstReadsLeaveShapeAndPathStorageUnchanged) {
    // Observe the same object's representation, including any hidden cache.
    // This detects the former first-read cache writes without exposing internals.
    const auto bytes = [](const auto &object) {
        std::array<std::byte, sizeof(object)> result;
        std::memcpy(result.data(), &object, sizeof(object));
        return result;
    };
    // Defined backing bytes keep object padding safe for memory tools.
    using Shape = mu::engraving::Shape;
    alignas(Shape) std::array<std::byte, sizeof(Shape)> shape_storage{};
    std::unique_ptr<Shape, void (*)(Shape *)> shape_owner(std::construct_at(reinterpret_cast<Shape *>(shape_storage.data())),
                                                          std::destroy_at<Shape>);
    auto &shape = *shape_owner;
    shape.add(muse::RectF(-2.0, 1.0, 3.0, 4.0));
    shape.add(muse::RectF(3.0, -1.0, 2.0, 8.0));
    const auto shape_bytes = bytes(shape);
    const auto elements = shape.elements();
    const auto &shape_view = std::as_const(shape);
    EXPECT_EQ(shape_view.bbox(), muse::RectF(-2.0, -1.0, 7.0, 8.0));
    EXPECT_EQ(shape_view.bbox(), muse::RectF(-2.0, -1.0, 7.0, 8.0));
    EXPECT_EQ(std::memcmp(shape_bytes.data(), &shape, sizeof(shape)), 0);
    EXPECT_EQ(shape_view.elements(), elements);
    // Direct mutable element access remains explicit; no stale bounds survive it.
    shape.elements().front().translate(-2.0, -3.0);
    EXPECT_EQ(shape_view.bbox(), muse::RectF(-4.0, -2.0, 9.0, 9.0));

    using Path = muse::draw::PainterPath;
    alignas(Path) std::array<std::byte, sizeof(Path)> path_storage{};
    std::unique_ptr<Path, void (*)(Path *)> path_owner(std::construct_at(reinterpret_cast<Path *>(path_storage.data())),
                                                       std::destroy_at<Path>);
    auto &path = *path_owner;
    path.add_rect(muse::RectF(-2.0, -1.0, 7.0, 8.0));
    std::vector<Path::Element> path_elements;
    for (size_t i = 0; i < path.element_count(); ++i) path_elements.push_back(path.element_at(i));
    const auto path_bytes = bytes(path);
    const auto &path_view = std::as_const(path);
    EXPECT_EQ(path_view.bounding_rect(), muse::RectF(-2.0, -1.0, 7.0, 8.0));
    EXPECT_EQ(path_view.bounding_rect(), muse::RectF(-2.0, -1.0, 7.0, 8.0));
    EXPECT_EQ(std::memcmp(path_bytes.data(), &path, sizeof(path)), 0);
    ASSERT_EQ(path_view.element_count(), path_elements.size());
    for (size_t i = 0; i < path_elements.size(); ++i) {
        EXPECT_EQ(path_view.element_at(i).x, path_elements[i].x);
        EXPECT_EQ(path_view.element_at(i).y, path_elements[i].y);
        EXPECT_EQ(path_view.element_at(i).type, path_elements[i].type);
    }
    path.translate(2.0, 3.0);
    EXPECT_EQ(path_view.bounding_rect(), muse::RectF(0.0, 2.0, 7.0, 8.0));
    EXPECT_TRUE(muse::draw::PainterPath().bounding_rect().is_null());
}

TEST(LayoutBounds, CompositeShapeReadsConsumePreparedOutputOnly) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    const auto probe = [&](EngravingItem &item) {
        const Shape prepared(muse::RectF(1.25, 2.5, 3.75, 4.5), &item);
        item.mutldata()->set_shape(prepared);
        const EngravingItem &view = item;
        ItemLayoutSpy spy;
        EXPECT_TRUE(view.shape().equal(prepared)) << item.type_name();
        EXPECT_TRUE(view.shape().equal(prepared)) << item.type_name();
        EXPECT_TRUE(view.ldata()->shape(LdAccess::PASS).equal(prepared));
        EXPECT_EQ(ItemLayoutSpy::count, 0u);
    };
    std::unique_ptr<Chord> chord(Factory::create_chord(master.dummy()));
    probe(*chord);
    std::unique_ptr<Rest> rest(Factory::create_rest(master.dummy()));
    probe(*rest);
    std::unique_ptr<MeasureRepeat> repeat(Factory::create_measure_repeat(master.dummy()));
    probe(*repeat);
    std::unique_ptr<MMRest> mm_rest(Factory::create_mm_rest(master.dummy()));
    probe(*mm_rest);
    std::unique_ptr<Tuplet> tuplet(Factory::create_tuplet(master.dummy()));
    probe(*tuplet);
    std::unique_ptr<GuitarBend> bend(Factory::create_guitar_bend(master.dummy()));
    std::unique_ptr<LineSegment> bend_segment(bend->create_line_segment());
    probe(*bend_segment);
    std::unique_ptr<Trill> trill(Factory::create_trill(master.dummy()));
    std::unique_ptr<LineSegment> trill_segment(trill->create_line_segment());
    probe(*trill_segment);
    std::unique_ptr<Hairpin> hairpin(Factory::create_hairpin(master.dummy()));
    std::unique_ptr<LineSegment> hairpin_segment(hairpin->create_line_segment());
    probe(*hairpin_segment);
}

TEST(LayoutBounds, PublishedShapesDoNotInvokeLayoutOrChangePayloads) {
    using namespace mu::engraving;
    for (const char *fixture : {"tests/data/inputs/golden/chordline_path.mscx", "tests/data/inputs/golden/layout_elements.mscx",
                                "tests/data/inputs/golden/tab_grace_prebend.mscx",
                                "src/engraving/tests/playback/playbackeventsrenderer_data/trill_line_on_tied_notes.mscx",
                                "src/engraving/tests/playback/playbackmodel_data/spanners/spanners.mscx"}) {
        SCOPED_TRACE(fixture);
        auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / fixture);
        ASSERT_TRUE(opened);
        auto &document = **opened;
        ASSERT_TRUE(document.page_count());
        ASSERT_TRUE(document.positions(engrave::PositionKind::SEGMENT));
        auto &master = master_of(document);
        auto &store = master.renderer()->layout_data_store(master);
        const auto size = store.size();
        const auto revision = document.revision();
        const auto layouts = master.layout_state.whole_count + master.layout_state.range_count;
        ItemLayoutSpy spy;
        store.for_each_output([&](EngravingItem &item, EngravingItem::LayoutData &data) {
            const EngravingItem &view = item;
            const auto before = data.shape(LdAccess::PASS);
            for (int i = 0; i < 2; ++i) {
                std::ignore = view.shape();
                std::ignore = view.ldata()->bbox();
            }
            EXPECT_TRUE(before.equal(data.shape(LdAccess::PASS))) << item.type_name();
        });
        EXPECT_EQ(ItemLayoutSpy::count, 0u);
        EXPECT_EQ(store.size(), size);
        EXPECT_EQ(document.revision(), revision);
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, layouts);
    }
}

TEST(LayoutBounds, CompletionSkipsDetachedTrillAfterOutputReset) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    std::unique_ptr<Trill> trill(Factory::create_trill(master.dummy()));
    std::unique_ptr<LineSegment> segment(trill->create_line_segment());
    const Shape prepared(muse::RectF(1.25, 2.5, 3.75, 4.5), segment.get());
    auto *payload = segment->mutldata();
    payload->set_shape(prepared);
    segment->set_spanner(nullptr);
    ASSERT_EQ(segment->ownership_parent(), nullptr);
    for (int i = 0; i < 2; ++i) {
        master.invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
        ASSERT_TRUE(document.page_count());
        EXPECT_EQ(segment->ldata(), payload);
        // The full reset pass clears scratch too; completion must leave this
        // detached payload empty instead of consulting its missing spanner.
        EXPECT_TRUE(segment->shape().empty());
    }
}

TEST(LayoutBounds, TablatureBracketPreparesItsOwnerBeforeCollisionPlacement) {
    using namespace mu::engraving;
    using namespace rendering::score;
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/tab_grace_prebend.mscx");
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    Chord *owner = nullptr;
    for (Measure *measure = master.first_measure(); measure && !owner; measure = measure->next_measure()) {
        for (Segment &segment : measure->segments()) {
            for (EngravingItem *item : segment.elist()) {
                if (item && item->is_chord() && item->on_tab_staff()) {
                    owner = to_chord(item);
                    break;
                }
            }
            if (owner) break;
        }
    }
    ASSERT_NE(owner, nullptr);
    owner->set_beam_mode(BeamMode::NONE);
    auto *bracket = Factory::create_chord_bracket(owner);
    owner->add(bracket);
    ASSERT_TRUE(document.page_count());
    LayoutContext ctx(&master);
    ChordLayout::fill_shape(owner, owner->mutldata());
    TLayout::layout_item(bracket, ctx);
    const auto expected = bracket->pos();
    ASSERT_LT(expected.x(), 0.0);
    for (int i = 0; i < 2; ++i) {
        owner->mutldata()->set_shape(Shape());
        TLayout::layout_item(bracket, ctx);
        EXPECT_EQ(bracket->pos(), expected);
    }
}

TEST(LayoutBounds, RestCollisionAndAlignmentRefreshChangedLedgerGlyphsImmediately) {
    using namespace mu::engraving;
    using namespace rendering::score;
    for (const auto type : {DurationType::V_HALF, DurationType::V_WHOLE}) {
        const bool half = type == DurationType::V_HALF;
        const auto xml = std::format(R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>{}</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>{}</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord></voice>
<voice><Rest><durationType>{}</durationType></Rest><Rest><durationType>{}</durationType></Rest></voice>
</Measure></Staff></Score></museScore>)",
                                     half ? 4 : 8, half ? "whole" : "breve", half ? "half" : "whole", half ? "half" : "whole");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        ASSERT_TRUE(document.page_count());
        auto &master = master_of(document);
        auto *measure = master.first_measure();
        auto *segment = measure->first(SegmentType::CHORD_REST);
        auto *chord = to_chord(segment->element(0));
        auto *first = to_rest(segment->element(1));
        auto *second = to_rest(segment->next(SegmentType::CHORD_REST)->element(1));
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        ASSERT_TRUE(measure->has_voices(0));
        LayoutContext ctx(&master);
        const double spatium = first->spatium();
        const auto ordinary = first->get_symbol(type, 4, 5);
        const auto ledger = first->get_symbol(type, 8, 5);
        ASSERT_NE(ordinary, ledger);
        const auto prepare = [&](Rest *rest, int line) {
            auto *data = rest->mutldata();
            data->set_pos(0.0, line * spatium);
            data->sym = rest->get_symbol(type, line, 5);
            rest->vertical_clearance().reset();
            RestLayout::fill_shape(rest, data, ctx.conf());
        };
        prepare(first, 4);
        prepare(second, 8);
        RestLayout::align_rests(measure->system(), ctx);
        EXPECT_EQ(first->ldata()->sym.value(), ledger);
        EXPECT_EQ(first->ldata()->shape(LdAccess::PASS).bbox(), first->sym_bbox(ledger));

        prepare(first, 4);
        std::vector<Rest *> rests{first};
        std::vector<Chord *> chords{chord};
        RestLayout::resolve_rest_vs_chord(rests, chords, first->staff(), segment);
        EXPECT_EQ(first->ldata()->sym.value(), ledger);
        EXPECT_EQ(first->ldata()->shape(LdAccess::PASS).bbox(), first->sym_bbox(ledger));
    }
}

TEST(LayoutBounds, RetiredGraceSlashAndLedgerLinesLeaveNoCollisionBorrowers) {
    using namespace mu::engraving;
    using namespace rendering::score;
    auto xml = legacy_segment_fixture("");
    xml.replace(xml.find("<pitch>60</pitch>"), 17, "<pitch>72</pitch>");
    xml.insert(xml.find("<Chord>"), R"(<Chord><acciaccatura/><durationType>eighth</durationType>
<BeamMode>no</BeamMode><Note><pitch>84</pitch><tpc>14</tpc></Note></Chord>)");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    auto &master = master_of(document);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    auto *main = to_chord(segment->element(0));
    ASSERT_EQ(main->grace_notes_before().size(), 1u);
    auto *grace = main->grace_notes_before().front();
    ASSERT_NE(grace->stem_slash(), nullptr);
    ASSERT_FALSE(grace->ledger_lines().empty());
    const auto contains_type = [](const Shape &shape, ElementType type) {
        return std::ranges::any_of(shape.elements(),
                                   [type](const ShapeElement &element) { return element.item() && element.item()->type() == type; });
    };
    ASSERT_TRUE(contains_type(grace->shape(), ElementType::STEM_SLASH));
    ASSERT_TRUE(contains_type(grace->shape(), ElementType::LEDGER_LINE));
    ASSERT_TRUE(contains_type(segment->staff_shape(0), ElementType::STEM_SLASH));
    LayoutContext ctx(&master);
    grace->set_show_stem_slash(false);
    ChordLayout::layout_stem(grace, ctx);
    grace->resize_ledger_lines_to(0);
    // Read immediately: a later complete-layout pass must not hide stale borrows.
    EXPECT_FALSE(contains_type(grace->shape(), ElementType::STEM_SLASH));
    EXPECT_FALSE(contains_type(grace->shape(), ElementType::LEDGER_LINE));
    EXPECT_FALSE(contains_type(segment->staff_shape(0), ElementType::STEM_SLASH));
    EXPECT_FALSE(contains_type(segment->staff_shape(0), ElementType::LEDGER_LINE));
    EXPECT_TRUE(std::isfinite(HorizontalSpacing::min_horizontal_distance(grace->shape(), main->shape(), grace->spatium())));
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(main->eid()), "color", engrave::Color{20, 40, 60}}}}));
    auto rendered = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(rendered);
    EXPECT_EQ(grace->up_note()->pitch(), 84);
    EXPECT_FALSE(grace->ledger_lines().empty());
    EXPECT_EQ(grace->beam(), nullptr);
    EXPECT_EQ(grace->stem_slash(), nullptr);
    EXPECT_FALSE(contains_type(grace->shape(), ElementType::STEM_SLASH));
}

TEST(LayoutBounds, StemSuppressionRetiresWarmHookAndStemShapes) {
    using namespace mu::engraving;
    using namespace rendering::score;
    auto xml = legacy_segment_fixture("");
    const std::string half = "<durationType>half</durationType>";
    xml.replace(xml.find(half), half.size(), "<durationType>eighth</durationType><BeamMode>no</BeamMode>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    auto &master = master_of(document);
    auto *measure = master.first_measure();
    auto *segment = measure->first(SegmentType::CHORD_REST);
    auto *chord = to_chord(segment->element(0));
    ASSERT_NE(chord->stem(), nullptr);
    ASSERT_NE(chord->hook(), nullptr);
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(chord->eid()), "noStem", true}}}));
    LayoutContext ctx(&master);
    ModifyDom::create_stems(measure, ctx);
    EXPECT_EQ(chord->stem(), nullptr);
    EXPECT_EQ(chord->hook(), nullptr);
    for (const auto &shape : {chord->shape(), segment->staff_shape(0)}) {
        for (const auto &element : shape.elements()) {
            if (!element.item()) continue;
            EXPECT_FALSE(element.item()->is_stem());
            EXPECT_FALSE(element.item()->is_hook());
        }
    }
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(chord->stem(), nullptr);
    EXPECT_EQ(chord->hook(), nullptr);
    EXPECT_EQ(chord->up_note()->pitch(), 60);
}

TEST(LayoutBounds, WarmSlashedGraceBeamRegroupingRebuildsCollisionShapes) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    std::string graces;
    for (const auto [pitch, tpc] : {std::pair{72, 14}, std::pair{74, 16}, std::pair{76, 18}})
        graces += std::format(
            "<Chord><acciaccatura/><durationType>eighth</durationType><Note><pitch>{}</pitch><tpc>{}</tpc></Note></Chord>", pitch, tpc);
    xml.insert(xml.find("<Chord>"), graces);
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *main = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    const auto notes = main->grace_notes_before();
    ASSERT_EQ(notes.size(), 3u);
    const auto check = [&](int slashes) {
        EXPECT_EQ(std::ranges::count_if(notes, [](const Chord *chord) { return chord->stem_slash() != nullptr; }), slashes);
        for (const auto *chord : notes) {
            EXPECT_TRUE(std::isfinite(
                rendering::score::HorizontalSpacing::min_horizontal_distance(chord->shape(), main->shape(), chord->spatium())));
        }
    };
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    check(1);
    for (const auto &mode : {"no", "auto", "no", "auto"}) {
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(notes[0]->eid()), "BeamMode", engrave::Enum{mode}}}}));
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        check(std::string_view(mode) == "no" ? 2 : 1);
        EXPECT_EQ(notes[0]->up_note()->pitch(), 72);
        EXPECT_EQ(notes[1]->up_note()->pitch(), 74);
        EXPECT_EQ(notes[2]->up_note()->pitch(), 76);
    }
}

TEST(LayoutBounds, DottedSlurSurvivesRepeatedOutputRetirement) {
    using namespace mu::engraving;
    const auto root = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT);
#ifdef _WIN32
    const std::filesystem::path platform = "windows";
#else
    const std::filesystem::path platform = "linux";
#endif
    const auto golden_dir = root / "tests/data/golden" / platform / "slurs.mscz";
    const std::array<std::string, 2> golden_pages = {
        normalize_lf(read_text(golden_dir / "page-0.svg")),
        normalize_lf(read_text(golden_dir / "page-1.svg")),
    };
    ASSERT_NE(golden_pages[0].find("<svg"), std::string::npos);
    ASSERT_NE(golden_pages[1].find("<svg"), std::string::npos);

    auto golden_opened = g_engine->open(root / "tests/data/inputs/golden/slurs.mscz");
    ASSERT_TRUE(golden_opened);
    auto &golden_doc = **golden_opened;
    auto &golden_master = master_of(golden_doc);
    for (int run = 0; run < 3; ++run) {
        engrave::DocumentAccess::impl(golden_doc).discard_layout_output(golden_master);
        const auto rendered = golden_doc.render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(rendered);
        ASSERT_EQ(rendered->pages.size(), golden_pages.size());
        for (std::size_t page = 0; page < golden_pages.size(); ++page) {
            EXPECT_EQ(normalize_lf(as_text(rendered->pages[page])), golden_pages[page]);
        }
    }

    const std::string slur_xml = R"(<Slur><eid>F_F</eid><track>0</track>
<startElement>B_B</startElement><endElement>C_C</endElement></Slur>)";
    auto xml = legacy_segment_fixture(slur_xml);
    const std::string half = "<durationType>half</durationType>";
    xml.insert(xml.find(half) + half.size(), "<dots>1</dots>");
    xml.replace(xml.find("<sigN>4</sigN>"), 14, "<sigN>5</sigN>");
    auto unslurred_xml = xml;
    unslurred_xml.erase(unslurred_xml.find(slur_xml), slur_xml.size());
    auto unslurred = g_engine->open(as_bytes(unslurred_xml));
    ASSERT_TRUE(unslurred);
    const auto unslurred_svg = (*unslurred)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(unslurred_svg);

    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_EQ(chord->up_note()->dots().size(), 1u);
    ASSERT_EQ(master.spanner_list().size(), 1u);
    auto *slur = master.spanner_list().front();
    for (int run = 0; run < 3; ++run) {
        engrave::DocumentAccess::impl(document).discard_layout_output(master);
        const auto rendered = document.render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(rendered);
        ASSERT_EQ(rendered->pages.size(), 1u);
        const std::string_view svg_text = as_text(rendered->pages.front());
        EXPECT_NE(svg_text.find("<svg"), std::string_view::npos);
        EXPECT_NE(svg_text.find("</svg>"), std::string_view::npos);
        EXPECT_NE(rendered->pages, unslurred_svg->pages);
        ASSERT_EQ(slur->spanner_segments().size(), 1u);
        const auto &slur_bbox = slur->spanner_segments().front()->ldata()->bbox();
        EXPECT_FALSE(slur_bbox.is_empty());
        EXPECT_GT(slur_bbox.width(), chord->spatium());
        EXPECT_GT(slur_bbox.height(), 0.0);
        ASSERT_EQ(chord->up_note()->dots().size(), 1u);
        const auto *dot = chord->up_note()->dots().front();
        EXPECT_FALSE(dot->ldata()->bbox().is_empty());
        EXPECT_GT(dot->ldata()->bbox().width(), 0.0);
        EXPECT_GT(dot->pos().x(), 0.0);
        const auto saved_mscx = document.export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved_mscx);
        const std::string_view mscx_text = as_text(*saved_mscx);
        EXPECT_NE(mscx_text.find("<durationType>half</durationType>"), std::string_view::npos);
        EXPECT_NE(mscx_text.find("<dots>1</dots>"), std::string_view::npos);
        EXPECT_NE(mscx_text.find("<Slur>"), std::string_view::npos);
        EXPECT_NE(mscx_text.find("<startElement>B_B</startElement>"), std::string_view::npos);
        EXPECT_NE(mscx_text.find("<endElement>C_C</endElement>"), std::string_view::npos);
        EXPECT_EQ(chord->up_note()->pitch(), 60);
    }
}

TEST(LayoutBounds, AuthoredNoteDeletionRetiresCachedCollisionPointersBeforeSpannerErase) {
    using namespace mu::engraving;
    auto expected_xml = legacy_segment_fixture("");
    expected_xml.replace(expected_xml.find("<Note>"), 6, "<Note><eid>D_D</eid>");
    auto expected_opened = g_engine->open(as_bytes(expected_xml));
    ASSERT_TRUE(expected_opened);
    const auto expected_svg = (*expected_opened)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(expected_svg);
    ASSERT_EQ(expected_svg->pages.size(), 1u);

    auto xml = legacy_segment_fixture(
        R"(<HairPin><eid>F_F</eid><track>0</track><subtype>0</subtype><startTick>0/1</startTick><ticks>1/1</ticks><track2>0</track2></HairPin>)");
    xml.replace(xml.find("<Note>"), 6, "<Note><eid>D_D</eid>");
    xml.insert(xml.find("</Note>") + 7,
               "<Note><eid>E_E</eid><Accidental><subtype>accidentalSharp</subtype></Accidental><pitch>61</pitch><tpc>21</tpc></Note>");

    for (const bool same_batch : {false, true}) {
        SCOPED_TRACE(same_batch ? "same_batch" : "sequential_edits");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));

        auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        ASSERT_EQ(chord->notes().size(), 2u);
        ASSERT_EQ(master.spanner_list().size(), 1u);
        auto *extra_note = chord->find_note(61);
        ASSERT_NE(extra_note, nullptr);
        const auto note_ref = engrave::from_engine(extra_note->eid());
        const auto hairpin_ref = engrave::from_engine(master.spanner_list().front()->eid());

        if (same_batch) {
            ASSERT_TRUE(document.edit({{engrave::Remove{note_ref}, engrave::Remove{hairpin_ref}}}));
        } else {
            ASSERT_TRUE(document.edit({{engrave::Remove{note_ref}}}));
            ASSERT_EQ(chord->notes().size(), 1u);
            EXPECT_EQ(chord->up_note()->pitch(), 60);
            ASSERT_TRUE(document.edit({{engrave::Remove{hairpin_ref}}}));
        }

        ASSERT_EQ(chord->notes().size(), 1u);
        EXPECT_EQ(chord->up_note()->pitch(), 60);
        EXPECT_TRUE(master.spanner_list().empty());

        const auto rendered = document.render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(rendered);
        EXPECT_EQ(rendered->pages, expected_svg->pages);
    }
}

TEST(LayoutBounds, ClonedMeasureDoesNotBorrowDeletedSourceCollisionShapes) {
    using namespace mu::engraving;
    auto expected_xml = legacy_segment_fixture("");
    expected_xml.replace(expected_xml.find("<Note>"), 6, "<Note><eid>D_D</eid>");
    auto expected_opened = g_engine->open(as_bytes(expected_xml));
    ASSERT_TRUE(expected_opened);
    const auto expected_svg = (*expected_opened)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(expected_svg);

    auto xml = legacy_segment_fixture("");
    xml.replace(xml.find("<Note>"), 6, "<Note><eid>D_D</eid>");
    xml.insert(xml.find("</Note>") + 7,
               "<Note><eid>E_E</eid><Accidental><subtype>accidentalSharp</subtype></Accidental><pitch>61</pitch><tpc>21</tpc></Note>");

    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));

    auto *source_measure = master.first_measure();
    auto *source_seg = source_measure->first(SegmentType::CHORD_REST);
    ASSERT_NE(source_seg, nullptr);
    ASSERT_FALSE(source_seg->staff_shape(0).empty());

    std::unique_ptr<Measure> cloned(source_measure->clone());
    auto *cloned_seg = cloned->first(SegmentType::CHORD_REST);
    ASSERT_NE(cloned_seg, nullptr);
    ASSERT_EQ(cloned_seg->shapes().size(), master.nstaves());
    EXPECT_TRUE(cloned_seg->staff_shape(0).empty());

    auto *extra_note = to_chord(source_seg->element(0))->find_note(61);
    ASSERT_NE(extra_note, nullptr);
    ASSERT_TRUE(document.edit({{engrave::Remove{engrave::from_engine(extra_note->eid())}}}));
    EXPECT_TRUE(cloned_seg->staff_shape(0).empty());

    const auto rendered = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(rendered);
    EXPECT_EQ(rendered->pages, expected_svg->pages);
}

TEST(LayoutBounds, MultiPageRetirementPreservesUnrelatedPageBspAndBinarySearches) {
    using namespace mu::engraving;
    const auto input_path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/slurs.mscz";
    auto opened = g_engine->open(input_path);
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    ASSERT_EQ(master.pages().size(), 2u);

    auto *page0 = master.pages()[0];
    auto *page1 = master.pages()[1];
    page0->rebuild_bsp_tree();
    page1->rebuild_bsp_tree();
    const auto page0_items = page0->items(page0->ldata()->bbox());
    const auto page1_items = page1->items(page1->ldata()->bbox());
    ASSERT_FALSE(page0_items.empty());
    ASSERT_FALSE(page1_items.empty());

    // Collect >= 17 items strictly on page 1 so BspTree::contains_any exercises binary_search.
    std::vector<const EngravingItem *> page1_only;
    ankerl::unordered_dense::set<const EngravingItem *> page0_set(page0_items.begin(), page0_items.end());
    for (const auto *item : page1_items) {
        if (!page0_set.contains(item)) page1_only.push_back(item);
        if (page1_only.size() >= 18) break;
    }
    ASSERT_GT(page1_only.size(), 16u);
    std::sort(page1_only.begin(), page1_only.end(), std::less<>{});

    page0->invalidate_bsp_tree_if_referenced(page1_only);
    EXPECT_EQ(page0->items(page0->ldata()->bbox()), page0_items);

    page1->invalidate_bsp_tree_if_referenced(page1_only);
    EXPECT_TRUE(page1->items(page1->ldata()->bbox()).empty());
}
