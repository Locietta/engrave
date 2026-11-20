#include "read_test_fixture.h"

using namespace read_test;

struct WarningIdentity : testing::TestWithParam<std::optional<engrave::ExportWarningObject>> {};

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
        if (format == engrave::Format::MSCX) {
            EXPECT_EQ(*default_bytes, *explicit_bytes);
        } else {
            const auto entries = [](const std::vector<std::byte> &archive) {
                muse::ByteArray bytes(reinterpret_cast<const uint8_t *>(archive.data()), archive.size());
                muse::io::Buffer buffer(&bytes);
                if (!buffer.open(muse::io::IODevice::READ_ONLY)) throw std::runtime_error("Cannot open ZIP buffer");
                muse::ZipReader zip(&buffer);
                std::map<std::string, std::string> result;
                for (const auto &entry : zip.file_info_list()) {
                    const auto name = entry.file_path.to_std_string();
                    const auto data = zip.file_data(name);
                    if (!result.emplace(name, std::string(data.const_char(), data.size())).second)
                        throw std::runtime_error("Duplicate ZIP entry: " + name);
                }
                if (zip.has_error() || result.empty()) throw std::runtime_error("Cannot read exported ZIP entries");
                return result;
            };
            EXPECT_EQ(entries(*default_bytes), entries(*explicit_bytes));
        }
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
    ASSERT_TRUE(document->edit({{engrave::SetStyle{selection, "staffDistance", 9.0}}}));
    auto styled = document->export_score_with_warnings({.format = engrave::Format::MSCX, .score = selection, .native_version = "4.70"});
    ASSERT_TRUE(styled) << styled.error().message;
    const std::string xml(reinterpret_cast<const char *>(styled->bytes.data()), styled->bytes.size());
    EXPECT_NE(xml.find("<staffDistance>9</staffDistance>"), std::string::npos);
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
    auto document = open_path(write470_fixture().parent_path() / "ties.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto score_id = engrave::from_engine(master.eid());
    master.style().set(mu::engraving::Sid::MASK_TIES, true);
    const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::UNSUPPORTED);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    const auto &warning = result.error().export_warnings.front();
    EXPECT_EQ(warning.code, "B470_UNSUPPORTED_FEATURE");
    EXPECT_NE(result.error().message.find("maskTies"), std::string::npos);
    ASSERT_TRUE(warning.object);
    EXPECT_EQ(warning.object->id, engrave::to_string(score_id));
    EXPECT_TRUE(master.style().style_b(mu::engraving::Sid::MASK_TIES));
}

TEST(Export470, DeduplicatesUnmappedStylesWithoutPublishing) {
    auto source = read_text(write470_fixture());
    source.insert(source.find("</voice>"), "<BarLine><subtype>dashed</subtype></BarLine>");
    auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    auto &master = master_of(**document);
    master.style().set(mu::engraving::Sid::DASH_BAR_DASH, mu::engraving::Spatium(1.2));
    master.style().set(mu::engraving::Sid::DASH_BAR_GAP, mu::engraving::Spatium(1.3));
    const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::UNSUPPORTED);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    EXPECT_EQ(result.error().export_warnings.front().code, "B470_UNSUPPORTED_FEATURE");
    EXPECT_NE(result.error().message.find("dashBarDash"), std::string::npos);
    EXPECT_NE(result.error().message.find("dashBarGap"), std::string::npos);
}

TEST(Export470, PreservesEffectiveLegacyStyle) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &style = master_of(*document).style();
    style.set(Sid::PAGE_WIDTH, 9.5);
    style.set(Sid::PAGE_ODD_LEFT_MARGIN, 1.125);
    style.set(Sid::SPATIUM, 7.25);
    style.set(Sid::HEADER_FONT_FACE, muse::String(u"FreeSerif"));
    style.set(Sid::LONG_INSTRUMENT_ALIGN, Align{AlignH::LEFT, AlignV::VCENTER});
    style.set(Sid::SHORT_INSTRUMENT_ALIGN, Align{AlignH::RIGHT, AlignV::BASELINE});
    style.set(Sid::HIDE_EMPTY_STAVES, true);
    style.set(Sid::MIN_SYSTEM_DISTANCE, Spatium(12.25));
    style.set(Sid::MAX_SYSTEM_DISTANCE, Spatium(18.5));
    style.set(Sid::MASK_TIES, true);
    const auto exported = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(exported) << exported.error().message;
    EXPECT_TRUE(exported->warnings.empty());
    const std::string xml(reinterpret_cast<const char *>(exported->bytes.data()), exported->bytes.size());
    for (const char *field :
         {"<defaultsVersion>470</defaultsVersion>", "<pageWidth>9.5</pageWidth>", "<pageOddLeftMargin>1.125</pageOddLeftMargin>",
          "<headerFontFace>FreeSerif</headerFontFace>", "<longInstrumentAlign>left,center</longInstrumentAlign>",
          "<shortInstrumentAlign>right,baseline</shortInstrumentAlign>", "<hideEmptyStaves>1</hideEmptyStaves>",
          "<minSystemDistance>12.25</minSystemDistance>", "<maxSystemDistance>18.5</maxSystemDistance>"})
        EXPECT_NE(xml.find(field), std::string::npos) << field;
    EXPECT_EQ(xml.find("<maskTies>"), std::string::npos);
    const auto reopened = g_engine->open(exported->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    EXPECT_NEAR(master_of(**reopened).style().spatium(), 7.25, 0.0001);
}

TEST(Export470, PreservesBlankGrandStaffPiano) {
    auto document = g_engine->create_document({.template_id = "Piano"});
    ASSERT_TRUE(document) << document.error().message;
    const auto exported = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(exported) << exported.error().message;
    EXPECT_TRUE(exported->warnings.empty());
    const std::string xml(reinterpret_cast<const char *>(exported->bytes.data()), exported->bytes.size());
    for (const auto *field :
         {"<bracket type=\"1\" span=\"2\" col=\"0\" visible=\"1\"", "<defaultClef>F</defaultClef>", "<durationType>measure</durationType>"})
        EXPECT_NE(xml.find(field), std::string::npos) << field;
    auto reopened = g_engine->open(exported->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto source = music::semantic_dump(&master_of(**document));
    const auto *treble = master_of(**reopened).staff(0);
    const auto *bass = master_of(**reopened).staff(1);
    ASSERT_NE(treble, nullptr);
    ASSERT_NE(bass, nullptr);
    EXPECT_EQ(treble->default_clef_type().concert_clef, mu::engraving::ClefType::G);
    EXPECT_EQ(bass->default_clef_type().concert_clef, mu::engraving::ClefType::F);
    const auto actual = music::semantic_dump(&master_of(**reopened));
    for (const char *key : {"parts", "measures", "events"}) EXPECT_TRUE(music::json_equal(actual[key], source[key])) << key;
}

TEST(Export470, PreservesPairedSharpPitchEditInBothFormats) {
    auto source = read_text(write470_fixture().parent_path() / "ties.mscx");
    for (std::size_t pos = 0; (pos = source.find("<pitch>60</pitch>", pos)) != std::string::npos; ++pos)
        source.replace(pos, std::string_view("<pitch>60</pitch>").size(), "<pitch>71</pitch>");
    for (std::size_t pos = 0; (pos = source.find("<tpc>14</tpc>", pos)) != std::string::npos; ++pos)
        source.replace(pos, std::string_view("<tpc>14</tpc>").size(), "<tpc>19</tpc>");
    auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    const auto first = *engrave::element_id_from_string("G_G");
    const auto second = *engrave::element_id_from_string("J_J");
    ASSERT_TRUE((*document)->edit({{engrave::SetPitch{first, 73, 21}, engrave::SetPitch{second, 73, 21}}}));
    for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        const auto exported = (*document)->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_TRUE(exported) << exported.error().message;
        EXPECT_TRUE(exported->warnings.empty());
        if (format == engrave::Format::MSCX) {
            const std::string xml(reinterpret_cast<const char *>(exported->bytes.data()), exported->bytes.size());
            EXPECT_NE(xml.find("<subtype>accidentalSharp</subtype>"), std::string::npos);
        }
        const auto reopened = g_engine->open(exported->bytes, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        auto *registry = master_of(**reopened).eid_register();
        auto *start = static_cast<mu::engraving::Note *>(registry->item_from_eid(engrave::to_engine(first)));
        auto *end = static_cast<mu::engraving::Note *>(registry->item_from_eid(engrave::to_engine(second)));
        ASSERT_NE(start, nullptr);
        ASSERT_NE(end, nullptr);
        for (const auto *note : {start, end}) {
            EXPECT_EQ(note->pitch(), 73);
            EXPECT_EQ(note->tpc(), 21);
        }
        ASSERT_NE(start->accidental(), nullptr);
        EXPECT_EQ(start->accidental()->accidental_type(), mu::engraving::AccidentalType::SHARP);
        ASSERT_NE(end->tie_back(), nullptr);
        EXPECT_EQ(end->tie_back()->start_note(), start);
    }
}

TEST(Export470, PreservesCommonNotationAndHarmonyCases) {
    auto source = read_text(write470_fixture());
    source.replace(source.find("version=\"4.70\""), std::string_view("version=\"4.70\"").size(), "version=\"5.00\"");
    source.insert(source.find("</Style>"), "<lyricsStackingOrder>0</lyricsStackingOrder>");
    source.insert(source.find("<Part "), R"(<Order id="custom"><name>Test order</name></Order>)");
    source.insert(source.find("<Measure>"), "<VBox><height>10</height><Text><style>title</style><text>Title</text></Text></VBox>");
    source.insert(source.find("<TimeSig>"), "<KeySig><concertKey>-1</concertKey></KeySig>");
    const auto first_chord = source.find("<Chord>");
    const auto after_chord = source.find("</Chord>") + std::string_view("</Chord>").size();
    source.replace(first_chord, after_chord - first_chord, R"(
        <Harmony><rootCase>auto</rootCase><bassCase>capital</bassCase><harmonyInfo><root>15</root><bass>14</bass></harmonyInfo></Harmony>
        <Dynamic><subtype>mf</subtype></Dynamic>
        <Tempo><tempo>1.5</tempo><text>Andante</text></Tempo>
        <Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>eighth</baseNote></Tuplet>
        <Chord><durationType>eighth</durationType><Articulation><subtype>articStaccatoAbove</subtype></Articulation>
          <Lyrics><text>word</text><centerBetweenStaves>off</centerBetweenStaves></Lyrics><Note><pitch>65</pitch><tpc>13</tpc></Note></Chord>
        <Chord><durationType>eighth</durationType><Note><pitch>67</pitch><tpc>15</tpc></Note></Chord>
        <Chord><durationType>eighth</durationType><Note><pitch>69</pitch><tpc>17</tpc></Note></Chord>
        <endTuplet/>
        <Harmony><rootCase>lower</rootCase><bassCase>upper</bassCase><harmonyInfo><root>14</root><bass>15</bass></harmonyInfo></Harmony>
        <Rest><durationType>half</durationType><dots>1</dots></Rest>)");
    auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    const auto exported = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(exported) << exported.error().message;
    EXPECT_TRUE(exported->warnings.empty());
    const std::string xml(reinterpret_cast<const char *>(exported->bytes.data()), exported->bytes.size());
    for (const char *field :
         {"<Order id=\"custom\"", "<VBox>", "<concertKey>-1</concertKey>", "<Tuplet>", "<endTuplet", "<Dynamic>", "<tempo>1.5</tempo>",
          "<text>Andante</text>", "<Articulation>", "<Lyrics>", "<centerBetweenStaves>off</centerBetweenStaves>", "<rootCase>-1</rootCase>",
          "<rootCase>1</rootCase>", "<bassCase>2</bassCase>"})
        EXPECT_NE(xml.find(field), std::string::npos) << field;
    auto reopened = g_engine->open(exported->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    std::vector<mu::engraving::NoteCaseType> root_cases, bass_cases;
    for (const auto *item : engrave::document_objects(master_of(**reopened))) {
        if (!item->is_harmony()) continue;
        const auto *harmony = static_cast<const mu::engraving::Harmony *>(item);
        root_cases.push_back(harmony->root_case());
        bass_cases.push_back(harmony->bass_case());
    }
    using enum mu::engraving::NoteCaseType;
    EXPECT_EQ(root_cases, (std::vector{AUTO, LOWER}));
    EXPECT_EQ(bass_cases, (std::vector{CAPITAL, UPPER}));
}

TEST(Export470, NamesAnEmbeddedExcerptInStyleRefusal) {
    auto document = open_path(write470_fixture().parent_path() / "excerpts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *excerpt = master.excerpts().front()->excerpt_score();
    excerpt->style().set(mu::engraving::Sid::MASK_TIES, true);
    const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error().export_warnings.size(), 1u) << result.error().message;
    const auto &warning = result.error().export_warnings.front();
    EXPECT_EQ(warning.code, "B470_UNSUPPORTED_FEATURE");
    ASSERT_TRUE(warning.object);
    EXPECT_EQ(warning.object->id, excerpt->eid().to_std_string());
}

TEST(Export470, RefusesFiveOnlyTremolos) {
    for (const auto *subtype : {"r128", "c256"}) {
        auto source = read_text(write470_fixture().parent_path() / "ties.mscx");
        const std::string tag = subtype[0] == 'r' ? "TremoloSingleChord" : "TremoloTwoChord";
        source.replace(source.find("version=\"4.70\""), std::string_view("version=\"4.70\"").size(), "version=\"5.00\"");
        source.insert(source.find("<Note>"), "<" + tag + "><subtype>" + subtype + "</subtype></" + tag + ">");
        // The legacy connector fixture serves only as the source of two half-note chords.
        for (auto pos = source.find("<Spanner"); pos != std::string::npos; pos = source.find("<Spanner")) {
            const auto end = source.find("</Spanner>", pos) + std::string_view("</Spanner>").size();
            source.erase(pos, end - pos);
        }
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_FALSE(result) << subtype;
        EXPECT_EQ(result.error().code, ErrorCode::UNSUPPORTED);
        ASSERT_EQ(result.error().export_warnings.size(), 1u);
        EXPECT_NE(result.error().message.find(subtype), std::string::npos);
    }
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
    auto &master = master_of(**document);
    const auto start = engrave::resolve(master, *engrave::element_id_from_string("G_G"));
    ASSERT_TRUE(start);
    auto *tie = mu::engraving::to_note(start->object)->tie_for();
    ASSERT_NE(tie, nullptr);
    master.eid_register()->remove_item(tie);
    auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    EXPECT_EQ(result.error().export_warnings[0].object, (engrave::ExportWarningObject{"note", "G_G"}));
    // Exercise the explicit non-EID locator when no owner is addressable.
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
