#include <amc/flatset.hpp>
#include <gtl/btree.hpp>

#include "engraving/dom/guitarbend.h"
#include "handles_test_fixture.h"

using namespace handles_test;

TEST(NativeSave, S14StemOverridesSurviveSuppressionSaveAndCheckpoint) {
    using namespace mu::engraving;
    for (const std::string version : {"4.60", "5.00"}) {
        for (bool stem_first : {false, true}) {
            std::string xml = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><eid>B_B</eid><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
</voice></Measure></Staff></Score></museScore>)";
            xml.replace(xml.find("5.00"), 4, version);
            const std::string stem = "<Stem><color r=\"17\" g=\"34\" b=\"51\" a=\"255\"/><userLen>2.5</userLen></Stem>";
            xml.insert(xml.find("<durationType>"), stem_first ? stem + "<noStem>1</noStem>" : "<noStem>1</noStem>" + stem);
            auto opened = g_engine->open(as_bytes(xml));
            ASSERT_TRUE(opened);
            const auto check = [](engrave::Document &document, bool suppressed) {
                auto *chord = to_chord(master_of(document).first_measure()->first(SegmentType::CHORD_REST)->element(0));
                EXPECT_EQ(chord->no_stem(), suppressed);
                EXPECT_DOUBLE_EQ(chord->stem_user_length().val(), 2.5);
                const auto *color = chord->authored_overrides.find({AuthoredChildRole::STEM}, Pid::COLOR);
                ASSERT_NE(color, nullptr);
                EXPECT_EQ(color->value.value<muse::draw::Color>(), muse::draw::Color(17, 34, 51));
            };
            auto &document = **opened;
            check(document, true);
            const auto id =
                engrave::from_engine(to_chord(master_of(document).first_measure()->first(SegmentType::CHORD_REST)->element(0))->eid());
            ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "noStem", false}}}));
            ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "noStem", true}}}));
            ASSERT_TRUE(document.checkpoint("hidden"));
            const auto saved = document.export_score({.format = engrave::Format::MSCX, .native_version = "5.00"});
            ASSERT_TRUE(saved);
            auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
            ASSERT_TRUE(reopened);
            check(**reopened, true);
            ASSERT_TRUE((*reopened)->edit({{engrave::SetProperty{id, "noStem", false}}}));
            check(**reopened, false);
            ASSERT_TRUE(document.restore_checkpoint("hidden"));
            check(document, true);
            ASSERT_TRUE(document.edit({{engrave::SetProperty{id, "noStem", false}}}));
            check(document, false);
            const auto &master = master_of(document);
            EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
        }
    }
}

TEST(NativeSave, ColdEqualsWarm) {
    std::vector<std::filesystem::path> inputs;
    for (const auto &entry : std::filesystem::directory_iterator(g_source_root / "tests/data/inputs/golden")) {
        if (written_by_engine(entry.path()) || entry.path().filename() == "created.json") inputs.push_back(entry.path());
    }
    for (const auto name :
         {"evening.musicxml", "mmr-boundary.mscx", "segment-purity.mscx", "segment-range.mscx", "stave-sharing.mscx",
          "stave-sharing-voices.mscx", "stave-sharing-mmr.mscx", "native-save-overrides.mscx", "native-save-overrides-460.mscx"})
        inputs.push_back(g_source_root / "tests/data/inputs" / name);
    std::ranges::sort(inputs);
    for (const auto &path : inputs) {
        SCOPED_TRACE(path.filename().string());
        auto opened = path.extension() == ".json" ? g_engine->open(as_bytes(composition_input())) : g_engine->open(path);
        ASSERT_TRUE(opened) << opened.error().message;
        auto &document = **opened;
        auto &master = master_of(document);
        expect_no_layout(master);
        const auto membership = [&] {
            gtl::btree_map<ElementId, EngravingObject *> handles;
            for (const auto &entry : engrave::live_ids(master)) handles.emplace(entry.id, entry.object);
            return handles;
        };
        const auto objects = membership();
        const auto cold = native_captures(document);
        EXPECT_EQ(membership(), objects);
        expect_no_layout(master);
        EXPECT_EQ(cold, native_captures(document));
        expect_no_layout(master);
        for (auto *score : native_scores(master)) {
            auto rendered = document.render({.score = score->is_master() ? engrave::ScoreSel{} : engrave::ScoreSel{engine_id(*score)},
                                             .format = engrave::RenderFormat::SVG});
            ASSERT_TRUE(rendered) << rendered.error().message;
            EXPECT_GT(score->layout_state.whole_count, 0u);
            EXPECT_FALSE(score->pages().empty());
        }
        EXPECT_EQ(cold, native_captures(document));
        EXPECT_EQ(membership(), objects);
    }
}

TEST(NativeSave, S13MissingCapoIdentityKeepsCustomFBoxChildren) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/fbox.mscx");
    xml.insert(xml.find("<Division>"), "<Style><displayCapoChords>2</displayCapoChords><capoPosition>0</capoPosition></Style>");
    xml.replace(xml.find("<frets>4</frets>"), 16, "<frets>7</frets><visible>0</visible>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *box = to_f_box(master.first());
    ASSERT_EQ(box->el().size(), 2u);
    EXPECT_EQ(to_fret_diagram(box->el().front())->frets(), 7);
    EXPECT_FALSE(box->el().front()->visible());
    expect_no_layout(master);
    const auto saved = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    auto *copy_box = to_f_box(master_of(**reopened).first());
    ASSERT_EQ(copy_box->el().size(), 2u);
    EXPECT_EQ(to_fret_diagram(copy_box->el().front())->frets(), 7);
    EXPECT_FALSE(copy_box->el().front()->visible());
    ASSERT_TRUE(document->edit({{engrave::SetStyle{{}, "displayCapoChords", std::int32_t{0}}}}));
    const auto children = box->el();
    ASSERT_TRUE(document->edit(
        {{engrave::SetStyle{{}, "displayCapoChords", std::int32_t{2}}, engrave::SetStyle{{}, "capoPosition", std::int32_t{2}}}}));
    EXPECT_EQ(box->el(), children);
    ASSERT_EQ(box->el().size(), 2u);
    EXPECT_EQ(to_fret_diagram(box->el().front())->frets(), 7);
    EXPECT_FALSE(box->el().front()->visible());
    expect_no_layout(master);
}

TEST(NativeSave, S13HiddenFBoxHarmonyDoesNotPaintItsOldLabel) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/fbox.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *box = to_f_box(master.first());
    auto *diagram = to_fret_diagram(box->el().front());
    const auto label_id = engine_id(*diagram->harmony());
    ASSERT_TRUE(document->edit({{engrave::SetProperty{label_id, "color", engrave::Color{255, 0, 0, 255}}}}));
    auto before = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(before);
    ASSERT_NE(text_of(before->pages.front()).find("rgb(100%, 0%, 0%)"), std::string::npos);
    const engrave::Address at = engrave::SegmentAddress{.measure = engine_id(*master.first_measure()), .tick = {0, 1}, .kind = "ChordRest"};
    ASSERT_TRUE(document->edit(
        {{engrave::SetProperty{engine_id(*diagram), "visible", false}, engrave::Add{at, "Harmony", {{"text", std::string("Dm")}}}}}));
    auto hidden = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(hidden);
    EXPECT_EQ(text_of(hidden->pages.front()).find("rgb(100%, 0%, 0%)"), std::string::npos);
    ASSERT_TRUE(document->edit({{engrave::SetProperty{engine_id(*diagram), "visible", true}}}));
    auto visible = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(visible);
    EXPECT_NE(text_of(visible->pages.front()).find("rgb(100%, 0%, 0%)"), std::string::npos);
}

TEST(NativeSave, S13HarmonyCreationParsesTypeBeforeText) {
    using namespace mu::engraving;
    for (bool type_first : {false, true}) {
        auto document = open_path(g_source_root / "tests/data/inputs/fbox.mscx");
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        const engrave::Address at =
            engrave::SegmentAddress{.measure = engine_id(*master.first_measure()), .tick = {0, 1}, .kind = "ChordRest"};
        std::vector<std::pair<std::string, engrave::Value>> props{{"text", std::string("1")}, {"harmonyType", std::int32_t{2}}};
        if (type_first) std::reverse(props.begin(), props.end());
        const auto added = document->edit({{engrave::Add{at, "Harmony", props}}});
        ASSERT_TRUE(added) << added.error().message;
        ASSERT_FALSE(added->rejection);
        ASSERT_EQ(added->ops.front().created.size(), 1u);
        ASSERT_TRUE(added->ops.front().created.front());
        const auto id = *added->ops.front().created.front();
        const auto item = engrave::resolve(master, id);
        ASSERT_TRUE(item);
        auto *harmony = to_harmony(item->object);
        ASSERT_EQ(harmony->chords().size(), 1u);
        EXPECT_EQ(harmony->harmony_type(), HarmonyType::NASHVILLE);
        EXPECT_EQ(harmony->chords().front()->root_tpc(), 14);
        EXPECT_TRUE(harmony->is_realizable());
        EXPECT_EQ(harmony->display_name(), u"1");
        expect_no_layout(master);
        const auto saved = document->export_score({.format = engrave::Format::MSCX});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        const auto found = engrave::resolve(master_of(**reopened), id);
        ASSERT_TRUE(found);
        EXPECT_EQ(to_harmony(found->object)->chords().front()->root_tpc(), 14);
        EXPECT_EQ(to_harmony(found->object)->harmony_type(), HarmonyType::NASHVILLE);
        EXPECT_EQ(to_harmony(found->object)->display_name(), u"1");
        expect_no_layout(master_of(**reopened));
    }
}

TEST(NativeSave, S13EmptyHarmonyCreationIsRejectedBeforePublication) {
    using Props = std::vector<std::pair<std::string, engrave::Value>>;
    for (const Props &props : std::vector<Props>{{},
                                                 {{"text", std::string{}}},
                                                 {{"text", std::string(" \t\n")}},
                                                 {{"text", std::string("()")}},
                                                 {{"text", std::string("|")}}}) {
        auto document = open_path(g_source_root / "tests/data/inputs/fbox.mscx");
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        const auto before = native_captures(*document);
        const engrave::Address at =
            engrave::SegmentAddress{.measure = engine_id(*master.first_measure()), .tick = {0, 1}, .kind = "ChordRest"};
        const auto added = document->edit({{engrave::Add{at, "Harmony", props}}});
        ASSERT_FALSE(added);
        EXPECT_EQ(added.error().code, engrave::ErrorCode::INVALID_ARGUMENT);
        const auto harmonies = document->query({.types = {"Harmony"}});
        ASSERT_TRUE(harmonies);
        EXPECT_EQ(harmonies->total, 4u);
        EXPECT_EQ(document->revision(), 1u);
        EXPECT_EQ(native_captures(*document), before);
        expect_no_layout(master);
    }
}

TEST(NativeSave, FBoxS13SynchronizesColdAndSurvivesRender) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/fbox.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_TRUE(master.first()->is_f_box());
    FBox *box = to_f_box(master.first());
    ASSERT_NE(box, nullptr);
    const auto names = [](const FBox &frame) {
        std::vector<std::string> result;
        for (const auto *child : frame.el()) result.push_back(to_fret_diagram(child)->harmony()->display_name().to_std_string());
        return result;
    };
    EXPECT_EQ(names(*box), (std::vector<std::string>{"C", "G"}));
    EXPECT_FALSE(box->needs_rebuild());
    expect_no_layout(master);
    const engrave::Address at = engrave::SegmentAddress{.measure = engine_id(*master.first_measure()), .tick = {0, 1}, .kind = "ChordRest"};
    const auto added = document->edit({{engrave::Add{at, "Harmony", {{"text", std::string("Dm")}}}}});
    ASSERT_TRUE(added) << added.error().message;
    EXPECT_EQ(names(*box), (std::vector<std::string>{"C", "Dm", "G"}));
    EXPECT_FALSE(box->needs_rebuild());
    expect_no_layout(master);
    const auto saved = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    auto &copy = master_of(**reopened);
    ASSERT_TRUE(copy.first()->is_f_box());
    FBox *copy_box = to_f_box(copy.first());
    ASSERT_NE(copy_box, nullptr);
    EXPECT_EQ(names(*copy_box), (std::vector<std::string>{"C", "Dm", "G"}));
    expect_no_layout(copy);
    const auto children = copy_box->el();
    const auto cold = native_captures(**reopened);
    for (int pass = 0; pass < 2; ++pass) {
        ASSERT_TRUE((*reopened)->render({.format = engrave::RenderFormat::SVG}));
        EXPECT_EQ(copy_box->el(), children);
        EXPECT_EQ(names(*copy_box), (std::vector<std::string>{"C", "Dm", "G"}));
        EXPECT_EQ(cold, native_captures(**reopened));
    }
}

TEST(NativeSave, S13KeepsAuthoredOffsetAndGraceOrderCold) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/layout-scratch.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    Chord *chord = to_chord(master.first_measure()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_NE(chord, nullptr);
    ASSERT_EQ(chord->grace_notes().size(), 3u);
    EXPECT_EQ(chord->grace_notes()[0]->grace_index(), 0u);
    EXPECT_EQ(chord->grace_notes()[1]->grace_index(), 1u);
    EXPECT_EQ(chord->grace_notes()[2]->grace_index(), 2u);
    Note *note = chord->notes().front();
    ASSERT_NE(note->accidental(), nullptr);
    EXPECT_EQ(note->accidental()->stacking_order_offset(), 20);
    EXPECT_EQ(note->user_dot_position(), DirectionV::DOWN);
    const auto cold = native_captures(*document);
    expect_no_layout(master);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    EXPECT_EQ(note->accidental()->stacking_order_offset(), 20);
    EXPECT_EQ(note->accidental()->ldata()->stacking_order_offset, 0);
    EXPECT_EQ(note->user_dot_position(), DirectionV::DOWN);
    EXPECT_EQ(cold, native_captures(*document));
    Chord *removed = chord->grace_notes()[1];
    chord->remove(removed);
    EXPECT_EQ(chord->grace_notes()[0]->grace_index(), 0u);
    EXPECT_EQ(chord->grace_notes()[1]->grace_index(), 1u);
    std::unique_ptr<Chord> copy(Factory::copy_chord(*chord));
    ASSERT_EQ(copy->grace_notes().size(), 2u);
    EXPECT_EQ(copy->grace_notes()[0]->grace_index(), 0u);
    EXPECT_EQ(copy->grace_notes()[1]->grace_index(), 1u);
    removed->set_grace_index(1);
    chord->add(removed);
    EXPECT_EQ(chord->grace_notes()[0]->grace_index(), 0u);
    EXPECT_EQ(chord->grace_notes()[1]->grace_index(), 1u);
    EXPECT_EQ(chord->grace_notes()[2]->grace_index(), 2u);
}

TEST(NativeSave, S13TwoRendersPreserveSourceBytes) {
    using namespace mu::engraving;
    for (const auto name :
         {"harmony-display.mscx", "fbox.mscx", "golden/layout_elements.mscx", "golden/tab_grace_prebend.mscx", "layout-scratch.mscx"}) {
        SCOPED_TRACE(name);
        auto document = open_path(g_source_root / "tests/data/inputs" / name);
        ASSERT_TRUE(document);
        const auto before = native_captures(*document);
        expect_no_layout(master_of(*document));
        for (int pass = 0; pass < 2; ++pass) {
            ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
            EXPECT_EQ(before, native_captures(*document));
        }
    }
}

TEST(NativeSave, TestModeKeepsAuthoredOverridesAndOmitsGeometry) {
    using namespace mu::engraving;

    struct RestoreMode {
        bool previous = MScore::test_mode;

        ~RestoreMode() { MScore::test_mode = previous; }
    } restore;

    MScore::test_mode = true;
    auto document = open_text(R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff id="1"/><Instrument><trackName>Piano</trackName></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Beam><eid>Z_Z</eid><Fragment><y1>-3.25</y1><y2>-2.5</y2></Fragment></Beam>
<Chord><durationType>eighth</durationType><Note><eid>B_B</eid><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><Note><eid>C_C</eid><pitch>62</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>65</pitch><tpc>13</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>67</pitch><tpc>15</tpc></Note>
<TremoloTwoChord><subtype>c16</subtype><Fragment><y1>1.25</y1><y2>2.75</y2></Fragment></TremoloTwoChord></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>69</pitch><tpc>17</tpc></Note></Chord>
</voice></Measure><Measure><voice>
<Chord><durationType>eighth</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>65</pitch><tpc>13</tpc></Note></Chord>
<Rest><durationType>half</durationType></Rest></voice></Measure></Staff><SpannerMap>
<Slur><track>0</track><track2>0</track2><startTick>0/1</startTick><ticks>1/8</ticks>
<SlurSegment no="0"><o2 x="1.5" y="-0.75"/><minDistance>3.25</minDistance></SlurSegment></Slur>
<Pedal><track>0</track><track2>0</track2><startTick>0/1</startTick><ticks>1/2</ticks>
<Segment><subtype>0</subtype><offset x="2" y="1"/><off2 x="-1" y="0.5"/></Segment></Pedal>
</SpannerMap></Score></museScore>)");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto cold = native_captures(*document);
    expect_no_layout(master);
    const auto &xml = cold.begin()->second;
    for (const auto authored : {"<minDistance>3.25</minDistance>", "<y1>-3.25</y1>", "<y2>-2.5</y2>", "<y1>1.25</y1>", "<y2>2.75</y2>",
                                "<o2 x=\"1.5\" y=\"-0.75\"", "<offset x=\"2\" y=\"1\"", "<off2 x=\"-1\" y=\"0.5\""})
        EXPECT_NE(xml.find(authored), std::string::npos) << authored;
    auto copy = open_text(xml);
    ASSERT_TRUE(copy);
    const auto &spanners = master_of(*copy).spanner_map().map();
    auto slur = std::ranges::find_if(spanners, [](const auto &entry) { return entry.second->is_slur(); });
    ASSERT_NE(slur, spanners.end());
    const auto *distance = slur->second->authored_overrides.find({AuthoredChildRole::SPANNER_SEGMENT}, Pid::MIN_DISTANCE);
    ASSERT_NE(distance, nullptr);
    EXPECT_DOUBLE_EQ(distance->value.value<Spatium>().val(), 3.25);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    const auto warm = native_captures(*document);
    EXPECT_EQ(cold, warm);
    for (const auto &[key, saved] : warm) {
        EXPECT_EQ(saved.find("<l1>"), std::string::npos) << key;
        EXPECT_EQ(saved.find("<l2>"), std::string::npos) << key;
        const auto first = saved.find("<Beam>");
        ASSERT_NE(first, std::string::npos);
        EXPECT_EQ(saved.find("<Beam>", first + 1), std::string::npos);
    }
}

TEST(NativeSave, NestedSpannersAndStemlessBeamSurviveColdSaveAndDiscard) {
    using namespace mu::engraving;
    for (const auto version : {"4.60", "5.00"}) {
        SCOPED_TRACE(version);
        const auto fixture = std::string_view(version) == "4.60" ? "native-save-overrides-460.mscx" : "native-save-overrides.mscx";
        auto input = read_text(g_source_root / "tests/data/inputs" / fixture);
        auto document = open_text(input);
        ASSERT_TRUE(document);
        const auto check = [](MasterScore &master) {
            auto bend = engrave::resolve(master, *engrave::element_id_from_string("H_H"));
            auto tapping = engrave::resolve(master, *engrave::element_id_from_string("T_T"));
            auto beam = engrave::resolve(master, *engrave::element_id_from_string("Z_Z"));
            ASSERT_TRUE(bend && tapping && beam);
            const auto &holds = to_guitar_bend(bend->object)->authored_overrides;
            const auto *play = holds.find({AuthoredChildRole::HOLD_LINE}, Pid::PLAY);
            ASSERT_NE(play, nullptr);
            EXPECT_FALSE(play->value.to_bool());
            const auto *width = holds.find({AuthoredChildRole::HOLD_LINE}, Pid::LINE_WIDTH);
            ASSERT_NE(width, nullptr);
            EXPECT_DOUBLE_EQ(width->value.value<Spatium>().val(), 0.35);
            const auto *offset = holds.find({AuthoredChildRole::HOLD_LINE, 0, 0, 1}, Pid::OFFSET2);
            ASSERT_NE(offset, nullptr);
            EXPECT_EQ(offset->value.value<PointF>(), PointF(1, 0.5));
            const auto &slurs = to_tapping(tapping->object)->authored_overrides;
            for (auto role : {AuthoredChildRole::TAPPING_SLUR_ABOVE, AuthoredChildRole::TAPPING_SLUR_BELOW}) {
                const auto *play = slurs.find({role}, Pid::PLAY);
                ASSERT_NE(play, nullptr);
                EXPECT_EQ(play->value.to_bool(), role == AuthoredChildRole::TAPPING_SLUR_BELOW);
                const auto *color = slurs.find({role}, Pid::COLOR);
                ASSERT_NE(color, nullptr);
                EXPECT_EQ(color->value.value<muse::draw::Color>(),
                          role == AuthoredChildRole::TAPPING_SLUR_ABOVE ? muse::draw::Color(20, 40, 160) : muse::draw::Color(120, 30, 40));
            }
            const auto *distance = slurs.find({AuthoredChildRole::TAPPING_SLUR_ABOVE, 0, 0, 1}, Pid::MIN_DISTANCE);
            ASSERT_NE(distance, nullptr);
            EXPECT_DOUBLE_EQ(distance->value.value<Spatium>().val(), 0.0);
            const auto *fragment = to_beam(beam->object)->authored_overrides.find({AuthoredChildRole::BEAM_FRAGMENT}, Pid::BEAM_POS);
            ASSERT_NE(fragment, nullptr);
            EXPECT_EQ(fragment->value.value<PairF>(), PairF(-3.25, -2.5));
        };
        auto &master = master_of(*document);
        check(master);
        auto bend = engrave::resolve(master, *engrave::element_id_from_string("H_H"));
        auto tapping = engrave::resolve(master, *engrave::element_id_from_string("T_T"));
        ASSERT_TRUE(bend && tapping);
        EXPECT_EQ(to_guitar_bend(bend->object)->hold_line(), nullptr);
        EXPECT_EQ(to_tapping(tapping->object)->half_slur_above(), nullptr);
        EXPECT_EQ(to_tapping(tapping->object)->half_slur_below(), nullptr);
        expect_no_layout(master);
        const auto cold = native_captures(*document);
        expect_no_layout(master);
        for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            const auto saved = document->export_score({.format = format});
            ASSERT_TRUE(saved);
            auto copy = g_engine->open(*saved, {.format = format});
            ASSERT_TRUE(copy);
            check(master_of(**copy));
        }
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        EXPECT_EQ(cold, native_captures(*document));
        const auto *hold = to_guitar_bend(bend->object)->hold_line();
        ASSERT_NE(hold, nullptr);
        EXPECT_EQ(hold->get_property(Pid::COLOR).value<muse::draw::Color>(), muse::draw::Color(100, 50, 25));
        EXPECT_DOUBLE_EQ(hold->get_property(Pid::LINE_WIDTH).value<Spatium>().val(), 0.35);
        const auto *half = to_tapping(tapping->object)->half_slur_above();
        ASSERT_NE(half, nullptr);
        EXPECT_EQ(half->get_property(Pid::COLOR).value<muse::draw::Color>(), muse::draw::Color(20, 40, 160));
        EXPECT_EQ(half->slur_direction(), DirectionV::DOWN);
        check(master);
        for (const auto show : {GuitarBendShowHoldLine::HIDE, GuitarBendShowHoldLine::SHOW}) {
            auto bend = engrave::resolve(master, *engrave::element_id_from_string("H_H"));
            ASSERT_TRUE(bend);
            ASSERT_TRUE(document->edit({{engrave::SetProperty{engine_id(*bend->object), "bendShowHoldLine", int64_t(show)}}}));
            master.style().set(Sid::LH_TAPPING_SHOW_ITEMS_TAB,
                               int(show == GuitarBendShowHoldLine::HIDE ? LHTappingShowItems::SYMBOL : LHTappingShowItems::HALF_SLUR));
            master.set_layout_all();
            const auto edited = native_captures(*document);
            ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
            EXPECT_EQ(edited, native_captures(*document));
            check(master);
            master.layout_data_store()->clear();
            EXPECT_EQ(edited, native_captures(*document));
            ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
            EXPECT_EQ(edited, native_captures(*document));
        }
        master.first_measure()->next_measure()->mstaves()[0]->set_stemless(false);
        master.set_layout_all();
        const auto stems = native_captures(*document);
        ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
        EXPECT_EQ(stems, native_captures(*document));
        check(master);
    }
}

TEST(NativeSave, AuthoredHoldWidthReachesDrawingAfterRecreation) {
    using namespace mu::engraving;
    for (const auto &[width, stroke] : {std::pair{"0.35", "28.937008"}, std::pair{"2.0", "165.354331"}}) {
        auto input = read_text(g_source_root / "tests/data/inputs/native-save-overrides.mscx");
        input.replace(input.find("<lineWidth>0.35</lineWidth>"), 27, std::string("<lineWidth>") + width + "</lineWidth>");
        size_t pos = 0;
        while ((pos = input.find("<play>0</play>", pos)) != std::string::npos) input.replace(pos, 14, "<play>1</play>");
        auto document = open_text(input);
        ASSERT_TRUE(document);
        auto &master = master_of(*document);
        const auto cold = native_captures(*document);
        auto reopened = open_text(cold.begin()->second);
        ASSERT_TRUE(reopened);
        const auto bend = engrave::resolve(master_of(*reopened), *engrave::element_id_from_string("H_H"));
        ASSERT_TRUE(bend);
        const auto *play = to_guitar_bend(bend->object)->authored_overrides.find({AuthoredChildRole::HOLD_LINE}, Pid::PLAY);
        ASSERT_NE(play, nullptr);
        EXPECT_TRUE(play->value.to_bool());
        for (int cycle = 0; cycle < 2; ++cycle) {
            const auto rendered = document->render({.format = engrave::RenderFormat::SVG});
            ASSERT_TRUE(rendered);
            const auto svg = text_of(rendered->pages.front());
            const auto color = svg.find("stroke=\"rgb(39.215686%, 19.607843%, 9.803922%)\"");
            ASSERT_NE(color, std::string::npos);
            const auto path = svg.rfind("<path", color);
            ASSERT_NE(path, std::string::npos);
            EXPECT_NE(svg.substr(path, color - path).find(std::string("stroke-width=\"") + stroke + "\""), std::string::npos);
            EXPECT_EQ(cold, native_captures(*document));
            master.layout_data_store()->clear();
        }
    }
}

TEST(NativeSave, EmptyTupletChordWritesRestWithoutCreatingSourceObjects) {
    using namespace mu::engraving;
    auto document = open_text(R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff id="1"/><Instrument><trackName>Piano</trackName></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Tuplet><eid>T_T</eid><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>eighth</baseNote></Tuplet>
<Beam><eid>B_B</eid><Fragment><y1>-3.25</y1><y2>-2.5</y2></Fragment></Beam>
<Chord><eid>C_C</eid><durationType>eighth</durationType></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord><endTuplet/>
<Rest><dots>1</dots><durationType>half</durationType></Rest></voice></Measure></Staff></Score></museScore>)");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto found = engrave::resolve(master, *engrave::element_id_from_string("C_C"));
    ASSERT_TRUE(found && found->object->is_chord());
    auto *chord = to_chord(found->object);
    ASSERT_TRUE(chord->notes().empty());
    const auto members = chord->tuplet()->elements();
    const auto children = chord->segment()->children();
    const auto handles = master.eid_register()->size();
    const auto saved = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(saved);
    EXPECT_EQ(chord->tuplet()->elements(), members);
    EXPECT_EQ(chord->segment()->children(), children);
    EXPECT_EQ(master.eid_register()->size(), handles);
    expect_no_layout(master);
    auto reopened = g_engine->open(*saved, {.format = engrave::Format::MSCX});
    ASSERT_TRUE(reopened);
    auto rest = engrave::resolve(master_of(**reopened), *engrave::element_id_from_string("C_C"));
    ASSERT_TRUE(rest && rest->object->is_rest());
    EXPECT_EQ(to_rest(rest->object)->ticks(), Fraction(1, 8));
    const auto beam = engrave::resolve(master_of(**reopened), *engrave::element_id_from_string("B_B"));
    ASSERT_TRUE(beam && beam->object->is_beam());
    EXPECT_EQ(to_rest(rest->object)->authored_beam(), beam->object);
    const auto *fragment = to_beam(beam->object)->authored_overrides.find({AuthoredChildRole::BEAM_FRAGMENT}, Pid::BEAM_POS);
    ASSERT_NE(fragment, nullptr);
    EXPECT_EQ(fragment->value.value<PairF>(), PairF(-3.25, -2.5));
}

TEST(NativeSave, PropertyUnitsNeverAskForLayoutParent) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);

    struct Probe : EngravingItem {
        explicit Probe(Score *score) : EngravingItem(ElementType::SYMBOL, score) {}

        Probe *clone() const override { return new Probe(*this); }

        EngravingItem *layout_parent() const override {
            ADD_FAILURE() << "native property units read layout";
            return nullptr;
        }
    } probe(&master_of(*document));

    probe.set_visible(false);
    muse::io::Buffer buffer;
    buffer.open(muse::io::IODevice::WRITE_ONLY);
    mu::engraving::XmlWriter xml(&buffer);
    mu::engraving::write::TWrite::write_property(&probe, xml, Pid::VISIBLE);
    xml.flush();
    EXPECT_NE(std::string(buffer.data().const_char(), buffer.data().size()).find("<visible>0</visible>"), std::string::npos);
}

TEST(NativeSave, ImageSizeInFrameUsesSourceStaffUnits) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/segment-purity.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);

    struct Frame : VBox {
        explicit Frame(Score *score) : VBox(score) {}

        bool guard = false;

        EngravingItem *layout_parent() const override {
            if (guard) ADD_FAILURE() << "image serialization reads frame layout";
            return VBox::layout_parent();
        }
    };

    auto *frame = new Frame(&master);
    auto *image = new Image(frame);
    const std::string svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="50"><rect width="100" height="50"/></svg>)";
    ASSERT_TRUE(image->load_from_data("source.svg", muse::ByteArray(svg.data(), svg.size())));
    image->set_track(0);
    image->set_size(SizeF(2, 1));
    auto staff_type = *master.staff(0)->staff_type(Fraction(0, 1));
    staff_type.set_small(true);
    master.staff(0)->set_staff_type(Fraction(1, 1), staff_type);
    commit(master, [&](auto &score) {
        score.insert_box(frame, score.first());
        frame->add(image);
    });
    expect_no_layout(master);
    frame->guard = true;
    const auto cold = native_captures(*document);
    frame->guard = false;
    expect_no_layout(master);
    EXPECT_NE(cold.begin()->second.find("<size w=\"2\" h=\"1\""), std::string::npos);
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    frame->guard = true;
    EXPECT_EQ(cold, native_captures(*document));
    frame->guard = false;
}

TEST(NativeSave, LegacyAndBoundedExportsReopenPinnedLinkedNotesWithoutLayout) {
    auto document = open_path(g_source_root / "engrave/tests/data/write470/excerpts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto check = [](MasterScore &score, bool standalone) {
        for (const auto id : {"I_I", "NB_NB"}) {
            if (standalone && std::string_view(id) == "I_I") continue;
            auto found = engrave::resolve(score, *engrave::element_id_from_string(id));
            ASSERT_TRUE(found) << id;
            ASSERT_TRUE(found->object->is_note());
            const auto *note = mu::engraving::to_note(found->object);
            EXPECT_EQ(note->pitch(), 71);
            EXPECT_EQ(note->chord()->actual_ticks(), mu::engraving::Fraction(1, 4));
        }
    };
    check(master, false);
    expect_no_layout(master);
    const auto cold = native_captures(*document, true);
    expect_no_layout(master);
    for (auto *score : native_scores(master)) {
        for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            for (const auto version : {"5.00", "4.70"}) {
                engrave::ExportOptions options{.format = format, .native_version = version};
                if (!score->is_master()) options.score.excerpt = engine_id(*score);
                options.capture.max_bytes = 1024 * 1024;
                const auto output = document->export_score(options);
                ASSERT_TRUE(output) << output.error().message;
                expect_no_layout(master);
                auto reopened = g_engine->open(*output, {.format = format});
                ASSERT_TRUE(reopened) << reopened.error().message;
                check(master_of(**reopened), !score->is_master());
                check(master, false);
            }
        }
    }
    for (auto *score : native_scores(master)) {
        ASSERT_TRUE(document->render({.score = score->is_master() ? engrave::ScoreSel{} : engrave::ScoreSel{engine_id(*score)},
                                      .format = engrave::RenderFormat::SVG}));
    }
    EXPECT_EQ(cold, native_captures(*document, true));
    check(master, false);
}

TEST(NativeSave, EditsAndMmrTogglesSaveBeforeRendering) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto note = engrave::resolve(master, *engrave::element_id_from_string("NB_NB"));
    ASSERT_TRUE(note && note->object->is_note());
    ASSERT_TRUE(document->render({.format = engrave::RenderFormat::SVG}));
    ASSERT_TRUE(document->edit({{engrave::SetProperty{engine_id(*note->object), "color", engrave::Color{200, 10, 20, 255}}}}));
    const auto check = [&] {
        for (auto *selected : native_scores(master)) {
            for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
                engrave::ExportOptions options{.format = format};
                if (!selected->is_master()) options.score.excerpt = engine_id(*selected);
                std::vector<std::pair<uint64_t, uint64_t>> counts;
                for (auto *score : native_scores(master))
                    counts.emplace_back(score->layout_state.whole_count, score->layout_state.range_count);
                const auto saved = document->export_score(options);
                ASSERT_TRUE(saved);
                size_t index = 0;
                for (auto *score : native_scores(master)) {
                    EXPECT_EQ(std::pair(score->layout_state.whole_count, score->layout_state.range_count), counts[index++]);
                }
                auto copy = g_engine->open(*saved, {.format = format});
                ASSERT_TRUE(copy);
                auto &reloaded = master_of(**copy);
                const auto edited = engrave::resolve(reloaded, *engrave::element_id_from_string("NB_NB"));
                ASSERT_TRUE(edited && edited->object->is_note());
                EXPECT_EQ(to_note(edited->object)->color(), muse::draw::Color(200, 10, 20, 255));
                EXPECT_FALSE(to_note(edited->object)->is_appearance_linked_to_master());
                if (selected->is_master()) {
                    const auto peer = engrave::resolve(reloaded, *engrave::element_id_from_string("I_I"));
                    ASSERT_TRUE(peer && peer->object->is_note());
                    EXPECT_EQ(to_note(peer->object)->color(), muse::draw::Color(0, 0, 0, 255));
                    EXPECT_EQ(to_note(peer->object)->pitch(), 71);
                    EXPECT_TRUE(std::ranges::contains(edited->object->link_list(), peer->object));
                    ASSERT_TRUE(
                        (*copy)->edit({{engrave::SetProperty{engine_id(*peer->object), "color", engrave::Color{10, 80, 150, 255}}}}));
                    EXPECT_EQ(to_note(peer->object)->color(), muse::draw::Color(10, 80, 150, 255));
                    EXPECT_EQ(to_note(edited->object)->color(), muse::draw::Color(200, 10, 20, 255));
                } else {
                    EXPECT_FALSE(engrave::resolve(reloaded, *engrave::element_id_from_string("I_I")));
                    amc::FlatSet<ElementId> links;
                    for (const auto *linked : edited->object->link_list()) {
                        EXPECT_EQ(linked->score(), &reloaded);
                        const auto resolved = engrave::resolve(reloaded, engine_id(*linked));
                        ASSERT_TRUE(resolved);
                        EXPECT_EQ(resolved->object, linked);
                        links.insert(engine_id(*linked));
                    }
                    EXPECT_EQ(links, (amc::FlatSet<ElementId>{*engrave::element_id_from_string("4_4"),
                                                              *engrave::element_id_from_string("NB_NB")}));
                }
            }
        }
        const auto cold = native_captures(*document);
        for (auto *score : native_scores(master)) {
            ASSERT_TRUE(document->render({.score = score->is_master() ? engrave::ScoreSel{} : engrave::ScoreSel{engine_id(*score)},
                                          .format = engrave::RenderFormat::SVG}));
        }
        EXPECT_EQ(cold, native_captures(*document));
    };
    EXPECT_EQ(to_note(note->object)->color(), muse::draw::Color(200, 10, 20, 255));
    const auto original = engrave::resolve(master, *engrave::element_id_from_string("I_I"));
    ASSERT_TRUE(original && original->object->is_note());
    EXPECT_EQ(to_note(original->object)->pitch(), 71);
    check();
    auto mmr_document = open_path(g_source_root / "tests/data/inputs/mmr-boundary.mscx");
    ASSERT_TRUE(mmr_document);
    for (bool enabled : {false, true, false, true}) {
        auto &mmr = master_of(*mmr_document);
        mmr.style().set(Sid::CREATE_MULTI_MEASURE_RESTS, enabled);
        mmr.set_layout_all();
        const auto whole = mmr.layout_state.whole_count;
        const auto range = mmr.layout_state.range_count;
        const auto cold = native_captures(*mmr_document);
        EXPECT_EQ(mmr.layout_state.whole_count, whole);
        EXPECT_EQ(mmr.layout_state.range_count, range);
        ASSERT_TRUE(mmr_document->render({.format = engrave::RenderFormat::SVG}));
        EXPECT_EQ(cold, native_captures(*mmr_document));
        EXPECT_EQ(cold.begin()->second.find("<multiMeasureRest>"), std::string::npos);
    }
}

TEST(NativeSave, GeneratedGhostParenthesesStayInDisplayOutput) {
    using namespace mu::engraving;
    const std::string source = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part><Staff id="1"/><Instrument><trackName>Piano</trackName></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>whole</durationType><Note><pitch>60</pitch><tpc>14</tpc><ghost>1</ghost></Note></Chord>
</voice></Measure></Staff></Score></museScore>)";
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &master = master_of(**opened);
    master.ensure_layout();
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    ASSERT_NE(note->chord()->layout_parenthesis_group(note), nullptr);
    muse::io::Buffer buffer;
    buffer.open(muse::io::IODevice::WRITE_ONLY);
    rw::WriteInOutData inout(&master);
    ASSERT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
    const std::string xml(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
    EXPECT_NE(xml.find("<ghost>1</ghost>"), std::string::npos);
    EXPECT_EQ(xml.find("<NoteParenGroup>"), std::string::npos);
    EXPECT_EQ(xml.find("<parentheses>"), std::string::npos);
}

TEST(NativeSave, AuthoredDividerSurvivesDiscardAndColdSave) {
    using namespace mu::engraving;
    for (const auto version : {"4.60", "4.70"}) {
        SCOPED_TRACE(version);
        const std::string source = std::string("<museScore version=\"") + version + R"("><Score><Division>480</Division>
<Style><dividerLeft>1</dividerLeft></Style>
<Part><Staff id="1"/><Instrument><trackName>Piano</trackName></Instrument></Part>
<SystemDividers><system idx="0"><SystemDivider type="left"><name>systemDividerLong</name><symbolsSize>1.75</symbolsSize>
<symbolAngle>12</symbolAngle><offset x="2" y="-3"/></SystemDivider></system></SystemDividers>
<Staff id="1"><Measure><LayoutBreak><subtype>line</subtype></LayoutBreak><voice>
<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig><Rest><durationType>whole</durationType></Rest></voice></Measure>
<Measure><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff></Score></museScore>)";
        auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::MSCX});
        ASSERT_TRUE(opened) << opened.error().message;
        auto &master = master_of(**opened);
        const auto write = [&] {
            muse::io::Buffer buffer;
            buffer.open(muse::io::IODevice::WRITE_ONLY);
            rw::WriteInOutData inout(&master);
            inout.ctx.set_inline_style(true);
            EXPECT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
            return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
        };
        const auto cold = write();
        EXPECT_NE(cold.find("<symbolsSize>1.75</symbolsSize>"), std::string::npos);
        EXPECT_NE(cold.find("<name>systemDividerLong</name>"), std::string::npos);
        EXPECT_EQ(master.system_divider(0, SystemDividerType::LEFT)->system(), nullptr);
        auto reopened = g_engine->open(as_bytes(cold), {.format = engrave::Format::MSCX});
        ASSERT_TRUE(reopened);
        auto &reloaded = master_of(**reopened);
        reloaded.ensure_layout();
        ASSERT_NE(reloaded.systems().front()->system_divider_left(), nullptr);
        EXPECT_EQ(reloaded.systems().front()->system_divider_left()->sym(), SymId::SYSTEM_DIVIDER_LONG);
        EXPECT_NE(cold.find("<symbolAngle>12</symbolAngle>"), std::string::npos);
        EXPECT_TRUE(master.pages().empty());
        EXPECT_EQ(master.layout_state.whole_count, 0u);
        for (int cycle = 0; cycle < 2; ++cycle) {
            master.ensure_layout();
            ASSERT_GE(master.systems().size(), 2u);
            const auto *divider = master.systems().front()->system_divider_left();
            ASSERT_NE(divider, nullptr);
            EXPECT_EQ(divider->sym(), SymId::SYSTEM_DIVIDER_LONG);
            EXPECT_DOUBLE_EQ(divider->symbols_size(), 1.75);
            EXPECT_DOUBLE_EQ(divider->sym_angle(), 12.0);
            EXPECT_DOUBLE_EQ(divider->offset().x() / master.style().spatium(), 2.0);
            EXPECT_DOUBLE_EQ(divider->offset().y() / master.style().spatium(), -3.0);
            EXPECT_EQ(write(), cold);
            master.layout_data_store()->clear();
            EXPECT_EQ(write(), cold);
        }
    }
}
