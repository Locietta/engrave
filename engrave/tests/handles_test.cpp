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
#include "engraving/dom/beam.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/playcounttext.h"
#include "engraving/dom/property.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spacer.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/editing/addremoveelement.h"
#include "engraving/editing/edittremolo.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/infrastructure/eidregister.h"
#include "engraving/rw/inoutdata.h"
#include "engraving/rw/rwregister.h"
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

TEST(NativeSave, ColdEqualsWarm) {
    using namespace mu::engraving;
    std::vector<std::filesystem::path> inputs;
    for (const auto &entry : std::filesystem::directory_iterator(g_source_root / "tests/data/inputs/golden")) {
        if (written_by_engine(entry.path()) || entry.path().filename() == "created.json") inputs.push_back(entry.path());
    }
    inputs.push_back(g_source_root / "tests/data/inputs/evening.musicxml");
    std::ranges::sort(inputs);
    const std::map<std::string, std::string> expected_differences{
        {"tab_grace_prebend.mscx", "S5b: bend layout overwrites destination note visibility"}};
    for (const auto &path : inputs) {
        SCOPED_TRACE(path.filename().string());
        auto opened = path.extension() == ".json" ? g_engine->open(as_bytes(composition_input())) : g_engine->open(path);
        ASSERT_TRUE(opened) << opened.error().message;
        auto &master = master_of(**opened);
        std::vector<Score *> scores{&master};
        for (const auto *excerpt : master.excerpts()) scores.push_back(excerpt->excerpt_score());
        const auto check_cold = [&] {
            for (const auto *score : scores) {
                EXPECT_EQ(score->layout_state.whole_count, 0u);
                EXPECT_EQ(score->layout_state.range_count, 0u);
                EXPECT_TRUE(score->pages().empty());
            }
        };
        const auto write = [&] {
            muse::io::Buffer buffer;
            buffer.open(muse::io::IODevice::WRITE_ONLY);
            rw::WriteInOutData inout(&master);
            inout.ctx.set_inline_style(true);
            inout.ctx.set_inline_excerpts(true);
            EXPECT_TRUE(rw::RWRegister::writer()->write_score(&master, &buffer, &inout));
            return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
        };
        check_cold();
        const auto cold = write();
        check_cold();
        EXPECT_EQ(cold, write());
        check_cold();
        for (auto *score : scores) {
            score->ensure_layout();
            EXPECT_GT(score->layout_state.whole_count, 0u);
            EXPECT_FALSE(score->pages().empty());
        }
        const auto warm = write();
        if (const auto expected = expected_differences.find(path.filename().string()); expected != expected_differences.end()) {
            EXPECT_NE(cold, warm) << expected->second << "; remove the exception once fixed";
        } else {
            EXPECT_EQ(cold, warm);
        }
    }
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

        // Deleting the master element takes the linked one with it.  The engine's deletion
        // command is used, not a bare element removal: deleting the last note of a chord
        // leaves a rest in both scores, because a chord without notes is not a valid model
        // element (layout reads its first note).
        commit(master, [&](MasterScore &) {
            master.select(master_note);
            master.cmd_delete_selection();
        });
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
