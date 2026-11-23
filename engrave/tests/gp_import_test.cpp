// Guitar Pro and PowerTab import of upstream's test data, through the library only.
//
//   engrave_gp_import_test --assets=<resources> [gtest options]
//
// The library contract for bad input is a returned Error{InvalidInput}: the importer must
// neither crash nor hand back a document without a score.  Upstream's fixtures include
// deliberately malformed files (unknown headers and version strings, a MIDI channel out
// of range), so the test opens every score file of
// src/importexport/guitarpro/tests/data and requires exactly those to fail; every other
// file, the .gpx fixtures with their reference scores included, has to open.  The
// reference MSCX files in that directory are not inputs and are skipped.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <amc/flatset.hpp>
#include <gtest/gtest.h>

#include "core_test_env.h"
#include "engrave/src/api/state.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spanner.h"
#include "global/flatmap.h"
#include "importexport/guitarpro/internal/importgtp.h"
#include <engrave/engrave.h>

namespace {

auto &g_resources = engrave::test::core_env().assets;
auto &g_engine = engrave::test::core_env().engine;

constexpr std::string_view score_extensions[] = {".gp", ".gp3", ".gp4", ".gp5", ".gpx", ".ptb"};

/// The fixtures that are malformed on purpose. Every other score file of the data
/// directory opens: the 17 .gpx files with reference scores used to be rejected by the
/// GPX decoder's length guard (it copied a back-reference only up to its offset and then
/// reached end of input one or two bytes short of the declared length).
const amc::FlatSet<std::string> malformed_fixtures = {
    "midi-channel-out-of-range.gp4",
    "unknown-header.gp4",
    "unknown-version-string.gp4",
    "unsupported-version.gp4",
};

std::filesystem::path test_data() {
    static const std::filesystem::path dir = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "src/importexport/guitarpro/tests/data";
    return dir;
}

/// The score files of the test data, sorted, in one run.
std::vector<std::filesystem::path> score_files() {
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::directory_iterator(test_data())) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension = entry.path().extension().string();
        if (std::find(std::begin(score_extensions), std::end(score_extensions), extension) != std::end(score_extensions)) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

/// Chords without a note are not valid model elements: layout reads a chord's first note,
/// so an importer that leaves one behind turns into a crash later.  Counted here so an
/// imported score is not merely present but structurally sound where the crash was.
std::size_t chords_without_notes(mu::engraving::Score *score) {
    std::size_t count = 0;
    for (mu::engraving::Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (mu::engraving::Segment *segment = measure->first(); segment; segment = segment->next()) {
            for (mu::engraving::EngravingItem *element : segment->elist()) {
                if (element && element->is_chord() && mu::engraving::to_chord(element)->notes().empty()) {
                    ++count;
                }
            }
        }
    }
    return count;
}

TEST(GuitarProImport, EveryTestDataFileOpensOrReturnsAnError) {
    const std::vector<std::filesystem::path> files = score_files();
    ASSERT_GE(files.size(), 240u) << "the Guitar Pro test data is missing under " << test_data();

    std::vector<std::string> errors;
    muse::FlatMap<std::string, std::string> rejected;
    std::size_t opened = 0;
    for (const std::filesystem::path &path : files) {
        SCOPED_TRACE(path.filename().string());
        // A crash in here aborts the binary, which is what the contract forbids; a returned
        // error is the expected answer for the malformed fixtures.
        auto document = g_engine->open(path);
        if (!document) {
            errors.push_back(path.filename().string() + ": " + document.error().message);
            rejected.emplace(path.filename().string(), document.error().message);
            continue;
        }
        ++opened;
        mu::engraving::MasterScore *master = engrave::DocumentAccess::master_score(**document);
        ASSERT_NE(master, nullptr) << "a document without a score";
        EXPECT_NE(master->first_measure(), nullptr) << "a score without a measure";
        EXPECT_FALSE(master->parts().empty()) << "a score without a part";
        EXPECT_EQ(chords_without_notes(master), 0u) << "an imported chord holds no note";
    }

    std::fprintf(stdout, "Guitar Pro test data: %zu score file(s), %zu opened, %zu returned an error\n", files.size(), opened,
                 errors.size());
    for (const std::string &line : errors) {
        std::fprintf(stdout, "  error %s\n", line.c_str());
    }
    std::fflush(stdout);
    EXPECT_GT(opened, 0u);
    EXPECT_EQ(opened + errors.size(), files.size());
    // Exactly the malformed fixtures fail, and every one of them does.
    EXPECT_EQ(errors.size(), malformed_fixtures.size());
    for (const auto &[name, message] : rejected) {
        EXPECT_TRUE(malformed_fixtures.contains(name)) << name << " was rejected: " << message;
    }
    for (const std::string &name : malformed_fixtures) {
        EXPECT_TRUE(rejected.contains(name)) << name << " is malformed but opened";
    }
}

struct SlideImporter : mu::iex::guitarpro::GuitarPro3 {
    explicit SlideImporter(mu::engraving::MasterScore *s) : GuitarPro3(s, 300) { slurs.resize(s->nstaves(), nullptr); }

    using GuitarPro::create_slide;
    using GuitarPro::create_slur;
    using GuitarPro::legato_slide;
    using GuitarPro::shift_slide;
};

TEST(GuitarProImport, CreateSlideFindsPreviousChordAcrossOtherTracksAndSkipsMissingStart) {
    using namespace mu::engraving;
    constexpr std::string_view xml = R"(<museScore version="4.60"><Score><Division>480</Division>
<Part><Staff id="1"/><Instrument id="g1"><trackName>Guitar 1</trackName><instrumentId>pluck.guitar</instrumentId></Instrument></Part>
<Part><Staff id="2"/><Instrument id="g2"><trackName>Guitar 2</trackName><instrumentId>pluck.guitar</instrumentId></Instrument></Part>
<Staff id="1"><Measure>
<voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>67</pitch><tpc>15</tpc></Note></Chord>
<Rest><durationType>eighth</durationType></Rest>
<Chord><durationType>eighth</durationType><Note><pitch>69</pitch><tpc>17</tpc></Note></Chord>
</voice>
<voice>
<Rest><durationType>quarter</durationType></Rest>
<Rest><durationType>eighth</durationType></Rest>
<Chord><durationType>eighth</durationType><Note><pitch>52</pitch><tpc>18</tpc></Note></Chord>
<Rest><durationType>half</durationType></Rest>
</voice>
</Measure></Staff>
<Staff id="2"><Measure>
<voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>quarter</durationType></Rest>
<Rest><durationType>eighth</durationType></Rest>
<Chord><durationType>eighth</durationType><Note><pitch>48</pitch><tpc>14</tpc></Note></Chord>
<Rest><durationType>half</durationType></Rest>
</voice>
<voice>
<Chord><durationType>half</durationType><Note><pitch>50</pitch><tpc>16</tpc></Note></Chord>
<Chord><durationType>quarter</durationType><Note><pitch>55</pitch><tpc>15</tpc></Note></Chord>
<Rest><durationType>quarter</durationType></Rest>
</voice>
</Measure></Staff></Score></museScore>)";

    std::vector<std::byte> bytes(xml.size());
    std::transform(xml.begin(), xml.end(), bytes.begin(), [](unsigned char c) { return static_cast<std::byte>(c); });
    auto opened = g_engine->open(bytes);
    ASSERT_TRUE(opened) << opened.error().message;
    MasterScore *master = engrave::DocumentAccess::master_score(**opened);
    ASSERT_NE(master, nullptr);
    Measure *measure = master->first_measure();
    ASSERT_NE(measure, nullptr);

    auto chord_at = [&](const Fraction &tick, track_idx_t track) -> Chord * {
        Segment *seg = measure->find_segment(SegmentType::CHORD_REST, tick);
        return (seg && seg->element(track) && seg->element(track)->is_chord()) ? to_chord(seg->element(track)) : nullptr;
    };

    Chord *first_chord = chord_at(Fraction(0, 1), 0);
    Chord *shift_start = chord_at(Fraction(1, 4), 0);
    Chord *shift_end = chord_at(Fraction(1, 2), 0);
    Chord *after_rest = chord_at(Fraction(7, 8), 0);
    Chord *legato_start = chord_at(Fraction(0, 1), 5);
    Chord *legato_end = chord_at(Fraction(1, 2), 5);
    ASSERT_TRUE(first_chord && shift_start && shift_end && after_rest && legato_start && legato_end);

    ASSERT_NE(shift_end->segment()->prev1(SegmentType::CHORD_REST), nullptr);
    EXPECT_EQ(shift_end->segment()->prev1(SegmentType::CHORD_REST)->tick(), Fraction(3, 8));
    EXPECT_EQ(shift_end->segment()->prev1(SegmentType::CHORD_REST)->element(0), nullptr);
    EXPECT_EQ(legato_end->segment()->prev1(SegmentType::CHORD_REST)->element(5), nullptr);

    SlideImporter importer(master);
    importer.create_slide(SlideImporter::shift_slide, first_chord, 0);
    importer.create_slide(SlideImporter::legato_slide, first_chord, 0);
    importer.create_slide(SlideImporter::shift_slide, after_rest, 0);
    importer.create_slide(SlideImporter::legato_slide, after_rest, 0);

    EXPECT_FALSE(first_chord->ends_note_anchored_line());
    EXPECT_FALSE(after_rest->ends_note_anchored_line());
    EXPECT_TRUE(master->spanner().empty());

    importer.create_slide(SlideImporter::shift_slide, shift_end, 0);
    importer.create_slide(SlideImporter::legato_slide, legato_end, 1);

    ASSERT_EQ(shift_end->up_note()->spanner_back().size(), 1u);
    auto *shift_gliss = to_glissando(shift_end->up_note()->spanner_back().front());
    EXPECT_EQ(shift_gliss->start_element(), shift_start->up_note());
    EXPECT_EQ(shift_gliss->end_element(), shift_end->up_note());
    EXPECT_TRUE(shift_gliss->glissando_shift());
    EXPECT_TRUE(shift_end->ends_note_anchored_line());

    ASSERT_EQ(legato_end->up_note()->spanner_back().size(), 1u);
    auto *legato_gliss = to_glissando(legato_end->up_note()->spanner_back().front());
    EXPECT_EQ(legato_gliss->start_element(), legato_start->up_note());
    EXPECT_EQ(legato_gliss->end_element(), legato_end->up_note());
    EXPECT_FALSE(legato_gliss->glissando_shift());
    EXPECT_TRUE(legato_end->ends_note_anchored_line());

    ASSERT_EQ(master->spanner().size(), 1u);
    Spanner *slur = master->spanner().begin()->second;
    ASSERT_TRUE(slur && slur->is_slur());
    EXPECT_EQ(slur->start_element(), legato_start);
    EXPECT_EQ(slur->end_element(), legato_end);

    importer.create_slur(true, 0, shift_start);
    importer.create_slide(SlideImporter::legato_slide, after_rest, 0);
    importer.create_slur(false, 0, shift_end);
    ASSERT_EQ(master->spanner().size(), 2u);
    const auto open_slur = master->spanner().find(shift_start->tick().ticks());
    ASSERT_NE(open_slur, master->spanner().end());
    ASSERT_TRUE(open_slur->second->is_slur());
    EXPECT_EQ(open_slur->second->start_element(), shift_start);
    EXPECT_EQ(open_slur->second->end_element(), shift_end);
    EXPECT_EQ(open_slur->second->tick2(), shift_end->tick());

    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
}

TEST(GuitarProImport, GP3SlidesImportAcrossInterveningStaffGraceNoteAndMissingPartner) {
    using namespace mu::engraving;
    std::vector<std::byte> gp3;
    auto push_u8 = [&](uint8_t v) { gp3.push_back(static_cast<std::byte>(v)); };
    auto push_i8 = [&](int8_t v) { gp3.push_back(static_cast<std::byte>(static_cast<uint8_t>(v))); };
    auto push_i32 = [&](int32_t v) {
        const uint32_t u = static_cast<uint32_t>(v);
        push_u8(static_cast<uint8_t>(u & 0xffu));
        push_u8(static_cast<uint8_t>((u >> 8) & 0xffu));
        push_u8(static_cast<uint8_t>((u >> 16) & 0xffu));
        push_u8(static_cast<uint8_t>((u >> 24) & 0xffu));
    };
    auto push_delphi = [&](std::string_view s) {
        push_i32(static_cast<int32_t>(s.size() + 1));
        push_u8(static_cast<uint8_t>(s.size()));
        for (unsigned char c : s) push_u8(c);
    };
    auto push_pascal_fixed = [&](std::string_view s, size_t cap) {
        push_u8(static_cast<uint8_t>(s.size()));
        for (unsigned char c : s) push_u8(c);
        for (size_t i = s.size(); i < cap; ++i) push_u8(0);
    };

    constexpr std::string_view header = "FICHIER GUITAR PRO v3.00";
    push_pascal_fixed(header, 30);
    for (int i = 0; i < 8; ++i) push_delphi("");
    push_i32(0);   // comments
    push_u8(0);    // shuffle
    push_i32(120); // tempo
    push_i32(0);   // key

    for (int i = 0; i < 64; ++i) {
        push_i32(24);
        push_u8(10);
        push_u8(8);
        for (int k = 0; k < 6; ++k) push_u8(0);
    }

    push_i32(1); // measures
    push_i32(2); // staves

    push_u8(0x03);
    push_u8(4);
    push_u8(4);

    constexpr int tuning[7] = {64, 59, 55, 50, 45, 40, 0};
    for (int staff_idx = 0; staff_idx < 2; ++staff_idx) {
        push_u8(0);
        push_pascal_fixed(staff_idx == 0 ? "Guitar 1" : "Guitar 2", 40);
        push_i32(6);
        for (int t : tuning) push_i32(t);
        push_i32(1);
        push_i32(staff_idx + 1);
        push_i32(staff_idx + 1);
        push_i32(24);
        push_i32(0);
        push_i32(0);
    }

    // Staff 1 (track 0):
    // beat 0 (0/1): quarter fret 3 with slide -> beat 1 (1/4) fret 5, across Staff 2's 1/8 note
    // beat 2 (1/2): quarter fret 7 with grace-note slide from fret 5 (transition = 1)
    // beat 3 (3/4): quarter fret 8 with unmatched slide at end of score
    push_i32(4);
    push_u8(0);
    push_i8(0);
    push_u8(1u << 6);
    push_u8(0x28);
    push_u8(1);
    push_u8(3);
    push_u8(0x04);

    push_u8(0);
    push_i8(0);
    push_u8(1u << 6);
    push_u8(0x20);
    push_u8(1);
    push_u8(5);

    push_u8(0);
    push_i8(0);
    push_u8(1u << 6);
    push_u8(0x28);
    push_u8(1);
    push_u8(7);
    push_u8(0x10);
    push_u8(5);
    push_u8(6);
    push_u8(1);
    push_u8(1);

    push_u8(0);
    push_i8(0);
    push_u8(1u << 6);
    push_u8(0x28);
    push_u8(1);
    push_u8(8);
    push_u8(0x04);

    // Staff 2 (track 4): two eighth notes at 0/1 and 1/8 so 1/8 intervenes between Staff 1's 0/1 and 1/4.
    push_i32(2);
    for (uint8_t fret : {uint8_t{1}, uint8_t{3}}) {
        push_u8(0);
        push_i8(1);
        push_u8(1u << 5);
        push_u8(0x20);
        push_u8(1);
        push_u8(fret);
    }

    auto opened = g_engine->open(gp3);
    ASSERT_TRUE(opened) << opened.error().message;
    MasterScore *master = engrave::DocumentAccess::master_score(**opened);
    ASSERT_NE(master, nullptr);
    Measure *measure = master->first_measure();
    ASSERT_NE(measure, nullptr);

    auto chord_at = [&](const Fraction &tick, track_idx_t track) -> Chord * {
        Segment *seg = measure->find_segment(SegmentType::CHORD_REST, tick);
        return (seg && seg->element(track) && seg->element(track)->is_chord()) ? to_chord(seg->element(track)) : nullptr;
    };

    Chord *beat0 = chord_at(Fraction(0, 1), 0);
    Chord *beat1 = chord_at(Fraction(1, 4), 0);
    Chord *beat2 = chord_at(Fraction(1, 2), 0);
    Chord *beat3 = chord_at(Fraction(3, 4), 0);
    Chord *staff2_intervening = chord_at(Fraction(1, 8), 4);
    ASSERT_TRUE(beat0 && beat1 && beat2 && beat3 && staff2_intervening);

    ASSERT_EQ(beat1->up_note()->spanner_back().size(), 1u);
    auto *inter_staff_gliss = to_glissando(beat1->up_note()->spanner_back().front());
    EXPECT_EQ(inter_staff_gliss->start_element(), beat0->up_note());
    EXPECT_EQ(inter_staff_gliss->end_element(), beat1->up_note());
    EXPECT_TRUE(beat1->ends_note_anchored_line());

    ASSERT_EQ(beat2->grace_notes().size(), 1u);
    Chord *grace_chord = beat2->grace_notes().front();
    ASSERT_EQ(grace_chord->notes().size(), 1u);
    Note *grace_note = grace_chord->notes().front();
    ASSERT_EQ(beat2->up_note()->spanner_back().size(), 1u);
    auto *grace_gliss = to_glissando(beat2->up_note()->spanner_back().front());
    EXPECT_EQ(grace_gliss->start_element(), grace_note);
    EXPECT_EQ(grace_gliss->end_element(), beat2->up_note());
    EXPECT_FALSE(grace_chord->ends_note_anchored_line());
    EXPECT_TRUE(beat2->ends_note_anchored_line());

    EXPECT_TRUE(beat3->up_note()->spanner_for().empty());
    EXPECT_TRUE(beat3->up_note()->spanner_back().empty());
    EXPECT_FALSE(beat3->ends_note_anchored_line());

    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
}

/// Little-endian Guitar Pro 3-5 primitives.
struct GpWriter {
    std::vector<std::byte> bytes;

    void u8(uint8_t v) { bytes.push_back(static_cast<std::byte>(v)); }

    void i8(int8_t v) { u8(static_cast<uint8_t>(v)); }

    void i32(int32_t v) {
        const auto u = static_cast<uint32_t>(v);
        for (int shift = 0; shift < 32; shift += 8) u8(static_cast<uint8_t>((u >> shift) & 0xffu));
    }

    void zeros(size_t n) { bytes.insert(bytes.end(), n, std::byte{0}); }

    void delphi(std::string_view s) {
        i32(static_cast<int32_t>(s.size() + 1));
        u8(static_cast<uint8_t>(s.size()));
        for (unsigned char c : s) u8(c);
    }

    void pascal_fixed(std::string_view s, size_t cap) {
        u8(static_cast<uint8_t>(s.size()));
        for (unsigned char c : s) u8(c);
        zeros(cap - s.size());
    }
};

/// One beat of a single-note voice: no string is a rest; a slide is a shift slide.
struct SlideBeat {
    int8_t len = 0; // -1 half, 0 quarter, 1 eighth
    int string = -1;
    int fret = 0;
    bool slide = false;
};

/// A GP4 or GP5 file of two guitars and three 4/4 bars. Guitar 1, in quarters:
///   bar 1: s2f2 slide, rest, s2f4, s1f5 slide
///   bar 2: s1f7, s0f1 slide, s3f2, s0f3
///   bar 3: s4f5 slide, s3f1, s2f1, s4f7
/// Guitar 2 plays s2f9 as two eighths and a quarter at the start of bar 1, so it has a note on
/// the first slide's string at the rest of guitar 1, and nothing in bars 2 and 3.
std::vector<std::byte> slide_file(int major_version) {
    const bool gp5 = major_version == 5;
    const std::vector<std::vector<SlideBeat>> guitar1 = {
        {{0, 2, 2, true}, {0}, {0, 2, 4}, {0, 1, 5, true}},
        {{0, 1, 7}, {0, 0, 1, true}, {0, 3, 2}, {0, 0, 3}},
        {{0, 4, 5, true}, {0, 3, 1}, {0, 2, 1}, {0, 4, 7}},
    };
    const std::vector<std::vector<SlideBeat>> guitar2 = {{{1, 2, 9}, {1, 2, 9}, {0, 2, 9}}, {}, {}};
    const std::vector<std::vector<SlideBeat>> *staves[] = {&guitar1, &guitar2};
    constexpr int bars = 3;

    GpWriter w;
    w.pascal_fixed(gp5 ? "FICHIER GUITAR PRO v5.00" : "FICHIER GUITAR PRO v4.00", 30);
    for (int i = 0; i < (gp5 ? 9 : 8); ++i) w.delphi("");
    w.i32(0);          // comments
    if (!gp5) w.u8(0); // triplet feel
    w.i32(0);          // lyric track
    w.i32(0);          // lyric start beat
    w.i32(0);          // lyrics
    for (int i = 0; i < 4; ++i) {
        w.i32(0);
        w.i32(0);
    }
    if (gp5) {
        w.zeros(30);
        for (int i = 0; i < 11; ++i) {
            w.zeros(4);
            w.u8(0);
        }
    }
    w.i32(120); // tempo
    w.i32(0);   // key
    w.u8(0);    // octave
    for (int i = 0; i < 64; ++i) {
        w.i32(24);
        w.u8(10);
        w.u8(8);
        w.zeros(6);
    }
    if (gp5) {
        for (int i = 0; i < 19; ++i) {
            w.u8(0xff); // no direction marker
            w.u8(0xff);
        }
        w.zeros(4);
    }
    w.i32(bars);
    w.i32(2);

    for (int bar = 0; bar < bars; ++bar) {
        if (gp5 && bar > 0) w.u8(0);
        w.u8(bar == 0 ? 0x03 : 0x00);
        if (bar == 0) {
            w.u8(4);
            w.u8(4);
        }
        if (gp5) {
            if (bar == 0) w.zeros(4); // beam groups
            w.u8(0);
            w.u8(0); // triplet feel
        }
    }

    constexpr int tuning[7] = {64, 59, 55, 50, 45, 40, 0};
    for (int staff = 0; staff < 2; ++staff) {
        w.u8(0);
        if (gp5) w.u8(0);
        w.pascal_fixed(staff == 0 ? "Guitar 1" : "Guitar 2", 40);
        w.i32(6);
        for (int t : tuning) w.i32(t);
        w.i32(1);         // port
        w.i32(staff + 1); // channel
        w.i32(staff + 1); // effects channel
        w.i32(24);        // frets
        w.i32(0);         // capo
        w.i32(0);         // color
        if (gp5) w.zeros(44);
    }
    if (gp5) w.zeros(2);

    auto write_beat = [&](const SlideBeat &beat) {
        w.u8(0);
        w.i8(beat.len);
        if (beat.string < 0) {
            w.u8(0);
        } else {
            w.u8(static_cast<uint8_t>(1u << (6 - beat.string)));
            w.u8(beat.slide ? 0x28 : 0x20); // fret and note type, effects when sliding
            w.u8(1);                        // normal note
            w.i8(static_cast<int8_t>(beat.fret));
            if (gp5) w.u8(0);
            if (beat.slide) {
                w.u8(0);
                w.u8(0x08); // slide
                w.u8(1);    // shift slide, in both formats
            }
        }
        if (gp5) {
            w.u8(0);
            w.u8(0);
        }
    };
    for (int bar = 0; bar < bars; ++bar) {
        for (int staff = 0; staff < 2; ++staff) {
            const auto &beats = (*staves[staff])[bar];
            w.i32(static_cast<int32_t>(beats.size()));
            for (const SlideBeat &beat : beats) write_beat(beat);
            if (gp5) {
                // The second voice is one empty beat, as Guitar Pro 5 writes an unused voice.
                w.i32(1);
                w.u8(0x40);
                w.u8(0);
                w.i8(0);
                w.u8(0);
                w.u8(0);
                w.u8(0);
                if (bar != bars - 1 || staff != 1) w.u8(0);
            }
        }
    }
    return std::move(w.bytes);
}

/// Every glissando of the score, as "track tick string fret -> track tick string fret".
std::vector<std::string> glissandi(mu::engraving::Score *score) {
    using namespace mu::engraving;
    std::vector<std::string> rows;
    for (Segment *seg = score->first_segment(SegmentType::CHORD_REST); seg; seg = seg->next1(SegmentType::CHORD_REST)) {
        for (EngravingItem *el : seg->elist()) {
            if (!el || !el->is_chord()) continue;
            for (Note *note : to_chord(el)->notes()) {
                for (Spanner *sp : note->spanner_for()) {
                    if (!sp->is_glissando() || !sp->end_element() || !sp->end_element()->is_note()) continue;
                    const Note *end = to_note(sp->end_element());
                    EXPECT_EQ(std::ranges::count(end->spanner_back(), sp), 1);
                    rows.push_back(std::format("{} {} s{} f{} -> {} {} s{} f{}", note->track(), note->tick().ticks(), note->string(),
                                               note->fret(), end->track(), end->tick().ticks(), end->string(), end->fret()));
                }
            }
        }
    }
    std::ranges::sort(rows);
    return rows;
}

void expect_slide_layout(mu::engraving::MasterScore *master) {
    using namespace mu::engraving;
    auto cr_at = [&](int ticks, track_idx_t track) -> ChordRest * {
        Segment *seg = master->tick2segment(Fraction::from_ticks(ticks), true, SegmentType::CHORD_REST);
        return seg ? seg->cr(track) : nullptr;
    };
    ChordRest *rest = cr_at(480, 0);
    ASSERT_TRUE(rest && rest->is_rest());
    ChordRest *other = cr_at(480, 4);
    ASSERT_TRUE(other && other->is_chord());
    EXPECT_EQ(to_chord(other)->up_note()->string(), 2);
    ChordRest *first = cr_at(0, 0);
    ASSERT_TRUE(first && first->is_chord());
    EXPECT_EQ(to_chord(first)->up_note()->string(), 2);
    EXPECT_EQ(to_chord(first)->up_note()->fret(), 2);
}

TEST(GuitarProImport, GP4ShiftSlidesReachTheNextTwoChordsOnTheirStringAndTrack) {
    auto opened = g_engine->open(slide_file(4));
    ASSERT_TRUE(opened) << opened.error().message;
    mu::engraving::MasterScore *master = engrave::DocumentAccess::master_score(**opened);
    ASSERT_NE(master, nullptr);
    expect_slide_layout(master);

    // Quarter = 480 ticks. The slide from s4f5 at bar 3 finds its string only in the third chord.
    const std::vector<std::string> expected = {
        "0 0 s2 f2 -> 0 960 s2 f4",
        "0 1440 s1 f5 -> 0 1920 s1 f7",
        "0 2400 s0 f1 -> 0 3360 s0 f3",
    };
    EXPECT_EQ(glissandi(master), expected);
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
}

TEST(GuitarProImport, GP5ShiftSlidesReachTheNextChordOnTheirStringAndTrack) {
    auto opened = g_engine->open(slide_file(5));
    ASSERT_TRUE(opened) << opened.error().message;
    mu::engraving::MasterScore *master = engrave::DocumentAccess::master_score(**opened);
    ASSERT_NE(master, nullptr);
    expect_slide_layout(master);

    const std::vector<std::string> expected = {
        "0 0 s2 f2 -> 0 960 s2 f4",
        "0 1440 s1 f5 -> 0 1920 s1 f7",
        "0 2400 s0 f1 -> 0 3360 s0 f3",
        "0 3840 s4 f5 -> 0 5280 s4 f7",
    };
    EXPECT_EQ(glissandi(master), expected);
    ASSERT_TRUE((*opened)->render({.format = engrave::RenderFormat::SVG}));
}

} // namespace
