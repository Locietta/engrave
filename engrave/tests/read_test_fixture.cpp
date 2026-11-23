#include "read_test_fixture.h"

#include "engraving/dom/dummyparent.h"
#include "global/btree.h"
#include "global/flatmap.h"

namespace read_test {

std::filesystem::path g_assets;

std::vector<std::filesystem::path> g_corpus;

engrave::Engine *g_engine = nullptr;

std::string read_text(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::byte> as_bytes(std::string_view text) {
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    return {data, data + text.size()};
}

std::string_view as_text(const std::vector<std::byte> &bytes) {
    if (bytes.empty()) return {};
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

std::string normalize_lf(std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (const char ch : text) {
        if (ch != '\r') normalized.push_back(ch);
    }
    return normalized;
}

std::unique_ptr<engrave::Document> open_path(const std::filesystem::path &path) {
    auto document = g_engine->open(path);
    EXPECT_TRUE(document) << path.string() << ": " << document.error().message;
    if (document) EXPECT_TRUE((*document)->page_count());
    return document ? std::move(*document) : nullptr;
}

MasterScore &master_of(engrave::Document &document) {
    MasterScore *master = engrave::DocumentAccess::master_score(document);
    EXPECT_NE(master, nullptr);
    return *master;
}

std::int64_t ticks_of(const engrave::Fraction &value) { return value.num * whole_note_ticks / value.den; }

std::filesystem::path write470_fixture() {
    return std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/write470/minimal.mscx";
}

std::string formatted_text_fixture() {
    auto source = read_text(write470_fixture());
    source.insert(source.find("<Chord>"),
                  "<StaffText><text><b><font size=\"20\"/><font face=\"Edwin\"/>hello</b>\nworld</text></StaffText>");
    return source;
}

mu::engraving::TextBase *staff_text_of(MasterScore &master) {
    for (auto *item : master.first_measure()->first(mu::engraving::SegmentType::CHORD_REST)->annotations()) {
        if (item->is_staff_text()) return mu::engraving::to_text_base(item);
    }
    return nullptr;
}

std::string mmr_source(bool prefix_note) {
    auto source = read_text(write470_fixture());
    source.insert(source.find("</Style>"), "<createMultiMeasureRests>1</createMultiMeasureRests>");
    constexpr std::string_view rest = "<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>";
    if (!prefix_note) {
        const auto begin = source.find("<Chord>");
        const auto end = source.find("</Chord>", begin) + std::string("</Chord>").size();
        source.replace(begin, end - begin, "<Rest><durationType>measure</durationType></Rest>");
    }
    source.insert(source.find("</Staff>", source.find("<Measure>")),
                  prefix_note ? std::string(rest) + std::string(rest) : std::string(rest));
    return source;
}

// Inspect ownership rather than scan_elements(), which omits layout-only groups.
size_t grace_group_count(const mu::engraving::EngravingObject *object) {
    size_t result = object->is_grace_notes_group() ? 1 : 0;
    for (const auto *child : object->children()) {
        result += grace_group_count(child);
    }
    if (object->is_segment()) {
        const auto *segment = mu::engraving::to_segment(object);
        for (mu::engraving::track_idx_t track = 0; track < segment->pre_appended_items().size(); ++track) {
            const auto *item = segment->pre_appended_item(track);
            EXPECT_TRUE(!item || !item->is_grace_notes_group());
        }
    }
    return result;
}

mu::engraving::Chord *add_test_grace(mu::engraving::Chord *chord, mu::engraving::NoteType type) {
    using namespace mu::engraving;
    Chord *grace = Factory::copy_chord(*chord);
    grace->set_note_type(type);
    grace->set_duration_type(DurationType::V_EIGHTH);
    grace->set_ticks(Fraction(1, 8));
    chord->add(grace);
    return grace;
}

const char *type_name(const music::Json &kind) {
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

std::string value_text(const music::Json &value) { return value.is_string() ? value.get<std::string>() : music::dump_json(value); }

// The rows the semantic dump reports for one score body.
void oracle_rows(const music::Json &body, muse::BTreeMultiSet<Row> &rows) {
    const music::Json &events = body.at("events");
    for (std::size_t staff = 0; staff < events.size(); ++staff) {
        for (std::size_t voice = 0; voice < events[staff].size(); ++voice) {
            for (const music::Json &item : events[staff][voice].get_array()) {
                const int tick = item.at("tick").as<int>();
                const int ticks = item.at("ticks").as<int>();
                rows.insert(event_row(type_name(item.at("kind")), tick, staff, voice, ticks));
                if (!item.contains("notes")) continue;
                for (const music::Json &note : item.at("notes").get_array()) {
                    rows.insert(note_row(tick, staff, voice, note.at("pitch").as<int>()));
                }
            }
        }
    }
    for (const music::Json &signature : body.at("signatures").get_array()) {
        const std::string type = signature.at("type").get<std::string>();
        const int tick = signature.at("tick").as<int>();
        const std::size_t staff = signature.at("staff").as<std::size_t>();
        if (type == "Clef") {
            // The clef token depends on the score's concert pitch setting; the tick and
            // the staff are what the walk has to agree on.
            rows.insert(std::format("Clef tick={} staff={}", tick, staff));
            continue;
        }
        rows.insert(std::format("{} tick={} staff={} value={}", type, tick, staff, value_text(signature.at("value"))));
    }
}

const PropertyRead *property(const engrave::ElementInfo &element, std::string_view name) {
    for (const PropertyRead &read : element.props) {
        if (read.name == name) return &read;
    }
    return nullptr;
}

std::int64_t int_property(const engrave::ElementInfo &element, std::string_view name) {
    const PropertyRead *read = property(element, name);
    EXPECT_NE(read, nullptr) << element.type << " " << engrave::to_string(element.id) << " has no " << name;
    if (!read || !std::holds_alternative<std::int64_t>(read->value)) return -1;
    return std::get<std::int64_t>(read->value);
}

// The rows a query reports, in the same form as oracle_rows.
void query_rows(const std::vector<engrave::ElementInfo> &elements, muse::BTreeMultiSet<Row> &rows) {
    for (const engrave::ElementInfo &element : elements) {
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
            rows.insert(std::format("KeySig tick={} staff={} value={}", tick, staff, int_property(element, "concertKey")));
        } else if (element.type == "TimeSig") {
            const PropertyRead *read = property(element, "timesig");
            EXPECT_NE(read, nullptr) << "a TimeSig has no timesig";
            const auto &sig = std::get<engrave::Fraction>(read->value);
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
void expect_same_rows(const muse::BTreeMultiSet<Row> &expected, const muse::BTreeMultiSet<Row> &actual, std::string_view what) {
    muse::FlatMap<Row, int> wanted;
    for (const Row &row : expected) ++wanted[row];
    muse::FlatMap<Row, int> got;
    for (const Row &row : actual) ++got[row];
    int reported = 0;
    for (const auto &[row, count] : wanted) {
        const auto found = got.find(row);
        const int mine = found == got.end() ? 0 : found->second;
        if (mine == count) continue;
        if (reported++ < 20) {
            ADD_FAILURE() << what << ": " << row << " appears " << mine << " time(s) in the query, " << count << " in the oracle";
        }
    }
    for (const auto &[row, count] : got) {
        if (wanted.contains(row)) continue;
        if (reported++ < 20) ADD_FAILURE() << what << ": the query has " << row << " " << count << " time(s), the oracle none";
    }
    if (reported > 20) ADD_FAILURE() << what << ": " << reported - 20 << " more row(s) differ";
}

std::vector<std::string> names(std::span<const std::string_view> values) {
    std::vector<std::string> result;
    for (std::string_view value : values) result.emplace_back(value);
    return result;
}

/// Every element of the comparison's types in one score, asked for measure window by
/// measure window so that a large score stays under the result cap, and page by page
/// so that paging is part of every corpus run.
std::vector<engrave::ElementInfo> read_all(engrave::Document &document, const engrave::ScoreSel &score, std::size_t measures) {
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
            elements.insert(elements.end(), std::make_move_iterator(page->elements.begin()), std::make_move_iterator(page->elements.end()));
            if (!page->next_cursor) break;
            cursor = *page->next_cursor;
        }
    }
    return elements;
}

std::filesystem::path corpus_file(std::initializer_list<std::string_view> extensions) {
    for (const auto &path : g_corpus) {
        if (extensions.size() == 0) return path;
        for (std::string_view extension : extensions) {
            if (path.extension() == extension) return path;
        }
    }
    return {};
}

/// The first corpus score that has an element of a type.
std::unique_ptr<engrave::Document> open_with(std::string_view type) {
    for (const auto &path : g_corpus) {
        auto document = g_engine->open(path);
        if (!document) continue;
        engrave::Query request;
        request.types = {std::string(type)};
        const auto found = (*document)->query(request);
        if (found && !found->elements.empty()) return std::move(*document);
    }
    return nullptr;
}

std::uint64_t layout_count(const mu::engraving::Score &score) { return score.layout_state.whole_count + score.layout_state.range_count; }

std::string dynamics_musicxml(std::string_view direction, std::string_view notations, std::string_view note_attributes) {
    return std::format(R"xml(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef>
</attributes>{}<note {}><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration>
<type>whole</type><notations>{}</notations></note></measure></part></score-partwise>)xml",
                       direction, note_attributes, notations);
}

std::vector<mu::engraving::Dynamic *> imported_dynamics(engrave::Document &document) {
    engrave::Query query;
    query.types = {"Dynamic"};
    const auto result = document.query(query);
    EXPECT_TRUE(result);
    std::vector<mu::engraving::Dynamic *> dynamics;
    if (!result) return dynamics;
    for (const auto &row : result->elements) {
        const auto element = engrave::resolve(master_of(document), row.id);
        EXPECT_TRUE(element);
        if (element) dynamics.push_back(static_cast<mu::engraving::Dynamic *>(element->object));
    }
    return dynamics;
}

// Parse the exported document before reopening it: successful import alone does
// not establish that literal text was serialized as well-formed XML.
std::unique_ptr<engrave::Document> dynamics_native_roundtrip(engrave::Document &document, std::string_view expected_text,
                                                             engrave::Format format) {
    const auto saved = document.export_score({.format = format});
    EXPECT_TRUE(saved);
    if (!saved) return nullptr;
    if (format == engrave::Format::MSCX && !expected_text.empty()) {
        const std::string xml(reinterpret_cast<const char *>(saved->data()), saved->size());
        EXPECT_NE(xml.find(std::format("<text>{}</text>", expected_text)), std::string::npos);
    }
    if (format == engrave::Format::MSCX) {
        const muse::ByteArray bytes(reinterpret_cast<const char *>(saved->data()), saved->size());
        muse::XmlStreamReader reader(bytes);
        while (!reader.at_end()) reader.read_next();
        EXPECT_FALSE(reader.is_error()) << reader.error_string().to_std_string();
    }
    auto reopened = g_engine->open(*saved, {.format = format});
    EXPECT_TRUE(reopened);
    return reopened ? std::move(*reopened) : nullptr;
}

std::string chordline_offset_fixture() {
    return read_text(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests/data/chordline-offsets/offsets-500.mscx");
}

std::vector<mu::engraving::ChordLine *> offset_test_lines(mu::engraving::Score &score) {
    std::vector<mu::engraving::ChordLine *> lines;
    score.scan_elements([&](mu::engraving::EngravingItem *item) {
        if (item->is_chord_line()) lines.push_back(mu::engraving::to_chord_line(item));
    });
    return lines;
}

std::string offset_test_write(const mu::engraving::ChordLine *line) {
    muse::ByteArray bytes;
    muse::io::Buffer buffer(&bytes);
    buffer.open(muse::io::IODevice::WRITE_ONLY);
    {
        mu::engraving::XmlWriter xml(&buffer);
        mu::engraving::write::WriteContext ctx(line->score());
        mu::engraving::write::TWrite::write(line, xml, ctx);
    }
    return {reinterpret_cast<const char *>(bytes.const_data()), bytes.size()};
}

std::string legacy_override_write(const mu::engraving::EngravingItem *item, bool standalone) {
    muse::ByteArray bytes;
    muse::io::Buffer buffer(&bytes);
    buffer.open(muse::io::IODevice::WRITE_ONLY);
    {
        mu::engraving::XmlWriter xml(&buffer);
        mu::engraving::write::WriteContext ctx(item->score());
        ctx.set_standalone(standalone);
        mu::engraving::write::TWrite::write_item(item, xml, ctx);
    }
    return {reinterpret_cast<const char *>(bytes.const_data()), bytes.size()};
}

std::string legacy_segment_fixture(std::string_view spanners) {
    return R"(<museScore version="5.00"><Score><Division>480</Division><Style><spatium>1.75</spatium></Style>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><eid>B_B</eid><durationType>half</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><eid>C_C</eid><durationType>half</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
</voice></Measure></Staff><SpannerMap>)" +
           std::string(spanners) + "</SpannerMap></Score></museScore>";
}

std::string split_spanner_fixture(std::string_view overrides) {
    auto xml = legacy_segment_fixture(std::string(R"(<HairPin><eid>F_F</eid><track>0</track><subtype>0</subtype>
<startTick>0/1</startTick><ticks>2/1</ticks><track2>0</track2>)") +
                                      std::string(overrides) + "</HairPin>");
    xml.insert(xml.find("</Staff><SpannerMap>"), R"(<Measure><voice>
<Chord><durationType>half</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
<Chord><durationType>half</durationType><Note><pitch>65</pitch><tpc>13</tpc></Note></Chord>
</voice></Measure>)");
    return xml;
}

void break_first_measure(MasterScore &master) {
    using namespace mu::engraving;
    auto *line_break = Factory::create_layout_break(master.first_measure());
    line_break->set_layout_break_type(LayoutBreakType::LINE);
    master.first_measure()->add(line_break);
    master.set_layout_all();
}

std::string cross_staff_grace_beam_fixture(bool after, int anchor_index) {
    auto xml = legacy_segment_fixture("");
    xml.insert(xml.find("</Part>"), "<Staff><StaffType group=\"pitched\"/></Staff>");
    xml.insert(xml.find("<SpannerMap>"),
               "<Staff id=\"2\"><Measure><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff>");
    std::string grace_xml;
    for (int i = 0; i < 3; ++i) {
        const int index = after ? 2 - i : i;
        if (index == anchor_index) grace_xml += "<Beam><eid>F_F</eid></Beam>";
        grace_xml += "<Chord>";
        grace_xml += after ? "<grace8after/>" : "<acciaccatura/>";
        grace_xml += "<durationType>eighth</durationType>";
        if (index == 0) grace_xml += "<staffMove>1</staffMove>";
        grace_xml += "<Note><pitch>" + std::to_string(65 + i) + "</pitch><tpc>13</tpc></Note></Chord>";
    }
    xml.insert(xml.find("<Chord>"), grace_xml);
    return xml;
}

std::string s18_break_fixture(const std::string &breaks) {
    return R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel/></Instrument></Part>
<Staff id="1"><Measure len="1/4"><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>quarter</durationType></Rest></voice>)" +
           breaks + R"(</Measure><Measure len="1/4"><endRepeat>2</endRepeat><voice>
<Rest><durationType>quarter</durationType></Rest></voice></Measure></Staff></Score></museScore>)";
}

std::string s18_break_xml(const std::string &type) { return "<LayoutBreak><subtype>" + type + "</subtype></LayoutBreak>"; }

void expect_s18_breaks(mu::engraving::Measure *measure, const std::vector<mu::engraving::LayoutBreakType> &expected) {
    using namespace mu::engraving;
    std::vector<LayoutBreakType> actual;
    for (auto *item : measure->el())
        if (item->is_layout_break()) actual.push_back(to_layout_break(item)->layout_break_type());
    EXPECT_EQ(actual, expected);
    const auto contains = [&](auto type) { return std::find(expected.begin(), expected.end(), type) != expected.end(); };
    EXPECT_EQ(measure->line_break(), contains(LayoutBreakType::LINE));
    EXPECT_EQ(measure->page_break(), contains(LayoutBreakType::PAGE));
    EXPECT_EQ(measure->section_break(), contains(LayoutBreakType::SECTION));
    EXPECT_EQ(measure->no_break(), contains(LayoutBreakType::NOBREAK));
}

void render_s20_scores(engrave::Document &document) {
    ASSERT_TRUE(document.render({.format = engrave::RenderFormat::SVG}));
    for (auto *excerpt : master_of(document).excerpts()) {
        const auto id = engrave::from_engine(excerpt->excerpt_score()->eid());
        ASSERT_TRUE(document.render({.score = {id}, .format = engrave::RenderFormat::SVG}));
    }
}

std::string s17_beam_fixture(const std::string &voice) {
    return R"(<museScore version="5.00"><Score><Division>480</Division>
<Part><Staff><StaffType group="pitched"/></Staff><Staff><StaffType group="pitched"/></Staff>
<Instrument id="piano"><Channel/></Instrument></Part><Staff id="1"><Measure><voice>
<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>)" +
           voice + R"(</voice></Measure></Staff>
<Staff id="2"><Measure><voice><Rest><durationType>whole</durationType></Rest></voice></Measure></Staff></Score></museScore>)";
}

// Compare the output across an explicit ownership boundary, not only a reflow
// which can reuse surviving payloads. Source shells remain transitional L3-L6.
void expect_discard_rebuild_parity(engrave::Document &document, mu::engraving::Score &score, const engrave::ScoreSel &selection,
                                   bool require_chord_overrides) {
    using namespace mu::engraving;
    const engrave::RenderOptions render{.score = selection, .format = engrave::RenderFormat::SVG};
    const engrave::ExportOptions save{.format = engrave::Format::MSCX, .score = selection};
    const auto native = document.export_score(save);
    ASSERT_TRUE(native);
    const auto baseline = document.render(render);
    ASSERT_TRUE(baseline);
    const auto positions = [&] {
        std::vector<double> values;
        for (auto kind : {engrave::PositionKind::MEASURE, engrave::PositionKind::SEGMENT}) {
            const auto result = document.positions(kind, selection);
            EXPECT_TRUE(result);
            if (!result) continue;
            values.insert(values.end(), {result->page_width, result->page_height});
            for (const auto &item : result->elements)
                values.insert(values.end(), {double(item.id), item.x, item.y, item.sx, item.sy, double(item.page)});
            for (const auto &event : result->events) values.insert(values.end(), {double(event.elid), double(event.position)});
        }
        return values;
    };
    const auto prepared_positions = positions();
    const auto source = music::dump_json(music::semantic_dump(score.master_score()), 1);
    const auto revision = document.revision();
    auto *store = score.layout_data_store();
    ASSERT_NE(store, nullptr);
    std::vector<std::pair<const EngravingItem *, AuthoredOverrideTable>> overrides;
    const auto *registry = score.master_score()->eid_register();
    for (const auto &[eid, object] : registry->entries()) {
        if (!object->is_engraving_item() || object->score() != &score || !registry->published(object->source_handle())) continue;
        auto *item = to_engraving_item(object);
        if (const auto *table = authored_override_table(item)) overrides.emplace_back(item, *table);
    }
    if (require_chord_overrides) {
        ASSERT_TRUE(
            std::ranges::any_of(overrides, [](const auto &entry) { return entry.first->is_chord() && !entry.second.entries.empty(); }));
    }
    const auto check_source = [&] {
        EXPECT_EQ(document.revision(), revision);
        EXPECT_EQ(music::dump_json(music::semantic_dump(score.master_score()), 1), source);
        for (const auto &[owner, before] : overrides) {
            const auto *after = authored_override_table(owner);
            ASSERT_NE(after, nullptr);
            ASSERT_EQ(before.entries.size(), after->entries.size());
            ASSERT_EQ(before.carriers.size(), after->carriers.size());
            for (size_t i = 0; i < before.entries.size(); ++i) {
                EXPECT_EQ(before.entries[i].child, after->entries[i].child);
                EXPECT_EQ(before.entries[i].pid, after->entries[i].pid);
                EXPECT_EQ(before.entries[i].value, after->entries[i].value);
                EXPECT_EQ(before.entries[i].flags, after->entries[i].flags);
                EXPECT_EQ(before.entries[i].unit, after->entries[i].unit);
            }
            for (size_t i = 0; i < before.carriers.size(); ++i) {
                EXPECT_EQ(before.carriers[i].child, after->carriers[i].child);
                EXPECT_EQ(before.carriers[i].type, after->carriers[i].type);
                EXPECT_EQ(before.carriers[i].legacy_eid, after->carriers[i].legacy_eid);
                EXPECT_EQ(before.carriers[i].track, after->carriers[i].track);
            }
        }
    };
    for (int cycle = 0; cycle < 3; ++cycle) {
        SCOPED_TRACE(cycle);
        ASSERT_GT(store->size(), 0u);
        // Populate the legacy drawing index while it still contains live shells.
        for (auto *page : score.pages()) std::ignore = page->items(page->page_bounding_rect());
        const auto layouts = layout_count(score);
        engrave::DocumentAccess::impl(document).discard_layout_output(score);
        EXPECT_EQ(score.layout_data_store(), store);
        EXPECT_EQ(store->size(), 0u);
        EXPECT_TRUE(score.layout_state.dirty);
        EXPECT_EQ(score.layout_state.start, Fraction(0, 1));
        EXPECT_EQ(score.layout_state.end, Fraction(-1, 1));
        EXPECT_TRUE(score.pages().empty());
        EXPECT_TRUE(score.systems().empty());
        const auto check_segments = [&](auto &&self, const EngravingObject &owner) -> void {
            if (owner.is_segment()) {
                const auto *segment = to_segment(&owner);
                for (const auto &shape : segment->shapes()) EXPECT_TRUE(shape.empty());
                EXPECT_EQ(segment->min_left(), 0.0);
                EXPECT_EQ(segment->min_right(), 0.0);
            }
            for (auto *child : owner.children()) {
                if (child->score() == &score) self(self, *child);
            }
        };
        check_segments(check_segments, score);
        check_source();
        {
            ItemLayoutSpy reads;
            for (auto *item : engrave::document_objects(*score.master_score())) {
                if (!item->is_engraving_item() || item->score() != &score) continue;
                const auto *view = to_engraving_item(item);
                EXPECT_FALSE(view->has_layout_data());
                std::ignore = view->ldata()->bbox();
                std::ignore = view->shape();
            }
            EXPECT_EQ(store->size(), 0u);
            EXPECT_EQ(ItemLayoutSpy::count, 0u);
            EXPECT_EQ(layout_count(score), layouts);
        }
        const auto rebuilt = document.render(render);
        ASSERT_TRUE(rebuilt);
        EXPECT_EQ(rebuilt->pages, baseline->pages);
        EXPECT_EQ(layout_count(score), layouts + 1);
        EXPECT_EQ(positions(), prepared_positions);
        const auto saved = document.export_score(save);
        ASSERT_TRUE(saved);
        EXPECT_EQ(*saved, *native);
        check_source();
    }
}

std::string parenthesis_link_fixture() {
    auto xml = chordline_offset_fixture();
    const auto begin = xml.find("<ChordLine>");
    xml.erase(begin, xml.find("</ChordLine>", begin) + 12 - begin);
    xml.insert(xml.find("<Note>") + 6, "<eid>C_C</eid><parentheses>both</parentheses>");
    xml.insert(xml.find("</Note>") + 7, "<Stem><eid>F_F</eid><userLen>0.5</userLen></Stem><NoteParenGroup><Parenthesis><eid>D_D</"
                                        "eid><offset x=\"1\" y=\"2\"/></Parenthesis>"
                                        "<Parenthesis><horizontalDirection>right</horizontalDirection><eid>E_E</eid></Parenthesis>"
                                        "<Notes><NoteEID>C_C</NoteEID></Notes></NoteParenGroup>"
                                        "<Note><eid>J_J</eid><pitch>64</pitch><tpc>18</tpc></Note>"
                                        "<NoteParenGroup><Parenthesis><eid>G_G</eid></Parenthesis>"
                                        "<Parenthesis><horizontalDirection>right</horizontalDirection><eid>H_H</eid></Parenthesis>"
                                        "<Notes><NoteEID>J_J</NoteEID></Notes></NoteParenGroup>");
    return xml;
}

std::string s11_triplet_fixture(bool stem_offset) {
    std::string xml = R"(<museScore version="5.00"><Score><Division>480</Division>
<Style><spatium>1.75</spatium><stemWidth>0.125</stemWidth></Style>
<Part><Staff><StaffType group="pitched"/></Staff><Instrument id="piano"><Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Tuplet><eid>T_T</eid><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>quarter</baseNote></Tuplet>
<Chord><eid>C_C</eid><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
<endTuplet/><Rest><durationType>half</durationType></Rest></voice></Measure></Staff></Score></museScore>)";
    if (stem_offset) xml.insert(xml.find("<Note>"), R"(<Stem><offset x="0.75" y="-0.5"/></Stem>)");
    return xml;
}

} // namespace read_test
