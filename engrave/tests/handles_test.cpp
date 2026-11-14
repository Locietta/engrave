// Element handles: the inventory (H1), the ids a score has after open and create
// (H2), what survives a save and a replay (H3, H4), linked parts (H5), deletion and
// replacement (H7, H8), hostile ids in the input (H9, H10), stale handles (H11),
// addresses (H12) and determinism (H13).  Every test ends with INV: the register is
// a bijection onto the elements that are part of the score, and every element that
// should have an id has one.
//
//   engrave_handles_test --assets=<resources> [--corpus=<file or dir>]... [options]
//   --deterministic       create the engine with sequential ids (H13's child)
//   --update-inventory    rewrite handle_inventory.json from the corpus
//
#include "engrave/src/api/handles.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/batch.h"
#include "engrave/src/api/state.h"
#include "engrave/src/runtime/score_io.h"
#include "engrave/src/score/export_options.h"
#include "engrave/src/score/json.h"
#include "engrave/src/score/score_excerpts.h"
#include "engraving/dom/accidental.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordbracket.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/image.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/page.h"
#include "engraving/dom/part.h"
#include "engraving/dom/partialtie.h"
#include "engraving/dom/playcounttext.h"
#include "engraving/dom/property.h"
#include "engraving/dom/rangelock.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/sharedpart.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spacer.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/system.h"
#include "engraving/dom/systemdivider.h"
#include "engraving/dom/tapping.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tiejumppointlist.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/addremoveelement.h"
#include "engraving/editing/editkeysig.h"
#include "engraving/editing/editmeasures.h"
#include "engraving/editing/editpagelocks.h"
#include "engraving/editing/edittremolo.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/infrastructure/eidregister.h"
#include "engraving/rendering/score/layoutdatastore.h"
#include "engraving/rw/inoutdata.h"
#include "engraving/rw/rwregister.h"
#include "engraving/rw/write/twrite.h"
#include "engraving/rw/write/writecontext.h"
#include "engraving/rw/xmlwriter.h"
#include "global/io/buffer.h"
#include "global/serialization/zipreader.h"
#include <engrave/engrave.h>

namespace {

using engrave::ElementId;
using engrave::ErrorCode;
using engrave::IdClass;
using mu::engraving::ElementType;
using mu::engraving::EngravingItem;
using mu::engraving::EngravingObject;
using mu::engraving::MasterScore;
using mu::engraving::Measure;
using mu::engraving::MeasureBase;
using mu::engraving::Note;
using mu::engraving::Part;
using mu::engraving::Score;
using mu::engraving::Segment;
using mu::engraving::SegmentType;
using mu::engraving::Staff;

std::filesystem::path g_assets;
std::filesystem::path g_inventory = ENGRAVE_TEST_SOURCE_ROOT "/engrave/tests/handle_inventory.json";
std::filesystem::path g_source_root = ENGRAVE_TEST_SOURCE_ROOT;
std::vector<std::filesystem::path> g_corpus;
std::vector<std::string> g_engine_args; // --assets and --corpus again, for the child
std::string g_program;
bool g_deterministic = false;
bool g_update_inventory = false;
std::unique_ptr<engrave::Engine> g_engine;

// --- text and bytes -------------------------------------------------------

std::string read_text(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::byte> as_bytes(std::string_view text) {
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    return {data, data + text.size()};
}

std::string text_of(const std::vector<std::byte> &bytes) { return {reinterpret_cast<const char *>(bytes.data()), bytes.size()}; }

// --- ids ------------------------------------------------------------------

const char *class_name(IdClass value) {
    switch (value) {
        case IdClass::PERSISTED: return "persisted";
        case IdClass::LAYOUT: return "layout";
        case IdClass::ADDRESS: return "address";
        case IdClass::SESSION: return "session";
    }
    return "unknown";
}

MasterScore &master_of(engrave::Document &document) {
    MasterScore *master = engrave::DocumentAccess::master_score(document);
    EXPECT_NE(master, nullptr);
    return *master;
}

/// Whether an id still names an element that is part of the score.
bool live(MasterScore &master, const ElementId &id) { return engrave::resolve(master, id).has_value(); }

ElementId engine_id(const EngravingObject &object) { return engrave::from_engine(object.eid()); }

std::string id_text(const EngravingObject &object) { return engrave::to_string(engine_id(object)); }

std::string object_name(const EngravingObject &object) { return std::string(object.type_name()); }

/// Where an element is, for the inventory's evidence.
std::string detail(const EngravingObject &object) {
    if (!object.is_engraving_item()) return object_name(object);
    const auto &item = static_cast<const EngravingItem &>(object);
    return std::format("tick={} track={} parent={}", item.tick().ticks(), static_cast<long long>(item.track()),
                       item.parent() ? item.parent()->type_name() : "null");
}

// --- score elements -------------------------------------------------------

Note *first_note(Score &score) {
    Note *found = nullptr;
    score.scan_elements([&found](EngravingItem *item) {
        if (!found && item->is_note()) found = mu::engraving::to_note(item);
    });
    return found;
}

Segment *first_segment(Score &score, SegmentType kind) {
    for (MeasureBase *mb = score.first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) continue;
        if (mu::engraving::to_measure(mb)->is_mm_rest()) continue;
        if (Segment *segment = mu::engraving::to_measure(mb)->first(kind)) return segment;
    }
    return nullptr;
}

mu::engraving::ChordRest *first_chord_rest(Score &score) {
    for (MeasureBase *mb = score.first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) continue;
        if (mu::engraving::to_measure(mb)->is_mm_rest()) continue;
        for (Segment *segment = mu::engraving::to_measure(mb)->first(SegmentType::CHORD_REST); segment; segment = segment->next()) {
            for (mu::engraving::track_idx_t track = 0; track < score.nstaves() * mu::engraving::voices; ++track) {
                EngravingItem *element = segment->element(track);
                if (element && element->is_chord_rest()) return mu::engraving::to_chord_rest(element);
            }
        }
    }
    return nullptr;
}

/// The first element of a type that the walk hands an id out for.
EngravingObject *first_live(MasterScore &master, ElementType type) {
    for (const engrave::LiveId &entry : engrave::live_ids(master)) {
        if (entry.object->type() == type) return entry.object;
    }
    return nullptr;
}

// --- transactions ---------------------------------------------------------

/// One transaction on the score, so that the engine's undo commands may run.
struct Command {
    explicit Command(MasterScore &score) : score_(score) {
        mu::engraving::MScore::set_error(mu::engraving::MsError::MS_NO_ERROR);
        score_.start_cmd(muse::TranslatableString::untranslatable("engrave handles test"));
    }

    ~Command() {
        if (open_) score_.end_cmd(true);
    }

    Command(const Command &) = delete;
    Command &operator=(const Command &) = delete;

    // Publish identities after source finalization.
    void commit() {
        score_.end_cmd(false);
        open_ = false;
        engrave::assign_missing_ids(score_);
    }

private:
    MasterScore &score_;
    bool open_ = true;
};

template <typename Function>
void commit(MasterScore &master, const Function &function) {
    Command command(master);
    function(master);
    command.commit();
}

// --- documents ------------------------------------------------------------

std::unique_ptr<engrave::Document> open_path(const std::filesystem::path &path) {
    auto document = g_engine->open(path);
    EXPECT_TRUE(document) << path.string() << ": " << document.error().message;
    return document ? std::move(*document) : nullptr;
}

std::unique_ptr<engrave::Document> open_text(std::string_view text, engrave::OpenOptions options = {}) {
    auto document = g_engine->open(as_bytes(text), options);
    EXPECT_TRUE(document) << document.error().message;
    return document ? std::move(*document) : nullptr;
}

std::string export_mscz(engrave::Document &document) {
    auto bytes = document.export_score({.format = engrave::Format::MSCZ});
    EXPECT_TRUE(bytes) << bytes.error().message;
    return bytes ? text_of(*bytes) : std::string{};
}

std::map<std::string, std::string> unzip_entries(std::string_view zip_bytes) {
    std::map<std::string, std::string> entries;
    muse::ByteArray bytes(zip_bytes.data(), zip_bytes.size());
    muse::io::Buffer buffer(&bytes);
    if (!buffer.open(muse::io::IODevice::READ_ONLY)) return entries;
    muse::ZipReader zip(&buffer);
    if (zip.has_error()) return entries;
    for (const auto &info : zip.file_info_list()) {
        if (!info.is_file) continue;
        const std::string name = info.file_path.to_std_string();
        const muse::ByteArray data = zip.file_data(name);
        entries[name] = std::string(data.const_char(), data.size());
    }
    return entries;
}

/// The engine's own MSCX of the score (or of one of its excerpts). The version it
/// carries is the current one, so the reader that repairs ids is the one under test.
std::string export_mscx(engrave::Document &document, int excerpt_id = -1) {
    music::ExportOptions options;
    options.excerpt_id = excerpt_id;
    return music::export_native_score(engrave::DocumentAccess::context(document), engrave::DocumentAccess::project(document), "mscx",
                                      options, {});
}

/// The snapshot path: write the score and read it back into a new document.
std::unique_ptr<engrave::Document> reopen(engrave::Document &document) {
    const std::string mscz = export_mscz(document);
    if (mscz.empty()) return nullptr;
    auto copy = g_engine->open(as_bytes(mscz));
    EXPECT_TRUE(copy) << copy.error().message;
    return copy ? std::move(*copy) : nullptr;
}

// --- corpus choice --------------------------------------------------------

/// A corpus file with one of these extensions (any, when none is given).
std::filesystem::path corpus_file(std::initializer_list<std::string_view> extensions) {
    for (const auto &path : g_corpus) {
        if (extensions.size() == 0) return path;
        for (std::string_view extension : extensions) {
            if (path.extension() == extension) return path;
        }
    }
    return {};
}

/// Whether the corpus file is one the engine writes itself, rather than an import that
/// brings its own layout quirks.
bool written_by_engine(const std::filesystem::path &path) {
    const std::string extension = path.extension().string();
    return extension == ".mscx" || extension == ".mscz" || extension == ".musicxml" || extension == ".mxl";
}

// Translate the historical composition input, including its part/instrument metadata.
std::string composition_input() {
    const auto score = music::parse_json<music::Json>(read_text(g_source_root / "tests/data/inputs/golden/created.json"));
    if (!score) throw std::runtime_error("invalid created.json");
    const auto escaped = [](const music::Json &value) {
        return muse::String::from_std_string(value.as<std::string>()).to_xml_escaped().to_std_string();
    };
    const auto &parts = (*score)["parts"].get_array();
    std::string xml = "<score-partwise version=\"4.0\"><work><work-title>" + escaped((*score)["title"]) + "</work-title></work><part-list>";
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto id = "P" + std::to_string(i + 1);
        const auto name = escaped(parts[i]["name"]);
        xml += std::format("<score-part id=\"{0}\"><part-name>{1}</part-name><score-instrument id=\"{0}-I1\">"
                           "<instrument-name>{1}</instrument-name></score-instrument><midi-instrument id=\"{0}-I1\">"
                           "<midi-channel>{2}</midi-channel><midi-program>{3}</midi-program></midi-instrument></score-part>",
                           id, name, i < 9 ? i + 1 : i + 2, parts[i]["midiProgram"].as<int>());
    }
    xml += "</part-list>";
    const std::map<int, std::string_view> types{{120, "16th"}, {240, "eighth"}, {480, "quarter"}, {960, "half"}, {1920, "whole"}};
    constexpr std::string_view steps = "CCDDEFFGGAAB";
    for (size_t i = 0; i < parts.size(); ++i) {
        xml += std::format("<part id=\"P{}\">", i + 1);
        int number = 0;
        for (const auto &measure : parts[i]["measures"].get_array()) {
            xml += std::format("<measure number=\"{}\">", ++number);
            if (number == 1) {
                xml += "<attributes><divisions>480</divisions><time><beats>4</beats><beat-type>4</beat-type></time>"
                       "<clef><sign>G</sign><line>2</line></clef></attributes>";
                if (i == 0)
                    xml += std::format("<direction><direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>{0}"
                                       "</per-minute></metronome></direction-type><sound tempo=\"{0}\"/></direction>",
                                       (*score)["tempo"].as<int>());
            }
            for (const auto &note : measure["notes"].get_array()) {
                const auto &pitches = note["pitches"].get_array();
                const int ticks = note["ticks"].as<int>();
                for (size_t n = 0; n < std::max(size_t{1}, pitches.size()); ++n) {
                    xml += n ? "<note><chord/>" : "<note>";
                    if (pitches.empty())
                        xml += "<rest/>";
                    else {
                        const int pitch = pitches[n].as<int>();
                        xml += std::format("<pitch><step>{}</step>{}<octave>{}</octave></pitch>", steps[pitch % 12],
                                           pitch % 12 == 1 || pitch % 12 == 3 || pitch % 12 == 6 || pitch % 12 == 8 || pitch % 12 == 10 ?
                                               "<alter>1</alter>" :
                                               "",
                                           pitch / 12 - 1);
                    }
                    xml += std::format("<duration>{}</duration><voice>1</voice><type>{}</type></note>", ticks, types.at(ticks));
                }
            }
            xml += "</measure>";
        }
        xml += "</part>";
    }
    return xml + "</score-partwise>";
}

std::vector<Score *> native_scores(MasterScore &master) {
    std::vector<Score *> scores{&master};
    for (const auto *excerpt : master.excerpts()) scores.push_back(excerpt->excerpt_score());
    return scores;
}

void expect_no_layout(MasterScore &master) {
    for (const auto *score : native_scores(master)) {
        EXPECT_EQ(score->layout_state.whole_count, 0u);
        EXPECT_EQ(score->layout_state.range_count, 0u);
        EXPECT_TRUE(score->pages().empty());
    }
}

std::map<std::string, std::string> native_captures(engrave::Document &document, bool legacy = false) {
    std::map<std::string, std::string> captures;
    for (auto *score : native_scores(master_of(document))) {
        for (auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
            for (const auto version : {"5.00", "4.70"}) {
                if (!legacy && version == std::string_view("4.70")) continue;
                engrave::ExportOptions options{.format = format, .native_version = version};
                if (!score->is_master()) options.score.excerpt = engine_id(*score);
                const auto key = std::format("{}/{}/{}", id_text(*score), version, format == engrave::Format::MSCX ? "mscx" : "mscz");
                SCOPED_TRACE(key);
                const auto output = document.export_score(options);
                EXPECT_TRUE(output) << output.error().message;
                if (!output) continue;
                const auto data = text_of(*output);
                if (format == engrave::Format::MSCX) {
                    captures[key] = data;
                } else {
                    auto entries = unzip_entries(data);
                    EXPECT_TRUE(entries.contains("score.mscx"));
                    for (const auto &[name, xml] : entries) {
                        if (name.ends_with(".mscx")) captures[key + "/" + name] = xml;
                    }
                }
            }
        }
    }
    return captures;
}

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
            std::map<ElementId, EngravingObject *> handles;
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
            std::map<Harmony *, String> cold;
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
                    std::set<ElementId> links;
                    for (const auto *linked : edited->object->link_list()) {
                        EXPECT_EQ(linked->score(), &reloaded);
                        const auto resolved = engrave::resolve(reloaded, engine_id(*linked));
                        ASSERT_TRUE(resolved);
                        EXPECT_EQ(resolved->object, linked);
                        links.insert(engine_id(*linked));
                    }
                    EXPECT_EQ(links,
                              (std::set<ElementId>{*engrave::element_id_from_string("4_4"), *engrave::element_id_from_string("NB_NB")}));
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

std::string source_native(Score &score) {
    muse::io::Buffer buffer;
    buffer.open(muse::io::IODevice::WRITE_ONLY);
    mu::engraving::rw::WriteInOutData inout(&score);
    inout.ctx.set_inline_style(true);
    EXPECT_TRUE(mu::engraving::rw::RWRegister::writer()->write_score(&score, &buffer, &inout));
    return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
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
    for (const auto *tag : {"<multiMeasureRest", "<mmRestLast", "<MMRest", "<MMRestRange"}) EXPECT_EQ(cold.find(tag), std::string::npos);
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

TEST(Handles, PageQueriesFollowRangeLayoutAndOutputDiscard) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    ASSERT_EQ(note->pitch(), 71);
    master.ensure_layout();
    const muse::RectF area(0, 0, 10000, 10000);
    ASSERT_FALSE(master.pages().empty());
    EXPECT_TRUE(std::ranges::contains(master.pages().front()->items(area), note));
    const auto ranges = master.layout_state.range_count;
    master.do_layout_range(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(1, 4));
    EXPECT_EQ(master.layout_state.range_count, ranges + 1);
    EXPECT_TRUE(std::ranges::contains(master.pages().front()->items(area), note));
    master.layout_data_store()->clear();
    const auto layouts = master.layout_state.whole_count + master.layout_state.range_count;
    EXPECT_TRUE(master.pages().empty());
    EXPECT_TRUE(master.systems().empty());
    EXPECT_EQ(master.first_measure()->system(), nullptr);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, layouts);
    master.ensure_layout();
    EXPECT_TRUE(std::ranges::contains(master.pages().front()->items(area), note));
}

// --- descriptors ----------------------------------------------------------

/// What an element is beyond its id: its type, where it sits, and a payload that a
/// save and reload has to preserve.
std::string payload(const EngravingObject &object) {
    if (object.type() == ElementType::NOTE) {
        const auto &note = static_cast<const Note &>(object);
        return std::format("pitch={} tpc={}", note.pitch(), note.tpc1());
    }
    if (object.type() == ElementType::MEASURE) {
        return std::format("ticks={}", static_cast<const Measure &>(object).ticks().ticks());
    }
    if (object.is_chord_rest()) {
        const auto &element = static_cast<const mu::engraving::ChordRest &>(object);
        return std::format("ticks={}", element.duration_type().ticks().ticks());
    }
    if (object.is_text_base()) {
        return "text=" + static_cast<const mu::engraving::TextBase &>(object).plain_text().to_std_string();
    }
    return {};
}

std::string describe(const engrave::LiveId &entry) {
    const EngravingObject &object = *entry.object;
    return std::format("{} score={} tick={} track={} {}", object.type_name(), entry.placement.score, entry.placement.tick,
                       entry.placement.track, payload(object));
}

/// Every id of the document and what it names, keyed by the id's text so that a
/// failure prints ids rather than bytes.
std::map<std::string, std::string> element_map(engrave::Document &document) {
    std::map<std::string, std::string> elements;
    for (const engrave::LiveId &entry : engrave::live_ids(master_of(document))) {
        elements.emplace(engrave::to_string(entry.id), describe(entry));
    }
    return elements;
}

/// Compares two element maps, reporting the ids that differ rather than the whole map.
void expect_same_ids(const std::map<std::string, std::string> &before, const std::map<std::string, std::string> &after,
                     std::string_view what) {
    for (const auto &[id, description] : before) {
        const auto found = after.find(id);
        if (found == after.end()) {
            ADD_FAILURE() << what << ": the reload dropped " << id << " (" << description << ")";
            continue;
        }
        EXPECT_EQ(found->second, description) << what << ": " << id << " names something else";
    }
    for (const auto &[id, description] : after) {
        if (!before.contains(id)) ADD_FAILURE() << what << ": the reload added " << id << " (" << description << ")";
    }
}

/// The first corpus score that has an element of this type with an id.
std::unique_ptr<engrave::Document> open_with(ElementType type) {
    for (const auto &path : g_corpus) {
        if (!written_by_engine(path)) continue;
        auto document = g_engine->open(path);
        if (!document) continue;
        if (first_live(*engrave::DocumentAccess::master_score(**document), type)) return std::move(*document);
    }
    return nullptr;
}

// --- INV ------------------------------------------------------------------

void expect_inv(engrave::Document &document) {
    MasterScore &master = master_of(document);
    std::set<const EngravingObject *> objects;
    for (EngravingObject *object : engrave::document_objects(master)) objects.insert(object);

    // The register is a bijection: an entry's element reports that id, and the id of a
    // persisted element names an element of the document.
    std::set<std::string> registered;
    for (const auto &[eid, object] : master.eid_register()->entries()) {
        const std::string text = eid.to_std_string();
        ASSERT_TRUE(eid.is_valid()) << "an invalid id is registered";
        EXPECT_EQ(object->eid(), eid) << text << ": the reverse entry disagrees";
        EXPECT_TRUE(registered.insert(text).second) << text << " is registered twice";
        if (engrave::id_class(*object) != IdClass::PERSISTED) continue; // an id that layout
        // or a read may drop is not handed out, so it is the engine's own entry.
        // Detached items retain private EIDs until destruction.
        if (!master.eid_register()->live_item(master.eid_register()->handle_of(object))) continue;
        EXPECT_TRUE(objects.contains(object) || engrave::in_score(master, object))
            << text << " (" << object->type_name() << ") left the document";
    }

    // Every element that should have an id has one, and the register maps it back to it.
    for (const engrave::LiveId &entry : engrave::live_ids(master)) {
        EXPECT_TRUE(registered.contains(engrave::to_string(entry.id)))
            << object_name(*entry.object) << " " << engrave::to_string(entry.id) << " is not registered";
        EXPECT_EQ(master.eid_register()->item_from_eid(engrave::to_engine(entry.id)), entry.object)
            << engrave::to_string(entry.id) << " names another element";
        EXPECT_EQ(master.eid_register()->live_item(master.eid_register()->handle_of(entry.object)), entry.object)
            << object_name(*entry.object) << " is absent from source membership";
    }
    for (EngravingObject *object : engrave::document_objects(master)) {
        if (!engrave::takes_id(*object)) continue;
        EXPECT_TRUE(object->eid().is_valid()) << object_name(*object) << " has no id";
    }
    EXPECT_LE(registered.size(), master.eid_register()->size());
}

// --- H1 -------------------------------------------------------------------

struct Evidence {
    IdClass declared = IdClass::PERSISTED;
    int assigned = 0;   // instances the walk gives an id
    int kept = 0;       // of those, how many kept that id through a reload
    int others = 0;     // other instances (not generated)
    int other_kept = 0; // of those, how many kept the id the probe gave them
    // The class of the strongest instance seen: a multi-measure rest and everything it
    // holds is layout's, but the measure and barline types are not, so one instance's
    // class does not decide the type's.
    int strength = -1;
};

int class_strength(IdClass value) {
    switch (value) {
        case IdClass::PERSISTED: return 3;
        case IdClass::SESSION: return 2;
        case IdClass::ADDRESS: return 1;
        case IdClass::LAYOUT: return 0;
    }
    return -1;
}

/// Feeds one open document to the inventory: what class each type declares, and whether
/// the ids the walk hands out survive the score's own write and read.
void inventory_evidence(engrave::Document &document, const std::string &source, std::map<std::string, Evidence> &evidence) {
    MasterScore &master = master_of(document);

    // The id of every object, and whether the walk is what gave it: the objects
    // the walk leaves alone get one here, so a save can show what it is worth. A
    // generated element stays out of it: it never gets one and its ids do not name
    // anything a caller can hold.
    struct Probed {
        std::string type;
        bool assigned = false;
        std::string detail;
    };

    std::map<ElementId, Probed> probed;
    for (EngravingObject *object : engrave::document_objects(master)) {
        Evidence &row = evidence[object_name(*object)];
        const IdClass own = engrave::id_class(*object);
        if (class_strength(own) > row.strength) {
            row.declared = own;
            row.strength = class_strength(own);
        }
        if (engrave::takes_id(*object)) {
            ++row.assigned;
            EXPECT_TRUE(object->eid().is_valid()) << object_name(*object) << " has no id";
            probed.emplace(engine_id(*object), Probed{object_name(*object), true, detail(*object)});
        } else if (!object->is_engraving_item() || !static_cast<const EngravingItem &>(*object).generated()) {
            ++row.others;
            if (!object->eid().is_valid()) object->assign_new_eid();
            probed.emplace(engine_id(*object), Probed{object_name(*object), false, detail(*object)});
        }
    }

    auto reloaded = reopen(document);
    if (!reloaded) return;
    std::map<ElementId, std::string> after;
    for (EngravingObject *object : engrave::document_objects(master_of(*reloaded))) {
        if (object->eid().is_valid()) after.emplace(engine_id(*object), object_name(*object));
    }
    for (const auto &[id, entry] : probed) {
        const auto found = after.find(id);
        if (found == after.end() || found->second != entry.type) {
            if (entry.assigned)
                std::fprintf(stderr, "LOST %s %s %s %s\n", source.c_str(), entry.type.c_str(), engrave::to_string(id).c_str(),
                             entry.detail.c_str());
            continue;
        }
        Evidence &row = evidence[entry.type];
        ++(entry.assigned ? row.kept : row.other_kept);
    }
}

/// A score with one instance of every type an operation can create, made through the
/// public edit API on a template. The corpus cannot show a type it has no instance of,
/// so the Add pair table and the other creating operations are measured here too.
std::unique_ptr<engrave::Document> created_types_document(std::set<std::string> &types) {
    auto created = g_engine->create_document({.template_id = "Treble_Clef"});
    if (!created) {
        ADD_FAILURE() << "the Treble_Clef template: " << created.error().message;
        return nullptr;
    }
    std::unique_ptr<engrave::Document> document = std::move(*created);
    engrave::Document &doc = *document;
    const auto outline = doc.outline();
    if (!outline || outline->measures.empty() || outline->parts.empty()) {
        ADD_FAILURE() << "the Treble_Clef template has no measure or no part";
        return nullptr;
    }
    const ElementId first_measure = outline->measures.front().id;
    const ElementId first_part = outline->parts.front().id;
    const auto address = [first_measure](std::int64_t num, std::int64_t den) {
        engrave::SegmentAddress at;
        at.measure = first_measure;
        at.staff = 0;
        at.voice = 0;
        at.tick = {num, den};
        at.kind = "ChordRest";
        return at;
    };
    // What the operations before this one made, for the operations that name an element
    // of their own batch or of an earlier one.
    std::vector<std::optional<ElementId>> chords;
    std::optional<ElementId> note;
    const auto apply = [&doc, &types](std::string_view what, const engrave::Op &op) -> std::optional<engrave::EditResult> {
        std::set<const EngravingObject *> before;
        for (EngravingObject *object : engrave::document_objects(master_of(doc))) before.insert(object);
        auto result = doc.edit(engrave::Batch{{op}});
        if (!result) {
            ADD_FAILURE() << what << ": " << std::string(engrave::to_string(result.error().code)) << ": " << result.error().message;
            return std::nullopt;
        }
        for (EngravingObject *object : engrave::document_objects(master_of(doc))) {
            if (!before.contains(object)) types.insert(object_name(*object));
        }
        return std::move(*result);
    };
    const auto enter = [&](std::string_view what, std::int64_t num, std::int64_t den,
                           std::vector<engrave::Pitch> pitches) -> std::optional<ElementId> {
        const auto result = apply(what, engrave::EnterNotes{address(num, den), {1, 4}, std::move(pitches)});
        if (!result || result->ops.empty() || result->ops[0].created.empty()) return std::nullopt;
        return result->ops[0].created.front();
    };

    const std::optional<ElementId> first = enter("EnterNotes a chord", 0, 1, {{60, std::nullopt}, {64, std::nullopt}});
    const std::optional<ElementId> second = enter("EnterNotes a second chord", 1, 4, {{60, std::nullopt}});
    enter("EnterNotes a rest", 2, 4, {});
    if (!first || !second) return document;
    // The first note of the first chord, for AddTie.
    {
        const auto found = engrave::resolve(master_of(doc), *first);
        if (found && found->object && found->object->is_chord()) {
            const auto &notes = mu::engraving::to_chord(found->object)->notes();
            if (!notes.empty()) note = engrave::from_engine(notes.front()->eid());
        }
    }

    apply("Add an articulation", engrave::Add{*first, "Articulation", {{"symbol", engrave::Enum{"articStaccatoAbove"}}}});
    apply("Add a fermata", engrave::Add{address(0, 1), "Fermata", {{"symbol", engrave::Enum{"fermataAbove"}}}});
    apply("Add a dynamic",
          engrave::Add{address(0, 1), "Dynamic", {{"DYNAMIC_TYPE", engrave::Enum{"ff"}}, {"text", std::string("<sym>dynamicFF</sym>")}}});
    apply("Add a staff text", engrave::Add{address(0, 1), "StaffText", {{"text", std::string("dolce")}}});
    apply("Add a tempo", engrave::Add{address(0, 1), "Tempo", {{"text", std::string("Allegro")}}});
    apply("AddText lyrics", engrave::AddText{address(0, 1), "lyrics_odd", "la"});
    apply("AddText system text", engrave::AddText{address(0, 1), "system", "solo"});
    apply("AddSpanner a slur", engrave::AddSpanner{"Slur", *first, *second, {}});
    apply("AddSpanner a hairpin", engrave::AddSpanner{"HairPin", *first, *second, {}});
    if (note) apply("AddTie", engrave::AddTie{*note});
    apply("AddTuplet", engrave::AddTuplet{*second, *second, 3, 2});
    apply("InsertClef", engrave::InsertClef{.at = address(0, 1), .clef = "F"});
    apply("InsertMeasures a measure", engrave::InsertMeasures{.before = std::nullopt, .count = 1, .kind = "Measure"});
    apply("InsertMeasures a frame", engrave::InsertMeasures{.before = std::nullopt, .count = 1, .kind = "VBox"});
    apply("SetLayoutBreak", engrave::SetLayoutBreak{first_measure, "line", true});
    apply("AddPart", engrave::AddPart{"flute", std::nullopt});
    apply("AddStaff", engrave::AddStaff{first_part});
    apply("GenerateExcerpts", engrave::GenerateExcerpts{{first_part}});
    return document;
}

TEST(Handles, H1Inventory) {
    ASSERT_FALSE(g_corpus.empty());
    std::map<std::string, Evidence> evidence;

    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        inventory_evidence(*document, path.filename().string(), evidence);
    }

    // Not only what the corpus shows: a type an operation can create is measured too,
    // so adding an Add pair or a creating operation fails here until the inventory says
    // what class the new type has.
    std::set<std::string> created;
    if (auto document = created_types_document(created)) {
        SCOPED_TRACE("types created through the edit API");
        inventory_evidence(*document, "created", evidence);
    }
    for (const char *type :
         {"Articulation", "Chord", "Clef", "Dynamic", "Fermata",   "HairPin",    "LayoutBreak", "Lyrics", "Measure", "Note", "Part",
          "Rest",         "Score", "Slur", "Staff",   "StaffText", "SystemText", "Tempo",       "Tie",    "Tuplet",  "VBox"}) {
        EXPECT_TRUE(created.contains(type)) << "the edit API did not create a " << type << "; does it still create one?";
    }
    ASSERT_FALSE(evidence.empty()) << "no corpus score opens";

    std::map<std::string, std::string> observed;
    for (const auto &[type, row] : evidence) {
        if (type == "Segment") {
            observed[type] = class_name(IdClass::ADDRESS);
        } else if (row.assigned > 0) {
            // The walk hands this type out, so every id it gave must survive a save.
            if (row.kept != row.assigned) {
                ADD_FAILURE() << type << ": only " << row.kept << " of " << row.assigned << " ids survived a save";
                continue;
            }
            observed[type] = class_name(IdClass::PERSISTED);
        } else {
            // The walk hands nothing out for this type, so only its declared class is left
            // to pin: an element layout builds, one an address reaches, or one whose id
            // only the file carries.
            observed[type] = class_name(row.declared);
        }
        EXPECT_EQ(class_name(row.declared), observed[type])
            << type << " is declared " << class_name(row.declared) << ", the corpus shows " << observed[type];
    }

    if (g_update_inventory) {
        music::SortedJson types = music::make_object<music::SortedJson>();
        // A row this run did not see is kept: the corpus a regeneration is run with is no
        // reason to forget what the walk was measured to do with a type.
        if (std::filesystem::is_regular_file(g_inventory)) {
            const auto old = music::parse_json<music::SortedJson>(read_text(g_inventory));
            if (old && old->contains("types")) {
                int kept = 0;
                for (const auto &[type, value] : old->at("types").get_object()) {
                    types[type] = value;
                    if (!observed.contains(type)) ++kept;
                }
                if (kept) std::printf("kept %d row(s) this corpus did not show\n", kept);
            }
        }
        for (const auto &[type, value] : observed) types[type] = value;
        music::SortedJson document = {
            {"note", "The id class of every element type the corpus shows or an operation can create. persisted: the "
                     "writer stores its id and a reload restores it; layout: created by layout, so the next pass hands "
                     "out a new id; address: reached through an Address instead; session: alive in memory but not "
                     "written. This test fails when a type is missing here or its class changed."},
            {"types", types},
        };
        std::ofstream out(g_inventory, std::ios::binary);
        out << music::dump_json(document, 2) << '\n';
        std::printf("wrote %zu types to %s\n", observed.size(), g_inventory.string().c_str());
        return;
    }

    ASSERT_TRUE(std::filesystem::is_regular_file(g_inventory)) << "no " << g_inventory.string() << "; run --update-inventory";
    const auto inventory = music::parse_json<music::SortedJson>(read_text(g_inventory));
    ASSERT_TRUE(inventory) << g_inventory.string() << " is not JSON";
    const auto &declared = inventory->at("types");
    for (const auto &[type, value] : observed) {
        ASSERT_TRUE(declared.contains(type)) << type << " is unclassified (" << value << "); add it to " << g_inventory.string();
        EXPECT_EQ(declared.at(type).get<std::string>(), value) << "the class of " << type << " changed";
    }
    for (const auto &[type, value] : declared.get_object()) {
        if (!observed.contains(type)) std::printf("note: %s is no longer in the corpus\n", type.c_str());
    }
}

// --- H2 -------------------------------------------------------------------

void expect_first_ids(engrave::Document &document) {
    MasterScore &master = master_of(document);
    std::map<std::string, std::string> by_text;
    int exposed = 0;
    for (EngravingObject *object : engrave::document_objects(master)) {
        if (!engrave::takes_id(*object)) continue;
        const mu::engraving::EID eid = object->eid();
        ASSERT_TRUE(eid.is_valid()) << object_name(*object) << " has no id";
        // The public text form is the engine's, byte for byte.
        EXPECT_EQ(engrave::to_string(engrave::from_engine(eid)), eid.to_std_string());
        const auto [entry, inserted] = by_text.emplace(eid.to_std_string(), object_name(*object));
        EXPECT_TRUE(inserted) << "id " << entry->first << " names both " << entry->second << " and " << object_name(*object);
        ++exposed;
    }
    EXPECT_GT(exposed, 0) << "the score exposes no element at all";
    expect_inv(document);
}

TEST(Handles, H2IdsBeforeTheFirstSave) {
    ASSERT_FALSE(g_corpus.empty());
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        expect_first_ids(*document);
    }

    const auto templates = g_engine->templates();
    ASSERT_GE(templates.size(), 40u);
    for (const auto &info : templates) {
        auto document = g_engine->create_document({.template_path = info.path});
        ASSERT_TRUE(document) << info.id << ": " << document.error().message;
        SCOPED_TRACE("template " + info.id);
        expect_first_ids(**document);
    }
}

// --- H3 -------------------------------------------------------------------

TEST(Handles, H3IdsSurviveSaveAndRestore) {
    ASSERT_FALSE(g_corpus.empty());
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        const auto before = element_map(*document);
        EXPECT_FALSE(before.empty());

        auto reloaded = reopen(*document);
        if (!reloaded) continue;
        expect_same_ids(before, element_map(*reloaded), "a save and reload");

        // A snapshot restore is the same write and read again.
        auto restored = reopen(*reloaded);
        if (!restored) continue;
        expect_same_ids(before, element_map(*restored), "the second round trip");
        expect_inv(*reloaded);
        expect_inv(*restored);
    }
}

// --- H4 -------------------------------------------------------------------

TEST(Handles, H4ExportLeavesIdsAndRevisionAlone) {
    ASSERT_FALSE(g_corpus.empty());
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        const auto before = element_map(*document);
        const std::string first = export_mscz(*document);
        const std::string second = export_mscz(*document);
        EXPECT_FALSE(first.empty());
        EXPECT_EQ(unzip_entries(first), unzip_entries(second)) << "two exports of one score differ";
        EXPECT_EQ(element_map(*document), before) << "an export changed an id";
        EXPECT_EQ(document->revision(), 1u);
        expect_inv(*document);
    }
}

// --- H5 -------------------------------------------------------------------

TEST(Handles, H5aLinksAcrossExcerpts) {
    // Excerpt generation links a part's elements as clones in the excerpt score.  It needs
    // notes to link, which a template-built score has none of, so the corpus is the source.
    int checked = 0;
    for (const auto &path : g_corpus) {
        if (path.extension() != ".mscx") continue;
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        MasterScore &master = master_of(*document);
        if (master.excerpts().empty()) music::generate_score_excerpts(&master);
        if (master.excerpts().empty()) continue;
        engrave::assign_missing_ids(master);

        // A note of the master and its clone in an excerpt: any note whose link group holds
        // a note of another score.
        Note *master_note = nullptr;
        Note *excerpt_note = nullptr;
        for (const engrave::LiveId &entry : engrave::live_ids(master)) {
            if (master_note || entry.object->type() != ElementType::NOTE) continue;
            if (entry.object->score() != &master) continue; // the master's own note, not a clone
            for (EngravingObject *linked : entry.object->link_list()) {
                if (!linked->is_note() || linked->score() == &master) continue;
                master_note = mu::engraving::to_note(entry.object);
                excerpt_note = mu::engraving::to_note(linked);
                break;
            }
        }
        if (!master_note || !excerpt_note) continue;
        ++checked;

        const ElementId master_id = engine_id(*master_note);
        const ElementId excerpt_id = engine_id(*excerpt_note);
        EXPECT_NE(master_note->eid(), excerpt_note->eid()) << "the excerpt clone shares the master's id";
        EXPECT_TRUE(live(master, master_id));
        EXPECT_TRUE(live(master, excerpt_id));

        // A property set on the master propagates to the excerpt clone.
        const muse::draw::Color red(255, 0, 0);
        commit(master, [&](MasterScore &) {
            master_note->undo_change_property(mu::engraving::Pid::COLOR, red, mu::engraving::PropertyFlags::NOSTYLE);
        });
        EXPECT_EQ(excerpt_note->color(), red) << "the property did not reach the linked element";
        EXPECT_TRUE(live(master, master_id)) << "a property edit lost the element's id";

        ASSERT_TRUE(document->edit({{engrave::Remove{master_id}}}));
        EXPECT_FALSE(live(master, master_id)) << "the removed element still resolves";
        EXPECT_FALSE(live(master, excerpt_id)) << "the linked clone survived the removal";
        expect_inv(*document);
        break; // one linked score is what this test needs; the first with notes is enough
    }
    EXPECT_GT(checked, 0) << "no corpus score has a linked note";
}

TEST(Handles, H5bRemovingAPartLeavesNoId) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);
    // A save gives the parts and their staves their ids.
    EXPECT_FALSE(export_mscz(*document).empty());
    expect_inv(*document);

    std::set<std::string> seen;
    for (const engrave::LiveId &entry : engrave::live_ids(master)) seen.insert(engrave::to_string(entry.id));

    const mu::engraving::InstrumentIndex index = mu::engraving::search_template_index_for_id(muse::String(u"piano"));
    ASSERT_NE(index.instr_template, nullptr);
    for (int round = 0; round < 3; ++round) {
        master.append_part(index.instr_template);
        ASSERT_GT(master.parts().size(), 1u);
        Part *part = master.parts().back();
        engrave::assign_missing_ids(master);
        const ElementId part_id = engine_id(*part);
        EXPECT_TRUE(seen.insert(engrave::to_string(part_id)).second) << "a new part inherited an id";
        for (Staff *staff : part->staves()) {
            if (!staff->eid().is_valid()) continue;
            EXPECT_TRUE(seen.insert(id_text(*staff)).second) << "a new staff inherited an id";
        }
        expect_inv(*document);

        master.cmd_remove_part(part);
        engrave::assign_missing_ids(master);
        expect_inv(*document);
        EXPECT_FALSE(live(master, part_id)) << "the removed part still resolves";
    }

    // An excerpt is a score, not an element, and leaves no entry either.
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_FALSE(master.excerpts().empty());
    engrave::assign_missing_ids(master);
    mu::engraving::Excerpt *excerpt = master.excerpts().front();
    const ElementId excerpt_id = engine_id(*excerpt->excerpt_score());
    expect_inv(*document);
    master.remove_excerpt(excerpt);
    delete excerpt;
    engrave::assign_missing_ids(master);
    expect_inv(*document);
    EXPECT_FALSE(live(master, excerpt_id)) << "the removed excerpt still resolves";

    // The same round through the edit API: a part and its staves get fresh ids and
    // leave none behind.
    for (int round = 0; round < 3; ++round) {
        auto added = document->edit(engrave::Batch{{engrave::AddPart{"piano", std::nullopt}}});
        ASSERT_TRUE(added) << added.error().message;
        ASSERT_FALSE(added->ops.empty());
        ASSERT_FALSE(added->ops.front().created.empty());
        const ElementId part_id = *added->ops.front().created.front();
        for (const auto &id : added->ops.front().created) {
            ASSERT_TRUE(id) << "a created part or staff has no id";
            EXPECT_TRUE(seen.insert(engrave::to_string(*id)).second) << "a new part or staff inherited an id";
        }
        expect_inv(*document);

        auto removed = document->edit(engrave::Batch{{engrave::RemovePart{part_id}}});
        ASSERT_TRUE(removed) << removed.error().message;
        expect_inv(*document);
        EXPECT_FALSE(live(master, part_id)) << "the removed part still resolves";
    }
}

// --- H7 -------------------------------------------------------------------

// --- H8 -------------------------------------------------------------------

TEST(Handles, H8ReplacedElementsLoseTheirId) {
    auto document = open_with(ElementType::CLEF);
    ASSERT_TRUE(document) << "no corpus score has a clef with an id";
    MasterScore &master = master_of(*document);

    // A ChangeElement swaps the element for a new one: the old id goes, and a new one
    // takes its place at the same position.
    EngravingItem *clef = mu::engraving::to_engraving_item(first_live(master, ElementType::CLEF));
    ASSERT_NE(clef, nullptr);
    Measure *measure = master.tick2measure(clef->tick());
    ASSERT_NE(measure, nullptr);
    Segment *clef_segment = nullptr;
    for (Segment *segment = measure->first(); segment && !clef_segment; segment = segment->next()) {
        if (segment->element(clef->track()) == clef) clef_segment = segment;
    }
    ASSERT_NE(clef_segment, nullptr) << "no segment holds the clef";
    const ElementId clef_id = engine_id(*clef);
    const auto before = engrave::live_ids(master);
    commit(master, [&](MasterScore &) {
        auto *replacement = mu::engraving::Factory::create_clef(clef_segment);
        replacement->set_clef_type(mu::engraving::ClefType::F);
        replacement->set_track(clef->track());
        master.apply_element_replace(clef, replacement);
    });
    const engrave::IdDiff diff = engrave::diff_ids(before, engrave::live_ids(master));
    EXPECT_FALSE(diff.replaced.empty()) << "the replaced clef kept its id";
    EXPECT_FALSE(diff.created.empty());
    for (const ElementId &id : diff.replaced) {
        EXPECT_FALSE(live(master, id)) << "a replaced id still resolves";
    }
    EXPECT_FALSE(live(master, clef_id)) << "the replaced clef still resolves";
    expect_inv(*document);

    // A duration change keeps a chord whose new length fits where it is (the length is
    // a property then) and rebuilds it otherwise; either way the diff and the resolver
    // agree on whether the old id still names the chord.
    mu::engraving::ChordRest *chord_rest = mu::engraving::to_chord_rest(first_live(master, ElementType::CHORD));
    ASSERT_NE(chord_rest, nullptr) << "the walk hands out no chord";
    const ElementId chord_id = engine_id(*chord_rest);
    const auto before_change = engrave::live_ids(master);
    commit(master, [&](MasterScore &) { master.change_c_rlen(chord_rest, mu::engraving::Fraction(1, 2)); });
    const engrave::IdDiff change = engrave::diff_ids(before_change, engrave::live_ids(master));
    const bool dropped = std::ranges::contains(change.replaced, chord_id) || std::ranges::contains(change.removed, chord_id);
    EXPECT_NE(live(master, chord_id), dropped) << "the diff and the resolver disagree about the chord";
    expect_inv(*document);
}

// --- crafted input --------------------------------------------------------

/// A current-version MSCX of an imported corpus score, plus one of its excerpts:
/// the files the 5.00 reader reads, which is the reader that repairs ids.
struct CraftedInput {
    std::string master;
    std::string excerpt;
};

CraftedInput crafted_input() {
    CraftedInput input;
    for (const auto &path : g_corpus) {
        const std::string extension = path.extension().string();
        if (extension != ".musicxml" && extension != ".mxl" && extension != ".mid") continue;
        auto document = open_path(path);
        if (!document) continue;
        MasterScore &master = master_of(*document);
        if (master.excerpts().empty()) music::generate_score_excerpts(&master);
        engrave::assign_missing_ids(master);
        std::string written = export_mscx(*document);
        if (written.find("version=\"5.00\"") == std::string::npos) continue;
        input.master = std::move(written);
        if (!master.excerpts().empty()) input.excerpt = export_mscx(*document, 0);
        return input;
    }
    return input;
}

/// The spans of the text inside every <eid> element.
std::vector<std::pair<std::size_t, std::size_t>> eid_spans(const std::string &text) {
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    const std::string open = "<eid>";
    const std::string close = "</eid>";
    for (std::size_t at = text.find(open); at != std::string::npos; at = text.find(open, at)) {
        const std::size_t start = at + open.size();
        const std::size_t end = text.find(close, start);
        if (end == std::string::npos) break;
        spans.emplace_back(start, end);
        at = end;
    }
    return spans;
}

/// The text with the n-th id replaced.
std::string with_eid(std::string text, std::size_t index, std::string_view value) {
    const auto spans = eid_spans(text);
    if (index >= spans.size()) return text;
    text.replace(spans[index].first, spans[index].second - spans[index].first, value);
    return text;
}

/// The text with every id `from` renamed to `to`, which gives two elements one id.
std::string with_copied_id(std::string text, std::string_view from, std::string_view to) {
    const std::string needle = ">" + std::string(from) + "<";
    const std::string replacement = ">" + std::string(to) + "<";
    for (std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + replacement.size())) {
        text.replace(at, needle.size(), replacement);
    }
    return text;
}

/// A corpus MSCX file that has ids of its own and, if asked, a tag.
std::filesystem::path corpus_mscx(std::string_view tag) {
    for (const auto &path : g_corpus) {
        if (path.extension() != ".mscx") continue;
        const std::string text = read_text(path);
        if (text.find("<eid>") == std::string::npos) continue;
        if (!tag.empty() && text.find(tag) == std::string::npos) continue;
        return path;
    }
    return {};
}

/// The id texts of a corpus file, with the type of the element each names.
std::map<std::string, std::string> id_types(const std::string &text) {
    std::map<std::string, std::string> types;
    auto document = open_text(text);
    if (!document) return types;
    for (const engrave::LiveId &entry : engrave::live_ids(master_of(*document))) {
        types.emplace(engrave::to_string(entry.id), object_name(*entry.object));
    }
    return types;
}

bool warned_about(engrave::Document &document, std::string_view text) {
    for (const auto &diagnostic : document.diagnostics()) {
        if (diagnostic.severity == engrave::Severity::WARNING && diagnostic.message.find(text) != std::string::npos) return true;
    }
    return false;
}

// --- H9 -------------------------------------------------------------------

/// Whether the crafted text gives the reader a duplicate to repair.
bool reader_sees_duplicate(const std::string &text) {
    auto document = open_text(text);
    if (!document) return false;
    return master_of(*document).eid_register()->repairs().duplicate > 0;
}

void expect_repaired(const std::string &text) {
    auto document = open_text(text);
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);
    // If the reader keeps both elements it repairs the duplicate; if it rebuilds one of
    // them, there is nothing left to repair and the register is consistent either way.
    const bool repaired = master.eid_register()->repairs().any();
    if (repaired) EXPECT_TRUE(warned_about(*document, "shared an id"));
    EXPECT_FALSE(engrave::live_ids(master).empty());
    expect_inv(*document);

    // The repaired score is usable: it saves and comes back with the same ids.
    const auto before = element_map(*document);
    auto reloaded = reopen(*document);
    if (reloaded) {
        EXPECT_EQ(element_map(*reloaded), before);
        expect_inv(*reloaded);
    }

    // A caller that cannot accept the repair asks for strict ids instead.
    auto strict = g_engine->open(as_bytes(text), {.strict_ids = true});
    if (repaired) {
        ASSERT_FALSE(strict);
        EXPECT_EQ(strict.error().code, ErrorCode::INVALID_INPUT);
    } else {
        EXPECT_TRUE(strict) << "a dropped duplicate does not need strict ids to reject it";
    }
}

TEST(Handles, H9DuplicateIdsInTheInput) {
    const CraftedInput input = crafted_input();
    if (input.master.empty()) GTEST_SKIP() << "no imported corpus score to write a current MSCX from";
    const auto types = id_types(input.master);
    ASSERT_GE(types.size(), 4u);

    // (a) two elements of the same type claim one id, (b) two types share one id.  Only
    // ids the file itself carries can be duplicated.
    const auto spans = eid_spans(input.master);
    std::set<std::string> in_file;
    for (const auto &[start, end] : spans) in_file.insert(input.master.substr(start, end - start));
    std::map<std::string, std::vector<std::string>> by_type;
    for (const auto &[id, type] : types) {
        if (in_file.contains(id)) by_type[type].push_back(id);
    }
    std::string same_a;
    std::string same_b;
    std::string other;
    std::string same_type;
    for (const auto &[type, ids] : by_type) {
        if (!same_a.empty() || ids.size() < 2) continue;
        same_type = type;
        same_a = ids[0];
        same_b = ids[1];
    }
    for (const auto &[type, ids] : by_type) {
        if (ids.empty() || type == same_type) continue;
        other = ids.front();
        break;
    }
    ASSERT_FALSE(same_b.empty()) << "no two elements share a type";
    ASSERT_FALSE(other.empty()) << "the score has only one element type";

    // (a) two elements of the same type claim one id, (b) two types share one id.  Only
    // ids the file itself carries can be duplicated, and only where the reader keeps both
    // elements: it replaces some of what it read (a measure it rebuilt, for one).
    std::string duplicate;
    for (const auto &[type, ids] : by_type) {
        if (ids.size() < 2) continue;
        std::string candidate = with_copied_id(input.master, ids[1], ids[0]);
        if (candidate == input.master || !reader_sees_duplicate(candidate)) continue;
        duplicate = std::move(candidate);
        break;
    }
    ASSERT_FALSE(duplicate.empty()) << "no two elements of one type that the reader keeps share an id";
    expect_repaired(duplicate);

    bool crossed = false;
    for (const auto &[from_type, from_ids] : by_type) {
        for (const auto &[to_type, to_ids] : by_type) {
            if (from_type == to_type || from_ids.empty() || to_ids.empty()) continue;
            std::string candidate = with_copied_id(input.master, from_ids.front(), to_ids.front());
            if (candidate == input.master || !reader_sees_duplicate(candidate)) continue;
            expect_repaired(candidate);
            crossed = true;
            break;
        }
        if (crossed) break;
    }
    EXPECT_TRUE(crossed) << "two types cannot be given one id";

    // (c) an element a <linkedTo> names appears twice: the link still has one target.
    if (!input.excerpt.empty()) {
        const std::size_t at = input.excerpt.find("<linkedTo>");
        if (at != std::string::npos) {
            const std::size_t start = at + std::string("<linkedTo>").size();
            const std::string target = input.excerpt.substr(start, input.excerpt.find("</linkedTo>", start) - start);
            const auto excerpt_types = id_types(input.excerpt);
            const auto candidate = std::ranges::find_if(excerpt_types, [&target](const auto &entry) { return entry.first != target; });
            ASSERT_TRUE(candidate != excerpt_types.end());
            expect_repaired(with_copied_id(input.excerpt, candidate->first, target));
        }
    }
}

// --- H10 ------------------------------------------------------------------

TEST(Handles, H10MalformedIdsInTheInput) {
    const CraftedInput input = crafted_input();
    if (input.master.empty()) GTEST_SKIP() << "no imported corpus score to write a current MSCX from";
    const std::string &source = input.master;
    const auto spans = eid_spans(source);
    ASSERT_GE(spans.size(), 8u);
    const std::size_t victim = 5;

    const std::vector<std::string> malformed = {
        "",                          // empty
        "A",                         // no separator
        "A_B_C",                     // three halves
        "!_A",                       // a character outside the alphabet
        "AAAAAAAAAAAA_AAAAAAAAAAAA", // halves longer than 11 characters
        "_",                         // two empty halves
    };
    for (const std::string &value : malformed) {
        SCOPED_TRACE("eid '" + value + "'");
        const std::string text = with_eid(source, victim, value);
        ASSERT_NE(text, source);
        auto document = open_text(text);
        ASSERT_TRUE(document);
        MasterScore &master = master_of(*document);
        EXPECT_EQ(master.eid_register()->repairs().malformed, 1u);
        EXPECT_TRUE(warned_about(*document, "malformed"));
        EXPECT_FALSE(engrave::live_ids(master).empty());
        expect_inv(*document);
        // The element got a fresh id, so a caller can still name it.
        EXPECT_TRUE(live(master, engrave::live_ids(master).front().id));

        auto strict = g_engine->open(as_bytes(text), {.strict_ids = true});
        ASSERT_FALSE(strict) << "strict ids accepted '" << value << "'";
        EXPECT_EQ(strict.error().code, ErrorCode::INVALID_INPUT);
    }

    // Two texts that differ but decode to one id: a half keeps its low 64 bits, so
    // the top two bits of an 11-character half do not count.
    const std::string colliding = with_eid(with_eid(source, victim, "AAAAAAAAAAP_AAAAAAAAAAP"), victim + 1, "AAAAAAAAAAf_AAAAAAAAAAf");
    auto document = open_text(colliding);
    ASSERT_TRUE(document);
    EXPECT_EQ(master_of(*document).eid_register()->repairs().duplicate, 1u);
    expect_inv(*document);
}

// --- H11 ------------------------------------------------------------------

TEST(Handles, H11StaleHandles) {
    ASSERT_GE(g_corpus.size(), 2u);
    const auto path = corpus_file({".mscx"});
    ASSERT_FALSE(path.empty());
    ElementId id;
    {
        auto document = open_path(path);
        ASSERT_TRUE(document);
        Note *note = first_note(master_of(*document));
        ASSERT_NE(note, nullptr);
        id = engine_id(*note);
        EXPECT_TRUE(live(master_of(*document), id));
        expect_inv(*document);
    }

    // Ids are document-scoped: the same file opened twice hands out the same ids, so
    // an id of the closed document is a documented false match in the new one.
    auto same = open_path(path);
    ASSERT_TRUE(same);
    MasterScore &master = master_of(*same);
    const auto same_ids = engrave::live_ids(master);
    const bool present = std::ranges::any_of(same_ids, [&id](const engrave::LiveId &entry) { return entry.id == id; });
    EXPECT_EQ(present, live(master, id)) << "the same file gave a different id set, or the id resolves differently";

    // An id of another document is not found.
    auto other = open_path(corpus_file({".musicxml"}));
    ASSERT_TRUE(other);
    MasterScore &other_master = master_of(*other);
    std::set<std::string> other_texts;
    for (const engrave::LiveId &entry : engrave::live_ids(other_master)) other_texts.insert(engrave::to_string(entry.id));
    int checked = 0;
    for (const engrave::LiveId &entry : same_ids) {
        if (other_texts.contains(engrave::to_string(entry.id))) continue;
        const auto resolved = engrave::resolve(other_master, entry.id);
        ASSERT_FALSE(resolved) << "another document's id resolves";
        EXPECT_EQ(resolved.error().code, ErrorCode::NOT_FOUND);
        ++checked;
        break;
    }
    EXPECT_EQ(checked, 1);

    // An id the snapshot path preserved still works.
    auto restored = reopen(*same);
    ASSERT_TRUE(restored);
    EXPECT_TRUE(live(master_of(*restored), id));

    // Generated and layout elements never carry an id, so no caller can hold one.
    for (EngravingObject *object : engrave::document_objects(master)) {
        if (engrave::id_class(*object) == IdClass::PERSISTED) continue;
        if (engrave::id_class(*object) == IdClass::SESSION) continue; // the file stores one
        EXPECT_FALSE(object->eid().is_valid()) << object_name(*object) << " has an id";
    }
    for (EngravingObject *object : engrave::document_objects(master)) {
        if (!object->is_engraving_item() || !static_cast<EngravingItem &>(*object).generated()) continue;
        EXPECT_FALSE(engrave::takes_id(*object)) << object_name(*object) << " is handed out";
    }
    expect_inv(*same);
}

// --- H12 ------------------------------------------------------------------

TEST(Handles, H12Addresses) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);

    Segment *segment = first_segment(master, SegmentType::CHORD_REST);
    ASSERT_NE(segment, nullptr);
    const auto address = engrave::address_of(*segment);
    ASSERT_TRUE(address) << "a chord-rest segment has no address";
    const auto first = engrave::resolve(master, *address);
    ASSERT_TRUE(first) << first.error().message;
    EXPECT_EQ(first->object, segment);
    EXPECT_TRUE(first->writable);

    // Unrelated edits leave the address alone.
    Note *note = first_note(master);
    ASSERT_NE(note, nullptr);
    commit(master, [&](MasterScore &) {
        note->undo_change_property(mu::engraving::Pid::COLOR, muse::draw::Color(0, 0, 255), mu::engraving::PropertyFlags::NOSTYLE);
    });
    const auto second = engrave::resolve(master, *address);
    ASSERT_TRUE(second) << second.error().message;
    EXPECT_EQ(second->object, segment);

    // The address fails once its measure is gone.
    Measure *measure = segment->measure()->next_measure();
    if (!measure) measure = segment->measure();
    Segment *doomed = measure->first(SegmentType::CHORD_REST);
    ASSERT_NE(doomed, nullptr);
    const auto doomed_address = engrave::address_of(*doomed);
    ASSERT_TRUE(doomed_address);
    commit(master, [&](MasterScore &) { master.delete_measures(measure, measure); });
    const auto gone = engrave::resolve(master, *doomed_address);
    EXPECT_FALSE(gone) << "an address of a removed measure still resolves";
    expect_inv(*document);

    // Layout addresses are read-only and name the layout object. Edits no longer
    // lay out, so this direct resolve requests geometry the way get() does.
    master.ensure_layout();
    const auto page = engrave::resolve(master, engrave::PageAddress{.score = {}, .index = 0});
    ASSERT_TRUE(page) << page.error().message;
    EXPECT_FALSE(page->writable);
    ASSERT_EQ(page->object->type(), ElementType::PAGE);
    const auto page_address = engrave::address_of(*page->object);
    ASSERT_TRUE(page_address);
    const auto page_again = engrave::resolve(master, *page_address);
    ASSERT_TRUE(page_again);
    EXPECT_EQ(page_again->object, page->object);

    const auto system = engrave::resolve(master, engrave::SystemAddress{.score = {}, .page = 0, .index = 0});
    ASSERT_TRUE(system) << system.error().message;
    EXPECT_FALSE(system->writable);
    ASSERT_EQ(system->object->type(), ElementType::SYSTEM);
    const auto system_address = engrave::address_of(*system->object);
    ASSERT_TRUE(system_address);
    const auto system_again = engrave::resolve(master, *system_address);
    ASSERT_TRUE(system_again);
    EXPECT_EQ(system_again->object, system->object);

    // A missing position is a stale handle, an unknown kind a caller error.
    const auto missing = engrave::resolve(master, engrave::PageAddress{.score = {}, .index = 9999});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::STALE_HANDLE);
    auto bad_kind = std::get<engrave::SegmentAddress>(*address);
    bad_kind.kind = "NoSuchSegment";
    const auto bad = engrave::resolve(master, engrave::Address{bad_kind});
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code, ErrorCode::INVALID_ARGUMENT);
}

TEST(Handles, SegmentTicksValidateBeforeResolvingOrEditing) {
    const auto fixture = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(fixture, g_corpus.end());
    auto document = open_path(*fixture);
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);
    Segment *segment = first_segment(master, SegmentType::CHORD_REST);
    ASSERT_NE(segment, nullptr);
    const auto resolved_address = engrave::address_of(*segment);
    ASSERT_TRUE(resolved_address);
    auto address = std::get<engrave::SegmentAddress>(*resolved_address);
    ASSERT_EQ(address.tick.num, 0);
    const std::int64_t low = std::numeric_limits<int>::min();
    const std::int64_t high = std::numeric_limits<int>::max();

    // The entire component range is accepted, including unreduced zero.
    for (const auto tick : {engrave::Fraction{0, 1}, {0, high - 1}, {0, high}}) {
        address.tick = tick;
        const auto found = engrave::resolve(master, engrave::Address{address});
        ASSERT_TRUE(found) << tick.num << '/' << tick.den << ": " << found.error().message;
        EXPECT_EQ(found->object, segment);
        EXPECT_TRUE(document->get(engrave::Ref{engrave::Address{address}}));
    }
    for (const auto tick : {engrave::Fraction{low, 1}, {low + 1, 1}, {high - 1, 1}, {high, 1}}) {
        address.tick = tick;
        const auto found = document->get(engrave::Ref{engrave::Address{address}});
        ASSERT_FALSE(found);
        EXPECT_EQ(found.error().code, ErrorCode::STALE_HANDLE);
    }
    // Equivalent large fractions must still resolve a nonzero position in a
    // later measure, without overflowing the measure-relative lookup.
    Measure *later_measure = segment->measure()->next_measure();
    ASSERT_NE(later_measure, nullptr);
    Segment *later_segment = later_measure->first(SegmentType::CHORD_REST);
    ASSERT_NE(later_segment, nullptr);
    const auto later_address = engrave::address_of(*later_segment);
    ASSERT_TRUE(later_address);
    auto equivalent = std::get<engrave::SegmentAddress>(*later_address);
    ASSERT_GT(equivalent.tick.num, 0);
    const std::int64_t scale = std::min(high / equivalent.tick.num, high / equivalent.tick.den);
    ASSERT_GT(scale, 1);
    equivalent.tick.num *= scale;
    equivalent.tick.den *= scale;
    const auto same_position = engrave::resolve(master, engrave::Address{equivalent});
    ASSERT_TRUE(same_position) << same_position.error().message;
    EXPECT_EQ(same_position->object, later_segment);

    // Adding two int fractions can leave 64-bit engine terms. The exact
    // absolute position is 2/1, not the wrapped numerator -4.
    const auto old_measure_tick = later_measure->tick();
    const auto old_relative_tick = later_segment->rtick();
    later_measure->set_tick(mu::engraving::Fraction(static_cast<int>(high), static_cast<int>(high - 1)));
    later_segment->set_rtick(mu::engraving::Fraction(static_cast<int>(high - 2), static_cast<int>(high - 1)));
    const auto wide_address = engrave::address_of(*later_segment);
    equivalent.tick = {2, 1};
    const auto wide_position = engrave::resolve(master, engrave::Address{equivalent});
    equivalent.tick = {-4, high - 1};
    const auto wrapped_position = engrave::resolve(master, engrave::Address{equivalent});
    later_measure->set_tick(mu::engraving::Fraction(1, static_cast<int>(high - 1)));
    later_segment->set_rtick(mu::engraving::Fraction(1, static_cast<int>(high)));
    const auto unrepresentable_address = engrave::address_of(*later_segment);
    later_measure->set_tick(old_measure_tick);
    later_segment->set_rtick(old_relative_tick);
    ASSERT_TRUE(wide_address);
    const auto written_tick = std::get<engrave::SegmentAddress>(*wide_address).tick;
    EXPECT_EQ(written_tick.num, 2);
    EXPECT_EQ(written_tick.den, 1);
    ASSERT_TRUE(wide_position) << wide_position.error().message;
    EXPECT_EQ(wide_position->object, later_segment);
    ASSERT_FALSE(wrapped_position);
    EXPECT_EQ(wrapped_position.error().code, ErrorCode::STALE_HANDLE);
    EXPECT_FALSE(unrepresentable_address);

    for (const auto tick : {engrave::Fraction{low - 1, 1},
                            {high + 1, 1},
                            {4294967296LL, 1},
                            {std::numeric_limits<std::int64_t>::min(), 1},
                            {std::numeric_limits<std::int64_t>::max(), 1},
                            {0, high + 1},
                            {0, std::numeric_limits<std::int64_t>::max()},
                            {0, 0},
                            {0, -1},
                            {0, std::numeric_limits<std::int64_t>::min()}}) {
        address.tick = tick;
        const auto found = document->get(engrave::Ref{engrave::Address{address}});
        ASSERT_FALSE(found) << tick.num << '/' << tick.den;
        EXPECT_EQ(found.error().code, ErrorCode::INVALID_ARGUMENT);
    }

    // This used to wrap to tick zero and add text at the first segment.
    address.tick = {4294967296LL, 1};
    const auto before = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(before);
    const auto revision = document->revision();
    const auto edit = document->edit({{engrave::AddText{engrave::Address{address}, "staff", "wrong position"}}});
    ASSERT_FALSE(edit);
    EXPECT_EQ(edit.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(document->revision(), revision);
    const auto after = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(after);
    EXPECT_EQ(*after, *before);

    // A foreign or missing measure keeps the usual handle error even if the
    // rest of the address is malformed.
    address.measure = {0, 0};
    const auto missing = document->get(engrave::Ref{engrave::Address{address}});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::NOT_FOUND);
}

// --- H13 ------------------------------------------------------------------

TEST(Handles, H13DeterministicIds) {
    // The engine is one per process and deterministic ids are a process-global
    // switch, so the check runs in a child process of this same binary.
    ASSERT_FALSE(g_deterministic) << "already the deterministic child";
    std::string command = "\"" + g_program + "\" --deterministic --gtest_filter=Handles.H13IdsWithDeterministicEngine";
#ifdef _WIN32
    command = "call " + command; // cmd strips the first quote of a command that starts with one
#endif
    for (const auto &argument : g_engine_args) command += " \"" + argument + "\"";
    const std::filesystem::path log = std::filesystem::temp_directory_path() / "engrave-handles-h13.log";
    command += " > \"" + log.string() + "\" 2>&1";
    const int status = std::system(command.c_str());
    if (status != 0) {
        std::fprintf(stderr, "the deterministic child failed: %s\n%s\n", command.c_str(), read_text(log).c_str());
    }
    EXPECT_EQ(status, 0) << "the deterministic child failed";
}

TEST(Handles, H13IdsWithDeterministicEngine) {
    if (!g_deterministic) GTEST_SKIP() << "only meaningful in the --deterministic child";
    auto first = open_path(g_corpus.front());
    ASSERT_TRUE(first);
    auto second = open_path(g_corpus.front());
    ASSERT_TRUE(second);
    EXPECT_EQ(element_map(*second), element_map(*first)) << "two opens of one file disagree";
    expect_inv(*first);
    expect_inv(*second);
    auto carrier_document = open_text(R"(<museScore version="5.00"><Score><eid>B_B</eid>
      <Part><Staff id="1"/><Instrument><Channel/></Instrument></Part>
      <Staff id="1"><Measure><voice><Chord><durationType>whole</durationType>
      <Stem><userLen>1</userLen></Stem><Note><eid>C_C</eid><pitch>60</pitch><tpc>14</tpc></Note>
      </Chord></voice></Measure></Staff></Score></museScore>)");
    ASSERT_TRUE(carrier_document);
    auto &carrier_master = master_of(*carrier_document);
    auto *carrier_note = first_note(carrier_master);
    ASSERT_NE(carrier_note, nullptr);
    EXPECT_EQ(carrier_note->eid().to_std_string(), "C_C");
    const auto *carrier = carrier_note->chord()->authored_overrides.carrier({mu::engraving::AuthoredChildRole::STEM});
    ASSERT_NE(carrier, nullptr);
    EXPECT_NE(carrier->legacy_eid.to_std_string(), "C_C");
    auto restored = reopen(*carrier_document);
    ASSERT_TRUE(restored);
    EXPECT_EQ(master_of(*restored).eid_register()->repairs().duplicate, 0u);
}

TEST(Handles, RegistryIdentityUpdatesKeepTheRuntimeHandle) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto &registry = *master.eid_register();
    const auto old_eid = note->eid();
    const auto handle = registry.handle_of(note);
    const auto replacement = registry.new_eid_for_item(note);
    ASSERT_NE(replacement, old_eid);
    EXPECT_EQ(registry.handle_of(note), handle);
    EXPECT_EQ(registry.item_from_eid(old_eid), nullptr);
    EXPECT_EQ(registry.item_from_eid(replacement), note);
    const auto repairs = registry.repairs().duplicate;
    EXPECT_FALSE(registry.register_item_eid(replacement, &master));
    EXPECT_EQ(registry.repairs().duplicate, repairs + 1);
    EXPECT_EQ(registry.item_from_handle(handle), note);
    EXPECT_EQ(note->eid(), replacement);
    expect_inv(*document);
}

TEST(Handles, LoadedMeasureSpacersKeepMembershipThroughRowMembership) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    constexpr std::string_view tag = "<Measure>";
    const auto at = xml.find(tag);
    ASSERT_NE(at, std::string::npos);
    xml.insert(at + tag.size(), "<vspacerUp>2.5</vspacerUp><vspacerDown>3.5</vspacerDown>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto &registry = *master.eid_register();
    auto *measure = master.first_measure();
    ASSERT_NE(measure, nullptr);
    const std::array spacers{measure->vspacer_up(0), measure->vspacer_down(0)};
    std::array<SourceHandle, 2> handles;
    std::array<ElementId, 2> ids;
    std::array<Spatium, 2> gaps;
    for (size_t i = 0; i < spacers.size(); ++i) {
        ASSERT_NE(spacers[i], nullptr);
        ASSERT_TRUE(spacers[i]->eid().is_valid());
        handles[i] = registry.handle_of(spacers[i]);
        ids[i] = engrave::from_engine(spacers[i]->eid());
        gaps[i] = spacers[i]->gap();
        ASSERT_EQ(registry.live_item(handles[i]), spacers[i]);
        const auto resolved = engrave::resolve(master, ids[i]);
        ASSERT_TRUE(resolved);
        EXPECT_EQ(resolved->object, spacers[i]);
    }
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    expect_inv(*document);
}

TEST(Handles, RegistryHandlesAreDocumentScopedAndReadOnly) {
    const auto path = corpus_file({".mscx"});
    auto first = open_path(path);
    auto second = open_path(path);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    auto &a = *master_of(*first).eid_register();
    auto &b = *master_of(*second).eid_register();
    auto *note = first_note(master_of(*first));
    ASSERT_NE(note, nullptr);
    const auto handle = a.handle_of(note);
    ASSERT_TRUE(handle.is_valid());
    EXPECT_EQ(b.item_from_handle(handle), nullptr);
    const auto before = a.entries();
    for (int i = 0; i != 100; ++i) {
        EXPECT_EQ(a.item_from_handle(handle), note);
        EXPECT_EQ(a.item_from_eid(note->eid()), note);
        EXPECT_EQ(a.handle_of(note), handle);
    }
    EXPECT_EQ(a.entries(), before);
    EXPECT_EQ(a.item_from_handle({}), nullptr);
    auto invalid = handle;
    invalid.slot = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(a.item_from_handle(invalid), nullptr);
    invalid = handle;
    ++invalid.generation;
    EXPECT_EQ(a.item_from_handle(invalid), nullptr);
    expect_inv(*first);
    expect_inv(*second);
}

TEST(Handles, RegistryDestructionRetiresHandleBeforeAddressReuse) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto &registry = *master.eid_register();
    auto *copy = note->clone();
    copy->assign_new_eid();
    const auto handle = registry.handle_of(copy);
    ASSERT_TRUE(handle.is_valid());
    delete copy;
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    auto *next = note->clone();
    next->assign_new_eid();
    const auto next_handle = registry.handle_of(next);
    EXPECT_NE(next_handle, handle);
    if (next_handle.slot == handle.slot) EXPECT_GT(next_handle.generation, handle.generation);
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    delete next;
    expect_inv(*document);
}

std::size_t *walk_count = nullptr;

void observe_walk() {
    if (walk_count) ++*walk_count;
}

struct WalkObserver {
    std::size_t count = 0;

    WalkObserver() {
        walk_count = &count;
        engrave::set_object_walk_hook(observe_walk);
    }

    ~WalkObserver() {
        engrave::set_object_walk_hook(nullptr);
        walk_count = nullptr;
    }
};

TEST(Handles, DetachedButParentedSourceRejectsLookupWithoutWalking) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    auto &registry = *master.eid_register();
    auto *copy = note->clone();
    copy->assign_new_eid();
    const auto handle = registry.handle_of(copy);
    EXPECT_EQ(registry.live_item(handle), nullptr) << "construction is not insertion";
    chord->add(copy);
    engrave::assign_missing_ids(master);
    const auto id = engrave::from_engine(copy->eid());
    EXPECT_EQ(registry.live_item(handle), copy);
    chord->remove(copy);
    EXPECT_EQ(copy->ownership_parent(), chord);
    EXPECT_EQ(registry.item_from_handle(handle), copy) << "detached roots remain alive until batch cleanup";
    {
        WalkObserver observer;
        for (int i = 0; i != 100; ++i) {
            const auto result = engrave::resolve(master, id);
            EXPECT_FALSE(result);
            if (!result) EXPECT_EQ(result.error().code, ErrorCode::STALE_HANDLE);
        }
        EXPECT_EQ(observer.count, 0u);
    }
    chord->add(copy);
    EXPECT_EQ(registry.handle_of(copy), handle);
    EXPECT_TRUE(engrave::resolve(master, id));
    chord->remove(copy);
    delete copy;
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    expect_inv(*document);
}

TEST(Handles, SourceSubtreeRemovalAndReinsertionPreserveHandles) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *measure = note->chord()->measure();
    auto &registry = *master.eid_register();
    const std::array<EngravingObject *, 4> objects{measure, note->chord()->segment(), note->chord(), note};
    std::array<mu::engraving::SourceHandle, 4> handles;
    for (std::size_t i = 0; i != objects.size(); ++i) {
        handles[i] = registry.handle_of(objects[i]);
        ASSERT_TRUE(handles[i].is_valid());
        ASSERT_EQ(registry.live_item(handles[i]), objects[i]);
    }
    master.measures()->remove(measure);
    for (auto handle : handles) EXPECT_EQ(registry.live_item(handle), nullptr);
    EXPECT_EQ(measure->ownership_parent(), &master);
    master.measures()->add(measure);
    for (std::size_t i = 0; i != objects.size(); ++i) {
        EXPECT_EQ(registry.handle_of(objects[i]), handles[i]);
        EXPECT_EQ(registry.live_item(handles[i]), objects[i]);
    }
    expect_inv(*document);
}

TEST(Handles, HandlesRejectForcedObjectAddressReuse) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_FALSE(master.parts().empty());
    auto &registry = *master.eid_register();
    alignas(Part) std::array<std::byte, sizeof(Part)> storage;
    auto *first = std::construct_at(reinterpret_cast<Part *>(storage.data()), &master);
    first->assign_new_eid();
    const auto old = registry.handle_of(first);
    ASSERT_TRUE(old.is_valid());
    std::destroy_at(first);
    auto *second = std::construct_at(reinterpret_cast<Part *>(storage.data()), &master);
    second->assign_new_eid();
    const auto current = registry.handle_of(second);
    EXPECT_EQ(first, second);
    EXPECT_EQ(current.slot, old.slot);
    EXPECT_GT(current.generation, old.generation);
    EXPECT_EQ(registry.item_from_handle(old), nullptr);
    EXPECT_EQ(registry.item_from_handle(current), second);
    std::destroy_at(second);
    expect_inv(*document);
}

TEST(Handles, ClosedDocumentHandlesCannotResolveAfterReopen) {
    const auto path = corpus_file({".mscx"});
    auto first = open_path(path);
    ASSERT_TRUE(first);
    auto *note = first_note(master_of(*first));
    ASSERT_NE(note, nullptr);
    const auto handle = master_of(*first).eid_register()->handle_of(note);
    const auto eid = note->eid();
    first.reset();
    auto reopened = open_path(path);
    ASSERT_TRUE(reopened);
    auto &registry = *master_of(*reopened).eid_register();
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    ASSERT_NE(registry.item_from_eid(eid), nullptr);
    EXPECT_NE(registry.handle_of(registry.item_from_eid(eid)).document, handle.document);
    expect_inv(*reopened);
}

TEST(Handles, ExcerptTeardownRetiresDummyOwnedSpanners) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_FALSE(master.excerpts().empty());
    auto *excerpt = master.excerpts().front();
    auto *score = excerpt->excerpt_score();
    auto *spanner = mu::engraving::Factory::create_text_line(score->dummy());
    spanner->set_track(0);
    score->add_spanner(spanner, false);
    engrave::assign_missing_ids(master);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(spanner);
    const auto eid = spanner->eid();
    ASSERT_EQ(registry.live_item(handle), spanner);
    master.remove_excerpt(excerpt);
    delete excerpt;
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    EXPECT_EQ(registry.item_from_eid(eid), nullptr);
    EXPECT_FALSE(engrave::resolve(master, engrave::from_engine(eid)));
    expect_inv(*document);
}

TEST(Handles, ImportedTieIsAddressableBeforeSaving) {
    auto xml = read_text(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    const auto at = xml.find("<eid>J_J</eid>");
    ASSERT_NE(at, std::string::npos);
    xml.erase(at, std::string_view("<eid>J_J</eid>").size());
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *tie = note->tie_for();
    ASSERT_NE(tie, nullptr);
    ASSERT_NE(tie->end_note(), nullptr);
    EXPECT_EQ(tie->tick(), mu::engraving::Fraction(0, 1));
    EXPECT_EQ(tie->end_note()->tick(), mu::engraving::Fraction(1, 4));
    EXPECT_EQ(engrave::id_class(*tie), IdClass::PERSISTED);
    ASSERT_TRUE(tie->eid().is_valid());
    const auto id = engine_id(*tie);
    const auto resolved = engrave::resolve(master, id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->object, tie);
    EXPECT_TRUE(master.eid_register()->published(master.eid_register()->handle_of(tie)));
    const auto ids = engrave::live_ids(master);
    EXPECT_TRUE(std::ranges::any_of(ids, [id](const auto &entry) { return entry.id == id; }));
    auto restored = reopen(*document);
    ASSERT_TRUE(restored);
    const auto reopened = engrave::resolve(master_of(*restored), id);
    ASSERT_TRUE(reopened);
    ASSERT_TRUE(reopened->object->is_tie());
    ASSERT_NE(mu::engraving::to_tie(reopened->object)->end_note(), nullptr);
    EXPECT_EQ(mu::engraving::to_tie(reopened->object)->end_note()->tick(), mu::engraving::Fraction(1, 4));
    expect_inv(*document);
    expect_inv(*restored);
}

TEST(Handles, ExcludingTieEndFromPartDoesNotPublishAnOrphan) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto excerpts = master.excerpts();
    for (auto *excerpt : excerpts) {
        master.remove_excerpt(excerpt);
        delete excerpt;
    }
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    ASSERT_NE(note->tie_for(), nullptr);
    note->tie_for()->end_note()->chord()->set_exclude_from_other_parts(true);
    ASSERT_TRUE(music::generate_score_excerpts(&master));
    engrave::assign_missing_ids(master);
    auto *part_note = mu::engraving::to_note(note->find_linked_in_staff(master.excerpts().front()->excerpt_score()->staff(0)));
    ASSERT_NE(part_note, nullptr);
    EXPECT_EQ(part_note->tie_for(), nullptr);
    EXPECT_EQ(note->tie_for()->end_note()->pitch(), 71);
    auto restored = reopen(*document);
    ASSERT_TRUE(restored);
    expect_inv(*document);
    expect_inv(*restored);
}

TEST(Handles, RewrittenPartKeepsSlightBendIdentity) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *bend = Factory::create_guitar_bend(note);
    bend->set_bend_type(GuitarBendType::SLIGHT_BEND);
    bend->set_note_span(note, note);
    bend->set_track(note->track());
    bend->set_track2(note->track());
    note->add_spanner_for(bend);
    note->add_spanner_back(bend);
    engrave::assign_missing_ids(master);
    const auto original_id = engine_id(*bend);
    const auto original_handle = master.eid_register()->handle_of(bend);
    ASSERT_TRUE(document->edit({{engrave::SetTimeSig{engine_id(*master.first_measure()), {3, 4}}}}));
    EXPECT_EQ(master.eid_register()->live_item(original_handle), nullptr);
    EXPECT_FALSE(engrave::resolve(master, original_id));
    GuitarBend *part_bend = nullptr;
    for (auto *object : engrave::document_objects(master))
        if (object->is_guitar_bend() && object->score() != &master) part_bend = to_guitar_bend(object);
    ASSERT_NE(part_bend, nullptr);
    EXPECT_EQ(part_bend->bend_type(), GuitarBendType::SLIGHT_BEND);
    EXPECT_EQ(part_bend->start_note()->pitch(), 71);
    const auto id = engine_id(*part_bend);
    ASSERT_TRUE(engrave::resolve(master, id));
    auto restored = reopen(*document);
    ASSERT_TRUE(restored);
    const auto found = engrave::resolve(master_of(*restored), id);
    ASSERT_TRUE(found);
    ASSERT_TRUE(found->object->is_guitar_bend());
    EXPECT_EQ(to_guitar_bend(found->object)->bend_type(), GuitarBendType::SLIGHT_BEND);
    expect_inv(*document);
    expect_inv(*restored);
}

TEST(Handles, ConnectingUnmatchedTieRetiresItsHandle) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    Note *last = nullptr;
    for (auto *object : engrave::document_objects(master)) {
        if (!object->is_note()) continue;
        auto *note = mu::engraving::to_note(object);
        if (!last || note->tick() > last->tick()) last = note;
    }
    ASSERT_NE(last, nullptr);
    ASSERT_EQ(last->tie_for(), nullptr);
    auto *tie = mu::engraving::Factory::create_tie(last);
    tie->set_track(last->track());
    tie->set_start_note(last);
    last->set_tie_for(tie);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(tie);
    ASSERT_EQ(registry.live_item(handle), tie);
    master.connect_ties(false);
    EXPECT_EQ(last->tie_for(), nullptr);
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    expect_inv(*document);
}

TEST(Handles, RemovedBeamStaysAliveUntilBatchEnd) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    ASSERT_EQ(chord->beam(), nullptr);
    auto *beam = mu::engraving::Factory::create_beam(&master);
    beam->set_generated(false);
    beam->add(chord);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(beam);
    ASSERT_EQ(registry.live_item(handle), beam);
    Command command(master);
    master.apply_element_remove(beam);
    EXPECT_EQ(registry.item_from_handle(handle), beam);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    command.commit();
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    expect_inv(*document);
}

TEST(Handles, MovingTremoloRebindsMembership) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    std::vector<mu::engraving::Chord *> chords;
    for (auto *segment = measure->first(); segment; segment = segment->next()) {
        auto *element = segment->element(0);
        if (element && element->is_chord()) chords.push_back(mu::engraving::to_chord(element));
    }
    ASSERT_GE(chords.size(), 4u);
    auto *tremolo = mu::engraving::Factory::create_tremolo_two_chord(chords[0]);
    tremolo->set_tremolo_type(mu::engraving::TremoloType::C8);
    tremolo->set_duration_type(chords[0]->duration_type());
    tremolo->set_chords(chords[0], chords[1]);
    chords[0]->set_tremolo_two_chord(tremolo, false);
    chords[1]->set_tremolo_two_chord(tremolo, false);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(tremolo);
    ASSERT_EQ(registry.live_item(handle), tremolo);
    Command command(master);
    mu::engraving::apply_move_tremolo(&master, chords[2]->tick(), chords[3]->tick(), tremolo, 0);
    auto *old_segment = chords[0]->segment();
    old_segment->remove(chords[0]);
    EXPECT_EQ(registry.live_item(handle), tremolo);
    old_segment->add(chords[0]);
    command.commit();
    EXPECT_EQ(registry.live_item(handle), tremolo);
    delete tremolo;
    expect_inv(*document);
}

TEST(Handles, StaffSortingAndReparentingPreserveIds) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(note);
    const auto id = engine_id(*note);
    auto *staff = master.staff(0);
    const auto staff_id = engine_id(*staff);
    const auto staff_handle = registry.handle_of(staff);
    std::vector<mu::engraving::staff_idx_t> order;
    for (size_t i = 0; i < master.staves().size(); ++i) order.push_back(master.staves().size() - i - 1);
    ASSERT_EQ(master.staves().size(), 2u);
    master.sort_staves(order);
    EXPECT_EQ(master.staff(1), staff);
    EXPECT_EQ(engrave::resolve(master, staff_id)->object, staff);
    EXPECT_EQ(registry.handle_of(staff), staff_handle);
    auto *destination = mu::engraving::to_chord(chord->segment()->next_cr(chord->track())->element(chord->track()));
    ASSERT_NE(destination, nullptr);
    mu::engraving::apply_element_reparent(*note, *destination, destination->track());
    EXPECT_EQ(engrave::resolve(master, id)->object, note);
    EXPECT_EQ(registry.handle_of(note), handle);
    EXPECT_EQ(note->chord(), destination);
    mu::engraving::apply_element_reparent(*note, *chord, chord->track());
    auto *segment = chord->segment();
    auto *measure = segment->measure();
    const auto tick = segment->rtick();
    const auto segment_handle = registry.handle_of(segment);
    ASSERT_NE(measure->next_measure(), nullptr);
    mu::engraving::apply_segment_reparent(*segment, *measure->next_measure(), mu::engraving::Fraction(1, 4));
    EXPECT_EQ(registry.live_item(segment_handle), segment);
    EXPECT_EQ(engrave::resolve(master, id)->object, note);
    mu::engraving::apply_segment_reparent(*segment, *measure, tick);
    expect_inv(*document);
}

TEST(Handles, BatchDiffRetainsDestroyedIdentityAcrossSlotReuse) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    const auto old_id = engine_id(*note);
    ElementId new_id;
    auto result = engrave::api::BatchGuard(*document).run({[&](engrave::api::EditContext &) -> engrave::Result<void> {
        auto *replacement = note->clone();
        master.apply_element_remove(note);
        delete note;
        replacement->assign_new_eid();
        chord->add(replacement);
        new_id = engine_id(*replacement);
        return {};
    }});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->created, new_id));
    EXPECT_TRUE(std::ranges::contains(result->replaced, old_id));
    EXPECT_FALSE(engrave::resolve(master, old_id));
    EXPECT_TRUE(engrave::resolve(master, new_id));
    expect_inv(*document);
}

TEST(Handles, InsertBeforeRemovalUsesTheOriginalReplacementPosition) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *other = note->clone();
    other->set_pitch(64);
    note->chord()->add(other);
    engrave::assign_missing_ids(master);
    const auto id = engine_id(*note);
    const auto measure = engine_id(*note->chord()->measure());
    const auto result = document->edit({{engrave::InsertMeasures{measure, 1}, engrave::Remove{id}}});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->replaced, id));
    EXPECT_FALSE(std::ranges::contains(result->removed, id));
    EXPECT_FALSE(engrave::resolve(master, id));
    expect_inv(*document);
}

TEST(Handles, RemovedExcerptDetachesBeamsBeforeTheNextOperation) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_FALSE(master.excerpts().empty());
    auto *excerpt = master.excerpts().front();
    auto *score = excerpt->excerpt_score();
    Note *note = nullptr;
    for (auto *object : engrave::document_objects(master))
        if (object->score() == score && object->is_note()) {
            note = mu::engraving::to_note(object);
            break;
        }
    ASSERT_NE(note, nullptr);
    auto *beam = mu::engraving::Factory::create_beam(score);
    beam->set_generated(false);
    beam->add(note->chord());
    engrave::assign_missing_ids(master);
    const auto id = engine_id(*beam);
    ASSERT_TRUE(engrave::resolve(master, id));
    auto result = engrave::api::BatchGuard(*document).run({[&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               master.apply_excerpt(excerpt, false);
                                                               return {};
                                                           },
                                                           [&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               EXPECT_FALSE(engrave::resolve(master, id));
                                                               return {};
                                                           }});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->removed, id));
    EXPECT_FALSE(engrave::resolve(master, id));
    expect_inv(*document);
}

TEST(Handles, RegeneratedExcerptReplacesIdsAtTheOriginalScoreIndex) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_EQ(master.excerpts().size(), 1u);
    engrave::assign_missing_ids(master);
    auto *excerpt = master.excerpts().front();
    ElementId old_id;
    for (auto *object : engrave::document_objects(master))
        if (object->score() == excerpt->excerpt_score() && object->is_note()) {
            old_id = engine_id(*object);
            break;
        }
    ASSERT_TRUE(engrave::resolve(master, old_id));
    auto result = engrave::api::BatchGuard(*document).run({[&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               master.apply_excerpt(excerpt, false);
                                                               return {};
                                                           },
                                                           [&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               music::generate_score_excerpts(&master);
                                                               return {};
                                                           }});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->replaced, old_id));
    EXPECT_FALSE(std::ranges::contains(result->removed, old_id));
    expect_inv(*document);
}

TEST(Handles, EmptyingPublishedTextRemovesItsDiffIdButKeepsItsHandle) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *text = first_live(master, ElementType::TEXT);
    ASSERT_NE(text, nullptr);
    const auto id = engine_id(*text);
    const auto cleared = document->edit({{engrave::SetProperty{id, "text", std::string{}}}});
    ASSERT_TRUE(cleared);
    EXPECT_TRUE(std::ranges::contains(cleared->removed, id));
    EXPECT_TRUE(engrave::resolve(master, id));
    const auto filled = document->edit({{engrave::SetProperty{id, "text", std::string{"Restored title"}}}});
    ASSERT_TRUE(filled);
    EXPECT_TRUE(std::ranges::contains(filled->created, id));
    expect_inv(*document);
}

TEST(Handles, HiddenStaffSpacersLeaveTheDiffWhileTheirHandlesStillResolve) {
    auto xml = read_text(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    const auto at = xml.find("<Measure>");
    ASSERT_NE(at, std::string::npos);
    xml.insert(at + std::string_view("<Measure>").size(), "<vspacerUp>2.5</vspacerUp><vspacerDown>3.5</vspacerDown>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    const std::array ids{engine_id(*measure->vspacer_up(0)), engine_id(*measure->vspacer_down(0))};
    for (auto target : {engine_id(*master.staff(0)), engine_id(*master.staff(0)->part())}) {
        const auto hidden = document->edit({{engrave::SetProperty{target, "visible", false}}});
        ASSERT_TRUE(hidden);
        for (auto id : ids) {
            EXPECT_TRUE(std::ranges::contains(hidden->removed, id));
            EXPECT_TRUE(engrave::resolve(master, id));
        }
        const auto shown = document->edit({{engrave::SetProperty{target, "visible", true}}});
        ASSERT_TRUE(shown);
        for (auto id : ids) EXPECT_TRUE(std::ranges::contains(shown->created, id));
    }
    expect_inv(*document);
}

TEST(Handles, RejectedAnnotationDoesNotBecomeSourceMember) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    auto *segment = measure->get_segment(SegmentType::END_BAR_LINE, measure->end_tick());
    auto *first = mu::engraving::Factory::create_play_count_text(segment);
    first->set_track(0);
    segment->add(first);
    auto *rejected = mu::engraving::Factory::create_play_count_text(segment);
    rejected->set_track(0);
    rejected->assign_new_eid();
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(rejected);
    segment->add(rejected);
    EXPECT_EQ(rejected->ownership_parent(), segment);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    EXPECT_EQ(segment->find_annotation(ElementType::PLAY_COUNT_TEXT, 0, 0), first);
    delete rejected;
    segment->remove(first);
    delete first;
    expect_inv(*document);
}

TEST(Handles, TextFrameReplacementMaintainsSourceMembership) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/repeat70.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *object = first_live(master, ElementType::TBOX);
    ASSERT_NE(object, nullptr);
    auto *frame = mu::engraving::to_t_box(object);
    auto *text = frame->text();
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(text);
    ASSERT_EQ(registry.live_item(handle), text);
    frame->remove(text);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    EXPECT_EQ(registry.live_item(registry.handle_of(frame->text())), frame->text());
    frame->add(text);
    EXPECT_EQ(registry.handle_of(text), handle);
    EXPECT_EQ(registry.live_item(handle), text);
    expect_inv(*document);
}

TEST(Handles, NativeArchiveAndCheckpointPreserveExcerptFileName) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_FALSE(master.excerpts().empty());
    auto *excerpt = master.excerpts().front();
    excerpt->set_file_name(u"imported_custom_name");
    ASSERT_TRUE(document->checkpoint("filename"));
    EXPECT_EQ(excerpt->file_name(), muse::String(u"imported_custom_name"));
    ASSERT_TRUE(document->export_score({.format = engrave::Format::MSCZ}));
    EXPECT_EQ(excerpt->file_name(), muse::String(u"imported_custom_name"));
    expect_inv(*document);
}

// --- corpus discovery -----------------------------------------------------

bool looks_like_score(const std::filesystem::path &path) {
    static const std::set<std::string> extensions = {".mscx", ".mscz", ".musicxml", ".mxl", ".mid", ".midi",
                                                     ".gp",   ".gp3",  ".gp4",      ".gp5", ".gpx", ".ptb"};
    return extensions.contains(path.extension().string());
}

void collect(const std::filesystem::path &path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::vector<std::filesystem::path> entries;
        for (const auto &entry : std::filesystem::directory_iterator(path, ec)) entries.push_back(entry.path());
        std::ranges::sort(entries);
        for (const auto &entry : entries) collect(entry);
        return;
    }
    if (std::filesystem::is_regular_file(path, ec) && looks_like_score(path)) g_corpus.push_back(path);
}

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    g_program = argv[0];
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument.starts_with("--assets=")) {
            g_assets = std::filesystem::path(argument.substr(9));
            g_engine_args.emplace_back(argument);
        } else if (argument.starts_with("--corpus=")) {
            collect(std::filesystem::path(argument.substr(9)));
            g_engine_args.emplace_back(argument);
        } else if (argument == "--deterministic") {
            g_deterministic = true;
        } else if (argument == "--update-inventory") {
            g_update_inventory = true;
        }
    }
    if (g_corpus.empty()) {
        for (const auto &candidate : {std::filesystem::path("fixtures/golden"), std::filesystem::path("../fixtures/golden"),
                                      std::filesystem::path("../../fixtures/golden")}) {
            std::error_code ec;
            if (std::filesystem::is_directory(candidate, ec)) collect(candidate);
        }
    }
    if (g_corpus.empty()) {
        std::fprintf(stderr, "no corpus: pass --corpus=<file or directory>\n");
        return 1;
    }

    auto engine = engrave::Engine::create({.resources = g_assets, .deterministic_ids = g_deterministic});
    if (!engine) {
        std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", engine.error().message.c_str());
        return 1;
    }
    g_engine = std::move(*engine);
    const int result = RUN_ALL_TESTS();
    g_engine.reset();
    return result;
}
