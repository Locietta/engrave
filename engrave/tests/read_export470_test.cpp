#include <pugixml.hpp>

#include "engrave/src/score/write470_spanners.h"
#include "engraving/dom/chordlist.h"
#include "engraving/dom/linkedobjects.h"
#include "engraving/dom/staff.h"
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
    for (const auto *subtype : {"r128", "r256", "c128", "c256", "c16"}) {
        SCOPED_TRACE(subtype);
        auto source = read_text(write470_fixture().parent_path() / "ties.mscx");
        const std::string tag = subtype[0] == 'r' ? "TremoloSingleChord" : "TremoloTwoChord";
        const bool authored_direction = std::string_view(subtype) == "c16";
        source.replace(source.find("version=\"4.70\""), std::string_view("version=\"4.70\"").size(), "version=\"5.00\"");
        source.insert(source.find("<Note>"), std::format("<{0}><subtype>{1}</subtype>{2}</{0}>", tag, subtype,
                                                         authored_direction ? "<StemDirection>up</StemDirection>" : ""));
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
        EXPECT_NE(result.error().message.find(authored_direction ? "TremoloTwoChord/StemDirection" : subtype), std::string::npos);
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

namespace {

std::string connector_score(std::string_view first_staff, std::string_view second_staff, std::string_view spanners) {
    return std::format(R"(<museScore version="5.00"><Score><eid>A_A</eid><Division>480</Division>
        <Part id="1"><Staff><eid>B_B</eid><StaffType group="pitched"/></Staff>
        <Staff><eid>C_C</eid><StaffType group="pitched"/></Staff><Instrument id="piano">
        <instrumentId>keyboard.piano</instrumentId><Channel><program value="0"/></Channel>
        </Instrument></Part><Staff id="1">{}</Staff><Staff id="2">{}</Staff><SpannerMap>{}</SpannerMap></Score></museScore>)",
                       first_staff, second_staff, spanners);
}

bool parse_export(pugi::xml_document &xml, const engrave::ExportOutput &result) {
    return bool(xml.load_buffer(result.bytes.data(), result.bytes.size()));
}

} // namespace

TEST(Export470, CombinesDuplicateGhostNoteGroupsAndRebasesBothLinks) {
    const auto corpus = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden";
    auto document = open_path(corpus / "ghost_note.mscx");
    ASSERT_TRUE(document);
    const auto result = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_TRUE(result->warnings.empty());
    pugi::xml_document xml;
    ASSERT_TRUE(parse_export(xml, *result));
    const auto groups = xml.select_nodes("//NoteParenGroup");
    ASSERT_EQ(groups.size(), 2u);
    const auto left = groups[0].node().child("Parenthesis");
    const auto right = left.next_sibling("Parenthesis");
    EXPECT_STREQ(left.child_value("eid"), "xuRxhYhEgwC_JMh0LQIeg2P");
    EXPECT_STREQ(right.child_value("eid"), "elgMPb24DwF_uHOs8mN13FO");
    const auto linked = groups[1].node().child("Parenthesis");
    EXPECT_STREQ(linked.child_value("linkedTo"), left.child_value("eid"));
    EXPECT_STREQ(linked.next_sibling("Parenthesis").child_value("linkedTo"), right.child_value("eid"));
}

TEST(Export470, RefusesDifferentParenthesisGroupsSharingANote) {
    const auto source = connector_score(
        R"(<Measure><voice><Chord><durationType>whole</durationType>
            <Note><eid>D_D</eid><pitch>60</pitch><tpc>14</tpc></Note>
            <Note><eid>E_E</eid><pitch>64</pitch><tpc>18</tpc></Note>
            <NoteParenGroup><Notes><NoteEID>D_D</NoteEID></Notes></NoteParenGroup>
            <NoteParenGroup><Notes><NoteEID>E_E</NoteEID><NoteEID>D_D</NoteEID></Notes></NoteParenGroup>
            </Chord></voice></Measure>)",
        "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>", "");
    const auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    EXPECT_EQ(result.error().export_warnings[0].code, "B470_UNSUPPORTED_FEATURE");
    EXPECT_NE(result.error().message.find("overlapping NoteParenGroups"), std::string::npos);
}

TEST(Export470, IndexesParenthesesInLegacyUnisonAndPercussionOrder) {
    for (const bool drums : {false, true}) {
        SCOPED_TRACE(drums);
        const std::string first_notes =
            drums ? "<Note><eid>D_D</eid><pitch>36</pitch><tpc>14</tpc></Note><Note><eid>E_E</eid><pitch>42</pitch><tpc>20</tpc></Note>" :
                    "<Note><eid>D_D</eid><pitch>60</pitch><tpc>14</tpc></Note><Note><eid>F_F</eid><pitch>60</pitch><tpc>14</tpc></Note>"
                    "<Note><eid>E_E</eid><pitch>60</pitch><tpc>14</tpc></Note>";
        auto source =
            connector_score("<Measure><voice><Chord><durationType>whole</durationType>" + first_notes +
                                "<NoteParenGroup><Notes><NoteEID>E_E</NoteEID></Notes></NoteParenGroup></Chord></voice></Measure>",
                            "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>", "");
        if (drums) {
            source.replace(source.find("group=\"pitched\""), std::string_view("group=\"pitched\"").size(), "group=\"percussion\"");
            source.insert(source.find("<instrumentId>"),
                          R"(<useDrumset>1</useDrumset><Drum pitch="36"><head>normal</head><line>7</line></Drum>
                             <Drum pitch="42"><head>cross</head><line>-1</line></Drum>)");
        }
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(result) << result.error().message;
        pugi::xml_document xml;
        ASSERT_TRUE(parse_export(xml, *result));
        const auto notes = xml.select_nodes("//Chord/Note");
        ASSERT_EQ(notes.size(), drums ? 2u : 3u);
        if (!drums) {
            EXPECT_STREQ(notes[0].node().child_value("eid"), "D_D");
            EXPECT_STREQ(notes[1].node().child_value("eid"), "E_E");
            EXPECT_STREQ(notes[2].node().child_value("eid"), "F_F");
        }
        const auto indices = xml.select_nodes("//NoteParenGroup/Notes/NoteIdx");
        ASSERT_EQ(indices.size(), 1u);
        EXPECT_EQ(indices[0].node().text().as_int(-1), drums ? 1 : 2);
    }
}

TEST(Export470, RefusesSparseBeamFragmentsAndNestedImages) {
    for (const bool image : {false, true}) {
        SCOPED_TRACE(image);
        const auto source =
            connector_score(image ? R"(<VBox><Image><path>missing.png</path><eid>D_D</eid></Image></VBox>
                       <Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>)" :
                                    R"(<Measure><voice><Beam><eid>D_D</eid><Fragment no="1"><y1>2</y1><y2>3</y2></Fragment></Beam>
                       <Chord><BeamMode>begin</BeamMode><durationType>eighth</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
                       <Chord><BeamMode>end</BeamMode><durationType>eighth</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
                       <Rest><durationType>half</durationType><dots>1</dots></Rest></voice></Measure>)",
                            "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>", "");
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().export_warnings.size(), 1u) << result.error().message;
        EXPECT_EQ(result.error().export_warnings[0].code, "B470_UNSUPPORTED_FEATURE");
        EXPECT_NE(result.error().message.find(image ? "VBox/Image" : "nonconsecutive Beam/Fragment"), std::string::npos);
    }
}

TEST(Export470, AttributesInstrumentChangeGroupNamesToTheOwningParts) {
    const auto source = R"(<museScore version="5.00"><Score><eid>A_A</eid><Division>480</Division>
        <Style><windsNameByGroup>1</windsNameByGroup></Style>
        <Part id="1"><eid>P_P</eid><Staff><eid>B_B</eid><StaffType group="pitched"/></Staff>
            <Instrument id="oboe"><instrumentId>wind.reed.oboe</instrumentId><trackName>Oboe</trackName></Instrument></Part>
        <Part id="2"><eid>Q_Q</eid><Staff><eid>C_C</eid><StaffType group="pitched"/></Staff>
            <Instrument id="flute"><instrumentId>wind.flutes.flute</instrumentId><trackName>Flute</trackName></Instrument></Part>
        <Staff id="1"><Measure><voice><Rest><durationType>half</durationType></Rest>
            <InstrumentChange><eid>D_D</eid><text>To flute</text><Instrument id="flute">
                <instrumentId>wind.flutes.flute</instrumentId><trackName>Flute</trackName></Instrument></InstrumentChange>
            <Rest><durationType>half</durationType></Rest></voice></Measure></Staff>
        <Staff id="2"><Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure></Staff>
        </Score></museScore>)";
    for (const bool authored : {false, true}) {
        SCOPED_TRACE(authored);
        std::string xml(source);
        if (authored)
            xml.insert(xml.find("<trackName>Flute</trackName>", xml.find("<InstrumentChange>")),
                       "<InstrumentLabel><useCustomGroupName>1</useCustomGroupName><customNameLongGroup>Winds</customNameLongGroup></"
                       "InstrumentLabel>");
        const auto document = g_engine->open(as_bytes(xml));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(result) << result.error().message;
        ASSERT_EQ(result->warnings.size(), 2u);
        for (const auto &[index, id] : std::array{std::pair{0u, "P_P"}, std::pair{1u, "Q_Q"}}) {
            const auto &warning = result->warnings[index];
            EXPECT_EQ(warning.code, "B470_INSTRUMENT_GROUP_NAME_DROPPED");
            ASSERT_TRUE(warning.object);
            EXPECT_EQ(warning.object->kind, "part");
            EXPECT_EQ(warning.object->id, id);
        }
    }
}

TEST(Export470, PreservesUserAndCourtesyAccidentalsAcrossATie) {
    auto source = read_text(write470_fixture().parent_path() / "ties.mscx");
    for (const auto &[id, accidental] :
         std::array{std::pair{"G_G", "<Accidental><role>1</role><subtype>sharp</subtype></Accidental>"},
                    std::pair{"J_J", "<Accidental><role>1</role><subtype>sharp</subtype><bracket>1</bracket></Accidental>"}}) {
        const auto pitch = source.find("<pitch>60</pitch>", source.find(std::format("<eid>{}</eid>", id)));
        ASSERT_NE(pitch, std::string::npos);
        source.replace(pitch, std::string_view("<pitch>60</pitch>").size(), std::string(accidental) + "<pitch>61</pitch>");
        const auto tpc = source.find("<tpc>14</tpc>", pitch);
        ASSERT_NE(tpc, std::string::npos);
        source.replace(tpc, std::string_view("<tpc>14</tpc>").size(), "<tpc>21</tpc>");
    }
    const auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        const auto result = (*document)->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_TRUE(result->warnings.empty());
        const auto reopened = g_engine->open(result->bytes, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        using namespace mu::engraving;
        const auto *registry = master_of(**reopened).eid_register();
        const auto *start = to_note(registry->item_from_eid(EID::from_std_string(std::string_view("G_G"))));
        const auto *end = to_note(registry->item_from_eid(EID::from_std_string(std::string_view("J_J"))));
        ASSERT_TRUE(start && end && start->tie_for());
        EXPECT_EQ(start->tie_for()->end_note(), end);
        for (const auto *note : {start, end}) {
            EXPECT_EQ(note->pitch(), 61);
            const auto *accidental = note->accidental();
            ASSERT_TRUE(accidental);
            EXPECT_EQ(accidental->role(), AccidentalRole::USER);
            EXPECT_EQ(accidental->accidental_type(), AccidentalType::SHARP);
            EXPECT_EQ(static_cast<int>(accidental->bracket()), note == end ? 1 : 0);
        }
    }
}

TEST(Export470, RetainsAttachedExcerptTracklistsNamesAndStyles) {
    auto document = open_path(write470_fixture().parent_path() / "attached_tracklists.mscx");
    ASSERT_TRUE(document);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        const auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_TRUE(result->warnings.empty());
        const auto reopened = g_engine->open(result->bytes, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        const auto &excerpts = master_of(**reopened).excerpts();
        ASSERT_EQ(excerpts.size(), 2u);
        for (const auto &[index, eid, name, track, distance] :
             {std::tuple{0u, "F_F", "Upper winds", 1u, 7.25}, std::tuple{1u, "I_I", "Lower winds", 6u, 8.5}}) {
            const auto &excerpt = *excerpts[index];
            const auto &score = *excerpt.excerpt_score();
            EXPECT_EQ(excerpt.name().to_std_string(), name);
            EXPECT_EQ(excerpt.initial_part_id().to_uint64(), index + 1);
            EXPECT_EQ(score.eid().to_std_string(), eid);
            EXPECT_DOUBLE_EQ(score.style().style_v(mu::engraving::Sid::STAFF_DISTANCE).value<mu::engraving::Spatium>().val(), distance);
            const auto &tracks = excerpts[index]->tracks_mapping();
            ASSERT_EQ(tracks.size(), 1u);
            EXPECT_EQ(tracks.begin()->first, track);
            EXPECT_EQ(tracks.begin()->second, 0u);
        }
    }
}

TEST(Export470, RetainsCustomChordDefinitionsAndArchiveSidecar) {
    auto document = open_path(write470_fixture().parent_path() / "custom_chordlist.mscx");
    ASSERT_TRUE(document);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        const auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_TRUE(result->warnings.empty());
        pugi::xml_document xml;
        pugi::xml_node root;
        if (format == engrave::Format::MSCX) {
            ASSERT_TRUE(parse_export(xml, *result));
            root = xml.child("museScore").child("Score").child("Style").child("ChordList");
        } else {
            muse::ByteArray bytes(reinterpret_cast<const uint8_t *>(result->bytes.data()), result->bytes.size());
            auto buffer = muse::io::Buffer::opened(muse::io::IODevice::READ_ONLY, &bytes);
            muse::ZipReader zip(&buffer);
            const auto sidecar = zip.file_data("chordlist.xml");
            ASSERT_TRUE(xml.load_buffer(sidecar.const_data(), sidecar.size()));
            root = xml.child("museScore");
            EXPECT_STREQ(root.attribute("version").value(), "4.70");
        }
        ASSERT_TRUE(root);
        EXPECT_STREQ(root.child("renderRoot").child_value(), ":n :a");
        const auto symbol = root.select_node("font/sym[@name='diamond']").node();
        EXPECT_STREQ(symbol.attribute("value").value(), "◇");
        EXPECT_STREQ(root.select_node("token[name='diamond']/render").node().child_value(), "diamond");
        EXPECT_STREQ(root.select_node("chord[@id='801']/name").node().child_value(), "altDiamond");
        const auto reopened = g_engine->open(result->bytes, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        const auto &list = *master_of(**reopened).chord_list();
        EXPECT_TRUE(list.custom_chord_list());
        EXPECT_TRUE(list.auto_adjust());
        EXPECT_DOUBLE_EQ(list.nominal_adjust(), 0.125);
        EXPECT_DOUBLE_EQ(list.nominal_mag(), 1.1);
        const auto custom = list.symbol(muse::String(u"diamond"));
        EXPECT_EQ(custom.value.to_std_string(), "◇");
        ASSERT_LT(static_cast<std::size_t>(custom.font_idx), list.fonts.size());
        EXPECT_DOUBLE_EQ(list.fonts[custom.font_idx].mag, 1.25);
        const auto *description = list.description(801);
        ASSERT_TRUE(description);
        EXPECT_EQ(description->names.front().to_std_string(), "altDiamond");
    }
}

TEST(Export470, RefusesCustomChordActionsThatCannotBeSerialized) {
    auto document = open_path(write470_fixture().parent_path() / "custom_chordlist.mscx");
    ASSERT_TRUE(document);
    using namespace mu::engraving;
    auto &description = master_of(*document).chord_list()->at(801);
    description.render_list.push_back(std::make_shared<RenderActionMoveTextWidth>(u"diamond"));
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        const auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().export_warnings.size(), 1u);
        EXPECT_EQ(result.error().export_warnings[0].code, "B470_UNSUPPORTED_FEATURE");
        EXPECT_NE(result.error().message.find("custom chord list"), std::string::npos);
    }
}

TEST(Export470, ExportsGeneratedAutomationButRefusesAuthoredPoints) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    master.init_automation();
    EXPECT_TRUE(document->export_score({.format = engrave::Format::MSCZ, .native_version = "4.70"}));
    AutomationPoint point;
    point.value.out_value = normalize_tempo(BeatsPerSecond(3.0));
    AutomationPointEdits edits{{480, AutomationPointEdit::SetPoint{point}}};
    master.apply_automation_points(tempo_key, edits);
    ASSERT_FALSE(master.automation_data()->curve(tempo_key).at(480).generated);
    for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        const auto result = document->export_score_with_warnings({.format = format, .native_version = "4.70"});
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().export_warnings.size(), 1u);
        EXPECT_EQ(result.error().export_warnings[0].code, "B470_UNSUPPORTED_FEATURE");
        EXPECT_NE(result.error().message.find("authored automation"), std::string::npos);
        ASSERT_TRUE(result.error().export_warnings[0].object);
        EXPECT_EQ(result.error().export_warnings[0].object->id, master.eid().to_std_string());
    }
}

TEST(Export470, StripsDormantChordLineControlsAndRefusesActiveOffsets) {
    const std::string_view rest = "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>";
    for (const bool active : {false, true}) {
        SCOPED_TRACE(active);
        const auto source = connector_score(std::format(R"(<Measure><voice><Chord><durationType>whole</durationType>
              <Note><pitch>60</pitch><tpc>14</tpc></Note>
              <ChordLine><subtype>1</subtype><straight>1</straight>
                <o1 x="0" y="0"/><o2 x="0.5" y="1"/><o3 x="0.25" y="-1"/><o4 x="{}" y="0"/>
              </ChordLine></Chord></voice></Measure>)",
                                                        active ? "1" : "0"),
                                            rest, "");
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        if (active) {
            ASSERT_FALSE(result);
            EXPECT_NE(result.error().message.find("ChordLine/o4"), std::string::npos);
        } else {
            ASSERT_TRUE(result) << result.error().message;
            pugi::xml_document xml;
            ASSERT_TRUE(parse_export(xml, *result));
            const auto line = xml.select_node("//ChordLine").node();
            ASSERT_TRUE(line);
            EXPECT_TRUE(line.child("straight").text().as_bool());
            for (const auto *field : {"o1", "o2", "o3", "o4"}) EXPECT_FALSE(line.child(field));
        }
    }
}

TEST(Export470, RefusesActiveHarmonyAndMeasureRepeatFields) {
    const auto corpus = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden";
    for (const auto &[parent, field] :
         std::array{std::pair{"Harmony", "excludeVerticalAlign"}, std::pair{"MeasureRepeat", "measureRepeatNumberOffset"}}) {
        SCOPED_TRACE(field);
        const bool harmony = std::string_view(parent) == "Harmony";
        auto source = harmony ? read_text(write470_fixture()) : read_text(corpus / "layout_elements.mscx");
        const auto version = source.find("version=\"4.70\"");
        if (version != std::string::npos) source.replace(version, std::string_view("version=\"4.70\"").size(), "version=\"5.00\"");
        if (harmony) source.insert(source.find("<Chord>"), "<Harmony><harmonyInfo><root>14</root></harmonyInfo></Harmony>");
        const auto at = source.find(std::format("<{}>", parent));
        ASSERT_NE(at, std::string::npos);
        source.insert(at + std::strlen(parent) + 2, std::format("<{0}>1</{0}>", field));
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().message.find(std::format("{}/{}", parent, field)), std::string::npos);
    }
}

TEST(Export470, RefusesUnprojectableChangesToStaffNames) {
    for (const auto *change :
         {"<instrumentLabelVisibility>short</instrumentLabelVisibility>", "<StaffLabel><longName>Solo</longName></StaffLabel>", ""}) {
        SCOPED_TRACE(change);
        auto source = connector_score(std::format(R"(<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
                <Measure><StaffTypeChange><eid>D_D</eid><StaffType group="pitched">{}</StaffType></StaffTypeChange>
                <voice><Rest><durationType>measure</durationType></Rest></voice></Measure>)",
                                                  change),
                                      "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>"
                                      "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>",
                                      "");
        // The final case resets an initially forced label back to AUTO.
        if (*change == 0) {
            auto pos = source.find("<StaffType group=\"pitched\"/>");
            ASSERT_NE(pos, std::string::npos);
            source.replace(pos, std::string_view("<StaffType group=\"pitched\"/>").size(),
                           "<StaffType group=\"pitched\"><instrumentLabelVisibility>long</instrumentLabelVisibility></StaffType>");
        }
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_FALSE(result);
        ASSERT_EQ(result.error().export_warnings.size(), 1u) << result.error().message;
        EXPECT_EQ(result.error().export_warnings[0].code, "B470_UNSUPPORTED_FEATURE");
        ASSERT_TRUE(result.error().export_warnings[0].object);
        EXPECT_EQ(result.error().export_warnings[0].object->id, "D_D");
    }
}

TEST(Export470, RefusesActivePartialLyricCentering) {
    for (const auto *policy : {"auto", "on"}) {
        SCOPED_TRACE(policy);
        const auto source = connector_score(
            R"(<Measure><voice><Chord><durationType>half</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
               <Chord><durationType>half</durationType><Note><pitch>61</pitch><tpc>21</tpc></Note></Chord></voice></Measure>)",
            "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>",
            std::format(R"(<PartialLyricsLine><eid>D_D</eid><track>0</track><startTick>0/1</startTick><ticks>1/2</ticks>
                <centerBetweenStaves>{}</centerBetweenStaves></PartialLyricsLine>)",
                        policy));
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        if (std::string_view(policy) == "on") {
            ASSERT_FALSE(result);
            EXPECT_NE(result.error().message.find("PartialLyricsLine/centerBetweenStaves"), std::string::npos);
        } else {
            ASSERT_TRUE(result) << result.error().message;
            pugi::xml_document xml;
            ASSERT_TRUE(parse_export(xml, *result));
            const auto line = xml.select_node("//PartialLyricsLine").node();
            ASSERT_TRUE(line);
            EXPECT_FALSE(line.child("centerBetweenStaves"));
        }
    }
}

TEST(Export470, ReconstructsCrossMeasureStaffVoiceGraceAndUnisonEndpoints) {
    using namespace mu::engraving;
    const auto source = connector_score(
        R"(<Measure><voice>
              <Chord><eid>D_D</eid><acciaccatura/><durationType>eighth</durationType>
                <Note><eid>E_E</eid><pitch>60</pitch><tpc>14</tpc></Note></Chord>
              <Chord><eid>F_F</eid><durationType>whole</durationType>
                <Note><eid>G_G</eid><pitch>62</pitch><tpc>16</tpc></Note></Chord>
            </voice></Measure><Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>)",
        R"(<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
            <Measure><voice><Rest><durationType>measure</durationType></Rest></voice><voice>
              <Chord><eid>H_H</eid><durationType>whole</durationType>
                <Note><eid>I_I</eid><pitch>60</pitch><tpc>14</tpc>
                  <Tie><eid>K_K</eid><startElement>E_E</startElement><endElement>I_I</endElement><track>0</track></Tie></Note>
                <Note><eid>J_J</eid><pitch>60</pitch><tpc>14</tpc></Note>
                <Note><eid>L_L</eid><pitch>60</pitch><tpc>14</tpc></Note>
              </Chord></voice></Measure>)",
        R"(<Slur><eid>M_M</eid><startElement>F_F</startElement><endElement>H_H</endElement><track>0</track></Slur>)");
    auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    auto *source_registry = master_of(**document).eid_register();
    auto *target = to_note(source_registry->item_from_eid(EID::from_std_string(std::string_view("I_I"))));
    ASSERT_TRUE(target);
    auto &source_unisons = target->chord()->notes();
    ASSERT_EQ(source_unisons.size(), 3u);
    const auto tied = std::ranges::find(source_unisons, target);
    ASSERT_NE(tied, source_unisons.end());
    // Make the tied note the second equal key on the wire, which the reader inserts third.
    std::iter_swap(tied, source_unisons.begin() + 1);
    auto legacy = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(legacy) << legacy.error().message;
    pugi::xml_document xml;
    ASSERT_TRUE(parse_export(xml, *legacy));
    const auto tie = xml.select_node("//Note[eid='E_E']/Spanner[@type='Tie']/next/location").node();
    ASSERT_TRUE(tie);
    EXPECT_EQ(tie.child("staves").text().as_int(), 1);
    EXPECT_EQ(tie.child("voices").text().as_int(), 1);
    EXPECT_EQ(tie.child("measures").text().as_int(), 1);
    const auto unisons = xml.select_nodes("//Chord[eid='H_H']/Note");
    ASSERT_EQ(unisons.size(), 3u);
    ASSERT_STREQ(unisons[1].node().child_value("eid"), "I_I");
    EXPECT_EQ(tie.child("notes").text().as_int(), 2);
    const auto reverse = xml.select_node("//Note[eid='I_I']/Spanner[@type='Tie']/prev/location").node();
    ASSERT_TRUE(reverse.child("grace"));
    EXPECT_EQ(reverse.child("grace").text().as_int(-1), 0);
    EXPECT_EQ(reverse.child("notes").text().as_int(), -2);
    const auto slur = xml.select_node("//Chord[eid='F_F']/Spanner[@type='Slur']/next/location").node();
    ASSERT_TRUE(slur);
    EXPECT_EQ(slur.child("staves").text().as_int(), 1);
    EXPECT_EQ(slur.child("voices").text().as_int(), 1);
    EXPECT_EQ(slur.child("measures").text().as_int(), 1);
    auto reopened = g_engine->open(legacy->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto *registry = master_of(**reopened).eid_register();
    const auto *end = to_note(registry->item_from_eid(EID::from_std_string(std::string_view("I_I"))));
    ASSERT_TRUE(end && end->tie_back_non_partial());
    EXPECT_EQ(end->tie_back_non_partial()->start_note()->eid().to_std_string(), "E_E");
    const auto *span = to_slur(registry->item_from_eid(EID::from_std_string(std::string_view("M_M"))));
    ASSERT_TRUE(span && span->end_element());
    EXPECT_EQ(span->end_element()->eid().to_std_string(), "H_H");
}

TEST(Export470, UsesInitialStaffTypeForLegacyNoteOrdinals) {
    using namespace mu::engraving;
    auto document = open_path(write470_fixture().parent_path() / "staff_type_change_tie.mscx");
    ASSERT_TRUE(document);
    const auto &source = master_of(*document);
    const auto *staff = source.staff(0);
    ASSERT_FALSE(staff->is_tab_staff(Fraction(0, 1)));
    ASSERT_TRUE(staff->is_tab_staff(Fraction(1, 1)));
    const auto source_note = [&](std::string_view id) { return to_note(source.eid_register()->item_from_eid(EID::from_std_string(id))); };
    const auto *high = source_note("E_E");
    const auto *low = source_note("F_F");
    ASSERT_TRUE(high && low);
    ASSERT_EQ(high->chord(), low->chord());
    ASSERT_GT(high->pitch(), low->pitch());
    ASSERT_GT(high->string(), low->string());
    const auto legacy = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(legacy) << legacy.error().message;
    pugi::xml_document xml;
    ASSERT_TRUE(parse_export(xml, *legacy));
    EXPECT_EQ(xml.select_node("//Note[eid='C_C']/Spanner/next/location/notes").node().text().as_int(-1), 1);
    EXPECT_EQ(xml.select_node("//NoteParenGroup/Notes/NoteIdx").node().text().as_int(-1), 1);
    const auto reopened = g_engine->open(legacy->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto *registry = master_of(**reopened).eid_register();
    const auto *start = to_note(registry->item_from_eid(EID::from_std_string(std::string_view("C_C"))));
    const auto *end = to_note(registry->item_from_eid(EID::from_std_string(std::string_view("E_E"))));
    ASSERT_TRUE(start && end && start->tie_for());
    EXPECT_EQ(start->tie_for()->end_note(), end);
    EXPECT_EQ(end->tie_back(), start->tie_for());
}

TEST(Export470, RefusesSpatiumDependentTextLineOverride) {
    constexpr std::string_view rest = "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>";
    const auto source =
        connector_score(rest, rest,
                        R"(<TextLine><eid>D_D</eid><track>0</track><track2>0</track2><startTick>0/1</startTick><ticks>1/2</ticks>
            <beginText>solo</beginText><textSizeIsSpatiumDependent>0</textSizeIsSpatiumDependent></TextLine>)");
    const auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    const auto result = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_FALSE(result);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    EXPECT_EQ(result.error().export_warnings[0].code, "B470_UNSUPPORTED_FEATURE");
    EXPECT_NE(result.error().message.find("TextLine/textSizeIsSpatiumDependent"), std::string::npos);
}

TEST(Export470, RejectsMismatchedNoteConnectorEndpoints) {
    auto document = open_path(write470_fixture().parent_path() / "staff_type_change_tie.mscx");
    ASSERT_TRUE(document);
    const auto native = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(native) << native.error().message;
    for (const auto *mutation : {"missing start", "foreign end", "wrong receiver"}) {
        SCOPED_TRACE(mutation);
        pugi::xml_document xml;
        ASSERT_TRUE(xml.load_buffer(native->data(), native->size()));
        auto tie = xml.select_node("//Note/Tie").node();
        ASSERT_TRUE(tie);
        if (std::string_view(mutation) == "missing start")
            tie.remove_child("startElement");
        else if (std::string_view(mutation) == "foreign end")
            tie.child("endElement").text().set("F_F");
        else {
            auto other = xml.select_node("//Note[eid='F_F']").node();
            ASSERT_TRUE(other.append_move(tie));
        }
        std::array<mu::engraving::Score *, 1> scores{&master_of(*document)};
        const auto issues = engrave::write470::reconstruct_connectors(xml.child("museScore"), scores, [] {});
        ASSERT_EQ(issues.size(), 1u);
        EXPECT_EQ(issues.front().detail, "Tie endpoint mapping");
        EXPECT_FALSE(xml.select_node("//Spanner[@type='Tie']"));
    }
}

TEST(Export470, PreservesCursorForInteriorMovesAndCreatesAnEmptyVoiceAnchor) {
    using namespace mu::engraving;
    const auto source = connector_score(
        R"(<Measure><voice>
              <Chord><eid>D_D</eid><durationType>half</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
              <Chord><eid>E_E</eid><durationType>half</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
            </voice></Measure>)",
        R"(<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>)",
        R"(<HairPin><eid>F_F</eid><subtype>0</subtype><track>0</track><track2>0</track2><startTick>1/8</startTick><ticks>3/8</ticks></HairPin>
            <Ottava><eid>G_G</eid><subtype>8va</subtype><track>2</track><track2>2</track2><startTick>0/1</startTick><ticks>3/4</ticks></Ottava>)");
    auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    auto &master = master_of(**document);
    auto *anchor = to_chord(master.eid_register()->item_from_eid(EID::from_std_string(std::string_view("D_D"))));
    ASSERT_TRUE(anchor);
    auto *partial = Factory::create_slur(master.dummy());
    std::ignore = partial->assign_new_eid();
    partial->set_partial_spanner_direction(PartialSpannerDirection::OUTGOING);
    partial->set_tick(anchor->tick());
    partial->set_tick2(anchor->tick());
    partial->set_track(anchor->track());
    partial->set_track2(anchor->track());
    partial->set_start_element(anchor);
    partial->set_end_element(anchor);
    const AuthoredChildKey segment{AuthoredChildRole::SPANNER_SEGMENT};
    partial->authored_overrides.set_carrier({segment, ElementType::SLUR_SEGMENT});
    partial->authored_overrides.set({segment, Pid::SLUR_UOFF1, muse::PointF(0.5, -1)});
    master.add_spanner(partial, false);
    const auto legacy = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(legacy) << legacy.error().message;
    pugi::xml_document xml;
    ASSERT_TRUE(parse_export(xml, *legacy));
    const auto measure = xml.select_node("//Score/Staff[@id='1']/Measure").node();
    auto first = measure.child("voice");
    auto second = first.next_sibling("voice");
    auto third = second.next_sibling("voice");
    ASSERT_TRUE(third);
    EXPECT_FALSE(second.first_child());
    EXPECT_STREQ(third.first_child().name(), "location");
    EXPECT_EQ(third.first_child().child("timeTick").text().as_int(), 1);
    EXPECT_FALSE(third.first_child().child("fractions"));
    const auto start = first.select_node("Spanner[@type='HairPin'][next]").node();
    ASSERT_TRUE(start);
    EXPECT_STREQ(start.previous_sibling().child_value("fractions"), "-3/8");
    EXPECT_STREQ(start.next_sibling().child_value("fractions"), "3/8");
    const auto slur = first.select_node("Chord[eid='D_D']/Spanner[@type='Slur'][next]").node();
    ASSERT_TRUE(slur);
    EXPECT_STREQ(slur.child("Slur").child_value("partialSpannerDirection"), "outgoing");
    EXPECT_TRUE(slur.child("Slur").child("SlurSegment").attribute("no"));
    ASSERT_TRUE(slur.child("next").child("location"));
    EXPECT_FALSE(slur.child("next").child("location").first_child());
    ASSERT_TRUE(slur.next_sibling("Spanner").child("prev").child("location"));
    const auto reopened = g_engine->open(legacy->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto expected = music::semantic_dump(&master_of(**document));
    const auto actual = music::semantic_dump(&master_of(**reopened));
    EXPECT_TRUE(music::json_equal(actual["events"], expected["events"]));
}

TEST(Export470, ValidatesAndTranslatesAllGuitarBendTokens) {
    const auto path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/bend.gp";
    auto document = open_path(path);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto native = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(native) << native.error().message;
    constexpr std::array tokens{"bend", "pre-bend", "grace-note-bend", "slight-bend", "dive", "pre-dive", "dip", "scoop", "unknown"};
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        SCOPED_TRACE(tokens[i]);
        pugi::xml_document xml;
        ASSERT_TRUE(xml.load_buffer(native->data(), native->size()));
        auto bend = xml.select_node("//Note/GuitarBend").node();
        ASSERT_TRUE(bend);
        bend.child("guitarBendType").text().set(tokens[i]);
        std::array<mu::engraving::Score *, 1> scores{&master};
        const auto issues = engrave::write470::reconstruct_connectors(xml.child("museScore"), scores, [] {});
        if (i == tokens.size() - 1) {
            ASSERT_EQ(issues.size(), 1u);
            EXPECT_EQ(issues.front().detail, "GuitarBend/guitarBendType");
        } else {
            ASSERT_TRUE(issues.empty());
            EXPECT_STREQ(bend.parent().attribute("type").value(), "GuitarBend");
            EXPECT_EQ(bend.child("guitarBendType").text().as_int(-1), i);
        }
    }
}

TEST(Export470, RebasesLinkedStartsToTheFirstEmittedMember) {
    using namespace mu::engraving;
    pugi::xml_document source;
    const auto fixture = read_text(write470_fixture().parent_path() / "attached_tracklists.mscx");
    ASSERT_TRUE(source.load_buffer(fixture.data(), fixture.size()));
    auto master = source.child("museScore").child("Score");
    auto excerpt = master.child("Score");
    constexpr std::string_view rest = "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>";
    for (auto score : {master, excerpt, excerpt.next_sibling("Score")}) {
        for (auto staff : score.children("Staff")) ASSERT_TRUE(staff.append_buffer(rest.data(), rest.size()));
    }
    constexpr std::string_view master_spanners =
        R"(<Pedal><eid>L_L</eid><track>0</track><track2>0</track2><startTick>1/1</startTick><ticks>1/2</ticks></Pedal>
           <Pedal><eid>M_M</eid><linkedTo>L_L</linkedTo><track>1</track><track2>1</track2><startTick>0/1</startTick><ticks>1/2</ticks></Pedal>)";
    ASSERT_TRUE(master.insert_child_before("SpannerMap", excerpt)
                    .append_buffer(master_spanners.data(), master_spanners.size(), pugi::parse_default | pugi::parse_fragment));
    constexpr std::string_view excerpt_spanners =
        R"(<Pedal><eid>N_N</eid><linkedTo>L_L</linkedTo><track>0</track><track2>0</track2><startTick>0/1</startTick><ticks>1/2</ticks></Pedal>
           <Pedal><eid>O_O</eid><linkedTo>L_L</linkedTo><track>1</track><track2>1</track2><startTick>0/1</startTick><ticks>1/2</ticks></Pedal>)";
    ASSERT_TRUE(excerpt.append_child("SpannerMap")
                    .append_buffer(excerpt_spanners.data(), excerpt_spanners.size(), pugi::parse_default | pugi::parse_fragment));
    std::ostringstream serialized;
    source.save(serialized);
    const auto document = g_engine->open(as_bytes(serialized.str()));
    ASSERT_TRUE(document) << document.error().message;
    const auto find = [](const MasterScore &score, std::string_view id) {
        return score.eid_register()->item_from_eid(EID::from_std_string(id));
    };
    const auto &original = master_of(**document);
    const auto *main = find(original, "L_L");
    ASSERT_TRUE(main && main->links());
    ASSERT_EQ(main->links()->size(), 4u);
    EXPECT_EQ(main->links()->main_element(), main);
    const auto legacy = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(legacy) << legacy.error().message;
    pugi::xml_document xml;
    ASSERT_TRUE(parse_export(xml, *legacy));
    const auto starts = xml.select_nodes("/museScore/Score/Staff//Spanner[@type='Pedal']/Pedal");
    ASSERT_EQ(starts.size(), 2u);
    EXPECT_STREQ(starts[0].node().child_value("eid"), "M_M");
    EXPECT_STREQ(starts[1].node().child_value("eid"), "L_L");
    EXPECT_FALSE(starts[0].node().child("linkedTo"));
    EXPECT_STREQ(starts[1].node().child_value("linkedTo"), "M_M");
    const auto reopened = g_engine->open(legacy->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto &full = master_of(**reopened);
    ASSERT_EQ(full.excerpts().size(), 2u);
    const auto *first = find(full, "M_M");
    ASSERT_TRUE(first && first->links());
    ASSERT_EQ(first->links()->size(), 4u);
    for (const auto [id, in_excerpt] : {std::pair{"L_L", false}, {"N_N", true}, {"O_O", true}}) {
        const auto *member = find(full, id);
        ASSERT_TRUE(member && member->is_spanner());
        EXPECT_EQ(member->score(), in_excerpt ? full.excerpts()[0]->excerpt_score() : &full);
        EXPECT_EQ(member->links(), first->links()) << id;
    }
    const auto standalone = (*document)->export_score_with_warnings(
        {.format = engrave::Format::MSCX, .score = {*engrave::element_id_from_string("F_F")}, .native_version = "4.70"});
    ASSERT_TRUE(standalone) << standalone.error().message;
    const auto selected = g_engine->open(standalone->bytes);
    ASSERT_TRUE(selected) << selected.error().message;
    const auto &part = master_of(**selected);
    EXPECT_TRUE(part.excerpts().empty());
    const auto *left = find(part, "N_N");
    const auto *right = find(part, "O_O");
    ASSERT_TRUE(left && right && left->links());
    EXPECT_EQ(left->links()->size(), 2u);
    EXPECT_EQ(left->links(), right->links());
}

TEST(Export470, PadsSparseLineSegmentsWithTheirDomSubtypes) {
    constexpr std::string_view rests = R"(<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
                                          <Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
                                          <Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>)";
    for (const bool has_subtype : {true, false}) {
        SCOPED_TRACE(has_subtype);
        const auto source =
            connector_score(rests, rests,
                            std::format(
                                R"(<Pedal><eid>D_D</eid><track>0</track><track2>0</track2><startTick>0/1</startTick><ticks>3/1</ticks>
            <Segment no="2">{}<offset x="1.25" y="-0.5"/></Segment></Pedal>)",
                                has_subtype ? "<subtype>3</subtype>" : ""));
        const auto document = g_engine->open(as_bytes(source));
        ASSERT_TRUE(document) << document.error().message;
        const auto legacy = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(legacy) << legacy.error().message;
        pugi::xml_document xml;
        ASSERT_TRUE(parse_export(xml, *legacy));
        const auto segments = xml.select_nodes("//Pedal/Segment");
        ASSERT_EQ(segments.size(), 3u);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            const auto segment = segments[i].node();
            EXPECT_FALSE(segment.attribute("no"));
            if (i < 2 || has_subtype)
                EXPECT_EQ(segment.child("subtype").text().as_int(), i + 1);
            else
                EXPECT_FALSE(segment.child("subtype"));
            EXPECT_EQ(bool(segment.child("offset")), i == 2);
        }
    }
}

TEST(Export470, RetainsGoldenLineFamiliesAndInlineLaissezVib) {
    using namespace mu::engraving;
    const auto corpus = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden";
    for (const auto &[file, tag] : std::array{std::pair{"layout_elements.mscx", "Glissando"}, std::pair{"coverage.musicxml", "LaissezVib"},
                                              std::pair{"repeat_with_2_voltas.mscx", "Volta"}}) {
        SCOPED_TRACE(file);
        auto document = open_path(corpus / file);
        ASSERT_TRUE(document);
        const auto legacy = document->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
        ASSERT_TRUE(legacy) << legacy.error().message;
        pugi::xml_document xml;
        ASSERT_TRUE(parse_export(xml, *legacy));
        const auto bodies = xml.select_nodes((std::string("//") + tag).c_str());
        ASSERT_FALSE(bodies.empty());
        for (const auto body : bodies) {
            const auto node = body.node();
            if (std::string_view(tag) == "LaissezVib") {
                EXPECT_STREQ(node.parent().name(), "Note");
            } else {
                EXPECT_STREQ(node.parent().attribute("type").value(), tag);
                EXPECT_STREQ(node.parent().parent().name(), std::string_view(tag) == "Volta" ? "voice" : "Note");
            }
        }
        auto reopened = g_engine->open(legacy->bytes);
        ASSERT_TRUE(reopened) << reopened.error().message;
        if (std::string_view(tag) == "Volta") {
            const auto &score = master_of(**reopened);
            int count = 0;
            for (const auto &[key, spanner] : score.spanner_map().map()) {
                if (!spanner->is_volta()) continue;
                ++count;
                const auto *end = to_measure(spanner->end_element());
                ASSERT_TRUE(end);
                EXPECT_EQ(spanner->tick2(), end->end_tick());
            }
            EXPECT_EQ(count, 2);
        }
    }
}

TEST(Export470, RoutesInteriorAndFinalVoltaEndsToDifferentMeasureCoordinates) {
    using mu::engraving::Fraction;
    constexpr std::string_view first = R"(<Measure><eid>D_D</eid><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
                                           <Measure><eid>E_E</eid><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>)";
    constexpr std::string_view second = R"(<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
                                            <Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>)";
    const auto source = connector_score(
        first, second,
        R"(<Volta><eid>F_F</eid><startElement>D_D</startElement><endElement>D_D</endElement><track>0</track><endings>1</endings></Volta>
            <Volta><eid>G_G</eid><startElement>E_E</startElement><endElement>E_E</endElement><track>0</track><endings>2</endings></Volta>)");
    auto document = g_engine->open(as_bytes(source));
    ASSERT_TRUE(document) << document.error().message;
    const auto legacy = (*document)->export_score_with_warnings({.format = engrave::Format::MSCX, .native_version = "4.70"});
    ASSERT_TRUE(legacy) << legacy.error().message;
    pugi::xml_document xml;
    ASSERT_TRUE(parse_export(xml, *legacy));
    const auto ends = xml.select_nodes("//voice/Spanner[@type='Volta'][prev]");
    ASSERT_EQ(ends.size(), 2u);
    for (const auto end : ends) EXPECT_STREQ(end.node().parent().parent().child_value("eid"), "E_E");
    const auto interior = ends[0].node().child("prev").child("location");
    EXPECT_EQ(interior.child("measures").text().as_int(), -1);
    EXPECT_FALSE(interior.child("fractions"));
    const auto final = ends[1].node().child("prev").child("location");
    EXPECT_FALSE(final.child("measures"));
    EXPECT_STREQ(final.child_value("fractions"), "-1/1");
    auto reopened = g_engine->open(legacy->bytes);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto &score = master_of(**reopened);
    const auto *registry = score.eid_register();
    for (const auto &[eid, tick] :
         std::array{std::pair{std::string_view("F_F"), Fraction{1, 1}}, std::pair{std::string_view("G_G"), Fraction{2, 1}}}) {
        const auto *object = registry->item_from_eid(mu::engraving::EID::from_std_string(eid));
        ASSERT_TRUE(object && object->is_volta());
        EXPECT_EQ(to_spanner(object)->tick2(), tick);
    }
}
