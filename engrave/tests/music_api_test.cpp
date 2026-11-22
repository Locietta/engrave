#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include "engrave/tests/core_test_env.h"
#include "engrave/tests/read_test_fixture.h"

namespace {
using namespace engrave;

Result<std::unique_ptr<Document>> open_fixture(std::string_view xml) {
    return test::core_env().engine->open(std::as_bytes(std::span(xml)));
}

void expect_fraction(Fraction fraction, std::int64_t num, std::int64_t den) {
    EXPECT_EQ(fraction.num, num);
    EXPECT_EQ(fraction.den, den);
}

void expect_pitch(const SpelledPitch &pitch, StepName step, int alter, int octave) {
    EXPECT_EQ(pitch.step, step);
    EXPECT_EQ(pitch.alter, alter);
    EXPECT_EQ(pitch.octave, octave);
}

constexpr std::string_view clarinet_fixture = R"(<museScore version="4.70">
<Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Clarinet</trackName>
<Instrument id="clarinet"><instrumentId>wind.reed.clarinet.bflat</instrumentId>
<transposeDiatonic>-1</transposeDiatonic><transposeChromatic>-2</transposeChromatic>
<Channel><program value="71"/></Channel></Instrument></Part>
<Staff id="1">
<Measure len="1/8"><startRepeat/><voice>
<TimeSig><sigN>6</sigN><sigD>8</sigD></TimeSig><KeySig><concertKey>-2</concertKey></KeySig>
<Chord><durationType>eighth</durationType><Note><pitch>60</pitch><tpc>26</tpc><tpc2>28</tpc2>
<Spanner type="Tie"><Tie/><next><location><measures>1</measures></location></next></Spanner>
</Note></Chord></voice></Measure>
<Measure><endRepeat>3</endRepeat><voice>
<Chord><durationType>16th</durationType><acciaccatura/><Note><pitch>62</pitch><tpc>16</tpc><tpc2>18</tpc2></Note></Chord>
<Chord><durationType>eighth</durationType><grace8after/><Note><pitch>64</pitch><tpc>18</tpc><tpc2>20</tpc2></Note></Chord>
<Chord><durationType>eighth</durationType><Note><pitch>60</pitch><tpc>26</tpc><tpc2>28</tpc2>
<Spanner type="Tie"><prev><location><measures>-1</measures></location></prev></Spanner>
</Note></Chord>
<Tuplet><normalNotes>2</normalNotes><actualNotes>3</actualNotes><baseNote>eighth</baseNote></Tuplet>
<Chord><durationType>eighth</durationType><Note><pitch>59</pitch><tpc>7</tpc><tpc2>9</tpc2></Note></Chord>
<Rest><durationType>eighth</durationType></Rest>
<Chord><durationType>eighth</durationType><Note><pitch>62</pitch><tpc>16</tpc><tpc2>18</tpc2></Note></Chord>
<endTuplet/><Rest><durationType>quarter</durationType><dots>1</dots><duration>3/8</duration></Rest>
</voice><voice><Chord><durationType>half</durationType><dots>1</dots><duration>3/4</duration>
<Note><pitch>55</pitch><tpc>15</tpc><tpc2>17</tpc2></Note></Chord></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
</Staff></Score></museScore>)";

TEST(MusicRead, MidScoreContextAndExactNotatedEvents) {
    auto document = open_fixture(clarinet_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto revision = (*document)->revision();
    const read_test::ItemLayoutSpy layout;
    const auto pickup = (*document)->read_music({});
    ASSERT_TRUE(pickup) << pickup.error().message;
    const auto window = (*document)->read_music({.first_measure = 1, .measure_count = 2});
    ASSERT_TRUE(window) << window.error().message;
    EXPECT_EQ(read_test::ItemLayoutSpy::count, 0u);
    EXPECT_EQ((*document)->revision(), revision);
    EXPECT_EQ(window->revision, revision);
    EXPECT_EQ((*document)->outline()->pages, std::nullopt);

    ASSERT_EQ(pickup->measures.size(), 1u);
    EXPECT_TRUE(pickup->measures[0].repeat_start);
    expect_fraction(pickup->measures[0].measure.duration, 1, 8);
    ASSERT_EQ(window->staves.size(), 1u);
    const auto &staff = window->staves.front();
    EXPECT_EQ(staff.transpose.diatonic, -1);
    EXPECT_EQ(staff.transpose.chromatic, -2);
    EXPECT_EQ(staff.key, -2);
    expect_fraction(staff.time_sig, 6, 8);
    ASSERT_EQ(window->measures.size(), 2u);
    const auto &bar = window->measures.front();
    EXPECT_TRUE(bar.repeat_end);
    EXPECT_EQ(bar.repeat_count, 3);
    expect_fraction(bar.measure.tick, 1, 8);
    expect_fraction(bar.measure.duration, 3, 4);
    EXPECT_TRUE(bar.signature_changes.empty());
    ASSERT_EQ(bar.voices.size(), 2u);
    EXPECT_EQ(bar.voices[1].voice, 1);
    ASSERT_EQ(bar.voices[1].events.size(), 1u);
    const auto &events = bar.voices[0].events;
    ASSERT_EQ(events.size(), 5u);
    const auto &principal = std::get<ChordMusic>(events[0].body);
    expect_fraction(events[0].offset, 0, 1);
    const auto &pitch = std::get<PitchedNote>(principal.notes.at(0).pitch);
    EXPECT_EQ(pitch.midi, 60);
    expect_pitch(pitch.concert, StepName::B, 1, 3);
    expect_pitch(pitch.written, StepName::C, 2, 4);
    ASSERT_TRUE(principal.notes[0].incoming);
    const auto &prior = std::get<ChordMusic>(pickup->measures[0].voices[0].events[0].body).notes.at(0);
    ASSERT_TRUE(prior.outgoing);
    EXPECT_EQ(principal.notes[0].incoming->other_note, prior.id);
    EXPECT_EQ(prior.outgoing->other_note, principal.notes[0].id);
    EXPECT_EQ(principal.notes[0].incoming->tie, prior.outgoing->tie);
    ASSERT_EQ(principal.before.size(), 1u);
    ASSERT_EQ(principal.after.size(), 1u);
    EXPECT_EQ(principal.before[0].kind, GraceKind::Acciaccatura);
    EXPECT_EQ(principal.after[0].kind, GraceKind::Grace8After);
    expect_fraction(principal.before[0].written_duration, 1, 16);

    ASSERT_EQ(window->tuplets.size(), 1u);
    const auto &tuplet = window->tuplets[0];
    EXPECT_EQ(tuplet.actual, 3);
    EXPECT_EQ(tuplet.normal, 2);
    expect_fraction(tuplet.base_duration, 1, 8);
    expect_fraction(events[1].offset, 1, 8);
    expect_fraction(events[1].written_duration, 1, 8);
    expect_fraction(events[1].elapsed_duration, 1, 12);
    EXPECT_EQ(events[1].tuplet_path, (std::vector{tuplet.id}));
    EXPECT_EQ(events[2].tuplet_path, (std::vector{tuplet.id}));
    expect_fraction(events[2].offset, 5, 24);
    EXPECT_FALSE(std::get<RestMusic>(events[2].body).measure_rest);
    const auto &flat = std::get<PitchedNote>(std::get<ChordMusic>(events[1].body).notes.at(0).pitch);
    EXPECT_EQ(flat.midi, 59);
    expect_pitch(flat.concert, StepName::C, -1, 4);
    expect_pitch(flat.written, StepName::D, -1, 4);
    ASSERT_EQ(window->measures[1].voices.size(), 1u);
    EXPECT_TRUE(std::get<RestMusic>(window->measures[1].voices[0].events.at(0).body).measure_rest);

    const auto huge = (*document)->read_music({.measure_count = 129});
    read_test::expect_code(huge, ErrorCode::RESOURCE_LIMIT, "measure capture limit");
}

constexpr std::string_view drum_fixture = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part id="1"><Staff><StaffType group="percussion"/></Staff><trackName>Kit</trackName>
<Instrument id="drumset"><useDrumset>1</useDrumset>
<Drum pitch="36"><name>Custom low drum</name><head>normal</head><line>4</line><stem>1</stem><voice>0</voice></Drum>
<Channel><program value="0"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>quarter</durationType><Note><pitch>36</pitch><tpc>14</tpc></Note>
<Note><pitch>72</pitch><tpc>14</tpc></Note></Chord>
<Rest><durationType>half</durationType><dots>1</dots><duration>3/4</duration></Rest></voice></Measure></Staff>
</Score></museScore>)";

TEST(MusicRead, UnmappedPercussionKeysSurviveReadback) {
    auto document = open_fixture(drum_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto window = (*document)->read_music({});
    ASSERT_TRUE(window) << window.error().message;
    EXPECT_TRUE(window->staves.at(0).uses_drumset);
    const auto &chord = std::get<ChordMusic>(window->measures.at(0).voices.at(0).events.at(0).body);
    ASSERT_EQ(chord.notes.size(), 2u);
    for (const auto &note : chord.notes) {
        const auto &drum = std::get<DrumNote>(note.pitch);
        if (drum.key == 36) {
            EXPECT_EQ(drum.kit_name, "Custom low drum");
        } else {
            EXPECT_EQ(drum.key, 72);
            EXPECT_EQ(drum.kit_name, std::nullopt);
        }
    }
    EXPECT_NE(std::get<DrumNote>(chord.notes[0].pitch).key, std::get<DrumNote>(chord.notes[1].pitch).key);
}

TEST(MusicRead, HornAndOctaveTranspositionDoNotUsePlaybackPitch) {
    constexpr std::string_view source = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Horn</trackName><Instrument id="horn">
<transposeDiatonic>-4</transposeDiatonic><transposeChromatic>-7</transposeChromatic>
<Channel><program value="60"/></Channel></Instrument></Part>
<Part id="2"><Staff/><trackName>Piccolo</trackName><Instrument id="piccolo">
<transposeDiatonic>7</transposeDiatonic><transposeChromatic>12</transposeChromatic>
<Channel><program value="72"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice>
<Chord><durationType>whole</durationType><Note><pitch>53</pitch><tpc>13</tpc><tpc2>14</tpc2></Note></Chord>
</voice></Measure></Staff>
<Staff id="2"><Measure><voice>
<Chord><durationType>whole</durationType><Note><pitch>72</pitch><tpc>14</tpc><tpc2>14</tpc2></Note></Chord>
</voice></Measure></Staff></Score></museScore>)";
    auto document = open_fixture(source);
    ASSERT_TRUE(document) << document.error().message;
    const auto window = (*document)->read_music({});
    ASSERT_TRUE(window) << window.error().message;
    const auto &voices = window->measures.at(0).voices;
    ASSERT_EQ(voices.size(), 2u);
    const auto pitch_of = [](const VoiceEvents &voice) {
        return std::get<PitchedNote>(std::get<ChordMusic>(voice.events.at(0).body).notes.at(0).pitch);
    };
    const auto horn = pitch_of(voices[0]);
    EXPECT_EQ(horn.midi, 53);
    expect_pitch(horn.concert, StepName::F, 0, 3);
    expect_pitch(horn.written, StepName::C, 0, 4);
    const auto piccolo = pitch_of(voices[1]);
    EXPECT_EQ(piccolo.midi, 72);
    expect_pitch(piccolo.concert, StepName::C, 0, 5);
    expect_pitch(piccolo.written, StepName::C, 0, 4);
}

TEST(MusicRead, OverlappingSpansRetainEndpointsInLinkedPart) {
    auto document = open_fixture(clarinet_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto &bars = outline->measures;
    const auto pickup = (*document)->read_music({});
    ASSERT_TRUE(pickup) << pickup.error().message;
    const auto second = (*document)->read_music({.first_measure = 1});
    ASSERT_TRUE(second) << second.error().message;
    const auto start = pickup->measures.at(0).voices.at(0).events.at(0).id;
    const auto next = second->measures.at(0).voices.at(0).events.at(0).id;
    const auto edits = (*document)->edit({{AddSpannerToBarEnd{"Ottava", start, {bars[2].id}, {}}, AddSpanner{"Slur", start, next, {}},
                                           AddVolta{bars[1].id, bars[1].id, {1, 2}, false}, GenerateExcerpts{{outline->parts.at(0).id}}}},
                                         EditOptions{});
    ASSERT_TRUE(edits) << edits.error().message;
    ASSERT_FALSE(edits->rejection) << edits->rejection->error.message;
    const auto excerpts = (*document)->outline()->excerpts;
    ASSERT_EQ(excerpts.size(), 1u);
    const ScoreSel excerpt{excerpts[0].id};
    const auto master_window = (*document)->read_music({.first_measure = 1});
    const auto part_window = (*document)->read_music({.score = excerpt, .first_measure = 1});
    ASSERT_TRUE(master_window) << master_window.error().message;
    ASSERT_TRUE(part_window) << part_window.error().message;
    const auto secondary = (*document)->read_music({.first_measure = 1, .voices = {1}});
    ASSERT_TRUE(secondary) << secondary.error().message;
    ASSERT_EQ(secondary->spanners.size(), 1u);
    EXPECT_EQ(secondary->spanners[0].type, "Ottava");
    const auto part_bars = (*document)->outline(excerpt)->measures;
    for (const auto *window : {&*master_window, &*part_window}) {
        const auto &score_bars = window == &*master_window ? bars : part_bars;
        ASSERT_EQ(window->voltas.size(), 1u);
        const auto &volta = window->voltas[0];
        EXPECT_EQ(volta.endings, (std::vector{1, 2}));
        EXPECT_FALSE(volta.closed);
        EXPECT_EQ(volta.span.start.measure, score_bars[1].id);
        EXPECT_EQ(volta.span.end.measure, score_bars[1].id);
        expect_fraction(volta.span.end.offset, 3, 4);
        const auto is_ottava = [](const SpanMusic &span) { return span.type == "Ottava"; };
        EXPECT_EQ(std::ranges::count_if(window->spanners, is_ottava), 1);
        const auto line = std::ranges::find_if(window->spanners, is_ottava);
        ASSERT_NE(line, window->spanners.end());
        EXPECT_EQ(line->start.measure, score_bars[0].id);
        EXPECT_EQ(line->end.measure, score_bars[2].id);
        expect_fraction(line->end.offset, 3, 4);
        const auto slur = std::ranges::find(window->spanners, "Slur", &SpanMusic::type);
        ASSERT_NE(slur, window->spanners.end());
        EXPECT_EQ(slur->end_element, window->measures.at(0).voices.at(0).events.at(0).id);
        const auto &notes = std::get<ChordMusic>(window->measures.at(0).voices.at(0).events.at(0).body).notes;
        EXPECT_EQ(std::get<PitchedNote>(notes.at(0).pitch).midi, 60);
    }
    ASSERT_TRUE((*document)->page_count());
    const auto laid_out = (*document)->read_music({.first_measure = 1});
    ASSERT_TRUE(laid_out) << laid_out.error().message;
    ASSERT_EQ(laid_out->spanners.size(), master_window->spanners.size());
    for (std::size_t i = 0; i < laid_out->spanners.size(); ++i) {
        EXPECT_EQ(laid_out->spanners[i].id, master_window->spanners[i].id);
        EXPECT_EQ(laid_out->spanners[i].start_element, master_window->spanners[i].start_element);
        EXPECT_EQ(laid_out->spanners[i].end_element, master_window->spanners[i].end_element);
    }
}

TEST(MusicRead, NoteOwnedLineCrossesAWindowWithoutEitherAnchor) {
    constexpr std::string_view source = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part>
<Staff id="1"><Measure><voice><Chord><durationType>whole</durationType>
<Note><eid>A_B</eid><pitch>60</pitch><tpc>14</tpc></Note></Chord></voice></Measure>
<Measure><voice><MeasureRepeat><subtype>1</subtype><durationType>measure</durationType>
<duration>4/4</duration></MeasureRepeat></voice></Measure>
<Measure><voice><Chord><durationType>whole</durationType><Note>
<eid>A_C</eid><pitch>67</pitch><tpc>15</tpc>
<Glissando><eid>A_D</eid><startElement>A_B</startElement><endElement>A_C</endElement><track>0</track><diagonal>1</diagonal></Glissando>
</Note></Chord></voice></Measure></Staff></Score></museScore>)";
    auto document = open_fixture(source);
    ASSERT_TRUE(document) << document.error().message;
    const auto window = (*document)->read_music({.first_measure = 1});
    ASSERT_TRUE(window) << window.error().message;
    ASSERT_EQ(window->spanners.size(), 1u);
    const auto &line = window->spanners[0];
    EXPECT_EQ(line.type, "Glissando");
    EXPECT_EQ(line.start_element, element_id_from_string("A_B"));
    EXPECT_EQ(line.end_element, element_id_from_string("A_C"));
    const auto &bar = window->measures.at(0);
    const auto &body = std::get<RestMusic>(bar.voices.at(0).events.at(0).body);
    EXPECT_EQ(body.measure_repeat_count, 1);
}

} // namespace
