// Tests of the public engrave API.
//
//   engrave_api_test --assets=<resources> [gtest options]
//
#include <algorithm>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include <cairo/cairo.h>
#include <gtest/gtest.h>

#include "core_test_env.h"
#include "engrave/src/api/handles.h"
#include "engrave/src/api/sniff.h"
#include "engrave/src/render/cairo_painter.h"
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/runtime/runtime_assets.h"
#include "engrave/src/score/json.h"
#include "engrave/tests/harness/harness.h"
#include "engraving/dom/bend.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/volta.h"
#include "global/hashmap.h"
#include "global/settings.h"
#include <engrave/engrave.h>

namespace {

using engrave::ErrorCode;
using engrave::Format;

using F = engrave::Fraction;
constexpr auto max_fraction_term = std::numeric_limits<std::int64_t>::max();
constexpr auto min_fraction_term = std::numeric_limits<std::int64_t>::min();
static_assert(F{1, 2} > F{1, 4});
static_assert(F{1, 2} == F{2, 4});
static_assert(F{-1, 2} < F{-1, 4});
static_assert(F{min_fraction_term, 2} == F{min_fraction_term / 2, 1});
static_assert(F{min_fraction_term, max_fraction_term} < F{-1, 1});
static_assert(F{max_fraction_term - 1, max_fraction_term} > F{max_fraction_term - 2, max_fraction_term - 1});
static_assert(F{max_fraction_term - 1, max_fraction_term} < F{1, 1});

auto &g_resources = engrave::test::core_env().assets;
auto &g_engine = engrave::test::core_env().engine;

std::vector<std::byte> bytes_of(std::string_view text) {
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    return {data, data + text.size()};
}

std::string text_of(const std::vector<std::byte> &bytes) { return {reinterpret_cast<const char *>(bytes.data()), bytes.size()}; }

std::string read_file(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

const std::filesystem::path &gp_data() {
    static const std::filesystem::path dir = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/importexport/guitarpro/tests/data";
    return dir;
}

std::unique_ptr<engrave::Document> treble_clef(const std::string &title = {}) {
    auto document = g_engine->create_document({.template_id = "Treble_Clef", .title = title});
    EXPECT_TRUE(document) << document.error().message;
    return document ? std::move(*document) : nullptr;
}

std::string exported(engrave::Document &document, Format format) {
    auto bytes = document.export_score({.format = format});
    EXPECT_TRUE(bytes) << bytes.error().message;
    return bytes ? text_of(*bytes) : std::string{};
}

struct ExportedMidiNote {
    int pitch;
    int on;
    int off = -1;
};

std::uint32_t midi_varint(const std::string &data, size_t &position) {
    std::uint32_t value = 0;
    unsigned char byte = 0;
    do {
        byte = static_cast<unsigned char>(data[position++]);
        value = (value << 7) | (byte & 0x7f);
    } while (byte & 0x80);
    return value;
}

std::string midi_grace_chord(std::string_view type, std::string_view duration, int pitch) {
    return "<Chord><durationType>" + std::string(duration) + "</durationType><" + std::string(type) + "/><Note><pitch>" +
           std::to_string(pitch) + "</pitch></Note></Chord>";
}

struct PrincipalExtras {
    std::string_view tremolo; // e.g. "r8": a single-chord tremolo
    bool tie_next = false;    // ties every note into the next chord
    bool tie_prev = false;    // the far end of the tie from the previous chord
};

std::string midi_principal_chord(std::string_view duration, const std::vector<int> &pitches, int dots = 0, bool arpeggio = false,
                                 PrincipalExtras extras = {}) {
    // Version 5.00 ties are Tie elements inside the first note that name both notes by element id
    constexpr std::string_view first_note = "AAAAAAAAAAA_AAAAAAAAAAA";
    constexpr std::string_view second_note = "BBBBBBBBBBB_BBBBBBBBBBB";
    // <dots> must precede <durationType>: the reader fixes the chord's length when it reads durationType
    std::string chord = "<Chord>";
    if (dots) chord += "<dots>" + std::to_string(dots) + "</dots>";
    chord += "<durationType>" + std::string(duration) + "</durationType>";
    for (int pitch : pitches) {
        chord += "<Note>";
        if (extras.tie_next) {
            chord += "<eid>" + std::string(first_note) + "</eid><Tie><startElement>" + std::string(first_note) +
                     "</startElement><endElement>" + std::string(second_note) +
                     "</endElement><eid>CCCCCCCCCCC_CCCCCCCCCCC</eid><track>0</track></Tie>";
        }
        if (extras.tie_prev) chord += "<eid>" + std::string(second_note) + "</eid>";
        chord += "<pitch>" + std::to_string(pitch) + "</pitch></Note>";
    }
    if (arpeggio) chord += "<Arpeggio><subtype>0</subtype></Arpeggio>";
    if (!extras.tremolo.empty()) chord += "<TremoloSingleChord><subtype>" + std::string(extras.tremolo) + "</subtype></TremoloSingleChord>";
    return chord + "</Chord>";
}

std::string midi_glissando_start_chord(int pitch) {
    return "<Chord><durationType>quarter</durationType><Note><eid>AAAAAAAAAAA_AAAAAAAAAAA</eid><pitch>" + std::to_string(pitch) +
           "</pitch></Note></Chord>";
}

std::string midi_glissando_end_chord(int pitch) {
    return "<Chord><durationType>quarter</durationType><Note><eid>BBBBBBBBBBB_BBBBBBBBBBB</eid><Glissando><startElement>"
           "AAAAAAAAAAA_AAAAAAAAAAA</startElement><endElement>BBBBBBBBBBB_BBBBBBBBBBB</endElement><eid>CCCCCCCCCCC_CCCCCCCCCCC"
           "</eid><track>0</track><diagonal>1</diagonal></Glissando><pitch>" +
           std::to_string(pitch) + "</pitch></Note></Chord>";
}

struct MidiScoreOptions {
    bool swing = false;
    bool drumset = false;
    int beats = 4;
    int beat_unit = 4;
};

std::vector<ExportedMidiNote> exported_midi_notes(const std::string &chords, MidiScoreOptions options = {}) {
    const bool swing = options.swing;
    const bool drumset = options.drumset;
    std::string xml = "<museScore version=\"5.00\"><Score><Division>480</Division><Part><Staff><StaffType group=\"";
    xml += drumset ? "percussion" : "pitched";
    xml += "\"/></Staff>";
    if (drumset) {
        xml += "<Instrument id=\"drums\"><useDrumset>1</useDrumset>";
        for (int pitch : {36, 38, 40}) {
            xml += "<Drum pitch=\"" + std::to_string(pitch) +
                   "\"><head>normal</head><line>0</line><voice>0</voice><name>Drum</name><stem>1</stem></Drum>";
        }
        xml += "<Channel><program value=\"0\"/></Channel></Instrument>";
    } else {
        xml += "<Instrument id=\"piano\"><Channel><program value=\"0\"/></Channel></Instrument>";
    }
    xml += "</Part><Staff id=\"1\"><Measure><voice><TimeSig><sigN>" + std::to_string(options.beats) + "</sigN><sigD>" +
           std::to_string(options.beat_unit) + "</sigD></TimeSig>";
    if (swing) xml += "<SystemText><swing unit=\"eighth\" ratio=\"75\"/><style>tempo</style><text>Swing</text></SystemText>";
    xml += chords + "</voice></Measure></Staff></Score></museScore>";
    auto document = g_engine->open(bytes_of(xml), {.format = Format::MSCX});
    EXPECT_TRUE(document) << document.error().message;
    if (!document) return {};
    const std::string midi = exported(**document, Format::MIDI);

    std::vector<ExportedMidiNote> notes;
    const auto byte_at = [&midi](size_t position) { return static_cast<unsigned char>(midi[position]); };
    const int track_count = (byte_at(10) << 8) | byte_at(11);
    size_t track_position = 14;
    for (int track = 0; track < track_count; ++track) {
        const std::uint32_t length = (std::uint32_t(byte_at(track_position + 4)) << 24) |
                                     (std::uint32_t(byte_at(track_position + 5)) << 16) |
                                     (std::uint32_t(byte_at(track_position + 6)) << 8) | byte_at(track_position + 7);
        size_t position = track_position + 8;
        const size_t end = position + length;
        track_position = end;
        int tick = 0;
        int running_status = 0;
        muse::HashMap<int, size_t> active;
        while (position < end) {
            tick += static_cast<int>(midi_varint(midi, position));
            const int lead = byte_at(position);
            if (lead == 0xff) {
                position += 2;
                const auto size = midi_varint(midi, position);
                position += size;
                running_status = 0;
                continue;
            }
            if (lead == 0xf0 || lead == 0xf7) {
                ++position;
                const auto size = midi_varint(midi, position);
                position += size;
                running_status = 0;
                continue;
            }
            if (lead & 0x80) {
                running_status = lead;
                ++position;
            }
            const int command = running_status & 0xf0;
            if (command == 0xc0 || command == 0xd0) {
                ++position;
                continue;
            }
            const int pitch = byte_at(position++);
            const int velocity = byte_at(position++);
            if (command == 0x90 && velocity > 0) {
                active[pitch] = notes.size();
                notes.push_back({pitch, tick});
            } else if (command == 0x80 || (command == 0x90 && velocity == 0)) {
                const auto found = active.find(pitch);
                if (found != active.end()) {
                    notes[found->second].off = tick;
                    active.erase(found);
                }
            }
        }
    }
    std::sort(notes.begin(), notes.end(),
              [](const auto &left, const auto &right) { return std::tie(left.on, left.pitch) < std::tie(right.on, right.pitch); });
    return notes;
}

TEST(Values, FractionsCompareByValueAndKeepWrittenTerms) {
    std::vector<F> fractions{{1, 2}, {1, 4}, {-1, 2}, {2, 4}, {max_fraction_term, 1}, {min_fraction_term, 1}, {0, max_fraction_term}};
    std::stable_sort(fractions.begin(), fractions.end());
    const std::vector<F> expected{{min_fraction_term, 1}, {-1, 2}, {0, 1}, {1, 4}, {1, 2}, {2, 4}, {max_fraction_term, 1}};
    EXPECT_EQ(fractions, expected);
    EXPECT_EQ(fractions[4].num, 1);
    EXPECT_EQ(fractions[4].den, 2);
    EXPECT_EQ(fractions[5].num, 2);
    EXPECT_EQ(fractions[5].den, 4);

    const F written{6, 8};
    EXPECT_EQ(written, (F{3, 4}));
    EXPECT_EQ(written.num, 6);
    EXPECT_EQ(written.den, 8);
    // Invalid denominators are outside the API contract but remain safe to sort.
    EXPECT_LT((F{1, 0}), (F{0, 1}));
    EXPECT_EQ((F{1, 0}), (F{1, 0}));
}

TEST(Open, RejectsPreMuseScore4) {
    for (const std::string version : {"1.14", "2.07", "3.00", "3.02", "4.00", "4.10", "4.40", "4.50"}) {
        const auto xml = bytes_of("<museScore version=\"" + version + "\"><Score/></museScore>");
        auto result = g_engine->open(xml, {.format = Format::MSCX});
        ASSERT_FALSE(result) << version;
        EXPECT_EQ(result.error().code, ErrorCode::UNSUPPORTED);
        EXPECT_NE(result.error().message.find("written by MuseScore " + version + "; scores from before MuseScore 4.6 are not supported"),
                  std::string::npos);
    }
}

// The writer stores z only when it differs from the type's default stacking order,
// so the default must not move with ElementType's ordinals: a Chord's is 12700. A
// custom 12600 (the default the deletion of LASSO would have produced) survives two
// saves, and an explicit 12700 is dropped as the default.
TEST(Open, StackingOrderSurvivesSaveAndReopen) {
    const auto chord_score = [](int z) {
        return "<museScore version=\"5.00\"><Score><Division>480</Division><Part><Staff><StaffType group=\"pitched\"/></Staff>"
               "<Instrument id=\"piano\"><Channel><program value=\"0\"/></Channel></Instrument></Part><Staff id=\"1\"><Measure>"
               "<voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig><Chord><z>" +
               std::to_string(z) +
               "</z><durationType>whole</durationType><Note><pitch>60</pitch></Note></Chord></voice></Measure></Staff></Score>"
               "</museScore>";
    };
    const auto saved_twice = [&](int z) {
        auto first = g_engine->open(bytes_of(chord_score(z)), {.format = Format::MSCX});
        EXPECT_TRUE(first) << first.error().message;
        if (!first) return std::string{};
        const std::string once = exported(**first, Format::MSCX);
        auto second = g_engine->open(bytes_of(once), {.format = Format::MSCX});
        EXPECT_TRUE(second) << second.error().message;
        return second ? exported(**second, Format::MSCX) : std::string{};
    };
    EXPECT_NE(saved_twice(12600).find("<z>12600</z>"), std::string::npos);
    const std::string by_default = saved_twice(12700);
    EXPECT_FALSE(by_default.empty());
    EXPECT_EQ(by_default.find("<z>"), std::string::npos);
}

TEST(Capabilities, ListsFormatsAndVersion) {
    const auto capabilities = g_engine->capabilities();
    EXPECT_FALSE(capabilities.engine_version.empty());
    EXPECT_EQ(capabilities.import_formats.size(), 7u);
    EXPECT_EQ(capabilities.export_formats, (std::vector<Format>{Format::MSCX, Format::MSCZ, Format::MUSIC_XML, Format::MXL, Format::MIDI}));
}

TEST(Templates, ListsCategoriesJsonInOrder) {
    const auto templates = g_engine->templates();
    ASSERT_GE(templates.size(), 40u);
    EXPECT_EQ(templates.front().id, "Treble_Clef");
    EXPECT_EQ(templates.front().title, "Treble Clef");
    EXPECT_EQ(templates.front().category, "General");
    muse::HashSet<std::string> ids;
    for (const auto &info : templates) {
        EXPECT_TRUE(ids.insert(info.id).second) << "duplicate template id " << info.id;
        EXPECT_TRUE(std::filesystem::is_regular_file(info.path)) << info.path;
        EXPECT_FALSE(info.category.empty());
    }
    EXPECT_TRUE(ids.contains("SATB_+_Organ"));
}

TEST(Create, AppliesMetadataAtRevisionOne) {
    auto document = treble_clef("An engrave title");
    ASSERT_TRUE(document);
    EXPECT_EQ(document->revision(), 1u);
    EXPECT_TRUE(document->diagnostics().empty());
    const std::string musicxml = exported(*document, Format::MUSIC_XML);
    EXPECT_NE(musicxml.find("An engrave title"), std::string::npos);
    EXPECT_NE(musicxml.find(">Piano</part-name>"), std::string::npos);
}

TEST(Create, FromTemplatePath) {
    const auto templates = g_engine->templates();
    ASSERT_FALSE(templates.empty());
    auto document = g_engine->create_document({.template_path = templates.back().path});
    ASSERT_TRUE(document) << document.error().message;
    EXPECT_EQ((*document)->revision(), 1u);
}

TEST(Create, RejectsBadOptions) {
    auto neither = g_engine->create_document({});
    ASSERT_FALSE(neither);
    EXPECT_EQ(neither.error().code, ErrorCode::INVALID_ARGUMENT);

    const auto path = g_engine->templates().front().path;
    auto both = g_engine->create_document({.template_id = "Treble_Clef", .template_path = path});
    ASSERT_FALSE(both);
    EXPECT_EQ(both.error().code, ErrorCode::INVALID_ARGUMENT);

    auto unknown = g_engine->create_document({.template_id = "No_Such_Template"});
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code, ErrorCode::NOT_FOUND);

    auto missing = g_engine->create_document({.template_path = path.parent_path() / "missing.mscx"});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::NOT_FOUND);

    auto style = g_engine->create_document({.template_path = path.parent_path() / "score_style.mss"});
    ASSERT_FALSE(style);
    EXPECT_EQ(style.error().code, ErrorCode::INVALID_ARGUMENT);
}

TEST(Export, WritesEachExportFormat) {
    auto document = treble_clef();
    ASSERT_TRUE(document);
    EXPECT_NE(exported(*document, Format::MSCX).find("<museScore"), std::string::npos);
    EXPECT_TRUE(exported(*document, Format::MSCZ).starts_with("PK\x03\x04"));
    EXPECT_TRUE(exported(*document, Format::MXL).starts_with("PK\x03\x04"));
    EXPECT_TRUE(exported(*document, Format::MIDI).starts_with("MThd"));
    EXPECT_NE(exported(*document, Format::MUSIC_XML).find("<score-partwise"), std::string::npos);
    EXPECT_EQ(document->revision(), 1u);
}

TEST(Export, MidiRendersGraceGroupsInChronologicalOrder) {
    const auto grace_before = midi_grace_chord("appoggiatura", "eighth", 67);
    const auto grace_after = midi_grace_chord("grace8after", "eighth", 69);
    const auto principal = midi_principal_chord("quarter", {65});

    const auto both = exported_midi_notes(grace_before + grace_after + principal);
    ASSERT_EQ(both.size(), 3u);
    EXPECT_EQ(both[0].pitch, 67);
    EXPECT_EQ(both[1].pitch, 65);
    EXPECT_EQ(both[2].pitch, 69);
    for (const auto &note : both) {
        EXPECT_GT(note.off, note.on);
    }
    EXPECT_LT(both[0].on, both[1].on);
    EXPECT_LT(both[1].on, both[2].on);
    EXPECT_LE(both[2].off, 480);

    // Acciaccatura before a principal quarter note sounds with positive duration before the principal.
    const auto acciaccatura = exported_midi_notes(midi_grace_chord("acciaccatura", "eighth", 67) + principal);
    ASSERT_EQ(acciaccatura.size(), 2u);
    EXPECT_EQ(acciaccatura[0].pitch, 67);
    EXPECT_EQ(acciaccatura[1].pitch, 65);
    EXPECT_GT(acciaccatura[0].off, acciaccatura[0].on);
    EXPECT_GT(acciaccatura[1].off, acciaccatura[1].on);

    // Multi-note grace groups stay in order with positive durations.
    std::string before_group, after_group;
    for (int pitch : {67, 68, 70}) before_group += midi_grace_chord("grace16", "16th", pitch);
    for (int pitch : {71, 72, 74}) after_group += midi_grace_chord("grace16after", "16th", pitch);
    const auto grouped = exported_midi_notes(before_group + after_group + principal);
    ASSERT_EQ(grouped.size(), 7u);
    const std::vector<int> expected_pitches{67, 68, 70, 65, 74, 72, 71};
    for (size_t i = 0; i < grouped.size(); ++i) {
        EXPECT_EQ(grouped[i].pitch, expected_pitches[i]);
        EXPECT_GT(grouped[i].off, grouped[i].on);
        if (i > 0) EXPECT_GE(grouped[i].on, grouped[i - 1].on);
    }
}

TEST(Export, MidiRendersArpeggioSwingTremoloGlissandoTiesAndDrums) {
    // Arpeggio staggers chord pitches in ascending onset order.
    const auto arpeggio = exported_midi_notes(midi_principal_chord("quarter", {60, 64, 67}, 0, true));
    ASSERT_EQ(arpeggio.size(), 3u);
    EXPECT_EQ(arpeggio[0].pitch, 60);
    EXPECT_EQ(arpeggio[1].pitch, 64);
    EXPECT_EQ(arpeggio[2].pitch, 67);
    EXPECT_LT(arpeggio[0].on, arpeggio[1].on);
    EXPECT_LT(arpeggio[1].on, arpeggio[2].on);

    // Swing delays the off-beat eighth past tick 240.
    const auto first = midi_principal_chord("eighth", {60});
    const auto second = midi_principal_chord("eighth", {65});
    const auto swung = exported_midi_notes(first + second, {.swing = true});
    ASSERT_EQ(swung.size(), 2u);
    EXPECT_EQ(swung[0].pitch, 60);
    EXPECT_EQ(swung[1].pitch, 65);
    EXPECT_EQ(swung[0].on, 0);
    EXPECT_GT(swung[1].on, 240);

    // Tremolo expands a quarter note into multiple strikes of its pitch.
    const PrincipalExtras tremolo{.tremolo = "r8"};
    const auto trem = exported_midi_notes(midi_principal_chord("quarter", {65}, 0, false, tremolo));
    ASSERT_GE(trem.size(), 2u);
    for (const auto &note : trem) {
        EXPECT_EQ(note.pitch, 65);
        EXPECT_GT(note.off, note.on);
    }

    // Glissando emits intermediate pitches between start (60) and end (72).
    const auto gliss = exported_midi_notes(midi_glissando_start_chord(60) + midi_glissando_end_chord(72));
    ASSERT_GT(gliss.size(), 2u);
    EXPECT_EQ(gliss.front().pitch, 60);
    EXPECT_EQ(gliss.back().pitch, 72);

    // Tied quarters merge into one note lasting longer than a single quarter (480 ticks).
    const PrincipalExtras start{.tie_next = true};
    const PrincipalExtras end{.tie_prev = true};
    const auto tied =
        exported_midi_notes(midi_principal_chord("quarter", {60}, 0, false, start) + midi_principal_chord("quarter", {60}, 0, false, end));
    ASSERT_EQ(tied.size(), 1u);
    EXPECT_EQ(tied[0].pitch, 60);
    EXPECT_EQ(tied[0].on, 0);
    EXPECT_GT(tied[0].off, 480);

    // Drumset with before and after graces preserves all 3 drum pitches with positive durations.
    const auto drums = exported_midi_notes(midi_grace_chord("appoggiatura", "eighth", 36) + midi_grace_chord("grace8after", "eighth", 40) +
                                               midi_principal_chord("quarter", {38}),
                                           {.drumset = true});
    ASSERT_EQ(drums.size(), 3u);
    EXPECT_EQ(drums[0].pitch, 36);
    EXPECT_EQ(drums[1].pitch, 38);
    EXPECT_EQ(drums[2].pitch, 40);
    for (const auto &note : drums) EXPECT_GT(note.off, note.on);
}

TEST(Export, RejectsFormatsTheEngineDoesNotWrite) {
    auto document = treble_clef();
    ASSERT_TRUE(document);
    for (const auto format : {Format::GUITAR_PRO, Format::MNX}) {
        const auto bytes = document->export_score({.format = format});
        ASSERT_FALSE(bytes);
        EXPECT_EQ(bytes.error().code, ErrorCode::UNSUPPORTED);
    }
}

TEST(Sniff, DetectsEveryImportFormat) {
    auto document = treble_clef();
    ASSERT_TRUE(document);
    EXPECT_EQ(engrave::sniff_format(exported(*document, Format::MSCZ)), Format::MSCZ);
    EXPECT_EQ(engrave::sniff_format(exported(*document, Format::MXL)), Format::MXL);
    EXPECT_EQ(engrave::sniff_format(exported(*document, Format::MUSIC_XML)), Format::MUSIC_XML);
    EXPECT_EQ(engrave::sniff_format(exported(*document, Format::MIDI)), Format::MIDI);
    EXPECT_EQ(engrave::sniff_format(read_file(g_engine->templates().front().path)), Format::MSCX);
    for (const char *name : {"bend.gp3", "bend.gp4", "heavy-accent.gp5", "sforzato.gpx", "sforzato.gp", "ptb-baseline.ptb"})
        EXPECT_EQ(engrave::sniff_format(read_file(gp_data() / name)), Format::GUITAR_PRO) << name;
}

TEST(Sniff, LooksPastByteOrderMarkAndProlog) {
    EXPECT_EQ(engrave::sniff_format("\xEF\xBB\xBF\n<?xml version=\"1.0\"?>\n<!-- c -->\n<museScore version=\"5.00\"/>"), Format::MSCX);
    EXPECT_EQ(engrave::sniff_format("<?xml version=\"1.0\"?>\n<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 "
                                    "Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">\n<score-timewise/>"),
              Format::MUSIC_XML);
}

TEST(Sniff, RejectsUnknownContent) {
    EXPECT_EQ(engrave::sniff_format(""), std::nullopt);
    EXPECT_EQ(engrave::sniff_format("hello"), std::nullopt);
    EXPECT_EQ(engrave::sniff_format("<html><body/></html>"), std::nullopt);
    EXPECT_EQ(engrave::sniff_format("<museScore"), std::nullopt) << "malformed XML";
    EXPECT_EQ(engrave::sniff_format(std::string("PK\x03\x04", 4) + "not a zip"), std::nullopt);
    EXPECT_EQ(engrave::sniff_format("\x10"
                                    "FICHIER GUITAR"),
              Format::GUITAR_PRO)
        << "the magic alone decides";
}

TEST(Sniff, MapsExtensions) {
    EXPECT_EQ(engrave::format_from_extension("a.MSCZ"), Format::MSCZ);
    EXPECT_EQ(engrave::format_from_extension("a.xml"), Format::MUSIC_XML);
    EXPECT_EQ(engrave::format_from_extension("a.kar"), Format::MIDI);
    EXPECT_EQ(engrave::format_from_extension("a.gtp"), Format::GUITAR_PRO);
    EXPECT_EQ(engrave::format_from_extension("a.pdf"), std::nullopt);
    EXPECT_EQ(engrave::format_from_extension("mscz"), std::nullopt);
}

TEST(Open, ReadsEveryImportFormatAtRevisionOne) {
    auto source = treble_clef("Round trip");
    ASSERT_TRUE(source);
    for (const auto format : {Format::MSCZ, Format::MXL, Format::MUSIC_XML, Format::MIDI}) {
        auto document = g_engine->open(bytes_of(exported(*source, format)));
        ASSERT_TRUE(document) << to_string(format) << ": " << document.error().message;
        EXPECT_EQ((*document)->revision(), 1u);
        EXPECT_TRUE((*document)->diagnostics().empty()) << to_string(format);
    }
    auto mscx = g_engine->open(g_engine->templates().front().path);
    ASSERT_TRUE(mscx) << mscx.error().message;
    for (const char *name : {"bend.gp3", "heavy-accent.gp5", "sforzato.gpx", "sforzato.gp", "ptb-baseline.ptb"}) {
        auto document = g_engine->open(gp_data() / name);
        EXPECT_TRUE(document) << name << ": " << document.error().message;
    }
}

TEST(Open, ExplicitFormatSkipsSniffing) {
    auto source = treble_clef();
    ASSERT_TRUE(source);
    const auto mscz = bytes_of(exported(*source, Format::MSCZ));
    auto document = g_engine->open(mscz, {.format = Format::MIDI});
    ASSERT_FALSE(document);
    EXPECT_EQ(document.error().code, ErrorCode::INVALID_INPUT);
}

TEST(Open, FallsBackToTheExtension) {
    // Content the sniffer does not recognise is read as the extension's format.
    auto document = g_engine->open(bytes_of("not xml"), {.filename_hint = "score.musicxml"});
    ASSERT_FALSE(document);
    EXPECT_EQ(document.error().code, ErrorCode::INVALID_INPUT);
    EXPECT_NE(document.error().message.find("MusicXML"), std::string::npos) << document.error().message;
}

TEST(Open, MusicXmlPartGroupsOnlyApplyWhenClosed) {
    const std::string part_list = R"(<score-partwise version="4.0"><part-list>
<part-group number="1" type="start"><group-symbol>bracket</group-symbol></part-group>
<score-part id="P1"><part-name>Flute</part-name></score-part>
<score-part id="P2"><part-name>Oboe</part-name></score-part>)";
    constexpr std::string_view body = R"(</part-list>
<part id="P1"><measure number="1"><note><rest/><duration>4</duration></note></measure></part>
<part id="P2"><measure number="1"><note><rest/><duration>4</duration></note></measure></part>
</score-partwise>)";
    for (bool closed : {true, false}) {
        SCOPED_TRACE(closed);
        std::string xml = part_list;
        if (closed) xml += R"(<part-group number="1" type="stop"/>)";
        xml += body;
        auto document = g_engine->open(bytes_of(xml), {.format = Format::MUSIC_XML});
        ASSERT_TRUE(document) << document.error().message;
        const auto result = exported(**document, Format::MUSIC_XML);
        EXPECT_EQ(result.find("<group-symbol>bracket</group-symbol>") != std::string::npos, closed);
    }
}

TEST(Open, ReportsCallerErrors) {
    const auto expect_error = [](const engrave::Result<std::unique_ptr<engrave::Document>> &result, ErrorCode code) {
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, code) << result.error().message;
        EXPECT_FALSE(result.error().message.empty());
    };
    expect_error(g_engine->open(std::span<const std::byte>{}), ErrorCode::INVALID_INPUT);
    expect_error(g_engine->open(bytes_of("hello")), ErrorCode::UNSUPPORTED_FORMAT);
    expect_error(g_engine->open(bytes_of("hello"), {.filename_hint = "notes.txt"}), ErrorCode::UNSUPPORTED_FORMAT);
    expect_error(g_engine->open(std::vector<std::byte>(engrave::max_input_bytes + 1)), ErrorCode::RESOURCE_LIMIT);
    // Malformed XML has no root to sniff; only a file name makes it MusicXML.
    expect_error(g_engine->open(bytes_of("<score-partwise><part")), ErrorCode::UNSUPPORTED_FORMAT);
    expect_error(g_engine->open(bytes_of("<score-partwise><part"), {.filename_hint = "a.xml"}), ErrorCode::INVALID_INPUT);
    expect_error(g_engine->open(bytes_of("MThd")), ErrorCode::INVALID_INPUT);
    expect_error(g_engine->open(g_resources / "no-such-score.mscz"), ErrorCode::NOT_FOUND);
    expect_error(g_engine->open(g_resources), ErrorCode::INVALID_ARGUMENT);
}

TEST(Open, LargerUncompressedXmlIsOnlyAcceptedFromFile) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto base = std::filesystem::temp_directory_path() / ("engrave-input-ceiling-" + std::to_string(stamp));

    struct Files {
        std::filesystem::path xml;
        std::filesystem::path archive;

        ~Files() {
            std::error_code error;
            std::filesystem::remove(xml, error);
            std::filesystem::remove(archive, error);
        }
    } files{base.string() + ".mscx", base.string() + ".mscz"};

    auto original = treble_clef("Large native score");
    ASSERT_TRUE(original);
    auto xml = exported(*original, Format::MSCX);
    ASSERT_FALSE(xml.empty());
    xml.resize(engrave::max_input_bytes + 1, ' ');
    ASSERT_TRUE(std::ofstream(files.xml, std::ios::binary).write(xml.data(), xml.size()));

    const auto opened = g_engine->open(files.xml);
    ASSERT_TRUE(opened) << opened.error().message;
    EXPECT_EQ((*opened)->outline()->measures.size(), original->outline()->measures.size());
    const auto bytes = g_engine->open(std::as_bytes(std::span(xml)));
    ASSERT_FALSE(bytes);
    EXPECT_EQ(bytes.error().code, ErrorCode::RESOURCE_LIMIT);

    ASSERT_TRUE(std::ofstream(files.archive, std::ios::binary) << exported(*original, Format::MSCZ));
    std::filesystem::resize_file(files.archive, engrave::max_input_bytes + 1);
    const auto archive = g_engine->open(files.archive);
    ASSERT_FALSE(archive);
    EXPECT_EQ(archive.error().code, ErrorCode::RESOURCE_LIMIT);
    std::filesystem::remove(files.xml);
    std::filesystem::rename(files.archive, files.xml);
    const auto disguised_archive = g_engine->open(files.xml);
    ASSERT_FALSE(disguised_archive);
    EXPECT_EQ(disguised_archive.error().code, ErrorCode::RESOURCE_LIMIT);

    std::filesystem::resize_file(files.xml, engrave::max_uncompressed_file_bytes + 1);
    const auto oversized = g_engine->open(files.xml);
    ASSERT_FALSE(oversized);
    EXPECT_EQ(oversized.error().code, ErrorCode::RESOURCE_LIMIT);
    EXPECT_NE(oversized.error().message.find(std::to_string(engrave::max_uncompressed_file_bytes)), std::string::npos);
}

TEST(Open, LargerUncompressedMusicXmlUsesTheExplicitFormatOrFilenameHint) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("engrave-musicxml-" + std::to_string(stamp) + ".musicxml");

    struct Cleanup {
        const std::filesystem::path &path;

        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } cleanup{path};

    auto original = treble_clef("Large MusicXML score");
    ASSERT_TRUE(original);
    auto xml = exported(*original, Format::MUSIC_XML);
    ASSERT_FALSE(xml.empty());
    xml.resize(engrave::max_input_bytes + 1, ' ');
    ASSERT_TRUE(std::ofstream(path, std::ios::binary).write(xml.data(), xml.size()));
    const auto check = [&](const engrave::OpenOptions &options) {
        const auto opened = g_engine->open(path, options);
        ASSERT_TRUE(opened) << opened.error().message;
        EXPECT_EQ((*opened)->outline()->measures.size(), original->outline()->measures.size());
    };
    check({});
    auto temporary = path;
    temporary.replace_extension(".tmp");
    std::filesystem::rename(path, temporary);
    path = temporary;
    check({.format = Format::MUSIC_XML, .filename_hint = "ignore.mxl"});
    check({.filename_hint = "score.xml"});
    for (const auto &options : {engrave::OpenOptions{}, engrave::OpenOptions{.format = Format::MXL, .filename_hint = "score.xml"}}) {
        const auto rejected = g_engine->open(path, options);
        ASSERT_FALSE(rejected);
        EXPECT_EQ(rejected.error().code, ErrorCode::RESOURCE_LIMIT);
    }
}

TEST(Open, LargerUncompressedNativeXmlSupportsBothUtf16ByteOrders) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() / ("engrave-utf16-" + std::to_string(stamp) + ".mscx");

    struct Cleanup {
        const std::filesystem::path &path;

        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } cleanup{path};

    auto original = treble_clef();
    ASSERT_TRUE(original);
    auto xml = exported(*original, Format::MSCX);
    const auto declaration_end = xml.find("?>");
    ASSERT_NE(declaration_end, std::string::npos);
    xml.replace(0, declaration_end + 2, " \t\r\n");
    auto text = muse::String::from_utf8(xml).to_std_u16_string();
    text.resize(engrave::max_input_bytes / 2 + 1, u' ');
    for (const bool little_endian : {true, false}) {
        SCOPED_TRACE(little_endian);
        std::string encoded(2 + 2 * text.size(), '\0');
        encoded.replace(0, 2, little_endian ? "\xFF\xFE" : "\xFE\xFF");
        for (std::size_t i = 0; i < text.size(); ++i) {
            encoded[2 + 2 * i + (little_endian ? 0 : 1)] = static_cast<char>(text[i] & 0xff);
            encoded[2 + 2 * i + (little_endian ? 1 : 0)] = static_cast<char>(text[i] >> 8);
        }
        ASSERT_TRUE(std::ofstream(path, std::ios::binary).write(encoded.data(), encoded.size()));
        const auto opened = g_engine->open(path);
        ASSERT_TRUE(opened) << opened.error().message;
        EXPECT_EQ((*opened)->outline()->measures.size(), original->outline()->measures.size());
    }
}

TEST(Contract, ActiveEngineRejectsAnotherCreateAfterValidatingOptions) {
    const auto invalid = engrave::Engine::create({.resources = g_resources, .audio_sample_rate = 100});
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, ErrorCode::INVALID_ARGUMENT);
    const auto active = engrave::Engine::create({.resources = g_resources});
    ASSERT_FALSE(active);
    EXPECT_EQ(active.error().code, ErrorCode::UNSUPPORTED);
}

TEST(Contract, CallFromAnotherThreadAborts) {
    EXPECT_DEATH(std::thread([] { std::ignore = g_engine->templates(); }).join(), "thread other than");
}

struct EngineLifecycle : ::testing::Test {
    void SetUp() override { engrave::test::core_env().set_engine(nullptr); }

    void TearDown() override {
        auto &env = engrave::test::core_env();
        auto created = engrave::Engine::create({.resources = env.assets, .deterministic_ids = env.deterministic});
        ASSERT_TRUE(created) << created.error().message;
        env.set_engine(std::move(*created));
    }
};

using EngineLifecycleDeathTest = EngineLifecycle;
constinit std::unique_ptr<engrave::Engine> engine_until_exit;
constinit std::unique_ptr<engrave::Document> document_until_exit;

TEST_F(EngineLifecycleDeathTest, GlobalEngineAndDocumentCanBeDestroyedAtProcessExit) {
    EXPECT_EXIT(
        {
            auto engine = engrave::Engine::create({.resources = g_resources});
            if (!engine) std::exit(EXIT_FAILURE);
            auto document = (*engine)->create_document({.template_id = "Treble_Clef"});
            if (!document) std::exit(EXIT_FAILURE);
            document_until_exit = std::move(*document);
            engine_until_exit = std::move(*engine);
            muse::settings()->begin_transaction();
            std::exit(EXIT_SUCCESS);
        },
        ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST_F(EngineLifecycle, ConcurrentStartsHaveOneOwner) {
    const engrave::EngineOptions options{.resources = g_resources};
    std::barrier rendezvous(2);
    std::optional<ErrorCode> worker_error;
    std::jthread worker([&] {
        rendezvous.arrive_and_wait();
        auto result = engrave::Engine::create(options);
        if (!result) worker_error = result.error().code;
        rendezvous.arrive_and_wait();
    });
    rendezvous.arrive_and_wait();
    const auto result = engrave::Engine::create(options);
    rendezvous.arrive_and_wait();
    worker.join();

    EXPECT_EQ(result.has_value(), worker_error.has_value());
    if (worker_error) EXPECT_EQ(*worker_error, ErrorCode::UNSUPPORTED);
    if (!result) EXPECT_EQ(result.error().code, ErrorCode::UNSUPPORTED);
}

TEST_F(EngineLifecycle, FontFailureReleasesInitializedRuntime) {
    const auto bad_font = g_resources / "engraving/instruments/instruments.xml";
    const auto failed = engrave::Engine::create({.resources = g_resources, .fonts = {bad_font}, .deterministic_ids = true});
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code, ErrorCode::INVALID_INPUT);
    EXPECT_FALSE(mu::engraving::MScore::test_mode);

    const auto retried = engrave::Engine::create({.resources = g_resources});
    ASSERT_TRUE(retried) << retried.error().message;
    EXPECT_TRUE((*retried)->runtime_fonts().empty());
}

TEST_F(EngineLifecycle, CairoFontsRefreshAcrossResourceDirectories) {
    namespace fs = std::filesystem;
    const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto alternate = fs::temp_directory_path() / ("engrave-cairo-restart-" + std::to_string(id));
    ASSERT_TRUE(fs::create_directory(alternate));

    struct Cleanup {
        const fs::path &path;

        ~Cleanup() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } cleanup{alternate};

    fs::copy(g_resources, alternate, fs::copy_options::recursive);

    const fs::path roman = "fonts/edwin/Edwin-Roman.otf";
    fs::copy_file(g_resources / "fonts/FreeSerif.ttf", alternate / roman, fs::copy_options::overwrite_existing);
    const auto render = [](const engrave::EngineOptions &options, std::string_view postscript_name) {
        auto engine = engrave::Engine::create(options);
        ASSERT_TRUE(engine) << engine.error().message;
        auto document = (*engine)->create_document({.template_id = "Treble_Clef", .title = "Cairo font restart"});
        ASSERT_TRUE(document) << document.error().message;
        const auto pdf = (*document)->render({.format = engrave::RenderFormat::PDF});
        ASSERT_TRUE(pdf) << pdf.error().message;
        EXPECT_NE(text_of(pdf->document).find(postscript_name), std::string::npos);
    };

    render({.resources = g_resources}, "Edwin-Roman");
    render({.resources = alternate}, "FreeSerif");
    render({.resources = alternate, .fonts = {g_resources / roman}}, "Edwin-Roman");
}

TEST_F(EngineLifecycle, CairoContextKeepsItsFontUsableAfterEngineTeardown) {
    using namespace muse::draw;
    const auto path = g_resources.u8string();
    auto surface = std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>(
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 200, 80), cairo_surface_destroy);
    auto cr = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>(cairo_create(surface.get()), cairo_destroy);
    {
        const music::EngineContext context(std::string(path.begin(), path.end()));
        CairoPainter painter(cr.get(), context.assets().cairo_fonts(), 72);
        Font font(u"Edwin", Font::Type::TEXT);
        font.set_point_size_f(16);
        painter.set_font(font);
        painter.draw_text(muse::PointF(4, 20), muse::String(u"A"));
    }
    cairo_text_extents_t extent{};
    cairo_text_extents(cr.get(), "W", &extent);
    EXPECT_EQ(cairo_status(cr.get()), CAIRO_STATUS_SUCCESS);
    EXPECT_GT(extent.width, 0);
}

TEST_F(EngineLifecycle, LastDocumentReleasesRuntimeForAnotherEngine) {
    using mu::engraving::MsError;
    using mu::engraving::MScore;
    using mu::engraving::sm_drumset;

    auto first = engrave::Engine::create({.resources = g_resources});
    ASSERT_TRUE(first) << first.error().message;
    auto document = (*first)->create_document({.template_id = "Treble_Clef"});
    ASSERT_TRUE(document) << document.error().message;
    auto other = (*first)->create_document({.template_id = "Treble_Clef"});
    ASSERT_TRUE(other) << other.error().message;
    ASSERT_TRUE((*document)->page_count());
    first->reset();

    const auto before = (*document)->outline();
    ASSERT_TRUE(before) << before.error().message;
    const auto edited = (*document)->edit({.ops = {engrave::InsertMeasures{.count = 2}}});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection);
    const auto measures = (*document)->query({.types = {"Measure"}});
    ASSERT_TRUE(measures) << measures.error().message;
    EXPECT_EQ(measures->total, static_cast<std::size_t>(before->measure_count + 2));
    const auto saved = (*document)->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved) << saved.error().message;
    EXPECT_DEATH(std::thread([&] { std::ignore = (*document)->outline(); }).join(), "thread other than");

    document->reset();
    const auto active = engrave::Engine::create({.resources = g_resources});
    ASSERT_FALSE(active);
    EXPECT_EQ(active.error().code, ErrorCode::UNSUPPORTED);

    const muse::Settings::Key key{"test", "engine_lifetime"};
    muse::settings()->set_local_value(key, muse::Val(true));
    muse::settings()->begin_transaction();
    sm_drumset->set_percussion_panel_columns(42);
    MScore::test_mode = true;
    MScore::set_error(MsError::NO_NOTE_SELECTED, true);
    other->reset();
    EXPECT_FALSE(MScore::test_mode);
    EXPECT_EQ(MScore::error, MsError::MS_NO_ERROR);
    EXPECT_FALSE(MScore::error_is_warning);

    const auto second = engrave::Engine::create({.resources = g_resources});
    ASSERT_TRUE(second) << second.error().message;
    EXPECT_FALSE(muse::settings()->items().contains(key));
    EXPECT_NO_THROW(muse::settings()->begin_transaction());
    muse::settings()->rollback_transaction();
    EXPECT_EQ(sm_drumset->percussion_panel_columns(), 8u);
    const auto reopened = (*second)->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto after = (*reopened)->outline();
    ASSERT_TRUE(after) << after.error().message;
    EXPECT_EQ(after->measure_count, before->measure_count + 2);
    const auto edit_again = (*reopened)->edit({.ops = {engrave::InsertMeasures{.count = 1}}});
    ASSERT_TRUE(edit_again) << edit_again.error().message;
    ASSERT_FALSE(edit_again->rejection);
    const auto render = (*reopened)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(render) << render.error().message;
    ASSERT_FALSE(render->pages.empty());
    EXPECT_NE(text_of(render->pages.front()).find("<svg"), std::string::npos);
}

std::unique_ptr<engrave::Document> notation_score(int bars, bool pickup = false, std::string_view part = {}) {
    constexpr std::string_view piano = R"(<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano">
<instrumentId>keyboard.piano</instrumentId><Channel><program value="0"/></Channel></Instrument></Part>)";
    std::string xml = R"(<museScore version="4.70"><Score><Division>480</Division>)";
    xml += part.empty() ? piano : part;
    xml += R"(<Staff id="1">)";
    for (int bar = 0; bar < bars; ++bar) {
        xml += pickup && bar == 0 ? R"(<Measure len="1/4">)" : "<Measure>";
        xml += "<voice>";
        if (bar == 0) xml += "<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>";
        xml += "<Rest><durationType>measure</durationType></Rest></voice></Measure>";
    }
    xml += "</Staff></Score></museScore>";
    auto opened = g_engine->open(bytes_of(xml));
    EXPECT_TRUE(opened) << opened.error().message;
    return opened ? std::move(*opened) : nullptr;
}

TEST(LiveEdit, InsertionShiftsOrdinalsAndSpannerEndpoints) {
    using namespace engrave;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto original_second = from_engine(score.last_measure()->eid());
    const auto edited = document->edit({{
        InsertMeasures{MeasureIndex{1}, 1},
        EnterNotes{RelativePosition{{1}}, {1, 4}, {{62}}},
        EnterNotes{RelativePosition{{2}}, {1, 4}, {{64}}},
        AddSpanner{"HairPin", RelativePosition{{1}}, RelativePosition{{2}}, {}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    EXPECT_EQ(edited->applied, 4u);
    EXPECT_EQ(from_engine(score.last_measure()->eid()), original_second);
    const auto *inserted = score.find_cr({1, 1}, 0);
    const auto *shifted = score.find_cr({2, 1}, 0);
    ASSERT_TRUE(inserted && inserted->is_chord());
    ASSERT_TRUE(shifted && shifted->is_chord());
    EXPECT_EQ(mu::engraving::to_chord(inserted)->notes().front()->pitch(), 62);
    EXPECT_EQ(mu::engraving::to_chord(shifted)->notes().front()->pitch(), 64);
    const auto lines = score.spanner_list();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_TRUE(lines.front()->is_hairpin());
    EXPECT_EQ(lines.front()->tick(), mu::engraving::Fraction(1, 1));
    EXPECT_EQ(lines.front()->tick2(), mu::engraving::Fraction(2, 1));
}

TEST(LiveEdit, DeletingOrdinalRangeRetargetsTheFollowingEntry) {
    using namespace engrave;
    auto document = notation_score(4);
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto original_last = from_engine(score.last_measure()->eid());
    const auto edited = document->edit({{
        RemoveMeasures{MeasureIndex{1}, MeasureIndex{2}},
        EnterNotes{RelativePosition{{1}, 0, 0, {1, 4}}, {1, 4}, {{67}}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    EXPECT_EQ(score.first_measure()->next_measure(), score.last_measure());
    EXPECT_EQ(from_engine(score.last_measure()->eid()), original_last);
    const auto *entered = score.find_cr({5, 4}, 0);
    ASSERT_TRUE(entered && entered->is_chord());
    EXPECT_EQ(mu::engraving::to_chord(entered)->notes().front()->pitch(), 67);
}

TEST(LiveEdit, ExpandingMeterMakesNewOffsetsWritable) {
    using namespace engrave;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    const auto edited = document->edit({{
        SetTimeSig{MeasureIndex{0}, {5, 4}},
        EnterNotes{RelativePosition{{0}, 0, 0, {1, 1}}, {1, 4}, {{70}}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    auto &score = harness::master_of(*document);
    EXPECT_EQ(score.first_measure()->ticks(), mu::engraving::Fraction(5, 4));
    const auto *entered = score.find_cr({1, 1}, 0);
    ASSERT_TRUE(entered && entered->is_chord());
    EXPECT_EQ(mu::engraving::to_chord(entered)->notes().front()->pitch(), 70);
}

TEST(LiveEdit, ShrinkingMeterRejectsAFormerOnsetWithAtomicAndPrefixSemantics) {
    using namespace engrave;
    for (const auto mode : {EditMode::Atomic, EditMode::Prefix}) {
        auto document = notation_score(1);
        ASSERT_TRUE(document);
        const auto revision = document->revision();
        const auto edited = document->edit({{
                                               SetTimeSig{MeasureIndex{0}, {3, 4}},
                                               EnterNotes{RelativePosition{{0}, 0, 0, {3, 4}}, {1, 4}, {{62}}},
                                           }},
                                           {.mode = mode});
        ASSERT_TRUE(edited) << edited.error().message;
        ASSERT_TRUE(edited->rejection);
        EXPECT_EQ(edited->rejection->op_index, 1u);
        EXPECT_EQ(edited->rejection->error.code, ErrorCode::EDIT_REJECTED);
        EXPECT_TRUE(edited->rejection->error.message.starts_with("at:"));
        EXPECT_NE(edited->rejection->error.message.find("offset"), std::string::npos);
        EXPECT_FALSE(edited->rejection->error.poisoned);
        const bool prefix = mode == EditMode::Prefix;
        EXPECT_EQ(edited->applied, prefix ? 1u : 0u);
        EXPECT_EQ(document->revision(), revision + prefix);
        auto &score = harness::master_of(*document);
        EXPECT_EQ(score.first_measure()->ticks(), prefix ? mu::engraving::Fraction(3, 4) : mu::engraving::Fraction(1, 1));
        const auto retry = document->edit({{EnterNotes{RelativePosition{{0}, 0, 0, {1, 4}}, {1, 4}, {{65}}}}});
        ASSERT_TRUE(retry) << retry.error().message;
        EXPECT_FALSE(retry->rejection);
        const auto *entered = score.find_cr({1, 4}, 0);
        ASSERT_TRUE(entered && entered->is_chord());
        EXPECT_EQ(mu::engraving::to_chord(entered)->notes().front()->pitch(), 65);
    }
}

TEST(LiveEdit, AddingPartsAndStavesMovesStaffOrdinalsBeforeEntry) {
    using namespace engrave;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto original_part = harness::find_id(*document, "Part");
    const auto *original_staff = score.staff(0);
    const auto edited = document->edit({{
        AddPart{"violin", original_part},
        AddStaff{OpRef{0, 0}},
        SetKeySig{MeasureIndex{0}, 2, StaffIndex{2}},
        EnterNotes{RelativePosition{{0}, 2}, {1, 4}, {{68}}},
        EnterNotes{RelativePosition{{0}, 1}, {1, 4}, {{72}}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    ASSERT_EQ(score.nstaves(), 3u);
    EXPECT_EQ(score.staff(2), original_staff);
    EXPECT_EQ(static_cast<int>(score.staff(2)->key({0, 1})), 2);
    EXPECT_EQ(static_cast<int>(score.staff(0)->key({0, 1})), 0);
    for (const auto [track, pitch] : {std::pair{4, 72}, {8, 68}}) {
        const auto *entered = score.find_cr({0, 1}, track);
        ASSERT_TRUE(entered && entered->is_chord());
        EXPECT_EQ(mu::engraving::to_chord(entered)->notes().front()->pitch(), pitch);
    }
    const auto removed = document->edit({{RemoveStaff{StaffIndex{1}}}});
    ASSERT_TRUE(removed) << removed.error().message;
    EXPECT_FALSE(removed->rejection);
    ASSERT_EQ(score.nstaves(), 2u);
    EXPECT_EQ(score.staff(1), original_staff);
}

TEST(LiveEdit, ExcerptScopeUsesLocalStaffAndRejectsForeignOperands) {
    using namespace engrave;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    const auto setup = document->edit({{AddPart{"violin"}, GenerateExcerpts{{OpRef{0, 0}}}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
    const ScoreSel excerpt_scope{setup->ops[1].created[0]};
    ASSERT_TRUE(excerpt_scope.excerpt);
    auto &master = harness::master_of(*document);
    auto *excerpt = master.score_list().back();
    ASSERT_NE(excerpt, &master);
    ASSERT_EQ(excerpt->nstaves(), 1u);
    const auto *first_bar = master.first_measure();
    const auto master_bar = from_engine(first_bar->eid());
    const auto master_rest = harness::find_id(*document, "Rest", [&](const auto &object) { return &object == master.find_cr({0, 1}, 0); });
    using mu::engraving::Sid;
    const auto master_width = master.style().style_d(Sid::PAGE_WIDTH);
    const auto edited = document->edit({
        .ops = {EnterNotes{RelativePosition{{0}}, {1, 4}, {{69}}}, SetStyle{std::nullopt, "pageWidth", 9.0}},
        .scope = excerpt_scope,
    });
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    for (const auto *chord : {excerpt->find_cr({0, 1}, 0), master.find_cr({0, 1}, 4)}) {
        ASSERT_TRUE(chord && chord->is_chord());
        EXPECT_EQ(mu::engraving::to_chord(chord)->notes().front()->pitch(), 69);
    }
    EXPECT_TRUE(master.find_cr({0, 1}, 0)->is_rest());
    EXPECT_EQ(excerpt->style().style_d(Sid::PAGE_WIDTH), 9.0);
    EXPECT_EQ(master.style().style_d(Sid::PAGE_WIDTH), master_width);
    const auto revision = document->revision();
    const auto part = harness::find_id(*document, "Part", [&](const auto &object) { return object.score() == &master; });
    for (const Op &op : std::vector<Op>{
             SetProperty{master_rest, "visible", false},
             EnterNotes{SegmentAddress{master_bar, 0, 0, {0, 1}, "ChordRest"}, {1, 4}, {{70}}},
             InsertMeasures{{}, 1},
             RemoveMeasures{MeasureIndex{0}, MeasureIndex{0}},
             AddPart{"violin"},
             RemovePart{part},
             GenerateExcerpts{{part}},
             SetStyle{ScoreSel{}, "pageWidth", 12.0},
         }) {
        const auto refused = document->edit(
            {
                .ops = {SetStyle{std::nullopt, "pageWidth", 10.0}, op},
                .scope = excerpt_scope,
            },
            {.mode = EditMode::Prefix});
        ASSERT_FALSE(refused);
        EXPECT_EQ(refused.error().code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(refused.error().op_index, 1u);
        EXPECT_EQ(document->revision(), revision);
        EXPECT_EQ(excerpt->style().style_d(Sid::PAGE_WIDTH), 9.0);
    }
    const auto foreign_result = document->edit(
        {
            .ops = {GenerateExcerpts{{part}}, SetProperty{OpRef{0, 0}, "visible", false}},
            .scope = ScoreSel{},
        },
        {.mode = EditMode::Prefix});
    ASSERT_TRUE(foreign_result) << foreign_result.error().message;
    ASSERT_TRUE(foreign_result->rejection);
    EXPECT_EQ(foreign_result->applied, 1u);
    EXPECT_EQ(foreign_result->rejection->op_index, 1u);
    EXPECT_NE(foreign_result->rejection->error.message.find("different score"), std::string::npos);

    const auto invalid_scope = document->edit({.ops = {}, .scope = ScoreSel{master_bar}});
    ASSERT_FALSE(invalid_scope);
    EXPECT_EQ(invalid_scope.error().code, ErrorCode::INVALID_ARGUMENT);
}

TEST(LiveEdit, ForeignInsertBeforeLeavesBothScoresIntact) {
    using namespace engrave;
    auto document = notation_score(3);
    ASSERT_TRUE(document);
    const auto setup = document->edit({{AddPart{"violin"}, GenerateExcerpts{{OpRef{0, 0}}}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;

    const auto score_shape = [](const mu::engraving::Score &score) {
        std::vector<ElementId> bars;
        std::size_t frames = 0;
        for (const auto *base = score.first(); base; base = base->next()) {
            if (base->is_measure())
                bars.push_back(from_engine(base->eid()));
            else
                ++frames;
        }
        return std::pair{std::move(bars), frames};
    };
    auto &master = harness::master_of(*document);
    const auto *excerpt = master.score_list().back();
    ASSERT_NE(excerpt, &master);
    const auto master_before = score_shape(master);
    const auto excerpt_before = score_shape(*excerpt);
    ASSERT_EQ(master_before.first.size(), 3u);
    ASSERT_EQ(excerpt_before.first.size(), 3u);
    const auto revision = document->revision();

    for (const auto mode : {EditMode::Atomic, EditMode::Prefix}) {
        for (const auto *kind : {"VBox", "Measure"}) {
            SCOPED_TRACE(kind);
            const auto edited = document->edit({{InsertMeasures{excerpt_before.first[1], 1, kind}}}, {.mode = mode});
            ASSERT_FALSE(edited);
            EXPECT_EQ(edited.error().code, ErrorCode::INVALID_ARGUMENT);
            EXPECT_EQ(edited.error().op_index, 0u);
            EXPECT_TRUE(edited.error().applied_ops.empty());
            EXPECT_EQ(document->revision(), revision);
            auto &current = harness::master_of(*document);
            EXPECT_EQ(score_shape(current), master_before);
            EXPECT_EQ(score_shape(*current.score_list().back()), excerpt_before);
        }
    }
}

TEST(LiveEdit, ExcerptMeterAndRepeatChangesRejectBeforeAnEarlierEdit) {
    using namespace engrave;
    using mu::engraving::Sid;
    auto document = notation_score(3);
    ASSERT_TRUE(document);
    const auto setup = document->edit({{AddPart{"violin"}, GenerateExcerpts{{OpRef{0, 0}}}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
    const ScoreSel scope{setup->ops[1].created[0]};
    ASSERT_TRUE(scope.excerpt);
    auto &master = harness::master_of(*document);
    const auto *excerpt = master.score_list().back();
    ASSERT_NE(excerpt, &master);
    const auto width = excerpt->style().style_d(Sid::PAGE_WIDTH);
    const auto revision = document->revision();

    for (const auto mode : {EditMode::Atomic, EditMode::Prefix}) {
        for (const auto &op : std::vector<Op>{SetTimeSig{MeasureIndex{1}, {3, 4}}, SetRepeatBarlines{MeasureIndex{1}, true}}) {
            const auto edited =
                document->edit({.ops = {SetStyle{std::nullopt, "pageWidth", width + 1}, op}, .scope = scope}, {.mode = mode});
            ASSERT_FALSE(edited);
            EXPECT_EQ(edited.error().code, ErrorCode::INVALID_ARGUMENT);
            EXPECT_EQ(edited.error().op_index, 1u);
            EXPECT_TRUE(edited.error().applied_ops.empty());
            EXPECT_EQ(document->revision(), revision);
            EXPECT_EQ(excerpt->style().style_d(Sid::PAGE_WIDTH), width);
            for (const auto *score : {static_cast<const mu::engraving::Score *>(&master), excerpt}) {
                const auto *bar = score->first_measure()->next_measure();
                EXPECT_EQ(bar->timesig(), mu::engraving::Fraction(4, 4));
                EXPECT_FALSE(bar->repeat_start());
            }
        }
    }
}

TEST(LiveEdit, ExcerptStaffEditsStayLocalAndKeepTheMasterSaveGuard) {
    using namespace engrave;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    const auto master_part = harness::find_id(*document, "Part");
    const auto setup = document->edit({{GenerateExcerpts{{master_part}}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
    const ScoreSel scope{setup->ops[0].created[0]};
    ASSERT_TRUE(scope.excerpt);
    auto &master = harness::master_of(*document);
    ASSERT_EQ(master.score_list().size(), 2u);
    auto *excerpt = master.score_list().back();
    const auto excerpt_part = harness::find_id(*document, "Part", [&](const auto &object) { return object.score() == excerpt; });

    const auto added = document->edit({.ops = {AddStaff{excerpt_part}}, .scope = scope});
    ASSERT_TRUE(added) << added.error().message;
    ASSERT_FALSE(added->rejection) << added->rejection->error.message;
    EXPECT_EQ(excerpt->nstaves(), 2u);
    EXPECT_EQ(master.nstaves(), 1u);

    const auto removed = document->edit({.ops = {RemoveStaff{StaffIndex{1}}}, .scope = scope});
    ASSERT_TRUE(removed) << removed.error().message;
    ASSERT_FALSE(removed->rejection) << removed->rejection->error.message;
    EXPECT_EQ(excerpt->nstaves(), 1u);
    EXPECT_EQ(master.nstaves(), 1u);

    const auto guarded = document->edit({{AddStaff{master_part}}});
    ASSERT_TRUE(guarded) << guarded.error().message;
    ASSERT_TRUE(guarded->rejection);
    EXPECT_EQ(guarded->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_NE(guarded->rejection->error.message.find("does not survive a save"), std::string::npos);
    EXPECT_EQ(guarded->applied, 0u);

    const auto saved = document->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved) << saved.error().message;
    const auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto scores = harness::master_of(**reopened).score_list();
    ASSERT_EQ(scores.size(), 2u);
    for (auto *score : scores) EXPECT_EQ(score->nstaves(), 1u);
}

TEST(LiveEdit, MeasureOrdinalsSkipFramesAndUseAuthoredBarsInsideMultiMeasureRests) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    constexpr std::string_view source = R"(<museScore version="4.70"><Score><Division>480</Division>
<Style><createMultiMeasureRests>1</createMultiMeasureRests></Style>
<Part><Staff id="1"/><Instrument><trackName>Piano</trackName></Instrument></Part><Staff id="1">
<VBox><height>10</height></VBox>
<Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>whole</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
</voice></Measure><HBox><width>5</width></HBox>
<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
</Staff></Score></museScore>)";
    const auto document = g_engine->open(bytes_of(source));
    ASSERT_TRUE(document) << document.error().message;
    auto &score = harness::master_of(**document);
    ASSERT_TRUE((*document)->render({.format = RenderFormat::SVG}));
    auto *first = score.first_measure();
    ASSERT_TRUE(first->prev() && first->prev()->is_v_box());
    ASSERT_TRUE(first->next() && first->next()->is_h_box());
    const auto *rest_run = first->next_measure()->mm_rest();
    ASSERT_NE(rest_run, nullptr);
    ASSERT_EQ(rest_run->mm_rest_count(), 3);

    const auto edited = (*document)->edit({{
        EnterNotes{RelativePosition{{2}, 0, 0, {1, 4}}, {1, 4}, {{67}}},
        SetLayoutBreak{MeasureIndex{3}, "line", true},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    const auto *second = first->next_measure();
    const auto *third = second->next_measure();
    ASSERT_EQ(third->tick(), Tick(2, 1));
    const auto *segment = third->find_segment_r(mu::engraving::SegmentType::CHORD_REST, {1, 4});
    ASSERT_NE(segment, nullptr);
    ASSERT_TRUE(segment->element(0) && segment->element(0)->is_chord());
    EXPECT_EQ(mu::engraving::to_chord(segment->element(0))->notes().front()->pitch(), 67);
    EXPECT_TRUE(second->first(mu::engraving::SegmentType::CHORD_REST)->element(0)->is_rest());
    EXPECT_EQ(score.last_measure()->tick(), Tick(3, 1));
    EXPECT_TRUE(score.last_measure()->line_break());
    EXPECT_FALSE(third->line_break());
}

TEST(LiveEdit, MarkingsAndLinesUseTheRequestedOnsetAndTrack) {
    using namespace engrave;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    const RelativePosition first{{0}, 0, 0, {0, 1}};
    const RelativePosition second{{0}, 0, 0, {1, 4}};
    const RelativePosition line_start{{0}, 0, 1, {0, 1}};
    const RelativePosition line_end{{0}, 0, 1, {1, 4}};
    const auto edited = document->edit({{
        EnterNotes{first, {1, 4}, {{60}}},
        EnterNotes{second, {1, 4}, {{62}}},
        Add{first, "Articulation", {{"symbol", Enum{"articStaccatoAbove"}}}},
        Add{line_start, "Dynamic", {{"DYNAMIC_TYPE", Enum{"ff"}}}},
        Add{first, "Fermata", {{"symbol", Enum{"fermataAbove"}}}},
        Add{first, "Harmony", {{"text", "Dm"}}},
        Add{first, "StaffText", {{"text", "dolce"}}},
        Add{first, "Tempo", {{"text", "Vivace"}}},
        Add{first, "Expression", {{"text", "sempre"}}},
        AddText{second, "staff", "second beat"},
        InsertClef{second, "F"},
        AddSpanner{"Slur", first, second, {}},
        AddSpanner{"HairPin", line_start, line_end, {}},
        InsertMeasures{{}, 1},
        AddSpannerToBarEnd{"TextLine", line_start, {MeasureIndex{1}}, {}},
        AddSpannerToBarEnd{"Pedal", first, {OpRef{13, 0}}, {}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->op_index << " " << edited->rejection->error.message;
    auto &score = harness::master_of(*document);
    const auto *chord = mu::engraving::to_chord(score.find_cr({0, 1}, 0));
    EXPECT_EQ(chord->articulations().size(), 1u);
    const auto *segment = chord->segment();
    EXPECT_EQ(segment->element(1), nullptr);
    for (const std::string_view type : {"Dynamic", "Fermata", "Harmony", "StaffText", "Tempo", "Expression"}) {
        const auto items = segment->annotations();
        const auto found = std::ranges::find_if(items, [&](const auto *item) { return item->type_name() == type; });
        ASSERT_NE(found, items.end()) << type;
        EXPECT_EQ((*found)->track(), type == "Dynamic" ? 1u : 0u);
    }
    const auto lines = score.spanner_list();
    ASSERT_EQ(lines.size(), 4u);
    for (const auto *line : lines) {
        EXPECT_EQ(line->tick(), mu::engraving::Fraction(0, 1));
        EXPECT_EQ(line->tick2(), line->is_slur() || line->is_hairpin() ? mu::engraving::Fraction(1, 4) : mu::engraving::Fraction(2, 1));
        EXPECT_EQ(line->track(), line->is_hairpin() || line->is_text_line() ? 1u : 0u);
    }
}

TEST(LiveEdit, ExplicitSegmentAnchorsWorkWithOtherReferenceTypes) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    const RelativePosition first{{0}, 0, 1, {0, 1}};
    const RelativePosition second{{0}, 0, 1, {1, 4}};
    const auto setup = document->edit({{EnterNotes{first, {1, 4}, {{60}}}, EnterNotes{second, {1, 4}, {{62}}}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
    ASSERT_TRUE(setup->ops[1].created[0]);

    auto &score = harness::master_of(*document);
    const auto bar_id = from_engine(score.first_measure()->eid());
    const Address first_segment = SegmentAddress{bar_id, 0, 1, {0, 1}, "ChordRest"};
    const Address second_segment = SegmentAddress{bar_id, 0, 1, {1, 4}, "ChordRest"};
    const ElementId second_chord = *setup->ops[1].created[0];
    const auto edited = document->edit({{
        Add{first_segment, "Articulation", {{"symbol", Enum{"articStaccatoAbove"}}}},
        Add{first_segment, "Dynamic", {{"DYNAMIC_TYPE", Enum{"ff"}}}},
        AddSpanner{"Slur", first_segment, second_chord, {}},
        AddSpanner{"Slur", first, second_segment, {}},
        AddSpanner{"HairPin", first_segment, second, {}},
        AddSpanner{"HairPin", first, second_segment, {}},
        AddSpanner{"HairPin", first_segment, second_chord, {}},
        AddSpannerToBarEnd{"TextLine", first_segment, {MeasureIndex{1}}, {}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->op_index << " " << edited->rejection->error.message;
    const auto *first_chord = score.find_cr({0, 1}, 1);
    ASSERT_TRUE(first_chord && first_chord->is_chord());
    EXPECT_EQ(mu::engraving::to_chord(first_chord)->articulations().size(), 1u);
    const auto &annotations = first_chord->segment()->annotations();
    const auto dynamic = std::ranges::find_if(annotations, [](const auto *item) { return item->is_dynamic(); });
    ASSERT_NE(dynamic, annotations.end());
    EXPECT_EQ((*dynamic)->track(), 1u);

    const auto lines = score.spanner_list();
    ASSERT_EQ(lines.size(), 6u);
    for (const auto *line : lines) {
        EXPECT_EQ(line->tick(), Tick(0, 1));
        EXPECT_EQ(line->tick2(), line->is_text_line() ? Tick(2, 1) : Tick(1, 4));
        EXPECT_EQ(line->track(), 1u);
        if (line->is_slur()) {
            EXPECT_EQ(line->start_element(), first_chord);
            ASSERT_TRUE(line->end_element());
            EXPECT_EQ(from_engine(line->end_element()->eid()), second_chord);
        } else {
            EXPECT_EQ(line->effective_track2(), 1u);
        }
    }
}

TEST(NotationEdits, ReversedSlursRejectWithoutMutation) {
    using namespace engrave;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    const RelativePosition first{{0}};
    const RelativePosition second{{0}, 0, 0, {1, 4}};
    const auto setup = document->edit({{EnterNotes{first, {1, 4}, {{60}}}, EnterNotes{second, {1, 4}, {{62}}}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
    ASSERT_TRUE(setup->ops[0].created[0] && setup->ops[1].created[0]);
    const auto bar_id = from_engine(harness::master_of(*document).first_measure()->eid());
    const Address explicit_second = SegmentAddress{bar_id, 0, 0, {1, 4}, "ChordRest"};
    const auto revision = document->revision();

    for (const auto mode : {EditMode::Atomic, EditMode::Prefix}) {
        for (const auto &op : {AddSpanner{"Slur", second, first, {}}, AddSpanner{"Slur", explicit_second, first, {}},
                               AddSpanner{"Slur", *setup->ops[1].created[0], *setup->ops[0].created[0], {}}}) {
            const auto edited = document->edit({{op}}, {.mode = mode});
            ASSERT_TRUE(edited) << edited.error().message;
            ASSERT_TRUE(edited->rejection);
            EXPECT_EQ(edited->rejection->error.code, ErrorCode::EDIT_REJECTED);
            EXPECT_EQ(edited->rejection->op_index, 0u);
            EXPECT_EQ(edited->applied, 0u);
            EXPECT_FALSE(edited->rejection->error.poisoned);
            EXPECT_TRUE(edited->created.empty() && edited->removed.empty() && edited->replaced.empty());
            EXPECT_EQ(document->revision(), revision);
            EXPECT_TRUE(harness::master_of(*document).spanner_list().empty());
        }
    }
}

TEST(LiveEdit, FailedAnchorLookupsLeaveTheSegmentListIntact) {
    using namespace engrave;
    using mu::engraving::SegmentType;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    const auto revision = document->revision();
    auto &score = harness::master_of(*document);
    auto *first_bar = score.first_measure();
    const RelativePosition first{{0}};
    const RelativePosition absent{{0}, 0, 0, {1, 2}};
    const RelativePosition empty_voice{{1}, 0, 1};
    for (const Op &op : std::vector<Op>{
             Add{absent, "Dynamic", {{"DYNAMIC_TYPE", Enum{"ff"}}}},
             Add{first, "Articulation", {{"symbol", Enum{"articStaccatoAbove"}}}},
             AddSpanner{"Slur", first, empty_voice, {}},
             AddSpanner{"HairPin", first, absent, {}},
             AddSpannerToBarEnd{"HairPin", absent, {MeasureIndex{1}}, {}},
             InsertClef{absent, "F"},
             AddText{absent, "staff", "missing"},
         }) {
        const auto edited = document->edit({{op}});
        ASSERT_TRUE(edited) << edited.error().message;
        ASSERT_TRUE(edited->rejection);
        EXPECT_EQ(edited->applied, 0u);
        EXPECT_EQ(edited->rejection->op_index, 0u);
        EXPECT_FALSE(edited->rejection->error.poisoned);
        EXPECT_EQ(document->revision(), revision);
        EXPECT_FALSE(first_bar->find_segment(SegmentType::CHORD_REST, {1, 2}));
        EXPECT_TRUE(score.spanner_list().empty());
    }

    // Two ChordRest segments can share an onset. The edit must not choose by list order.
    auto *duplicate = mu::engraving::Factory::create_segment(first_bar, SegmentType::CHORD_REST, {0, 1});
    first_bar->add(duplicate);
    const auto edited = document->edit({{Add{first, "StaffText", {{"text", "ambiguous"}}}}});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_TRUE(edited->rejection);
    EXPECT_NE(edited->rejection->error.message.find("more than one"), std::string::npos);
    EXPECT_TRUE(duplicate->annotations().empty());
    EXPECT_TRUE(first_bar->first(SegmentType::CHORD_REST)->annotations().empty());
}

TEST(LiveEdit, AmbiguousRelativeNoteAndDrumEntryDoNotMutateOrPoisonTheBatch) {
    using namespace engrave;
    using mu::engraving::SegmentType;
    for (const auto mode : {EditMode::Atomic, EditMode::Prefix}) {
        for (const Op &op :
             std::vector<Op>{EnterNotes{RelativePosition{{0}, 0}, {1, 4}, {{65}}}, EnterDrums{RelativePosition{{0}, 1}, {1, 4}, {{38}}}}) {
            auto document = notation_score(2);
            ASSERT_TRUE(document);
            const auto setup = document->edit({{AddPart{"drumset"}}});
            ASSERT_TRUE(setup) << setup.error().message;
            ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
            auto *bar = harness::master_of(*document).first_measure();
            auto *original = bar->first(SegmentType::CHORD_REST);
            ASSERT_TRUE(original->element(0) && original->element(0)->is_rest());
            ASSERT_TRUE(original->element(4) && original->element(4)->is_rest());
            const auto note_rest = original->element(0)->eid();
            const auto drum_rest = original->element(4)->eid();
            auto *duplicate = mu::engraving::Factory::create_segment(bar, SegmentType::CHORD_REST, {0, 1});
            bar->add(duplicate);
            const auto revision = document->revision();
            const auto result = document->edit({{op}}, {.mode = mode});
            ASSERT_TRUE(result) << result.error().message;
            ASSERT_TRUE(result->rejection);
            EXPECT_EQ(result->rejection->error.code, ErrorCode::EDIT_REJECTED);
            EXPECT_EQ(result->rejection->error.message, "at: more than one ChordRest segment matches this onset");
            EXPECT_FALSE(result->rejection->error.poisoned);
            EXPECT_EQ(result->applied, 0u);
            EXPECT_TRUE(result->created.empty() && result->removed.empty() && result->replaced.empty());
            EXPECT_EQ(document->revision(), revision);

            // Atomic rollback reopens the score and drops the empty fixture segment.
            auto &score = harness::master_of(*document);
            for (const auto &[track, id] : {std::pair{0u, note_rest}, std::pair{4u, drum_rest}}) {
                const auto *rest = score.find_cr({0, 1}, track);
                ASSERT_TRUE(rest && rest->is_rest());
                EXPECT_EQ(rest->eid(), id);
                EXPECT_EQ(rest->ticks(), mu::engraving::Fraction(1, 1));
            }
            if (mode == EditMode::Prefix) {
                EXPECT_EQ(duplicate->element(0), nullptr);
                EXPECT_EQ(duplicate->element(4), nullptr);
            }
            EXPECT_EQ(score.first_measure()->find_segment(SegmentType::CHORD_REST, {1, 4}), nullptr);
            const auto retry = document->edit({{EnterNotes{RelativePosition{{1}, 0, 0, {1, 4}}, {1, 4}, {{65}}}}}, {.mode = mode});
            ASSERT_TRUE(retry) << retry.error().message;
            ASSERT_FALSE(retry->rejection) << retry->rejection->error.message;
            const auto *chord = score.find_cr({5, 4}, 0);
            ASSERT_TRUE(chord && chord->is_chord());
            EXPECT_EQ(mu::engraving::to_chord(chord)->notes().front()->pitch(), 65);
        }
    }
}

TEST(LiveEdit, OutOfRangeOrdinalsAndUnrepresentableTicksRejectTheStep) {
    using namespace engrave;
    auto document = notation_score(3);
    ASSERT_TRUE(document);
    const auto revision = document->revision();
    const auto max_term = std::numeric_limits<std::int32_t>::max();
    using Kind = PositionError::Kind;

    struct Case {
        Op op;
        std::string_view field;
        std::optional<PositionError> position;
    };

    // The fixture has three measures and one staff.
    for (const auto &[op, field, position] : std::vector<Case>{
             {SetLayoutBreak{MeasureIndex{3}, "line", true}, "measure:", PositionError{Kind::MEASURE, "measure", 3, 3}},
             {SetKeySig{MeasureIndex{0}, 2, StaffIndex{1}}, "staff:", PositionError{Kind::STAFF, "staff", 1, 1}},
             {EnterNotes{RelativePosition{{7}}, {1, 4}, {{65}}}, "at:", PositionError{Kind::MEASURE, "at", 7, 3}},
             {EnterNotes{RelativePosition{{0}, 1}, {1, 4}, {{65}}}, "at:", PositionError{Kind::STAFF, "at", 1, 1}},
             {AddSpannerToBarEnd{"HairPin", RelativePosition{}, {MeasureIndex{4}}, {}}, "end:", PositionError{Kind::MEASURE, "end", 4, 3}},
             {EnterNotes{RelativePosition{{0}, 0, 4}, {1, 4}, {{65}}}, "at:", PositionError{Kind::VOICE, "at", 4, 4}},
             {EnterNotes{RelativePosition{{2}, 0, 0, {1, max_term}}, {1, 4}, {{65}}}, "at:", std::nullopt},
         }) {
        const auto edited = document->edit({{op}});
        ASSERT_TRUE(edited) << edited.error().message;
        ASSERT_TRUE(edited->rejection);
        const auto &error = edited->rejection->error;
        EXPECT_EQ(error.code, ErrorCode::EDIT_REJECTED);
        EXPECT_TRUE(error.message.starts_with(field)) << error.message;
        ASSERT_EQ(error.position.has_value(), position.has_value()) << error.message;
        if (position) {
            EXPECT_EQ(error.position->kind, position->kind) << error.message;
            EXPECT_EQ(error.position->field, position->field) << error.message;
            EXPECT_EQ(error.position->index, position->index) << error.message;
            EXPECT_EQ(error.position->count, position->count) << error.message;
        }
        EXPECT_EQ(edited->applied, 0u);
        EXPECT_EQ(document->revision(), revision);
    }
}

TEST(LiveEdit, DrumEntryUsesTheNewPercussionStaffAtAMeasureOffset) {
    using namespace engrave;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    const auto edited = document->edit({{
        AddPart{"drumset"},
        EnterDrums{RelativePosition{{1}, 1, 0, {1, 2}}, {1, 4}, {{38}}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    auto &score = harness::master_of(*document);
    ASSERT_EQ(score.nstaves(), 2u);
    const auto *entered = score.find_cr({3, 2}, 4);
    ASSERT_TRUE(entered && entered->is_chord());
    EXPECT_EQ(mu::engraving::to_chord(entered)->notes().front()->pitch(), 38);
}

TEST(LiveEdit, StaticSelectorErrorsDoNotStartABatch) {
    using namespace engrave;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    const auto revision = document->revision();
    for (const Op &op : std::vector<Op>{
             SetTimeSig{MeasureIndex{-1}, {3, 4}},
             RemoveStaff{StaffIndex{-1}},
             Remove{MeasureIndex{0}},
             Remove{StaffIndex{0}},
             Remove{RelativePosition{}},
             EnterNotes{RelativePosition{{-1}}, {1, 4}, {}},
             EnterNotes{RelativePosition{{0}, -1}, {1, 4}, {}},
             EnterNotes{RelativePosition{{0}, 0, -1}, {1, 4}, {}},
             EnterNotes{RelativePosition{{0}, 0, 0, {1, 0}}, {1, 4}, {}},
             EnterNotes{RelativePosition{{0}, 0, 0, {-1, 4}}, {1, 4}, {}},
             EnterNotes{RelativePosition{{0}, 0, 0, {1, max_fraction_term}}, {1, 4}, {}},
             AddSpannerToBarEnd{"HairPin", RelativePosition{}, {OpRef{3, 0}}, {}},
         }) {
        const auto edited = document->edit({{InsertMeasures{{}, 1}, op}}, {.mode = EditMode::Prefix});
        ASSERT_FALSE(edited);
        EXPECT_EQ(edited.error().op_index, 1u);
        EXPECT_EQ(edited.error().code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(document->revision(), revision);
    }
}

TEST(LiveEdit, MasterOnlyDeletionAndChordRestAnchorsRejectInvalidTargets) {
    using namespace engrave;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    ASSERT_TRUE(document->edit({{AddPart{"violin"}, GenerateExcerpts{{OpRef{0, 0}}}}}));
    const auto outline = document->outline();
    ASSERT_TRUE(outline);
    ASSERT_EQ(outline->excerpts.size(), 1u);
    const auto excerpt_outline = document->outline(ScoreSel{outline->excerpts.front().id});
    ASSERT_TRUE(excerpt_outline);
    ASSERT_GE(excerpt_outline->measures.size(), 2u);
    const auto excerpt_m0 = excerpt_outline->measures[0].id;
    const auto excerpt_m1 = excerpt_outline->measures[1].id;
    const auto master_m0 = outline->measures[0].id;
    const auto revision = document->revision();

    for (const auto mode : {EditMode::Atomic, EditMode::Prefix}) {
        const auto removed = document->edit({{RemoveMeasures{excerpt_m0, excerpt_m1}}}, {.mode = mode});
        ASSERT_FALSE(removed);
        EXPECT_EQ(removed.error().code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_FALSE(removed.error().poisoned);
        EXPECT_EQ(document->revision(), revision);
    }

    const auto non_cr = document->edit({{Add{SegmentAddress{master_m0, 0, 0, {0, 1}, "TimeSig"}, "Dynamic", {{"DYNAMIC_TYPE", "p"}}}}});
    ASSERT_FALSE(non_cr);
    EXPECT_EQ(non_cr.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(document->revision(), revision);

    const auto neg_voice = document->edit({{EnterNotes{SegmentAddress{master_m0, 0, -1, {0, 1}, "ChordRest"}, {1, 4}, {{60}}}}});
    ASSERT_FALSE(neg_voice);
    EXPECT_EQ(neg_voice.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_FALSE(neg_voice.error().position.has_value());
}

TEST(NotationEdits, SegmentLinesReachPickupAndFinalBarlines) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(2, true);
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto pickup = harness::find_id(*document, "Measure");
    const auto final =
        harness::find_id(*document, "Measure", [](const auto &object) { return mu::engraving::to_measure(&object)->tick() == Tick(1, 4); });
    const auto result = document->edit({{
        EnterNotes{SegmentAddress{pickup, 0, 0, {0, 1}, "ChordRest"}, {1, 4}, {{60}}},
        AddSpannerToBarEnd{"HairPin", OpRef{0, 0}, {final}, {}},
        AddSpannerToBarEnd{"TextLine", OpRef{0, 0}, {pickup}, {}},
    }});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    auto check = [](mu::engraving::Score &score) {
        const auto lines = score.spanner_list();
        ASSERT_EQ(lines.size(), 2u);
        for (const auto *line : lines) {
            EXPECT_EQ(line->tick(), Tick(0, 1));
            if (line->is_hairpin()) {
                EXPECT_EQ(line->tick2(), Tick(5, 4));
            } else {
                EXPECT_TRUE(line->is_text_line());
                EXPECT_EQ(line->tick2(), Tick(1, 4));
            }
            ASSERT_FALSE(line->spanner_segments().empty());
        }
    };
    auto render = document->render({.format = RenderFormat::SVG});
    ASSERT_TRUE(render) << render.error().message;
    check(score);
    auto saved = document->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved) << saved.error().message;
    auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    render = (*reopened)->render({.format = RenderFormat::SVG});
    ASSERT_TRUE(render) << render.error().message;
    check(harness::master_of(**reopened));

    const auto start = *result->ops[0].created[0];
    const auto rejected = document->edit({{AddSpannerToBarEnd{"Slur", start, {final}, {}}}});
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().code, ErrorCode::INVALID_ARGUMENT);
}

TEST(NotationEdits, MixedLineEndpointsKeepBothTracksOnTheStartStaffInLinkedScores) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    const RelativePosition lead{{0}, 1};
    const RelativePosition second_voice{{0}, 1, 1};
    const RelativePosition other_staff{{0}, 0, 0, {1, 4}};
    const RelativePosition end{{0}, 1, 0, {1, 4}};
    const auto edited = document->edit({{
        AddPart{"violin"},
        EnterNotes{lead, {1, 4}, {{67}}},
        EnterNotes{second_voice, {1, 4}, {{62}}},
        EnterNotes{other_staff, {1, 4}, {{60}}},
        EnterNotes{end, {1, 4}, {{69}}},
        GenerateExcerpts{{OpRef{0, 0}}},
        AddSpanner{"HairPin", OpRef{1, 0}, other_staff, {}},
        AddSpanner{"TextLine", second_voice, OpRef{4, 0}, {}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->op_index << " " << edited->rejection->error.message;
    auto check = [](mu::engraving::MasterScore &master) {
        const auto scores = master.score_list();
        ASSERT_EQ(scores.size(), 2u);
        ASSERT_EQ(master.nstaves(), 2u);
        const auto *excerpt = scores.back();
        ASSERT_EQ(excerpt->nstaves(), 1u);
        const auto master_lines = master.spanner_list();
        const auto excerpt_lines = excerpt->spanner_list();
        ASSERT_EQ(master_lines.size(), 2u);
        ASSERT_EQ(excerpt_lines.size(), 2u);
        for (const auto *line : master_lines) {
            SCOPED_TRACE(line->type_name());
            ASSERT_TRUE(line->is_hairpin() || line->is_text_line());
            const unsigned voice = line->is_text_line() ? 1 : 0;
            EXPECT_EQ(line->track(), 4u + voice);
            EXPECT_EQ(line->effective_track2(), 4u + voice);
            const auto copy = std::ranges::find_if(excerpt_lines, [&](const auto *item) { return item->type() == line->type(); });
            ASSERT_NE(copy, excerpt_lines.end());
            const auto *copy_line = *copy;
            EXPECT_EQ(line->find_linked_in_score(excerpt), copy_line);
            EXPECT_EQ(copy_line->track(), voice);
            EXPECT_EQ(copy_line->effective_track2(), voice);
            for (const auto *linked : {line, copy_line}) {
                EXPECT_EQ(linked->tick(), Tick(0, 1));
                EXPECT_EQ(linked->tick2(), Tick(1, 4));
            }
        }
    };
    check(harness::master_of(*document));
    const auto saved = document->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved) << saved.error().message;
    const auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    ASSERT_TRUE((*reopened)->render({.format = RenderFormat::SVG}));
    check(harness::master_of(**reopened));
}

TEST(NotationEdits, LyricsLineRequiresAnOwnerForEitherSpannerOperation) {
    using namespace engrave;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    const auto revision = document->revision();
    const RelativePosition first{{0}};
    for (const Op &op : std::vector<Op>{AddSpanner{"LyricsLine", first, RelativePosition{{1}}, {}},
                                        AddSpannerToBarEnd{"LyricsLine", first, {MeasureIndex{1}}, {}}}) {
        const auto result = document->edit({{op}});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(result.error().op_index, 0u);
        EXPECT_NE(result.error().message.find("LyricsLine"), std::string::npos);
        EXPECT_EQ(document->revision(), revision);
        EXPECT_TRUE(harness::master_of(*document).spanner_list().empty());
    }
}

TEST(NotationEdits, PositionalAnchorsUsePrincipalChordsAtGraceOnsets) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    const RelativePosition first{{0}};
    const RelativePosition second{{0}, 0, 0, {1, 4}};
    const auto edited = document->edit({{
        EnterNotes{first, {1, 4}, {{60}}},
        EnterNotes{second, {1, 4}, {{62}}},
        AddGraceChord{OpRef{0, 0}, {GraceKind::Acciaccatura, {1, 8}, {{65}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Grace8After, {1, 8}, {{67}}}},
        AddSpanner{"Slur", first, second, {}},
        AddSpanner{"TextLine", OpRef{2, 1}, second, {}},
        Add{second, "Articulation", {{"symbol", Enum{"articStaccatoAbove"}}}},
    }});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->op_index << " " << edited->rejection->error.message;
    auto &score = harness::master_of(*document);
    auto *from = score.find_cr(Tick(0, 1), 0);
    auto *to = score.find_cr(Tick(1, 4), 0);
    ASSERT_TRUE(from && from->is_chord());
    ASSERT_TRUE(to && to->is_chord());
    const auto *from_chord = mu::engraving::to_chord(from);
    const auto *to_chord = mu::engraving::to_chord(to);
    const auto before = from_chord->grace_notes_before();
    const auto after = to_chord->grace_notes_after();
    ASSERT_EQ(before.size(), 1u);
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ((*before.begin())->notes().front()->pitch(), 65);
    EXPECT_EQ((*after.begin())->notes().front()->pitch(), 67);
    EXPECT_EQ(to_chord->articulations().size(), 1u);
    EXPECT_TRUE((*after.begin())->articulations().empty());

    const auto spanners = score.spanner_list();
    ASSERT_EQ(spanners.size(), 2u);
    for (const auto *spanner : spanners) {
        EXPECT_EQ(spanner->tick(), Tick(0, 1));
        EXPECT_EQ(spanner->tick2(), Tick(1, 4));
        if (spanner->is_slur()) {
            EXPECT_EQ(spanner->start_element(), from);
            EXPECT_EQ(spanner->end_element(), to);
        } else {
            ASSERT_TRUE(spanner->is_text_line());
            EXPECT_EQ(spanner->start_segment(), from->segment());
            EXPECT_EQ(spanner->end_segment(), to->segment());
        }
    }
}

TEST(NotationEdits, UnsupportedSpannerPropertiesRejectTheBatchBeforeMutation) {
    using namespace engrave;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto first = harness::find_id(*document, "Rest");
    const auto last = harness::find_id(*document, "Rest", [&](const auto &object) { return &object == score.find_cr({1, 1}, 0); });
    const auto final = harness::find_id(*document, "Measure", [&](const auto &object) { return &object == score.last_measure(); });

    const std::vector<Op> invalid{
        AddSpannerToBarEnd{"HairPin", first, {final}, {{"visible", false}, {"pitch", 60}}},
        AddSpanner{"HairPin", first, last, {{"pitch", 60}}},
        AddSpanner{"Slur", first, last, {{"pitch", 60}}},
    };
    for (const auto &op : invalid) {
        const auto result = document->edit({{SetProperty{first, "visible", false}, op}}, {.mode = EditMode::Prefix});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().op_index, 1u);
        EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(harness::property(*document, first, "visible"), Value{true});
        EXPECT_TRUE(score.spanner_list().empty());
    }
    const auto result = document->edit({{AddSpannerToBarEnd{"HairPin", first, {final}, {}}}}, {.mode = EditMode::Prefix});
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_FALSE(result->rejection);
    EXPECT_EQ(score.spanner_list().size(), 1u);
}

TEST(NotationEdits, ExcerptSystemLinesKeepTheirLiveIdsAndGuitarLinesFollowStaffLinks) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(2);
    ASSERT_TRUE(document);
    auto &master = harness::master_of(*document);
    const auto part = harness::find_id(*document, "Part");
    const auto generated = document->edit({{GenerateExcerpts{{part}}}});
    ASSERT_TRUE(generated) << generated.error().message;
    ASSERT_FALSE(generated->rejection) << generated->rejection->error.message;
    const auto scores = master.score_list();
    ASSERT_EQ(scores.size(), 2u);
    auto *excerpt = scores.back();
    const auto bar = [&](mu::engraving::Score *score) {
        return harness::find_id(*document, "Measure", [&](const auto &object) { return &object == score->last_measure(); });
    };
    const auto rest = [&](mu::engraving::Score *score) {
        return harness::find_id(*document, "Rest", [&](const auto &object) { return &object == score->find_cr({1, 1}, 0); });
    };
    const auto result = document->edit({{
        AddVolta{bar(excerpt), bar(excerpt), {1}},
        AddSpannerToBarEnd{"GradualTempoChange", rest(excerpt), {bar(excerpt)}, {}},
        AddSpannerToBarEnd{"Rasgueado", rest(&master), {bar(&master)}, {}},
        AddSpannerToBarEnd{"HarmonicMark", rest(&master), {bar(&master)}, {}},
        AddSpannerToBarEnd{"PickScrape", rest(&master), {bar(&master)}, {}},
    }});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;

    const std::vector<std::string_view> types{"Volta", "GradualTempoChange", "Rasgueado", "HarmonicMark", "PickScrape"};
    ASSERT_EQ(result->ops.size(), types.size());
    for (auto *score : scores) EXPECT_EQ(score->spanner_list().size(), types.size());
    for (std::size_t i = 0; i < types.size(); ++i) {
        SCOPED_TRACE(types[i]);
        ASSERT_EQ(result->ops[i].created.size(), 1u);
        ASSERT_TRUE(result->ops[i].created.front());
        auto &object = harness::element(*document, *result->ops[i].created.front());
        ASSERT_EQ(object.type_name(), types[i]);
        const auto *line = mu::engraving::to_spanner(&object);
        auto *origin = i < 2 ? excerpt : &master;
        auto *other = i < 2 ? &master : excerpt;
        const auto originals = origin->spanner_list();
        EXPECT_NE(std::ranges::find(originals, line), originals.end());
        EXPECT_EQ(line->score(), origin);
        EXPECT_EQ(line->tick(), Tick(1, 1));
        EXPECT_EQ(line->tick2(), Tick(2, 1));
        // An unattached original would leave a third member in the linked group.
        EXPECT_EQ(line->link_list().size(), 2u);
        const auto copies = other->spanner_list();
        const auto linked = std::ranges::find_if(copies, [&](const auto *copy) { return copy->type_name() == types[i]; });
        ASSERT_NE(linked, copies.end());
        EXPECT_EQ(line->find_linked_in_score(other), *linked);
        EXPECT_EQ((*linked)->tick(), Tick(1, 1));
        EXPECT_EQ((*linked)->tick2(), Tick(2, 1));
    }
}

TEST(NotationEdits, ShortEntryPreservesBothSidesOfAColoredInvisibleRest) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto part = harness::find_id(*document, "Part");
    const auto bar = harness::find_id(*document, "Measure");
    const auto rest = harness::find_id(*document, "Rest");
    const Color color{42, 82, 120, 255};
    const auto result = document->edit({{
        GenerateExcerpts{{part}},
        SetProperty{rest, "visible", false},
        SetProperty{rest, "color", color},
        EnterNotes{SegmentAddress{bar, 0, 0, {1, 4}, "ChordRest"}, {1, 4}, {{60}}},
    }});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    for (auto *copy : score.score_list()) {
        for (const auto &[start, end] : {std::pair{Tick(0, 1), Tick(1, 4)}, std::pair{Tick(1, 2), Tick(1, 1)}}) {
            const auto *split = copy->find_cr(start, 0);
            ASSERT_TRUE(split && split->is_rest());
            EXPECT_EQ(split->tick(), start);
            EXPECT_EQ(split->end_tick(), end);
            EXPECT_FALSE(split->visible());
            EXPECT_EQ(split->color().red(), 42);
            EXPECT_EQ(split->color().green(), 82);
            EXPECT_EQ(split->color().blue(), 120);
        }
    }
}

TEST(NotationEdits, ChordContinuationsTieEveryPitchAndPreserveTheLeadingRest) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    const std::vector<Pitch> pitches{{67}, {60}, {64}};
    for (const auto &[duration, piece_count] : {std::pair{F{1, 2}, 2u}, std::pair{F{23, 16}, 3u}}) {
        SCOPED_TRACE(piece_count);
        auto document = notation_score(3);
        ASSERT_TRUE(document);
        auto &score = harness::master_of(*document);
        const auto part = harness::find_id(*document, "Part");
        const auto first_bar = harness::find_id(*document, "Measure");
        const auto first_rest = harness::find_id(*document, "Rest");
        const Color color{42, 82, 120, 255};
        const auto result = document->edit({{
            GenerateExcerpts{{part}},
            SetProperty{first_rest, "visible", false},
            SetProperty{first_rest, "color", color},
            EnterNotes{SegmentAddress{first_bar, 0, 0, {3, 4}, "ChordRest"}, duration, pitches},
        }});
        ASSERT_TRUE(result) << result.error().message;
        ASSERT_FALSE(result->rejection) << result->rejection->error.message;
        const auto &ids = result->ops[3].created;
        ASSERT_EQ(ids.size(), piece_count * 4);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            ASSERT_TRUE(ids[i]);
            auto &object = harness::element(*document, *ids[i]);
            if (i % 4 == 0) {
                EXPECT_TRUE(object.is_chord());
            } else {
                ASSERT_TRUE(object.is_note());
                EXPECT_EQ(mu::engraving::to_note(&object)->pitch(), pitches[(i % 4) - 1].pitch);
            }
        }
        auto check = [&](mu::engraving::MasterScore &master) {
            const auto scores = master.score_list();
            ASSERT_EQ(scores.size(), 2u);
            for (auto *copy : scores) {
                for (const auto &[start, end] : {std::pair{Tick(0, 1), Tick(1, 2)}, std::pair{Tick(1, 2), Tick(3, 4)}}) {
                    auto *leading = copy->find_cr(start, 0);
                    ASSERT_TRUE(leading && leading->is_rest());
                    EXPECT_EQ(leading->tick(), start);
                    EXPECT_EQ(leading->end_tick(), end);
                    EXPECT_FALSE(leading->visible());
                    EXPECT_EQ(leading->color().red(), 42);
                    EXPECT_EQ(leading->color().green(), 82);
                    EXPECT_EQ(leading->color().blue(), 120);
                }

                std::vector<Tick> ticks{Tick(3, 4), Tick(1, 1)};
                if (piece_count == 3) ticks.emplace_back(2, 1);
                const Tick final_tick = piece_count == 3 ? Tick(35, 16) : Tick(5, 4);
                std::vector<mu::engraving::Chord *> chords;
                for (const Tick &tick : ticks) {
                    auto *piece = copy->find_cr(tick, 0);
                    ASSERT_TRUE(piece && piece->is_chord());
                    ASSERT_EQ(piece->tick(), tick);
                    auto *chord = mu::engraving::to_chord(piece);
                    ASSERT_EQ(chord->notes().size(), pitches.size());
                    chords.push_back(chord);
                }
                EXPECT_EQ(chords.back()->end_tick(), final_tick);
                for (const auto &pitch : pitches) {
                    mu::engraving::Note *previous = nullptr;
                    for (auto *chord : chords) {
                        auto *note = chord->find_note(pitch.pitch);
                        ASSERT_NE(note, nullptr);
                        if (previous) {
                            auto *tie = previous->tie_for();
                            ASSERT_NE(tie, nullptr);
                            EXPECT_EQ(tie->start_note(), previous);
                            EXPECT_EQ(tie->end_note(), note);
                            EXPECT_EQ(note->tie_back(), tie);
                        } else {
                            EXPECT_EQ(note->tie_back(), nullptr);
                        }
                        previous = note;
                    }
                    EXPECT_EQ(previous->tie_for(), nullptr);
                }
            }
        };
        check(score);
        auto saved = document->export_score({.format = Format::MSCZ});
        ASSERT_TRUE(saved) << saved.error().message;
        auto reopened = g_engine->open(*saved);
        ASSERT_TRUE(reopened) << reopened.error().message;
        check(harness::master_of(**reopened));
    }
}

TEST(NotationEdits, ChordSplitsIntoThreeWrittenValuesWithinOneMeasure) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(1);
    ASSERT_TRUE(document);
    const auto measure = harness::find_id(*document, "Measure");
    const auto result = document->edit({{EnterNotes{SegmentAddress{measure, 0, 0, {0, 1}, "ChordRest"}, {21, 32}, {{60}, {64}, {67}}}}});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    auto &score = harness::master_of(*document);
    auto *initial = score.find_cr(Tick(0, 1), 0);
    auto *middle = score.find_cr(Tick(1, 2), 0);
    auto *final = score.find_cr(Tick(5, 8), 0);
    ASSERT_TRUE(initial && initial->is_chord());
    ASSERT_TRUE(middle && middle->is_chord());
    ASSERT_TRUE(final && final->is_chord());
    EXPECT_EQ(initial->end_tick(), Tick(1, 2));
    EXPECT_EQ(middle->end_tick(), Tick(5, 8));
    EXPECT_EQ(final->end_tick(), Tick(21, 32));
    for (const int pitch : {60, 64, 67}) {
        auto *start_note = mu::engraving::to_chord(initial)->find_note(pitch);
        auto *middle_note = mu::engraving::to_chord(middle)->find_note(pitch);
        auto *end_note = mu::engraving::to_chord(final)->find_note(pitch);
        ASSERT_TRUE(start_note && middle_note && end_note);
        ASSERT_NE(start_note->tie_for(), nullptr);
        ASSERT_NE(middle_note->tie_for(), nullptr);
        EXPECT_EQ(start_note->tie_for()->start_note(), start_note);
        EXPECT_EQ(start_note->tie_for()->end_note(), middle_note);
        EXPECT_EQ(middle_note->tie_for()->start_note(), middle_note);
        EXPECT_EQ(middle_note->tie_for()->end_note(), end_note);
        EXPECT_EQ(middle_note->tie_back(), start_note->tie_for());
        EXPECT_EQ(end_note->tie_back(), middle_note->tie_for());
    }
}

TEST(NotationEdits, RepeatBarlinesPatchLinkedScoresAndVoltasChooseThePass) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = notation_score(3);
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto part = harness::find_id(*document, "Part");
    std::vector<ElementId> bars;
    for (int bar = 0; bar < 3; ++bar) {
        bars.push_back(harness::find_id(*document, "Measure",
                                        [bar](const auto &object) { return mu::engraving::to_measure(&object)->tick() == Tick(bar, 1); }));
    }
    const auto traversal = [](const mu::engraving::Score &score) {
        std::vector<int> order;
        for (const auto *segment : score.expanded_repeat_list()) {
            for (const auto *measure : segment->measure_list()) order.push_back(measure->tick().ticks() / Tick(1, 1).ticks());
        }
        return order;
    };
    const auto apply = [&](std::vector<Op> ops) {
        auto result = document->edit({std::move(ops)});
        ASSERT_TRUE(result) << result.error().message;
        ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    };
    EXPECT_EQ(traversal(score), (std::vector{0, 1, 2}));
    apply({GenerateExcerpts{{part}}, SetRepeatBarlines{.measure = bars[0], .start = true},
           SetRepeatBarlines{.measure = bars[1], .end = true, .count = 3}});
    ASSERT_EQ(score.score_list().size(), 2u);
    for (auto *copy : score.score_list()) {
        EXPECT_TRUE(copy->first_measure()->repeat_start());
        EXPECT_TRUE(copy->tick2measure(Tick(1, 1))->repeat_end());
        EXPECT_EQ(copy->tick2measure(Tick(1, 1))->repeat_count(), 3);
    }
    EXPECT_EQ(traversal(score), (std::vector{0, 1, 0, 1, 0, 1, 2}));
    apply({SetRepeatBarlines{.measure = bars[1], .count = 2}});
    EXPECT_EQ(traversal(score), (std::vector{0, 1, 0, 1, 2}));
    apply({SetRepeatBarlines{.measure = bars[0], .start = false}, SetRepeatBarlines{.measure = bars[1], .end = false}});
    for (auto *copy : score.score_list()) {
        EXPECT_FALSE(copy->first_measure()->repeat_start());
        EXPECT_FALSE(copy->tick2measure(Tick(1, 1))->repeat_end());
        EXPECT_EQ(copy->tick2measure(Tick(1, 1))->repeat_count(), 2);
    }
    EXPECT_EQ(traversal(score), (std::vector{0, 1, 2}));
    const auto reject_crossing = [&](const AddVolta &volta) {
        const auto result = document->edit({{volta}}, {.mode = EditMode::Prefix});
        ASSERT_TRUE(result) << result.error().message;
        ASSERT_TRUE(result->rejection);
        EXPECT_EQ(result->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_FALSE(result->rejection->error.poisoned);
        EXPECT_EQ(result->applied, 0u);
        for (auto *copy : score.score_list()) EXPECT_TRUE(copy->spanner_list().empty());
    };
    apply({SetRepeatBarlines{.measure = bars[1], .start = true}});
    reject_crossing(AddVolta{bars[0], bars[1], {1}, false});
    apply({SetRepeatBarlines{.measure = bars[1], .start = false, .end = true}, SetRepeatBarlines{.measure = bars[0], .start = true}});
    reject_crossing(AddVolta{bars[0], bars[2], {1}});
    apply({AddVolta{bars[1], bars[1], {1}}, AddVolta{bars[2], bars[2], {2}, false}});
    auto check = [&](mu::engraving::MasterScore &master) {
        EXPECT_EQ(traversal(master), (std::vector{0, 1, 0, 2}));
        for (auto *copy : master.score_list()) {
            const auto spans = copy->spanner_list();
            ASSERT_EQ(spans.size(), 2u);
            for (std::size_t i = 0; i < spans.size(); ++i) {
                ASSERT_TRUE(spans[i]->is_volta());
                auto *volta = mu::engraving::to_volta(spans[i]);
                const auto pass = static_cast<int>(i + 1);
                EXPECT_EQ(volta->tick(), Tick(pass, 1));
                EXPECT_EQ(volta->tick2(), Tick(pass + 1, 1));
                EXPECT_EQ(volta->endings(), std::vector{pass});
                EXPECT_EQ(volta->text().to_std_string(), std::to_string(pass) + ".");
                EXPECT_EQ(volta->volta_type(), i == 0 ? mu::engraving::Volta::Type::CLOSED : mu::engraving::Volta::Type::OPEN);
            }
        }
    };
    check(score);
    auto saved = document->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved) << saved.error().message;
    auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    check(harness::master_of(**reopened));
}

TEST(NotationEdits, GraceChordsPreserveWrittenSpellingAndOrderOnBothSides) {
    using namespace engrave;
    using namespace mu::engraving;
    using Tick = mu::engraving::Fraction;
    constexpr auto clarinet = R"(<Part id="1"><Staff/><trackName>Clarinet</trackName><Instrument id="clarinet">
<instrumentId>wind.reed.clarinet.bflat</instrumentId><transposeDiatonic>-1</transposeDiatonic><transposeChromatic>-2</transposeChromatic>
<Channel><program value="71"/></Channel></Instrument></Part>)";
    auto document = notation_score(1, false, clarinet);
    ASSERT_TRUE(document);
    const auto bar = harness::find_id(*document, "Measure");
    const auto part = harness::find_id(*document, "Part");
    const auto at = SegmentAddress{bar, 0, 0, {0, 1}, "ChordRest"};
    const auto result = document->edit({{
        GenerateExcerpts{{part}},
        EnterNotes{at, {1, 4}, {{60}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Acciaccatura, {1, 16}, {{61, TPC_D_B}, {66, TPC_F_S}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Grace8After, {1, 8}, {{62}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Appoggiatura, {1, 8}, {{63}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Grace16After, {1, 16}, {{64}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Grace4, {1, 4}, {{65}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Grace32After, {1, 32}, {{67}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Grace16, {1, 16}, {{68}}}},
        AddGraceChord{OpRef{1, 0}, {GraceKind::Grace32, {1, 32}, {{69}}}},
    }});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    const auto &created = result->ops[2].created;
    ASSERT_EQ(created.size(), 3u);
    ASSERT_TRUE(created[0] && created[1] && created[2]);
    EXPECT_TRUE(harness::element(*document, *created[0]).is_chord());
    EXPECT_EQ(to_note(&harness::element(*document, *created[1]))->pitch(), 61);
    EXPECT_EQ(to_note(&harness::element(*document, *created[2]))->pitch(), 66);

    auto check = [](MasterScore &master) {
        ASSERT_EQ(master.score_list().size(), 2u);
        for (auto *score : master.score_list()) {
            auto *element = score->find_cr(Tick(0, 1), 0);
            ASSERT_TRUE(element && element->is_chord());
            const auto *chord = to_chord(element);
            EXPECT_EQ(chord->ticks(), Tick(1, 4));
            const auto before = chord->grace_notes_before();
            const auto after = chord->grace_notes_after();
            ASSERT_EQ(before.size(), 5u);
            ASSERT_EQ(after.size(), 3u);

            const std::vector<NoteType> before_types{NoteType::ACCIACCATURA, NoteType::APPOGGIATURA, NoteType::GRACE4, NoteType::GRACE16,
                                                     NoteType::GRACE32};
            const std::vector<NoteType> after_types{NoteType::GRACE8_AFTER, NoteType::GRACE16_AFTER, NoteType::GRACE32_AFTER};
            for (std::size_t i = 0; i < before.size(); ++i) EXPECT_EQ(before.at(i)->note_type(), before_types[i]);
            for (std::size_t i = 0; i < after.size(); ++i) EXPECT_EQ(after.at(i)->note_type(), after_types[i]);
            ASSERT_EQ(before.at(0)->notes().size(), 2u);
            const auto *flat = before.at(0)->find_note(61);
            const auto *sharp = before.at(0)->find_note(66);
            ASSERT_TRUE(flat && sharp);
            EXPECT_EQ(flat->tpc1(), TPC_D_B);
            EXPECT_EQ(flat->tpc2(), TPC_E_B);
            EXPECT_EQ(sharp->tpc1(), TPC_F_S);
            EXPECT_EQ(sharp->tpc2(), TPC_G_S);
            EXPECT_EQ(before.at(0)->ticks(), Tick(1, 16));
            EXPECT_EQ(before.at(1)->up_note()->pitch(), 63);
            EXPECT_EQ(after.at(0)->up_note()->pitch(), 62);
            EXPECT_EQ(after.at(1)->up_note()->pitch(), 64);
            EXPECT_EQ(after.at(2)->up_note()->pitch(), 67);
            EXPECT_EQ(after.at(2)->ticks(), Tick(1, 32));
        }
    };
    check(harness::master_of(*document));
    const auto saved = document->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    check(harness::master_of(**reopened));

    const auto revision = document->revision();
    const auto wrong_duration = document->edit({{AddGraceChord{*result->ops[1].created[0], {GraceKind::Grace16, {1, 8}, {{60}}}}}});
    ASSERT_FALSE(wrong_duration);
    EXPECT_EQ(wrong_duration.error().code, ErrorCode::INVALID_ARGUMENT);
    const auto nested = document->edit({{AddGraceChord{*created[0], {GraceKind::Appoggiatura, {1, 8}, {{60}}}}}});
    ASSERT_TRUE(nested);
    ASSERT_TRUE(nested->rejection);
    EXPECT_EQ(nested->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    const auto leftover_rest = harness::find_id(*document, "Rest");
    const auto on_rest = document->edit({{AddGraceChord{leftover_rest, {GraceKind::Appoggiatura, {1, 8}, {{60}}}}}});
    ASSERT_TRUE(on_rest);
    ASSERT_TRUE(on_rest->rejection);
    EXPECT_EQ(on_rest->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(document->revision(), revision);
}

std::unique_ptr<engrave::Document> drum_score(std::string_view drums = {}) {
    std::string part = R"(<Part id="1"><Staff><StaffType group="percussion"><lines>5</lines></StaffType></Staff>
<trackName>Drums</trackName><Instrument id="drumset"><useDrumset>1</useDrumset>)";
    part += drums;
    part += "<Channel><program value=\"0\"/></Channel></Instrument></Part>";
    return notation_score(2, false, part);
}

TEST(NotationEdits, DrumEntryRejectsCrossingATupletBoundaryBeforeChangingTheScore) {
    using namespace engrave;
    using Tick = mu::engraving::Fraction;
    auto document = drum_score();
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto bar = harness::find_id(*document, "Measure");
    const auto start = SegmentAddress{bar, 0, 0, {0, 1}, "ChordRest"};
    const auto inside = SegmentAddress{bar, 0, 0, {1, 4}, "ChordRest"};
    const auto setup = document->edit({{EnterDrums{inside, {1, 4}, {{36}}}, AddTuplet{OpRef{0, 0}, OpRef{0, 0}, 3, 2}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
    const auto *ordinary = score.find_cr(Tick(0, 1), 0);
    ASSERT_TRUE(ordinary && ordinary->is_rest());
    ASSERT_EQ(ordinary->tuplet(), nullptr);
    ASSERT_EQ(ordinary->end_tick(), Tick(1, 4));
    const auto *tupleted = score.find_cr(Tick(1, 4), 0);
    ASSERT_TRUE(tupleted && tupleted->tuplet());
    const auto before = exported(*document, Format::MSCX);
    for (const auto &entry : {EnterDrums{start, {3, 8}, {{38}}}, EnterDrums{inside, {1, 2}, {{38}}}}) {
        const auto result = document->edit({{entry}}, {.mode = EditMode::Prefix});
        ASSERT_TRUE(result) << result.error().message;
        ASSERT_TRUE(result->rejection);
        EXPECT_EQ(result->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
        EXPECT_EQ(result->applied, 0u);
        EXPECT_EQ(exported(*document, Format::MSCX), before);
    }
    const auto result = document->edit({{EnterDrums{start, {1, 4}, {{38}}}}});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    EXPECT_EQ(score.find_cr(Tick(1, 4), 0), tupleted);
}

TEST(NotationEdits, DrumEntryUsesKitAndRequestedVoice) {
    using namespace engrave;
    using namespace mu::engraving;
    using Tick = mu::engraving::Fraction;
    auto document = drum_score();
    ASSERT_TRUE(document);
    auto &score = harness::master_of(*document);
    const auto bar = harness::find_id(*document, "Measure");
    const auto at = SegmentAddress{bar, 0, 0, {1, 4}, "ChordRest"};
    const auto result = document->edit({{EnterDrums{at, {1, 4}, {{36}, {42}, {38}}}}});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    const auto *leading = score.find_cr(Tick(0, 1), 0);
    ASSERT_TRUE(leading && leading->is_rest());
    EXPECT_EQ(leading->end_tick(), Tick(1, 4));
    const auto *entry = score.find_cr(Tick(1, 4), 0);
    ASSERT_TRUE(entry && entry->is_chord());
    const auto *chord = to_chord(entry);
    ASSERT_EQ(chord->notes().size(), 3u);
    EXPECT_EQ(chord->stem_direction(), DirectionV::DOWN);
    const std::vector<std::tuple<int, int, NoteHeadGroup>> mapping{
        {36, 7, NoteHeadGroup::HEAD_NORMAL}, {42, -1, NoteHeadGroup::HEAD_CROSS}, {38, 3, NoteHeadGroup::HEAD_NORMAL}};
    for (const auto &[key, line, head] : mapping) {
        const auto *note = chord->find_note(key);
        ASSERT_NE(note, nullptr);
        EXPECT_EQ(note->line(), line);
        EXPECT_EQ(note->head_group(), head);
    }

    const auto revision = document->revision();
    const auto out_of_range = document->edit({{EnterDrums{at, {1, 4}, {{128}}}}});
    ASSERT_FALSE(out_of_range);
    EXPECT_EQ(out_of_range.error().code, ErrorCode::INVALID_ARGUMENT);
    for (const auto &invalid : {EnterDrums{at, {1, 4}, {{127}}}, EnterDrums{at, {1, 1}, {{36}}}}) {
        const auto result = document->edit({{invalid}});
        ASSERT_TRUE(result);
        ASSERT_TRUE(result->rejection);
        EXPECT_EQ(result->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
    }
    EXPECT_EQ(document->revision(), revision);
    const auto replace = document->edit({{EnterDrums{at, {1, 4}, {}}}});
    ASSERT_TRUE(replace);
    ASSERT_FALSE(replace->rejection) << replace->rejection->error.message;
    auto *replaced = score.find_cr(Tick(1, 4), 0);
    ASSERT_TRUE(replaced && replaced->is_rest());

    auto pitched = notation_score(1);
    ASSERT_TRUE(pitched);
    const auto pitched_bar = harness::find_id(*pitched, "Measure");
    const auto wrong_staff = pitched->edit({{EnterDrums{SegmentAddress{pitched_bar, 0, 0, {0, 1}, "ChordRest"}, {1, 4}, {{36}}}}});
    ASSERT_TRUE(wrong_staff);
    ASSERT_TRUE(wrong_staff->rejection);
    EXPECT_EQ(wrong_staff->rejection->error.code, ErrorCode::INVALID_ARGUMENT);
}

TEST(NotationEdits, CustomDrumkitMapsKeysAfterSaveAndReopen) {
    using namespace engrave;
    using namespace mu::engraving;
    using Tick = mu::engraving::Fraction;
    auto document =
        drum_score(R"(<Drum pitch="36"><name>High drum</name><head>diamond</head><line>-4</line><stem>1</stem><voice>0</voice></Drum>
<Drum pitch="72"><name>Low drum</name><head>cross</head><line>10</line><stem>2</stem><voice>1</voice></Drum>)");
    ASSERT_TRUE(document);
    const auto bar = harness::find_id(*document, "Measure");
    const auto result = document->edit({{EnterDrums{SegmentAddress{bar, 0, 2, {1, 2}, "ChordRest"}, {1, 4}, {{36}, {72}}}}});
    ASSERT_TRUE(result) << result.error().message;
    ASSERT_FALSE(result->rejection) << result->rejection->error.message;
    auto check = [](MasterScore &score) {
        const auto *piece = score.find_cr(Tick(1, 2), 2);
        ASSERT_TRUE(piece && piece->is_chord());
        EXPECT_EQ(piece->track(), 2u);
        const auto *chord = to_chord(piece);
        ASSERT_EQ(chord->notes().size(), 2u);
        EXPECT_EQ(chord->stem_direction(), DirectionV::UP);
        const auto *high = chord->find_note(36);
        const auto *low = chord->find_note(72);
        ASSERT_TRUE(high && low);
        EXPECT_EQ(high->head_group(), NoteHeadGroup::HEAD_DIAMOND);
        EXPECT_EQ(high->line(), -4);
        EXPECT_EQ(low->head_group(), NoteHeadGroup::HEAD_CROSS);
        EXPECT_EQ(low->line(), 10);
    };
    check(harness::master_of(*document));
    const auto saved = document->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved);
    auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    check(harness::master_of(**reopened));
}

TEST(JsonHelpers, ParseMatchesTheOldWireRules) {
    using music::Json;
    const auto value = music::parse_json<Json>(R"({"a":1,"b":[true,null]})");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->at("a").as<int>(), 1);
    EXPECT_FALSE(music::parse_json<Json>(R"({"a":1} garbage)"));
    EXPECT_FALSE(music::parse_json<Json>(R"({"a":1}{"b":2})"));
    EXPECT_TRUE(music::parse_json<Json>("{\"a\":1} \n\t"));
    const auto bom_value = music::parse_json<Json>("\xEF\xBB\xBF{\"a\":1}");
    ASSERT_TRUE(bom_value);
    EXPECT_TRUE(music::json_equal(*bom_value, Json{{"a", 1}}));
    EXPECT_FALSE(music::parse_json<Json>("\xEF\xBB\xBF"));
}

TEST(JsonHelpers, OrderedEqualityComparesMembersInPosition) {
    const music::Json ordered_a{{"x", 1}, {"y", 2}};
    const music::Json ordered_b{{"y", 2}, {"x", 1}};
    EXPECT_FALSE(music::json_equal(ordered_a, ordered_b));
    const music::SortedJson sorted_a{{"x", 1}, {"y", 2}};
    const music::SortedJson sorted_b{{"y", 2}, {"x", 1}};
    EXPECT_TRUE(music::json_equal(sorted_a, sorted_b));
}

} // namespace

TEST(Open, UnicodeFilenameLyricsAndNonBmpRoundTrip) {
    const std::string text = "\xe9\x9f\xb3\xe6\xa5\xbd \xf0\x9d\x84\x9e \xf0\x9f\x98\x80";
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?><score-partwise version=\"4.0\">"
                            "<part-list><score-part id=\"P1\"><part-name>Piano</part-name></score-part></part-list>"
                            "<part id=\"P1\"><measure number=\"1\"><attributes><divisions>1</divisions>"
                            "<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef>"
                            "</attributes><note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration>"
                            "<type>whole</type><lyric><text>" +
                            text + "</text></lyric></note></measure></part></score-partwise>";
    const auto path = std::filesystem::current_path() / std::filesystem::u8path(text + ".musicxml");

    struct Cleanup {
        std::filesystem::path p;

        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    } cleanup{path};

    {
        std::ofstream out(path, std::ios::binary);
        out << xml;
        ASSERT_TRUE(out.good());
    }
    auto document = g_engine->open(path);
    ASSERT_TRUE(document) << document.error().message;
    const auto saved = (*document)->export_score({.format = Format::MSCZ});
    ASSERT_TRUE(saved) << saved.error().message;
    auto reopened = g_engine->open(*saved);
    ASSERT_TRUE(reopened) << reopened.error().message;
    EXPECT_NE(exported(**reopened, Format::MUSIC_XML).find(text), std::string::npos);
    const auto render = (*reopened)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(render) << render.error().message;
}

TEST(Export, LegacyBendPitchLabelsAndExtremeCurvesRenderAndExport) {
    using mu::engraving::Bend;
    EXPECT_EQ(Bend::pitch_label(0).to_std_string(), "");
    EXPECT_EQ(Bend::pitch_label(12).to_std_string(), "");
    EXPECT_EQ(Bend::pitch_label(13).to_std_string(), "1/4");
    EXPECT_EQ(Bend::pitch_label(25).to_std_string(), "1/4");
    EXPECT_EQ(Bend::pitch_label(50).to_std_string(), "1/2");
    EXPECT_EQ(Bend::pitch_label(75).to_std_string(), "3/4");
    EXPECT_EQ(Bend::pitch_label(100).to_std_string(), "full");
    EXPECT_EQ(Bend::pitch_label(300).to_std_string(), "3");
    EXPECT_EQ(Bend::pitch_label(312).to_std_string(), "3");
    EXPECT_EQ(Bend::pitch_label(313).to_std_string(), "3 1/4");
    EXPECT_EQ(Bend::pitch_label(386).to_std_string(), "3 3/4");
    EXPECT_EQ(Bend::pitch_label(-12).to_std_string(), "");
    EXPECT_EQ(Bend::pitch_label(-13).to_std_string(), "-1/4");
    EXPECT_EQ(Bend::pitch_label(-25).to_std_string(), "-1/4");
    EXPECT_EQ(Bend::pitch_label(-100).to_std_string(), "-full");
    EXPECT_EQ(Bend::pitch_label(-1090).to_std_string(), "-11");
    EXPECT_EQ(Bend::pitch_label(std::numeric_limits<int>::min()).to_std_string(), "-21474836 1/2");
    EXPECT_EQ(Bend::pitch_label(std::numeric_limits<int>::max()).to_std_string(), "21474836 1/2");

    constexpr std::string_view mscx = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Division>480</Division>
    <Part id="1">
      <Staff id="1"><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <trackName>Voice</trackName>
      <Instrument id="voice"><trackName>Voice</trackName><Channel/></Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Chord>
            <durationType>quarter</durationType>
            <Note>
              <pitch>64</pitch><tpc>18</tpc>
              <Bend>
                <point time="0" pitch="-1090" vibrato="0"/>
                <point time="30" pitch="-100" vibrato="0"/>
                <point time="60" pitch="0" vibrato="0"/>
              </Bend>
            </Note>
          </Chord>
          <Chord>
            <durationType>quarter</durationType>
            <Note>
              <pitch>67</pitch><tpc>15</tpc>
              <Bend>
                <point time="0" pitch="0" vibrato="0"/>
                <point time="30" pitch="386" vibrato="0"/>
                <point time="60" pitch="386" vibrato="0"/>
              </Bend>
            </Note>
          </Chord>
          <Chord>
            <durationType>quarter</durationType>
            <Note>
              <pitch>69</pitch><tpc>17</tpc>
              <Bend/>
            </Note>
          </Chord>
          <Chord>
            <durationType>quarter</durationType>
            <Note>
              <pitch>71</pitch><tpc>19</tpc>
              <Bend>
                <point time="0" pitch="-200" vibrato="0"/>
              </Bend>
            </Note>
          </Chord>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>
)xml";

    const std::vector<std::byte> bytes(reinterpret_cast<const std::byte *>(mscx.data()),
                                       reinterpret_cast<const std::byte *>(mscx.data() + mscx.size()));
    auto document = g_engine->open(bytes, {.format = Format::MSCX});
    ASSERT_TRUE(document) << document.error().message;

    const auto svg = (*document)->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(svg) << svg.error().message;
    ASSERT_FALSE(svg->pages.empty());

    const auto pdf = (*document)->render({.format = engrave::RenderFormat::PDF});
    ASSERT_TRUE(pdf) << pdf.error().message;
    EXPECT_FALSE(pdf->document.empty());

    const auto mxl = (*document)->export_score({.format = Format::MUSIC_XML});
    ASSERT_TRUE(mxl) << mxl.error().message;
    EXPECT_FALSE(mxl->empty());

    const auto midi = (*document)->export_score({.format = Format::MIDI});
    ASSERT_TRUE(midi) << midi.error().message;
    EXPECT_FALSE(midi->empty());
}
