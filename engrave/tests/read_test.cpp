// The read API: outline(), query() and get(), against the A2 score code, which walks
// the same score independently.
//
//   engrave_read_test --assets=<resources> [--corpus=<file or dir>]...
//
// The corpus test compares a query with the semantic dump (engrave/src/score/
// score_semantic.cpp).  The comparison is exact and covers the element kinds both
// walks report: for every score of a document, master and excerpts,
//   - each chord, rest and measure repeat of every staff and voice, with its tick and
//     length in ticks;
//   - each note of those chords, with its pitch;
//   - each time signature, key signature and clef, with its tick, staff and value.
// A query over the same types must give one row per dump row with the same fields and
// no row of its own.  What the dump reports and a query does not is out of scope:
// annotations, spanners, frame elements, and everything layout builds.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <engrave/engrave.h>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/score/score_metadata.h"
#include "engrave/src/score/score_positions.h"
#include "engrave/src/score/score_semantic.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/property.h"
#include "engraving/dom/rangelock.h"
#include "engraving/dom/score.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/timesig.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/types/constants.h"

namespace {

using engrave::ElementId;
using engrave::ErrorCode;
using engrave::PropertyRead;
using mu::engraving::MasterScore;

// A whole note in the engine's ticks, which is what a Fraction in the API counts.
constexpr std::int64_t whole_note_ticks = 4 * mu::engraving::Constants::DIVISION;

std::filesystem::path g_assets;
std::vector<std::filesystem::path> g_corpus;
std::unique_ptr<engrave::Engine> g_engine;

// --- text, bytes and documents -------------------------------------------

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::byte> as_bytes(std::string_view text) {
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    return {data, data + text.size()};
}

std::unique_ptr<engrave::Document> open_path(const std::filesystem::path& path) {
    auto document = g_engine->open(path);
    EXPECT_TRUE(document) << path.string() << ": " << document.error().message;
    return document ? std::move(*document) : nullptr;
}

MasterScore& master_of(engrave::Document& document) {
    MasterScore* master = engrave::DocumentAccess::master_score(document);
    EXPECT_NE(master, nullptr);
    return *master;
}

std::int64_t ticks_of(const engrave::Fraction& value) {
    return value.num * whole_note_ticks / value.den;
}

std::filesystem::path write470_fixture() {
    return std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/write470/minimal.mscx";
}

TEST(Export470, WarningIdentifiersAreStableAndDistinct) {
    using namespace engrave::export_warning_code;
    const std::array<std::string_view, 10> actual{part_identity_omitted, page_lock, shared_part,
        stave_sharing_label, group_bracket, excerpt, automation, custom_chord_list,
        unsupported_style, unsupported_construct};
    const std::array<std::string_view, 10> expected{"engrave.470.part_identity_omitted", "engrave.470.page_lock",
        "engrave.470.shared_part", "engrave.470.stave_sharing_label", "engrave.470.group_bracket",
        "engrave.470.excerpt", "engrave.470.automation", "engrave.470.custom_chord_list",
        "engrave.470.unsupported_style", "engrave.470.unsupported_construct"};
    EXPECT_EQ(actual, expected);
    EXPECT_EQ((std::set<std::string_view>(actual.begin(), actual.end()).size()), actual.size());
}

TEST(Export470, SelectsVersionsAndPreservesTheDefaultWriter) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    for (auto format : {engrave::Format::Mscx, engrave::Format::Mscz}) {
        auto tick = std::time(nullptr) / 2;
        auto original = document->export_score({.format = format});
        auto explicit_500 = document->export_score_with_warnings({.format = format, .native_version = "5.00"});
        // ZIP timestamps have two-second precision, as in the existing capture tests.
        for (int attempt = 0; attempt < 2 && format == engrave::Format::Mscz
                              && tick != std::time(nullptr) / 2; ++attempt) {
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
        EXPECT_EQ(invalid.error().code, ErrorCode::Unsupported);
        EXPECT_NE(invalid.error().message.find("4.71"), std::string::npos);
    }
    const auto non_native = document->export_score_with_warnings({.format = engrave::Format::MusicXml,
                                                                  .native_version = "4.70"});
    ASSERT_FALSE(non_native);
    EXPECT_EQ(non_native.error().code, ErrorCode::Unsupported);
}

TEST(Export470, ConvertsInstrumentNamesAndReportsPartIdentityLoss) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    const auto revision = document->revision();
    const auto xml = document->export_score_with_warnings({.format = engrave::Format::Mscx, .native_version = "4.70"});
    if (!xml) {
        for (const auto& w : xml.error().export_warnings) ADD_FAILURE() << w.code << ": " << w.message;
        FAIL() << xml.error().message;
    }
    ASSERT_EQ(xml->warnings.size(), 1u);
    EXPECT_EQ(xml->warnings[0].code, "engrave.470.part_identity_omitted");
    EXPECT_TRUE(xml->warnings[0].object);
    const std::string text(reinterpret_cast<const char*>(xml->bytes.data()), xml->bytes.size());
    EXPECT_NE(text.find("version=\"4.70\""), std::string::npos);
    EXPECT_EQ(text.find("<InstrumentLabel>"), std::string::npos);
    EXPECT_NE(text.find("<longName>Piano</longName>"), std::string::npos);
    EXPECT_NE(text.find("<shortName>Pno.</shortName>"), std::string::npos);
    const auto archive = document->export_score_with_warnings({.format = engrave::Format::Mscz, .native_version = "4.70"});
    if (!archive) FAIL() << archive.error().message;
    ASSERT_EQ(archive->warnings.size(), 1u);
    EXPECT_EQ(archive->warnings[0].code, xml->warnings[0].code);
    EXPECT_EQ(archive->warnings[0].object, xml->warnings[0].object);
    const auto reopened = g_engine->open(archive->bytes, {.format = engrave::Format::Mscz});
    ASSERT_TRUE(reopened);
    EXPECT_EQ(document->revision(), revision);
    const auto legacy = document->export_score({.format = engrave::Format::Mscx, .native_version = "4.70"});
    ASSERT_FALSE(legacy);
    EXPECT_EQ(legacy.error().code, ErrorCode::Unsupported);
    ASSERT_EQ(legacy.error().export_warnings.size(), 1u);
    EXPECT_EQ(legacy.error().export_warnings[0].code, "engrave.470.part_identity_omitted");
    const auto bounded = document->export_score_with_warnings({.format = engrave::Format::Mscx,
        .capture = {.max_bytes = xml->bytes.size() + 1, .max_working_bytes = 16 * 1024 * 1024},
        .native_version = "4.70"});
    ASSERT_TRUE(bounded);
    EXPECT_EQ(bounded->bytes, xml->bytes);
    ASSERT_EQ(bounded->warnings.size(), 1u);
    EXPECT_EQ(bounded->warnings[0].code, xml->warnings[0].code);
    EXPECT_EQ(bounded->warnings[0].object, xml->warnings[0].object);
    const auto too_small = document->export_score_with_warnings({.format = engrave::Format::Mscx,
        .capture = {.max_bytes = 1024, .max_working_bytes = 16 * 1024 * 1024}, .native_version = "4.70"});
    ASSERT_FALSE(too_small);
    EXPECT_EQ(too_small.error().code, ErrorCode::ResourceLimit);
    const auto stopped = document->export_score_with_warnings({.format = engrave::Format::Mscz,
        .capture = {.cancelled = [] { return true; }}, .native_version = "4.70"});
    ASSERT_FALSE(stopped);
    EXPECT_EQ(stopped.error().code, ErrorCode::Cancelled);
}

TEST(Export470, RefusesPageLocksWithStableObjectWarnings) {
    auto source = read_text(write470_fixture());
    const auto start = source.find("<Measure>");
    const auto end = source.find("</Measure>", start);
    ASSERT_NE(start, std::string::npos);
    ASSERT_NE(end, std::string::npos);
    std::string second = source.substr(start, end + std::string_view("</Measure>").size() - start);
    for (const auto& [from, to] : {std::pair{"C_C", "H_H"}, {"D_D", "I_I"}, {"E_E", "J_J"},
                                  {"F_F", "K_K"}, {"G_G", "L_L"}}) {
        const auto pos = second.find(from);
        ASSERT_NE(pos, std::string::npos);
        second.replace(pos, std::char_traits<char>::length(from), to);
    }
    source.insert(end + std::string_view("</Measure>").size(), second);
    auto opened = g_engine->open(as_bytes(source), {.format = engrave::Format::Mscx});
    ASSERT_TRUE(opened);
    auto document = std::move(*opened);
    auto& master = master_of(*document);
    auto* measure = master.firstMeasure();
    ASSERT_NE(measure, nullptr);
    auto* next = measure->nextMeasure();
    ASSERT_NE(next, nullptr);
    const auto id = engrave::from_engine(measure->eid());
    const auto next_id = engrave::from_engine(next->eid());
    mu::engraving::RangeLock lock(measure, measure);
    mu::engraving::RangeLock next_lock(next, next);
    master.addPageLock(&lock);
    master.addPageLock(&next_lock);
    struct RemoveLock {
        MasterScore& score;
        const mu::engraving::RangeLock* first;
        const mu::engraving::RangeLock* second;
        ~RemoveLock() { score.removePageLock(first); score.removePageLock(second); }
    } remove_lock{master, &lock, &next_lock};
    for (int repeat = 0; repeat != 2; ++repeat) {
        const auto result = document->export_score_with_warnings({.format = engrave::Format::Mscz, .native_version = "4.70"});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::Unsupported);
        ASSERT_EQ(result.error().export_warnings.size(), 2u);
        EXPECT_EQ(result.error().export_warnings[0].code, "engrave.470.page_lock");
        ASSERT_TRUE(result.error().export_warnings[0].object);
        EXPECT_EQ(*result.error().export_warnings[0].object, id);
        EXPECT_EQ(result.error().export_warnings[1].code, "engrave.470.page_lock");
        ASSERT_TRUE(result.error().export_warnings[1].object);
        EXPECT_EQ(*result.error().export_warnings[1].object, next_id);
        EXPECT_EQ(master.pageLocks()->allLocks().size(), 2u);
    }
}

TEST(Export470, RefusesANonNeutralFiveOnlyStyle) {
    auto document = open_path(write470_fixture());
    ASSERT_TRUE(document);
    auto& master = master_of(*document);
    const auto score_id = engrave::from_engine(master.eid());
    master.style().set(mu::engraving::Sid::maskSlurs, true);
    const auto result = document->export_score_with_warnings({.format = engrave::Format::Mscx, .native_version = "4.70"});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::Unsupported);
    ASSERT_EQ(result.error().export_warnings.size(), 1u);
    const auto& warning = result.error().export_warnings.front();
    EXPECT_EQ(warning.code, "engrave.470.unsupported_style");
    EXPECT_NE(warning.message.find("maskSlurs"), std::string::npos);
    ASSERT_TRUE(warning.object);
    EXPECT_EQ(*warning.object, score_id);
    EXPECT_TRUE(master.style().styleB(mu::engraving::Sid::maskSlurs));
}

/// Checks the error a call must fail with, without reading .error() of a success.
template <typename T>
void expect_code(const engrave::Result<T>& result, ErrorCode expected, std::string_view what) {
    if (result) {
        ADD_FAILURE() << what << ": the call succeeded";
        return;
    }
    EXPECT_EQ(result.error().code, expected) << what << ": " << result.error().message;
}

// --- transactions --------------------------------------------------------

/// One transaction on the score, so that an engine command may run (the batch API is
/// S6's; a read test drives the primitive the same way the handle tests do).
class Command {
public:
    explicit Command(MasterScore& score) : m_score(score) {
        mu::engraving::MScore::setError(mu::engraving::MsError::MS_NO_ERROR);
        m_score.startCmd(muse::TranslatableString::untranslatable("engrave read test"));
    }
    ~Command() {
        if (m_open) m_score.endCmd(false, true);
    }
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;

    void commit() {
        m_score.endCmd(false, true);
        m_open = false;
        engrave::assign_missing_ids(m_score);
        m_score.undoStack()->clearHistory();
    }

private:
    MasterScore& m_score;
    bool m_open = true;
};

// --- rows of the oracle comparison ---------------------------------------

// One element of the comparison, as a line so that a difference prints readably.
using Row = std::string;

const char* type_name(const nlohmann::json& kind) {
    const std::string value = kind.get<std::string>();
    if (value == "chord") return "Chord";
    if (value == "rest") return "Rest";
    if (value == "measureRepeat") return "MeasureRepeat";
    return "unknown";
}

Row event_row(std::string_view type, int tick, std::size_t staff, std::size_t voice, int ticks) {
    return std::format("{} tick={} staff={} voice={} ticks={}", type, tick, staff, voice, ticks);
}

Row note_row(int tick, std::size_t staff, std::size_t voice, int pitch) {
    return std::format("Note tick={} staff={} voice={} pitch={}", tick, staff, voice, pitch);
}

std::string value_text(const nlohmann::json& value) {
    return value.is_string() ? value.get<std::string>() : value.dump();
}

// The rows the semantic dump reports for one score body.
void oracle_rows(const nlohmann::json& body, std::multiset<Row>& rows) {
    const nlohmann::json& events = body.at("events");
    for (std::size_t staff = 0; staff < events.size(); ++staff) {
        for (std::size_t voice = 0; voice < events[staff].size(); ++voice) {
            for (const nlohmann::json& item : events[staff][voice]) {
                const int tick = item.at("tick").get<int>();
                const int ticks = item.at("ticks").get<int>();
                rows.insert(event_row(type_name(item.at("kind")), tick, staff, voice, ticks));
                if (!item.contains("notes")) continue;
                for (const nlohmann::json& note : item.at("notes")) {
                    rows.insert(note_row(tick, staff, voice, note.at("pitch").get<int>()));
                }
            }
        }
    }
    for (const nlohmann::json& signature : body.at("signatures")) {
        const std::string type = signature.at("type").get<std::string>();
        const int tick = signature.at("tick").get<int>();
        const std::size_t staff = signature.at("staff").get<std::size_t>();
        if (type == "Clef") {
            // The clef token depends on the score's concert pitch setting; the tick and
            // the staff are what the walk has to agree on.
            rows.insert(std::format("Clef tick={} staff={}", tick, staff));
            continue;
        }
        rows.insert(std::format("{} tick={} staff={} value={}", type, tick, staff, value_text(signature.at("value"))));
    }
}

const PropertyRead* property(const engrave::ElementInfo& element, std::string_view name) {
    for (const PropertyRead& read : element.props) {
        if (read.name == name) return &read;
    }
    return nullptr;
}

std::int64_t int_property(const engrave::ElementInfo& element, std::string_view name) {
    const PropertyRead* read = property(element, name);
    EXPECT_NE(read, nullptr) << element.type << " " << engrave::to_string(element.id) << " has no " << name;
    if (!read || !std::holds_alternative<std::int64_t>(read->value)) return -1;
    return std::get<std::int64_t>(read->value);
}

// The rows a query reports, in the same form as oracle_rows.
void query_rows(const std::vector<engrave::ElementInfo>& elements, std::multiset<Row>& rows) {
    for (const engrave::ElementInfo& element : elements) {
        const auto tick = static_cast<int>(ticks_of(element.tick));
        const auto staff = static_cast<std::size_t>(element.staff);
        const auto voice = static_cast<std::size_t>(element.voice);
        if (element.type == "Note") {
            rows.insert(note_row(tick, staff, voice, static_cast<int>(int_property(element, "pitch"))));
        } else if (element.type == "Chord" || element.type == "Rest" || element.type == "MeasureRepeat") {
            rows.insert(event_row(element.type, tick, staff, voice, static_cast<int>(ticks_of(element.duration))));
        } else if (element.type == "Clef") {
            rows.insert(std::format("Clef tick={} staff={}", tick, staff));
        } else if (element.type == "KeySig") {
            rows.insert(std::format("KeySig tick={} staff={} value={}", tick, staff,
                                    int_property(element, "concertKey")));
        } else if (element.type == "TimeSig") {
            const PropertyRead* read = property(element, "timesig");
            EXPECT_NE(read, nullptr) << "a TimeSig has no timesig";
            const auto& sig = std::get<engrave::Fraction>(read->value);
            rows.insert(std::format("TimeSig tick={} staff={} value={}/{}", tick, staff, sig.num, sig.den));
        } else if (element.type == "Clef") {
            rows.insert(std::format("Clef tick={} staff={}", tick, staff));
        } else {
            ADD_FAILURE() << "the comparison covers no " << element.type;
        }
    }
}

/// Compares two row sets, reporting the rows whose counts differ rather than the whole
/// set.
void expect_same_rows(const std::multiset<Row>& expected, const std::multiset<Row>& actual, std::string_view what) {
    std::map<Row, int> wanted;
    for (const Row& row : expected) ++wanted[row];
    std::map<Row, int> got;
    for (const Row& row : actual) ++got[row];
    int reported = 0;
    for (const auto& [row, count] : wanted) {
        const auto found = got.find(row);
        const int mine = found == got.end() ? 0 : found->second;
        if (mine == count) continue;
        if (reported++ < 20) {
            ADD_FAILURE() << what << ": " << row << " appears " << mine << " time(s) in the query, " << count
                          << " in the oracle";
        }
    }
    for (const auto& [row, count] : got) {
        if (wanted.contains(row)) continue;
        if (reported++ < 20) ADD_FAILURE() << what << ": the query has " << row << " " << count << " time(s), the oracle none";
    }
    if (reported > 20) ADD_FAILURE() << what << ": " << reported - 20 << " more row(s) differ";
}

// --- reading a whole score ----------------------------------------------

constexpr std::string_view compared_types[] = {"Chord", "Rest", "MeasureRepeat", "Note", "Clef", "KeySig", "TimeSig"};
constexpr std::string_view compared_props[] = {"pitch", "timesig", "concertKey"};

std::vector<std::string> names(std::span<const std::string_view> values) {
    std::vector<std::string> result;
    for (std::string_view value : values) result.emplace_back(value);
    return result;
}

/// Every element of the comparison's types in one score, asked for measure window by
/// measure window so that a large score stays under the result cap, and page by page
/// so that paging is part of every corpus run.
std::vector<engrave::ElementInfo> read_all(engrave::Document& document, const engrave::ScoreSel& score,
                                           std::size_t measures) {
    std::vector<engrave::ElementInfo> elements;
    for (std::size_t first = 0; first < measures; first += engrave::max_query_measures) {
        engrave::Query request;
        request.score = score;
        request.types = names(compared_types);
        request.props = names(compared_props);
        request.first_measure = static_cast<std::int32_t>(first);
        request.measure_count = static_cast<std::int32_t>(std::min(engrave::max_query_measures, measures - first));
        for (std::size_t cursor = 0;;) {
            request.cursor = cursor;
            auto page = document.query(request);
            if (!page) {
                ADD_FAILURE() << "query: " << page.error().message;
                return elements;
            }
            elements.insert(elements.end(), std::make_move_iterator(page->elements.begin()),
                            std::make_move_iterator(page->elements.end()));
            if (!page->next_cursor) break;
            cursor = *page->next_cursor;
        }
    }
    return elements;
}

// --- corpus choice -------------------------------------------------------

std::filesystem::path corpus_file(std::initializer_list<std::string_view> extensions) {
    for (const auto& path : g_corpus) {
        if (extensions.size() == 0) return path;
        for (std::string_view extension : extensions) {
            if (path.extension() == extension) return path;
        }
    }
    return {};
}

/// The first corpus score that has an element of a type.
std::unique_ptr<engrave::Document> open_with(std::string_view type) {
    for (const auto& path : g_corpus) {
        auto document = g_engine->open(path);
        if (!document) continue;
        engrave::Query request;
        request.types = {std::string(type)};
        const auto found = (*document)->query(request);
        if (found && !found->elements.empty()) return std::move(*document);
    }
    return nullptr;
}

// --- the tests -----------------------------------------------------------

TEST(Read, EditRefreshesLayoutAfterAFontIsAdded) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto& path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    std::string xml = read_text(*it);
    const auto frame = xml.find("<height>10</height>");
    ASSERT_NE(frame, std::string::npos);
    xml.insert(frame, "<Text><style>title</style><text><font face=\"Petaluma Script\"/>MMMWWWiii012345</text></Text>");
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::Mscx});
    ASSERT_TRUE(opened);
    auto& document = **opened;
    auto* title = master_of(document).getText(mu::engraving::TextStyleType::TITLE);
    ASSERT_NE(title, nullptr);
    const double before = title->ldata()->bbox().width();
    ASSERT_GT(before, 0.0);
    const auto revision = document.revision();
    ASSERT_TRUE(g_engine->add_font(g_assets / "fonts/petaluma/PetalumaScript.otf"));
    EXPECT_EQ(document.revision(), revision);
    ASSERT_TRUE(document.edit({}));
    EXPECT_NE(title->ldata()->bbox().width(), before);
    EXPECT_EQ(document.revision(), revision);
}

TEST(Read, PositionsExpandRepeatsInRawPageUnitsWithoutEditing) {
    constexpr std::string_view xml = R"(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef>
</attributes><barline location="left"><repeat direction="forward"/></barline>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
<barline location="right"><repeat direction="backward"/></barline></measure></part>
</score-partwise>)";
    auto opened = g_engine->open(as_bytes(xml), {.format = engrave::Format::MusicXml});
    ASSERT_TRUE(opened);
    auto& document = **opened;
    const auto revision = document.revision();
    for (const auto kind : {engrave::PositionKind::Measure, engrave::PositionKind::Segment}) {
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
        if (kind == engrave::PositionKind::Segment) {
            EXPECT_EQ(positions->elements[0].x, std::trunc(positions->elements[0].x));
            EXPECT_EQ(positions->elements[0].y, std::trunc(positions->elements[0].y));
        }
    }
    EXPECT_EQ(document.revision(), revision);
}

TEST(Read, PositionsUsePageLocalCoordinatesOnLaterPages) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto& path) { return path.filename() == "slurs.mscz"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto pages = document->page_count();
    ASSERT_TRUE(pages);
    ASSERT_GE(*pages, 2);
    const auto positions = document->positions(engrave::PositionKind::Measure);
    ASSERT_TRUE(positions);
    EXPECT_NEAR(positions->page_width, 9921.264, 1.0);
    EXPECT_NEAR(positions->page_height, 1417.32, 1.0);
    const auto second_page = std::ranges::find_if(positions->elements, [](const auto& e) { return e.page == 1; });
    ASSERT_NE(second_page, positions->elements.end());
    EXPECT_NEAR(second_page->x, 708.6612, 1.0);
    EXPECT_NEAR(second_page->y, 578.6674, 1.0);
    EXPECT_LT(second_page->y, positions->page_height);
    for (const auto& element : positions->elements) {
        EXPECT_GE(element.page, 0);
        EXPECT_LT(element.page, *pages);
    }
    EXPECT_EQ(document->revision(), 1u);
}

TEST(Read, PositionCaptureLimitsAndCancellation) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    for (const auto kind : {engrave::PositionKind::Measure, engrave::PositionKind::Segment}) {
        const auto original = document->positions(kind);
        ASSERT_TRUE(original);
        ASSERT_FALSE(original->elements.empty());
        const auto size = engrave::position_payload_bytes(*original);
        ASSERT_GT(size, 0u);
        const auto rejected = document->positions(kind, {}, {.max_bytes = size - 1});
        expect_code(rejected, ErrorCode::ResourceLimit, "position payload limit");
        const auto bounded = document->positions(kind, {}, {.max_bytes = size + 1});
        ASSERT_TRUE(bounded);
        EXPECT_EQ(bounded->page_width, original->page_width);
        EXPECT_EQ(bounded->page_height, original->page_height);
        ASSERT_EQ(bounded->elements.size(), original->elements.size());
        ASSERT_EQ(bounded->events.size(), original->events.size());
        for (std::size_t i = 0; i < bounded->elements.size(); ++i) {
            const auto& a = bounded->elements[i];
            const auto& b = original->elements[i];
            EXPECT_EQ(a.id, b.id); EXPECT_EQ(a.page, b.page);
            EXPECT_DOUBLE_EQ(a.x, b.x); EXPECT_DOUBLE_EQ(a.y, b.y);
            EXPECT_DOUBLE_EQ(a.sx, b.sx); EXPECT_DOUBLE_EQ(a.sy, b.sy);
        }
        for (std::size_t i = 0; i < bounded->events.size(); ++i) {
            EXPECT_EQ(bounded->events[i].elid, original->events[i].elid);
            EXPECT_EQ(bounded->events[i].position, original->events[i].position);
        }
        int checks = 0;
        const auto cancelled = document->positions(kind, {}, {.cancelled = [&] { return ++checks == 3; }});
        expect_code(cancelled, ErrorCode::Cancelled, "cancelled position capture");
        EXPECT_EQ(checks, 3);
    }
}

TEST(Read, ExcerptPositionsMatchTheLegacyWriterAndSelectionErrors) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto& path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    ASSERT_FALSE(outline->excerpts.empty());
    const engrave::ScoreSel excerpt{outline->excerpts.front().id};
    const auto revision = document->revision();
    for (const auto kind : {engrave::PositionKind::Measure, engrave::PositionKind::Segment}) {
        const auto positions = document->positions(kind, excerpt);
        ASSERT_TRUE(positions);
        ASSERT_FALSE(positions->elements.empty());
        const auto pages = document->page_count(excerpt);
        ASSERT_TRUE(pages);
        auto selected = engrave::select_score(master_of(*document), excerpt);
        ASSERT_TRUE(selected);
        const auto legacy = music::score_positions(*selected, kind == engrave::PositionKind::Segment);
        EXPECT_DOUBLE_EQ(positions->page_width, legacy.at("pageSize").at("width").get<double>());
        EXPECT_DOUBLE_EQ(positions->page_height, legacy.at("pageSize").at("height").get<double>());
        EXPECT_NEAR(positions->page_width, 9921.2598, 0.1);
        EXPECT_NEAR(positions->page_height, 14031.4961, 0.1);
        ASSERT_EQ(positions->elements.size(), legacy.at("elements").size());
        ASSERT_EQ(positions->events.size(), legacy.at("events").size());
        for (std::size_t i = 0; i < positions->elements.size(); ++i) {
            const auto& element = positions->elements[i];
            const auto& expected = legacy.at("elements")[i];
            EXPECT_EQ(element.id, static_cast<int>(i));
            EXPECT_GE(element.page, 0);
            EXPECT_LT(element.page, *pages);
            EXPECT_DOUBLE_EQ(element.x, expected.at("x").get<double>());
            EXPECT_DOUBLE_EQ(element.y, expected.at("y").get<double>());
            EXPECT_DOUBLE_EQ(element.sx, expected.at("sx").get<double>());
            EXPECT_DOUBLE_EQ(element.sy, expected.at("sy").get<double>());
            EXPECT_EQ(element.page, expected.at("page").get<int>());
            if (kind == engrave::PositionKind::Segment) {
                EXPECT_EQ(element.x, std::trunc(element.x));
                EXPECT_EQ(element.y, std::trunc(element.y));
            }
        }
        for (std::size_t i = 0; i < positions->events.size(); ++i) {
            EXPECT_EQ(positions->events[i].elid, legacy.at("events")[i].at("elid").get<int>());
            EXPECT_EQ(positions->events[i].position, legacy.at("events")[i].at("position").get<int>());
        }
    }
    EXPECT_EQ(document->revision(), revision);
    const ElementId missing{0x1234, 0x5678};
    expect_code(document->positions(engrave::PositionKind::Measure, {missing}), ErrorCode::NotFound, "missing score");
    const auto notes = document->query({.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(notes);
    ASSERT_FALSE(notes->elements.empty());
    expect_code(document->positions(engrave::PositionKind::Measure, {notes->elements.front().id}),
                ErrorCode::InvalidArgument, "note is not a score");
    expect_code(document->positions(static_cast<engrave::PositionKind>(99)), ErrorCode::InvalidArgument, "unknown kind");

    // Keep the removed score alive in the engine's id registry while checking
    // that it is no longer reachable from the document, then restore ownership.
    MasterScore& master = master_of(*document);
    auto* removed = master.excerpts().front();
    master.removeExcerpt(removed);
    expect_code(document->outline(excerpt), ErrorCode::StaleHandle, "stale outline");
    expect_code(document->positions(engrave::PositionKind::Measure, excerpt), ErrorCode::StaleHandle, "stale score");
    master.addExcerpt(removed);
}

TEST(Read, PositionsRefuseAPoisonedDocument) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto& hooks = engrave::DocumentAccess::edit_hooks(*document);
    hooks.policy = engrave::api::Policy::Snapshot;
    hooks.fail_restore = true;
    hooks.before_step = [](std::size_t) { throw std::runtime_error("injected"); };
    EXPECT_ANY_THROW((void)document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
    hooks = {};
    expect_code(document->positions(engrave::PositionKind::Measure), ErrorCode::Poisoned, "poisoned document");
}

TEST(Read, ExcerptExportsReopenAsIndependentScores) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto& path) { return path.filename() == "chordline_path.mscx"; });
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
    const auto pitches = [](const engrave::QueryResult& notes) {
        std::vector<std::int64_t> result;
        for (const auto& note : notes.elements) {
            if (!note.props.empty()) result.push_back(std::get<std::int64_t>(note.props.front().value));
        }
        std::ranges::sort(result);
        return result;
    };
    const auto revision = document->revision();
    for (const auto format : {engrave::Format::Mscx, engrave::Format::Mscz, engrave::Format::MusicXml,
                              engrave::Format::Mxl, engrave::Format::Midi}) {
        SCOPED_TRACE(engrave::to_string(format));
        const auto exported = document->export_score({.format = format, .score = selection});
        ASSERT_TRUE(exported);
        if (format == engrave::Format::Mscx) {
            const std::string xml(reinterpret_cast<const char*>(exported->data()), exported->size());
            EXPECT_EQ(xml.find("<linkedTo>"), std::string::npos)
                << "this one-staff excerpt must not refer to master-score elements";
            const auto bounded = document->export_score({.format = format, .score = selection,
                .capture = {.max_bytes = exported->size() + 1}});
            ASSERT_TRUE(bounded);
            EXPECT_EQ(*bounded, *exported);
            expect_code(document->export_score({.format = format, .score = selection,
                .capture = {.max_bytes = exported->size() - 1}}), ErrorCode::ResourceLimit,
                "standalone excerpt output limit");
        }
        const auto reopened = g_engine->open(*exported, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        const auto part = (*reopened)->outline();
        ASSERT_TRUE(part);
        EXPECT_EQ(part->parts.size(), 1u);
        EXPECT_TRUE(part->excerpts.empty());
        if (format != engrave::Format::Midi) {
            EXPECT_EQ(part->parts.front().name, source->parts.front().name);
            EXPECT_EQ(part->measure_count, source->measure_count);
            EXPECT_EQ(part->title, source->title);
        }
        const auto notes = (*reopened)->query({.types = {"Note"}, .props = {"pitch"}});
        ASSERT_TRUE(notes);
        EXPECT_EQ(pitches(*notes), pitches(*source_notes));
        if (format == engrave::Format::Mscx || format == engrave::Format::Mscz) {
            EXPECT_DOUBLE_EQ(master_of(**reopened).style().styleS(mu::engraving::Sid::staffDistance).val(), 9.0);
            EXPECT_EQ(master_of(**reopened).metaTag(u"workTitle"), master_of(*document).metaTag(u"workTitle"));
            EXPECT_EQ(master_of(**reopened).metaTag(u"partName").toStdString(), with_excerpt->excerpts.front().name);
        }
    }
    EXPECT_EQ(document->revision(), revision);
    const ElementId missing{0x1234, 0x5678};
    expect_code(document->export_score({.format = engrave::Format::Mscz, .score = {missing}}),
                ErrorCode::NotFound, "missing excerpt export");
    const auto master_notes = document->query({.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(master_notes);
    ASSERT_FALSE(master_notes->elements.empty());
    expect_code(document->export_score({.format = engrave::Format::MusicXml,
                                        .score = {master_notes->elements.front().id}}),
                ErrorCode::InvalidArgument, "note is not an excerpt");
    auto* removed = master_of(*document).excerpts().front();
    master_of(*document).removeExcerpt(removed);
    expect_code(document->export_score({.format = engrave::Format::Mscz, .score = selection}),
                ErrorCode::StaleHandle, "stale excerpt export");
    master_of(*document).addExcerpt(removed);
}

TEST(Read, MscxMasterExportRetainsStyle) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto& path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto original = document->outline();
    ASSERT_TRUE(original);
    ASSERT_FALSE(original->excerpts.empty());
    ASSERT_TRUE(document->edit({{engrave::SetStyle{{}, "staffDistance", 9.0}}}));
    const auto exported = document->export_score({.format = engrave::Format::Mscx});
    ASSERT_TRUE(exported);
    const auto reopened = g_engine->open(*exported, {.format = engrave::Format::Mscx});
    ASSERT_TRUE(reopened);
    EXPECT_DOUBLE_EQ(master_of(**reopened).style().styleS(mu::engraving::Sid::staffDistance).val(), 9.0);
    const auto outline = (*reopened)->outline();
    ASSERT_TRUE(outline);
    ASSERT_EQ(outline->excerpts.size(), original->excerpts.size());
    EXPECT_EQ(outline->excerpts.front().name, original->excerpts.front().name);
}

TEST(Read, ScoreCaptureLimitsAndCancellation) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    for (const auto format : {engrave::Format::Mscx, engrave::Format::Mscz,
                              engrave::Format::MusicXml, engrave::Format::Mxl}) {
        SCOPED_TRACE(engrave::to_string(format));
        auto tick = std::time(nullptr) / 2;
        auto original = document->export_score({.format = format});
        ASSERT_TRUE(original);
        ASSERT_FALSE(original->empty());
        auto bounded = document->export_score({.format = format, .capture = {.max_bytes = original->size() + 1}});
        // ZIP stores the current time in two-second units. Retry a pair that
        // happened to straddle that boundary, so identical writers compare
        // exactly without masking any archive bytes.
        for (int attempt = 0; attempt < 2 && (format == engrave::Format::Mscz || format == engrave::Format::Mxl)
                              && tick != std::time(nullptr) / 2; ++attempt) {
            tick = std::time(nullptr) / 2;
            original = document->export_score({.format = format});
            ASSERT_TRUE(original);
            bounded = document->export_score({.format = format, .capture = {.max_bytes = original->size() + 1}});
        }
        ASSERT_TRUE(bounded) << bounded.error().message;
        EXPECT_EQ(*bounded, *original);
        const auto rejected = document->export_score({.format = format, .capture = {.max_bytes = original->size() - 1}});
        expect_code(rejected, ErrorCode::ResourceLimit, "score output limit");
        const auto workspace = document->export_score({.format = format, .capture = {.max_working_bytes = 64}});
        expect_code(workspace, ErrorCode::ResourceLimit, "score workspace limit");
        int checks = 0;
        const auto cancelled = document->export_score({.format = format,
            .capture = {.cancelled = [&] { return ++checks == 20; }}});
        expect_code(cancelled, ErrorCode::Cancelled, "cancelled score capture");
        EXPECT_EQ(checks, 20);
    }
    expect_code(document->export_score({.format = engrave::Format::Midi, .capture = {.max_bytes = 64}}),
                ErrorCode::Unsupported, "bounded MIDI capture");
    int checks = 0;
    EXPECT_THROW((void)document->export_score({.format = engrave::Format::Mscz,
        .capture = {.cancelled = [&]() -> bool {
            if (++checks == 20) throw std::runtime_error("capture callback failure");
            return false;
        }}}), std::runtime_error);
    EXPECT_EQ(checks, 20);
    EXPECT_TRUE(document->export_score({.format = engrave::Format::Mscz}));
}

TEST(Read, LargeScoreCaptureStopsBeforeMaterializingTheOutput) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    master_of(*document).setMetaTag(u"workTitle", muse::String::fromStdString(std::string(512 * 1024, 'x')));
    const auto rejected = document->export_score({.format = engrave::Format::Mscx,
                                                   .capture = {.max_bytes = 1024}});
    expect_code(rejected, ErrorCode::ResourceLimit, "large score output limit");
    const auto success = document->export_score({.format = engrave::Format::Mscx});
    ASSERT_TRUE(success);
    ASSERT_GT(success->size(), 512u * 1024u);
    const auto bounded = document->export_score({.format = engrave::Format::Mscx,
                                                 .capture = {.max_bytes = success->size() + 1}});
    ASSERT_TRUE(bounded);
    EXPECT_EQ(*bounded, *success);
    auto tick = std::time(nullptr) / 2;
    auto archive = document->export_score({.format = engrave::Format::Mscz});
    ASSERT_TRUE(archive);
    auto bounded_archive = document->export_score({.format = engrave::Format::Mscz,
        .capture = {.max_bytes = archive->size() + 1, .max_working_bytes = 16 * 1024 * 1024}});
    for (int attempt = 0; attempt < 2 && tick != std::time(nullptr) / 2; ++attempt) {
        tick = std::time(nullptr) / 2;
        archive = document->export_score({.format = engrave::Format::Mscz});
        ASSERT_TRUE(archive);
        bounded_archive = document->export_score({.format = engrave::Format::Mscz,
            .capture = {.max_bytes = archive->size() + 1, .max_working_bytes = 16 * 1024 * 1024}});
    }
    ASSERT_TRUE(bounded_archive);
    EXPECT_EQ(*bounded_archive, *archive);
}

TEST(Read, StandaloneExcerptKeepsLinksBetweenItsOwnStaves) {
    const auto it = std::ranges::find_if(g_corpus, [](const auto& path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(it, g_corpus.end());
    auto document = open_path(*it);
    ASSERT_TRUE(document);
    const auto full = document->outline();
    ASSERT_TRUE(full);
    ASSERT_FALSE(full->excerpts.empty());
    const engrave::ScoreSel selection{full->excerpts.front().id};
    const auto exported = document->export_score({.format = engrave::Format::Mscx, .score = selection});
    ASSERT_TRUE(exported);
    const std::string xml(reinterpret_cast<const char*>(exported->data()), exported->size());
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
            EXPECT_TRUE(seen.contains(xml.substr(link + 10, end - link - 10)))
                << "a standalone link points outside the score or forward";
            ++links;
            cursor = end + 11;
        }
    }
    ASSERT_GT(links, 0u);
    const auto reopened = g_engine->open(*exported, {.format = engrave::Format::Mscx});
    ASSERT_TRUE(reopened);
    const auto outline = (*reopened)->outline();
    ASSERT_TRUE(outline);
    ASSERT_EQ(outline->parts.size(), 1u);
    EXPECT_EQ(outline->parts.front().staves.size(), 2u);
    const auto notes = (*reopened)->query({.types = {"Note"}});
    ASSERT_TRUE(notes);
    EXPECT_TRUE(std::ranges::any_of(notes->elements, [](const auto& note) { return note.links.size() >= 2; }));
}

TEST(Read, MatchesTheSemanticOracle) {
    ASSERT_FALSE(g_corpus.empty());
    int scores = 0;
    std::size_t rows = 0;
    for (const auto& path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        const nlohmann::json dump = music::semantic_dump(&master_of(*document));

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
        for (const nlohmann::json& listed : dump.at("excerpts")) {
            if (!listed.contains("score")) continue;
            const std::string name = listed.at("name").get<std::string>();
            while (next < outline->excerpts.size() && outline->excerpts[next].name != name) ++next;
            if (next == outline->excerpts.size()) {
                ADD_FAILURE() << "the outline has no excerpt named '" << name << "'";
                continue;
            }
            const engrave::ExcerptInfo& excerpt = outline->excerpts[next++];
            const auto excerpt_outline = document->outline(engrave::ScoreSel{excerpt.id});
            ASSERT_TRUE(excerpt_outline) << excerpt_outline.error().message;
            std::multiset<Row> excerpt_expected;
            oracle_rows(listed.at("score"), excerpt_expected);
            std::multiset<Row> excerpt_actual;
            query_rows(read_all(*document, engrave::ScoreSel{excerpt.id}, excerpt_outline->measures.size()),
                       excerpt_actual);
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
    for (const auto& path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        MasterScore& master = master_of(*document);
        const nlohmann::json metadata = music::score_metadata(&master, std::vector<music::ScoreMetadataTrack>{});
        const auto outline = document->outline();
        ASSERT_TRUE(outline) << outline.error().message;

        // The metadata fields are the engine's own derivation.
        EXPECT_EQ(outline->title, metadata.at("title").get<std::string>());
        EXPECT_EQ(outline->subtitle, metadata.at("subtitle").get<std::string>());
        EXPECT_EQ(outline->composer, metadata.at("composer").get<std::string>());
        EXPECT_EQ(outline->poet, metadata.at("poet").get<std::string>());
        EXPECT_EQ(outline->timesig, metadata.at("timesig").get<std::string>());
        EXPECT_EQ(outline->mscore_version, metadata.at("mscoreVersion").get<std::string>());
        EXPECT_EQ(outline->file_version, metadata.at("fileVersion").get<int>());
        EXPECT_EQ(outline->pages, metadata.at("pages").get<int>());
        EXPECT_EQ(outline->measure_count, metadata.at("measures").get<int>());
        EXPECT_EQ(outline->key, metadata.at("keysig").get<int>());
        EXPECT_EQ(outline->duration, metadata.at("duration").get<double>());

        // Parts and staves are named as the metadata names them, and every id it hands
        // out resolves.
        const nlohmann::json& parts = metadata.at("parts");
        ASSERT_EQ(outline->parts.size(), parts.size());
        for (std::size_t i = 0; i < outline->parts.size(); ++i) {
            const engrave::PartInfo& part = outline->parts[i];
            EXPECT_EQ(part.name, parts[i].at("name").get<std::string>());
            EXPECT_EQ(part.instrument_id, parts[i].at("instrumentId").get<std::string>());
            EXPECT_TRUE(document->get(part.id).has_value()) << "the part id does not resolve";
            ASSERT_FALSE(part.staves.empty());
            for (const engrave::StaffInfo& staff : part.staves) {
                EXPECT_TRUE(document->get(staff.id).has_value()) << "the staff id does not resolve";
                EXPECT_EQ(staff.part, static_cast<std::int32_t>(i));
                EXPECT_GE(staff.lines, 0);
            }
        }

        // The measures are the score's own list, in order, and each id names the
        // measure at that index.
        ASSERT_EQ(outline->measures.size(), master.nmeasures());
        for (std::size_t i = 0; i < outline->measures.size(); ++i) {
            const engrave::MeasureInfo& measure = outline->measures[i];
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
        for (const nlohmann::json& signature : music::semantic_dump(&master).at("signatures")) {
            const std::string type = signature.at("type").get<std::string>();
            if (type == "Clef") continue;
            expected.insert(std::format("{} tick={} staff={}", type, signature.at("tick").get<int>(),
                                        signature.at("staff").get<std::size_t>()));
        }
        std::multiset<std::string> actual;
        for (const engrave::SignatureInfo& signature : outline->signatures) {
            if (signature.type == "Tempo") continue;
            actual.insert(std::format("{} tick={} staff={}", signature.type, static_cast<int>(ticks_of(signature.tick)),
                                      signature.staff));
        }
        EXPECT_EQ(expected, actual) << "the outline's signatures are not the dump's";

        // The tempo markings are the dump's annotations of that type.
        std::multiset<std::string> expected_tempi;
        for (const nlohmann::json& annotation : music::semantic_dump(&master).at("annotations")) {
            if (annotation.at("type").get<std::string>() != "Tempo") continue;
            expected_tempi.insert(std::format("tick={} bpm={} text={}", annotation.at("tick").get<int>(),
                                              annotation.at("bpm").get<double>(),
                                              annotation.at("text").get<std::string>()));
        }
        std::multiset<std::string> actual_tempi;
        for (const engrave::SignatureInfo& signature : outline->signatures) {
            if (signature.type != "Tempo") continue;
            actual_tempi.insert(std::format("tick={} bpm={} text={}", static_cast<int>(ticks_of(signature.tick)),
                                            signature.bpm, signature.text));
        }
        EXPECT_EQ(expected_tempi, actual_tempi) << "the outline's tempo markings are not the dump's";
    }
}

TEST(Read, FiltersAndPagingAgree) {
    auto document = open_path(corpus_file({".mscx"}));
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

    const auto ids = [](const std::vector<engrave::ElementInfo>& elements) {
        std::vector<std::string> result;
        for (const auto& element : elements) result.push_back(engrave::to_string(element.id));
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
    for (const auto& element : full->elements) notes_in_full += element.type == "Note";
    EXPECT_EQ(notes->total, notes_in_full);
    for (const auto& element : notes->elements) EXPECT_EQ(element.type, "Note");

    // Adjacent measure windows tile the score.
    std::vector<std::string> tiled;
    for (std::size_t first = 0; first < measures; ++first) {
        engrave::Query window;
        window.first_measure = static_cast<std::int32_t>(first);
        window.measure_count = 1;
        window.limit = engrave::max_query_results;
        const auto part = document->query(window);
        ASSERT_TRUE(part) << part.error().message;
        for (const auto& element : part->elements) {
            EXPECT_EQ(element.measure_index, static_cast<std::int32_t>(first));
            tiled.push_back(engrave::to_string(element.id));
        }
    }
    // Every element the whole-score query reported for a measure is in exactly one
    // window; the score and its parts and staves are in none.
    std::vector<std::string> in_measures;
    for (const auto& element : full->elements) {
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
        for (const auto& element : full->elements) expected += element.staff == 1;
        EXPECT_EQ(on_staff->total, expected);
        for (const auto& element : on_staff->elements) EXPECT_EQ(element.staff, 1);
    }
    engrave::Query voice;
    voice.voices = {2};
    voice.limit = engrave::max_query_results;
    const auto in_voice = document->query(voice);
    ASSERT_TRUE(in_voice) << in_voice.error().message;
    for (const auto& element : in_voice->elements) EXPECT_EQ(element.voice, 2);

    // A tick range keeps the elements that start inside it.
    engrave::Query ticks;
    ticks.from_tick = engrave::Fraction{1, 4};
    ticks.to_tick = engrave::Fraction{1, 2};
    ticks.limit = engrave::max_query_results;
    const auto in_ticks = document->query(ticks);
    ASSERT_TRUE(in_ticks) << in_ticks.error().message;
    std::size_t expected = 0;
    for (const auto& element : full->elements) {
        const double start = static_cast<double>(element.tick.num) / static_cast<double>(element.tick.den);
        expected += start >= 0.25 && start < 0.5;
    }
    EXPECT_EQ(in_ticks->total, expected);
}

TEST(Read, ExtremeTickRangesAndMeasureWindows) {
    const auto fixture = std::ranges::find_if(g_corpus, [](const auto& path) {
        return path.filename() == "linked_parts.mscx";
    });
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
    const auto at_one = std::ranges::count_if(all->elements, [](const auto& element) {
        return element.tick.num == element.tick.den;
    });
    ASSERT_GT(at_one, 0);
    EXPECT_EQ(narrow->total, static_cast<std::size_t>(at_one));
    std::swap(request.from_tick, request.to_tick);
    expect_code(document->query(request), ErrorCode::InvalidArgument, "reversed extreme bounds");

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
        expect_code(document->query(request), ErrorCode::InvalidArgument, "invalid lower denominator");
        request.from_tick.reset();
        request.to_tick = engrave::Fraction{0, denominator};
        expect_code(document->query(request), ErrorCode::InvalidArgument, "invalid upper denominator");
        request.to_tick.reset();
    }
    const auto max32 = std::numeric_limits<std::int32_t>::max();
    const auto min32 = std::numeric_limits<std::int32_t>::min();
    for (const auto [first, count] : {std::pair{max32, std::int32_t{1}}, std::pair{max32, max32},
                                      std::pair{min32, min32}, std::pair{std::int32_t{0}, min32}}) {
        request.first_measure = first;
        request.measure_count = count;
        expect_code(document->query(request), ErrorCode::InvalidArgument, "out-of-range measure window");
    }
}

TEST(Read, FractionPropertyDefaultsKeepWrittenTerms) {
    const auto fixture = std::ranges::find_if(g_corpus, [](const auto& path) {
        return path.filename() == "linked_parts.mscx";
    });
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
    auto* signature = static_cast<mu::engraving::TimeSig*>(found->object);
    signature->setSig(mu::engraving::Fraction(2, 2));
    const auto written = document->get(id, {"timesig"});
    ASSERT_TRUE(written);
    ASSERT_EQ(written->size(), 1u);
    const auto actual = std::get<engrave::Fraction>(written->front().value);
    EXPECT_EQ(actual.num, 2);
    EXPECT_EQ(actual.den, 2);
    EXPECT_FALSE(written->front().is_default); // the default is written as 4/4

    signature->setSig(mu::engraving::Fraction(4, 4));
    const auto default_value = document->get(id, {"timesig"});
    ASSERT_TRUE(default_value);
    ASSERT_EQ(default_value->size(), 1u);
    EXPECT_TRUE(default_value->front().is_default);
}

TEST(Read, LimitsAreEnforced) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    MasterScore& master = master_of(*document);
    const auto outline = document->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto measures = static_cast<std::int32_t>(outline->measures.size());

    engrave::Query request;
    request.limit = 0;
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a limit of 0");
    request.limit = engrave::max_query_results + 1;
    expect_code(document->query(request), ErrorCode::ResourceLimit, "a limit over the cap");
    request.limit = engrave::max_query_results;
    request.types = {"NoSuchType"};
    expect_code(document->query(request), ErrorCode::InvalidArgument, "an unknown type");
    request.types = {};
    request.props = {"noSuchProperty"};
    expect_code(document->query(request), ErrorCode::NotFound, "an unknown property name");
    request.props = {};
    request.staves = {99};
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a staff the score has not");
    request.staves = {};
    request.voices = {4};
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a voice over 3");
    request.voices = {};
    request.first_measure = 0;
    request.measure_count = static_cast<std::int32_t>(engrave::max_query_measures) + 1;
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a window over the measure cap");
    request.measure_count = measures + 1;
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a window past the last measure");
    request.measure_count.reset();
    request.first_measure = measures + 1;
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a window that starts past the end");
    request.first_measure.reset();
    request.from_tick = engrave::Fraction{1, 2};
    request.to_tick = engrave::Fraction{1, 4};
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a reversed tick range");
    request.from_tick.reset();
    request.to_tick.reset();
    request.cursor = 100000;
    expect_code(document->query(request), ErrorCode::InvalidArgument, "a cursor past the end");
    request.cursor = 0;

    // A score with more elements than one query may report is a ResourceLimit, not a
    // partial answer.  Measures are appended through the engine's own command; the
    // batch API that would do this for a client is S6's.
    {
        Command command(master);
        for (std::size_t i = 0; i <= engrave::max_query_results; ++i) {
            if (!master.Score::insertMeasure(mu::engraving::ElementType::MEASURE, nullptr)) {
                GTEST_SKIP() << "the engine did not append a measure";
            }
        }
        command.commit();
    }
    engrave::Query everything;
    everything.types = {"Measure"};
    const auto tooMany = document->query(everything);
    ASSERT_FALSE(tooMany) << "a query over " << everything.types.front() << " elements succeeded";
    EXPECT_EQ(tooMany.error().code, ErrorCode::ResourceLimit);
    EXPECT_FALSE(tooMany.error().message.empty());

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
    MasterScore& master = master_of(*document);

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
    for (const PropertyRead& read : *all) names.insert(read.name);
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
        dynamic->object->undoResetProperty(mu::engraving::Pid::MIN_DISTANCE);
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
        dynamic->object->undoChangeProperty(mu::engraving::Pid::MIN_DISTANCE,
                                            mu::engraving::PropertyValue::fromValue(2.5),
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
        dynamic->object->undoResetProperty(mu::engraving::Pid::MIN_DISTANCE);
        command.commit();
    }
    auto reset = document->get(dynamic_id, {"minDistance"});
    ASSERT_TRUE(reset) << reset.error().message;
    EXPECT_TRUE((*reset)[0].styled) << "a reset property did not go back to its style";
    EXPECT_TRUE((*reset)[0].is_default) << "a reset property is not the default again";

    // A property name the catalogue does not know, and one the type does not have.
    const auto unknown = document->get(note_id, {"noSuchProperty"});
    expect_code(unknown, ErrorCode::NotFound, "an unknown property name");
    const auto foreign = document->get(dynamic_id, {"concertClefType"});
    expect_code(foreign, ErrorCode::InvalidArgument, "a property of another type");
    // A reference only a batch can use.
    expect_code(document->get(engrave::OpRef{}), ErrorCode::InvalidArgument, "an OpRef");
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
    for (const auto& measure : measures->elements) {
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
    for (const auto& measure : bare->elements) EXPECT_FALSE(measure.box);
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
    expect_code(document->get(missing), ErrorCode::NotFound, "an id nothing has");
    engrave::Query window;
    window.from_measure = missing;
    expect_code(document->query(window), ErrorCode::NotFound, "a measure id nothing has");
    // A selection has to name a score.
    engrave::Query wrong_score;
    wrong_score.score = engrave::ScoreSel{note_id};
    expect_code(document->query(wrong_score), ErrorCode::InvalidArgument, "a selection that names a note");
    wrong_score.score = engrave::ScoreSel{missing};
    expect_code(document->query(wrong_score), ErrorCode::NotFound, "a selection nothing has");
    expect_code(document->outline(engrave::ScoreSel{missing}), ErrorCode::NotFound, "an outline of nothing");
    expect_code(document->outline(engrave::ScoreSel{note_id}), ErrorCode::InvalidArgument, "an outline of a note");
    // A measure window names a measure of this score.
    engrave::Query not_a_measure;
    not_a_measure.from_measure = note_id;
    expect_code(document->query(not_a_measure), ErrorCode::InvalidArgument, "a window that names a note");
    engrave::Query both_forms;
    both_forms.first_measure = 0;
    both_forms.from_measure = outline->measures.front().id;
    expect_code(document->query(both_forms), ErrorCode::InvalidArgument, "a window named twice");

    // A page, a system and a segment that the layout does not have are stale.
    expect_code(document->get(engrave::Address{engrave::PageAddress{.score = {}, .index = 9999}}),
                ErrorCode::StaleHandle, "a page past the end");
    expect_code(document->get(engrave::Address{engrave::SystemAddress{.score = {}, .page = 9999, .index = 0}}),
                ErrorCode::StaleHandle, "a system on no page");
    expect_code(document->get(engrave::Address{engrave::SegmentAddress{
                    .measure = outline->measures.front().id, .tick = engrave::Fraction{1000, 1}, .kind = "ChordRest"}}),
                ErrorCode::StaleHandle, "a segment at no tick");
    // The first page and the first measure's own segment do resolve.
    const auto first_page = document->get(engrave::Address{engrave::PageAddress{.score = {}, .index = 0}});
    ASSERT_TRUE(first_page) << first_page.error().message;
    EXPECT_FALSE(first_page->empty());
    const auto first_segment = document->get(engrave::Address{engrave::SegmentAddress{
        .measure = outline->measures.front().id, .tick = engrave::Fraction{0, 1}, .kind = "ChordRest"}});
    ASSERT_TRUE(first_segment) << first_segment.error().message;

    // An id of a document that is gone names nothing in another one.
    const auto other = corpus_file({".musicxml"});
    ASSERT_FALSE(other.empty());
    auto second = open_path(other);
    ASSERT_TRUE(second);
    expect_code(second->get(note_id), ErrorCode::NotFound, "an id of another document");
}

// A leading omitted rest must remain part of the triplet. Otherwise measure
// repair starts its duration at tick 320 and overwrites the A4 at tick 1280.
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
        const std::multiset<Row> expected = {
            note_row(320, 0, 0, 74), note_row(640, 0, 0, 72),
            note_row(960, 0, 0, 72), note_row(1280, 0, 0, 69), note_row(1600, 0, 0, 67)
        };
        EXPECT_EQ(actual, expected);
        query.types = {"Chord"};
        const auto chords = (*document)->query(query);
        ASSERT_TRUE(chords);
        ASSERT_EQ(chords->elements.size(), 5u);
        for (const auto& chord : chords->elements) EXPECT_EQ(ticks_of(chord.duration), 320);
        query.types = {"Rest"};
        const auto rests = (*document)->query(query);
        ASSERT_TRUE(rests);
        ASSERT_EQ(rests->elements.size(), 1u);
        EXPECT_EQ(ticks_of(rests->elements.front().tick), 0);
        EXPECT_EQ(ticks_of(rests->elements.front().duration), 320);
        const auto saved = (*document)->export_score({.format = engrave::Format::Mscz});
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

// --- corpus discovery ----------------------------------------------------

bool looks_like_score(const std::filesystem::path& path) {
    static const std::set<std::string> extensions = {".mscx", ".mscz", ".musicxml", ".mxl", ".mid", ".midi",
                                                     ".gp", ".gpx", ".gp3", ".gp4", ".gp5", ".ptb"};
    return extensions.contains(path.extension().string());
}

void collect(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) files.push_back(entry.path());
        std::ranges::sort(files);
        for (const auto& file : files) collect(file);
        return;
    }
    if (std::filesystem::is_regular_file(path, ec) && looks_like_score(path)) g_corpus.push_back(path);
}

} // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument.starts_with("--assets=")) {
            g_assets = std::filesystem::path(argument.substr(9));
        } else if (argument.starts_with("--corpus=")) {
            collect(std::filesystem::path(argument.substr(9)));
        }
    }
    if (g_corpus.empty()) {
        for (const auto& candidate : {std::filesystem::path("fixtures/golden"), std::filesystem::path("../fixtures/golden"),
                                      std::filesystem::path("../../fixtures/golden")}) {
            if (std::filesystem::is_directory(candidate)) {
                collect(candidate);
                break;
            }
        }
    }
    if (g_corpus.empty()) {
        std::fprintf(stderr, "no corpus: pass --corpus=<file or directory>\n");
        return 1;
    }

    auto engine = engrave::Engine::create({.resources = g_assets});
    if (!engine) {
        std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", engine.error().message.c_str());
        return 1;
    }
    g_engine = std::move(*engine);
    const int result = RUN_ALL_TESTS();
    g_engine.reset();
    return result;
}
