#include "engraving/dom/dummyparent.h"
#include "engraving/dom/guitarbend.h"
#include "read_test_fixture.h"

using namespace read_test;

TEST(Read, BendGhostStateIsNormalizedBeforeFirstRender) {
    ItemLayoutSpy spy;
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/bend.gp");
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    const auto before = music::dump_json(music::semantic_dump(&master), 1);
    int found = 0;
    for (auto *object : engrave::document_objects(master)) {
        if (!object->is_note()) continue;
        auto *note = mu::engraving::to_note(object);
        if (note->chord()->is_grace() && note->bend_for() && note->bend_for()->bend_type() == mu::engraving::GuitarBendType::PRE_BEND &&
            ((note->tick().ticks() == 3840 && note->pitch() == 57) || (note->tick().ticks() == 4800 && note->pitch() == 53))) {
            EXPECT_TRUE(note->ghost());
            ++found;
        }
    }
    EXPECT_EQ(found, 2);
    EXPECT_EQ(ItemLayoutSpy::count, 0u);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(music::dump_json(music::semantic_dump(&master), 1), before);
}

TEST(Read, ParenthesizedPrebendsKeepPerStaffGhostState) {
    auto opened = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/tab_grace_prebend.mscx");
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    std::vector<std::pair<mu::engraving::Note *, bool>> ghosts;
    for (auto *object : engrave::document_objects(master)) {
        if (object->is_note()) ghosts.emplace_back(mu::engraving::to_note(object), mu::engraving::to_note(object)->ghost());
    }
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    for (const auto &[note, ghost] : ghosts) EXPECT_EQ(note->ghost(), ghost);
}

TEST(Read, LiveVersionAndRevisionMacrosKeepSourceProvenanceAfterEdit) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    source.insert(source.find("<Score>"), "<programRevision>abcdef1</programRevision>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    master.style().set(Sid::ODD_HEADER_C, muse::String(u"$v / $r"));
    master.set_layout_all();
    const auto version = master.mscore_version();
    const auto revision = master.mscore_revision();
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    auto *header = master.pages().front()->header_text(1);
    ASSERT_NE(header, nullptr);
    const auto before = header->plain_text();
    EXPECT_TRUE(before.contains(version));
    EXPECT_TRUE(before.contains(u"abcdef1"));
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    ASSERT_TRUE(document.edit({{engrave::SetPitch{engrave::from_engine(note->eid()), 64}}}));
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(master.pages().front()->header_text(1)->plain_text(), before);
    EXPECT_EQ(master.mscore_version(), version);
    EXPECT_EQ(master.mscore_revision(), revision);
}

TEST(Read, SourceTextResetPreservesCharactersWithoutLayout) {
    using namespace mu::engraving;
    for (const char *property : {"fontStyle", "FONT_SIZE", "family"}) {
        for (bool together : {false, true}) {
            SCOPED_TRACE(std::format("{} batch={}", property, together));
            ItemLayoutSpy spy;
            auto opened = g_engine->open(as_bytes(formatted_text_fixture()), {.format = engrave::Format::MSCX});
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *text = staff_text_of(master);
            ASSERT_NE(text, nullptr);
            const auto id = engrave::from_engine(text->eid());
            const engrave::Op set = engrave::SetProperty{id, "text", std::string("<b>hello</b>\nworld <sym>accidentalSharp</sym>")};
            const engrave::Op reset = engrave::ResetProperty{id, property};
            if (together)
                ASSERT_TRUE(document.edit({{set, reset}}));
            else {
                ASSERT_TRUE(document.edit({{set}}));
                ASSERT_TRUE(document.edit({{reset}}));
            }
            EXPECT_TRUE(text->xml_text().contains(u"hello"));
            EXPECT_TRUE(text->xml_text().contains(u"world"));
            EXPECT_TRUE(text->xml_text().contains(u"<sym>accidentalSharp</sym>"));
            EXPECT_EQ(text->plain_text(), u"hello\nworld \uE262");
            EXPECT_EQ(text->property_flags(engrave::api::pid_of(property)), PropertyFlags::STYLED);
            EXPECT_EQ(ItemLayoutSpy::count, 0u);
            EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        }
    }
}

TEST(Read, SourceTextFormattingReadsAgreeBeforeAndAfterRender) {
    using namespace mu::engraving;
    auto source = formatted_text_fixture();
    const auto begin = source.find("<text><b>");
    const auto end = source.find("</text>", begin);
    source.replace(begin, end - begin, "<text><b><font size=\"20\"/><font face=\"Leland Text\"/><sup>hello</sup></b>");
    ItemLayoutSpy spy;
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *text = staff_text_of(master);
    ASSERT_NE(text, nullptr);
    const auto markup = text->xml_text();
    const auto flags = text->property_flags(Pid::FONT_FACE);
    const auto id = engrave::from_engine(text->eid());
    const std::vector<std::string> properties{"fontStyle", "FONT_SIZE", "family", "TEXT_SCRIPT_ALIGN"};
    const auto before = document.get(id, properties);
    ASSERT_TRUE(before) << before.error().message;
    EXPECT_EQ(text->get_property(Pid::FONT_STYLE).to_int(), static_cast<int>(FontStyle::BOLD));
    EXPECT_EQ(text->get_property(Pid::FONT_SIZE).to_double(), 20.0);
    EXPECT_EQ(text->get_property(Pid::FONT_FACE).value<muse::String>(), u"Leland Text");
    EXPECT_EQ(text->get_property(Pid::TEXT_SCRIPT_ALIGN).value<VerticalAlignment>(), VerticalAlignment::ALIGN_SUPER_SCRIPT);
    EXPECT_EQ(ItemLayoutSpy::count, 0u);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    const auto after = document.get(id, properties);
    ASSERT_TRUE(after);
    ASSERT_EQ(before->size(), after->size());
    for (std::size_t i = 0; i < before->size(); ++i) {
        EXPECT_EQ((*before)[i].value, (*after)[i].value);
        EXPECT_EQ((*before)[i].styled, (*after)[i].styled);
    }
    EXPECT_EQ(text->xml_text(), markup);
    EXPECT_EQ(text->property_flags(Pid::FONT_FACE), flags);
}

TEST(Read, UnchangedSourceFormattingPreservesMarkupAndUnlaidConstruction) {
    using namespace mu::engraving;
    auto source = formatted_text_fixture();
    const auto begin = source.find("<text><b>");
    const auto end = source.find("</text>", begin);
    source.replace(begin, end - begin, "<text><sym>metNoteQuarterUp</sym> = 120");
    ItemLayoutSpy spy;
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *text = staff_text_of(master);
    ASSERT_NE(text, nullptr);
    const auto markup = text->xml_text();
    const auto id = engrave::from_engine(text->eid());
    ASSERT_TRUE(document.edit(
        {{engrave::SetProperty{id, "FONT_SIZE", text->size()}, engrave::SetProperty{id, "family", text->family().to_std_string()},
          engrave::SetProperty{id, "fontStyle", std::int64_t(text->font_style())}}}));
    EXPECT_EQ(text->xml_text(), markup);
    std::unique_ptr<Dynamic> unlaid(Factory::create_dynamic(master.dummy()));
    EXPECT_FALSE(unlaid->has_layout_data());
    EXPECT_EQ(ItemLayoutSpy::count, 0u);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, WholeTextFormattingMatchesEagerSourceAndRenderWithoutItemLayout) {
    using namespace mu::engraving;
    std::vector<std::vector<std::byte>> pages;
    muse::String markup;
    for (bool eager : {true, false}) {
        auto source = formatted_text_fixture();
        source.insert(source.find("world") + 5, " <sym>noteheadDoubleWholeAlt</sym><sym>accidentalSharp</sym>");
        auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *text = staff_text_of(master);
        ASSERT_NE(text, nullptr);
        const auto id = engrave::from_engine(text->eid());
        if (eager) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        const auto count = master.layout_state.whole_count + master.layout_state.range_count;
        {
            ItemLayoutSpy spy;
            ASSERT_TRUE(
                document.edit({{engrave::SetProperty{id, "FONT_SIZE", 14.0}, engrave::SetProperty{id, "family", std::string("Leland Text")},
                                engrave::SetProperty{id, "fontStyle", std::int64_t(0)}}}));
            EXPECT_EQ(text->get_property(Pid::FONT_SIZE).to_double(), 14.0);
            EXPECT_EQ(text->get_property(Pid::FONT_FACE).value<muse::String>(), u"Leland Text");
            EXPECT_EQ(text->get_property(Pid::FONT_STYLE).to_int(), 0);
            EXPECT_FALSE(text->xml_text().contains(u"<b>"));
            EXPECT_TRUE(text->plain_text().starts_with(u"hello\nworld "));
            EXPECT_TRUE(text->xml_text().contains(u"<sym>noteheadDoubleWholeAlt</sym><sym>accidentalSharp</sym>"));
            EXPECT_EQ(ItemLayoutSpy::count, 0u);
            EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
        }
        auto rendered = document.render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(rendered);
        if (eager) {
            pages = rendered->pages;
            markup = text->xml_text();
        } else {
            EXPECT_EQ(text->xml_text(), markup);
            EXPECT_EQ(rendered->pages, pages);
        }
    }
}

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

TEST(Read, FingeringSourceReadsDeferLayoutUntilGeometryAddresses) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    source.insert(source.find("</Measure>"), "<voice><Chord><durationType>whole</durationType><Note><pitch>48</pitch><tpc>14</tpc>"
                                             "<Fingering><text>1</text></Fingering></Note></Chord></voice>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto fingers = document.query({.types = {"Fingering"}, .props = {"placement"}});
    ASSERT_TRUE(fingers);
    ASSERT_EQ(fingers->elements.size(), 1u);
    const auto id = fingers->elements.front().id;
    for (bool warm : {false, true}) {
        SCOPED_TRACE(warm);
        const auto layouts = master.layout_state.whole_count + master.layout_state.range_count;
        const auto placement = document.get(id, {"placement"});
        ASSERT_TRUE(placement);
        EXPECT_EQ(std::get<engrave::Enum>(placement->front().value).token, "above");
        EXPECT_TRUE(placement->front().is_default);
        const auto query = document.query({.types = {"Fingering"}, .props = {"placement"}});
        ASSERT_TRUE(query);
        EXPECT_EQ(std::get<engrave::Enum>(query->elements.front().props.front().value).token, "above");
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, layouts);
        if (!warm) {
            EXPECT_FALSE(document.outline()->pages.has_value());
            ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        }
    }
    const auto resolved = engrave::resolve(master, id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(to_fingering(resolved->object)->placement(), PlacementV::BELOW);
    const auto saved = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    const std::string saved_xml(reinterpret_cast<const char *>(saved->data()), saved->size());
    const auto begin = saved_xml.find("<Fingering>");
    ASSERT_NE(begin, std::string::npos);
    EXPECT_EQ(saved_xml.substr(begin, saved_xml.find("</Fingering>", begin) - begin).find("<placement>"), std::string::npos);
    master.invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    ASSERT_TRUE(document.get(engrave::Address{engrave::PageAddress{}}, {"visible"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 2u);
    master.invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    ASSERT_TRUE(document.get(engrave::Address{engrave::SystemAddress{}}, {"visible"}));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 3u);
}

TEST(Read, SourceSemanticDumpIsUnchangedByRendering) {
    const auto root = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT);
    for (const auto &path :
         {root / "src/importexport/mnx/tests/data/project_examples/bowDirection.mnx",
          root / "tests/data/inputs/golden/hairpins_and_repeats.mscx", root / "tests/data/inputs/golden/tab_grace_prebend.mscx"}) {
        SCOPED_TRACE(path.string());
        auto opened = g_engine->open(path);
        ASSERT_TRUE(opened) << opened.error().message;
        auto &document = **opened;
        const auto cold = music::semantic_dump(&master_of(document));
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        EXPECT_TRUE(music::json_equal(cold, music::semantic_dump(&master_of(document))));
    }
}

TEST(Read, ExplicitFingeringPlacementSurvivesColdAndWarmSave) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    source.insert(source.find("</Measure>"), "<voice><Chord><durationType>whole</durationType><Note><pitch>48</pitch><tpc>14</tpc>"
                                             "<Fingering><placement>above</placement><text>1</text></Fingering></Note></Chord></voice>");
    auto opened = g_engine->open(as_bytes(source));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto cold = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(cold);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    const auto warm = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(warm);
    EXPECT_EQ(*cold, *warm);
    const std::string xml(reinterpret_cast<const char *>(cold->data()), cold->size());
    EXPECT_NE(xml.find("<placement>above</placement>"), std::string::npos);
    auto reopened = g_engine->open(*cold, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
    const auto query = (*reopened)->query({.types = {"Fingering"}, .props = {"placement"}});
    ASSERT_TRUE(query);
    ASSERT_EQ(query->elements.size(), 1u);
    EXPECT_EQ(std::get<engrave::Enum>(query->elements.front().props.front().value).token, "above");
    const auto resolved = engrave::resolve(master_of(**reopened), query->elements.front().id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(to_fingering(resolved->object)->property_flags(Pid::PLACEMENT), PropertyFlags::UNSTYLED);
    EXPECT_EQ(to_fingering(resolved->object)->placement(), PlacementV::ABOVE);
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
    master.end_cmd(false);
    EXPECT_EQ(ItemLayoutSpy::count, 0u);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, count);
    EXPECT_TRUE(master.layout_state.dirty);
}

TEST(Read, DeadNoteHeadReadKeepsTheAuthoredGroup) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    source.insert(source.find("</Note>"), "<dead>1</dead>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const auto id = engrave::from_engine(note->eid());
    for (bool warm : {false, true}) {
        if (warm) master.ensure_layout();
        auto value = document.get(id, {"head"});
        ASSERT_TRUE(value);
        ASSERT_EQ(value->size(), 1u);
        EXPECT_EQ(std::get<engrave::Enum>(value->front().value).token, "normal");
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, warm ? 1u : 0u);
    }
}

TEST(Read, BendRemovalPreservesAuthoredNoteAppearance) {
    using namespace mu::engraving;
    auto source = read_text(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/tab_grace_prebend.mscx");
    size_t at = 0;
    while ((at = source.find("<Note>", at)) != std::string::npos) {
        const auto end = source.find("</Note>", at);
        auto note = source.substr(at, end - at);
        const auto visible = note.find("<visible>1</visible>");
        if (visible != std::string::npos)
            note.replace(visible, 20, "<visible>0</visible>");
        else
            note.insert(6, "<visible>0</visible>");
        note.insert(6, "<ghost>1</ghost>");
        source.replace(at, end - at, note);
        at += note.size();
    }
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    GuitarBend *bend = nullptr;
    for (auto *object : engrave::document_objects(master))
        if (object->is_guitar_bend()) {
            bend = to_guitar_bend(object);
            break;
        }
    ASSERT_NE(bend, nullptr);
    auto *end_note = bend->end_note();
    ASSERT_NE(end_note, nullptr);
    ASSERT_TRUE((*opened)->edit({{engrave::Remove{engrave::from_engine(bend->eid())}}}));
    EXPECT_TRUE(end_note->ghost());
    EXPECT_FALSE(end_note->visible());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, PlacedArticulationUsesTheBelowGlyphBounds) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    std::unique_ptr<Articulation> artic(Factory::create_articulation(chord));
    artic->set_sym_id(SymId::ARTIC_MARCATO_ABOVE);
    artic->set_up(false);
    rendering::score::TLayout::fill_articulation_shape(artic.get(), artic->mutldata());
    EXPECT_EQ(artic->sym_id(), SymId::ARTIC_MARCATO_ABOVE);
    EXPECT_EQ(artic->ldata()->bbox(), artic->sym_bbox(SymId::ARTIC_MARCATO_BELOW));
}

TEST(Read, DirtyBeamDirectionReadFallsBackToAutoDefault) {
    using namespace mu::engraving;
    auto source = read_text(write470_fixture());
    const auto begin = source.find("<Chord>");
    const auto end = source.find("</Chord>", begin) + 8;
    source.replace(begin, end - begin,
                   "<Chord><durationType>eighth</durationType><Note><pitch>84</pitch><tpc>14</tpc></Note></Chord>"
                   "<Chord><durationType>eighth</durationType><Note><pitch>86</pitch><tpc>16</tpc></Note></Chord>"
                   "<Rest><durationType>half</durationType><dots>1</dots></Rest>");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    master.ensure_layout();
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *beam = chord->beam();
    ASSERT_NE(beam, nullptr);
    ASSERT_FALSE(beam->up());
    master.invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    EXPECT_TRUE(beam->up());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 1u);
}

TEST(Read, DerivedClefAndTimeSigFlagsRejectPropertyEdits) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    for (const auto [type, pid] : {std::pair{"Clef", Pid::SMALL}, std::pair{"TimeSig", Pid::SYSTEM_FLAG}}) {
        const auto found = (*opened)->query({.types = {type}, .limit = 1});
        ASSERT_TRUE(found);
        ASSERT_FALSE(found->elements.empty());
        const auto id = found->elements.front().id;
        const auto resolved = engrave::resolve(master, id);
        ASSERT_TRUE(resolved);
        auto *item = to_engraving_item(resolved->object);
        EXPECT_EQ(engrave::api::settable(type, pid), false);
        EXPECT_FALSE(item->set_property(pid, true));
        auto edit = (*opened)->edit({{engrave::SetProperty{id, std::string(engrave::api::name_of(pid)), true}}});
        ASSERT_FALSE(edit);
        EXPECT_EQ(edit.error().code, engrave::ErrorCode::INVALID_ARGUMENT);
    }
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
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
    normalize_score(master);
    EXPECT_FALSE(master.normalize_state.dirty);
    EXPECT_FALSE(note->has_layout_data());
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

TEST(Read, NormalizeResolvesInstrumentDependentGlissandoStyleWithoutLayout) {
    using namespace mu::engraving;
    for (const bool styled : {false, true}) {
        auto opened = g_engine->open(write470_fixture().parent_path() / "ties_mismatched.mscx");
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *first = master.first_measure()->first(SegmentType::CHORD_REST);
        auto *start = to_chord(first->element(0))->notes().front();
        auto *end = to_chord(first->next(SegmentType::CHORD_REST)->element(0))->notes().front();
        std::unique_ptr<Glissando> line(Factory::create_glissando(start));
        line->set_note_span(start, end);
        line->set_glissando_style(GlissandoStyle::PORTAMENTO);
        line->set_property_flags(Pid::GLISS_STYLE, styled ? PropertyFlags::STYLED : PropertyFlags::UNSTYLED);
        start->add(line.get());
        auto *instrument = master.parts().front()->instrument(Fraction(0, 1));
        instrument->set_id(u"piano");
        master.set_layout_all();
        normalize_score(master, {.mode = NormalizeMode::REPLAY});
        EXPECT_EQ(line->is_harp_gliss(), false);
        instrument->set_id(u"harp");
        master.set_layout_all();
        normalize_score(master, {.mode = NormalizeMode::REPLAY});
        EXPECT_EQ(line->is_harp_gliss(), true);
        EXPECT_EQ(line->glissando_style(),
                  styled ? line->property_default(Pid::GLISS_STYLE).value<GlissandoStyle>() : GlissandoStyle::DIATONIC);
        EXPECT_EQ(line->is_styled(Pid::GLISS_STYLE), styled);
        EXPECT_FALSE(line->has_layout_data());
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        const auto style = line->glissando_style();
        EXPECT_TRUE(document.page_count());
        EXPECT_EQ(line->glissando_style(), style);
        start->remove(line.get());
        end->chord()->update_ends_note_anchored_line();
    }
}

TEST(Read, GetAllFreshSourcePropertiesSkipsInvalidEnumDefaultTokens) {
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    const auto rows = document.query({});
    ASSERT_TRUE(rows);
    for (const auto &row : rows->elements) {
        SCOPED_TRACE(row.type);
        const auto props = document.get(row.id);
        ASSERT_TRUE(props);
        if (row.type == "Clef") {
            for (const auto &prop : *props) {
                if (prop.name == "concertClefType" || prop.name == "transposingClefType") EXPECT_FALSE(prop.is_default);
            }
        }
    }
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, LedgerLinesBelongToLayoutAndPreserveNormalizedSource) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *note = chord->up_note();
    ASSERT_TRUE(document.edit({{engrave::SetPitch{engrave::from_engine(note->eid()), 96}}}));
    EXPECT_FALSE(chord->has_layout_data());
    EXPECT_TRUE(chord->ledger_lines().empty());
    EXPECT_FALSE(chord->has_layout_data());
    auto source_properties = [&] {
        std::vector<std::tuple<EID, Pid, PropertyValue, PropertyFlags>> values;
        for (const auto *object : engrave::document_objects(master)) {
            if (!engrave::takes_id(*object)) continue;
            values.emplace_back(object->eid(), Pid::END, PropertyValue{}, PropertyFlags::NOSTYLE);
            for (const auto pid : engrave::api::pids_of(object->type_name())) {
                values.emplace_back(object->eid(), pid, object->get_property(pid), object->property_flags(pid));
            }
        }
        return values;
    };
    auto source_structure = [&] {
        std::vector<std::tuple<const EngravingObject *, const EngravingObject *, EID>> structure;
        for (const auto *object : engrave::document_objects(master)) {
            if (!engrave::takes_id(*object)) continue;
            for (const auto *ancestor = object; ancestor; ancestor = ancestor->ownership_parent()) {
                structure.emplace_back(ancestor, ancestor->ownership_parent(), ancestor->eid());
                if (ancestor == &master) break;
            }
        }
        return structure;
    };
    const auto before = source_properties();
    const auto structure_before = source_structure();
    const auto revision = document.revision();
    ASSERT_TRUE(document.page_count());
    EXPECT_EQ(source_properties(), before);
    EXPECT_EQ(source_structure(), structure_before);
    EXPECT_EQ(document.revision(), revision);
    ASSERT_FALSE(chord->ledger_lines().empty());
    for (const auto *line : chord->ledger_lines()) EXPECT_TRUE(line->generated());
    EXPECT_EQ(chord->ledger_lines().data(), chord->ldata()->ledger_lines.data());
    chord->resize_ledger_lines_to(0);
    EXPECT_TRUE(chord->ledger_lines().empty());
    master.set_layout_all();
    ASSERT_TRUE(document.page_count());
    EXPECT_FALSE(chord->ledger_lines().empty());
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
        master.end_cmd(false);
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
    master.end_cmd(false);
}

TEST(Read, NormalizeCleansInvalidMelismaBeforeRepairingLyricTicks) {
    using namespace mu::engraving;
    for (const auto &[with_separator, ticks] :
         std::array{std::pair{false, Fraction(1, 4)}, std::pair{true, Fraction(1, 4)}, std::pair{false, Lyrics::temp_melisma_ticks},
                    std::pair{true, Lyrics::temp_melisma_ticks}}) {
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
            lyrics->set_ticks(ticks);
            first->add(lyrics);
            auto *following = Factory::create_lyrics(second);
            following->set_track(0);
            second->add(following);
            if (with_separator) {
                auto *separator = Factory::create_lyrics_line(lyrics);
                separator->set_track(0);
                separator->set_track2(0);
                separator->set_start_element(first);
                separator->set_end_element(second);
                separator->set_ticks(Fraction(1, 4));
                lyrics->set_separator(separator);
                master.add_unmanaged_spanner(separator);
            }
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
}

TEST(Read, MissingNativeDisplayIdentitiesAreFinalizedBeforeSaving) {
    auto source = mmr_source();
    const auto first_measure = source.find("<Measure>") + std::string("<Measure>").size();
    source.insert(first_measure, "<MeasureNumber><text>99</text></MeasureNumber>");
    auto opened = g_engine->open(as_bytes(source));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *number = master.first_measure()->mstaves().front()->measure_number();
    ASSERT_NE(number, nullptr);
    EXPECT_FALSE(number->generated());
    EXPECT_TRUE(number->eid().is_valid());
    ASSERT_TRUE((*opened)->checkpoint("number"));
    auto legacy = mmr_source();
    const auto after_first = legacy.find("</Measure>") + std::string("</Measure>").size();
    legacy.insert(after_first, "<Measure><multiMeasureRest>2</multiMeasureRest><voice>"
                               "<KeySig><concertKey>3</concertKey><actualKey>3</actualKey></KeySig>"
                               "<Rest><durationType>measure</durationType></Rest></voice></Measure>");
    auto reopened = g_engine->open(as_bytes(legacy));
    ASSERT_TRUE(reopened) << reopened.error().message;
    auto &restored = master_of(**reopened);
    EXPECT_EQ(restored.first_measure()->mm_rest(), nullptr);
    auto *key = restored.first_measure()->find_segment_r(mu::engraving::SegmentType::KEY_SIG, mu::engraving::Fraction(0, 1));
    ASSERT_NE(key, nullptr);
    ASSERT_NE(key->element(0), nullptr);
    EXPECT_EQ(mu::engraving::to_key_sig(key->element(0))->key(), mu::engraving::Key::A);
    EXPECT_EQ(restored.layout_state.whole_count + restored.layout_state.range_count, 0u);
    EXPECT_TRUE((*reopened)->checkpoint("mmr"));
}

TEST(Read, LegacyMmrLocksAndStaffTextRecoverAcrossEmbeddedExcerpt) {
    using namespace mu::engraving;
    const auto score = [](std::string_view first, std::string_view last, std::string_view display) {
        return std::format(R"(<Score><Division>480</Division><Style><createMultiMeasureRests>1</createMultiMeasureRests></Style>
<Part><Staff id="1"/><Instrument><trackName>Piano</trackName></Instrument></Part><Staff id="1">
<Measure><eid>{0}</eid><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig><Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><multiMeasureRest>2</multiMeasureRest><eid>{2}</eid><mmRestLast>{1}</mmRestLast>
<MeasureNumber><text>91</text></MeasureNumber><MMRestRange><text>bars</text></MMRestRange><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><eid>{1}</eid><voice><Rest><durationType>measure</durationType></Rest></voice></Measure></Staff>
<SystemLocks><systemLock><startMeasure>{2}</startMeasure><endMeasure>{1}</endMeasure></systemLock></SystemLocks></Score>)",
                           first, last, display);
    };
    auto input = score("A_A", "B_B", "C_C");
    input.insert(input.rfind("</Score>"), score("D_D", "E_E", "F_F"));
    auto opened = g_engine->open(as_bytes("<museScore version=\"5.00\">" + input + "</museScore>"));
    ASSERT_TRUE(opened) << opened.error().message;
    auto &master = master_of(**opened);
    ASSERT_EQ(master.excerpts().size(), 1u);
    for (auto *source : master.score_list()) {
        EXPECT_EQ(source->first_measure()->mm_rest(), nullptr);
        const auto locks = source->system_locks()->all_locks();
        ASSERT_EQ(locks.size(), 1u);
        EXPECT_EQ(locks.front()->start_mb(), source->first_measure());
        EXPECT_EQ(locks.front()->end_mb(), source->last_measure());
        auto *row = source->first_measure()->mstaves().front();
        ASSERT_NE(row->measure_number(), nullptr);
        ASSERT_NE(row->mm_range_text(), nullptr);
        EXPECT_EQ(row->measure_number()->xml_text(), u"91");
        EXPECT_EQ(row->mm_range_text()->xml_text(), u"[bars]");
    }
    const auto write = [&] {
        muse::io::Buffer buffer;
        buffer.open(muse::io::IODevice::WRITE_ONLY);
        rw::WriteInOutData inout(&master);
        inout.ctx.set_inline_style(true);
        inout.ctx.set_inline_excerpts(true);
        EXPECT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
        return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
    };
    const auto cold = write();
    EXPECT_TRUE((*opened)->checkpoint("legacy-source"));
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(write(), cold);
    for (auto *source : master.score_list()) {
        source->layout_data_store()->clear();
        EXPECT_EQ(source->system_locks()->all_locks().front()->start_mb(), source->first_measure());
    }
}

TEST(Read, BoundedNativeWritePreservesDisabledAutoplace) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.style().set(mu::engraving::Sid::AUTOPLACE_ENABLED, false);
    auto *chord = mu::engraving::to_chord(master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST)->element(0));
    chord->notes().front()->set_autoplace(false);
    auto complete = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(complete);
    const auto offset = std::string(reinterpret_cast<const char *>(complete->data()), complete->size()).find("<autoplace>0</autoplace>");
    ASSERT_NE(offset, std::string::npos);
    const auto limited = document->export_score({.format = engrave::Format::MSCX, .capture = {.max_bytes = offset + 5}});
    ASSERT_FALSE(limited);
    EXPECT_EQ(limited.error().code, ErrorCode::RESOURCE_LIMIT);
    EXPECT_FALSE(master.style().style_b(mu::engraving::Sid::AUTOPLACE_ENABLED));
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

TEST(Read, NativeMasterExportLeavesGeneratedExcerptsCold) {
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
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        EXPECT_EQ(part->layout_state.whole_count + part->layout_state.range_count, 0u);
        EXPECT_TRUE(master.pages().empty());
        EXPECT_TRUE(part->pages().empty());
        ASSERT_TRUE(document.export_score({.format = format}));
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        EXPECT_EQ(part->layout_state.whole_count + part->layout_state.range_count, 0u);
    }
}

TEST(Read, LegacyMmrMarkersRestoreTheCorrectSourceEndpoint) {
    using namespace mu::engraving;
    const auto fixture = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/mmr/legacy_source_ownership.mscx";
    for (const auto version : {"4.60", "5.00"}) {
        for (const auto &[type, style, label, at_end] :
             {std::tuple{"fine", "repeat_right", "fine", true}, std::tuple{"coda", "repeat_right", "coda", true},
              std::tuple{"user", "repeat_right", "custom-end", true}, std::tuple{"user", "repeat_left", "custom-start", false}}) {
            SCOPED_TRACE(std::string(version) + ":" + label);
            auto source = read_text(fixture);
            source.replace(source.find("5.00"), 4, version);
            const auto jump = source.find("<Jump>");
            const auto jump_end = source.find("</Jump>", jump) + std::string("</Jump>").size();
            source.replace(jump, jump_end - jump,
                           std::format("<Marker><style>{}</style><label>{}</label><markerType>{}</markerType><text>{}</text></Marker>",
                                       style, label, type, label));
            const bool fine = std::string_view(type) == "fine";
            source.insert(source.rfind("</Staff>"),
                          std::string("<Measure>") +
                              (fine ? "<Jump><jumpTo>start</jumpTo><playUntil>fine</playUntil><text>D.C. al Fine</text></Jump>" : "") +
                              "<voice><Rest><durationType>measure</durationType></Rest></voice></Measure>");
            auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *expected = at_end ? master.first_measure()->next_measure() : master.first_measure();
            const auto found = std::ranges::find_if(expected->el(), [](const auto *item) { return item->is_marker(); });
            ASSERT_NE(found, expected->el().end());
            EXPECT_EQ(to_marker(*found)->label().to_std_string(), label);
            EXPECT_EQ((*found)->tick(), expected->tick());
            if (fine) EXPECT_EQ(master.repeat_list(true).ticks(), 5 * whole_note_ticks);
            EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
            const auto saved = document.export_score({.format = engrave::Format::MSCX});
            ASSERT_TRUE(saved);
            auto reloaded = g_engine->open(*saved, {.format = engrave::Format::MSCX});
            ASSERT_TRUE(reloaded);
            auto &copy = master_of(**reloaded);
            auto *copy_measure = at_end ? copy.first_measure()->next_measure() : copy.first_measure();
            EXPECT_TRUE(std::ranges::any_of(copy_measure->el(), [](const auto *item) { return item->is_marker(); }));
            if (fine) EXPECT_EQ(copy.repeat_list(true).ticks(), 5 * whole_note_ticks);
        }
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

TEST(Read, NormalizeKeepsOffNoteAnchorsOutOfSourceAndDoesNotLayOut) {
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
    const auto objects = engrave::document_objects(master);
    normalize_score(master);
    EXPECT_EQ(engrave::document_objects(master), objects);
    EXPECT_EQ(master.first_measure()->find_segment(SegmentType::TIME_TICK, Fraction(1, 4)), nullptr);
    EXPECT_EQ(master.first_measure()->find_segment(SegmentType::TIME_TICK, Fraction(3, 4)), nullptr);
    EXPECT_FALSE(hairpin->has_layout_data());
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

TEST(Read, GraceSpacingIsIndependentOfPreviousLayoutContext) {
    using namespace mu::engraving;
    using namespace mu::engraving::rendering::score;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    Measure *measure = master.first_measure();
    Segment *segment = measure->first(SegmentType::CHORD_REST);
    add_test_grace(to_chord(segment->element(0)), NoteType::ACCIACCATURA);
    master.do_layout();
    const auto expected = segment->staff_shape(0).size();
    for (int run = 0; run < 2; ++run) {
        {
            LayoutContext context(&master);
            MeasureLayout::update_grace_notes(measure, context);
            EXPECT_EQ(segment->staff_shape(0).size(), expected);
        }
        EXPECT_EQ(grace_group_count(&master), 0);
    }
}

TEST(Read, SlurGraceGroupsDoNotSurviveRepeatedScoreLayout) {
    auto document = open_path(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/slurs.mscz");
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
        master.apply_segment_parent_change(clef_segment, next, Fraction(0, 1));
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

TEST(Read, RepeatRepairPublishesExplicitChanges) {
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
    manager->begin_transaction(muse::TranslatableString::untranslatable("remove repeat"));
    master.apply_property_change(second_measure, Pid::REPEAT_END, false);
    manager->prepare_commit();
    EXPECT_FALSE(slur->is_incoming());
    manager->end_transaction();
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
    ASSERT_TRUE(document.edit(
        {{engrave::SetProperty{engrave::Ref{engrave::from_engine(measure->eid())}, "timesigActual", engrave::Fraction{1, 2}}}}));
    EXPECT_EQ(following->tick().ticks(), 2880);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(2879).val, 2.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).tempo(2880).val, 1.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).tempo(5760).val, 1.0);
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
    EXPECT_EQ(layout_count(master), count);
    EXPECT_EQ(music::dump_json(music::semantic_dump(&master), 1), before);

    manager->begin_transaction(muse::TranslatableString::untranslatable("repeat edit"));
    master.apply_property_change(master.first_measure(), Pid::REPEAT_COUNT, 3);
    manager->end_transaction();
    EXPECT_EQ(master.repeat_list(true).ticks(), 5760);
    EXPECT_EQ(master.repeat_list(false).ticks(), 1920);

    TempoText *mark = nullptr;
    master.scan_elements([&mark](EngravingItem *item) {
        if (item->is_tempo_text()) mark = to_tempo_text(item);
    });
    ASSERT_NE(mark, nullptr);
    const auto midi_before = document.export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(midi_before);
    manager->begin_transaction(muse::TranslatableString::untranslatable("tempo edit"));
    master.apply_property_change(mark, Pid::TEMPO, BeatsPerSecond(1.0));
    manager->end_transaction();
    EXPECT_DOUBLE_EQ(master.tempo_timeline(true).utick2utime(1920), 4.0);
    EXPECT_DOUBLE_EQ(master.tempo_timeline(false).utick2utime(1920), 4.0);
    const auto midi_after = document.export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(midi_after);
    EXPECT_NE(*midi_after, *midi_before);
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
    master.apply_automation_points(tempo_key, edits);
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
        master.apply_property_change(target, property, replacement);
        manager->end_transaction();
        const auto after = signature();
        EXPECT_NE(after, before) << type;
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

// Replacement keeps a full layout pending. Counter deltas are meaningful within
// each Score lifetime, not across project replacement.

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
    auto document = open_path(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/layout_elements.mscx");
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
TEST(Read, TupletNumberStyleLivesOnSourceBeforeLayoutAndAfterDisplayRemoval) {
    constexpr std::string_view fixture = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>quarter</baseNote>
<Number><size>12</size></Number><size>15</size></Tuplet>
<Chord><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
<endTuplet/><Rest><durationType>half</durationType></Rest></voice></Measure></Staff></Score></museScore>)";
    for (const std::string version : {"4.60", "5.00"}) {
        auto xml = std::string(fixture);
        xml.replace(xml.find("5.00"), 4, version);
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document) << document.error().message;
        auto &master = master_of(**document);
        auto *chord = mu::engraving::to_chord(master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST)->element(0));
        auto *tuplet = chord->tuplet();
        ASSERT_NE(tuplet, nullptr);
        EXPECT_DOUBLE_EQ(tuplet->get_property(mu::engraving::Pid::FONT_SIZE).to_double(), 15.0);
        const auto flags = tuplet->property_flags(mu::engraving::Pid::FONT_SIZE);
        EXPECT_EQ(tuplet->number(), nullptr);
        EXPECT_FALSE(tuplet->has_layout_data());
        EXPECT_DOUBLE_EQ(tuplet->get_property(mu::engraving::Pid::FONT_SIZE).to_double(), 15.0);
        EXPECT_EQ(tuplet->property_flags(mu::engraving::Pid::FONT_SIZE), flags);
        std::unique_ptr<mu::engraving::Tuplet> copy(tuplet->clone());
        EXPECT_DOUBLE_EQ(copy->get_property(mu::engraving::Pid::FONT_SIZE).to_double(), 15.0);
        ASSERT_TRUE((*document)->page_count());
        ASSERT_NE(tuplet->number(), nullptr);
        EXPECT_DOUBLE_EQ(tuplet->number()->get_property(mu::engraving::Pid::FONT_SIZE).to_double(), 15.0);
        EXPECT_EQ(tuplet->property_flags(mu::engraving::Pid::FONT_SIZE), flags);
    }
}

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

TEST(Read, ColdSvgDimensionsAndLockedResizeUseIntrinsicAspect) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *measure = master.first_measure();
    std::unique_ptr<Image> image(Factory::create_image(measure));
    const muse::ByteArray svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="12"><rect width="24" height="12"/></svg>)");
    ASSERT_TRUE(image->load_from_data("s5b-intrinsic.svg", svg));
    image->set_size_is_spatium(false);
    EXPECT_DOUBLE_EQ(image->get_property(Pid::IMAGE_WIDTH).to_double(), 24.0 / dpmm);
    EXPECT_DOUBLE_EQ(image->get_property(Pid::IMAGE_HEIGHT).to_double(), 12.0 / dpmm);
    EXPECT_EQ(image->get_property(Pid::SIZE).value<muse::SizeF>(), muse::SizeF(24.0 / dpmm, 12.0 / dpmm));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    rendering::score::TLayout::layout_image(image.get(), image->mutldata());
    EXPECT_NEAR(image->ldata()->bbox().width(), 24.0, 1e-9);
    EXPECT_NEAR(image->ldata()->bbox().height(), 12.0, 1e-9);
    ASSERT_TRUE(image->set_property(Pid::IMAGE_WIDTH, 36.0));
    EXPECT_DOUBLE_EQ(image->image_height(), 18.0);
    ASSERT_TRUE(image->set_property(Pid::IMAGE_HEIGHT, 7.0));
    EXPECT_DOUBLE_EQ(image->image_width(), 14.0);
}

TEST(Read, ImageReloadReleasesOldStoreReferencesBeforeArchiveWrite) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    ImageStoreItem *first = nullptr;
    ImageStoreItem *second = nullptr;
    {
        Image image(master.first_measure());
        ASSERT_TRUE(image.load_from_data("s22-first.svg", muse::ByteArray(R"(<svg width="31" height="17"/>)")));
        first = image.store_item();
        {
            Image copy(image);
            ASSERT_TRUE(image.load_from_data("s22-second.svg", muse::ByteArray(R"(<svg width="37" height="19"/>)")));
            second = image.store_item();
            EXPECT_TRUE(first->is_used(&master));
        }
        EXPECT_FALSE(first->is_used());
        EXPECT_FALSE(first->is_used(&master));
        ASSERT_TRUE(image.load_from_data("s22-second.svg", second->buffer()));
        EXPECT_TRUE(second->is_used(&master));
        ASSERT_TRUE(image.load_from_store(first->hash_name()));
        EXPECT_FALSE(second->is_used());
        EXPECT_EQ(image.image_size(), muse::SizeF(31, 17));
        ASSERT_TRUE(image.load_from_store(first->hash_name()));
        EXPECT_TRUE(first->is_used(&master));
        EXPECT_FALSE(image.load_from_store("missing.svg"));
        Image copy(image);
        EXPECT_EQ(copy.image_size(), muse::SizeF(31, 17));
    }
    EXPECT_FALSE(first->is_used());
    EXPECT_FALSE(second->is_used());

    const auto temp_root = std::filesystem::temp_directory_path();
    std::filesystem::path run_dir;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto candidate = temp_root / ("engrave_read_s22_" + std::to_string(tick) + "_" + std::to_string(attempt));
        std::error_code mkdir_ec;
        if (std::filesystem::create_directory(candidate, mkdir_ec)) {
            run_dir = candidate;
            break;
        }
    }
    ASSERT_FALSE(run_dir.empty());

    struct Cleanup {
        std::filesystem::path dir;

        ~Cleanup() {
            if (dir.empty()) return;
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
    } cleanup{run_dir};

    const auto file1 = run_dir / "file1.svg";
    const auto file2 = run_dir / "file2.svg";
    const auto missing_file = run_dir / "missing.svg";
    const std::string svg1 = R"(<svg width="41" height="23"/>)";
    const std::string svg2 = R"(<svg width="53" height="29"/>)";

    {
        std::ofstream out1(file1, std::ios::binary);
        out1 << svg1;
        ASSERT_TRUE(static_cast<bool>(out1));
        std::ofstream out2(file2, std::ios::binary);
        out2 << svg2;
        ASSERT_TRUE(static_cast<bool>(out2));
    }

    ImageStoreItem *file_first = nullptr;
    ImageStoreItem *file_second = nullptr;
    {
        Image file_image(master.first_measure());
        ASSERT_TRUE(file_image.load_from_file(muse::io::PathT(file1.generic_string())));
        file_first = file_image.store_item();
        ASSERT_NE(file_first, nullptr);
        EXPECT_TRUE(file_first->is_used(&master));
        EXPECT_EQ(file_image.image_size(), muse::SizeF(41, 23));

        ASSERT_TRUE(file_image.load_from_file(muse::io::PathT(file2.generic_string())));
        file_second = file_image.store_item();
        ASSERT_NE(file_second, nullptr);
        ASSERT_NE(file_second, file_first);
        EXPECT_FALSE(file_first->is_used());
        EXPECT_TRUE(file_second->is_used(&master));
        EXPECT_EQ(file_image.image_size(), muse::SizeF(53, 29));
        rendering::score::TLayout::layout_image(&file_image, file_image.mutldata());
        const muse::RectF laid_out_bbox = file_image.ldata()->bbox();
        EXPECT_NEAR(laid_out_bbox.width(), 53.0, 1e-9);
        EXPECT_NEAR(laid_out_bbox.height(), 29.0, 1e-9);
        const std::string previous_store_path = file_image.store_path();

        EXPECT_FALSE(file_image.load_from_file(muse::io::PathT(missing_file.generic_string())));
        EXPECT_EQ(file_image.store_item(), file_second);
        EXPECT_EQ(file_image.store_path(), previous_store_path);
        EXPECT_TRUE(file_second->is_used(&master));
        EXPECT_TRUE(file_image.is_valid());
        ASSERT_NE(file_image.svg_renderer(), nullptr);
        EXPECT_EQ(file_image.svg_renderer()->default_size(), muse::SizeF(53, 29));
        EXPECT_EQ(file_image.image_size(), muse::SizeF(53, 29));
        EXPECT_EQ(file_second->buffer(), muse::ByteArray(svg2.data(), svg2.size()));
        rendering::score::TLayout::layout_image(&file_image, file_image.mutldata());
        EXPECT_EQ(file_image.ldata()->bbox(), laid_out_bbox);
    }
    EXPECT_FALSE(file_first->is_used());
    EXPECT_FALSE(file_second->is_used());

    auto other = g_engine->open(write470_fixture());
    ASSERT_TRUE(other);
    EXPECT_FALSE(first->is_used(&master_of(**other)));
    EXPECT_FALSE(second->is_used(&master_of(**other)));
    EXPECT_FALSE(file_first->is_used(&master_of(**other)));
    EXPECT_FALSE(file_second->is_used(&master_of(**other)));
    const auto archive = (*other)->export_score({.format = engrave::Format::MSCZ});
    ASSERT_TRUE(archive) << archive.error().message;
    EXPECT_TRUE(g_engine->open(*archive));
}

TEST(Read, SvgIntrinsicPhysicalUnitsAndViewBoxAreAvailableCold) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    Image image(master.first_measure());
    ASSERT_TRUE(image.load_from_data("s5b-mm.svg", muse::ByteArray(R"(<svg width="25.4mm" height="12.7mm"/>)")));
    EXPECT_NEAR(image.image_size().width(), 96.0, 1e-9);
    EXPECT_NEAR(image.image_size().height(), 48.0, 1e-9);
    ASSERT_TRUE(image.load_from_data("s5b-viewbox.svg", muse::ByteArray(R"(<svg viewBox="0,0,24,12"/>)")));
    EXPECT_EQ(image.image_size(), muse::SizeF(24.0, 12.0));
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
}

TEST(Read, CapoAutoDisplayRefreshesWithoutChangingAuthoredText) {
    using namespace mu::engraving;
    for (const std::string authored : {"", "My capo"}) {
        auto source = read_text(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/tab_grace_prebend.mscx");
        source.insert(source.find("<voice>") + 7,
                      "<Capo>" + (authored.empty() ? std::string() : "<text>" + authored + "</text>") + "</Capo>");
        auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
        ASSERT_TRUE(opened);
        auto &master = master_of(**opened);
        Capo *capo = nullptr;
        for (auto *object : engrave::document_objects(master))
            if (object->is_capo()) {
                capo = to_capo(object);
                break;
            }
        ASSERT_NE(capo, nullptr);
        const auto before = legacy_override_write(capo);
        const auto expect_source = [&] {
            EXPECT_EQ(capo->xml_text(), muse::String::from_std_string(authored));
            EXPECT_EQ(capo->plain_text(), muse::String::from_std_string(authored));
            auto text = (*opened)->get(engrave::from_engine(capo->eid()), {"text"});
            ASSERT_TRUE(text);
            EXPECT_EQ(std::get<std::string>(text->front().value), authored);
        };
        expect_source();
        master.ensure_layout();
        const auto display = [&] {
            muse::String text;
            for (const auto &block : capo->ldata()->blocks)
                for (const auto &fragment : block.fragments()) text += fragment.text;
            return text;
        };
        EXPECT_EQ(display(), u"Capo 1");
        expect_source();
        EXPECT_EQ(legacy_override_write(capo), before);
        ASSERT_TRUE(capo->set_property(Pid::CAPO_FRET_POSITION, 2));
        master.ensure_layout();
        EXPECT_EQ(display(), u"Capo 2");
        ASSERT_TRUE(capo->set_property(Pid::ACTIVE, false));
        master.ensure_layout();
        EXPECT_EQ(display(), u"No capo");
        expect_source();
        ASSERT_TRUE(capo->set_property(Pid::CAPO_GENERATE_TEXT, false));
        master.ensure_layout();
        EXPECT_EQ(display(), authored.empty() ? muse::String(u"No capo") : muse::String(u"My capo"));
    }
}

TEST(Read, SparseChildOverridesSurviveLayoutAndMirrorChanges) {
    using namespace mu::engraving;
    for (const std::string version : {"4.60", "5.00"}) {
        auto input = legacy_segment_fixture("");
        input.replace(input.find("5.00"), 4, version);
        input.insert(
            input.find("<Note>"),
            R"(<Stem><eid>D_D</eid><userLen>1.25</userLen><lineWidth>0.2</lineWidth><offset x="0.25" y="-0.5"/><visible>0</visible></Stem>
<Hook><name>flag8thUp</name><offset x="0.5" y="0"/></Hook>
<StemSlash><offset x="-0.25" y="0.5"/></StemSlash>)");
        input.insert(input.find("</Note>"), R"(<NoteDot><eid>E_E</eid><offset x="0.75" y="-0.25"/><visible>0</visible></NoteDot>)");
        auto document = g_engine->open(as_bytes(input));
        ASSERT_TRUE(document) << document.error().message;
        Note *note = nullptr;
        master_of(**document).scan_elements([&](EngravingItem *item) {
            if (!note && item->is_note()) note = to_note(item);
        });
        ASSERT_NE(note, nullptr);
        auto *chord = note->chord();
        const auto source = chord->authored_overrides;
        const auto dots = note->authored_overrides;
        ASSERT_NE(source.find({AuthoredChildRole::STEM}, Pid::OFFSET), nullptr);
        ASSERT_NE(source.find({AuthoredChildRole::HOOK}, Pid::SYMBOL), nullptr);
        ASSERT_NE(dots.find({AuthoredChildRole::DOT}, Pid::OFFSET), nullptr);
        const auto before = legacy_override_write(chord);
        EXPECT_EQ(chord->stem(), nullptr);
        ASSERT_TRUE((*document)->page_count());
        ASSERT_NE(chord->stem(), nullptr);
        chord->stem()->set_offset(PointF(99, 99)); // Disposable output cannot change a cold save.
        chord->stem()->set_line_width(Spatium(9));
        EXPECT_EQ(legacy_override_write(chord), before);
        ASSERT_TRUE((*document)->page_count());
        EXPECT_EQ(legacy_override_write(chord), before);
        const auto expect_source = [](const auto &expected, const auto &actual) {
            ASSERT_EQ(expected.entries.size(), actual.entries.size());
            ASSERT_EQ(expected.carriers.size(), actual.carriers.size());
            for (size_t i = 0; i < expected.entries.size(); ++i) {
                EXPECT_EQ(expected.entries[i].child, actual.entries[i].child);
                EXPECT_EQ(expected.entries[i].pid, actual.entries[i].pid);
                EXPECT_EQ(expected.entries[i].value, actual.entries[i].value);
                EXPECT_EQ(expected.entries[i].flags, actual.entries[i].flags);
                EXPECT_EQ(expected.entries[i].unit, actual.entries[i].unit);
            }
            for (size_t i = 0; i < expected.carriers.size(); ++i) {
                EXPECT_EQ(expected.carriers[i].child, actual.carriers[i].child);
                EXPECT_EQ(expected.carriers[i].legacy_eid, actual.carriers[i].legacy_eid);
                EXPECT_EQ(expected.carriers[i].track, actual.carriers[i].track);
            }
        };
        expect_source(source, chord->authored_overrides);
        expect_source(dots, note->authored_overrides);
        const auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened) << reopened.error().message;
    }
}

TEST(Read, TupletNumberTextSurvivesDisplayDisposalAndLayout) {
    using namespace mu::engraving;
    const std::string fixture = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>quarter</baseNote>
<Number><eid>G_G</eid><text>three</text><offset x="0.25" y="-0.5"/></Number></Tuplet>
<Chord><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
<endTuplet/><Rest><durationType>half</durationType></Rest></voice></Measure></Staff></Score></museScore>)";
    for (const std::string version : {"4.60", "5.00"})
        for (bool millimetres : {false, true})
            for (bool unit_last : {false, true}) {
                auto input = fixture;
                if (millimetres)
                    input.insert(unit_last ? input.find("</Number>") : input.find("<Number>") + 8,
                                 "<sizeIsSpatiumDependent>0</sizeIsSpatiumDependent>");
                input.replace(input.find("5.00"), 4, version);
                auto document = g_engine->open(as_bytes(input));
                ASSERT_TRUE(document) << document.error().message;
                auto *chord = to_chord(master_of(**document).first_measure()->first(SegmentType::CHORD_REST)->element(0));
                auto *tuplet = chord->tuplet();
                ASSERT_NE(tuplet, nullptr);
                const AuthoredChildKey key{AuthoredChildRole::TUPLET_NUMBER};
                ASSERT_NE(tuplet->authored_overrides.find(key, Pid::TEXT), nullptr);
                EXPECT_EQ(tuplet->number(), nullptr);
                EXPECT_EQ(tuplet->authored_overrides.find(key, Pid::TEXT)->value, PropertyValue(String(u"three")));
                ASSERT_TRUE((*document)->page_count());
                EXPECT_EQ(tuplet->number()->xml_text(), String(u"three"));
                EXPECT_TRUE(tuplet->children().empty());
                tuplet->set_color(muse::draw::Color(10, 20, 30));
                tuplet->set_visible(false);
                {
                    rendering::score::LayoutContext ctx(tuplet->score());
                    rendering::score::TupletLayout::layout(tuplet, ctx);
                }
                EXPECT_EQ(tuplet->number()->color(), muse::draw::Color(10, 20, 30));
                EXPECT_FALSE(tuplet->number()->visible());
                ASSERT_NE(tuplet->authored_overrides.find(key, Pid::OFFSET), nullptr);
                const auto unit = millimetres ? AuthoredOverrideUnit::MILLIMETRE : AuthoredOverrideUnit::SPATIUM;
                EXPECT_EQ(tuplet->authored_overrides.find(key, Pid::OFFSET)->unit, unit);
                EXPECT_EQ(tuplet->authored_overrides.find(key, Pid::OFFSET)->value, PropertyValue(PointF(0.25, -0.5)));
                ASSERT_TRUE(set_child_override(*tuplet, key, Pid::TEXT, String(u"chosen"), PropertyFlags::NOSTYLE));
                const auto before = legacy_override_write(tuplet);
                const auto entries = tuplet->authored_overrides.entries;
                delete tuplet->number();
                tuplet->mutldata()->number = nullptr;
                EXPECT_EQ(legacy_override_write(tuplet), before);
                ASSERT_TRUE((*document)->page_count());
                ASSERT_NE(tuplet->number(), nullptr);
                EXPECT_EQ(tuplet->number()->xml_text(), String(u"chosen"));
                EXPECT_EQ(legacy_override_write(tuplet), before);
                ASSERT_EQ(tuplet->authored_overrides.entries.size(), entries.size());
                for (size_t i = 0; i < entries.size(); ++i) {
                    EXPECT_EQ(tuplet->authored_overrides.entries[i].value, entries[i].value);
                    EXPECT_EQ(tuplet->authored_overrides.entries[i].flags, entries[i].flags);
                }
                auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
                ASSERT_TRUE(saved) << saved.error().message;
                auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
                ASSERT_TRUE(reopened) << reopened.error().message;
                auto *restored = to_chord(master_of(**reopened).first_measure()->first(SegmentType::CHORD_REST)->element(0))->tuplet();
                ASSERT_NE(restored->authored_overrides.carrier(key), nullptr);
                EXPECT_EQ(restored->authored_overrides.carrier(key)->legacy_eid.to_std_string(), "G_G");
                EXPECT_EQ(restored->authored_overrides.find(key, Pid::TEXT)->value, PropertyValue(String(u"chosen")));
                ASSERT_NE(restored->authored_overrides.find(key, Pid::OFFSET), nullptr);
                EXPECT_EQ(restored->authored_overrides.find(key, Pid::OFFSET)->unit, unit);
                EXPECT_EQ(restored->authored_overrides.find(key, Pid::OFFSET)->value, PropertyValue(PointF(0.25, -0.5)));
            }
}

TEST(Read, LegacyLineSegmentOrdinalAndExplicitZeroBaselineBeforeLayout) {
    const auto xml = legacy_segment_fixture(R"(<HairPin><eid>F_F</eid><track>0</track><subtype>0</subtype>
<startTick>0/1</startTick><ticks>1/1</ticks><track2>0</track2>
<Segment><subtype>1</subtype><offset x="0.25" y="0.5"/><off2 x="-0.25" y="0"/><minDistance>0</minDistance></Segment>
<Segment><subtype>3</subtype><offset x="0" y="-0.25"/><off2 x="1" y="0"/></Segment></HairPin>)");
    auto document = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(document) << document.error().message;
    auto &master = master_of(**document);
    ASSERT_EQ(master.spanner_map().map().size(), 1u);
    auto *line = master.spanner_map().map().begin()->second;
    using namespace mu::engraving;
    EXPECT_TRUE(line->segments_empty());
    ASSERT_NE(line->authored_overrides.find({AuthoredChildRole::SPANNER_SEGMENT}, Pid::MIN_DISTANCE), nullptr);
    EXPECT_DOUBLE_EQ(line->authored_overrides.find({AuthoredChildRole::SPANNER_SEGMENT}, Pid::MIN_DISTANCE)->value.to_double(), 0.0);
    EXPECT_EQ(line->authored_overrides.find({AuthoredChildRole::SPANNER_SEGMENT, 1}, Pid::OFFSET2)->value, PropertyValue(PointF(1, 0)));
    const auto bytes = legacy_override_write(line);
    EXPECT_EQ(legacy_override_write(line), bytes);
    EXPECT_NE(bytes.find("<minDistance>0</minDistance>"), std::string::npos);
    auto reopened = g_engine->open(as_bytes(legacy_segment_fixture(bytes)));
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto &restored = master_of(**reopened).spanner_map().map();
    ASSERT_EQ(restored.size(), 1u);
    EXPECT_EQ(legacy_override_write(restored.begin()->second), bytes);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    const auto source = line->authored_overrides.entries;
    ASSERT_TRUE((*document)->page_count());
    EXPECT_EQ(legacy_override_write(line), bytes);
    ASSERT_EQ(line->authored_overrides.entries.size(), source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        EXPECT_EQ(line->authored_overrides.entries[i].value, source[i].value);
        EXPECT_EQ(line->authored_overrides.entries[i].flags, source[i].flags);
    }
    line->erase_spanner_segments();
    EXPECT_EQ(legacy_override_write(line), bytes);
}

TEST(Read, AuthoredSpannerOffsetsRenderAndColdEditSurvivesReopen) {
    using namespace mu::engraving;
    for (const std::string version : {"4.60", "5.00"}) {
        auto xml = legacy_segment_fixture(R"(<HairPin><eid>F_F</eid><track>0</track><subtype>0</subtype>
<startTick>0/1</startTick><ticks>1/1</ticks><track2>0</track2>
<Segment no="0"><offset x="0.25" y="0.5"/><off2 x="-0.25" y="0"/></Segment></HairPin>)");
        SCOPED_TRACE(version);
        if (version == "4.60") {
            xml = legacy_segment_fixture("");
            xml.replace(xml.find("5.00"), 4, version);
            xml.insert(xml.find("<Chord>"), R"(<Spanner type="HairPin"><HairPin><eid>F_F</eid><subtype>0</subtype>
<Segment no="0"><offset x="0.25" y="0.5"/><off2 x="-0.25" y="0"/></Segment></HairPin>
<next><location><fractions>1/1</fractions></location></next></Spanner>)");
            xml.insert(xml.find("</voice>"),
                       R"(<Spanner type="HairPin"><prev><location><fractions>-1/1</fractions></location></prev></Spanner>)");
        }
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened) << opened.error().message;
        auto &document = **opened;
        auto &master = master_of(document);
        ASSERT_FALSE(master.spanner_map().map().empty());
        auto *line = master.spanner_map().map().begin()->second;
        const AuthoredChildKey key{AuthoredChildRole::SPANNER_SEGMENT};
        EXPECT_TRUE(line->segments_empty());
        EXPECT_EQ(line->front_segment(), nullptr);
        const auto *entry = line->authored_overrides.find(key, Pid::OFFSET);
        ASSERT_NE(entry, nullptr);
        EXPECT_EQ(entry->value, PropertyValue(PointF(0.25, 0.5)));
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(line->front_segment(), nullptr);
        EXPECT_EQ(line->front_segment()->offset() / master.style().spatium(), PointF(0.25, 0.5));
        master.layout_data_store()->clear();
        EXPECT_TRUE(line->segments_empty());
        ASSERT_TRUE(set_child_override(*line, key, Pid::OFFSET2, PointF(1.5, -0.75) * master.style().spatium(), PropertyFlags::NOSTYLE));
        EXPECT_TRUE(line->segments_empty());
        auto saved = document.export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto &copy = master_of(**reopened);
        auto *restored = copy.spanner_map().map().begin()->second;
        const auto *offset = restored->authored_overrides.find(key, Pid::OFFSET2);
        ASSERT_NE(offset, nullptr);
        EXPECT_EQ(offset->value, PropertyValue(PointF(1.5, -0.75)));
        EXPECT_TRUE(restored->segments_empty());
        ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(restored->front_segment(), nullptr);
        EXPECT_EQ(restored->front_segment()->user_off2() / copy.style().spatium(), PointF(1.5, 0));
        ASSERT_TRUE(reset_child_override(*restored, key, Pid::OFFSET2));
        ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(restored->front_segment(), nullptr);
        EXPECT_EQ(restored->front_segment()->user_off2(), PointF());
        restored->set_property(Pid::SPANNER_TICKS, Fraction(1, 2));
        EXPECT_TRUE(restored->segments_empty());
        EXPECT_EQ(restored->front_segment(), nullptr);
    }
}

TEST(Read, SegmentColorCreationAndReusePreserveSourceAndDrawAuthoredColor) {
    using namespace mu::engraving;
    auto input = read_text(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/native-save-overrides.mscx");
    input.insert(input.find("<guitarBendType>"), R"(<color r="153" g="51" b="17" a="255"/>)");
    auto opened = g_engine->open(as_bytes(input));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto resolved = engrave::resolve(master, *engrave::element_id_from_string("H_H"));
    ASSERT_TRUE(resolved);
    auto *bend = to_guitar_bend(resolved->object);
    EXPECT_EQ(bend->color(), muse::draw::Color(0, 0, 0));
    std::unique_ptr<SpannerSegment> carrier(bend->create_line_segment());
    EXPECT_EQ(bend->color(), muse::draw::Color(0, 0, 0));
    carrier.reset();
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    ASSERT_NE(bend->front_segment(), nullptr);
    auto *segment = bend->front_segment();
    EXPECT_EQ(segment->color(), muse::draw::Color(153, 51, 17));
    EXPECT_EQ(bend->color(), muse::draw::Color(0, 0, 0));
    bend->set_property(Pid::COLOR, muse::draw::Color(17, 51, 153));
    const auto authored_raw_color = bend->color();
    const auto rendered = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(rendered);
    EXPECT_EQ(bend->front_segment(), segment);
    EXPECT_EQ(segment->color(), muse::draw::Color(17, 51, 153));
    EXPECT_EQ(bend->color(), authored_raw_color);
    const auto &bytes = rendered->pages.front();
    const std::string svg(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    EXPECT_NE(svg.find("stroke=\"rgb(6.666667%, 20%, 60%)\""), std::string::npos);
}

TEST(Read, LaterSystemsPreserveTheBeginSegmentVisibilityOverride) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(split_spanner_fixture(R"(<Segment no="0"><visible>0</visible></Segment>)")));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    break_first_measure(master);
    master.ensure_layout();
    auto *line = master.spanner_list().front();
    ASSERT_EQ(line->nsegments(), 2u);
    EXPECT_FALSE(line->front_segment()->visible());
    EXPECT_TRUE(line->back_segment()->visible());
    EXPECT_TRUE(line->visible());
}

TEST(Read, ShrinkRetiresTailBorrowersWithoutClearingOtherCollisions) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(split_spanner_fixture()));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    break_first_measure(master);
    master.ensure_layout();
    auto *line = to_s_line(master.spanner_list().front());
    ASSERT_EQ(line->nsegments(), 2u);
    auto *retired = line->back_segment();
    auto *tail = retired->system();
    auto *head = line->front_segment()->system();
    auto *note = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    const RectF keep(100, 200, 3, 4);
    const RectF remove(110, 210, 5, 6);
    for (auto *system : {head, tail}) {
        for (auto *edge : {&system->staves().front()->skyline().north(), &system->staves().front()->skyline().south()}) {
            edge->elements().emplace_back(keep, note);
            edge->elements().emplace_back(remove, retired);
        }
    }
    auto *source = master.first_measure()->first(SegmentType::CHORD_REST);
    source->staff_shape(0).add(keep, note);
    source->staff_shape(0).add(remove, retired);
    master.systems().pop_back();
    master.cmd_state().lock();
    line->fixup_segments(1, [line] { return line->create_line_segment(); });
    master.cmd_state().unlock();
    master.systems().push_back(tail);
    for (auto *system : {head, tail}) {
        const auto &skyline = system->staves().front()->skyline();
        for (const auto *edge : {&skyline.north(), &skyline.south()}) {
            EXPECT_FALSE(std::ranges::any_of(edge->elements(), [&](const auto &e) { return e.item() == retired; }));
        }
        EXPECT_TRUE(std::ranges::any_of(skyline.north().elements(), [&](const auto &e) { return e.item() == note && e == keep; }));
    }
    EXPECT_TRUE(std::ranges::any_of(source->staff_shape(0).elements(), [&](const auto &e) { return e.item() == note && e == keep; }));
    EXPECT_FALSE(std::ranges::any_of(source->staff_shape(0).elements(), [&](const auto &e) { return e.item() == retired; }));
}

TEST(Read, ShorterRangeLayoutDropsDetachedLineSegmentsAndMatchesFullLayout) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(split_spanner_fixture()));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    break_first_measure(master);
    master.ensure_layout();
    auto *line = master.spanner_list().front();
    ASSERT_EQ(line->nsegments(), 2u);
    line->set_property(Pid::SPANNER_TICKS, Fraction(1, 2));
    master.do_layout_range(Fraction(0, 1), Fraction(2, 1));
    ASSERT_EQ(line->nsegments(), 1u);
    EXPECT_EQ(line->back_segment()->system(), master.first_measure()->system());
    const auto range = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(range);
    master.do_layout();
    const auto full = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(full);
    EXPECT_EQ(range->pages, full->pages);
}

TEST(Read, BulkPositionResetRemovesColdSparseSegmentOffsetsAndGrips) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture(R"(<HairPin><eid>F_F</eid><track>0</track><subtype>0</subtype>
<startTick>0/1</startTick><ticks>1/1</ticks><track2>0</track2>
<Segment no="0"><offset x="0.25" y="0.5"/><off2 x="-0.75" y="1"/></Segment>
<Segment no="2"><offset x="1.25" y="-0.5"/></Segment></HairPin>
<Slur><eid>G_G</eid><track>0</track><startElement>B_B</startElement><endElement>C_C</endElement>
<SlurSegment no="0"><o1 x="0.5" y="-1"/><o4 x="-1" y="0.25"/></SlurSegment></Slur>)");
    for (const bool warm : {false, true}) {
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &master = master_of(**opened);
        if (warm) master.ensure_layout();
        Reset::reset_all_positions(master.transaction_manager()->current_or_dummy_transaction(), &master);
        auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        for (auto *owner : master_of(**reopened).spanner_list()) {
            for (const uint32_t ordinal : {0u, 2u})
                for (const auto pid : {Pid::OFFSET, Pid::OFFSET2, Pid::SLUR_UOFF1, Pid::SLUR_UOFF4})
                    EXPECT_EQ(owner->authored_overrides.find({AuthoredChildRole::SPANNER_SEGMENT, ordinal}, pid), nullptr);
            EXPECT_TRUE(owner->segments_empty());
        }
        auto line = engrave::resolve(master_of(**reopened), *engrave::element_id_from_string("F_F"));
        ASSERT_TRUE(line);
        const auto &table = to_spanner(line->object)->authored_overrides;
        ASSERT_NE(table.carrier({AuthoredChildRole::SPANNER_SEGMENT, 2}), nullptr);
        EXPECT_EQ(table.find({AuthoredChildRole::SPANNER_SEGMENT, 2}, Pid::OFFSET), nullptr);
    }
}

TEST(Read, PartSpannerRelinkClearsLocalPiecesWithoutCopyingMasterOverrides) {
    using namespace mu::engraving;
    const auto fixture = [](bool overrides, bool bend) {
        const auto score = [](bool part, bool local) {
            return std::string(R"(<Score><Division>480</Division><Style><spatium>1.75</spatium>
<hairpinPosAbove x="0" y="0"/><hairpinPosBelow x="0" y="0"/><bendOffset x="0" y="0"/></Style><Part><Staff>)") +
                   (part ? "<linkedTo>S_S</linkedTo>" : "<eid>S_S</eid>") + R"(<StaffType group="pitched"/></Staff>
<Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part><Staff id="1"><Measure><voice>
<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff>
<SpannerMap><HairPin><eid>)" +
                   (part ? "G_G</eid>" : "F_F</eid>") + R"(<track>0</track><subtype>0</subtype>
<color r="0" g="0" b="0" a="255"/><startTick>0/1</startTick><ticks>1/1</ticks><track2>0</track2><Segment no="0"><eid>)" +
                   (part ? "K_K</eid>" : "J_J</eid>") +
                   (local ? (part ? R"(<offset x="0.75" y="-1"/><color r="10" g="80" b="160" a="255"/>)" :
                                    R"(<offset x="1.25" y="-0.5"/><color r="120" g="40" b="10" a="255"/>)") :
                            "") +
                   "</Segment>" +
                   (part ? "<positionLinkedToMaster>1</positionLinkedToMaster><appearanceLinkedToMaster>1</"
                           "appearanceLinkedToMaster><linkedTo>F_F</linkedTo>" :
                           "") +
                   "</HairPin></SpannerMap>";
        };
        auto xml = "<museScore version=\"5.00\">" + score(false, overrides) + score(true, overrides) + "</Score></Score></museScore>";
        if (bend) {
            for (size_t at; (at = xml.find("<StaffType group=\"pitched\"/>")) != std::string::npos;)
                xml.replace(at, 28, "<StaffType group=\"tablature\"><lines>6</lines></StaffType>");
            const std::string strings = "<StringData><frets>24</frets><string>40</string><string>45</string><string>50</string><string>55</"
                                        "string><string>59</string><string>64</string></StringData>";
            for (size_t from = 0, at; (at = xml.find("</Instrument>", from)) != std::string::npos; from = at + strings.size() + 13)
                xml.insert(at, strings);
            for (const bool part : {false, true}) {
                const auto notes = std::string("<Chord><durationType>half</durationType><Note><eid>") + (part ? "N_N" : "B_B") +
                                   "</eid><pitch>60</pitch><tpc>14</tpc><string>1</string><fret>1</fret></Note></Chord>" +
                                   "<Chord><durationType>half</durationType><Note><eid>" + (part ? "O_O" : "C_C") +
                                   "</eid><pitch>62</pitch><tpc>16</tpc><string>1</string><fret>3</fret></Note></Chord>";
                const auto rest = xml.find("<Rest>");
                xml.replace(rest, xml.find("</Rest>", rest) + 7 - rest, notes);
            }
            size_t from = 0;
            for (const bool part : {false, true}) {
                const auto at = xml.find("<ticks>1/1</ticks>", from);
                const auto anchors = std::string("<startElement>") + (part ? "N_N" : "B_B") + "</startElement><endElement>" +
                                     (part ? "O_O" : "C_C") + "</endElement><ticks>1/2</ticks>";
                xml.replace(at, 18, anchors);
                from = at + anchors.size();
            }
            for (size_t at; (at = xml.find("HairPin")) != std::string::npos;) xml.replace(at, 7, "GuitarBend");
            if (overrides) {
                const std::string text = R"(<GuitarBendText><sizeIsSpatiumDependent>0</sizeIsSpatiumDependent>
<offset x="0.5" y="-0.25"/><text>full</text></GuitarBendText>)";
                for (size_t from = 0, at; (at = xml.find("</Segment>", from)) != std::string::npos; from = at + text.size() + 10)
                    xml.insert(at, text);
            }
        }
        if (bend) {
            size_t from = 0;
            for (const auto end_id : {"C_C", "O_O"}) {
                const auto begin = xml.find("<GuitarBend>", xml.find("<SpannerMap>", from));
                const auto end = xml.find("</GuitarBend>", begin) + 13;
                const auto spanner = xml.substr(begin, end - begin);
                xml.erase(begin, end - begin);
                xml.insert(xml.find("</Note>", xml.find(std::string("<eid>") + end_id)), spanner);
                from = end;
            }
        }
        if (!bend) {
            const std::string marker = "</voice></Measure></Staff>";
            const std::string measure = "<Measure><voice><Rest><durationType>whole</durationType></Rest></voice></Measure>";
            for (size_t from = 0, at; (at = xml.find(marker, from)) != std::string::npos; from = at + measure.size() + marker.size())
                xml.insert(at + 18, measure);
            for (size_t from = 0, at; (at = xml.find("<ticks>1/1</ticks>", from)) != std::string::npos; from = at + 18)
                xml.replace(at, 18, "<ticks>2/1</ticks>");
            xml.insert(xml.find("</voice></Measure>") + 8, "<LayoutBreak><subtype>line</subtype></LayoutBreak>");
        }
        return xml;
    };
    const auto find_spanner = [](MasterScore &master, std::string_view eid) {
        auto *object = master.eid_register()->item_from_eid(EID::from_std_string(eid));
        return object && object->is_spanner() ? static_cast<Spanner *>(object) : nullptr;
    };
    const AuthoredChildKey key{AuthoredChildRole::SPANNER_SEGMENT};
    for (const bool unlink_first : {false, true}) {
        for (const bool bend : {false, true}) {
            for (const auto property : {"positionLinkedToMaster", "appearanceLinkedToMaster"}) {
                const bool position = std::string_view(property) == "positionLinkedToMaster";
                for (const bool overrides : {false, true}) {
                    SCOPED_TRACE(bend);
                    SCOPED_TRACE(property);
                    SCOPED_TRACE(overrides);
                    auto opened = g_engine->open(as_bytes(fixture(overrides, bend)));
                    ASSERT_TRUE(opened) << opened.error().message;
                    auto &document = **opened;
                    auto &master = master_of(document);
                    ASSERT_FALSE(master.excerpts().empty());
                    auto *part = master.excerpts().front()->excerpt_score();
                    auto *piece_owner = find_spanner(master, "G_G");
                    ASSERT_NE(piece_owner, nullptr);
                    if (overrides) {
                        if (unlink_first)
                            ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(piece_owner->eid()), property, false}}}));
                        EXPECT_EQ(piece_owner->authored_overrides.find(key, Pid::OFFSET)->value, PropertyValue(PointF(0.75, -1)));
                        EXPECT_EQ(piece_owner->authored_overrides.find(key, Pid::COLOR)->value.value<muse::draw::Color>(),
                                  muse::draw::Color(10, 80, 160));
                        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(piece_owner->eid()), property, true}}}));
                        EXPECT_EQ(piece_owner->authored_overrides.find(key, position ? Pid::OFFSET : Pid::SUBTYPE), nullptr);
                        SCOPED_TRACE(legacy_override_write(piece_owner));
                        auto bytes = document.export_score({.format = engrave::Format::MSCZ});
                        ASSERT_TRUE(bytes);
                        opened = g_engine->open(*bytes, {.format = engrave::Format::MSCZ});
                        ASSERT_TRUE(opened) << opened.error().message;
                        auto &restored = master_of(**opened);
                        part = restored.excerpts().front()->excerpt_score();
                        piece_owner = find_spanner(restored, "G_G");
                        ASSERT_NE(piece_owner, nullptr);
                        SCOPED_TRACE(legacy_override_write(piece_owner));
                        const auto &source = find_spanner(restored, "F_F")->authored_overrides;
                        EXPECT_EQ(source.find(key, Pid::OFFSET)->value, PropertyValue(PointF(1.25, -0.5)));
                        EXPECT_EQ(source.find(key, Pid::COLOR)->value.value<muse::draw::Color>(), muse::draw::Color(120, 40, 10));
                        EXPECT_EQ(piece_owner->authored_overrides.find(key, position ? Pid::OFFSET : Pid::COLOR), nullptr);
                        EXPECT_EQ(piece_owner->authored_overrides.find(key, position ? Pid::OFFSET2 : Pid::SUBTYPE), nullptr);
                        if (bend) {
                            const AuthoredChildKey text{AuthoredChildRole::SEGMENT_TEXT};
                            EXPECT_EQ(piece_owner->authored_overrides.find(text, position ? Pid::OFFSET : Pid::SIZE_SPATIUM_DEPENDENT),
                                      nullptr);
                            if (!position) {
                                const auto *offset = piece_owner->authored_overrides.find(text, Pid::OFFSET);
                                ASSERT_NE(offset, nullptr);
                                EXPECT_NEAR(offset->value.value<PointF>().x(), 2.0 / 7.0, 1e-6);
                                EXPECT_NEAR(offset->value.value<PointF>().y(), -1.0 / 7.0, 1e-6);
                            }
                        }
                        EXPECT_EQ(piece_owner->authored_overrides.carrier(key)->legacy_eid.to_std_string(), "K_K");
                        EXPECT_TRUE(part->pages().empty());
                        EXPECT_TRUE(restored.pages().empty());
                    }
                    if (!bend) {
                        master_of(**opened).ensure_layout();
                        EXPECT_EQ(find_spanner(master_of(**opened), "F_F")->nsegments(), 2u);
                    }
                    part->ensure_layout();
                    if (!bend) EXPECT_EQ(piece_owner->nsegments(), 1u);
                    ASSERT_NE(piece_owner->front_segment(), nullptr);
                    if (bend) {
                        const auto offset = to_guitar_bend_segment(piece_owner->front_segment())->bend_text()->offset();
                        EXPECT_NEAR(offset.x() / ::mu::engraving::dpmm, overrides && !position ? 0.5 : 0.0, 1e-6);
                        EXPECT_NEAR(offset.y() / ::mu::engraving::dpmm, overrides && !position ? -0.25 : 0.0, 1e-6);
                    } else if (position)
                        EXPECT_EQ(piece_owner->front_segment()->offset(), PointF());
                    else
                        EXPECT_EQ(piece_owner->front_segment()->color(), muse::draw::Color(0, 0, 0));
                }
            }
        }
    }
}

TEST(Read, BendTextOffsetRetainsPhysicalPositionAcrossColdStyleChangeAndSave) {
    using namespace mu::engraving;
    const AuthoredChildKey key{AuthoredChildRole::SEGMENT_TEXT};
    for (const bool dependent : {false, true}) {
        auto xml = legacy_segment_fixture("");
        xml.insert(xml.find("</Style>"), dependent ? "<bendFontSpatiumDependent>1</bendFontSpatiumDependent>" :
                                                     "<bendFontSpatiumDependent>0</bendFontSpatiumDependent>");
        xml.insert(xml.find("<Note>") + 6, "<eid>N_N</eid>");
        xml.insert(xml.rfind("<Note>") + 6, "<eid>O_O</eid>");
        xml.insert(xml.rfind("</Note>"), R"(<GuitarBend><eid>F_F</eid><track>0</track>
<startElement>N_N</startElement><endElement>O_O</endElement><Segment><GuitarBendText>
<offset x="0.5" y="-0.25"/><text>full</text></GuitarBendText></Segment></GuitarBend>)");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        ASSERT_TRUE((*opened)->edit({{engrave::SetStyle{{}, "bendFontSpatiumDependent", !dependent}}}));
        auto bytes = (*opened)->export_score({.format = engrave::Format::MSCZ});
        ASSERT_TRUE(bytes);
        auto reopened = g_engine->open(*bytes, {.format = engrave::Format::MSCZ});
        ASSERT_TRUE(reopened);
        auto &master = master_of(**reopened);
        auto *bend = static_cast<GuitarBend *>(master.eid_register()->item_from_eid(EID::from_std_string(std::string_view("F_F"))));
        ASSERT_NE(bend, nullptr);
        const auto *offset = bend->authored_overrides.find(key, Pid::OFFSET);
        ASSERT_NE(offset, nullptr);
        EXPECT_NEAR(offset->value.value<PointF>().x(), dependent ? 0.875 : 2.0 / 7.0, 1e-6);
        EXPECT_NEAR(offset->value.value<PointF>().y(), dependent ? -0.4375 : -1.0 / 7.0, 1e-6);
        EXPECT_EQ(bend->authored_overrides.find(key, Pid::SIZE_SPATIUM_DEPENDENT), nullptr);
        EXPECT_TRUE(master.pages().empty());
    }
}

TEST(Read, LegacySlurGripOrdinalGapsBaselineBeforeLayout) {
    const auto xml = legacy_segment_fixture(R"(<Slur><eid>F_F</eid><track>0</track>
<startElement>B_B</startElement><endElement>C_C</endElement>
<SlurSegment no="0"><o1 x="0.25" y="-0.5"/><o2 x="0" y="0.25"/></SlurSegment>
<SlurSegment no="2"><o3 x="-0.75" y="0"/><o4 x="0" y="0.5"/></SlurSegment></Slur>)");
    auto document = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(document) << document.error().message;
    auto &master = master_of(**document);
    ASSERT_EQ(master.spanner_map().map().size(), 1u);
    auto *slur = master.spanner_map().map().begin()->second;
    using namespace mu::engraving;
    EXPECT_TRUE(slur->segments_empty());
    ASSERT_NE(slur->authored_overrides.find({AuthoredChildRole::SPANNER_SEGMENT, 2}, Pid::SLUR_UOFF3), nullptr);
    EXPECT_EQ(slur->authored_overrides.find({AuthoredChildRole::SPANNER_SEGMENT, 2}, Pid::SLUR_UOFF3)->value,
              PropertyValue(PointF(-0.75, 0)));
    const auto bytes = legacy_override_write(slur);
    EXPECT_NE(bytes.find("no=\"0\""), std::string::npos);
    EXPECT_EQ(bytes.find("no=\"1\""), std::string::npos);
    EXPECT_NE(bytes.find("no=\"2\""), std::string::npos);
    auto reopened = g_engine->open(as_bytes(legacy_segment_fixture(bytes)));
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto &restored = master_of(**reopened).spanner_map().map();
    ASSERT_EQ(restored.size(), 1u);
    EXPECT_TRUE(restored.begin()->second->segments_empty());
    EXPECT_EQ(legacy_override_write(restored.begin()->second), bytes);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    const auto source = slur->authored_overrides.entries;
    ASSERT_TRUE((*document)->page_count());
    EXPECT_EQ(legacy_override_write(slur), bytes);
    ASSERT_EQ(slur->authored_overrides.entries.size(), source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        EXPECT_EQ(slur->authored_overrides.entries[i].value, source[i].value);
        EXPECT_EQ(slur->authored_overrides.entries[i].flags, source[i].flags);
    }
    slur->erase_spanner_segments();
    EXPECT_EQ(legacy_override_write(slur), bytes);
}

TEST(Read, SpannerSegmentRemovalDetachesEveryGuitarTechnique) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    ASSERT_TRUE((*opened)->page_count());
    auto &master = master_of(**opened);
    for (const auto type :
         {ElementType::LET_RING, ElementType::PALM_MUTE, ElementType::WHAMMY_BAR, ElementType::RASGUEADO, ElementType::HARMONIC_MARK,
          ElementType::PICK_SCRAPE, ElementType::HAMMER_ON_PULL_OFF, ElementType::TAPPING_HALF_SLUR}) {
        SCOPED_TRACE(static_cast<int>(type));
        std::unique_ptr<Tapping> tapping(Factory::create_tapping(master.dummy()));
        std::unique_ptr<Spanner> spanner(type == ElementType::TAPPING_HALF_SLUR ?
                                             new TappingHalfSlur(tapping.get()) :
                                             to_spanner(Factory::create_item(type, master.first_measure())));
        const auto create_segment = [&]() -> SpannerSegment * {
            if (spanner->is_slur()) return to_slur(spanner.get())->new_slur_tie_segment();
            return to_s_line(spanner.get())->create_line_segment();
        };

        struct CmdLock {
            MasterScore &score;

            explicit CmdLock(MasterScore &score) : score(score) { score.cmd_state().lock(); }

            ~CmdLock() { score.cmd_state().unlock(); }
        };

        const CmdLock layout_lock(master);
        std::unique_ptr<System> first(Factory::create_system(&master));
        std::unique_ptr<System> second(Factory::create_system(&master));
        first->mutldata();
        second->mutldata();
        ASSERT_TRUE(first->is_layout_data_current());
        ASSERT_TRUE(second->is_layout_data_current());

        auto *segment = create_segment();
        spanner->add(segment);
        segment->move_to_system(first.get());
        ASSERT_TRUE(first->is_layout_data_current());
        ASSERT_EQ(first->spanner_segments().size(), 1u);
        EXPECT_EQ(first->spanner_segments().front(), segment);

        segment->move_to_system(second.get());
        ASSERT_TRUE(first->is_layout_data_current());
        ASSERT_TRUE(second->is_layout_data_current());
        EXPECT_TRUE(first->spanner_segments().empty());
        ASSERT_EQ(second->spanner_segments().size(), 1u);
        EXPECT_EQ(second->spanner_segments().front(), segment);

        second->add(segment);
        ASSERT_TRUE(second->is_layout_data_current());
        EXPECT_EQ(second->spanner_segments().size(), 1u);
        second->remove(segment);
        ASSERT_TRUE(second->is_layout_data_current());
        EXPECT_TRUE(second->spanner_segments().empty());
        EXPECT_EQ(segment->system(), nullptr);

        segment->move_to_system(first.get());
        ASSERT_TRUE(first->is_layout_data_current());
        ASSERT_EQ(first->spanner_segments().size(), 1u);
        spanner->erase_spanner_segments();
        ASSERT_TRUE(first->is_layout_data_current());
        EXPECT_TRUE(first->spanner_segments().empty());

        segment = create_segment();
        spanner->add(segment);
        {
            std::unique_ptr<System> temporary(Factory::create_system(&master));
            temporary->mutldata();
            segment->move_to_system(temporary.get());
            ASSERT_TRUE(temporary->is_layout_data_current());
            ASSERT_EQ(temporary->spanner_segments().size(), 1u);
        }
        EXPECT_EQ(segment->system(), nullptr);
        spanner->erase_spanner_segments();
    }
}

TEST(Read, LegacySegmentTextAppearanceAndExplicitZeroFontSurviveDiscard) {
    using namespace mu::engraving;
    const auto input = legacy_segment_fixture(R"(<HammerOnPullOff><eid>F_F</eid><track>0</track>
<startElement>B_B</startElement><endElement>C_C</endElement>
<HammerOnPullOffSegment no="0"><o1 x="0.25" y="-0.5"/>
<HammerOnPullOffText idx="0"><family>Edwin</family><bold>0</bold><italic>0</italic>
<offset x="0.5" y="-0.25"/><text>custom</text></HammerOnPullOffText>
</HammerOnPullOffSegment></HammerOnPullOff>)");
    auto document = g_engine->open(as_bytes(input));
    ASSERT_TRUE(document) << document.error().message;
    auto &master = master_of(**document);
    ASSERT_EQ(master.spanner_map().map().size(), 1u);
    auto *spanner = master.spanner_map().map().begin()->second;
    const AuthoredChildKey key{AuthoredChildRole::SEGMENT_TEXT, 0};
    const auto *font = spanner->authored_overrides.find(key, Pid::FONT_STYLE);
    ASSERT_NE(font, nullptr);
    EXPECT_EQ(font->value.to_int(), 0);
    EXPECT_EQ(font->flags, PropertyFlags::UNSTYLED);
    ASSERT_NE(spanner->authored_overrides.find(key, Pid::TEXT), nullptr);
    const auto bytes = legacy_override_write(spanner);
    ASSERT_TRUE((*document)->page_count());
    EXPECT_EQ(legacy_override_write(spanner), bytes);
    spanner->erase_spanner_segments();
    EXPECT_EQ(legacy_override_write(spanner), bytes);
    auto reopened = g_engine->open(as_bytes(legacy_segment_fixture(bytes)));
    ASSERT_TRUE(reopened) << reopened.error().message;
    auto *restored = master_of(**reopened).spanner_map().map().begin()->second;
    EXPECT_EQ(legacy_override_write(restored), bytes);
    ASSERT_NE(restored->authored_overrides.find(key, Pid::FONT_STYLE), nullptr);
    EXPECT_EQ(restored->authored_overrides.find(key, Pid::FONT_STYLE)->flags, PropertyFlags::UNSTYLED);
}

TEST(Read, ImportedDotColorAndColdVisibilitySurviveCloneAndLayout) {
    using namespace mu::engraving;
    for (const std::string version : {"4.60", "5.00"}) {
        auto input = legacy_segment_fixture("");
        input.replace(input.find("5.00"), 4, version);
        input.replace(input.find("<durationType>half</durationType>"), 33, "<dots>1</dots><durationType>quarter</durationType>");
        input.insert(input.find("</Note>"), "<NoteDot><color r=\"10\" g=\"20\" b=\"30\" a=\"255\"/></NoteDot>");
        auto opened = g_engine->open(as_bytes(input));
        ASSERT_TRUE(opened) << opened.error().message;
        auto &master = master_of(**opened);
        auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        auto *original = chord->notes().front();
        EXPECT_TRUE(original->dots().empty());
        EXPECT_FALSE(original->has_layout_data());
        auto *copy = original->clone();
        chord->remove(original);
        delete original;
        chord->add(copy);
        engrave::assign_missing_ids(master);
        ASSERT_TRUE((*opened)->edit({{engrave::SetProperty{engrave::from_engine(copy->eid()), "visible", false}}}));
        EXPECT_TRUE(copy->dots().empty());
        ASSERT_TRUE((*opened)->page_count());
        ASSERT_EQ(copy->dots().size(), 1u);
        EXPECT_EQ(copy->dots().front()->color(), muse::draw::Color(10, 20, 30));
        EXPECT_FALSE(copy->dots().front()->visible());
    }
}

TEST(Read, DeleteLinkedRestsRemovesEmptySegments) {
    using namespace mu::engraving;
    const auto fixture = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/engraving/tests/parts_data/delete-linked-rests.mscx";
    for (bool from_part : {false, true}) {
        SCOPED_TRACE(from_part);
        auto opened = g_engine->open(fixture);
        ASSERT_TRUE(opened) << opened.error().message;
        auto &master = master_of(**opened);
        const auto outline = (*opened)->outline();
        ASSERT_TRUE(outline);
        ASSERT_TRUE((*opened)->edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
        const auto check = [](MasterScore &score, size_t expected) {
            ASSERT_EQ(score.excerpts().size(), 1u);
            for (auto *part : {static_cast<Score *>(&score), score.excerpts().front()->excerpt_score()}) {
                size_t count = 0;
                for (auto *segment = part->first_measure()->first(SegmentType::CHORD_REST); segment;
                     segment = segment->next(SegmentType::CHORD_REST)) {
                    EXPECT_FALSE(segment->empty());
                    auto *item = segment->element(0);
                    ASSERT_NE(item, nullptr);
                    ASSERT_TRUE(item->is_rest());
                    EXPECT_EQ(to_rest(item)->ticks(), expected == 1 ? Fraction(1, 1) : Fraction(1, 4));
                    ++count;
                }
                EXPECT_EQ(count, expected);
            }
        };
        check(master, 4);
        auto *part = master.excerpts().front()->excerpt_score();
        const auto layouts = master.layout_state.whole_count + master.layout_state.range_count;
        const auto part_layouts = part->layout_state.whole_count + part->layout_state.range_count;
        EXPECT_EQ(part_layouts, 0u);
        auto *owner = from_part ? part : &master;
        auto *first = owner->first_measure()->first(SegmentType::CHORD_REST)->element(0);
        ASSERT_TRUE((*opened)->edit({{engrave::SetDuration{engrave::from_engine(first->eid()), engrave::Fraction{1, 1}}}}));
        check(master, 1);
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, layouts);
        EXPECT_EQ(part->layout_state.whole_count + part->layout_state.range_count, part_layouts);
        const auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        check(master_of(**reopened), 1);
    }
}

TEST(Read, RestDotsStayColdUntilLayoutAndRespectSourceVisibility) {
    using namespace mu::engraving;
    for (const std::string version : {"4.60", "5.00"}) {
        for (bool authored_hidden_dot : {false, true}) {
            SCOPED_TRACE(version);
            SCOPED_TRACE(authored_hidden_dot);
            auto input = legacy_segment_fixture("");
            input.replace(input.find("5.00"), 4, version);
            const auto start = input.find("<Chord>");
            const std::string dot = authored_hidden_dot ? "<visible>0</visible>" : "";
            input.replace(start, input.find("</Chord>", start) + 8 - start,
                          "<Rest><dots>1</dots><durationType>quarter</durationType>"
                          "<NoteDot>" +
                              dot + "<color r=\"30\" g=\"20\" b=\"10\" a=\"255\"/></NoteDot></Rest>");
            auto opened = g_engine->open(as_bytes(input));
            ASSERT_TRUE(opened) << opened.error().message;
            auto &master = master_of(**opened);
            auto *rest = to_rest(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
            if (!authored_hidden_dot)
                ASSERT_TRUE((*opened)->edit({{engrave::SetProperty{engrave::from_engine(rest->eid()), "visible", false}}}));
            const auto saved = (*opened)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
            ASSERT_TRUE(saved) << saved.error().message;
            auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
            ASSERT_TRUE(reopened) << reopened.error().message;
            for (auto *document : {opened->get(), reopened->get()}) {
                auto &score = master_of(*document);
                auto *item = to_rest(score.first_measure()->first(SegmentType::CHORD_REST)->element(0));
                EXPECT_EQ(item->visible(), authored_hidden_dot);
                EXPECT_EQ(item->dot(0), nullptr);
                EXPECT_FALSE(item->has_layout_data());
                EXPECT_TRUE(item->dot_list().empty());
                ASSERT_TRUE(document->page_count());
                ASSERT_NE(item->dot(0), nullptr);
                EXPECT_EQ(item->dot(0)->color(), muse::draw::Color(30, 20, 10));
                EXPECT_FALSE(item->dot(0)->visible());
            }
        }
    }
}

TEST(Read, RetainedDotsFollowStaffRemoval) {
    using namespace mu::engraving;
    auto input = legacy_segment_fixture("");
    input.replace(input.find("<durationType>half</durationType>"), 33, "<dots>1</dots><durationType>quarter</durationType>");
    const auto second = input.find("<Chord>", input.find("</Chord>"));
    input.replace(second, input.find("</Chord>", second) + 8 - second, "<Rest><dots>1</dots><durationType>quarter</durationType></Rest>");
    input.insert(input.find("<Instrument"), "<Staff><StaffType group=\"pitched\"/></Staff>");
    const auto start = input.find("<Staff id=\"1\">");
    auto staff = input.substr(start, input.find("</Staff>", start) + 8 - start);
    staff.replace(staff.find("id=\"1\""), 6, "id=\"2\"");
    input.insert(input.find("<SpannerMap>"), staff);
    auto opened = g_engine->open(as_bytes(input));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
    auto *note = to_chord(segment->element(4))->notes().front();
    auto *rest = to_rest(segment->next(SegmentType::CHORD_REST)->element(4));
    ASSERT_TRUE((*opened)->page_count());
    EXPECT_TRUE(std::ranges::none_of(note->children(), [](auto *item) { return item->is_note_dot(); }));
    EXPECT_TRUE(std::ranges::none_of(rest->children(), [](auto *item) { return item->is_note_dot(); }));
    const auto outline = (*opened)->outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE((*opened)->edit({{engrave::RemoveStaff{outline->parts.front().staves.front().id}}}));
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_NE(note->dot(0), nullptr);
    ASSERT_NE(rest->dot(0), nullptr);
    EXPECT_EQ(note->dot(0)->track(), 0u);
    EXPECT_EQ(rest->dot(0)->track(), 0u);
    EXPECT_NE(note->dot(0)->staff(), nullptr);
    EXPECT_NE(rest->dot(0)->staff(), nullptr);
}

TEST(Read, SlashToggleRestoresDotsHiddenBeforeFirstLayout) {
    using namespace mu::engraving;
    auto input = legacy_segment_fixture("");
    input.replace(input.find("<durationType>half</durationType>"), 33, "<dots>1</dots><durationType>quarter</durationType>");
    input.insert(input.find("</Chord>"), "<Note><pitch>64</pitch><tpc>18</tpc></Note>");
    auto opened = g_engine->open(as_bytes(input));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    chord->set_slash(true, true);
    ASSERT_TRUE((*opened)->page_count());
    EXPECT_FALSE(chord->notes().back()->dot(0)->visible());
    chord->set_slash(false, false);
    ASSERT_TRUE((*opened)->page_count());
    for (auto *note : chord->notes()) EXPECT_TRUE(note->dot(0)->visible());
}

TEST(Read, CrossMeasureDisplayDotsDoNotChangeMusicXmlDurations) {
    using namespace mu::engraving;
    auto input = legacy_segment_fixture("");
    input.replace(input.find("<durationType>half</durationType>"), 33, "<durationType>quarter</durationType>");
    input.replace(input.find("<durationType>half</durationType>"), 33, "<dots>1</dots><durationType>eighth</durationType>");
    auto opened = g_engine->open(as_bytes(input));
    ASSERT_TRUE(opened);
    ASSERT_TRUE((*opened)->page_count());
    auto *segment = master_of(**opened).first_measure()->first(SegmentType::CHORD_REST);
    auto *first = to_chord(segment->element(0));
    auto *second = to_chord(segment->next(SegmentType::CHORD_REST)->element(0));
    first->set_cross_measure(CrossMeasure::FIRST);
    first->set_cross_measure_duration_type(TDuration(Fraction(7, 16)));
    second->set_cross_measure(CrossMeasure::SECOND);
    const auto saved = (*opened)->export_score({.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(saved);
    auto restored = g_engine->open(*saved, {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(restored);
    auto *actual = master_of(**restored).first_measure()->first(SegmentType::CHORD_REST);
    EXPECT_EQ(to_chord(actual->element(0))->actual_dots(), 0);
    EXPECT_EQ(to_chord(actual->next(SegmentType::CHORD_REST)->element(0))->actual_dots(), 1);
}

TEST(Read, ThirdDotEditKeepsPrecedingLegacyOrdinalCarriers) {
    using namespace mu::engraving;
    auto input = legacy_segment_fixture("");
    const auto duration = input.find("<durationType>half</durationType>");
    input.replace(duration, std::string("<durationType>half</durationType>").size(), "<durationType>eighth</durationType><dots>3</dots>");
    auto document = g_engine->open(as_bytes(input));
    ASSERT_TRUE(document) << document.error().message;
    ASSERT_TRUE((*document)->page_count());
    auto *chord = to_chord(master_of(**document).first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *note = chord->notes().front();
    ASSERT_EQ(note->dots().size(), 3u);
    ASSERT_TRUE(set_child_override(*note, {AuthoredChildRole::DOT, 2}, Pid::COLOR, muse::draw::Color(10, 20, 30), PropertyFlags::NOSTYLE));
    EXPECT_EQ(note->authored_overrides.find({AuthoredChildRole::DOT, 0}, Pid::COLOR), nullptr);
    ASSERT_NE(note->authored_overrides.find({AuthoredChildRole::DOT, 2}, Pid::COLOR), nullptr);
    auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
    ASSERT_TRUE(saved) << saved.error().message;
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened) << reopened.error().message;
    auto *restored = to_chord(master_of(**reopened).first_measure()->first(SegmentType::CHORD_REST)->element(0))->notes().front();
    EXPECT_EQ(restored->authored_overrides.find({AuthoredChildRole::DOT, 0}, Pid::COLOR), nullptr);
    ASSERT_NE(restored->authored_overrides.find({AuthoredChildRole::DOT, 2}, Pid::COLOR), nullptr);
}

TEST(Read, PublicStemSlashEditReachesTheRenderedGraceBeam) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    xml.insert(xml.find("<Chord>"), R"(<Chord><eid>D_D</eid><acciaccatura/><durationType>eighth</durationType>
<Note><pitch>65</pitch><tpc>13</tpc></Note></Chord>
<Chord><eid>E_E</eid><acciaccatura/><durationType>eighth</durationType>
<Note><pitch>67</pitch><tpc>15</tpc></Note></Chord>)");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *main = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_FALSE(main->grace_notes().empty());
    ASSERT_TRUE(document.page_count());
    auto *first = main->grace_notes().front();
    ASSERT_NE(first->beam(), nullptr);
    EXPECT_EQ(first->authored_beam(), nullptr);
    const auto edited =
        document.edit({{engrave::SetProperty{engrave::from_engine(main->eid()), "color", engrave::Color{20, 40, 60}},
                        engrave::SetProperty{engrave::from_engine(main->grace_notes().back()->eid()), "showStemSlash", false}}});
    ASSERT_TRUE(edited) << edited.error().message;
    for (auto *grace : main->grace_notes()) EXPECT_FALSE(grace->show_stem_slash());
    const auto saved = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    auto *restored = to_chord(master_of(**reopened).first_measure()->first(SegmentType::CHORD_REST)->element(0));
    for (auto *grace : restored->grace_notes()) EXPECT_FALSE(grace->show_stem_slash());
}

TEST(Read, S16RemovingDuplicatePageBreakPreservesFollowingRepeat) {
    using namespace mu::engraving;
    for (bool remove_all : {false, true}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(remove_all);
            SCOPED_TRACE(warm);
            auto xml = legacy_segment_fixture("");
            xml.insert(xml.find("</voice>"), "<BarLine><subtype>double</subtype></BarLine>");
            const std::string ending = "</voice></Measure></Staff>";
            xml.replace(xml.find(ending), ending.size(),
                        "</voice><LayoutBreak><subtype>section</subtype></LayoutBreak>"
                        "<LayoutBreak><subtype>page</subtype></LayoutBreak></Measure>"
                        "<Measure><startRepeat/><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff>");
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *measure = master.first_measure();
            auto *bar = measure->end_bar_line();
            ASSERT_NE(bar, nullptr);
            ASSERT_TRUE(measure->section_break());
            ASSERT_TRUE(measure->page_break());
            auto *section = measure->section_break_element();
            ASSERT_NE(section, nullptr);
            LayoutBreak *page = nullptr;
            for (auto *item : measure->el())
                if (item->is_layout_break() && to_layout_break(item)->is_page_break()) page = to_layout_break(item);
            ASSERT_NE(page, nullptr);
            const auto section_id = engrave::from_engine(section->eid());
            const auto page_id = engrave::from_engine(page->eid());
            if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            const auto layouts = layout_count(master);
            ASSERT_TRUE(document.edit({{engrave::Remove{section_id}}}));
            EXPECT_FALSE(measure->section_break());
            EXPECT_TRUE(measure->page_break());
            if (remove_all) ASSERT_TRUE(document.edit({{engrave::Remove{page_id}}}));
            ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(bar->eid()), "BARLINE_TYPE", engrave::Enum{"end"}}}}));
            EXPECT_EQ(measure->next_measure()->repeat_start(), !remove_all);
            EXPECT_EQ(measure->page_break(), !remove_all);
            EXPECT_EQ(layout_count(master), layouts);
        }
    }
}

TEST(Read, S16RestAnchorUsesEffectiveBeamMode) {
    using namespace mu::engraving;
    for (const std::string mode : {"auto", "no", "begin", "mid"}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(mode);
            SCOPED_TRACE(warm);
            auto xml = legacy_segment_fixture("");
            for (size_t at; (at = xml.find("<durationType>half</durationType>")) != std::string::npos;)
                xml.replace(at, 33, "<durationType>eighth</durationType>");
            xml.insert(xml.find("<Chord>"), "<Beam><eid>F_F</eid></Beam><Rest><durationType>quarter</durationType>"
                                            "<BeamMode>begin</BeamMode></Rest>");
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *rest = to_rest(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
            auto *first = to_chord(rest->segment()->next(SegmentType::CHORD_REST)->element(0));
            auto *second = to_chord(first->segment()->next(SegmentType::CHORD_REST)->element(0));
            auto *beam = rest->authored_beam();
            ASSERT_NE(beam, nullptr);
            if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            const auto layouts = layout_count(master);
            ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(rest->eid()), "BeamMode", engrave::Enum{mode}}}}));
            ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(beam->eid()), "StemDirection", engrave::Enum{"down"}}}}));
            const auto expected = mode == "begin" || mode == "mid" ? DirectionV::DOWN : DirectionV::AUTO;
            EXPECT_EQ(first->stem_direction(), expected);
            EXPECT_EQ(second->stem_direction(), expected);
            EXPECT_EQ(layout_count(master), layouts);
            ASSERT_TRUE(document.page_count());
            if (mode == "auto" || mode == "no") {
                EXPECT_EQ(rest->beam(), nullptr);
                EXPECT_NE(first->beam(), nullptr);
                EXPECT_EQ(first->beam(), second->beam());
            } else {
                ASSERT_NE(rest->beam(), nullptr);
                EXPECT_EQ(first->beam(), rest->beam());
                EXPECT_EQ(second->beam(), rest->beam());
            }
        }
    }
}

TEST(Read, S16GraceBeamValidationIncludesPrecedingMembers) {
    using namespace mu::engraving;
    for (bool after : {false, true}) {
        for (int anchor_index : {0, 1}) {
            for (bool warm : {false, true}) {
                SCOPED_TRACE(after);
                SCOPED_TRACE(anchor_index);
                SCOPED_TRACE(warm);
                auto xml = cross_staff_grace_beam_fixture(after, anchor_index);
                auto opened = g_engine->open(as_bytes(xml));
                ASSERT_TRUE(opened);
                auto &document = **opened;
                auto &master = master_of(document);
                auto *main = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
                const auto notes = after ? main->grace_notes_after() : main->grace_notes_before();
                ASSERT_EQ(notes.size(), 3u);
                ASSERT_EQ(notes.front()->staff_move(), 1);
                auto *beam = notes[anchor_index]->authored_beam();
                ASSERT_NE(beam, nullptr);
                if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                const auto layouts = layout_count(master);
                const auto id = engrave::from_engine(beam->eid());
                ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "crossStaffMove", int64_t{-1}}}}));
                EXPECT_EQ(beam->cross_staff_move(), -1);
                ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "crossStaffMove", int64_t{-2}}}}));
                EXPECT_EQ(beam->cross_staff_move(), -1);
                ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "StemDirection", engrave::Enum{"down"}}}}));
                EXPECT_EQ(beam->direction(), DirectionV::AUTO);
                for (auto *note : notes) EXPECT_EQ(note->stem_direction(), DirectionV::AUTO);
                EXPECT_EQ(layout_count(master), layouts);
            }
        }
    }
}

TEST(Read, S16GraceAnchorRespectsBeamBoundaries) {
    using namespace mu::engraving;
    for (bool after : {false, true}) {
        for (const std::string boundary : {"begin", "no", "quarter", "end"}) {
            for (bool warm : {false, true}) {
                SCOPED_TRACE(after);
                SCOPED_TRACE(boundary);
                SCOPED_TRACE(warm);
                auto xml = cross_staff_grace_beam_fixture(after, 1);
                const auto anchor = xml.find("<Chord>", xml.find("<Beam>"));
                if (boundary == "quarter")
                    xml.replace(xml.find("<durationType>eighth</durationType>", anchor), 35, "<durationType>quarter</durationType>");
                else
                    xml.insert(anchor + 7, "<BeamMode>" + boundary + "</BeamMode>");
                auto opened = g_engine->open(as_bytes(xml));
                ASSERT_TRUE(opened);
                auto &document = **opened;
                auto &master = master_of(document);
                auto *main = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
                const auto notes = after ? main->grace_notes_after() : main->grace_notes_before();
                ASSERT_EQ(notes.size(), 3u);
                auto *beam = notes[1]->authored_beam();
                ASSERT_NE(beam, nullptr);
                if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                const auto layouts = layout_count(master);
                const auto id = engrave::from_engine(beam->eid());
                ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "crossStaffMove", int64_t{-1}},
                                            engrave::SetProperty{id, "StemDirection", engrave::Enum{"down"}}}}));
                const bool cross = boundary == "end";
                EXPECT_EQ(beam->cross_staff_move(), cross ? -1 : 0);
                EXPECT_EQ(beam->direction(), cross ? DirectionV::AUTO : DirectionV::DOWN);
                EXPECT_EQ(notes[0]->stem_direction(), DirectionV::AUTO);
                EXPECT_EQ(notes[1]->stem_direction(), cross ? DirectionV::AUTO : DirectionV::DOWN);
                EXPECT_EQ(notes[2]->stem_direction(), boundary == "begin" ? DirectionV::DOWN : DirectionV::AUTO);
                EXPECT_EQ(layout_count(master), layouts);
            }
        }
    }
}

TEST(Read, S17LayoutBreakReplacementRemovesConflictingElements) {
    using namespace mu::engraving;
    const std::array<std::string, 4> kinds{"line", "page", "section", "nobreak"};
    for (const auto &old : kinds) {
        for (const auto &replacement : kinds) {
            for (bool warm : {false, true}) {
                SCOPED_TRACE(old + " -> " + replacement);
                SCOPED_TRACE(warm);
                auto xml = legacy_segment_fixture("");
                xml.insert(xml.find("</Measure>"), "<LayoutBreak><subtype>" + old + "</subtype></LayoutBreak>");
                auto opened = g_engine->open(as_bytes(xml));
                ASSERT_TRUE(opened);
                auto &document = **opened;
                auto &master = master_of(document);
                auto *measure = master.first_measure();
                if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                const auto layouts = layout_count(master);
                ASSERT_TRUE(document.edit({{engrave::SetLayoutBreak{engrave::from_engine(measure->eid()), replacement, true}}}));
                EXPECT_EQ(layout_count(master), layouts);
                const bool coexist = (old == "page" && replacement == "section") || (old == "section" && replacement == "page");
                const auto check = [&](bool on) {
                    std::array<int, 4> counts{};
                    for (auto *item : measure->el()) {
                        if (!item->is_layout_break()) continue;
                        const auto *br = to_layout_break(item);
                        if (br->is_line_break()) ++counts[0];
                        if (br->is_page_break()) ++counts[1];
                        if (br->is_section_break()) ++counts[2];
                        if (br->is_no_break()) ++counts[3];
                    }
                    const std::array<bool, 4> flags{measure->line_break(), measure->page_break(), measure->section_break(),
                                                    measure->no_break()};
                    for (size_t i = 0; i < kinds.size(); ++i) {
                        const bool expected = (on && kinds[i] == replacement) || (coexist && kinds[i] == old);
                        EXPECT_EQ(counts[i], expected ? 1 : 0) << kinds[i];
                        EXPECT_EQ(flags[i], expected) << kinds[i];
                    }
                };
                check(true);
                ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                check(true);
                ASSERT_TRUE(document.edit({{engrave::SetLayoutBreak{engrave::from_engine(measure->eid()), replacement, false}}}));
                check(false);
                ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                check(false);
            }
        }
    }
}

TEST(Read, S17SectionReplacementInvalidatesRepeatTraversal) {
    using namespace mu::engraving;
    const std::string xml = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel/></Instrument></Part>
<Staff id="1"><Measure len="1/4"><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>quarter</durationType></Rest></voice><LayoutBreak><subtype>section</subtype></LayoutBreak></Measure>
<Measure len="1/4"><endRepeat>2</endRepeat><voice><Rest><durationType>quarter</durationType></Rest></voice></Measure>
</Staff></Score></museScore>)";
    for (bool warm : {false, true}) {
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        master.update_repeat_list(false);
        // The first quarter plays once, the second repeats within its section.
        EXPECT_EQ(master.repeat_list(true).ticks(), 1440);
        const auto layouts = layout_count(master);
        const auto id = engrave::from_engine(master.first_measure()->eid());
        ASSERT_TRUE(document.edit({{engrave::SetLayoutBreak{id, "nobreak", true}}}));
        master.update_repeat_list(false);
        EXPECT_EQ(master.repeat_list(true).ticks(), 1920);
        EXPECT_EQ(layout_count(master), layouts);
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        EXPECT_EQ(master.repeat_list(true).ticks(), 1920);
        ASSERT_TRUE(document.edit({{engrave::SetLayoutBreak{id, "section", true}}}));
        master.update_repeat_list(false);
        EXPECT_EQ(master.repeat_list(true).ticks(), 1440);
    }
}

TEST(Read, S18ImportBreakConflictsUseIncomingOrderWithoutLayout) {
    using enum mu::engraving::LayoutBreakType;

    struct Case {
        std::vector<std::string> incoming;
        std::vector<mu::engraving::LayoutBreakType> expected;
    };

    const std::vector<Case> cases{
        {{"section", "nobreak"}, {NOBREAK}},
        {{"nobreak", "section"}, {SECTION}},
        {{"line", "page"}, {PAGE}},
        {{"page", "line"}, {LINE}},
        {{"line", "section"}, {SECTION}},
        {{"section", "line"}, {LINE}},
        {{"page", "nobreak"}, {NOBREAK}},
        {{"nobreak", "page"}, {PAGE}},
        {{"line", "nobreak"}, {NOBREAK}},
        {{"nobreak", "line"}, {LINE}},
        {{"page", "section"}, {PAGE, SECTION}},
        {{"section", "page"}, {SECTION, PAGE}},
        {{"line", "page", "section"}, {PAGE, SECTION}},
        {{"page", "line", "section"}, {SECTION}},
        {{"page", "section", "nobreak"}, {NOBREAK}},
    };
    for (const std::string version : {"4.60", "5.00"}) {
        for (const auto &spec : cases) {
            SCOPED_TRACE(version);
            std::string breaks;
            for (const auto &type : spec.incoming) breaks += s18_break_xml(type);
            SCOPED_TRACE(breaks);
            auto xml = s18_break_fixture(breaks);
            xml.replace(xml.find("5.00"), 4, version);
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &master = master_of(**opened);
            expect_s18_breaks(master.first_measure(), spec.expected);
            EXPECT_EQ(layout_count(master), 0u);
        }
    }
}

TEST(Read, S18BreakSubtypeEditsRemoveSiblingsAndRepeatBoundary) {
    using namespace mu::engraving;
    for (const std::string selected : {"page", "section"}) {
        for (const std::string replacement : {"nobreak", "line"}) {
            for (bool warm : {false, true}) {
                SCOPED_TRACE(selected + " -> " + replacement);
                SCOPED_TRACE(warm);
                auto opened = g_engine->open(as_bytes(s18_break_fixture(s18_break_xml("section") + s18_break_xml("page"))));
                ASSERT_TRUE(opened);
                auto &document = **opened;
                auto &master = master_of(document);
                auto *measure = master.first_measure();
                LayoutBreak *target = nullptr;
                LayoutBreak *sibling = nullptr;
                for (auto *item : measure->el()) {
                    if (!item->is_layout_break()) continue;
                    auto *br = to_layout_break(item);
                    if (br->is_page_break() == (selected == "page"))
                        target = br;
                    else
                        sibling = br;
                }
                ASSERT_NE(target, nullptr);
                ASSERT_NE(sibling, nullptr);
                const auto target_id = engrave::from_engine(target->eid());
                const auto sibling_id = engrave::from_engine(sibling->eid());
                if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                master.update_repeat_list(false);
                // Without a section boundary the end repeat includes both quarters.
                EXPECT_EQ(master.repeat_list(true).ticks(), 1440);
                const auto layouts = layout_count(master);
                const auto result = document.edit({{engrave::SetProperty{target_id, "LAYOUT_BREAK", engrave::Enum{replacement}}}});
                ASSERT_TRUE(result);
                ASSERT_FALSE(result->rejection);
                EXPECT_FALSE(engrave::resolve(master, sibling_id));
                EXPECT_TRUE(engrave::resolve(master, target_id));
                const auto expected = replacement == "line" ? LayoutBreakType::LINE : LayoutBreakType::NOBREAK;
                expect_s18_breaks(measure, {expected});
                EXPECT_EQ(layout_count(master), layouts);
                master.update_repeat_list(false);
                EXPECT_EQ(master.repeat_list(true).ticks(), 1920);
                for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
                    auto saved = document.export_score({.format = format});
                    ASSERT_TRUE(saved);
                    auto reopened = g_engine->open(*saved, {.format = format});
                    ASSERT_TRUE(reopened);
                    auto &copy = master_of(**reopened);
                    expect_s18_breaks(copy.first_measure(), {expected});
                    copy.update_repeat_list(false);
                    EXPECT_EQ(copy.repeat_list(true).ticks(), 1920);
                    ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
                    expect_s18_breaks(copy.first_measure(), {expected});
                }
                ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                expect_s18_breaks(measure, {expected});
            }
        }
    }
}

TEST(Read, S18BreakSubtypeBatchFailurePreservesTheBatchContract) {
    using namespace mu::engraving;
    for (bool warm : {false, true}) {
        SCOPED_TRACE(warm);
        auto opened = g_engine->open(as_bytes(s18_break_fixture(s18_break_xml("section") + s18_break_xml("page"))));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *measure = master.first_measure();
        const auto section = engrave::from_engine(measure->section_break_element()->eid());
        LayoutBreak *page = nullptr;
        for (auto *item : measure->el())
            if (item->is_layout_break() && to_layout_break(item)->is_page_break()) page = to_layout_break(item);
        ASSERT_NE(page, nullptr);
        const auto page_id = engrave::from_engine(page->eid());
        if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        const auto layouts = layout_count(master);
        const auto preflight = document.edit({{engrave::SetProperty{page_id, "LAYOUT_BREAK", engrave::Enum{"line"}},
                                               engrave::SetProperty{section, "unknown-property", int64_t{1}}}});
        ASSERT_FALSE(preflight);
        EXPECT_FALSE(preflight.error().poisoned);
        expect_s18_breaks(measure, {LayoutBreakType::SECTION, LayoutBreakType::PAGE});
        const auto prefix =
            document.edit({{engrave::SetProperty{page_id, "LAYOUT_BREAK", engrave::Enum{"line"}}, engrave::Remove{section}}});
        ASSERT_TRUE(prefix);
        ASSERT_TRUE(prefix->rejection);
        EXPECT_EQ(prefix->applied, 1u);
        EXPECT_EQ(prefix->rejection->op_index, 1u);
        EXPECT_FALSE(prefix->rejection->error.poisoned);
        expect_s18_breaks(measure, {LayoutBreakType::LINE});
        EXPECT_FALSE(engrave::resolve(master, section));
        EXPECT_EQ(layout_count(master), layouts);
        ASSERT_TRUE(document.edit({}));
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    }
}

TEST(Read, S18DirectBreakSubtypeEditsRemoveLinkedSiblings) {
    using namespace mu::engraving;
    for (bool active : {false, true}) {
        SCOPED_TRACE(active);
        auto opened = g_engine->open(as_bytes(s18_break_fixture(s18_break_xml("section") + s18_break_xml("page"))));
        ASSERT_TRUE(opened);
        auto &master = master_of(**opened);
        ASSERT_TRUE((*opened)->edit({{engrave::GenerateExcerpts{{engrave::from_engine(master.parts().front()->eid())}}}}));
        ASSERT_EQ(master.excerpts().size(), 1u);
        auto *part = master.excerpts().front()->excerpt_score();
        ASSERT_NE(part, nullptr);
        auto *measure = master.first_measure();
        auto *other = part->first_measure();
        ASSERT_NE(other->section_break_element(), nullptr);
        ASSERT_NE(other->section_break_element()->links(), nullptr);
        auto *page = to_layout_break(measure->el().back());
        // Generated excerpts omit page breaks; attach a linked authored copy.
        auto *page_copy = page->linked_clone();
        page_copy->set_ownership_parent(other);
        other->add(page_copy);
        expect_s18_breaks(other, {LayoutBreakType::SECTION, LayoutBreakType::PAGE});
        if (active) {
            ASSERT_TRUE(
                (*opened)->edit({{engrave::SetProperty{engrave::from_engine(page->eid()), "LAYOUT_BREAK", engrave::Enum{"line"}}}}));
        } else {
            page->set_layout_break_type(LayoutBreakType::LINE);
        }
        expect_s18_breaks(measure, {LayoutBreakType::LINE});
        if (active)
            expect_s18_breaks(other, {LayoutBreakType::LINE});
        else
            expect_s18_breaks(other, {LayoutBreakType::SECTION, LayoutBreakType::PAGE});
        master.update_repeat_list(false);
        EXPECT_EQ(master.repeat_list(true).ticks(), 1920);
        ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_TRUE((*opened)->render({.score = {engrave::from_engine(part->eid())}, .format = engrave::RenderFormat::SVG}));
    }
}

TEST(Read, S19DuplicateSubtypeEditPreservesIdentityAndFollowingRepeat) {
    using namespace mu::engraving;
    for (bool warm : {false, true}) {
        SCOPED_TRACE(warm);
        auto xml = s18_break_fixture(s18_break_xml("section") + s18_break_xml("page"));
        xml.insert(xml.find("</voice>"), "<BarLine><subtype>double</subtype></BarLine>");
        xml.insert(xml.find("<endRepeat>"), "<startRepeat/>");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *measure = master.first_measure();
        auto *section = measure->section_break_element();
        ASSERT_NE(section, nullptr);
        const auto section_id = engrave::from_engine(section->eid());
        const auto page_id = engrave::from_engine(measure->el().back()->eid());
        if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        const auto layouts = layout_count(master);
        const auto edited = document.edit({{engrave::SetProperty{section_id, "LAYOUT_BREAK", engrave::Enum{"page"}}}});
        ASSERT_TRUE(edited);
        ASSERT_FALSE(edited->rejection);
        const auto check = [&](engrave::Document &doc) {
            auto &score = master_of(doc);
            auto *first = score.first_measure();
            expect_s18_breaks(first, {LayoutBreakType::PAGE});
            ASSERT_EQ(first->el().size(), 1u);
            EXPECT_EQ(engrave::from_engine(first->el().front()->eid()), section_id);
            EXPECT_TRUE(doc.get(section_id));
            EXPECT_FALSE(doc.get(page_id));
            EXPECT_TRUE(first->next_measure()->repeat_start());
        };
        ASSERT_TRUE(document.edit(
            {{engrave::SetProperty{engrave::from_engine(measure->end_bar_line()->eid()), "BARLINE_TYPE", engrave::Enum{"end"}}}}));
        check(document);
        EXPECT_EQ(layout_count(master), layouts);
        ASSERT_TRUE(document.checkpoint("single page"));
        ASSERT_TRUE(document.edit({{engrave::Remove{section_id}}}));
        ASSERT_TRUE(document.restore_checkpoint("single page"));
        check(document);
        for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            SCOPED_TRACE(static_cast<int>(format));
            const auto saved = document.export_score({.format = format});
            ASSERT_TRUE(saved);
            auto reopened = g_engine->open(*saved, {.format = format});
            ASSERT_TRUE(reopened);
            check(**reopened);
            ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
            check(**reopened);
        }
    }
}

TEST(Read, S19LocalReplacementPreservesIndependentExcerptBreak) {
    using namespace mu::engraving;
    for (bool measure_break : {false, true}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(measure_break);
            SCOPED_TRACE(warm);
            auto opened = g_engine->open(as_bytes(s18_break_fixture(s18_break_xml("section") + s18_break_xml("page"))));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{engrave::from_engine(master.parts().front()->eid())}}}}));
            ASSERT_EQ(master.excerpts().size(), 1u);
            auto *part = master.excerpts().front()->excerpt_score();
            ASSERT_NE(part, nullptr);
            auto *section = part->first_measure()->section_break_element();
            ASSERT_NE(section, nullptr);
            ASSERT_NE(section->links(), nullptr);
            const auto excerpt_break_id = engrave::from_engine(section->eid());
            const auto excerpt_id = engrave::from_engine(part->eid());
            auto *measure = master.first_measure();
            const auto page_id = engrave::from_engine(measure->el().back()->eid());
            if (warm) {
                ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                ASSERT_TRUE(document.render({.score = {excerpt_id}, .format = engrave::RenderFormat::SVG}));
            }
            const auto layouts = layout_count(master);
            const auto part_layouts = layout_count(*part);
            const auto independent = document.edit({{engrave::SetProperty{excerpt_break_id, "LAYOUT_BREAK", engrave::Enum{"page"}}}});
            ASSERT_TRUE(independent);
            ASSERT_FALSE(independent->rejection);
            expect_s18_breaks(measure, {LayoutBreakType::SECTION, LayoutBreakType::PAGE});
            expect_s18_breaks(part->first_measure(), {LayoutBreakType::PAGE});
            const auto replaced = measure_break ?
                                      document.edit({{engrave::SetLayoutBreak{engrave::from_engine(measure->eid()), "line", true}}}) :
                                      document.edit({{engrave::SetProperty{page_id, "LAYOUT_BREAK", engrave::Enum{"line"}}}});
            ASSERT_TRUE(replaced);
            ASSERT_FALSE(replaced->rejection);
            const auto check = [&](engrave::Document &doc) {
                auto &score = master_of(doc);
                expect_s18_breaks(score.first_measure(), {LayoutBreakType::LINE});
                if (!measure_break) EXPECT_TRUE(doc.get(page_id));
                ASSERT_EQ(score.excerpts().size(), 1u);
                auto *excerpt = score.excerpts().front()->excerpt_score();
                ASSERT_NE(excerpt, nullptr);
                expect_s18_breaks(excerpt->first_measure(), {LayoutBreakType::PAGE});
                ASSERT_EQ(excerpt->first_measure()->el().size(), 1u);
                EXPECT_EQ(engrave::from_engine(excerpt->first_measure()->el().front()->eid()), excerpt_break_id);
                EXPECT_TRUE(doc.get(excerpt_break_id));
            };
            check(document);
            EXPECT_EQ(layout_count(master), layouts);
            EXPECT_EQ(layout_count(*part), part_layouts);
            ASSERT_TRUE(document.checkpoint("independent excerpt"));
            ASSERT_TRUE(document.restore_checkpoint("independent excerpt"));
            check(document);
            for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
                SCOPED_TRACE(static_cast<int>(format));
                const auto saved = document.export_score({.format = format});
                ASSERT_TRUE(saved);
                auto reopened = g_engine->open(*saved, {.format = format});
                ASSERT_TRUE(reopened);
                check(**reopened);
                ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
                const auto restored_excerpt = engrave::from_engine(master_of(**reopened).excerpts().front()->excerpt_score()->eid());
                ASSERT_TRUE((*reopened)->render({.score = {restored_excerpt}, .format = engrave::RenderFormat::SVG}));
                check(**reopened);
            }
        }
    }
}

TEST(Read, S20MasterSectionTogglePreservesIndependentExcerptPage) {
    using namespace mu::engraving;
    for (bool warm : {false, true}) {
        SCOPED_TRACE(warm);
        auto opened = g_engine->open(as_bytes(s18_break_fixture(s18_break_xml("section") + s18_break_xml("page"))));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{engrave::from_engine(master.parts().front()->eid())}}}}));
        auto *part = master.excerpts().front()->excerpt_score();
        auto *section = part->first_measure()->section_break_element();
        ASSERT_NE(section, nullptr);
        const auto excerpt_break_id = engrave::from_engine(section->eid());
        const auto master_section_id = engrave::from_engine(master.first_measure()->section_break_element()->eid());
        const auto master_page_id = engrave::from_engine(master.first_measure()->el().back()->eid());
        if (warm) ASSERT_NO_FATAL_FAILURE(render_s20_scores(document));
        const auto independent = document.edit({{engrave::SetProperty{excerpt_break_id, "LAYOUT_BREAK", engrave::Enum{"page"}}}});
        ASSERT_TRUE(independent);
        ASSERT_FALSE(independent->rejection);
        EXPECT_FALSE(section->is_property_linked_to_master(Pid::LAYOUT_BREAK));
        expect_s18_breaks(master.first_measure(), {LayoutBreakType::SECTION, LayoutBreakType::PAGE});
        expect_s18_breaks(part->first_measure(), {LayoutBreakType::PAGE});
        const auto removed =
            document.edit({{engrave::SetLayoutBreak{engrave::from_engine(master.first_measure()->eid()), "section", false}}});
        ASSERT_TRUE(removed);
        ASSERT_FALSE(removed->rejection);
        const auto check = [&](engrave::Document &doc) {
            auto &score = master_of(doc);
            expect_s18_breaks(score.first_measure(), {LayoutBreakType::PAGE});
            EXPECT_FALSE(doc.get(master_section_id));
            EXPECT_TRUE(doc.get(master_page_id));
            ASSERT_EQ(score.excerpts().size(), 1u);
            auto *measure = score.excerpts().front()->excerpt_score()->first_measure();
            expect_s18_breaks(measure, {LayoutBreakType::PAGE});
            ASSERT_EQ(measure->el().size(), 1u);
            EXPECT_EQ(engrave::from_engine(measure->el().front()->eid()), excerpt_break_id);
            EXPECT_TRUE(doc.get(excerpt_break_id));
        };
        ASSERT_NO_FATAL_FAILURE(check_s20_roundtrips(document, check));
    }
}

TEST(Read, S20MasterSectionReadditionRetainsSingleExcerptSection) {
    using namespace mu::engraving;
    for (bool measure_break : {false, true}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(measure_break);
            SCOPED_TRACE(warm);
            auto opened = g_engine->open(as_bytes(s18_break_fixture(s18_break_xml("section") + s18_break_xml("page"))));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{engrave::from_engine(master.parts().front()->eid())}}}}));
            auto *part = master.excerpts().front()->excerpt_score();
            const auto excerpt_break_id = engrave::from_engine(part->first_measure()->section_break_element()->eid());
            const auto measure_id = engrave::from_engine(master.first_measure()->eid());
            const auto page_id = engrave::from_engine(master.first_measure()->el().back()->eid());
            if (warm) ASSERT_NO_FATAL_FAILURE(render_s20_scores(document));
            const auto replaced = measure_break ? document.edit({{engrave::SetLayoutBreak{measure_id, "line", true}}}) :
                                                  document.edit({{engrave::SetProperty{page_id, "LAYOUT_BREAK", engrave::Enum{"line"}}}});
            ASSERT_TRUE(replaced);
            ASSERT_FALSE(replaced->rejection);
            expect_s18_breaks(master.first_measure(), {LayoutBreakType::LINE});
            expect_s18_breaks(part->first_measure(), {LayoutBreakType::SECTION});
            const auto added = document.edit({{engrave::SetLayoutBreak{measure_id, "section", true}}});
            ASSERT_TRUE(added);
            ASSERT_FALSE(added->rejection);
            const auto master_break_id = engrave::from_engine(master.first_measure()->section_break_element()->eid());
            const auto check = [&](engrave::Document &doc) {
                auto &score = master_of(doc);
                expect_s18_breaks(score.first_measure(), {LayoutBreakType::SECTION});
                EXPECT_TRUE(doc.get(master_break_id));
                EXPECT_FALSE(doc.get(page_id));
                ASSERT_EQ(score.excerpts().size(), 1u);
                auto *measure = score.excerpts().front()->excerpt_score()->first_measure();
                expect_s18_breaks(measure, {LayoutBreakType::SECTION});
                ASSERT_EQ(measure->el().size(), 1u);
                EXPECT_EQ(engrave::from_engine(measure->el().front()->eid()), excerpt_break_id);
                EXPECT_TRUE(doc.get(excerpt_break_id));
            };
            ASSERT_NO_FATAL_FAILURE(check_s20_roundtrips(document, check));
        }
    }
}

TEST(Read, S20MasterSectionAdditionDisplacesExcerptConflictsAndKeepsPage) {
    using namespace mu::engraving;
    for (const std::string type : {"line", "nobreak", "page"}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(type);
            SCOPED_TRACE(warm);
            auto opened = g_engine->open(as_bytes(s18_break_fixture("")));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{engrave::from_engine(master.parts().front()->eid())}}}}));
            auto *part = master.excerpts().front()->excerpt_score();
            if (warm) ASSERT_NO_FATAL_FAILURE(render_s20_scores(document));
            const auto independent =
                document.edit({{engrave::SetLayoutBreak{engrave::from_engine(part->first_measure()->eid()), type, true}}});
            ASSERT_TRUE(independent);
            ASSERT_FALSE(independent->rejection);
            ASSERT_EQ(part->first_measure()->el().size(), 1u);
            const auto excerpt_break_id = engrave::from_engine(part->first_measure()->el().front()->eid());
            expect_s18_breaks(master.first_measure(), {});
            const auto added =
                document.edit({{engrave::SetLayoutBreak{engrave::from_engine(master.first_measure()->eid()), "section", true}}});
            ASSERT_TRUE(added);
            ASSERT_FALSE(added->rejection);
            auto *section = part->first_measure()->section_break_element();
            ASSERT_NE(section, nullptr);
            const auto excerpt_section_id = engrave::from_engine(section->eid());
            const auto check = [&](engrave::Document &doc) {
                auto &score = master_of(doc);
                expect_s18_breaks(score.first_measure(), {LayoutBreakType::SECTION});
                ASSERT_EQ(score.excerpts().size(), 1u);
                auto *measure = score.excerpts().front()->excerpt_score()->first_measure();
                if (type == "page") {
                    expect_s18_breaks(measure, {LayoutBreakType::PAGE, LayoutBreakType::SECTION});
                    ASSERT_EQ(measure->el().size(), 2u);
                    EXPECT_EQ(engrave::from_engine(measure->el().front()->eid()), excerpt_break_id);
                    EXPECT_TRUE(doc.get(excerpt_break_id));
                } else {
                    expect_s18_breaks(measure, {LayoutBreakType::SECTION});
                    EXPECT_FALSE(doc.get(excerpt_break_id));
                }
                ASSERT_NE(measure->section_break_element(), nullptr);
                EXPECT_EQ(engrave::from_engine(measure->section_break_element()->eid()), excerpt_section_id);
                EXPECT_TRUE(doc.get(excerpt_section_id));
            };
            ASSERT_NO_FATAL_FAILURE(check_s20_roundtrips(document, check));
        }
    }
}

TEST(Read, S19DuplicateImportAndCleanupKeepTheIncomingBreak) {
    using namespace mu::engraving;
    const std::vector<std::pair<std::string, LayoutBreakType>> cases{{"page", LayoutBreakType::PAGE},
                                                                     {"section", LayoutBreakType::SECTION},
                                                                     {"line", LayoutBreakType::LINE},
                                                                     {"nobreak", LayoutBreakType::NOBREAK}};
    for (const auto &[type, subtype] : cases) {
        for (const std::string version : {"4.60", "5.00"}) {
            SCOPED_TRACE(type);
            SCOPED_TRACE(version);
            auto xml = s18_break_fixture("<LayoutBreak><eid>A_A</eid><subtype>" + type + "</subtype></LayoutBreak>" +
                                         "<LayoutBreak><eid>B_B</eid><subtype>" + type + "</subtype></LayoutBreak>");
            xml.replace(xml.find("5.00"), 4, version);
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &master = master_of(**opened);
            auto *measure = master.first_measure();
            ASSERT_EQ(measure->el().size(), 1u);
            EXPECT_EQ(measure->el().front()->eid().to_std_string(), "B_B");
            EXPECT_FALSE((*opened)->get(*engrave::element_id_from_string("A_A")));
            EXPECT_TRUE((*opened)->get(*engrave::element_id_from_string("B_B")));
            expect_s18_breaks(measure, {subtype});
            for (bool undo : {false, true}) {
                SCOPED_TRACE(undo);
                auto *incoming = Factory::create_layout_break(measure);
                incoming->set_layout_break_type(subtype);
                incoming->set_track(0);
                measure->add(incoming);
                measure->cleanup_layout_breaks(undo);
                expect_s18_breaks(measure, {subtype});
                ASSERT_EQ(measure->el().size(), 1u);
                EXPECT_EQ(measure->el().front(), incoming);
            }
            EXPECT_EQ(layout_count(master), 0u);
        }
    }
}

TEST(Read, S17GraceBeamEditsRespectSeparateAuthoredOwnership) {
    using namespace mu::engraving;
    for (bool after : {false, true}) {
        for (bool warm : {false, true}) {
            std::vector<std::byte> unslashed_svg;
            for (bool slashed : {false, true}) {
                SCOPED_TRACE(slashed);
                SCOPED_TRACE(after);
                SCOPED_TRACE(warm);
                auto xml = cross_staff_grace_beam_fixture(after, 1);
                if (!slashed) {
                    for (size_t pos; (pos = xml.find("<acciaccatura/>")) != std::string::npos;) xml.replace(pos, 15, "<appoggiatura/>");
                }
                if (after && slashed) {
                    for (size_t pos = 0; (pos = xml.find("<grace8after/>", pos)) != std::string::npos;) {
                        xml.insert(pos + 14, "<showStemSlash>1</showStemSlash>");
                        pos += 44;
                    }
                }
                const auto first_chord = xml.find("<Chord>");
                const auto next_chord = xml.find("<Chord>", first_chord + 7);
                xml.insert(after ? xml.find("<Chord>", next_chord + 7) : first_chord, "<Beam><eid>G_G</eid></Beam>");
                auto opened = g_engine->open(as_bytes(xml));
                ASSERT_TRUE(opened);
                auto &document = **opened;
                auto &master = master_of(document);
                auto *main = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
                const auto notes = after ? main->grace_notes_after() : main->grace_notes_before();
                ASSERT_EQ(notes.size(), 3u);
                ASSERT_NE(notes[0]->authored_beam(), nullptr);
                auto *beam = notes[1]->authored_beam();
                ASSERT_NE(beam, nullptr);
                ASSERT_NE(beam, notes[0]->authored_beam());
                const std::vector<ChordRest *> expected{notes[1], notes[2]};
                EXPECT_EQ(beam->source_elements(), expected);
                const std::vector<ChordRest *> rendered_expected{notes[0], notes[1], notes[2]};
                const auto check_slashes = [&]() {
                    auto *rendered_beam = notes[1]->beam();
                    ASSERT_NE(rendered_beam, nullptr);
                    EXPECT_EQ(rendered_beam, notes[0]->beam());
                    EXPECT_EQ(rendered_beam, notes[2]->beam());
                    EXPECT_EQ(rendered_beam->rendered_elements(), rendered_expected);
                    EXPECT_EQ(notes[0]->stem_slash() != nullptr, slashed);
                    EXPECT_EQ(notes[1]->stem_slash(), nullptr);
                    EXPECT_EQ(notes[2]->stem_slash(), nullptr);
                    if (slashed) {
                        ASSERT_NE(notes[0]->stem_slash(), nullptr);
                        EXPECT_FALSE(notes[0]->stem_slash()->ldata()->bbox().is_empty());
                    }
                };
                if (warm) {
                    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                    check_slashes();
                }
                const auto layouts = layout_count(master);
                const auto id = engrave::from_engine(beam->eid());
                ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "crossStaffMove", int64_t{-1}},
                                            engrave::SetProperty{id, "StemDirection", engrave::Enum{"down"}}}}));
                EXPECT_EQ(beam->cross_staff_move(), 0);
                EXPECT_EQ(beam->direction(), DirectionV::DOWN);
                EXPECT_EQ(notes[0]->stem_direction(), DirectionV::AUTO);
                EXPECT_EQ(notes[1]->stem_direction(), DirectionV::DOWN);
                EXPECT_EQ(notes[2]->stem_direction(), DirectionV::DOWN);
                EXPECT_EQ(layout_count(master), layouts);
                const auto rendered = document.render({.format = engrave::RenderFormat::SVG});
                ASSERT_TRUE(rendered);
                ASSERT_EQ(rendered->pages.size(), 1u);
                EXPECT_EQ(beam->source_elements(), expected);
                EXPECT_EQ(notes[0]->stem_direction(), DirectionV::AUTO);
                EXPECT_EQ(notes[1]->hook(), nullptr);
                EXPECT_EQ(notes[2]->hook(), nullptr);
                check_slashes();
                if (!slashed) {
                    unslashed_svg = rendered->pages.front();
                } else {
                    EXPECT_NE(rendered->pages.front(), unslashed_svg);
                }
            }
        }
    }
}

TEST(Read, S18AuthoredBeamOwnersPartitionUnanchoredMembers) {
    using namespace mu::engraving;
    for (int grace : {-1, 0, 1}) {
        for (bool cross_staff : {false, true}) {
            for (bool warm : {false, true}) {
                SCOPED_TRACE(grace);
                SCOPED_TRACE(cross_staff);
                SCOPED_TRACE(warm);
                std::string voice;
                for (int i = 0; i < 4; ++i) {
                    const int index = grace == 1 ? 3 - i : i;
                    if (index == 0) voice += "<Beam><eid>F_F</eid></Beam>";
                    if (index == 2) voice += "<Beam><eid>G_G</eid></Beam>";
                    voice += "<Chord>";
                    if (grace != 0) voice += grace == -1 ? "<appoggiatura/>" : "<grace8after/>";
                    voice += "<durationType>eighth</durationType><BeamMode>";
                    voice += index == 0 ? "begin" : "mid";
                    voice += "</BeamMode>";
                    if (cross_staff && index == 0) voice += "<staffMove>1</staffMove>";
                    voice += "<Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>";
                }
                if (grace != 0) voice += "<Chord><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>";
                auto opened = g_engine->open(as_bytes(s17_beam_fixture(voice)));
                ASSERT_TRUE(opened);
                auto &document = **opened;
                auto &master = master_of(document);
                auto *measure = master.first_measure();
                std::vector<ChordRest *> notes;
                if (grace == 0) {
                    for (auto *seg = measure->first(SegmentType::CHORD_REST); seg; seg = seg->next(SegmentType::CHORD_REST))
                        if (auto *cr = to_chord_rest(seg->element(0)); cr && cr->is_chord()) notes.push_back(cr);
                } else {
                    auto *main = to_chord(measure->first(SegmentType::CHORD_REST)->element(0));
                    const auto graces = grace == -1 ? main->grace_notes_before() : main->grace_notes_after();
                    for (auto *cr : graces) notes.push_back(cr);
                }
                ASSERT_EQ(notes.size(), 4u);
                auto *first = notes[0]->authored_beam();
                auto *second = notes[2]->authored_beam();
                ASSERT_NE(first, nullptr);
                ASSERT_NE(second, nullptr);
                ASSERT_NE(first, second);
                const std::vector<ChordRest *> first_members{notes[0], notes[1]};
                const std::vector<ChordRest *> second_members{notes[2], notes[3]};
                const auto check = [&] {
                    EXPECT_EQ(first->source_elements(), first_members);
                    EXPECT_EQ(second->source_elements(), second_members);
                };
                check();
                EXPECT_EQ(layout_count(master), 0u);
                if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                check();
                const auto layouts = layout_count(master);
                ASSERT_TRUE(
                    document.edit({{engrave::SetProperty{engrave::from_engine(second->eid()), "StemDirection", engrave::Enum{"down"}}}}));
                EXPECT_EQ(to_chord(notes[0])->stem_direction(), DirectionV::AUTO);
                EXPECT_EQ(to_chord(notes[1])->stem_direction(), DirectionV::AUTO);
                ASSERT_TRUE(
                    document.edit({{engrave::SetProperty{engrave::from_engine(first->eid()), "StemDirection", engrave::Enum{"up"}}}}));
                EXPECT_EQ(to_chord(notes[0])->stem_direction(), cross_staff ? DirectionV::AUTO : DirectionV::UP);
                EXPECT_EQ(to_chord(notes[1])->stem_direction(), cross_staff ? DirectionV::AUTO : DirectionV::UP);
                EXPECT_EQ(to_chord(notes[2])->stem_direction(), DirectionV::DOWN);
                EXPECT_EQ(to_chord(notes[3])->stem_direction(), DirectionV::DOWN);
                EXPECT_EQ(layout_count(master), layouts);
                ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                check();
                EXPECT_EQ(to_chord(notes[1])->stem_direction(), cross_staff ? DirectionV::AUTO : DirectionV::UP);
                EXPECT_EQ(to_chord(notes[2])->stem_direction(), DirectionV::DOWN);
            }
        }
    }
}

TEST(Read, S17SourceBeamGroupsMatchRenderedTransitions) {
    using namespace mu::engraving;

    struct Case {
        std::array<std::string, 3> modes;
        std::array<int, 3> groups;
    };

    const std::array<Case, 8> cases{{
        {{{"auto", "mid", "mid"}}, {0, 0, 0}},
        {{{"begin", "end", "mid"}}, {0, 0, 0}},
        {{{"begin", "mid", "end"}}, {0, 0, 0}},
        {{{"end", "mid", "mid"}}, {0, 0, 0}},
        {{{"begin", "begin", "mid"}}, {0, 1, 1}},
        {{{"begin", "no", "mid"}}, {0, 1, 2}},
        {{{"begin", "mid", "begin"}}, {0, 0, 1}},
        {{{"begin", "begin16", "begin32"}}, {0, 0, 0}},
    }};
    for (const auto &spec : cases) {
        for (int grace : {-1, 0, 1}) {
            for (int anchor = 0; anchor < 3; ++anchor) {
                for (bool warm : {false, true}) {
                    SCOPED_TRACE(spec.modes[0] + "/" + spec.modes[1] + "/" + spec.modes[2]);
                    SCOPED_TRACE(grace);
                    SCOPED_TRACE(anchor);
                    SCOPED_TRACE(warm);
                    std::string voice;
                    for (int i = 0; i < 3; ++i) {
                        const int index = grace == 1 ? 2 - i : i;
                        if (index == anchor) voice += "<Beam><eid>F_F</eid></Beam>";
                        voice += "<Chord>";
                        if (grace != 0) voice += grace == -1 ? "<appoggiatura/>" : "<grace8after/>";
                        voice += "<durationType>eighth</durationType><BeamMode>" + spec.modes[index] +
                                 "</BeamMode><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>";
                    }
                    if (grace != 0)
                        voice += "<Chord><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>";
                    auto opened = g_engine->open(as_bytes(s17_beam_fixture(voice)));
                    ASSERT_TRUE(opened);
                    auto &document = **opened;
                    auto &master = master_of(document);
                    auto *measure = master.first_measure();
                    std::vector<ChordRest *> notes;
                    if (grace == 0) {
                        for (auto *seg = measure->first(SegmentType::CHORD_REST); seg; seg = seg->next(SegmentType::CHORD_REST))
                            if (auto *cr = to_chord_rest(seg->element(0)); cr && cr->is_chord()) notes.push_back(cr);
                    } else {
                        auto *main = to_chord(measure->first(SegmentType::CHORD_REST)->element(0));
                        const auto grace_notes = grace == -1 ? main->grace_notes_before() : main->grace_notes_after();
                        for (auto *cr : grace_notes) notes.push_back(cr);
                    }
                    ASSERT_EQ(notes.size(), 3u);
                    std::vector<ChordRest *> expected;
                    for (int i = 0; i < 3; ++i)
                        if (spec.groups[i] == spec.groups[anchor]) expected.push_back(notes[i]);
                    auto *beam = notes[anchor]->authored_beam();
                    ASSERT_NE(beam, nullptr);
                    EXPECT_EQ(beam->source_elements(), expected);
                    EXPECT_EQ(layout_count(master), 0u);
                    if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                    const auto layouts = layout_count(master);
                    ASSERT_TRUE(
                        document.edit({{engrave::SetProperty{engrave::from_engine(beam->eid()), "StemDirection", engrave::Enum{"down"}}}}));
                    for (int i = 0; i < 3; ++i)
                        EXPECT_EQ(to_chord(notes[i])->stem_direction(),
                                  spec.groups[i] == spec.groups[anchor] ? DirectionV::DOWN : DirectionV::AUTO);
                    EXPECT_EQ(layout_count(master), layouts);
                    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                    EXPECT_EQ(beam->source_elements(), expected);
                    if (expected.size() > 1) {
                        ASSERT_NE(notes[anchor]->beam(), nullptr);
                        EXPECT_EQ(notes[anchor]->beam()->rendered_elements(), expected);
                    } else {
                        EXPECT_EQ(notes[anchor]->beam(), nullptr);
                    }
                }
            }
        }
    }
}

TEST(Read, S17MiddleBeamAnchorIncludesPrecedingCrossStaffChord) {
    using namespace mu::engraving;
    for (bool warm : {false, true}) {
        auto opened = g_engine->open(as_bytes(s17_beam_fixture(R"(
<Chord><durationType>eighth</durationType><staffMove>1</staffMove><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Beam><eid>F_F</eid></Beam><Chord><durationType>eighth</durationType><BeamMode>mid</BeamMode><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><BeamMode>mid</BeamMode><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>)")));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *segment = master.first_measure()->first(SegmentType::CHORD_REST);
        auto *first = to_chord(segment->element(0));
        auto *second = to_chord(segment->next(SegmentType::CHORD_REST)->element(0));
        auto *third = to_chord(second->segment()->next(SegmentType::CHORD_REST)->element(0));
        auto *beam = second->authored_beam();
        ASSERT_NE(beam, nullptr);
        const std::vector<ChordRest *> expected{first, second, third};
        EXPECT_EQ(beam->source_elements(), expected);
        EXPECT_FALSE(first->has_layout_data());
        if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        const auto layouts = layout_count(master);
        const auto id = engrave::from_engine(beam->eid());
        ASSERT_TRUE(document.edit(
            {{engrave::SetProperty{id, "crossStaffMove", int64_t{-1}}, engrave::SetProperty{id, "StemDirection", engrave::Enum{"down"}}}}));
        EXPECT_EQ(beam->cross_staff_move(), -1);
        EXPECT_EQ(beam->direction(), DirectionV::AUTO);
        for (auto *chord : {first, second, third}) EXPECT_EQ(chord->stem_direction(), DirectionV::AUTO);
        EXPECT_EQ(layout_count(master), layouts);
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(first->beam(), nullptr);
        EXPECT_EQ(first->beam()->rendered_elements(), expected);
        EXPECT_EQ(beam->source_elements(), expected);
    }
}

TEST(Read, S17EndRestAnchorIncludesFollowingMidChords) {
    using namespace mu::engraving;
    for (const std::string duration : {"quarter", "eighth"}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(duration);
            SCOPED_TRACE(warm);
            auto opened = g_engine->open(as_bytes(s17_beam_fixture("<Beam><eid>F_F</eid></Beam><Rest><durationType>" + duration +
                                                                   R"(</durationType><BeamMode>end</BeamMode></Rest>
<Chord><durationType>eighth</durationType><BeamMode>mid</BeamMode><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><BeamMode>mid</BeamMode><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>)")));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *rest = to_rest(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
            auto *first = to_chord(rest->segment()->next(SegmentType::CHORD_REST)->element(0));
            auto *second = to_chord(first->segment()->next(SegmentType::CHORD_REST)->element(0));
            auto *beam = rest->authored_beam();
            ASSERT_NE(beam, nullptr);
            const std::vector<ChordRest *> expected{rest, first, second};
            EXPECT_EQ(beam->source_elements(), expected);
            EXPECT_FALSE(rest->has_layout_data());
            if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            const auto layouts = layout_count(master);
            ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(beam->eid()), "StemDirection", engrave::Enum{"down"}}}}));
            EXPECT_EQ(beam->direction(), DirectionV::DOWN);
            EXPECT_EQ(first->stem_direction(), DirectionV::DOWN);
            EXPECT_EQ(second->stem_direction(), DirectionV::DOWN);
            EXPECT_EQ(layout_count(master), layouts);
            ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            ASSERT_NE(rest->beam(), nullptr);
            EXPECT_EQ(rest->beam()->rendered_elements(), expected);
            EXPECT_EQ(beam->source_elements(), expected);
        }
    }
}

TEST(Read, S16SemanticArticulationsDistinguishTextTappingAndSymbols) {
    const std::array<std::pair<std::string_view, std::string_view>, 5> cases{{
        {"<Articulation><subtype>Slap</subtype></Articulation>", "Slap"},
        {"<Articulation><subtype>Pop</subtype></Articulation>", "Pop"},
        {"<Tapping><hand>left</hand></Tapping>", "tapping:left"},
        {"<Tapping><hand>right</hand></Tapping>", "tapping:right"},
        {"<Articulation><subtype>articStaccatoBelow</subtype></Articulation>", "articStaccatoAbove"},
    }};
    for (const auto &[element, expected] : cases) {
        SCOPED_TRACE(expected);
        auto xml = legacy_segment_fixture("");
        xml.insert(xml.find("</Chord>"), element);
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        const auto before = layout_count(master);
        const auto dump = music::semantic_dump(&master);
        EXPECT_EQ(layout_count(master), before);
        const auto &articulations = dump["events"][0][0][0]["articulations"];
        ASSERT_EQ(articulations.size(), 1u);
        EXPECT_EQ(articulations[0].get<std::string>(), expected);
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        EXPECT_TRUE(music::json_equal(dump, music::semantic_dump(&master)));
    }
}

TEST(Read, NativeBeamDirectionRespectsCrossMeasureBoundaries) {
    using namespace mu::engraving;
    for (const std::string boundary : {"", "line", "page", "section", "frame", "other-beam", "end-mode"}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(boundary);
            SCOPED_TRACE(warm);
            std::string xml = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel/></Instrument></Part>
<Staff id="1"><Measure len="1/8"><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Beam><eid>F_F</eid></Beam><Chord><durationType>eighth</durationType>)";
            if (boundary == "end-mode") xml += "<BeamMode>end</BeamMode>";
            xml += "<Note><pitch>60</pitch><tpc>14</tpc></Note></Chord></voice>";
            if (boundary == "line" || boundary == "page" || boundary == "section")
                xml += "<LayoutBreak><subtype>" + boundary + "</subtype></LayoutBreak>";
            xml += "</Measure>";
            if (boundary == "frame") xml += "<VBox><height>4</height></VBox>";
            xml += "<Measure len=\"1/8\"><voice>";
            if (boundary == "other-beam") xml += "<Beam><eid>G_G</eid></Beam>";
            xml += "<Chord><durationType>eighth</durationType><BeamMode>mid</BeamMode>"
                   "<Note><pitch>62</pitch><tpc>16</tpc></Note></Chord></voice></Measure></Staff></Score></museScore>";
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *first = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
            auto *second = to_chord(master.last_measure()->first(SegmentType::CHORD_REST)->element(0));
            ASSERT_NE(first->authored_beam(), nullptr);
            if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            const auto layouts = layout_count(master);
            ASSERT_TRUE(document.edit(
                {{engrave::SetProperty{engrave::from_engine(first->authored_beam()->eid()), "StemDirection", engrave::Enum{"down"}}}}));
            EXPECT_EQ(first->stem_direction(), DirectionV::DOWN);
            EXPECT_EQ(second->stem_direction(), boundary.empty() ? DirectionV::DOWN : DirectionV::AUTO);
            EXPECT_EQ(layout_count(master), layouts);
        }
    }
}

TEST(Read, NativeGraceBeamDirectionFollowsSourceGraceList) {
    using namespace mu::engraving;
    for (bool warm : {false, true}) {
        auto xml = legacy_segment_fixture("");
        xml.insert(xml.find("<Chord>"), R"(<Beam><eid>F_F</eid></Beam>
<Chord><acciaccatura/><durationType>eighth</durationType><Note><pitch>65</pitch><tpc>13</tpc></Note></Chord>
<Chord><acciaccatura/><durationType>eighth</durationType><Note><pitch>67</pitch><tpc>15</tpc></Note></Chord>)");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *main = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        ASSERT_EQ(main->grace_notes().size(), 2u);
        auto *beam = main->grace_notes().front()->authored_beam();
        ASSERT_NE(beam, nullptr);
        if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        const auto layouts = layout_count(master);
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(beam->eid()), "StemDirection", engrave::Enum{"down"}}}}));
        for (const auto *grace : main->grace_notes()) EXPECT_EQ(grace->stem_direction(), DirectionV::DOWN);
        EXPECT_EQ(main->stem_direction(), DirectionV::AUTO);
        EXPECT_EQ(layout_count(master), layouts);
    }
}

TEST(Read, S14DormantBeamDirectionUsesAuthoredMembers) {
    using namespace mu::engraving;
    for (bool warm : {false, true}) {
        auto xml = legacy_segment_fixture("");
        for (size_t at; (at = xml.find("<durationType>half</durationType>")) != std::string::npos;)
            xml.replace(at, 33, "<durationType>eighth</durationType>");
        xml.insert(xml.find("<Chord>"), "<Beam><eid>F_F</eid></Beam>");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *first = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        auto *second = to_chord(first->segment()->next(SegmentType::CHORD_REST)->element(0));
        auto *beam = first->authored_beam();
        ASSERT_NE(beam, nullptr);
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(second->eid()), "BeamMode", engrave::Enum{"no"}}}}));
        if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        const auto layouts = layout_count(master);
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(beam->eid()), "StemDirection", engrave::Enum{"down"}}}}));
        EXPECT_EQ(beam->direction(), DirectionV::DOWN);
        EXPECT_EQ(first->stem_direction(), DirectionV::DOWN);
        EXPECT_EQ(second->stem_direction(), DirectionV::AUTO);
        EXPECT_EQ(layout_count(master), layouts);
    }
}

TEST(Read, S14MovedBeamRenderingPreservesSourceTrackAndRevision) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    xml.insert(xml.find("</Part>"), "<Staff><StaffType group=\"pitched\"/></Staff>");
    xml.insert(xml.find("<SpannerMap>"),
               "<Staff id=\"2\"><Measure><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff>");
    for (size_t at; (at = xml.find("<durationType>half</durationType>")) != std::string::npos;)
        xml.replace(at, 33, "<durationType>eighth</durationType><staffMove>1</staffMove>");
    xml.insert(xml.find("<Chord>"), "<Beam><eid>F_F</eid></Beam>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *beam = chord->authored_beam();
    ASSERT_NE(beam, nullptr);
    const auto id = engrave::from_engine(beam->eid());
    const auto revision = document.revision();
    for (int pass = 0; pass != 3; ++pass) {
        const auto track = document.get(id, {"TRACK"});
        ASSERT_TRUE(track);
        EXPECT_EQ(std::get<int64_t>(track->front().value), 0);
        const auto found = document.query({.types = {"Beam"}, .staves = {0}});
        ASSERT_TRUE(found);
        ASSERT_EQ(found->elements.size(), 1u);
        EXPECT_EQ(found->elements.front().id, id);
        EXPECT_EQ(found->elements.front().staff, 0);
        EXPECT_EQ(document.revision(), revision);
        ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        EXPECT_EQ(beam->v_staff_idx(), 1u);
        if (pass == 1) engrave::DocumentAccess::impl(document).discard_layout_output(master);
    }
}

TEST(Read, S14EndBarlinePreservesRepeatOnlyAtAuthoredBoundaries) {
    using namespace mu::engraving;
    for (const std::string boundary : {"", "line", "page", "section", "frame", "removed-line"}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(boundary);
            SCOPED_TRACE(warm);
            auto xml = legacy_segment_fixture("");
            const std::string subtype = boundary == "removed-line" ? "line" : boundary;
            std::string measure_end = "</voice>";
            if (!boundary.empty() && boundary != "frame") measure_end += "<LayoutBreak><subtype>" + subtype + "</subtype></LayoutBreak>";
            measure_end += "</Measure>";
            if (boundary == "frame") measure_end += "<VBox><height>4</height></VBox>";
            measure_end += "<Measure><startRepeat/><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff>";
            xml.insert(xml.find("</voice>"), "<BarLine><subtype>double</subtype></BarLine>");
            const std::string ending = "</voice></Measure></Staff>";
            xml.replace(xml.find(ending), ending.size(), measure_end);
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *measure = master.first_measure();
            ASSERT_TRUE(measure->next_measure()->repeat_start());
            auto *bar = measure->end_bar_line();
            ASSERT_NE(bar, nullptr);
            ASSERT_EQ(bar->bar_line_type(), BarLineType::DOUBLE);
            if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            if (boundary == "removed-line") {
                const auto breaks = document.query({.types = {"LayoutBreak"}});
                ASSERT_TRUE(breaks);
                ASSERT_EQ(breaks->elements.size(), 1u);
                ASSERT_TRUE(document.edit({{engrave::Remove{breaks->elements.front().id}}}));
            }
            const auto layouts = layout_count(master);
            const auto changed =
                document.edit({{engrave::SetProperty{engrave::from_engine(bar->eid()), "BARLINE_TYPE", engrave::Enum{"end"}}}});
            ASSERT_TRUE(changed) << changed.error().message;
            EXPECT_EQ(measure->next_measure()->repeat_start(), !boundary.empty() && boundary != "removed-line");
            EXPECT_EQ(layout_count(master), layouts);
        }
    }
}

TEST(Read, LayoutBreakSubtypeEditsControlFollowingRepeatWithoutLayout) {
    using namespace mu::engraving;
    for (const std::string target : {"nobreak", "line", "page", "section"}) {
        for (bool warm : {false, true}) {
            SCOPED_TRACE(target);
            SCOPED_TRACE(warm);
            auto xml = legacy_segment_fixture("");
            xml.insert(xml.find("</voice>"), "<BarLine><subtype>double</subtype></BarLine>");
            const std::string ending = "</voice></Measure></Staff>";
            const std::string initial = target == "nobreak" ? "line" : "nobreak";
            xml.replace(xml.find(ending), ending.size(),
                        "</voice><LayoutBreak><subtype>" + initial +
                            "</subtype></LayoutBreak></Measure><Measure><startRepeat/><voice><Rest>"
                            "<durationType>whole</durationType></Rest></voice></Measure></Staff>");
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &document = **opened;
            auto &master = master_of(document);
            auto *measure = master.first_measure();
            const auto breaks = document.query({.types = {"LayoutBreak"}});
            ASSERT_TRUE(breaks);
            ASSERT_EQ(breaks->elements.size(), 1u);
            if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            const auto layouts = layout_count(master);
            ASSERT_TRUE(document.edit({{engrave::SetProperty{breaks->elements.front().id, "LAYOUT_BREAK", engrave::Enum{target}}}}));
            ASSERT_TRUE(document.edit(
                {{engrave::SetProperty{engrave::from_engine(measure->end_bar_line()->eid()), "BARLINE_TYPE", engrave::Enum{"end"}}}}));
            EXPECT_EQ(measure->next_measure()->repeat_start(), target != "nobreak");
            EXPECT_EQ(layout_count(master), layouts);
            const auto saved = document.export_score({.format = engrave::Format::MSCX});
            ASSERT_TRUE(saved);
            auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
            ASSERT_TRUE(reopened);
            auto *restored = master_of(**reopened).first_measure();
            EXPECT_EQ(restored->line_break(), target == "line");
            EXPECT_EQ(restored->page_break(), target == "page");
            EXPECT_EQ(restored->section_break(), target == "section");
            EXPECT_EQ(restored->no_break(), target == "nobreak");
            EXPECT_EQ(restored->next_measure()->repeat_start(), target != "nobreak");
        }
    }
}

TEST(Read, StartRepeatBarlineEditsUseAuthoredBoundaries) {
    using namespace mu::engraving;
    for (const std::string boundary : {"first", "line", "page", "section", "frame", "wrapped"}) {
        for (const std::string type : {"end", "normal", "end-repeat"}) {
            for (bool warm : {false, true}) {
                SCOPED_TRACE(boundary + ":" + type);
                SCOPED_TRACE(warm);
                auto xml = legacy_segment_fixture("");
                const std::string ending = "</voice></Measure></Staff>";
                if (boundary != "first") {
                    xml.insert(xml.find("</voice>"), "<BarLine><subtype>double</subtype></BarLine>");
                    std::string tail = "</voice>";
                    if (boundary == "line" || boundary == "page" || boundary == "section")
                        tail += "<LayoutBreak><subtype>" + boundary + "</subtype></LayoutBreak>";
                    tail += "</Measure>";
                    if (boundary == "frame") tail += "<VBox><height>4</height></VBox>";
                    tail += "<Measure><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff>";
                    xml.replace(xml.find(ending), ending.size(), tail);
                }
                const auto voice = boundary == "first" ? xml.find("<voice>") : xml.rfind("<voice>");
                xml.insert(voice + 7, "<BarLine><subtype>start-repeat</subtype></BarLine>");
                xml.insert(voice, "<startRepeat/>");
                if (boundary == "wrapped") {
                    xml.insert(xml.find("<Style>") + 7,
                               "<pageWidth>4</pageWidth><pagePrintableWidth>2</pagePrintableWidth><minMeasureWidth>16</minMeasureWidth>");
                }
                auto opened = g_engine->open(as_bytes(xml));
                ASSERT_TRUE(opened);
                auto &document = **opened;
                auto &master = master_of(document);
                auto *measure = boundary == "first" ? master.first_measure() : master.first_measure()->next_measure();
                auto *bar = to_bar_line(measure->first(SegmentType::START_REPEAT_BAR_LINE)->element(0));
                ASSERT_NE(bar, nullptr);
                const auto bar_id = engrave::from_engine(bar->eid());
                if (warm) {
                    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
                    if (boundary == "wrapped") EXPECT_NE(measure->system(), measure->prev_measure()->system());
                }
                const auto layouts = layout_count(master);
                ASSERT_TRUE(document.edit({{engrave::SetProperty{bar_id, "BARLINE_TYPE", engrave::Enum{type}}}}));
                EXPECT_EQ(measure->repeat_start(), type == "end-repeat" && boundary != "wrapped");
                if (auto *prev = measure->prev_measure()) {
                    EXPECT_EQ(prev->repeat_end(), boundary == "wrapped" && type == "end-repeat");
                    if (boundary != "wrapped") EXPECT_EQ(prev->end_bar_line()->bar_line_type(), BarLineType::DOUBLE);
                }
                EXPECT_EQ(layout_count(master), layouts);
                const auto saved = document.export_score({.format = engrave::Format::MSCX});
                ASSERT_TRUE(saved);
                auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
                ASSERT_TRUE(reopened);
                auto *restored = master_of(**reopened).first_measure();
                if (boundary != "first") restored = restored->next_measure();
                EXPECT_EQ(restored->repeat_start(), type == "end-repeat" && boundary != "wrapped");
                if (auto *prev = restored->prev_measure()) EXPECT_EQ(prev->repeat_end(), boundary == "wrapped" && type == "end-repeat");
            }
        }
    }
}

TEST(Read, S14BeamCrossStaffValidationUsesCurrentSourceMoves) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    xml.insert(xml.find("</Part>"), "<Staff><StaffType group=\"pitched\"/></Staff>");
    xml.insert(xml.find("<SpannerMap>"),
               "<Staff id=\"2\"><Measure><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff>");
    for (size_t at; (at = xml.find("<durationType>half</durationType>")) != std::string::npos;)
        xml.replace(at, 33, "<durationType>eighth</durationType>");
    xml.insert(xml.find("<Chord>"), "<Beam><eid>F_F</eid></Beam>");
    for (bool warm : {false, true}) {
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *first = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        auto *second = to_chord(first->segment()->next(SegmentType::CHORD_REST)->element(0));
        auto *beam = first->authored_beam();
        ASSERT_NE(beam, nullptr);
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(second->eid()), "staffMove", int64_t{1}}}}));
        if (warm) ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
        const auto layouts = layout_count(master);
        const auto id = engrave::from_engine(beam->eid());
        ASSERT_TRUE(document.edit(
            {{engrave::SetProperty{id, "crossStaffMove", int64_t{-1}}, engrave::SetProperty{id, "StemDirection", engrave::Enum{"down"}}}}));
        EXPECT_EQ(beam->cross_staff_move(), -1);
        EXPECT_EQ(beam->direction(), DirectionV::AUTO);
        ASSERT_TRUE(document.edit(
            {{engrave::SetProperty{engrave::from_engine(second->eid()), "staffMove", int64_t{0}},
              engrave::SetProperty{id, "crossStaffMove", int64_t{0}}, engrave::SetProperty{id, "StemDirection", engrave::Enum{"down"}}}}));
        EXPECT_EQ(beam->cross_staff_move(), 0);
        EXPECT_EQ(beam->direction(), DirectionV::DOWN);
        EXPECT_EQ(first->stem_direction(), DirectionV::DOWN);
        EXPECT_EQ(second->stem_direction(), DirectionV::DOWN);
        ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "crossStaffMove", int64_t{-1}}}}));
        EXPECT_EQ(beam->cross_staff_move(), 0);
        EXPECT_EQ(layout_count(master), layouts);
    }
}

TEST(Read, AuthoredBeamPositionSurvivesRegroupingAndFullLayout) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    for (size_t at; (at = xml.find("<durationType>half</durationType>")) != std::string::npos;)
        xml.replace(at, 33, "<durationType>eighth</durationType>");
    xml.insert(xml.find("<Chord>"), "<Beam><eid>F_F</eid><Fragment><y1>1.25</y1><y2>2.5</y2></Fragment></Beam>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *first = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *second = to_chord(first->segment()->next(SegmentType::CHORD_REST)->element(0));
    auto *authored = first->authored_beam();
    ASSERT_NE(authored, nullptr);
    ASSERT_TRUE(document.page_count());
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(second->eid()), "BeamMode", engrave::Enum{"no"}}}}));
    ASSERT_TRUE(document.page_count());
    EXPECT_EQ(first->beam(), nullptr);
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(second->eid()), "BeamMode", engrave::Enum{"auto"}}}}));
    const auto range = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(range);
    EXPECT_EQ(first->beam(), authored);
    ASSERT_FALSE(authored->beam_fragments().empty());
    EXPECT_DOUBLE_EQ(authored->beam_fragments().front()->py1[0] / authored->spatium(), 1.25);
    EXPECT_DOUBLE_EQ(authored->beam_fragments().front()->py2[0] / authored->spatium(), 2.5);
    master.do_layout();
    const auto full = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(full);
    EXPECT_EQ(range->pages, full->pages);
    EXPECT_EQ(first->beam(), authored);
    EXPECT_TRUE(engrave::resolve(master, engrave::from_engine(authored->eid())));
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(authored->eid()), "BEAM_POS", engrave::Point{5.25, 6.75}},
                                engrave::SetProperty{engrave::from_engine(authored->eid()), "noSlope", true}}}));
    ASSERT_TRUE(document.page_count());
    EXPECT_DOUBLE_EQ(authored->beam_fragments().front()->py1[0] / authored->spatium(), 6.0);
    EXPECT_DOUBLE_EQ(authored->beam_fragments().front()->py2[0] / authored->spatium(), 6.0);
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(first->eid()), "color", engrave::Color{20, 40, 60}},
                                engrave::SetProperty{engrave::from_engine(authored->eid()), "StemDirection", engrave::Enum{"down"}}}}));
    EXPECT_EQ(first->stem_direction(), DirectionV::DOWN);
    EXPECT_EQ(second->stem_direction(), DirectionV::DOWN);
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(second->eid()), "BeamMode", engrave::Enum{"no"}}}}));
    ASSERT_TRUE(document.page_count());
    EXPECT_EQ(second->stem_direction(), DirectionV::DOWN);
    const auto saved = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    const auto restored = engrave::resolve(master_of(**reopened), engrave::from_engine(second->eid()));
    ASSERT_TRUE(restored);
    EXPECT_EQ(to_chord(restored->object)->stem_direction(), DirectionV::DOWN);
}

TEST(Read, IndividualTabStemSuppressionOnlyDetachesRenderedBeams) {
    using namespace mu::engraving;
    for (bool authored : {false, true}) {
        auto xml = legacy_segment_fixture("");
        xml.replace(xml.find("<StaffType group=\"pitched\"/>"), 28,
                    "<StaffType group=\"tablature\"><lines>6</lines><stemless>0</stemless><stemsThrough>1</stemsThrough></StaffType>");
        xml.insert(xml.find("<Channel>"), "<StringData><frets>24</frets><string>40</string><string>45</string>"
                                          "<string>50</string><string>55</string><string>59</string><string>64</string></StringData>");
        for (size_t at; (at = xml.find("<durationType>half</durationType>")) != std::string::npos;)
            xml.replace(at, 33, "<durationType>eighth</durationType>");
        if (authored) xml.insert(xml.find("<Chord>"), "<Beam><eid>F_F</eid><Fragment><y1>1.25</y1><y2>2.5</y2></Fragment></Beam>");
        xml.insert(xml.find("<durationType>"), "<noStem>1</noStem>");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *first = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        const auto cold = document.export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(cold);
        ASSERT_TRUE(document.page_count());
        EXPECT_EQ(first->beam(), nullptr);
        const auto warm = document.export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(warm);
        EXPECT_EQ(*cold, *warm);
        if (authored) EXPECT_TRUE(engrave::resolve(master, engrave::from_engine(first->authored_beam()->eid())));
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(first->eid()), "color", engrave::Color{20, 40, 60}}}}));
        ASSERT_TRUE(document.page_count());
        master.layout_data_store()->clear();
        ASSERT_TRUE(document.page_count());
        EXPECT_EQ(first->beam(), nullptr);
    }
}

TEST(Read, AutomaticBeamsStayOutsideSourceAndHandlesAcrossRegrouping) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    for (size_t at; (at = xml.find("<durationType>half</durationType>")) != std::string::npos;)
        xml.replace(at, 33, "<durationType>eighth</durationType>");
    xml.replace(xml.find("<durationType>eighth</durationType>"), 35, "<durationType>16th</durationType>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *first = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *second = to_chord(first->segment()->next(SegmentType::CHORD_REST)->element(0));
    const auto cold = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(cold);
    const auto cold_beams = document.query({.types = {"Beam"}});
    ASSERT_TRUE(cold_beams);
    EXPECT_TRUE(cold_beams->elements.empty());
    ASSERT_TRUE(document.page_count());
    ASSERT_NE(first->beam(), nullptr);
    EXPECT_EQ(first->beam(), second->beam());
    ASSERT_NE(first->beamlet(), nullptr);
    EXPECT_EQ(first->authored_beam(), nullptr);
    EXPECT_EQ(second->authored_beam(), nullptr);
    EXPECT_FALSE(source_object(*first->beam()));
    EXPECT_FALSE(engrave::takes_id(*first->beam()));
    EXPECT_FALSE(engrave::address_of(*first->beam()));
    engrave::assign_missing_ids(master);
    EXPECT_FALSE(engrave::resolve(master, engrave::from_engine(first->beam()->eid())));
    EXPECT_FALSE(document.edit({{engrave::SetProperty{engrave::from_engine(first->beam()->eid()), "growLeft", 0.5}}}));
    const auto warm_beams = document.query({.types = {"Beam"}});
    ASSERT_TRUE(warm_beams);
    EXPECT_TRUE(warm_beams->elements.empty());
    const auto warm = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(warm);
    EXPECT_EQ(*cold, *warm);
    master.layout_data_store()->clear();
    EXPECT_EQ(first->beamlet(), nullptr);
    EXPECT_EQ(first->beam(), nullptr);
    ASSERT_TRUE(document.page_count());
    ASSERT_NE(first->beamlet(), nullptr);
    const auto edited = document.edit({{engrave::SetProperty{engrave::from_engine(second->eid()), "BeamMode", engrave::Enum{"no"}}}});
    ASSERT_TRUE(edited) << edited.error().message;
    EXPECT_EQ(first->beam(), nullptr);
    master.do_layout_range(Fraction(0, 1), Fraction(1, 1));
    EXPECT_EQ(first->beam(), nullptr);
    EXPECT_EQ(second->beam(), nullptr);
    const auto range = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(range);
    master.do_layout();
    const auto full = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(full);
    EXPECT_EQ(range->pages, full->pages);
    master.layout_data_store()->clear();
    EXPECT_EQ(first->beamlet(), nullptr);
    EXPECT_EQ(first->beam(), nullptr);
}

TEST(Read, BeamManualLegacyOrdinalsAndDirectionsSurviveLayout) {
    using namespace mu::engraving;
    for (const std::string version : {"4.60", "5.00"}) {
        auto input = read_text(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/layout_elements.mscx");
        const auto score_version = input.find("version=\"", input.find("<museScore"));
        input.replace(score_version + 9, 4, version);
        input.insert(input.find("<Beam>") + 6, R"(<Fragment><y1>1.25</y1><y2>2.5</y2></Fragment>
<Fragment no="2"><y1>3.75</y1><y2>4.5</y2></Fragment>
<Fragment no="0" direction="1"><y1>-1.25</y1><y2>-2.5</y2></Fragment>)");
        auto document = g_engine->open(as_bytes(input));
        ASSERT_TRUE(document) << document.error().message;
        const auto find = [](Score &score) {
            Beam *result = nullptr;
            score.scan_elements([&](EngravingItem *item) {
                if (!item->is_note()) return;
                auto *beam = to_note(item)->chord()->beam();
                if (beam && beam->authored_overrides.find({AuthoredChildRole::BEAM_FRAGMENT, 2, 0}, Pid::BEAM_POS)) result = beam;
            });
            return result;
        };
        auto *beam = find(master_of(**document));
        ASSERT_NE(beam, nullptr);
        EXPECT_TRUE(beam->beam_fragments().empty());
        EXPECT_TRUE(beam->beam_segments().empty());
        EXPECT_DOUBLE_EQ(beam->beam_pos().first, 3.75);
        EXPECT_DOUBLE_EQ(beam->beam_pos().second, 4.5);
        const auto beam_id = engrave::from_engine(beam->eid());
        const auto source = beam->authored_overrides.entries;
        const auto bytes = legacy_override_write(beam);
        EXPECT_NE(bytes.find("no=\"2\""), std::string::npos);
        EXPECT_NE(bytes.find("direction=\"1\""), std::string::npos);
        ASSERT_TRUE((*document)->page_count());
        beam = find(master_of(**document));
        ASSERT_NE(beam, nullptr);
        EXPECT_EQ(legacy_override_write(beam), bytes);
        ASSERT_FALSE(beam->beam_fragments().empty());
        EXPECT_DOUBLE_EQ(beam->beam_fragments().front()->py1[0] / beam->spatium(), 1.25);
        EXPECT_DOUBLE_EQ(beam->beam_fragments().front()->py2[0] / beam->spatium(), 2.5);
        ASSERT_EQ(beam->authored_overrides.entries.size(), source.size());
        for (size_t i = 0; i < source.size(); ++i) EXPECT_EQ(beam->authored_overrides.entries[i].value, source[i].value);
        for (auto *fragment : beam->beam_fragments()) {
            fragment->py1[0] = 99;
            fragment->py2[0] = -99;
        }
        EXPECT_EQ(legacy_override_write(beam), bytes);
        auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto *restored = find(master_of(**reopened));
        ASSERT_NE(restored, nullptr);
        EXPECT_EQ(legacy_override_write(restored), bytes);
        std::unique_ptr<Beam> copy(restored->clone());
        EXPECT_EQ(copy->authored_overrides.entries.size(), source.size());
        const auto edited =
            (*document)->edit({{engrave::SetProperty{beam_id, "BEAM_POS", engrave::Point{5.25, 6.75}},
                                engrave::SetProperty{beam_id, "noSlope", true}, engrave::SetProperty{beam_id, "growLeft", 0.5}}});
        ASSERT_TRUE(edited) << edited.error().message;
        EXPECT_DOUBLE_EQ(beam->beam_pos().first, 6.0);
        EXPECT_DOUBLE_EQ(beam->beam_pos().second, 6.0);
        EXPECT_DOUBLE_EQ(beam->grow_left(), 0.5);
        const auto same = (*document)->edit({{engrave::SetProperty{beam_id, "BEAM_POS", engrave::Point{6.0, 6.0}}}});
        ASSERT_TRUE(same);
        EXPECT_EQ(same->revision, edited->revision);
        const auto authored = beam->elements();
        master_of(**document).layout_data_store()->clear();
        EXPECT_TRUE(beam->beam_fragments().empty());
        EXPECT_TRUE(beam->beam_segments().empty());
        ASSERT_TRUE((*document)->page_count());
        EXPECT_EQ(beam->elements(), authored);
        EXPECT_TRUE(engrave::resolve(master_of(**document), beam_id));
    }
}

TEST(Read, TremoloManualFragmentImportsBeforeLayoutAndSurvivesNativeReopen) {
    const auto path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) /
                      "src/engraving/tests/playback/playbackeventsrenderer_data/two_chords_tremolo/two_chords_tremolo.mscx";
    for (const std::string version : {"4.60", "5.00"}) {
        auto xml = read_text(path);
        xml.replace(xml.find("5.00"), 4, version);
        xml.insert(xml.find("</TremoloTwoChord>"), "<Fragment><y1>1.25</y1><y2>2.5</y2></Fragment>");
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document) << document.error().message;
        mu::engraving::TremoloTwoChord *tremolo = nullptr;
        master_of(**document).scan_elements([&](mu::engraving::EngravingItem *item) {
            if (item->type() == mu::engraving::ElementType::TREMOLO_TWOCHORD) tremolo = static_cast<mu::engraving::TremoloTwoChord *>(item);
        });
        ASSERT_NE(tremolo, nullptr);
        EXPECT_DOUBLE_EQ(tremolo->beam_pos().first, 1.25);
        EXPECT_DOUBLE_EQ(tremolo->beam_pos().second, 2.5);
        const auto bytes = legacy_override_write(tremolo);
        ASSERT_TRUE((*document)->page_count());
        EXPECT_EQ(legacy_override_write(tremolo), bytes);
        const auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened) << reopened.error().message;
        mu::engraving::TremoloTwoChord *restored = nullptr;
        master_of(**reopened).scan_elements([&](mu::engraving::EngravingItem *item) {
            if (item->type() == mu::engraving::ElementType::TREMOLO_TWOCHORD)
                restored = static_cast<mu::engraving::TremoloTwoChord *>(item);
        });
        ASSERT_NE(restored, nullptr);
        EXPECT_DOUBLE_EQ(restored->beam_pos().first, 1.25);
        EXPECT_DOUBLE_EQ(restored->beam_pos().second, 2.5);
    }
}

TEST(Read, PublicLinkedTremoloResetPropagatesFromMasterAndUnlinksFromPart) {
    using namespace mu::engraving;
    const auto fixture = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) /
                         "src/engraving/tests/playback/playbackeventsrenderer_data/two_chords_tremolo/two_chords_tremolo.mscx";
    const auto find_tremolo = [](Score &score) {
        TremoloTwoChord *result = nullptr;
        score.scan_elements([&](EngravingItem *item) {
            if (item->type() == ElementType::TREMOLO_TWOCHORD) result = static_cast<TremoloTwoChord *>(item);
        });
        return result;
    };
    for (const bool from_part : {false, true}) {
        auto opened = g_engine->open(fixture);
        ASSERT_TRUE(opened);
        auto &master = master_of(**opened);
        const auto outline = (*opened)->outline();
        ASSERT_TRUE(outline);
        ASSERT_TRUE((*opened)->edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
        auto *source = find_tremolo(master);
        auto *part = find_tremolo(*master.excerpts().front()->excerpt_score());
        ASSERT_NE(source, nullptr);
        ASSERT_NE(part, nullptr);
        ASSERT_TRUE((*opened)->edit({{engrave::SetProperty{engrave::from_engine(source->eid()), "BEAM_POS", engrave::Point{5.25, 6.75}}}}));
        EXPECT_EQ(part->beam_pos(), muse::PairF(5.25, 6.75));
        ASSERT_TRUE(part->is_position_linked_to_master());
        auto *target = from_part ? part : source;
        ASSERT_TRUE((*opened)->edit({{engrave::ResetProperty{engrave::from_engine(target->eid()), "BEAM_POS"}}}));
        EXPECT_EQ(source->beam_pos(), from_part ? muse::PairF(5.25, 6.75) : muse::PairF(0.0, 0.0));
        EXPECT_EQ(part->beam_pos(), muse::PairF(0.0, 0.0));
        EXPECT_EQ(part->is_position_linked_to_master(), !from_part);
        EXPECT_EQ(target->authored_overrides.find({AuthoredChildRole::BEAM_FRAGMENT, 0, uint8_t(target->direction_idx())}, Pid::BEAM_POS),
                  nullptr);
        ASSERT_TRUE(
            (*opened)->edit({{engrave::SetProperty{engrave::from_engine(source->eid()), "BEAM_POS", engrave::Point{-1.25, -2.5}}}}));
        auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        auto &copy = master_of(**reopened);
        auto *restored_source = find_tremolo(copy);
        auto *restored_part = find_tremolo(*copy.excerpts().front()->excerpt_score());
        ASSERT_NE(restored_source, nullptr);
        ASSERT_NE(restored_part, nullptr);
        EXPECT_EQ(restored_source->beam_pos(), muse::PairF(-1.25, -2.5));
        EXPECT_EQ(restored_part->beam_pos(), from_part ? muse::PairF(0.0, 0.0) : muse::PairF(-1.25, -2.5));
        EXPECT_EQ(restored_part->is_position_linked_to_master(), !from_part);
    }
}

TEST(Read, TremoloManualFragmentOwnsBothDirectionsAndIgnoresDisplayWrites) {
    auto document = g_engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) /
                                   "src/engraving/tests/playback/playbackeventsrenderer_data/two_chords_tremolo/two_chords_tremolo.mscx");
    ASSERT_TRUE(document) << document.error().message;
    mu::engraving::TremoloTwoChord *tremolo = nullptr;
    master_of(**document).scan_elements([&](mu::engraving::EngravingItem *item) {
        if (item->type() == mu::engraving::ElementType::TREMOLO_TWOCHORD) tremolo = static_cast<mu::engraving::TremoloTwoChord *>(item);
    });
    ASSERT_NE(tremolo, nullptr);
    for (auto direction : {mu::engraving::DirectionV::DOWN, mu::engraving::DirectionV::UP}) {
        tremolo->do_set_direction(direction);
        tremolo->set_user_modified(true);
        const auto endpoints = direction == mu::engraving::DirectionV::UP ? muse::PairF(-1.25, -2.5) : muse::PairF(1.25, 2.5);
        ASSERT_TRUE(tremolo->set_property(mu::engraving::Pid::BEAM_POS, mu::engraving::PropertyValue::from_value(endpoints)));
        const auto bytes = legacy_override_write(tremolo);
        tremolo->beam_fragment().py1[tremolo->direction_idx()] = 999.0;
        tremolo->beam_fragment().py2[tremolo->direction_idx()] = -999.0;
        EXPECT_EQ(legacy_override_write(tremolo), bytes);
        EXPECT_DOUBLE_EQ(tremolo->beam_pos().first, endpoints.first);
        EXPECT_DOUBLE_EQ(tremolo->beam_pos().second, endpoints.second);
    }
    std::unique_ptr<mu::engraving::TremoloTwoChord> copy(tremolo->clone());
    for (auto direction : {mu::engraving::DirectionV::DOWN, mu::engraving::DirectionV::UP}) {
        tremolo->do_set_direction(direction);
        copy->do_set_direction(direction);
        EXPECT_EQ(copy->get_property(mu::engraving::Pid::BEAM_POS), tremolo->get_property(mu::engraving::Pid::BEAM_POS));
    }
    const auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
    ASSERT_TRUE(saved) << saved.error().message;
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened) << reopened.error().message;
    mu::engraving::TremoloTwoChord *restored = nullptr;
    master_of(**reopened).scan_elements([&](mu::engraving::EngravingItem *item) {
        if (item->type() == mu::engraving::ElementType::TREMOLO_TWOCHORD) restored = static_cast<mu::engraving::TremoloTwoChord *>(item);
    });
    ASSERT_NE(restored, nullptr);
    for (const auto direction : {uint8_t(0), uint8_t(1)}) {
        const mu::engraving::AuthoredChildKey key{mu::engraving::AuthoredChildRole::BEAM_FRAGMENT, 0, direction};
        ASSERT_NE(restored->authored_overrides.find(key, mu::engraving::Pid::BEAM_POS), nullptr);
        EXPECT_EQ(restored->authored_overrides.find(key, mu::engraving::Pid::BEAM_POS)->value,
                  tremolo->authored_overrides.find(key, mu::engraving::Pid::BEAM_POS)->value);
    }
}

TEST(Read, AuthoredParenthesisGroupLegacyBaseline) {
    for (const std::string version : {"4.60", "5.00"}) {
        auto xml = chordline_offset_fixture();
        xml.replace(xml.find("5.00"), 4, version);
        const auto begin = xml.find("<ChordLine>");
        xml.erase(begin, xml.find("</ChordLine>", begin) + 12 - begin);
        xml.insert(xml.find("<Note>") + 6, "<eid>C_C</eid><parentheses>both</parentheses>");
        xml.insert(xml.find("</Note>") + 7,
                   "<NoteParenGroup><Parenthesis><eid>D_D</eid><linkedTo>Z_Z</linkedTo><offset x=\"0.25\" y=\"-0.5\"/></Parenthesis>"
                   "<Parenthesis><horizontalDirection>right</horizontalDirection><eid>E_E</eid></Parenthesis>"
                   "<Notes><NoteEID>C_C</NoteEID></Notes></NoteParenGroup>");
        if (version == "4.60") {
            const auto note_ref = xml.find("<NoteEID>C_C</NoteEID>");
            xml.replace(note_ref, std::string_view("<NoteEID>C_C</NoteEID>").size(), "<NoteIdx>0</NoteIdx>");
        }
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document) << document.error().message;
        auto &master = master_of(**document);
        auto *chord = mu::engraving::to_chord(master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST)->element(0));
        ASSERT_EQ(chord->note_parentheses().size(), 1u);
        const auto *group = chord->note_parentheses().front();
        ASSERT_EQ(group->notes().size(), 1u);
        EXPECT_EQ(group->notes().front(), chord->notes().front());
        EXPECT_TRUE(chord->layout_parentheses().empty());
        const auto bytes = legacy_override_write(chord);
        const auto source = chord->authored_overrides.entries;
        std::unique_ptr<mu::engraving::Chord> copy(chord->clone());
        EXPECT_FALSE(copy->has_layout_data());
        EXPECT_TRUE(copy->layout_parentheses().empty());
        EXPECT_EQ(copy->note_parentheses().front()->notes().front(), copy->notes().front());
        mu::engraving::rendering::score::LayoutContext ctx(&master);
        mu::engraving::rendering::score::ChordLayout::layout(copy.get(), ctx);
        ASSERT_FALSE(copy->layout_parentheses().empty());
        EXPECT_EQ(copy->layout_parentheses().front().left->offset(), muse::PointF(0.25, -0.5) * master.style().spatium());
        master.renderer()->layout_data_store(master).erase(*copy);
        EXPECT_TRUE(copy->layout_parentheses().empty());
        ASSERT_TRUE((*document)->page_count());
        EXPECT_EQ(legacy_override_write(chord), bytes);
        ASSERT_EQ(chord->authored_overrides.entries.size(), source.size());
        for (size_t i = 0; i < source.size(); ++i) {
            EXPECT_EQ(chord->authored_overrides.entries[i].child, source[i].child);
            EXPECT_EQ(chord->authored_overrides.entries[i].value, source[i].value);
            EXPECT_EQ(chord->authored_overrides.entries[i].flags, source[i].flags);
            EXPECT_EQ(chord->authored_overrides.entries[i].unit, source[i].unit);
        }
        const auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto &restored = master_of(**reopened);
        auto *restored_chord = mu::engraving::to_chord(restored.first_measure()->first(mu::engraving::SegmentType::CHORD_REST)->element(0));
        EXPECT_EQ(legacy_override_write(restored_chord), bytes);
    }
}

TEST(Read, GhostParenthesesRemainLayoutOutput) {
    using namespace mu::engraving;
    auto xml = chordline_offset_fixture();
    const auto begin = xml.find("<ChordLine>");
    xml.erase(begin, xml.find("</ChordLine>", begin) + 12 - begin);
    xml.insert(xml.find("<Note>") + 6, "<ghost>1</ghost>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    EXPECT_TRUE(chord->layout_parentheses().empty());
    ASSERT_TRUE((*opened)->page_count());
    ASSERT_FALSE(chord->layout_parentheses().empty());
    EXPECT_TRUE(chord->layout_parentheses().front().left->generated());
    EXPECT_TRUE(chord->note_parentheses().empty());
    EXPECT_EQ(chord->notes().front()->parentheses_mode(), ParenthesesMode::NONE);
    master.renderer()->layout_data_store(master).erase(*chord);
    EXPECT_TRUE(chord->layout_parentheses().empty());
}

TEST(Read, ChordVisibilityIncludesLayoutChildren) {
    using namespace mu::engraving;
    auto xml = chordline_offset_fixture();
    xml.replace(xml.find("<durationType>whole</durationType>"), 34, "<durationType>eighth</durationType><showStemSlash>1</showStemSlash>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = offset_test_lines(master).front()->chord();
    ASSERT_TRUE((*opened)->page_count());
    ASSERT_NE(chord->hook(), nullptr);
    ASSERT_NE(chord->stem_slash(), nullptr);
    {
        Command command(master);
        chord->undo_change_property(Pid::VISIBLE, false);
        command.commit();
    }
    ASSERT_TRUE((*opened)->page_count());
    EXPECT_FALSE(chord->notes().front()->visible());
    EXPECT_FALSE(chord->stem()->visible());
    EXPECT_FALSE(chord->hook()->visible());
    EXPECT_FALSE(chord->stem_slash()->visible());
}

TEST(Read, MusicXmlChordKeepsTheFirstNotesStemColor) {
    using namespace mu::engraving;
    auto xml = dynamics_musicxml("", "");
    xml.replace(xml.find("<duration>4</duration>"), 22, "<duration>1</duration>");
    xml.replace(xml.find("<type>whole</type>"), 18, "<type>quarter</type><stem color=\"#ff0000\">up</stem>");
    xml.insert(
        xml.find("</measure>"),
        R"(<note color="#0000ff"><chord/><pitch><step>E</step><octave>4</octave></pitch><duration>1</duration><type>quarter</type></note>)");
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_EQ(chord->notes().size(), 2u);
    ASSERT_EQ(chord->stem(), nullptr);
    const auto *color = chord->authored_overrides.find({AuthoredChildRole::STEM}, Pid::COLOR);
    ASSERT_NE(color, nullptr);
    EXPECT_EQ(color->value.value<muse::draw::Color>(), muse::draw::Color::from_string("#ff0000"));
    ASSERT_TRUE((*opened)->page_count());
    EXPECT_EQ(chord->stem()->color(), muse::draw::Color::from_string("#ff0000"));
}

TEST(Read, StemUserLengthSurvivesDisplayRecreation) {
    for (const std::string version : {"4.60", "5.00"}) {
        auto xml = chordline_offset_fixture();
        xml.replace(xml.find("<durationType>whole</durationType>"), 34, "<durationType>quarter</durationType>");
        xml.replace(xml.find("5.00"), 4, version);
        xml.insert(xml.find("<durationType>"), "<Stem><userLen>2.5</userLen></Stem>");
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document) << document.error().message;
        auto *chord = offset_test_lines(master_of(**document)).front()->chord();
        EXPECT_FALSE(chord->has_layout_data());
        EXPECT_EQ(chord->stem(), nullptr);
        EXPECT_EQ(chord->hook(), nullptr);
        EXPECT_EQ(chord->stem_slash(), nullptr);
        std::unique_ptr<mu::engraving::Chord> copy(chord->clone());
        EXPECT_EQ(copy->stem(), nullptr);
        EXPECT_DOUBLE_EQ(copy->stem_user_length().val(), 2.5);
        ASSERT_TRUE((*document)->page_count());
        ASSERT_NE(chord->stem(), nullptr);
        EXPECT_DOUBLE_EQ(chord->stem()->user_length().val(), 2.5);
        ASSERT_TRUE(mu::engraving::set_child_override(*chord, {mu::engraving::AuthoredChildRole::STEM}, mu::engraving::Pid::USER_LEN,
                                                      mu::engraving::Spatium(1.25), mu::engraving::PropertyFlags::NOSTYLE));
        EXPECT_DOUBLE_EQ(copy->stem_user_length().val(), 2.5);
        const auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened) << reopened.error().message;
        EXPECT_DOUBLE_EQ(offset_test_lines(master_of(**reopened)).front()->chord()->stem_user_length().val(), 1.25);
    }
}

TEST(Read, S14AuthoredGeometryPropertiesGetAndQueryWithoutLayout) {
    for (bool beam : {false, true}) {
        auto xml = beam ? legacy_segment_fixture("") : chordline_offset_fixture();
        if (beam) xml.insert(xml.find("<Chord>"), "<Beam><eid>F_F</eid><Fragment><y1>1.25</y1><y2>2.5</y2></Fragment></Beam>");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        const std::string property = beam ? "BEAM_POS" : "path";
        const engrave::Value expected = beam ? engrave::Value{engrave::Point{1.25, 2.5}} : engrave::Value{engrave::Opaque{"draw_path"}};
        const std::string type = beam ? "Beam" : "ChordLine";
        auto found = document.query({.types = {type}});
        ASSERT_TRUE(found);
        ASSERT_EQ(found->elements.size(), 1u);
        const auto id = found->elements.front().id;
        for (int pass = 0; pass != 2; ++pass) {
            const auto layouts = layout_count(master);
            const auto value = document.get(id, {property});
            ASSERT_TRUE(value) << value.error().message;
            ASSERT_EQ(value->size(), 1u);
            EXPECT_EQ(value->front().value, expected);
            ASSERT_TRUE(document.get(id));
            const auto query = document.query({.types = {type}, .props = {property}});
            ASSERT_TRUE(query);
            ASSERT_EQ(query->elements.size(), 1u);
            EXPECT_EQ(query->elements.front().props.front().value, expected);
            EXPECT_EQ(layout_count(master), layouts);
            ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "color", engrave::Color{20, 40, 60}}}}));
        }
        EXPECT_EQ(layout_count(master), 0u);
    }
}

TEST(Read, ChordLineDefaultPathReadDoesNotAllocateLayout) {
    auto xml = chordline_offset_fixture();
    const auto begin = xml.find("<o1");
    const auto end = xml.find("<subtype>", begin);
    xml.erase(begin, end - begin);
    auto document = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(document) << document.error().message;
    const auto lines = offset_test_lines(master_of(**document));
    ASSERT_EQ(lines.size(), 1u);
    auto *line = lines.front();
    ASSERT_FALSE(line->has_layout_data());
    EXPECT_EQ(line->get_property(mu::engraving::Pid::PATH), mu::engraving::PropertyValue::from_value(muse::draw::PainterPath()));
    EXPECT_FALSE(line->has_layout_data());
    std::unique_ptr<mu::engraving::ChordLine> copy(line->clone());
    EXPECT_FALSE(copy->has_layout_data());
}

TEST(Read, ChordLineLegacySourceSurvivesLayoutAndCacheReplacement) {
    for (const std::string version : {"4.60", "5.00"}) {
        SCOPED_TRACE(version);
        auto xml = chordline_offset_fixture();
        xml.replace(xml.find("5.00"), 4, version);
        const auto begin = xml.find("<o1");
        const auto end = xml.find("<subtype>", begin);
        xml.replace(begin, end - begin,
                    "<Path><Element type=\"0\" x=\"0\" y=\"0\"/>"
                    "<Element type=\"1\" x=\"1.25\" y=\"-0.5\"/></Path>");
        auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document) << document.error().message;
        auto &master = master_of(**document);
        const auto lines = offset_test_lines(master);
        ASSERT_EQ(lines.size(), 1u);
        auto *line = lines.front();
        ASSERT_TRUE(line->modified());
        ASSERT_FALSE(line->has_source_offsets());
        EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        const auto source = line->get_property(mu::engraving::Pid::PATH);
        const auto bytes = offset_test_write(line);
        line->mutldata()->path = muse::draw::PainterPath();
        EXPECT_EQ(offset_test_write(line), bytes);
        EXPECT_EQ(line->get_property(mu::engraving::Pid::PATH), source);
        ASSERT_TRUE((*document)->page_count());
        EXPECT_EQ(offset_test_write(line), bytes);
        EXPECT_EQ(line->get_property(mu::engraving::Pid::PATH), source);
        EXPECT_EQ(line->ldata()->path, line->legacy_absolute_path());
        for (const std::string native : {"5.00", "4.70"}) {
            const auto saved = (*document)->export_score({.format = engrave::Format::MSCX, .native_version = native});
            ASSERT_TRUE(saved) << saved.error().message;
            auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
            ASSERT_TRUE(reopened) << reopened.error().message;
            const auto restored = offset_test_lines(master_of(**reopened));
            ASSERT_EQ(restored.size(), 1u);
            EXPECT_EQ(offset_test_write(restored.front()), bytes);
        }
    }
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
        // This detached probe bypasses the edit finalizer.
        copy->assign_new_eid();
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

TEST(Read, SpannerRootStylesDoNotBecomeSegmentOverrides) {
    using namespace mu::engraving;
    auto opened = g_engine->open(write470_fixture());
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    std::unique_ptr<TextLine> line(Factory::create_text_line(master.dummy()));
    const Align alignment{AlignH::RIGHT, AlignV::BASELINE};
    ASSERT_TRUE(set_child_override(*line, {AuthoredChildRole::SPANNER_SEGMENT}, Pid::BEGIN_TEXT_ALIGN, PropertyValue::from_value(alignment),
                                   PropertyFlags::NOSTYLE));
    EXPECT_EQ(line->get_property(Pid::BEGIN_TEXT_ALIGN).value<Align>(), alignment);
    EXPECT_TRUE(line->authored_overrides.entries.empty());
    EXPECT_TRUE(line->authored_overrides.carriers.empty());
    std::unique_ptr<GuitarBend> bend(Factory::create_guitar_bend(master.dummy()));
    EXPECT_FALSE(set_child_override(*bend, {AuthoredChildRole::SPANNER_SEGMENT}, Pid::BEGIN_TEXT_ALIGN,
                                    PropertyValue::from_value(alignment), PropertyFlags::NOSTYLE));
    EXPECT_TRUE(bend->authored_overrides.entries.empty());
    EXPECT_TRUE(bend->authored_overrides.carriers.empty());
}

TEST(Read, SuppressedStemOverridesKeepPartLinksAcrossNativeSave) {
    using namespace mu::engraving;
    for (bool suppressed : {false, true}) {
        SCOPED_TRACE(suppressed);
        auto xml = legacy_segment_fixture("");
        xml.insert(xml.find("<Note>"), "<Stem><eid>F_F</eid><userLen>1.25</userLen><color r=\"20\" g=\"40\" b=\"60\" a=\"255\"/></Stem>");
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        const auto chords = document.query({.types = {"Chord"}, .limit = 1});
        ASSERT_TRUE(chords);
        ASSERT_TRUE(document.edit({{engrave::SetProperty{chords->elements.front().id, "noStem", suppressed}}}));
        const auto outline = document.outline();
        ASSERT_TRUE(outline);
        ASSERT_TRUE(document.edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
        const auto saved = document.export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        const std::string saved_xml(reinterpret_cast<const char *>(saved->data()), saved->size());
        EXPECT_NE(saved_xml.find("<linkedTo>F_F</linkedTo>"), std::string::npos);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        ASSERT_TRUE((*reopened)->edit({{engrave::SetProperty{chords->elements.front().id, "StemDirection", engrave::Enum{"down"}}}}));
        auto &master = master_of(**reopened);
        ASSERT_FALSE(master.excerpts().empty());
        auto *part = master.excerpts().front()->excerpt_score();
        auto *clone = to_chord(part->first_measure()->first(SegmentType::CHORD_REST)->element(0));
        EXPECT_EQ(clone->stem_direction(), DirectionV::DOWN);
        EXPECT_EQ(clone->no_stem(), suppressed);
        EXPECT_DOUBLE_EQ(clone->stem_user_length().val(), 1.25);
        ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
        const auto warm = (*reopened)->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(warm);
        const std::string warm_xml(reinterpret_cast<const char *>(warm->data()), warm->size());
        EXPECT_NE(warm_xml.find("<linkedTo>F_F</linkedTo>"), std::string::npos);
    }
}

TEST(Read, ParenthesisAndStemLinksFollowTheirChord) {
    using namespace mu::engraving;
    auto xml = parenthesis_link_fixture();
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *source = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    const auto outline = (*opened)->outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE((*opened)->edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
    auto *part = master.excerpts().front()->excerpt_score();
    auto *clone = to_chord(part->first_measure()->first(SegmentType::CHORD_REST)->element(0));
    const auto linked = legacy_override_write(clone);
    EXPECT_NE(linked.find("<linkedTo>D_D</linkedTo>"), std::string::npos);
    EXPECT_NE(linked.find("<linkedTo>E_E</linkedTo>"), std::string::npos);
    EXPECT_NE(linked.find("<linkedTo>F_F</linkedTo>"), std::string::npos);
    EXPECT_EQ(legacy_override_write(clone, true).find("<linkedTo>"), std::string::npos);
    clone->unlink();
    const auto unlinked = legacy_override_write(clone);
    const auto group_start = unlinked.find("<NoteParenGroup>");
    ASSERT_NE(group_start, std::string::npos);
    EXPECT_EQ(unlinked.substr(group_start).find("<linkedTo>"), std::string::npos);
    const auto stem_start = unlinked.find("<Stem>");
    ASSERT_NE(stem_start, std::string::npos);
    EXPECT_EQ(unlinked.substr(stem_start, unlinked.find("</Stem>", stem_start) - stem_start).find("<linkedTo>"), std::string::npos);
    clone->link_to(source);
    EXPECT_EQ(legacy_override_write(clone), linked);
    auto *removed = clone->note_parentheses().front();
    clone->remove_note_parenthesis_info(removed);
    delete removed;
    const auto shifted = legacy_override_write(clone);
    EXPECT_NE(shifted.find("<linkedTo>G_G</linkedTo>"), std::string::npos);
    EXPECT_NE(shifted.find("<linkedTo>H_H</linkedTo>"), std::string::npos);
    EXPECT_EQ(shifted.find("<linkedTo>D_D</linkedTo>"), std::string::npos);
    removed = source->note_parentheses().back();
    source->remove_note_parenthesis_info(removed);
    delete removed;
    const auto unmatched = legacy_override_write(clone);
    EXPECT_EQ(unmatched.substr(unmatched.find("<NoteParenGroup>")).find("<linkedTo>"), std::string::npos);
}

TEST(Read, RemovingParenthesisGroupRebuildsShapeAndClearsItsOffset) {
    using namespace mu::engraving;
    auto xml = parenthesis_link_fixture();
    xml.insert(xml.find("<eid>D_D</eid>") + 14, "<visible>0</visible>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_TRUE((*opened)->edit({{engrave::SetProperty{engrave::from_engine(chord->find_note(60)->eid()), "visible", false},
                                  engrave::SetProperty{engrave::from_engine(chord->find_note(60)->eid()), "visible", true}}}));
    ASSERT_TRUE((*opened)->page_count());
    EXPECT_TRUE(chord->layout_parentheses().front().left->visible());
    chord->find_note(60)->set_parentheses_mode(ParenthesesMode::NONE, false);
    ASSERT_TRUE((*opened)->page_count());
    ASSERT_EQ(chord->layout_parentheses().size(), 1u);
    EXPECT_EQ(chord->layout_parentheses().front().left->offset(), muse::PointF());
    EXPECT_EQ(chord->layout_parentheses().front().info.notes().front()->pitch(), 64);
}

TEST(Read, ParenthesisMembershipKeepsColdPartGroupsIndependent) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(parenthesis_link_fixture()));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    const auto outline = (*opened)->outline();
    ASSERT_TRUE(outline);
    ASSERT_TRUE((*opened)->edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
    auto *source = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *part = master.excerpts().front()->excerpt_score();
    auto *clone = to_chord(part->first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_TRUE((*opened)->page_count());
    const AuthoredChildKey key{AuthoredChildRole::PAREN_LEFT, 0, 0, 1};
    source->set_stem_user_length(Spatium(0));
    std::erase_if(source->authored_overrides.entries, [](const auto &entry) { return entry.child.role == AuthoredChildRole::STEM; });
    EXPECT_EQ(legacy_override_write(clone).find("<linkedTo>F_F</linkedTo>"), std::string::npos);
    const_cast<NoteParenthesisInfo *>(clone->note_parentheses().front())->insert_note(clone->find_note(64));
    source->find_note(60)->set_parentheses_mode(ParenthesesMode::NONE);
    ASSERT_EQ(clone->note_parentheses().front()->notes().size(), 2u);
    EXPECT_EQ(clone->note_parentheses().front()->notes().front()->pitch(), 60);
    EXPECT_EQ(clone->note_parentheses().front()->notes().back()->pitch(), 64);
}

TEST(Read, CheckpointAfterRemovingRenderedGhostDoesNotReadCachedMembership) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    xml.insert(xml.find("<Note>") + 6, "<ghost>1</ghost>");
    xml.insert(xml.find("</Note>") + 7, "<Note><pitch>64</pitch><tpc>18</tpc></Note>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_TRUE((*opened)->page_count());
    auto *ghost = chord->find_note(60);
    ASSERT_NE(ghost, nullptr);
    const auto id = engrave::from_engine(ghost->eid());
    ASSERT_TRUE((*opened)->edit({{engrave::Remove{engrave::Ref{id}}}}));
    ASSERT_TRUE((*opened)->checkpoint("removed ghost"));
    ASSERT_TRUE((*opened)->restore_checkpoint("removed ghost"));
    auto *restored = to_chord(master_of(**opened).first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_EQ(restored->notes().size(), 1u);
    EXPECT_EQ(restored->notes().front()->pitch(), 64);
    EXPECT_TRUE(restored->note_parentheses().empty());
}

TEST(Read, PublicTupletNumberStyleSavesAndReopensCold) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(s11_triplet_fixture()));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    auto *tuplet = chord->tuplet();
    ASSERT_NE(tuplet, nullptr);
    ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(tuplet->eid()), "FONT_SIZE", 17.0}}}));
    EXPECT_EQ(tuplet->number(), nullptr);
    auto saved = document.export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    EXPECT_EQ(tuplet->number(), nullptr);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    auto &copy = master_of(**reopened);
    auto *number_owner = to_chord(copy.first_measure()->first(SegmentType::CHORD_REST)->element(0))->tuplet();
    ASSERT_NE(number_owner, nullptr);
    EXPECT_EQ(number_owner->number(), nullptr);
    EXPECT_DOUBLE_EQ(number_owner->get_property(Pid::FONT_SIZE).to_double(), 17.0);
    EXPECT_EQ(number_owner->authored_overrides.find({AuthoredChildRole::TUPLET_NUMBER}, Pid::FONT_SIZE), nullptr);
    ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_NE(number_owner->number(), nullptr);
    EXPECT_DOUBLE_EQ(number_owner->number()->size(), 17.0);
}

TEST(Read, StyleChangesDoNotAuthorOutputOverrides) {
    using namespace mu::engraving;
    auto opened = g_engine->open(as_bytes(s11_triplet_fixture()));
    ASSERT_TRUE(opened);
    auto &document = **opened;
    auto &master = master_of(document);
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    ASSERT_TRUE(document.edit({{engrave::SetStyle{{}, "stemWidth", 0.1875}}}));
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    master.scan_elements([](EngravingItem *item) {
        if (const auto *table = authored_override_table(item)) EXPECT_TRUE(table->entries.empty());
    });
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_NE(chord->stem(), nullptr);
    EXPECT_DOUBLE_EQ(chord->stem()->line_width().val(), 0.1875);
    ASSERT_NE(chord->tuplet(), nullptr);
    EXPECT_TRUE(chord->tuplet()->authored_overrides.entries.empty());
}

TEST(Read, ColdChildPositionResetAndRecreationRestorePinnedDefaults) {
    using namespace mu::engraving;
    for (const bool warm : {false, true}) {
        auto opened = g_engine->open(as_bytes(s11_triplet_fixture(true)));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        const AuthoredChildKey key{AuthoredChildRole::STEM};
        ASSERT_NE(chord->authored_overrides.find(key, Pid::OFFSET), nullptr);
        EXPECT_EQ(chord->authored_overrides.find(key, Pid::OFFSET)->value, PropertyValue(PointF(0.75, -0.5)));
        if (warm) {
            ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
            EXPECT_EQ(chord->stem()->offset() / master.style().spatium(), PointF(0.75, -0.5));
        }
        Reset::reset_all_positions(master.transaction_manager()->current_or_dummy_transaction(), &master);
        auto saved = document.export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        auto &copy = master_of(**reopened);
        auto *restored = to_chord(copy.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        EXPECT_EQ(restored->stem(), nullptr);
        EXPECT_EQ(restored->authored_overrides.find(key, Pid::OFFSET), nullptr);
        for (int pass = 0; pass != 2; ++pass) {
            ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
            ASSERT_NE(restored->stem(), nullptr);
            EXPECT_EQ(restored->stem()->offset(), PointF());
            EXPECT_DOUBLE_EQ(restored->stem()->line_width().val(), 0.125);
            engrave::DocumentAccess::impl(**reopened).discard_layout_output(copy);
        }
    }
}

TEST(Read, GenericRestParenthesesKeepAuthoredColorWithoutTableCarriers) {
    using namespace mu::engraving;
    const std::string fixture = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><eid>R_R</eid><durationType>whole</durationType><Parenthesis><horizontalDirection>left</horizontalDirection>
<color r="24" g="80" b="120" a="255"/><offset x="0.5" y="-0.25"/></Parenthesis></Rest>
</voice></Measure></Staff></Score></museScore>)";
    for (const std::string version : {"4.60", "5.00"}) {
        auto xml = fixture;
        xml.replace(xml.find("5.00"), 4, version);
        auto opened = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(opened);
        auto &document = **opened;
        auto &master = master_of(document);
        auto *rest = to_rest(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engrave::from_engine(rest->eid()), "parentheses", engrave::Enum{"both"}}}}));
        EXPECT_EQ(rest->parentheses_mode(), ParenthesesMode::BOTH);
        auto saved = document.export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        auto &copy = master_of(**reopened);
        auto *restored = to_rest(copy.first_measure()->first(SegmentType::CHORD_REST)->element(0));
        for (const bool render : {false, true}) {
            if (render) ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
            EXPECT_EQ(restored->parentheses_mode(), ParenthesesMode::BOTH);
            ASSERT_NE(restored->left_paren(), nullptr);
            ASSERT_NE(restored->right_paren(), nullptr);
            EXPECT_EQ(restored->left_paren()->color(), muse::draw::Color(24, 80, 120));
            EXPECT_EQ(restored->left_paren()->offset() / copy.style().spatium(), PointF(0.5, -0.25));
            EXPECT_EQ(restored->left_paren()->authored_child.role, AuthoredChildRole::NONE);
            for (auto role : {AuthoredChildRole::PAREN_LEFT, AuthoredChildRole::PAREN_RIGHT}) {
                EXPECT_EQ(restored->authored_overrides.carrier({role}), nullptr);
                EXPECT_FALSE(restored->authored_overrides.has_entries({role}));
            }
        }
    }
}

TEST(Read, GenericParenthesisPositionsResetColdAndWarmWithoutClearingAppearance) {
    using namespace mu::engraving;
    const std::string fixture = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>whole</durationType>
<Parenthesis><color r="24" g="80" b="120" a="255"/><offset x="0.5" y="-0.25"/><autoplace>0</autoplace></Parenthesis>
<Parenthesis><horizontalDirection>right</horizontalDirection><offset x="-1.25" y="0.75"/></Parenthesis>
</Rest></voice></Measure></Staff></Score></museScore>)";
    for (const std::string version : {"4.60", "5.00"}) {
        for (const bool warm : {false, true}) {
            auto xml = fixture;
            xml.replace(xml.find("5.00"), 4, version);
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            auto &master = master_of(**opened);
            auto *rest = to_rest(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
            ASSERT_NE(rest->left_paren(), nullptr);
            ASSERT_NE(rest->right_paren(), nullptr);
            EXPECT_EQ(rest->left_paren()->offset() / master.style().spatium(), PointF(0.5, -0.25));
            EXPECT_EQ(rest->right_paren()->offset() / master.style().spatium(), PointF(-1.25, 0.75));
            EXPECT_FALSE(rest->left_paren()->autoplace());
            if (warm) ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
            Reset::reset_all_positions(master.transaction_manager()->current_or_dummy_transaction(), &master);
            auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
            ASSERT_TRUE(saved);
            auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
            ASSERT_TRUE(reopened);
            auto &copy = master_of(**reopened);
            auto *restored = to_rest(copy.first_measure()->first(SegmentType::CHORD_REST)->element(0));
            for (int pass = 0; pass != 2; ++pass) {
                EXPECT_EQ(restored->parentheses_mode(), ParenthesesMode::BOTH);
                ASSERT_NE(restored->left_paren(), nullptr);
                ASSERT_NE(restored->right_paren(), nullptr);
                EXPECT_EQ(restored->left_paren()->offset(), PointF());
                EXPECT_EQ(restored->right_paren()->offset(), PointF());
                EXPECT_TRUE(restored->left_paren()->autoplace());
                EXPECT_EQ(restored->left_paren()->color(), muse::draw::Color(24, 80, 120));
                EXPECT_FALSE(restored->authored_overrides.has_entries({AuthoredChildRole::PAREN_LEFT}));
                EXPECT_FALSE(restored->authored_overrides.has_entries({AuthoredChildRole::PAREN_RIGHT}));
                ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
                engrave::DocumentAccess::impl(**reopened).discard_layout_output(copy);
            }
        }
    }
}

TEST(Read, GenericParenthesisVisibilityIsIndependentOfOwnerTagOrder) {
    using namespace mu::engraving;
    for (const std::string version : {"4.60", "5.00"}) {
        for (const bool child_visible : {false, true}) {
            for (const bool owner_first : {false, true}) {
                const auto owner = std::string("<visible>") + (child_visible ? "0" : "1") + "</visible>";
                const auto child = std::string("<Parenthesis><visible>") + (child_visible ? "1" : "0") + "</visible></Parenthesis>";
                const auto xml = std::string("<museScore version=\"") + version + R"("><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>whole</durationType>)" +
                                 (owner_first ? owner + child : child + owner) + "</Rest></voice></Measure></Staff></Score></museScore>";
                auto opened = g_engine->open(as_bytes(xml));
                ASSERT_TRUE(opened);
                const auto check = [&](MasterScore &score) {
                    const auto *rest = to_rest(score.first_measure()->first(SegmentType::CHORD_REST)->element(0));
                    EXPECT_EQ(rest->visible(), !child_visible);
                    EXPECT_EQ(rest->parentheses_mode(), ParenthesesMode::LEFT);
                    ASSERT_NE(rest->left_paren(), nullptr);
                    EXPECT_EQ(rest->left_paren()->visible(), child_visible);
                    EXPECT_FALSE(rest->authored_overrides.has_entries({AuthoredChildRole::PAREN_LEFT}));
                };
                check(master_of(**opened));
                auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
                ASSERT_TRUE(saved);
                auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
                ASSERT_TRUE(reopened);
                check(master_of(**reopened));
                ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
                check(master_of(**reopened));
            }
        }
    }
}

TEST(Read, LinkedRestParenthesesSaveAfterPublicMasterAndPartEdits) {
    using namespace mu::engraving;
    const std::string fixture = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>whole</durationType></Rest></voice></Measure></Staff></Score></museScore>)";
    for (const bool from_part : {false, true}) {
        auto opened = g_engine->open(as_bytes(fixture));
        ASSERT_TRUE(opened);
        auto &master = master_of(**opened);
        const auto outline = (*opened)->outline();
        ASSERT_TRUE(outline);
        ASSERT_TRUE((*opened)->edit({{engrave::GenerateExcerpts{{outline->parts.front().id}}}}));
        auto *part = master.excerpts().front()->excerpt_score();
        auto *score = from_part ? part : &master;
        auto *rest = to_rest(score->first_measure()->first(SegmentType::CHORD_REST)->element(0));
        ASSERT_TRUE((*opened)->edit({{engrave::SetProperty{engrave::from_engine(rest->eid()), "parentheses", engrave::Enum{"both"}}}}));
        auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto &copy = master_of(**reopened);
        ASSERT_EQ(copy.excerpts().size(), 1u);
        for (auto *linked_score : {static_cast<Score *>(&copy), copy.excerpts().front()->excerpt_score()}) {
            const auto *restored = to_rest(linked_score->first_measure()->first(SegmentType::CHORD_REST)->element(0));
            EXPECT_EQ(restored->parentheses_mode(), ParenthesesMode::BOTH);
            ASSERT_NE(restored->left_paren(), nullptr);
            ASSERT_NE(restored->right_paren(), nullptr);
            EXPECT_TRUE(restored->left_paren()->eid().is_valid());
            EXPECT_TRUE(restored->right_paren()->eid().is_valid());
            EXPECT_EQ(restored->left_paren()->authored_child.role, AuthoredChildRole::NONE);
            EXPECT_FALSE(restored->authored_overrides.has_entries({AuthoredChildRole::PAREN_LEFT}));
            EXPECT_FALSE(restored->authored_overrides.has_entries({AuthoredChildRole::PAREN_RIGHT}));
        }
        ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
    }
}

TEST(Read, OwnerColorKeepsGeneratedStemAndDotDefaultsAfterColdSave) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    const auto at = xml.find("<durationType>half</durationType>");
    xml.replace(at, 33, "<durationType>quarter</durationType><dots>1</dots>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_TRUE((*opened)->edit({{engrave::SetProperty{engrave::from_engine(chord->eid()), "color", engrave::Color{24, 80, 120}}}}));
    auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    auto &copy = master_of(**reopened);
    auto *restored = to_chord(copy.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    EXPECT_EQ(restored->notes().front()->color(), muse::draw::Color(24, 80, 120));
    for (int cycle = 0; cycle != 2; ++cycle) {
        ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(restored->stem(), nullptr);
        EXPECT_EQ(restored->stem()->color(), muse::draw::Color(0, 0, 0));
        auto *note = restored->notes().front();
        ASSERT_NE(note->dot(0), nullptr);
        EXPECT_EQ(note->dot(0)->color(), muse::draw::Color(0, 0, 0));
        EXPECT_EQ(note->authored_overrides.find({AuthoredChildRole::DOT}, Pid::COLOR), nullptr);
        EXPECT_EQ(restored->authored_overrides.find({AuthoredChildRole::STEM}, Pid::COLOR), nullptr);
        engrave::DocumentAccess::impl(**reopened).discard_layout_output(copy);
    }
}

TEST(Read, SegmentTextUsesTheAuthoredTextStyleDefaultsAfterRecreation) {
    using namespace mu::engraving;
    auto xml = legacy_segment_fixture("");
    xml.insert(xml.find("<Note>") + 6, "<eid>N_N</eid>");
    xml.insert(xml.rfind("<Note>") + 6, "<eid>O_O</eid>");
    xml.insert(xml.rfind("</Note>"), R"(<GuitarBend><eid>F_F</eid><track>0</track>
<startElement>N_N</startElement><endElement>O_O</endElement><Segment/></GuitarBend>)");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *bend = static_cast<GuitarBend *>(master.eid_register()->item_from_eid(EID::from_std_string(std::string_view("F_F"))));
    ASSERT_NE(bend, nullptr);
    ASSERT_TRUE((*opened)->edit({{engrave::SetStyle{{}, "titleFontSize", 23.0}, engrave::SetStyle{{}, "bendFontSize", 11.0}}}));
    const AuthoredChildKey text{AuthoredChildRole::SEGMENT_TEXT};
    ASSERT_TRUE(set_child_override(*bend, text, Pid::TEXT_STYLE, PropertyValue::from_value(TextStyleType::TITLE), PropertyFlags::NOSTYLE));
    auto saved = (*opened)->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    auto &copy = master_of(**reopened);
    auto *restored = static_cast<GuitarBend *>(copy.eid_register()->item_from_eid(EID::from_std_string(std::string_view("F_F"))));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->authored_overrides.find(text, Pid::FONT_SIZE), nullptr);
    for (int pass = 0; pass != 2; ++pass) {
        ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
        ASSERT_NE(restored->front_segment(), nullptr);
        const auto *label = to_guitar_bend_segment(restored->front_segment())->bend_text();
        EXPECT_EQ(label->text_style_type(), TextStyleType::TITLE);
        EXPECT_DOUBLE_EQ(label->size(), 23.0);
        engrave::DocumentAccess::impl(**reopened).discard_layout_output(copy);
    }
}
