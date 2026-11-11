// Tests of the describe catalogue, of the value codec and of the pid support table.
//
//   engrave_describe_test --assets=<resources> [--python=<interpreter>] <score file or directory>...
//
// The score files are the corpus: they supply one live instance per element type,
// so the table can be checked against the engine itself.  A type the corpus does
// not contain is skipped and counted.  --python enables the generator check
// (tools/gen_pid_support.py --check); without it that test is skipped.
//
// The corpus is read with the library's own loader (music::load_native_score), not
// through a Document, because the engine's objects are what the table is about.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/pid_support.h"
#include "engrave/src/api/sniff.h"
#include "engrave/src/api/state.h"
#include "engrave/src/api/value_codec.h"
#include "engrave/src/runtime/score_io.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/property.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/engravingproject.h"
#include "engraving/types/groupnode.h"
#include "engraving/types/pitchvalue.h"
#include "engraving/types/typesconv.h"
#include <engrave/engrave.h>

namespace {

using engrave::Enum;
using engrave::ErrorCode;
using engrave::Opaque;
using engrave::Point;
using engrave::Size;
using engrave::Unit;
using engrave::Value;
using engrave::ValueKind;
using mu::engraving::ElementType;
using mu::engraving::EngravingObject;
using mu::engraving::PType;
using mu::engraving::Pid;
using mu::engraving::PropertyValue;

std::filesystem::path g_assets;
std::filesystem::path g_python;
std::vector<std::filesystem::path> g_corpus_files;
std::unique_ptr<engrave::Engine> g_engine;
// A document of g_engine through which the corpus loader reaches the engine context.
std::unique_ptr<engrave::Document> g_anchor;

std::string read_file(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

const char *backend_name(engrave::Format format) {
    switch (format) {
        case engrave::Format::MSCZ: return "mscz";
        case engrave::Format::MSCX: return "mscx";
        case engrave::Format::MUSIC_XML: return "musicxml";
        case engrave::Format::MXL: return "mxl";
        case engrave::Format::MIDI: return "midi";
        case engrave::Format::GUITAR_PRO: return "gp";
    }
    return "";
}

// One loaded corpus score per file, with the first live instance of each type.
struct Corpus {
    std::vector<music::ScoreProject> projects;
    std::map<ElementType, EngravingObject *> instances;
    std::vector<std::filesystem::path> unreadable;

    void add(EngravingObject *object) { instances.try_emplace(object->type(), object); }

    // The engine's scanElements visits an item's children, not the item, so the
    // walk adds every level itself: the score's parts and staves, then each
    // measure's segments and their elements.
    void add_score(mu::engraving::Score *score) {
        add(score);
        for (mu::engraving::Part *part : score->parts()) {
            add(part);
            for (mu::engraving::Staff *staff : part->staves()) add(staff);
        }
        const auto scan = [this](mu::engraving::EngravingItem *item) {
            add(item);
            item->scan_elements([this](mu::engraving::EngravingItem *child) { add(child); });
        };
        for (mu::engraving::MeasureBase *base = score->first(); base; base = base->next()) {
            add(base);
            if (!base->is_measure()) continue;
            auto *measure = mu::engraving::to_measure(base);
            add(measure);
            for (mu::engraving::Segment *segment = measure->first(); segment; segment = segment->next()) {
                add(segment);
                for (mu::engraving::track_idx_t track = 0; track < score->nstaves() * mu::engraving::voices; ++track)
                    if (mu::engraving::EngravingItem *item = segment->element(track)) scan(item);
                for (mu::engraving::EngravingItem *item : segment->annotations()) scan(item);
            }
        }
        // The engine's own walk reaches the system-level items (pages, systems,
        // instrument names, brackets) that the measure walk above does not.
        score->scan_elements([this](mu::engraving::EngravingItem *item) { add(item); });
    }

    void load(const std::filesystem::path &file) {
        const std::string bytes = read_file(file);
        const auto format = engrave::sniff_format(bytes);
        if (!format) {
            unreadable.push_back(file);
            return;
        }
        try {
            projects.push_back(music::load_native_score(engrave::DocumentAccess::context(*g_anchor), bytes, backend_name(*format)));
        } catch (const std::exception &) {
            unreadable.push_back(file);
            return;
        }
        mu::engraving::MasterScore *master = projects.back()->master_score();
        add_score(master);
        for (mu::engraving::Excerpt *excerpt : master->excerpts()) add_score(excerpt->excerpt_score());
    }
};

bool is_enum_without_tokens(PType type) {
    for (const PType declared : {PType::SYMID, PType::ALIGN, PType::ORNAMENT_INTERVAL, PType::ACCIDENTAL_ROLE, PType::HOOK_TYPE})
        if (declared == type) return true;
    return false;
}

std::vector<PType> all_types() {
    std::vector<PType> types;
    for (int type = 0; type <= int(PType::GROUPS); ++type) types.push_back(PType(type));
    return types;
}

// A value of each kind, and an enum token for each enum type.
Value sample(PType type) {
    switch (engrave::api::kind_of(type)) {
        case ValueKind::BOOL: return true;
        case ValueKind::INT: return std::int64_t(42);
        case ValueKind::REAL: return 1.5;
        case ValueKind::STRING: return std::string("text");
        case ValueKind::POINT: return Point{1.5, -2.5};
        case ValueKind::SIZE: return Size{3.5, 4.5};
        case ValueKind::COLOR: return engrave::Color{1, 2, 3, 4};
        case ValueKind::FRACTION: return engrave::Fraction{3, 4};
        case ValueKind::INT_LIST: return std::vector<std::int64_t>{1, 2, 3};
        case ValueKind::ENUM: {
            const auto tokens = engrave::api::enum_tokens(type);
            if (!tokens.empty()) return Enum{std::string(tokens.front())};
            switch (type) {
                case PType::SYMID: return Enum{"noteheadBlack"};
                case PType::ALIGN: return Enum{"left,top"};
                case PType::ORNAMENT_INTERVAL: return Enum{"second,auto"};
                case PType::ACCIDENTAL_ROLE: return Enum{"0"};
                case PType::HOOK_TYPE: return Enum{"0"};
                default: return Enum{"unspecified"};
            }
        }
        case ValueKind::OPAQUE: return Opaque{"read-only"};
    }
    return {};
}

// A property value each Opaque kind maps from.
PropertyValue opaque_sample(PType type) {
    switch (type) {
        case PType::DRAW_PATH: return PropertyValue(muse::draw::PainterPath());
        case PType::GROUPS: return PropertyValue(mu::engraving::GroupNodes());
        case PType::PITCH_VALUES: return PropertyValue(mu::engraving::PitchValues());
        case PType::DURATION_TYPE_WITH_DOTS: return PropertyValue(mu::engraving::DurationTypeWithDots());
        default: return PropertyValue();
    }
}

ElementType element_type(std::string_view name) {
    return mu::engraving::TConv::from_xml(muse::AsciiStringView(name.data(), name.size()), ElementType::INVALID);
}

PType value_type(std::string_view name) { return mu::engraving::property_type(engrave::api::pid_of(name)); }

bool contains(const std::vector<std::string> &list, std::string_view text) { return std::ranges::find(list, text) != list.end(); }

} // namespace

TEST(ValueCodec, RoundTripsEveryType) {
    int opaque = 0;
    for (const PType type : all_types()) {
        if (type == PType::UNDEFINED) {
            EXPECT_TRUE(std::holds_alternative<std::monostate>(engrave::api::to_value(type, PropertyValue())));
            continue;
        }
        if (engrave::api::kind_of(type) == ValueKind::OPAQUE) {
            EXPECT_TRUE(std::holds_alternative<Opaque>(engrave::api::to_value(type, opaque_sample(type)))) << int(type);
            ++opaque;
            continue;
        }
        const Value value = sample(type);
        const auto property = engrave::api::from_value(type, value);
        ASSERT_TRUE(property) << int(type) << ": " << property.error().message;
        EXPECT_EQ(engrave::api::to_value(type, *property), value) << int(type);
    }
    EXPECT_EQ(opaque, 4) << "DRAW_PATH, GROUPS, PITCH_VALUES and DURATION_TYPE_WITH_DOTS";
}

TEST(ValueCodec, RoundTripsEveryEnumToken) {
    int tokens = 0;
    for (const PType type : all_types()) {
        if (engrave::api::kind_of(type) != ValueKind::ENUM) continue;
        const auto listed = engrave::api::enum_tokens(type);
        EXPECT_TRUE(!listed.empty() || is_enum_without_tokens(type)) << int(type) << " lists no tokens";
        for (const std::string_view token : listed) {
            const auto property = engrave::api::from_value(type, Enum{std::string(token)});
            ASSERT_TRUE(property) << int(type) << " token '" << token << "': " << property.error().message;
            EXPECT_EQ(engrave::api::to_value(type, *property), Value(Enum{std::string(token)})) << int(type);
            ++tokens;
        }
    }
    EXPECT_GT(tokens, 400) << "the enum token tables";
}

TEST(ValueCodec, RejectsWhatDoesNotFitTheType) {
    const auto unknown = engrave::api::from_value(PType::ALIGN_H, Enum{"no-such-token"});
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code, ErrorCode::INVALID_ARGUMENT);

    const auto wrong_kind = engrave::api::from_value(PType::BOOL, std::int64_t(1));
    ASSERT_FALSE(wrong_kind);
    EXPECT_EQ(wrong_kind.error().code, ErrorCode::INVALID_ARGUMENT);

    const auto wrong_size = engrave::api::from_value(PType::SIZE, Point{1, 2});
    ASSERT_FALSE(wrong_size);
    EXPECT_EQ(wrong_size.error().code, ErrorCode::INVALID_ARGUMENT);

    // A caller's integer has to fit what the engine stores; the probe of finding 3 of
    // the edit validation set an int style to 4294967297 and got 1 back.
    for (const std::int64_t too_big : {std::int64_t{4294967297LL}, std::int64_t{1} << 32, std::int64_t{-4294967295LL}}) {
        const auto overflow = engrave::api::from_value(PType::INT, too_big);
        ASSERT_FALSE(overflow) << too_big;
        EXPECT_EQ(overflow.error().code, ErrorCode::INVALID_ARGUMENT) << too_big;
        const auto fraction = engrave::api::from_value(PType::FRACTION, engrave::Fraction{too_big, 1});
        ASSERT_FALSE(fraction) << too_big;
        EXPECT_EQ(fraction.error().code, ErrorCode::INVALID_ARGUMENT) << too_big;
    }
    for (const std::int64_t negative : {std::int64_t{-1}, std::int64_t{-4294967295LL}}) {
        const auto size = engrave::api::from_value(PType::SIZE_T, negative);
        ASSERT_FALSE(size) << negative;
        EXPECT_EQ(size.error().code, ErrorCode::INVALID_ARGUMENT) << negative;
    }
    const auto negative_size = engrave::api::from_value(PType::SIZE_T, std::int64_t{-1});
    ASSERT_FALSE(negative_size);
    EXPECT_EQ(negative_size.error().code, ErrorCode::INVALID_ARGUMENT);
    const auto list = engrave::api::from_value(PType::INT_VEC, std::vector<std::int64_t>{1, 4294967297LL, 3});
    ASSERT_FALSE(list);
    EXPECT_EQ(list.error().code, ErrorCode::INVALID_ARGUMENT);
    const auto fraction = engrave::api::from_value(PType::FRACTION, engrave::Fraction{1, 4294967297LL});
    ASSERT_FALSE(fraction);
    EXPECT_EQ(fraction.error().code, ErrorCode::INVALID_ARGUMENT);

    for (const PType type : {PType::DRAW_PATH, PType::GROUPS, PType::PITCH_VALUES, PType::DURATION_TYPE_WITH_DOTS}) {
        const auto read_only = engrave::api::from_value(type, Opaque{"draw_path"});
        ASSERT_FALSE(read_only);
        EXPECT_EQ(read_only.error().code, ErrorCode::UNSUPPORTED) << int(type);
    }
}

TEST(Describe, ListsEveryImplementedTypeWithSortedProps) {
    const auto types = g_engine->describe();
    EXPECT_EQ(types.size(), 147u) << "the TConv element types that a class declares";
    std::set<std::string> names;
    for (const auto &type : types) {
        EXPECT_TRUE(names.insert(type.type).second) << "duplicate type " << type.type;
        EXPECT_NE(element_type(type.type), ElementType::INVALID) << type.type;
        EXPECT_TRUE(std::ranges::is_sorted(type.props, {}, &engrave::PropInfo::name)) << type.type;
        // Score's getProperty answers nothing: its properties are structural.
        EXPECT_TRUE(!type.props.empty() || type.type == "Score") << type.type;
        for (const auto &prop : type.props) {
            EXPECT_NE(engrave::api::pid_of(prop.name), Pid::END) << type.type << " / " << prop.name;
            EXPECT_FALSE(prop.group.empty()) << prop.name;
            if (prop.kind == ValueKind::ENUM)
                EXPECT_TRUE(!prop.enum_tokens.empty() || is_enum_without_tokens(value_type(prop.name))) << type.type << " / " << prop.name;
        }
    }
    EXPECT_TRUE(names.contains("Note"));
    EXPECT_TRUE(names.contains("Accidental"));
    EXPECT_FALSE(names.contains("Selection")) << "no class declares it";
}

TEST(Describe, NotesTheKindsAndUnits) {
    const auto note = g_engine->describe("Note");
    ASSERT_TRUE(note) << note.error().message;
    EXPECT_EQ(note->type, "Note");
    const auto find = [&note](std::string_view name) -> const engrave::PropInfo * {
        const auto it = std::ranges::find(note->props, name, &engrave::PropInfo::name);
        return it == note->props.end() ? nullptr : &*it;
    };
    const auto *pitch = find("pitch");
    ASSERT_TRUE(pitch);
    EXPECT_EQ(pitch->kind, ValueKind::INT);
    EXPECT_FALSE(pitch->settable) << "pitch goes through the engine's pitch edit";
    EXPECT_EQ(pitch->group, "none");
    const auto *min_distance = find("minDistance");
    ASSERT_TRUE(min_distance);
    EXPECT_EQ(min_distance->kind, ValueKind::REAL);
    EXPECT_EQ(min_distance->unit, Unit::SPATIUM);
    EXPECT_EQ(min_distance->group, "position");
    const auto *color = find("color");
    ASSERT_TRUE(color);
    EXPECT_EQ(color->kind, ValueKind::COLOR);
    EXPECT_EQ(color->unit, Unit::NONE);
    const auto *autoplace = find("autoplace");
    ASSERT_TRUE(autoplace);
    EXPECT_TRUE(autoplace->settable);
}

TEST(Describe, UnknownTypeIsNotFound) {
    const auto missing = g_engine->describe("NoSuchElement");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::NOT_FOUND);
    EXPECT_FALSE(missing.error().message.empty());
}

TEST(Describe, StylesCoverEveryStyleId) {
    const auto styles = g_engine->describe_styles();
    ASSERT_EQ(styles.size(), size_t(mu::engraving::Sid::STYLES));
    std::set<std::string> names;
    int with_value = 0;
    for (size_t i = 0; i < styles.size(); ++i) {
        const auto &style = styles[i];
        EXPECT_TRUE(names.insert(style.name).second) << "duplicate style " << style.name;
        EXPECT_FALSE(style.name.empty());
        if (std::holds_alternative<std::monostate>(style.default_value)) continue;
        ++with_value;
        EXPECT_EQ(style.kind, engrave::api::kind_of(mu::engraving::StyleDef::style_values[i].value_type())) << style.name;
        EXPECT_EQ(style.unit, engrave::api::unit_of(mu::engraving::StyleDef::style_values[i].value_type())) << style.name;
    }
    EXPECT_GT(with_value, 200) << "styles with a default value";
    EXPECT_TRUE(names.contains("spatium"));
    EXPECT_TRUE(names.contains("pageWidth"));
}

// The writer omits z when it equals the default, so a default that moves with the
// ElementType ordinals silently drops or changes saved stacking values. The defaults
// stay at the ordinals of ea2b00ee48, around and after the deleted LASSO slot (117).
TEST(Describe, DefaultStackingOrderIsPinned) {
    using mu::engraving::default_z;
    EXPECT_EQ(default_z(ElementType::INVALID), 0);
    EXPECT_EQ(default_z(ElementType::HOOK), 2800);
    EXPECT_EQ(default_z(ElementType::SELECTION), 11600);
    EXPECT_EQ(default_z(ElementType::TAB_DURATION_SYMBOL), 11900);
    EXPECT_EQ(default_z(ElementType::BAGPIPE_EMBELLISHMENT), 13300);
    EXPECT_EQ(default_z(ElementType::CHORD), 12700);
    EXPECT_EQ(default_z(ElementType::DUMMY), 15300);
}

TEST(Corpus, EveryPropertyOfEveryCorpusTypeReads) {
    ASSERT_FALSE(g_corpus_files.empty()) << "pass score files or directories on the command line";
    Corpus corpus;
    for (const auto &file : g_corpus_files) {
        ASSERT_NO_FATAL_FAILURE(corpus.load(file)) << file;
    }
    ASSERT_FALSE(corpus.instances.empty());

    std::vector<std::string> skipped;
    std::vector<std::string> without_value;
    int checked = 0;
    int with_corpus = 0;
    for (const auto &type : g_engine->describe()) {
        const auto instance = corpus.instances.find(element_type(type.type));
        if (instance == corpus.instances.end()) {
            skipped.push_back(type.type);
            continue;
        }
        ++with_corpus;
        for (const auto &prop : type.props) {
            const Pid pid = engrave::api::pid_of(prop.name);
            ASSERT_NE(pid, Pid::END) << type.type << " / " << prop.name;
            const PropertyValue value = instance->second->get_property(pid);
            if (!value.is_valid()) {
                without_value.push_back(type.type + " / " + prop.name);
                continue;
            }
            const Value mapped = engrave::api::to_value(mu::engraving::property_type(pid), value);
            EXPECT_FALSE(std::holds_alternative<std::monostate>(mapped)) << type.type << " / " << prop.name;
            if (const auto *token = std::get_if<Enum>(&mapped); token && !prop.enum_tokens.empty()) {
                EXPECT_TRUE(contains(prop.enum_tokens, token->token))
                    << type.type << " / " << prop.name << ": token '" << token->token << "' is not in the table";
            }
            ++checked;
        }
    }
    std::printf("corpus: %zu file(s), %zu unreadable, %zu type(s) with an instance, %d checked (type, pid) pair(s), "
                "%zu type(s) skipped, %zu pair(s) without a value\n",
                g_corpus_files.size(), corpus.unreadable.size(), corpus.instances.size(), checked, skipped.size(), without_value.size());
    for (const auto &file : corpus.unreadable) std::printf("  unreadable: %s\n", file.string().c_str());
    for (const auto &name : skipped) std::printf("  skipped type: %s\n", name.c_str());
    for (const auto &name : without_value) std::printf("  no value: %s\n", name.c_str());
    EXPECT_GT(checked, 1000);
    EXPECT_GT(with_corpus, 60);
    EXPECT_LT(without_value.size(), 10u) << "the table should not claim properties that answer nothing";
}

TEST(Generator, CheckedInTableIsCurrent) {
    if (g_python.empty()) GTEST_SKIP() << "pass --python=<interpreter> to run the generator check";
    const std::string command =
        std::format("\"{}\" \"{}\" --check --root \"{}\"", g_python.make_preferred().string(),
                    (std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tools" / "gen_pid_support.py").make_preferred().string(),
                    std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT).make_preferred().string());
#ifdef _WIN32
    // cmd.exe drops the first and the last quote of a command line that starts with
    // one; an extra pair keeps the inner quotes intact.
    const std::string quoted = std::format("\"{}\"", command);
#else
    const std::string quoted = command;
#endif
    EXPECT_EQ(std::system(quoted.c_str()), 0) << quoted;
}

namespace {

void collect(const std::filesystem::path &path) {
    static const std::set<std::string> score_extensions = {".mscx", ".mscz", ".musicxml", ".mxl", ".mid", ".midi",
                                                           ".gp",   ".gp3",  ".gp4",      ".gp5", ".gpx", ".ptb"};
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(path, ec))
            if (entry.is_regular_file(ec) && score_extensions.contains(entry.path().extension().string())) files.push_back(entry.path());
        std::ranges::sort(files);
        for (const auto &file : files) g_corpus_files.push_back(file);
        return;
    }
    g_corpus_files.push_back(path);
}

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg.starts_with("--assets="))
            g_assets = std::filesystem::path(arg.substr(9));
        else if (arg.starts_with("--python="))
            g_python = std::filesystem::path(arg.substr(9));
        else if (!arg.starts_with("--gtest"))
            collect(arg);
    }
    auto engine = engrave::Engine::create({.resources = g_assets});
    if (!engine) {
        std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", engine.error().message.c_str());
        return 1;
    }
    g_engine = std::move(*engine);
    const std::string anchor =
        R"(<score-partwise version="4.0"><part-list><score-part id="P1"><part-name>P</part-name></score-part></part-list>
<part id="P1"><measure number="1"><note><rest/><duration>4</duration></note></measure></part></score-partwise>)";
    auto document = g_engine->open(std::as_bytes(std::span(anchor.data(), anchor.size())));
    if (!document) {
        std::fprintf(stderr, "Engine::open: %s\n", document.error().message.c_str());
        return 1;
    }
    g_anchor = std::move(*document);
    const int result = RUN_ALL_TESTS();
    g_anchor.reset();
    g_engine.reset();
    return result;
}
