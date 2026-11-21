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
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

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
const std::set<std::string> malformed_fixtures = {
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
    std::map<std::string, std::string> rejected;
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

} // namespace
