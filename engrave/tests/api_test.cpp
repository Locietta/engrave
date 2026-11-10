// Tests of the public engrave API.
//
//   engrave_api_test --assets=<resources> [gtest options]
//
// One Engine serves the whole process, as the API requires; the death tests
// re-run main in a child process, which creates its own.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/sniff.h"
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

std::filesystem::path g_resources;
std::unique_ptr<engrave::Engine> g_engine;

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

std::string midi_principal_chord(std::string_view duration, const std::vector<int> &pitches, int dots = 0, bool arpeggio = false) {
    // <dots> must precede <durationType>: the reader fixes the chord's length when it reads durationType
    std::string chord = "<Chord>";
    if (dots) chord += "<dots>" + std::to_string(dots) + "</dots>";
    chord += "<durationType>" + std::string(duration) + "</durationType>";
    for (int pitch : pitches) chord += "<Note><pitch>" + std::to_string(pitch) + "</pitch></Note>";
    if (arpeggio) chord += "<Arpeggio><subtype>0</subtype></Arpeggio>";
    return chord + "</Chord>";
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
        std::map<int, size_t> active;
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

void expect_midi_note(const ExportedMidiNote &note, int pitch, int on, int off) {
    EXPECT_EQ(note.pitch, pitch);
    EXPECT_EQ(note.on, on);
    EXPECT_EQ(note.off, off);
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
    std::set<std::string> ids;
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

TEST(Export, GraceGroupsShareHalfWithoutChangingSingleGroupTiming) {
    const auto grace_before = midi_grace_chord("appoggiatura", "eighth", 67);
    const auto grace_after = midi_grace_chord("grace8after", "eighth", 69);
    const auto principal = midi_principal_chord("quarter", {65});
    const auto both = exported_midi_notes(grace_before + grace_after + principal);
    ASSERT_EQ(both.size(), 3u);
    expect_midi_note(both[0], 67, 0, 119);   // appoggiatura before: 120 ticks
    expect_midi_note(both[1], 65, 120, 359); // principal: a positive 240 ticks
    expect_midi_note(both[2], 69, 360, 479); // appoggiatura after: 120 ticks
    EXPECT_GT(both[1].off - both[1].on + 1, 0);
    EXPECT_EQ((both[0].off - both[0].on + 1) + (both[2].off - both[2].on + 1), 240);

    // Single groups: the ticks of the unchanged legacy code
    const auto before = exported_midi_notes(grace_before + principal);
    ASSERT_EQ(before.size(), 2u);
    expect_midi_note(before[0], 67, 0, 239);
    expect_midi_note(before[1], 65, 240, 479);

    const auto after = exported_midi_notes(grace_after + principal);
    ASSERT_EQ(after.size(), 2u);
    expect_midi_note(after[0], 65, 0, 239);
    expect_midi_note(after[1], 69, 240, 479);
}

TEST(Export, GraceGroupsUseAudioNotatedDurationRules) {
    // Short graces keep their notated length: sixteenths on a quarter take 120 ticks each side, not a fixed half
    const auto short_before = midi_grace_chord("appoggiatura", "16th", 67);
    const auto short_after = midi_grace_chord("grace16after", "16th", 69);
    const auto quarter = midi_principal_chord("quarter", {65});
    const auto short_graces = exported_midi_notes(short_before + short_after + quarter);
    ASSERT_EQ(short_graces.size(), 3u);
    expect_midi_note(short_graces[0], 67, 0, 119);
    expect_midi_note(short_graces[1], 65, 120, 359);
    expect_midi_note(short_graces[2], 69, 360, 479);

    // Single sixteenth groups: legacy ticks
    const auto short_before_only = exported_midi_notes(short_before + quarter);
    ASSERT_EQ(short_before_only.size(), 2u);
    expect_midi_note(short_before_only[0], 67, 0, 119);
    expect_midi_note(short_before_only[1], 65, 120, 479);
    const auto short_after_only = exported_midi_notes(short_after + quarter);
    ASSERT_EQ(short_after_only.size(), 2u);
    expect_midi_note(short_after_only[0], 65, 0, 239);
    expect_midi_note(short_after_only[1], 69, 240, 479);

    // Dotted quarter (720 ticks): eighths want 240 each, over half (360), so both shrink to 180
    const auto eighth_before = midi_grace_chord("appoggiatura", "eighth", 67);
    const auto eighth_after = midi_grace_chord("grace8after", "eighth", 69);
    const auto dotted_quarter = midi_principal_chord("quarter", {65}, 1);
    const auto dotted = exported_midi_notes(eighth_before + eighth_after + dotted_quarter);
    ASSERT_EQ(dotted.size(), 3u);
    expect_midi_note(dotted[0], 67, 0, 179);
    expect_midi_note(dotted[1], 65, 180, 539);
    expect_midi_note(dotted[2], 69, 540, 719);

    // Dotted sixteenths fit within half: they keep their 120 ticks and the principal keeps the rest
    const auto dotted_short = exported_midi_notes(short_before + short_after + dotted_quarter);
    ASSERT_EQ(dotted_short.size(), 3u);
    expect_midi_note(dotted_short[0], 67, 0, 119);
    expect_midi_note(dotted_short[1], 65, 120, 599);
    expect_midi_note(dotted_short[2], 69, 600, 719);

    // Dotted principal, single groups: legacy ticks
    const auto dotted_before_only = exported_midi_notes(eighth_before + dotted_quarter);
    ASSERT_EQ(dotted_before_only.size(), 2u);
    expect_midi_note(dotted_before_only[0], 67, 0, 238);
    expect_midi_note(dotted_before_only[1], 65, 239, 718);
    const auto dotted_after_only = exported_midi_notes(eighth_after + dotted_quarter);
    ASSERT_EQ(dotted_after_only.size(), 2u);
    expect_midi_note(dotted_after_only[0], 65, 0, 238);
    expect_midi_note(dotted_after_only[1], 69, 239, 718);

    // An acciaccatura before takes half a demisemiquaver (30 ticks) and the after-group the rest of the shared half:
    // 30 + 240 > 240, so they shrink to 27 and 213. It no longer exports at zero length. Here the chord starts the
    // measure; mid-score the exporter still moves acciaccatura events before the beat (see the exporter's grace offset).
    const auto acciaccatura = exported_midi_notes(midi_grace_chord("acciaccatura", "eighth", 67) + eighth_after + quarter);
    ASSERT_EQ(acciaccatura.size(), 3u);
    expect_midi_note(acciaccatura[0], 67, 0, 26);
    expect_midi_note(acciaccatura[1], 65, 27, 266);
    expect_midi_note(acciaccatura[2], 69, 267, 479);

    // Acciaccatura alone: legacy, zero length (played before the beat)
    const auto acciaccatura_only = exported_midi_notes(midi_grace_chord("acciaccatura", "eighth", 67) + quarter);
    ASSERT_EQ(acciaccatura_only.size(), 2u);
    expect_midi_note(acciaccatura_only[0], 65, 0, 479);
    expect_midi_note(acciaccatura_only[1], 67, 0, 0);

    // The appoggiatura limit is two thirds of a dotted quarter in 6/8, half of it in 4/4: with a quarter after-grace
    // the shares differ (120/240 against 144/216)
    const auto quarter_after = midi_grace_chord("grace8after", "quarter", 69);
    const auto compound = exported_midi_notes(eighth_before + quarter_after + dotted_quarter, {.beats = 6, .beat_unit = 8});
    ASSERT_EQ(compound.size(), 3u);
    expect_midi_note(compound[0], 67, 0, 119);
    expect_midi_note(compound[1], 65, 120, 479);
    expect_midi_note(compound[2], 69, 480, 719);
    const auto simple = exported_midi_notes(eighth_before + quarter_after + dotted_quarter);
    ASSERT_EQ(simple.size(), 3u);
    expect_midi_note(simple[0], 67, 0, 143);
    expect_midi_note(simple[1], 65, 144, 503);
    expect_midi_note(simple[2], 69, 504, 719);
}

TEST(Export, GraceGroupsUseCumulativeMidiTickBoundaries) {
    const auto before_group = [] {
        std::string chords;
        for (int pitch : {67, 68, 70}) chords += midi_grace_chord("grace16", "16th", pitch);
        return chords;
    }();
    const auto after_group = [] {
        std::string chords;
        for (int pitch : {71, 72, 74}) chords += midi_grace_chord("grace16after", "16th", pitch);
        return chords;
    }();
    const auto quarter = midi_principal_chord("quarter", {65});

    // Three graces each side: budgets 90 and 240 shrink to 65 and 175. Every boundary is a whole tick, so the graces
    // and the principal are adjacent (the old per-grace integer division left 3 ticks between the last grace and the
    // principal)
    const auto notes = exported_midi_notes(before_group + after_group + quarter);
    ASSERT_EQ(notes.size(), 7u);
    expect_midi_note(notes[0], 67, 0, 21);
    expect_midi_note(notes[1], 68, 22, 42);
    expect_midi_note(notes[2], 70, 43, 64);
    expect_midi_note(notes[3], 65, 65, 304);
    expect_midi_note(notes[4], 74, 305, 362); // grace_notes_after lists the after-group in reverse order
    expect_midi_note(notes[5], 72, 363, 421);
    expect_midi_note(notes[6], 71, 422, 479);
    for (size_t i = 1; i < notes.size(); ++i) {
        EXPECT_EQ(notes[i].on, notes[i - 1].off + 1) << "note " << i;
    }

    // Single groups: legacy ticks, including the legacy rounding gaps
    const auto before_only = exported_midi_notes(before_group + quarter);
    ASSERT_EQ(before_only.size(), 4u);
    expect_midi_note(before_only[0], 67, 0, 38);
    expect_midi_note(before_only[1], 68, 39, 77);
    expect_midi_note(before_only[2], 70, 79, 117);
    expect_midi_note(before_only[3], 65, 120, 479);
    const auto after_only = exported_midi_notes(after_group + quarter);
    ASSERT_EQ(after_only.size(), 4u);
    expect_midi_note(after_only[0], 65, 0, 239);
    expect_midi_note(after_only[1], 74, 240, 318);
    expect_midi_note(after_only[2], 72, 319, 397);
    expect_midi_note(after_only[3], 71, 399, 477);

    // A dotted half is longer than 1000 ticks; here the budgets still land on whole ticks
    const auto dotted_half = exported_midi_notes(before_group + midi_grace_chord("grace16after", "16th", 71) +
                                                 midi_grace_chord("grace16after", "16th", 72) + midi_principal_chord("half", {65}, 1));
    ASSERT_EQ(dotted_half.size(), 6u);
    expect_midi_note(dotted_half[0], 67, 0, 29);
    expect_midi_note(dotted_half[1], 68, 30, 59);
    expect_midi_note(dotted_half[2], 70, 60, 89);
    expect_midi_note(dotted_half[3], 65, 90, 1199);
    expect_midi_note(dotted_half[4], 72, 1200, 1319);
    expect_midi_note(dotted_half[5], 71, 1320, 1439);
}

TEST(Export, GraceGroupsShortenArpeggioAtAfterBoundary) {
    const auto before = midi_grace_chord("appoggiatura", "eighth", 71);
    const auto after = midi_grace_chord("grace8after", "eighth", 72);
    const auto arpeggio = midi_principal_chord("quarter", {60, 64, 67}, 0, true);

    // Every arpeggiated note stops where the after-group begins (the old end ran to 478, over the after-grace)
    const auto both = exported_midi_notes(before + after + arpeggio);
    ASSERT_EQ(both.size(), 5u);
    expect_midi_note(both[0], 71, 0, 119);
    expect_midi_note(both[1], 60, 120, 359);
    expect_midi_note(both[2], 64, 183, 359);
    expect_midi_note(both[3], 67, 247, 359);
    expect_midi_note(both[4], 72, 360, 479);

    // Single groups: legacy ticks
    const auto before_only = exported_midi_notes(before + arpeggio);
    ASSERT_EQ(before_only.size(), 4u);
    expect_midi_note(before_only[0], 71, 0, 239);
    expect_midi_note(before_only[1], 60, 240, 479);
    expect_midi_note(before_only[2], 64, 303, 478);
    expect_midi_note(before_only[3], 67, 367, 478);
    const auto after_only = exported_midi_notes(after + arpeggio);
    ASSERT_EQ(after_only.size(), 4u);
    expect_midi_note(after_only[0], 60, 0, 479);
    expect_midi_note(after_only[1], 64, 63, 478);
    expect_midi_note(after_only[2], 67, 127, 478);
    expect_midi_note(after_only[3], 72, 240, 479);
}

TEST(Export, SwingKeepsGraceBudgetsSeparate) {
    const auto eighth_before = midi_grace_chord("appoggiatura", "eighth", 67);
    const auto eighth_after = midi_grace_chord("grace8after", "eighth", 69);
    const auto first = midi_principal_chord("eighth", {60});
    const auto second = midi_principal_chord("eighth", {65});

    // After-group only on a swung off-beat eighth: the swing shift must not remove the principal (it did when the
    // whole of ontime was subtracted whenever an after-group existed). Legacy ticks.
    const auto after_only = exported_midi_notes(first + eighth_after + second, {.swing = true});
    ASSERT_EQ(after_only.size(), 3u);
    expect_midi_note(after_only[0], 60, 0, 359);
    expect_midi_note(after_only[1], 65, 360, 419);
    expect_midi_note(after_only[2], 69, 360, 479);

    // Before-group only: legacy ticks (the swing shift adds to the before-group's budget, pushing the principal out)
    const auto before_only = exported_midi_notes(first + eighth_before + second, {.swing = true});
    ASSERT_EQ(before_only.size(), 3u);
    expect_midi_note(before_only[0], 60, 0, 359);
    expect_midi_note(before_only[1], 67, 240, 359);
    expect_midi_note(before_only[2], 65, 480, 539);

    // Both groups: each takes 60 of the 120 ticks they share, leaving the principal a 120-tick span (60..179). As in
    // playback, swing acts on the principal alone, inside that span: the 500 per mille shift moves its start by half
    // the span (60 ticks) and the swing gate time halves its length, so it ends at 419, strictly between the groups.
    // The grace groups themselves are not swung (the legacy exporter never swung them either).
    const auto both = exported_midi_notes(first + eighth_before + eighth_after + second, {.swing = true});
    ASSERT_EQ(both.size(), 4u);
    expect_midi_note(both[0], 60, 0, 359);
    expect_midi_note(both[1], 67, 240, 299);
    expect_midi_note(both[2], 65, 360, 419);
    expect_midi_note(both[3], 69, 420, 479);
    EXPECT_GT(both[2].on, both[1].off);
    EXPECT_LT(both[2].off, both[3].on);
}

TEST(Export, GraceGroupsStayOnTheBeatMidScore) {
    const auto previous = midi_principal_chord("quarter", {60});
    const auto quarter = midi_principal_chord("quarter", {65});
    const auto acciaccatura = midi_grace_chord("acciaccatura", "eighth", 67);
    const auto after = midi_grace_chord("grace8after", "eighth", 69);

    // Both groups: the acciaccatura is on the beat (480) with its 27-tick share, the principal and after-group follow
    // and the previous chord keeps its full length. Nothing moves before the beat.
    const auto both = exported_midi_notes(previous + acciaccatura + after + quarter);
    ASSERT_EQ(both.size(), 4u);
    expect_midi_note(both[0], 60, 0, 479);
    expect_midi_note(both[1], 67, 480, 506);
    expect_midi_note(both[2], 65, 507, 746);
    expect_midi_note(both[3], 69, 747, 959);

    // Acciaccatura alone keeps its pre-beat placement: the previous chord is cut short at 239 and the zero-length
    // acciaccatura sounds across 240..479
    const auto only = exported_midi_notes(previous + acciaccatura + quarter);
    ASSERT_EQ(only.size(), 3u);
    expect_midi_note(only[0], 60, 0, 239);
    expect_midi_note(only[1], 67, 240, 479);
    expect_midi_note(only[2], 65, 480, 959);
}

TEST(Export, DrumFlamsKeepLegacyTimingWithAfterGraces) {
    const auto flam_before = midi_grace_chord("appoggiatura", "eighth", 36);
    const auto flam_after = midi_grace_chord("grace8after", "eighth", 40);
    const auto principal = midi_principal_chord("quarter", {38});

    // The 15 ms flam path never enters the grouped scaling: with both groups the ticks stay legacy
    const auto both = exported_midi_notes(flam_before + flam_after + principal, {.drumset = true});
    ASSERT_EQ(both.size(), 3u);
    expect_midi_note(both[0], 36, 0, 13);
    expect_midi_note(both[1], 38, 14, 373);
    expect_midi_note(both[2], 40, 360, 479);

    const auto before_only = exported_midi_notes(flam_before + principal, {.drumset = true});
    ASSERT_EQ(before_only.size(), 2u);
    expect_midi_note(before_only[0], 36, 0, 13);
    expect_midi_note(before_only[1], 38, 14, 493);

    const auto after_only = exported_midi_notes(flam_after + principal, {.drumset = true});
    ASSERT_EQ(after_only.size(), 2u);
    expect_midi_note(after_only[0], 38, 0, 239);
    expect_midi_note(after_only[1], 40, 240, 479);
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

TEST(Contract, SecondEngineAborts) {
    EXPECT_DEATH(std::ignore = engrave::Engine::create({.resources = g_resources}), "one Engine per process");
}

TEST(Contract, CallFromAnotherThreadAborts) {
    EXPECT_DEATH(std::thread([] { std::ignore = g_engine->templates(); }).join(), "thread other than");
}

TEST(Contract, EngineDestroyedBeforeItsDocumentsAborts) {
    EXPECT_DEATH(
        {
            auto document = treble_clef();
            g_engine.reset();
        },
        "while Documents were open");
}

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg.starts_with("--assets=")) g_resources = std::filesystem::path(arg.substr(9));
    }
    // Failure before binding services must leave startup retryable.
    auto missing = engrave::Engine::create({.resources = g_resources / "c5-missing-resources"});
    if (missing || missing.error().code != ErrorCode::NOT_FOUND) {
        std::fprintf(stderr, "C5 startup failure did not leave services unbound\n");
        return 1;
    }
    auto engine = engrave::Engine::create({.resources = g_resources});
    if (!engine) {
        std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", engine.error().message.c_str());
        return 1;
    }
    g_engine = std::move(*engine);
    const int result = RUN_ALL_TESTS();
    g_engine.reset();
    return result;
}

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
