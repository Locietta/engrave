#include "engraving/dom/dummyparent.h"
#include "global/hashmap.h"
#include "handles_test_fixture.h"

using namespace handles_test;

TEST(HarmonyNames, ColdS13NamesEqualRenderedText) {
    using namespace mu::engraving;
    for (const auto name : {"harmony-display.mscx", "fbox.mscx", "golden/layout_elements.mscx"}) {
        for (const auto capo : {DisplayCapoChordType::CONCERT, DisplayCapoChordType::BOTH, DisplayCapoChordType::TRANSPOSED}) {
            SCOPED_TRACE(name);
            SCOPED_TRACE(static_cast<int>(capo));
            auto document = open_path(g_source_root / "tests/data/inputs" / name);
            ASSERT_TRUE(document);
            auto &master = master_of(*document);
            master.style().set(Sid::CAPO_POSITION, 2);
            master.style().set(Sid::DISPLAY_CAPO_CHORDS, static_cast<int>(capo));
            muse::HashMap<Harmony *, String> cold;
            for (auto *object : engrave::document_objects(master)) {
                if (object->is_harmony()) cold.emplace(to_harmony(object), to_harmony(object)->display_name());
            }
            ASSERT_FALSE(cold.empty());
            expect_no_layout(master);
            ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
            for (const auto &[harmony, expected] : cold) {
                // This is the pre-S13 display_text calculation over actual rendered items.
                String rendered;
                for (const auto *item : harmony->ldata()->render_item_list()) {
                    if (const auto *text = dynamic_cast<const TextSegment *>(item)) {
                        String value = text->text();
                        value.replace(u"\uE87C", u"/");
                        rendered += value;
                    } else if (const auto *paren = dynamic_cast<const ChordSymbolParen *>(item)) {
                        rendered += paren->paren_item->direction() == DirectionH::LEFT ? u"(" : u")";
                    }
                }
                EXPECT_EQ(expected, rendered);
                EXPECT_EQ(harmony->display_text(), rendered);
            }
        }
    }
}

TEST(MmrOutput, InternalLockKeepsSourceIdentityWithoutAnInternalDisplayBoundary) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/mmr-boundary.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *second = master.first_measure()->next_measure();
    auto *last = master.last_measure();
    auto *lock = new RangeLock(second, last);
    master.add_system_lock(lock);
    EXPECT_TRUE(second->is_start_of_system_lock());
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_NE(master.first_measure()->mm_rest(), nullptr);
    EXPECT_EQ(master.first_measure()->mm_rest()->system_lock(), nullptr);
    EXPECT_EQ(last->covering_mm_rest_or_this()->system_lock(), nullptr);
    EXPECT_EQ(lock->start_mb(), second);
    EXPECT_EQ(lock->end_mb(), last);
    master.layout_data_store()->clear();
    EXPECT_EQ(master.system_locks()->all_locks().front(), lock);
    EXPECT_EQ(lock->start_mb(), second);
}

TEST(MmrOutput, AuthoredStartClefIsCopiedAtTheDisplayStart) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/mmr-boundary.mscx");
    const auto position = xml.find("<clefToBarlinePos>1</clefToBarlinePos>");
    ASSERT_NE(position, std::string::npos);
    xml.replace(position, std::string("<clefToBarlinePos>1</clefToBarlinePos>").size(), "<clefToBarlinePos>2</clefToBarlinePos>");
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *third = master.first_measure()->next_measure()->next_measure();
    auto *source = third->find_segment_r(SegmentType::CLEF, Fraction(0, 1));
    ASSERT_NE(source, nullptr);
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
    auto *copy = master.layout_copy(*source->element(0));
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->find_measure(), third->mm_rest());
    EXPECT_EQ(copy->tick(), third->tick());
    EXPECT_EQ(to_clef(copy)->clef_type(), ClefType::F);
    EXPECT_GT(copy->width(), 0.0);
}

TEST(MmrOutput, AuthoredEndKeyRemainsAtTheDisplayEnd) {
    using namespace mu::engraving;
    const auto rest = "<Rest><durationType>measure</durationType></Rest>";
    const auto key = [](int value) {
        return std::format("<KeySig><concertKey>{0}</concertKey><actualKey>{0}</actualKey></KeySig>", value);
    };
    auto xml = std::string(
        R"(<museScore version="5.00"><Score><Division>480</Division><Style><createMultiMeasureRests>1</createMultiMeasureRests><changesBeforeBarlineRepeats>1</changesBeforeBarlineRepeats></Style><Part><Staff id="1"/><Instrument><trackName>Piano</trackName></Instrument></Part><Staff id="1">)");
    xml += "<Measure><startRepeat/><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>" + key(2) + rest + "</voice></Measure>";
    xml += "<Measure><voice>" + key(0) + rest + "</voice></Measure>";
    xml += std::string("<Measure><voice>") + rest + "</voice></Measure>";
    xml += std::string("<Measure><endRepeat>2</endRepeat><voice>") + rest + key(2) + "</voice></Measure>";
    xml += std::string("<Measure><voice>") + rest + "</voice></Measure></Staff></Score></museScore>";
    auto opened = g_engine->open(as_bytes(xml));
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *first = master.first_measure()->next_measure();
    auto *last = first->next_measure()->next_measure();
    auto *source = last->find_segment_r(SegmentType::KEY_SIG, last->ticks());
    ASSERT_NE(source, nullptr);
    EXPECT_TRUE(source->end_of_measure_change());
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_NE(first->mm_rest(), nullptr);
    EXPECT_EQ(first->mm_rest()->mm_rest_count(), 2);
    ASSERT_NE(last->mm_rest(), nullptr);
    EXPECT_EQ(last->mm_rest()->mm_rest_count(), 1);
    auto *copy = master.layout_copy(*source->element(0));
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->find_measure(), last->mm_rest());
    EXPECT_EQ(copy->tick(), first->tick() + Fraction(3, 1));
    EXPECT_EQ(source->measure(), last);
    EXPECT_EQ(source->rtick(), Fraction(1, 1));
    EXPECT_EQ(to_key_sig(copy)->key(), Key::D);
    EXPECT_GT(copy->width(), 0.0);
}

TEST(MmrOutput, SourceLocksAndBorrowedAnchorsSurviveRangeDissolution) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/mmr-boundary.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure();
    auto *last = master.last_measure();
    master.ensure_layout();
    auto *display = first->mm_rest();
    ASSERT_NE(display, nullptr);
    auto *lock = new RangeLock(display, last->covering_mm_rest_or_this());
    master.add_system_lock(lock);
    EXPECT_EQ(lock->start_mb(), first);
    EXPECT_EQ(lock->end_mb(), last);
    master.invalidate_layout(first->tick(), last->end_tick());
    master.ensure_layout();
    ASSERT_NE(first->mm_rest(), nullptr);
    EXPECT_TRUE(first->mm_rest()->is_start_of_system_lock());
    EXPECT_EQ(first->mm_rest()->system(), last->covering_mm_rest_or_this()->system());
    auto *line = Factory::create_text_line(master.dummy());
    line->set_track(0);
    line->set_tick(Fraction(0, 1));
    line->set_tick2(last->end_tick());
    line->set_start_element(first->mm_rest()->first(SegmentType::CHORD_REST)->effective_element(0));
    master.add_element(line);
    engrave::assign_missing_ids(master);
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, false);
    master.invalidate_layout(first->tick(), last->end_tick());
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(first->mm_rest(), nullptr);
    if (line->start_element()) EXPECT_TRUE(source_object(*line->start_element()));
    master.layout_data_store()->clear();
    EXPECT_EQ(lock->start_mb(), first);
    EXPECT_EQ(lock->end_mb(), last);
    const auto saved = export_mscx(*document);
    EXPECT_NE(saved.find(first->eid().to_std_string()), std::string::npos);
    EXPECT_EQ(saved.find("<multiMeasureRest>"), std::string::npos);
}

TEST(MmrOutput, DestinationSignatureSegmentWinsWithoutMergingTracks) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/mmr-conflict.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *second = master.first_measure()->next_measure();
    auto *third = second->next_measure();
    auto *key = third->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    auto *time = third->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
    ASSERT_NE(key, nullptr);
    ASSERT_NE(time, nullptr);
    EXPECT_EQ(to_key_sig(key->element(0))->key(), Key::D);
    EXPECT_EQ(key->element(4), nullptr);
    EXPECT_EQ(time->element(4), nullptr);
    EXPECT_EQ(second->find_segment_r(SegmentType::KEY_SIG, second->ticks()), nullptr);
    EXPECT_EQ(second->find_segment_r(SegmentType::TIME_SIG, second->ticks()), nullptr);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_NE(third->mm_rest(), nullptr);
    auto *shown_key = third->mm_rest()->find_rendered_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    auto *shown_time = third->mm_rest()->find_rendered_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
    ASSERT_NE(shown_key, nullptr);
    ASSERT_NE(shown_time, nullptr);
    EXPECT_EQ(to_key_sig(shown_key->effective_element(0))->key(), Key::D);
    EXPECT_EQ(shown_key->effective_element(4), nullptr);
    EXPECT_EQ(shown_time->effective_element(4), nullptr);
}

TEST(MmrOutput, BoundaryRenderDiscardAndTogglePreserveSource) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/mmr-boundary.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure();
    auto *second = first->next_measure();
    auto *third = second->next_measure();
    auto *last = third->next_measure();
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(first->mm_rest(), nullptr);
    EXPECT_EQ(last->next(), nullptr);
    auto *key = third->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    auto *time = third->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
    auto *clef = second->find_segment_r(SegmentType::CLEF, second->ticks());
    ASSERT_NE(key, nullptr);
    ASSERT_NE(time, nullptr);
    ASSERT_NE(clef, nullptr);
    EXPECT_EQ(to_key_sig(key->element(0))->key(), Key::D);
    EXPECT_EQ(to_clef(clef->element(0))->clef_type(), ClefType::F);
    const auto objects = engrave::document_objects(master);
    std::vector<Segment *> segments;
    for (auto *m = first; m; m = m->next_measure())
        for (auto *segment = m->first(); segment; segment = segment->next()) segments.push_back(segment);
    const auto write = [&] {
        muse::io::Buffer buffer;
        buffer.open(muse::io::IODevice::WRITE_ONLY);
        rw::WriteInOutData inout(&master);
        inout.ctx.set_inline_style(true);
        EXPECT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
        return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
    };
    const auto cold = write();
    for (const auto *tag : {"<multiMeasureRest>", "<mmRestLast>", "<MMRest>", "<MMRestRange>"})
        EXPECT_EQ(cold.find(tag), std::string::npos);
    std::vector<std::vector<std::byte>> baseline;
    for (int cycle = 0; cycle < 2; ++cycle) {
        auto svg = document->render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(svg) << svg.error().message;
        ASSERT_FALSE(svg->pages.empty());
        auto *display = first->mm_rest();
        ASSERT_NE(display, nullptr);
        EXPECT_EQ(display->mm_rest_count(), 2);
        EXPECT_EQ(display->mm_rest_first(), first);
        EXPECT_EQ(display->mm_rest_last(), second);
        EXPECT_EQ(first->next(), second);
        EXPECT_EQ(second->prev(), first);
        EXPECT_EQ(second->next(), third);
        EXPECT_EQ(third->prev(), second);
        EXPECT_FALSE(source_object(*display));
        EXPECT_EQ(engrave::id_class(*display->first(SegmentType::CHORD_REST)), IdClass::LAYOUT);
        auto *rest = display->first(SegmentType::CHORD_REST)->effective_element(0);
        ASSERT_NE(rest, nullptr);
        EXPECT_TRUE(rest->is_mm_rest());
        EXPECT_GT(rest->width(), 0.0);
        const auto *shown_clef = master.layout_copy(*clef->element(0));
        ASSERT_NE(shown_clef, nullptr);
        EXPECT_EQ(shown_clef->find_measure(), display);
        EXPECT_EQ(shown_clef->tick(), display->end_tick());
        EXPECT_GT(shown_clef->width(), 0.0);
        for (auto *signature : {key->element(0), time->element(0)}) {
            auto *copy = master.layout_copy(*signature);
            ASSERT_NE(copy, nullptr);
            EXPECT_EQ(copy->find_measure(), third->mm_rest());
            EXPECT_GT(copy->width(), 0.0);
        }
        EXPECT_EQ(write(), cold);
        EXPECT_EQ(engrave::document_objects(master), objects);
        std::vector<Segment *> current;
        for (auto *m = first; m; m = m->next_measure())
            for (auto *segment = m->first(); segment; segment = segment->next()) current.push_back(segment);
        EXPECT_EQ(current, segments);
        if (cycle == 0)
            baseline = svg->pages;
        else
            EXPECT_EQ(svg->pages, baseline);
        master.layout_data_store()->clear();
        EXPECT_EQ(first->mm_rest(), nullptr);
        EXPECT_EQ(master.layout_copy(*clef->element(0)), nullptr);
        EXPECT_EQ(write(), cold);
    }
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, false);
    master.invalidate_layout(third->tick(), third->end_tick());
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(first->mm_rest(), nullptr);
    master.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
    master.invalidate_layout(third->tick(), third->end_tick());
    auto ranged = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(ranged);
    master.layout_data_store()->clear();
    auto full = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(full);
    EXPECT_EQ(ranged->pages, full->pages);
    EXPECT_EQ(full->pages, baseline);
}

TEST(SegmentOutput, MixedBarlineKeepsAuthoredContentAndSourceIdentity) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    auto *end = measure->find_segment_r(SegmentType::END_BAR_LINE, measure->ticks());
    ASSERT_NE(end, nullptr);
    ASSERT_NE(end->element(0), nullptr);
    ASSERT_EQ(end->annotations().size(), 1u);
    EXPECT_TRUE(end->annotations().front()->is_fermata());
    EXPECT_EQ(to_bar_line(end->element(0))->bar_line_type(), BarLineType::DOUBLE);
    EXPECT_EQ(to_bar_line(end->element(0))->span_from(), 1);
    EXPECT_EQ(to_bar_line(end->element(0))->span_to(), 2);
    EXPECT_EQ(end->element(4), nullptr);
    std::vector<Segment *> source;
    for (auto *m = master.first_measure(); m; m = m->next_measure())
        for (auto *s = m->first(); s; s = s->next()) source.push_back(s);
    const auto objects = engrave::document_objects(master);
    auto authored_query = document->query({.types = {"BarLine", "Fermata"}});
    ASSERT_TRUE(authored_query);
    ASSERT_EQ(authored_query->elements.size(), 2u);
    const auto write = [&] {
        muse::io::Buffer buffer;
        buffer.open(muse::io::IODevice::WRITE_ONLY);
        rw::WriteInOutData inout(&master);
        inout.ctx.set_inline_style(true);
        EXPECT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
        return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
    };
    const auto cold = write();
    EXPECT_NE(cold.find("<subtype>double</subtype>"), std::string::npos);
    EXPECT_NE(cold.find("<Fermata>"), std::string::npos);
    for (int cycle = 0; cycle < 2; ++cycle) {
        auto rendered = document->render({.format = engrave::RenderFormat::SVG});
        ASSERT_TRUE(rendered) << rendered.error().message;
        ASSERT_NE(end->effective_element(4), nullptr);
        EXPECT_TRUE(end->effective_element(4)->generated());
        EXPECT_GT(end->effective_element(4)->width(), 0.0);
        EXPECT_EQ(end->element(4), nullptr);
        EXPECT_FALSE(source_object(*end->effective_element(4)));
        EXPECT_EQ(write(), cold);
        std::vector<Segment *> current;
        for (auto *m = master.first_measure(); m; m = m->next_measure())
            for (auto *s = m->first(); s; s = s->next()) current.push_back(s);
        EXPECT_EQ(current, source);
        EXPECT_EQ(engrave::document_objects(master), objects);
        auto query = document->query({.types = {"BarLine", "Fermata"}});
        ASSERT_TRUE(query);
        ASSERT_EQ(query->elements.size(), 2u);
        for (size_t i = 0; i < query->elements.size(); ++i) EXPECT_EQ(query->elements[i].id, authored_query->elements[i].id);
        master.layout_data_store()->clear();
        EXPECT_EQ(write(), cold);
        EXPECT_EQ(engrave::document_objects(master), objects);
    }
}

TEST(SegmentOutput, AuthoredKeyAtOutputNodeMatchesFullLayout) {
    using namespace mu::engraving;
    auto ranged = open_path(g_source_root / "tests/data/inputs/segment-range.mscx");
    auto full = open_path(g_source_root / "tests/data/inputs/segment-range.mscx");
    ASSERT_TRUE(ranged);
    ASSERT_TRUE(full);
    auto &range_master = master_of(*ranged);
    range_master.ensure_layout();
    auto *target = range_master.first_measure()->next_measure()->next_measure()->next_measure();
    ASSERT_NE(target->find_rendered_segment_r(SegmentType::KEY_SIG, Fraction(0, 1)), nullptr);
    EXPECT_EQ(target->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1)), nullptr);
    const auto add_key = [](engrave::Document &document) {
        auto &master = master_of(document);
        auto *m = master.first_measure()->next_measure()->next_measure()->next_measure();
        const auto changed =
            document.edit({{engrave::SetKeySig{.measure = engine_id(*m), .key = 2, .staff = engine_id(*master.staff(0))}}});
        ASSERT_TRUE(changed) << changed.error().message;
    };
    add_key(*ranged);
    add_key(*full);
    const auto before = range_master.layout_state.range_count;
    auto a = ranged->render({.format = engrave::RenderFormat::SVG});
    auto b = full->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(a) << a.error().message;
    ASSERT_TRUE(b) << b.error().message;
    EXPECT_GT(range_master.layout_state.range_count, before);
    ASSERT_EQ(a->pages.size(), b->pages.size());
    for (size_t page = 0; page < a->pages.size(); ++page) EXPECT_EQ(a->pages[page], b->pages[page]);
    auto *source = target->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    EXPECT_EQ(target->find_rendered_segment_r(SegmentType::KEY_SIG, Fraction(0, 1)), source);
    EXPECT_FALSE(target->owns_output_segment(source));
    EXPECT_EQ(to_key_sig(source->element(0))->key(), Key::D);
}

TEST(SegmentOutput, RemovingKeyEventRemovesAuthoredSignature) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure()->next_measure();
    auto *segment = measure->get_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    auto *key = Factory::create_key_sig(segment);
    key->set_track(0);
    key->set_key(Key::D);
    segment->add(key);
    apply_change_key_sig(key, key->key_sig_event(), key->show_courtesy(), false);
    EXPECT_EQ(measure->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1)), nullptr);
    EXPECT_EQ(master.staff(0)->key(measure->tick()), Key::C);
    master.ensure_layout();
    EXPECT_EQ(measure->find_segment_r(SegmentType::KEY_SIG, Fraction(0, 1)), nullptr);
    auto *header = measure->find_rendered_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(to_key_sig(header->effective_element(0))->key(), Key::C);
}

TEST(SegmentOutput, ResizedMeasureRetiresOldBoundary) {
    using namespace mu::engraving;
    auto ranged = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    auto full = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(ranged);
    ASSERT_TRUE(full);
    auto &master = master_of(*ranged);
    master.ensure_layout();
    const auto shorten = [](engrave::Document &document) {
        auto &score = master_of(document);
        auto *measure = score.first_measure()->next_measure();
        ASSERT_TRUE(document.edit({{engrave::SetProperty{engine_id(*measure), "timesigActual", engrave::Fraction{3, 4}}}}));
        for (auto *item : measure->first(SegmentType::CHORD_REST)->elist())
            if (item && item->is_chord_rest()) to_chord_rest(item)->set_ticks(mu::engraving::Fraction(3, 4));
        score.invalidate_layout(measure->tick(), measure->end_tick());
    };
    shorten(*ranged);
    shorten(*full);
    auto actual = ranged->render({.format = engrave::RenderFormat::SVG});
    auto expected = full->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(actual);
    ASSERT_TRUE(expected);
    EXPECT_EQ(actual->pages, expected->pages);
    auto *measure = master.first_measure()->next_measure();
    size_t boundaries = 0;
    for (auto *seg = measure->rendered_first(); seg; seg = seg->rendered_next()) {
        if (!seg->is_end_bar_line_type()) continue;
        ++boundaries;
        EXPECT_EQ(seg->rtick(), mu::engraving::Fraction(3, 4));
    }
    EXPECT_EQ(boundaries, 1u);
}

TEST(SegmentOutput, ImplicitWrittenKeyAndGeneratedRemovalStayOutOfSource) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.parts().front()->instrument()->set_transpose(Interval(-1, -2));
    master.style().set(Sid::CONCERT_PITCH, false);
    master.ensure_layout();
    const auto check = [&](Key expected) {
        auto *measure = master.first_measure();
        EXPECT_EQ(measure->find_segment_r(SegmentType::KEY_SIG, mu::engraving::Fraction(0, 1)), nullptr);
        EXPECT_TRUE(master.staff(0)->key_list()->empty());
        auto *segment = measure->find_rendered_segment_r(SegmentType::KEY_SIG, mu::engraving::Fraction(0, 1));
        ASSERT_NE(segment, nullptr);
        auto *key = to_key_sig(segment->effective_element(0));
        ASSERT_NE(key, nullptr);
        EXPECT_EQ(key->key(), expected);
        apply_change_key_sig(key, key->key_sig_event(), key->show_courtesy(), false);
        EXPECT_EQ(segment->effective_element(0), key);
    };
    check(Key::D);
    master.cmd_concert_pitch_changed(true);
    master.ensure_layout();
    check(Key::C);
    master.cmd_concert_pitch_changed(false);
    master.ensure_layout();
    check(Key::D);
}

TEST(SegmentOutput, RepeatDisplayKeepsAuthoredBarline) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    measure->set_repeat_end(true);
    auto *barline = to_bar_line(measure->find_segment_r(SegmentType::END_BAR_LINE, measure->ticks())->element(0));
    const auto write = [&] {
        muse::io::Buffer buffer;
        buffer.open(muse::io::IODevice::WRITE_ONLY);
        rw::WriteInOutData inout(&master);
        EXPECT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
        return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
    };
    const auto cold = write();
    master.ensure_layout();
    EXPECT_EQ(barline->bar_line_type(), BarLineType::DOUBLE);
    EXPECT_FALSE(barline->generated());
    EXPECT_EQ(barline->display_bar_line_type(), BarLineType::END_REPEAT);
    EXPECT_EQ(write(), cold);
}

TEST(SegmentOutput, OutputHeaderOrdersKeysBeforeAuthoredClef) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *first = master.first_measure();
    auto *second = first->next_measure();
    auto changed = document->edit({{engrave::SetKeySig{.measure = engine_id(*first), .key = 2, .staff = engine_id(*master.staff(0))}}});
    ASSERT_TRUE(changed);
    first->set_repeat_end(true);
    second->set_repeat_start(true);
    master.style().set(Sid::CHANGES_BETWEEN_END_START_REPEAT, true);
    auto *source = second->get_segment_r(SegmentType::CLEF, mu::engraving::Fraction(0, 1));
    auto *clef = Factory::create_clef(source);
    clef->set_track(0);
    clef->set_clef_type(ClefType::F);
    clef->set_clef_to_barline_position(ClefToBarlinePosition::AFTER);
    source->add(clef);
    master.ensure_layout();
    int header = -1, key = -1, authored = -1, index = 0;
    for (auto *segment = second->rendered_first(); segment; segment = segment->rendered_next(), ++index) {
        if (segment->is_header_clef_type()) header = index;
        if (segment->is_key_sig_type()) key = index;
        if (segment == source) authored = index;
    }
    ASSERT_GE(header, 0);
    ASSERT_GE(key, 0);
    ASSERT_GE(authored, 0);
    EXPECT_LT(header, key);
    EXPECT_LT(key, authored);
}

TEST(SegmentOutput, AuthoredCourtesiesSurviveSuppressedDisplay) {
    using namespace mu::engraving;
    auto source = read_text(g_source_root / "tests/data/inputs/segment-purity.mscx");
    source.insert(source.find("</voice>"), R"(<TimeSig><isCourtesy>1</isCourtesy><sigN>3</sigN><sigD>4</sigD></TimeSig>
<KeySig><isCourtesy>1</isCourtesy><concertKey>2</concertKey></KeySig>)");
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    auto *measure = master.first_measure();
    auto *time = measure->find_segment_r(SegmentType::TIME_SIG_ANNOUNCE, measure->ticks());
    auto *key = measure->find_segment_r(SegmentType::KEY_SIG_ANNOUNCE, measure->ticks());
    ASSERT_NE(time, nullptr);
    ASSERT_NE(key, nullptr);
    auto *authored_time = time->element(0);
    auto *authored_key = key->element(0);
    ASSERT_NE(authored_time, nullptr);
    ASSERT_NE(authored_key, nullptr);
    const auto write = [&] {
        muse::io::Buffer buffer;
        buffer.open(muse::io::IODevice::WRITE_ONLY);
        rw::WriteInOutData inout(&master);
        EXPECT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
        return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
    };
    const auto cold = write();
    master.ensure_layout();
    EXPECT_EQ(time->element(0), authored_time);
    EXPECT_EQ(key->element(0), authored_key);
    EXPECT_EQ(write(), cold);
}

TEST(SegmentOutput, NonzeroVoiceSpannerUsesOwnedTimeAnchor) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *line = Factory::create_hairpin(master.dummy());
    line->set_track(1);
    line->set_track2(1);
    line->set_tick(mu::engraving::Fraction(3, 8));
    line->set_ticks(mu::engraving::Fraction(1, 4));
    master.add_spanner(line);
    master.ensure_layout();
    line->compute_start_element();
    line->compute_end_element();
    ASSERT_NE(line->start_element(), nullptr);
    ASSERT_NE(line->end_element(), nullptr);
    EXPECT_TRUE(line->start_element()->is_time_tick_anchor());
    EXPECT_TRUE(line->end_element()->is_time_tick_anchor());
    EXPECT_FALSE(source_object(*line->start_element()));
    auto *measure = master.first_measure();
    EXPECT_EQ(measure->find_segment_r(SegmentType::TIME_TICK, mu::engraving::Fraction(3, 8)), nullptr);
    master.layout_data_store()->clear();
    EXPECT_EQ(line->start_element(), nullptr);
    EXPECT_EQ(line->end_element(), nullptr);
}
