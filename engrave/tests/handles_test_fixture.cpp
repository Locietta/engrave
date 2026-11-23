#include "handles_test_fixture.h"

#include "global/btree.h"
#include "global/hashmap.h"

namespace handles_test {

std::filesystem::path g_assets;

std::filesystem::path g_inventory = ENGRAVE_TEST_SOURCE_ROOT "/engrave/tests/handle_inventory.json";

std::filesystem::path g_source_root = ENGRAVE_TEST_SOURCE_ROOT;

std::vector<std::filesystem::path> g_corpus;

std::vector<std::string> g_engine_args;
// --assets and --corpus again, for the child
std::string g_program;

bool g_deterministic = false;

bool g_update_inventory = false;

engrave::Engine *g_engine = nullptr;

std::string read_text(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::byte> as_bytes(std::string_view text) {
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    return {data, data + text.size()};
}

std::string text_of(const std::vector<std::byte> &bytes) { return {reinterpret_cast<const char *>(bytes.data()), bytes.size()}; }

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

std::unique_ptr<engrave::Document> open_path(const std::filesystem::path &path) {
    auto document = g_engine->open(path);
    EXPECT_TRUE(document) << path.string() << ": " << document.error().message;
    return document ? std::move(*document) : nullptr;
}

std::unique_ptr<engrave::Document> open_text(std::string_view text, engrave::OpenOptions options) {
    auto document = g_engine->open(as_bytes(text), options);
    EXPECT_TRUE(document) << document.error().message;
    return document ? std::move(*document) : nullptr;
}

std::string export_mscz(engrave::Document &document) {
    auto bytes = document.export_score({.format = engrave::Format::MSCZ});
    EXPECT_TRUE(bytes) << bytes.error().message;
    return bytes ? text_of(*bytes) : std::string{};
}

muse::BTreeMap<std::string, std::string> unzip_entries(std::string_view zip_bytes) {
    muse::BTreeMap<std::string, std::string> entries;
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
std::string export_mscx(engrave::Document &document, int excerpt_id) {
    music::ExportOptions options;
    options.excerpt_id = excerpt_id;
    return music::export_native_score(engrave::DocumentAccess::context(document), engrave::DocumentAccess::project(document), "mscx",
                                      options)
        .value();
}

/// The snapshot path: write the score and read it back into a new document.
std::unique_ptr<engrave::Document> reopen(engrave::Document &document) {
    const std::string mscz = export_mscz(document);
    if (mscz.empty()) return nullptr;
    auto copy = g_engine->open(as_bytes(mscz));
    EXPECT_TRUE(copy) << copy.error().message;
    return copy ? std::move(*copy) : nullptr;
}

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
    const muse::HashMap<int, std::string_view> types{{120, "16th"}, {240, "eighth"}, {480, "quarter"}, {960, "half"}, {1920, "whole"}};
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

muse::BTreeMap<std::string, std::string> native_captures(engrave::Document &document, bool legacy) {
    muse::BTreeMap<std::string, std::string> captures;
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

std::string source_native(Score &score) {
    muse::io::Buffer buffer;
    buffer.open(muse::io::IODevice::WRITE_ONLY);
    mu::engraving::rw::WriteInOutData inout(&score);
    inout.ctx.set_inline_style(true);
    EXPECT_TRUE(mu::engraving::rw::RWRegister::writer()->write_score(&score, &buffer, &inout));
    return std::string(reinterpret_cast<const char *>(buffer.data().const_data()), buffer.data().size());
}

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
muse::BTreeMap<std::string, std::string> element_map(engrave::Document &document) {
    muse::BTreeMap<std::string, std::string> elements;
    for (const engrave::LiveId &entry : engrave::live_ids(master_of(document))) {
        elements.emplace(engrave::to_string(entry.id), describe(entry));
    }
    return elements;
}

/// Compares two element maps, reporting the ids that differ rather than the whole map.
void expect_same_ids(const muse::BTreeMap<std::string, std::string> &before, const muse::BTreeMap<std::string, std::string> &after,
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

void expect_inv(engrave::Document &document) {
    MasterScore &master = master_of(document);
    muse::HashSet<const EngravingObject *> objects;
    for (EngravingObject *object : engrave::document_objects(master)) objects.insert(object);

    // The register is a bijection: an entry's element reports that id, and the id of a
    // persisted element names an element of the document.
    muse::HashSet<std::string> registered;
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
}

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
void inventory_evidence(engrave::Document &document, const std::string &source, muse::BTreeMap<std::string, Evidence> &evidence) {
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

    muse::BTreeMap<ElementId, Probed> probed;
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
    muse::BTreeMap<ElementId, std::string> after;
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
std::unique_ptr<engrave::Document> created_types_document(muse::HashSet<std::string> &types) {
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
        muse::HashSet<const EngravingObject *> before;
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

void expect_first_ids(engrave::Document &document) {
    MasterScore &master = master_of(document);
    muse::HashMap<std::string, std::string> by_text;
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
muse::BTreeMap<std::string, std::string> id_types(const std::string &text) {
    muse::BTreeMap<std::string, std::string> types;
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

} // namespace handles_test
