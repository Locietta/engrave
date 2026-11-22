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

TEST(MusicRead, HairpinSubtypes) {
    const auto document = open_fixture(clarinet_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto music = (*document)->read_music({.measure_count = 2});
    ASSERT_TRUE(music) << music.error().message;
    const auto start = music->measures.at(0).voices.at(0).events.at(0).id;
    const auto end = music->measures.at(1).voices.at(0).events.at(0).id;
    const std::vector<std::string_view> expected{"crescendo", "diminuendo", "crescendo_line", "diminuendo_line"};
    Batch batch;
    for (int type = 0; type < static_cast<int>(expected.size()); ++type) {
        batch.ops.push_back(AddSpanner{"HairPin", start, end, {{"HAIRPIN_TYPE", type}}});
    }
    batch.ops.push_back(AddSpanner{"Slur", start, end, {}});
    const auto edit = (*document)->edit(batch);
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    const auto window = (*document)->read_music({.measure_count = 2});
    ASSERT_TRUE(window) << window.error().message;
    ASSERT_EQ(window->spanners.size(), expected.size() + 1);
    for (const auto subtype : expected) {
        EXPECT_EQ(std::ranges::count_if(window->spanners,
                                        [&](const SpanMusic &span) { return span.type == "HairPin" && span.subtype == subtype; }),
                  1)
            << subtype;
    }
    const auto slur = std::ranges::find(window->spanners, "Slur", &SpanMusic::type);
    ASSERT_NE(slur, window->spanners.end());
    EXPECT_FALSE(slur->subtype);
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

constexpr std::string_view piano_fixture = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part id="1"><Staff/><Staff/><trackName>Piano</trackName>
<Instrument id="piano"><instrumentId>keyboard.piano</instrumentId><Channel><program value="0"/></Channel></Instrument>
</Part>
<Staff id="1"><Measure len="1/8"><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>eighth</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><voice><TimeSig><sigN>6</sigN><sigD>8</sigD></TimeSig>
<Rest><durationType>measure</durationType></Rest></voice></Measure></Staff>
<Staff id="2"><Measure len="1/8"><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><voice><TimeSig><sigN>6</sigN><sigD>8</sigD></TimeSig>
<Rest><durationType>measure</durationType></Rest></voice></Measure></Staff>
</Score></museScore>)";

constexpr SpelledPitch c4{StepName::C, 0, 4};
constexpr SpelledPitch e4{StepName::E, 0, 4};
constexpr SpelledPitch g4{StepName::G, 0, 4};

Result<std::unique_ptr<Document>> reload(Document &document) {
    auto bytes = document.export_score({.format = Format::MSCZ});
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    return test::core_env().engine->open(*bytes);
}

const mu::engraving::Chord *chord_at(const mu::engraving::Score &score, mu::engraving::Fraction tick, mu::engraving::track_idx_t track) {
    auto *rhythm = score.find_cr(tick, track);
    return rhythm && rhythm->is_chord() ? mu::engraving::to_chord(rhythm) : nullptr;
}

TEST(MusicWrite, BarChecksFollowPickupAndMeasureBoundaries) {
    auto document = open_fixture(piano_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    Phrase phrase{.lanes = {{{outline->measures.at(0).id, {0, 1}},
                             PitchDomain::Concert,
                             {PhraseEvent{{1, 8}, {{c4}}}, BarCheck{}, PhraseEvent{{1, 1}, {{e4}}}, BarCheck{}, PhraseEvent{{3, 4}, {{g4}}},
                              BarCheck{}}}}};
    const auto written = (*document)->write_phrase(phrase);
    ASSERT_TRUE(written) << written.error().message;
    ASSERT_FALSE(written->rejection);

    phrase.lanes[0].items = {BarCheck{}};
    const auto bar_only = (*document)->write_phrase(phrase);
    ASSERT_FALSE(bar_only);
    EXPECT_EQ(bar_only.error().message, "lane 0: a lane needs at least one musical event or tuplet");

    phrase.lanes[0].items = {PhraseEvent{{1, 8}, {{c4}}}, BarCheck{}, PhraseEvent{{0, 1}, {{e4}}}};
    const auto bad_duration = (*document)->write_phrase(phrase);
    ASSERT_FALSE(bad_duration);
    EXPECT_TRUE(bad_duration.error().message.starts_with("lane 0, item 2: "));
}

TEST(MusicWrite, BarChecksUseTheStretchedStaffCursor) {
    constexpr std::string_view source = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part id="1"><Staff/><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Rest><durationType>measure</durationType></Rest></voice></Measure></Staff>
<Staff id="2"><Measure><voice><TimeSig><sigN>8</sigN><sigD>4</sigD><stretchN>2</stretchN><stretchD>1</stretchD></TimeSig>
<Rest><durationType>measure</durationType></Rest></voice></Measure></Staff>
</Score></museScore>)";
    auto document = open_fixture(source);
    ASSERT_TRUE(document) << document.error().message;
    const auto &score = read_test::master_of(**document);
    ASSERT_EQ(score.staff(1)->time_stretch({0, 1}), mu::engraving::Fraction(2, 1));
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    EXPECT_EQ(outline->measures.at(0).duration, (Fraction{1, 1}));
    Phrase phrase{.lanes = {{{outline->measures.at(0).id, {0, 1}, 1, 1}, PitchDomain::Concert, {PhraseEvent{{1, 1}, {{c4}}}, BarCheck{}}}}};
    const auto halfway = (*document)->write_phrase(phrase);
    ASSERT_FALSE(halfway);
    EXPECT_EQ(halfway.error().message, "lane 0, item 1: bar check is not at a measure boundary");

    phrase.lanes[0].items.insert(phrase.lanes[0].items.begin() + 1, PhraseEvent{{1, 1}, {{e4}}});
    const auto written = (*document)->write_phrase(phrase);
    ASSERT_TRUE(written) << written.error().message;
    ASSERT_FALSE(written->rejection) << written->rejection->error.message;
}

TEST(MusicWrite, PianoPhraseCrossesPickupAndMeterChangeWithSelectiveTies) {
    auto document = open_fixture(piano_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto source = (*document)->outline();
    ASSERT_TRUE(source) << source.error().message;
    const auto &bars = source->measures;
    const Phrase phrase{.lanes = {
                            {{bars.at(0).id, {0, 1}},
                             PitchDomain::Concert,
                             {PhraseEvent{{1, 2}, {{c4, true}, {e4}}}, PhraseEvent{{1, 4}, {{c4}, {e4}}}, PhraseEvent{{1, 8}},
                              PhraseEvent{{1, 2}, {{g4}}, {}, {{GraceKind::Grace8After, {1, 8}, {e4}}}}}},
                            {{bars.at(1).id, {1, 8}, 0, 1}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{g4}}}}},
                            {{bars.at(1).id, {1, 4}, 1, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{c4}}}}},
                        }};
    const auto revision = (*document)->revision();
    const auto written = (*document)->write_phrase(phrase, {.expected_revision = revision});
    ASSERT_TRUE(written) << written.error().message;
    ASSERT_FALSE(written->rejection) << written->rejection->error.message;
    EXPECT_EQ(written->revision, revision + 1);

    auto reopened = reload(**document);
    ASSERT_TRUE(reopened) << reopened.error().message;
    for (auto *copy : {document->get(), reopened->get()}) {
        const auto window = copy->read_music({.measure_count = 3});
        ASSERT_TRUE(window) << window.error().message;
        const auto &voices = window->measures.at(1).voices;
        ASSERT_EQ(voices.size(), 3u);
        EXPECT_EQ(voices[1].staff, 0);
        EXPECT_EQ(voices[1].voice, 1);
        expect_fraction(voices[1].events.at(1).offset, 1, 8);
        EXPECT_TRUE(std::holds_alternative<RestMusic>(voices[1].events.at(0).body));
        EXPECT_EQ(voices[2].staff, 1);
        EXPECT_EQ(voices[2].voice, 0);
        expect_fraction(voices[2].events.at(1).offset, 1, 4);
        expect_fraction(voices[0].events.at(0).written_duration, 3, 8);
        expect_fraction(window->measures.at(2).measure.duration, 3, 4);

        const auto &score = read_test::master_of(*copy);
        const auto *pickup = chord_at(score, {0, 1}, 0);
        const auto *continued = chord_at(score, {1, 8}, 0);
        const auto *next = chord_at(score, {1, 2}, 0);
        ASSERT_TRUE(pickup && continued && next);
        for (int midi : {60, 64}) {
            const auto *a = pickup->find_note(midi);
            const auto *b = continued->find_note(midi);
            ASSERT_TRUE(a && b && a->tie_for());
            EXPECT_EQ(a->tie_for()->end_note(), b);
        }
        const auto *c = continued->find_note(60);
        ASSERT_TRUE(c->tie_for());
        EXPECT_EQ(c->tie_for()->end_note(), next->find_note(60));
        EXPECT_EQ(continued->find_note(64)->tie_for(), nullptr);
        const auto *crossing = chord_at(score, {7, 8}, 0);
        const auto *crossed = chord_at(score, {9, 8}, 0);
        ASSERT_TRUE(crossing && crossed && crossing->find_note(67)->tie_for());
        EXPECT_EQ(crossing->find_note(67)->tie_for()->end_note(), crossed->find_note(67));
        EXPECT_TRUE(crossing->grace_notes_after().empty());
        const auto after = crossed->grace_notes_after();
        ASSERT_EQ(after.size(), 1u);
        EXPECT_NE(after.front()->find_note(64), nullptr);
    }
}

TEST(MusicWrite, ConcertAndWrittenEnharmonicsRoundTripOnClarinet) {
    auto document = open_fixture(clarinet_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto bar = outline->measures.at(2).id;
    constexpr SpelledPitch b_sharp{StepName::B, 1, 3};
    constexpr SpelledPitch c_flat{StepName::C, -1, 4};
    const Phrase phrase{.lanes = {
                            {{bar, {0, 1}}, PitchDomain::Concert, {PhraseEvent{{1, 8}, {{b_sharp}}}, PhraseEvent{{1, 8}, {{c_flat}}}}},
                            {{bar, {1, 4}}, PitchDomain::Written, {PhraseEvent{{1, 8}, {{b_sharp}}}, PhraseEvent{{1, 8}, {{c_flat}}}}},
                        }};
    const auto edit = (*document)->write_phrase(phrase);
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    auto reopened = reload(**document);
    ASSERT_TRUE(reopened) << reopened.error().message;
    for (auto *copy : {document->get(), reopened->get()}) {
        const auto window = copy->read_music({.first_measure = 2});
        ASSERT_TRUE(window) << window.error().message;
        const auto &events = window->measures.at(0).voices.at(0).events;
        auto pitch_at = [&](std::size_t index) {
            return std::get<PitchedNote>(std::get<ChordMusic>(events.at(index).body).notes.at(0).pitch);
        };
        const auto concert_sharp = pitch_at(0);
        EXPECT_EQ(concert_sharp.midi, 60);
        expect_pitch(concert_sharp.concert, StepName::B, 1, 3);
        expect_pitch(concert_sharp.written, StepName::C, 2, 4);
        const auto concert_flat = pitch_at(1);
        EXPECT_EQ(concert_flat.midi, 59);
        expect_pitch(concert_flat.concert, StepName::C, -1, 4);
        expect_pitch(concert_flat.written, StepName::D, -1, 4);
        const auto written_sharp = pitch_at(2);
        EXPECT_EQ(written_sharp.midi, 58);
        expect_pitch(written_sharp.concert, StepName::A, 1, 3);
        expect_pitch(written_sharp.written, StepName::B, 1, 3);
        const auto written_flat = pitch_at(3);
        EXPECT_EQ(written_flat.midi, 57);
        expect_pitch(written_flat.concert, StepName::B, -2, 3);
        expect_pitch(written_flat.written, StepName::C, -1, 4);
    }
}

TEST(MusicWrite, TupletMembersAndOrderedGracesSurviveNativeSave) {
    auto document = open_fixture(piano_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto bar = outline->measures.at(1).id;
    const TupletGroup triplet{
        .members =
            {
                {{1, 8},
                 {{c4, true}, {g4}},
                 {{GraceKind::Appoggiatura, {1, 8}, {e4}}, {GraceKind::Acciaccatura, {1, 16}, {g4}}},
                 {{GraceKind::Grace8After, {1, 8}, {c4}}, {GraceKind::Grace16After, {1, 16}, {e4}}}},
                {{1, 8}, {{c4}}, {{GraceKind::Appoggiatura, {1, 8}, {c4}}}},
                {{1, 8}},
            },
    };
    const auto edit = (*document)->write_phrase({.lanes = {{{bar, {1, 8}}, PitchDomain::Concert, {triplet}}}});
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;

    auto reopened = reload(**document);
    ASSERT_TRUE(reopened) << reopened.error().message;
    for (auto *copy : {document->get(), reopened->get()}) {
        const auto window = copy->read_music({.first_measure = 1, .staves = {0}});
        ASSERT_TRUE(window) << window.error().message;
        ASSERT_EQ(window->tuplets.size(), 1u);
        const auto &events = window->measures.at(0).voices.at(0).events;
        ASSERT_GE(events.size(), 4u);
        for (std::size_t i = 1; i <= 3; ++i) {
            EXPECT_EQ(events[i].tuplet_path, (std::vector{window->tuplets[0].id}));
            expect_fraction(events[i].written_duration, 1, 8);
            expect_fraction(events[i].elapsed_duration, 1, 12);
        }
        expect_fraction(events[1].offset, 1, 8);
        expect_fraction(events[2].offset, 5, 24);
        expect_fraction(events[3].offset, 7, 24);
        EXPECT_TRUE(std::holds_alternative<RestMusic>(events[3].body));
        const auto &first = std::get<ChordMusic>(events[1].body);
        ASSERT_EQ(first.before.size(), 2u);
        ASSERT_EQ(first.after.size(), 2u);
        EXPECT_EQ(first.before[0].kind, GraceKind::Appoggiatura);
        EXPECT_EQ(first.before[1].kind, GraceKind::Acciaccatura);
        EXPECT_EQ(first.after[0].kind, GraceKind::Grace8After);
        EXPECT_EQ(first.after[1].kind, GraceKind::Grace16After);
        const auto &dom = read_test::master_of(*copy);
        const auto *from = chord_at(dom, {1, 4}, 0);
        const auto *to = chord_at(dom, {1, 3}, 0);
        ASSERT_TRUE(from && to && from->tuplet());
        EXPECT_EQ(from->tuplet(), to->tuplet());
        ASSERT_TRUE(from->find_note(60)->tie_for());
        EXPECT_EQ(from->find_note(60)->tie_for()->end_note(), to->find_note(60));
        EXPECT_EQ(from->find_note(67)->tie_for(), nullptr);
    }
}

TEST(MusicWrite, CustomKitAcceptsKeysAndRejectsMixedPitches) {
    auto document = open_fixture(drum_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    Phrase phrase{
        .lanes = {{{outline->measures.at(0).id, {1, 4}}, PitchDomain::Concert, {PhraseEvent{{1, 8}, {{DrumKey{36}}}}}}},
    };
    const auto edit = (*document)->write_phrase(phrase);
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    auto reopened = reload(**document);
    ASSERT_TRUE(reopened) << reopened.error().message;
    const auto window = (*reopened)->read_music({});
    ASSERT_TRUE(window) << window.error().message;
    const auto &entered = std::get<ChordMusic>(window->measures.at(0).voices.at(0).events.at(1).body);
    ASSERT_EQ(entered.notes.size(), 1u);
    EXPECT_EQ(std::get<DrumNote>(entered.notes[0].pitch).kit_name, "Custom low drum");
    const auto revision = (*document)->revision();
    auto &notes = std::get<PhraseEvent>(phrase.lanes[0].items[0]).notes;
    notes.push_back({c4});
    const auto mixed = (*document)->write_phrase(phrase);
    read_test::expect_code(mixed, ErrorCode::INVALID_ARGUMENT, "mixed pitched and drum notes");
    ASSERT_FALSE(mixed);
    EXPECT_NE(mixed.error().message.find("lane 0, item 0"), std::string::npos);
    notes = {{DrumKey{72}}};
    const auto unmapped = (*document)->write_phrase(phrase);
    read_test::expect_code(unmapped, ErrorCode::INVALID_ARGUMENT, "unmapped kit key");
    EXPECT_EQ((*document)->revision(), revision);
}

TEST(MusicWrite, RejectionLeavesEarlierLanesUnchanged) {
    auto document = open_fixture(piano_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto pickup = outline->measures.at(0).id;
    const auto bar = outline->measures.at(1).id;
    const auto setup = (*document)->edit({{EnterNotes{SegmentAddress{pickup, 1, 0, {1, 16}, "ChordRest"}, {1, 16}, {{60}}}}});
    ASSERT_TRUE(setup) << setup.error().message;
    ASSERT_FALSE(setup->rejection) << setup->rejection->error.message;
    const PhraseLane bass{{bar, {0, 1}, 1, 0}, PitchDomain::Concert, {PhraseEvent{{1, 8}, {{c4}}}}};
    const auto revision = (*document)->revision();
    const Phrase runtime_failure{
        .lanes = {bass, {{pickup, {1, 16}}, PitchDomain::Concert, {PhraseEvent{{1, 16}, {{g4}}}}}},
    };
    const auto rejected = (*document)->write_phrase(runtime_failure);
    ASSERT_TRUE(rejected) << rejected.error().message;
    ASSERT_TRUE(rejected->rejection);
    EXPECT_EQ(rejected->applied, 0u);
    EXPECT_TRUE(rejected->created.empty());
    EXPECT_EQ(rejected->revision, revision);
    EXPECT_NE(rejected->rejection->error.message.find("lane 1, item 0"), std::string::npos);
    EXPECT_NE(rejected->rejection->error.message.find("inside a sounding event"), std::string::npos);
    EXPECT_EQ(chord_at(read_test::master_of(**document), {1, 8}, 4), nullptr);

    const Phrase validation_failure{
        .lanes = {bass,
                  {{bar, {0, 1}}, PitchDomain::Concert, {PhraseEvent{{1, 8}, {{g4}}}, PhraseEvent{{1, 8}}, PhraseEvent{{0, 1}, {{c4}}}}}},
    };
    const auto invalid = (*document)->write_phrase(validation_failure);
    read_test::expect_code(invalid, ErrorCode::INVALID_ARGUMENT, "late invalid duration");
    ASSERT_FALSE(invalid);
    EXPECT_NE(invalid.error().message.find("lane 1, item 2"), std::string::npos);
    EXPECT_EQ((*document)->revision(), revision);
    EXPECT_EQ(chord_at(read_test::master_of(**document), {1, 8}, 4), nullptr);
}

TEST(MusicWrite, APrincipalCannotSustainPastAnInternalInstrumentChange) {
    constexpr std::string_view source = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part>
<Staff id="1"><Measure><voice><Rest><durationType>quarter</durationType></Rest>
<InstrumentChange><Instrument id="drumset"><useDrumset>1</useDrumset></Instrument></InstrumentChange>
<Rest><durationType>half</durationType><dots>1</dots><duration>3/4</duration></Rest>
</voice></Measure></Staff></Score></museScore>)";
    auto document = open_fixture(source);
    ASSERT_TRUE(document) << document.error().message;
    const auto &score = read_test::master_of(**document);
    EXPECT_FALSE(score.staff(0)->is_drum_staff({0, 1}));
    ASSERT_TRUE(score.staff(0)->is_drum_staff({1, 4}));
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto bar = outline->measures.at(0).id;
    const auto revision = (*document)->revision();
    const auto result = (*document)->write_phrase({.lanes = {{{bar, {0, 1}}, PitchDomain::Concert, {PhraseEvent{{1, 2}, {{c4}}}}}}});
    read_test::expect_code(result, ErrorCode::INVALID_ARGUMENT, "instrument change inside a principal duration");
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message.find("lane 0, item 0"), std::string::npos);
    EXPECT_EQ((*document)->revision(), revision);
}

TEST(MusicWrite, HugeFractionTermsAreRejectedBeforeComparingLanes) {
    auto document = open_fixture(piano_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto bar = outline->measures.at(1).id;
    const Phrase phrase{.lanes = {
                            {{bar, {1073741823, 2147483647}}, PitchDomain::Concert, {PhraseEvent{{1, 1024}, {{c4}}}}},
                            {{bar, {1610612735, 2147483647}}, PitchDomain::Concert, {PhraseEvent{{1, 1024}, {{g4}}}}},
                        }};
    const auto revision = (*document)->revision();
    const auto result = (*document)->write_phrase(phrase);
    read_test::expect_code(result, ErrorCode::INVALID_ARGUMENT, "engine fraction range");
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message.find("fraction range"), std::string::npos);
    EXPECT_EQ((*document)->revision(), revision);
}

} // namespace
