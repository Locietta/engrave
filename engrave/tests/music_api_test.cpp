#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include "engrave/tests/core_test_env.h"
#include "engrave/tests/read_test_fixture.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"

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

constexpr std::string_view expressive_fixture = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Upper</trackName><Instrument id="piano"/></Part>
<Part id="2"><Staff/><trackName>Lower</trackName><Instrument id="piano"/></Part>
<Staff id="1">
<Measure><startRepeat/><voice>
<BarLine><subtype>start-repeat</subtype></BarLine>
<Clef><eid>B_A</eid><concertClefType>G</concertClefType><transposingClefType>G</transposingClefType><isHeader>1</isHeader></Clef>
<TimeSig><eid>B_B</eid><sigN>4</sigN><sigD>4</sigD></TimeSig>
<KeySig><eid>B_C</eid><concertKey>-2</concertKey></KeySig>
<Tempo><eid>B_D</eid><tempo>2</tempo><text>Andante</text></Tempo>
<SystemText><eid>B_E</eid><text>With motion</text></SystemText>
<RehearsalMark><eid>B_F</eid><text>A</text></RehearsalMark>
<StaffText><eid>B_G</eid><text><b>leggiero</b> &amp; clear</text></StaffText>
<Expression><eid>B_H</eid><text>dolce</text></Expression>
<Harmony><eid>B_I</eid><harmonyInfo><root>14</root><name>m7</name></harmonyInfo></Harmony>
<FretDiagram><eid>C_A</eid><strings>6</strings><fretDiagram><string no="0"><dot fret="3">normal</dot></string></fretDiagram>
<Harmony><eid>C_B</eid><harmonyInfo><root>15</root><name>7</name></harmonyInfo></Harmony></FretDiagram>
<Dynamic><eid>B_J</eid><subtype>p</subtype></Dynamic>
<Fermata><eid>B_K</eid><subtype>fermataAbove</subtype></Fermata>
<Chord><eid>B_L</eid><durationType>16th</durationType><acciaccatura/>
<Note><pitch>62</pitch><tpc>16</tpc></Note><Articulation><eid>B_M</eid><subtype>articStaccatoAbove</subtype></Articulation></Chord>
<Chord><eid>B_N</eid><durationType>16th</durationType><acciaccatura/>
<Note><pitch>64</pitch><tpc>18</tpc></Note><Ornament><eid>B_O</eid><subtype>ornamentTurn</subtype></Ornament></Chord>
<Chord><eid>B_i</eid><durationType>eighth</durationType><grace8after/>
<Note><pitch>67</pitch><tpc>15</tpc></Note><Articulation><eid>B_j</eid><subtype>articTenutoAbove</subtype></Articulation></Chord>
<Chord><eid>B_P</eid><durationType>quarter</durationType>
<Note><pitch>65</pitch><tpc>13</tpc></Note><Note><pitch>69</pitch><tpc>17</tpc></Note>
<Articulation><eid>B_Q</eid><subtype>articAccentAbove</subtype></Articulation>
<Ornament><eid>B_R</eid><subtype>ornamentMordent</subtype></Ornament>
<Lyrics><eid>B_S</eid><syllabic>begin</syllabic><ticks_f>1/4</ticks_f><text>glo</text></Lyrics>
<Lyrics><eid>B_T</eid><no>1</no><text>light</text></Lyrics>
<Arpeggio><eid>B_U</eid><subtype>1</subtype></Arpeggio>
<TremoloSingleChord><eid>B_V</eid><subtype>r16</subtype></TremoloSingleChord>
</Chord>
<BarLine><subtype>normal</subtype></BarLine>
<Clef><eid>B_W</eid><concertClefType>F</concertClefType><transposingClefType>F</transposingClefType></Clef>
<Dynamic><eid>B_X</eid><subtype>mf</subtype><text>mezzoforte</text></Dynamic>
<Chord><durationType>quarter</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note>
<Lyrics><eid>B_Y</eid><syllabic>middle</syllabic><text>ri</text></Lyrics></Chord>
<Chord><durationType>half</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note>
<Lyrics><eid>B_Z</eid><syllabic>end</syllabic><text>a</text></Lyrics></Chord>
<BarLine><eid>B_a</eid><subtype>double</subtype></BarLine>
</voice><voice>
<Dynamic><eid>B_b</eid><subtype>sfz</subtype><voiceAssignment>currentVoiceOnly</voiceAssignment></Dynamic>
<Chord><durationType>whole</durationType><Note><pitch>55</pitch><tpc>15</tpc></Note></Chord>
</voice></Measure>
<Measure><endRepeat>2</endRepeat><voice>
<Dynamic><eid>B_c</eid><subtype>other</subtype><text>quasi niente</text></Dynamic>
<Chord><eid>B_d</eid><durationType>whole</durationType><duration>1/2</duration><Note><pitch>64</pitch><tpc>18</tpc></Note>
<TremoloTwoChord><eid>B_e</eid><subtype>c16</subtype></TremoloTwoChord></Chord>
<Chord><eid>B_f</eid><durationType>whole</durationType><duration>1/2</duration><Note><pitch>67</pitch><tpc>15</tpc></Note></Chord>
<Clef><eid>B_g</eid><concertClefType>C3</concertClefType><transposingClefType>C3</transposingClefType></Clef>
<BarLine><subtype>end-repeat</subtype></BarLine>
</voice></Measure></Staff>
<Staff id="2"><Measure><voice><StaffText><eid>B_h</eid><text>bass only</text></StaffText>
<Rest><durationType>measure</durationType><duration>1/1</duration></Rest></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType><duration>1/1</duration></Rest></voice></Measure></Staff>
</Score></museScore>)";

const MarkMusic *find_mark(const MeasureMusic &measure, std::string_view id) {
    const auto element = element_id_from_string(id);
    const auto found = std::ranges::find(measure.markings, element, &MarkMusic::id);
    return found == measure.markings.end() ? nullptr : &*found;
}

TEST(MusicRead, ExpressiveMarksCarryTheirAuthoredValuesAndAnchors) {
    auto document = open_fixture(expressive_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto courtesy = resolve(read_test::master_of(**document), *element_id_from_string("B_g"));
    ASSERT_TRUE(courtesy) << courtesy.error().message;
    mu::engraving::to_clef(courtesy->object)->set_is_courtesy(true);
    const auto window = (*document)->read_music({.measure_count = 2});
    ASSERT_TRUE(window) << window.error().message;
    const auto &bar = window->measures.at(0);
    const auto &second = window->measures.at(1);
    EXPECT_EQ(window->staves.at(0).clef, "G");
    const auto later = (*document)->read_music({.first_measure = 1});
    ASSERT_TRUE(later) << later.error().message;
    EXPECT_EQ(later->staves.at(0).clef, "F");
    EXPECT_TRUE(bar.repeat_start);
    EXPECT_TRUE(second.repeat_end);
    EXPECT_EQ(std::ranges::count_if(bar.markings, [](const auto &mark) { return std::holds_alternative<BarlineMark>(mark.payload); }), 1);
    EXPECT_EQ(second.markings.size(), 2u);
    EXPECT_EQ(find_mark(second, "B_g"), nullptr);

    auto mark = [&](std::string_view id) -> const MarkMusic & {
        const auto *row = find_mark(bar, id);
        if (!row) ADD_FAILURE() << "Missing marking " << id;
        if (row) return *row;
        static const MarkMusic empty;
        return empty;
    };
    EXPECT_EQ(std::get<DynamicMark>(mark("B_J").payload).value, "p");
    const auto &modified = std::get<DynamicMark>(mark("B_X").payload);
    EXPECT_EQ(modified.value, "mf");
    EXPECT_EQ(modified.custom_text, "mezzoforte");
    EXPECT_EQ(std::get<DynamicMark>(mark("B_b").payload).value, "sfz");
    ASSERT_NE(find_mark(second, "B_c"), nullptr);
    EXPECT_EQ(std::get<DynamicMark>(find_mark(second, "B_c")->payload).value, "quasi niente");
    EXPECT_EQ(std::get<FermataMark>(mark("B_K").payload).symbol, "fermataAbove");
    EXPECT_EQ(std::get<HarmonyMark>(mark("B_I").payload).text, "Cm7");
    for (const auto &[id, kind, text] : {
             std::tuple{"B_E", TextMark::Kind::SYSTEM, "With motion"},
             std::tuple{"B_F", TextMark::Kind::REHEARSAL, "A"},
             std::tuple{"B_G", TextMark::Kind::STAFF, "leggiero & clear"},
             std::tuple{"B_H", TextMark::Kind::EXPRESSION, "dolce"},
         }) {
        EXPECT_FALSE(mark(id).attachment);
        const auto &value = std::get<TextMark>(mark(id).payload);
        EXPECT_EQ(value.kind, kind);
        EXPECT_EQ(value.text, text);
    }
    for (const auto &[id, symbol, ornament] : {
             std::tuple{"B_M", "articStaccatoAbove", false},
             std::tuple{"B_O", "ornamentTurn", true},
             std::tuple{"B_j", "articTenutoAbove", false},
             std::tuple{"B_Q", "articAccentAbove", false},
             std::tuple{"B_R", "ornamentMordent", true},
         }) {
        const auto &row = mark(id);
        expect_fraction(row.offset, 0, 1);
        EXPECT_EQ(row.scope, MarkScope::VOICE);
        ASSERT_TRUE(row.attachment);
        EXPECT_EQ(row.attachment->event, element_id_from_string("B_P"));
        const auto &value = std::get<ArticulationMark>(row.payload);
        EXPECT_EQ(value.symbol, symbol);
        EXPECT_EQ(value.ornament, ornament);
    }
    for (const auto &[id, ordinal, chord] : {std::tuple{"B_M", 0, "B_L"}, std::tuple{"B_O", 1, "B_N"}}) {
        const auto &grace = mark(id).attachment->grace;
        ASSERT_TRUE(grace);
        EXPECT_FALSE(grace->after);
        EXPECT_EQ(grace->ordinal, ordinal);
        EXPECT_EQ(grace->chord, element_id_from_string(chord));
    }
    const auto &after_grace = mark("B_j").attachment->grace;
    ASSERT_TRUE(after_grace);
    EXPECT_TRUE(after_grace->after);
    EXPECT_EQ(after_grace->ordinal, 0);
    EXPECT_EQ(after_grace->chord, element_id_from_string("B_i"));
    EXPECT_FALSE(mark("B_Q").attachment->grace);
    for (const auto &[id, verse, syllabic, text] : {
             std::tuple{"B_S", 0, "begin", "glo"},
             std::tuple{"B_T", 1, "single", "light"},
             std::tuple{"B_Y", 0, "middle", "ri"},
             std::tuple{"B_Z", 0, "end", "a"},
         }) {
        const auto &value = std::get<LyricMark>(mark(id).payload);
        EXPECT_EQ(value.verse, verse);
        EXPECT_EQ(value.syllabic, syllabic);
        EXPECT_EQ(value.text, text);
        if (std::string_view(id) == "B_S") {
            ASSERT_TRUE(value.melisma_ticks);
            expect_fraction(*value.melisma_ticks, 1, 4);
        } else {
            EXPECT_FALSE(value.melisma_ticks);
        }
    }
    EXPECT_EQ(std::get<ArpeggioMark>(mark("B_U").payload).subtype, "up");
    EXPECT_EQ(std::get<TremoloMark>(mark("B_V").payload).subtype, "r16");
    EXPECT_FALSE(std::get<TremoloMark>(mark("B_V").payload).two_chord);
    const auto *pair = find_mark(second, "B_e");
    ASSERT_NE(pair, nullptr);
    const auto &tremolo = std::get<TremoloMark>(pair->payload);
    EXPECT_TRUE(tremolo.two_chord);
    EXPECT_EQ(tremolo.subtype, "c16");
    EXPECT_EQ(pair->attachment->event, element_id_from_string("B_d"));
    EXPECT_EQ(tremolo.end_event, element_id_from_string("B_f"));
    ASSERT_TRUE(tremolo.end);
    EXPECT_EQ(tremolo.end->measure, second.measure.id);
    EXPECT_EQ(tremolo.end->measure_index, 1);
    expect_fraction(tremolo.end->offset, 1, 2);

    EXPECT_EQ(find_mark(bar, "B_A"), nullptr);
    EXPECT_EQ(std::get<ClefMark>(mark("B_W").payload).clef, "F");
    expect_fraction(mark("B_W").offset, 1, 4);
    EXPECT_EQ(std::get<BarlineMark>(mark("B_a").payload).subtype, "double");
    expect_fraction(mark("B_a").offset, 1, 1);
    ASSERT_GE(bar.markings.size(), 3u);
    EXPECT_EQ(bar.markings[0].id, element_id_from_string("B_E"));
    EXPECT_EQ(bar.markings[1].id, element_id_from_string("B_F"));
    EXPECT_EQ(bar.markings[2].id, element_id_from_string("B_J"));

    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    for (const auto &[type, id] : {std::pair{"TimeSig", "B_B"}, std::pair{"KeySig", "B_C"}, std::pair{"Tempo", "B_D"}}) {
        for (const auto *signatures : {&bar.signature_changes, &outline->signatures}) {
            const auto row = std::ranges::find(*signatures, element_id_from_string(id), &SignatureInfo::id);
            ASSERT_NE(row, signatures->end()) << type;
            EXPECT_EQ(row->type, type);
        }
    }
}

TEST(MusicRead, CourtesySignaturesDoNotAppearInEitherReadView) {
    constexpr std::string_view source = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part>
<Staff id="1"><Measure><voice>
<TimeSig><eid>A_A</eid><sigN>4</sigN><sigD>4</sigD><showCourtesySig>0</showCourtesySig></TimeSig>
<KeySig><eid>A_B</eid><concertKey>-1</concertKey></KeySig>
<Rest><durationType>measure</durationType><duration>1/1</duration></Rest>
<TimeSig><eid>A_C</eid><sigN>3</sigN><sigD>4</sigD><isCourtesy>1</isCourtesy></TimeSig>
<KeySig><eid>A_D</eid><concertKey>2</concertKey><isCourtesy>1</isCourtesy></KeySig>
</voice></Measure><Measure><voice>
<TimeSig><eid>A_E</eid><sigN>3</sigN><sigD>4</sigD></TimeSig>
<KeySig><eid>A_F</eid><concertKey>2</concertKey></KeySig>
<Rest><durationType>measure</durationType><duration>3/4</duration></Rest>
</voice></Measure></Staff></Score></museScore>)";
    const auto document = open_fixture(source);
    ASSERT_TRUE(document) << document.error().message;
    const auto *measure = read_test::master_of(**document).first_measure();
    for (const auto [type, id] : {std::pair{mu::engraving::SegmentType::TIME_SIG_ANNOUNCE, "A_C"},
                                  std::pair{mu::engraving::SegmentType::KEY_SIG_ANNOUNCE, "A_D"}}) {
        const auto *segment = measure->first(type);
        ASSERT_NE(segment, nullptr);
        const auto *item = segment->element(0);
        ASSERT_NE(item, nullptr);
        EXPECT_FALSE(item->generated());
        EXPECT_EQ(from_engine(item->eid()), *element_id_from_string(id));
    }
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto window = (*document)->read_music({.measure_count = 2});
    ASSERT_TRUE(window) << window.error().message;
    ASSERT_EQ(outline->signatures.size(), 4u);
    for (int bar = 0; bar < 2; ++bar) {
        const auto &changes = window->measures.at(bar).signature_changes;
        ASSERT_EQ(changes.size(), 2u);
        for (auto id : bar == 0 ? std::array{"A_A", "A_B"} : std::array{"A_E", "A_F"}) {
            EXPECT_NE(std::ranges::find(changes, element_id_from_string(id), &SignatureInfo::id), changes.end());
            EXPECT_NE(std::ranges::find(outline->signatures, element_id_from_string(id), &SignatureInfo::id), outline->signatures.end());
        }
    }
}

TEST(MusicRead, OpeningHeaderClefRemainsRemovableThroughItsSourceId) {
    const auto document = open_fixture(expressive_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto id = *element_id_from_string("B_A");
    const auto clef = (*document)->get(id);
    ASSERT_TRUE(clef) << clef.error().message;
    const auto edit = (*document)->edit({{Remove{id}}});
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    EXPECT_FALSE((*document)->get(id));
}

TEST(MusicRead, MarksRespectStaffVoiceAndExcerptSelections) {
    auto document = open_fixture(expressive_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto system_count = [](const MeasureMusic &measure) {
        return std::ranges::count_if(measure.markings, [](const auto &mark) { return mark.scope == MarkScope::SYSTEM; });
    };
    const auto all = (*document)->read_music({});
    ASSERT_TRUE(all) << all.error().message;
    EXPECT_EQ(system_count(all->measures.front()), 2);
    const auto voice = (*document)->read_music({.staves = {0}, .voices = {1}});
    ASSERT_TRUE(voice) << voice.error().message;
    const auto &bar = voice->measures.front();
    EXPECT_EQ(system_count(bar), 2);
    EXPECT_NE(find_mark(bar, "B_J"), nullptr);
    EXPECT_NE(find_mark(bar, "B_K"), nullptr);
    EXPECT_EQ(find_mark(bar, "B_Q"), nullptr);
    EXPECT_EQ(find_mark(bar, "B_h"), nullptr);
    const auto *sfz = find_mark(bar, "B_b");
    ASSERT_NE(sfz, nullptr);
    EXPECT_EQ(sfz->scope, MarkScope::VOICE);
    EXPECT_EQ(sfz->voice, 1);
    const auto principal = (*document)->read_music({.voices = {0}});
    ASSERT_TRUE(principal) << principal.error().message;
    EXPECT_EQ(find_mark(principal->measures.front(), "B_b"), nullptr);
    const auto lower = (*document)->read_music({.staves = {1}, .voices = {1}});
    ASSERT_TRUE(lower) << lower.error().message;
    EXPECT_EQ(system_count(lower->measures.front()), 2);
    EXPECT_NE(find_mark(lower->measures.front(), "B_h"), nullptr);
    EXPECT_EQ(find_mark(lower->measures.front(), "B_J"), nullptr);
    for (const auto &mark : lower->measures.front().markings) {
        if (mark.scope == MarkScope::SYSTEM) {
            EXPECT_EQ(mark.staff, -1);
            EXPECT_EQ(mark.voice, -1);
        }
    }
    const auto outline = (*document)->outline();
    ASSERT_TRUE(outline) << outline.error().message;
    const auto edit = (*document)->edit({{GenerateExcerpts{{outline->parts.at(1).id}}}});
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    const auto after = (*document)->outline();
    ASSERT_TRUE(after) << after.error().message;
    const auto excerpt = (*document)->read_music({.score = {after->excerpts.at(0).id}});
    ASSERT_TRUE(excerpt) << excerpt.error().message;
    const auto &excerpt_bar = excerpt->measures.front();
    EXPECT_EQ(system_count(excerpt_bar), 2);
    EXPECT_TRUE(std::ranges::any_of(excerpt_bar.markings, [](const auto &mark) {
        const auto *text = std::get_if<TextMark>(&mark.payload);
        return text && text->text == "bass only" && mark.staff == 0 && mark.id;
    }));
    EXPECT_FALSE(
        std::ranges::any_of(excerpt_bar.markings, [](const auto &mark) { return std::holds_alternative<DynamicMark>(mark.payload); }));
}

TEST(MusicRead, ReturnedMarkIdsTargetTheirOwnObjectsAcrossNativeSave) {
    auto document = open_fixture(expressive_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto before = (*document)->read_music({});
    ASSERT_TRUE(before) << before.error().message;
    const auto &bar = before->measures.front();
    const auto mark_id = [&](std::string_view id) { return *find_mark(bar, id)->id; };
    for (auto id : {"B_G", "B_T", "B_K", "B_Q", "B_F", "B_H", "B_I", "B_J", "C_B"}) ASSERT_NE(find_mark(bar, id), nullptr) << id;
    EXPECT_EQ(find_mark(bar, "C_A"), nullptr);
    EXPECT_EQ(std::get<HarmonyMark>(find_mark(bar, "C_B")->payload).text, "G7");
    const auto signature = [](const MeasureMusic &measure, std::string_view kind) -> const SignatureInfo * {
        const auto found = std::ranges::find(measure.signature_changes, kind, &SignatureInfo::type);
        return found == measure.signature_changes.end() ? nullptr : &*found;
    };
    for (auto kind : {"TimeSig", "KeySig", "Tempo"}) {
        ASSERT_NE(signature(bar, kind), nullptr);
        ASSERT_TRUE(signature(bar, kind)->id);
    }
    const auto edited = (*document)->edit(
        {{SetProperty{*signature(bar, "TimeSig")->id, "timesig", Fraction{2, 2}},
          SetProperty{*signature(bar, "KeySig")->id, "concertKey", 3}, SetProperty{*signature(bar, "Tempo")->id, "tempo", 2.5},
          SetProperty{mark_id("B_G"), "text", std::string("sostenuto")}, SetProperty{mark_id("B_T"), "text", std::string("shine")},
          SetProperty{mark_id("B_F"), "text", std::string("B")}, SetProperty{mark_id("B_H"), "text", std::string("cantabile")},
          SetProperty{mark_id("B_J"), "DYNAMIC_TYPE", Enum{"f"}}, SetProperty{mark_id("B_I"), "text", std::string("Dm7")},
          SetProperty{mark_id("C_B"), "text", std::string("A7")}, Remove{mark_id("B_K")}, Remove{mark_id("B_Q")}}});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;
    for (const auto format : {Format::MSCX, Format::MSCZ}) {
        const auto saved = (*document)->export_score({.format = format, .native_version = "5.00"});
        ASSERT_TRUE(saved) << saved.error().message;
        const auto reopened = test::core_env().engine->open(*saved, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        const auto after = (*reopened)->read_music({});
        ASSERT_TRUE(after) << after.error().message;
        const auto &restored = after->measures.front();
        for (auto kind : {"TimeSig", "KeySig", "Tempo"}) {
            ASSERT_NE(signature(restored, kind), nullptr);
            EXPECT_EQ(signature(restored, kind)->id, signature(bar, kind)->id);
        }
        ASSERT_TRUE(signature(restored, "TimeSig")->timesig);
        expect_fraction(*signature(restored, "TimeSig")->timesig, 2, 2);
        EXPECT_EQ(signature(restored, "KeySig")->key, 3);
        EXPECT_DOUBLE_EQ(signature(restored, "Tempo")->bpm, 150);
        for (auto id : {"B_G", "B_T", "B_F", "B_H", "B_R", "B_I", "B_J", "B_S", "C_B"}) ASSERT_NE(find_mark(restored, id), nullptr) << id;
        EXPECT_EQ(std::get<TextMark>(find_mark(restored, "B_G")->payload).text, "sostenuto");
        EXPECT_EQ(std::get<LyricMark>(find_mark(restored, "B_T")->payload).text, "shine");
        EXPECT_EQ(std::get<TextMark>(find_mark(restored, "B_F")->payload).text, "B");
        EXPECT_EQ(std::get<TextMark>(find_mark(restored, "B_H")->payload).text, "cantabile");
        EXPECT_EQ(std::get<ArticulationMark>(find_mark(restored, "B_R")->payload).symbol, "ornamentMordent");
        EXPECT_EQ(std::get<DynamicMark>(find_mark(restored, "B_J")->payload).value, "f");
        EXPECT_EQ(std::get<LyricMark>(find_mark(restored, "B_S")->payload).text, "glo");
        EXPECT_EQ(std::get<HarmonyMark>(find_mark(restored, "B_I")->payload).text, "Dm7");
        EXPECT_EQ(std::get<HarmonyMark>(find_mark(restored, "C_B")->payload).text, "A7");
        EXPECT_EQ(find_mark(restored, "B_K"), nullptr);
        EXPECT_EQ(find_mark(restored, "B_Q"), nullptr);

        const auto removed = (*reopened)->edit({{Remove{*find_mark(restored, "B_I")->id}, Remove{*find_mark(restored, "C_B")->id}}});
        ASSERT_TRUE(removed) << removed.error().message;
        ASSERT_FALSE(removed->rejection) << removed->rejection->error.message;
        const auto remaining = (*reopened)->read_music({});
        ASSERT_TRUE(remaining) << remaining.error().message;
        EXPECT_EQ(find_mark(remaining->measures.front(), "B_I"), nullptr);
        EXPECT_EQ(find_mark(remaining->measures.front(), "C_B"), nullptr);
        const auto diagram = (*reopened)->get(*element_id_from_string("C_A"));
        ASSERT_TRUE(diagram) << diagram.error().message;
    }
}

TEST(MusicRead, LinkedAndGeneratedCopiesDoNotCreateEditableMarkings) {
    auto document = open_fixture(expressive_fixture);
    ASSERT_TRUE(document) << document.error().message;
    auto &master = read_test::master_of(**document);
    const auto system = resolve(master, *element_id_from_string("B_E"));
    const auto staff = resolve(master, *element_id_from_string("B_G"));
    ASSERT_TRUE(system) << system.error().message;
    ASSERT_TRUE(staff) << staff.error().message;
    auto *original = mu::engraving::to_engraving_item(system->object);
    auto *copy = original->clone();
    copy->set_track(mu::engraving::staff2track(1));
    mu::engraving::to_segment(original->ownership_parent())->add(copy);
    copy->link_to(original);
    const auto copy_id = from_engine(copy->assign_new_eid());

    auto *staff_text = mu::engraving::to_engraving_item(staff->object);
    master.eid_register()->remove_item(staff_text);
    auto *generated = mu::engraving::to_text_base(staff_text->clone());
    generated->set_plain_text(muse::String(u"layout copy"));
    generated->set_generated(true);
    mu::engraving::to_segment(staff_text->ownership_parent())->add(generated);
    std::ignore = generated->assign_new_eid();

    const auto window = (*document)->read_music({});
    ASSERT_TRUE(window) << window.error().message;
    const auto &bar = window->measures.front();
    ASSERT_NE(find_mark(bar, "B_E"), nullptr);
    EXPECT_TRUE(std::ranges::none_of(bar.markings, [&](const auto &mark) { return mark.id == copy_id; }));
    const auto staff_mark = std::ranges::find_if(bar.markings, [](const auto &mark) {
        const auto *text = std::get_if<TextMark>(&mark.payload);
        return text && text->text == "leggiero & clear";
    });
    ASSERT_NE(staff_mark, bar.markings.end());
    EXPECT_FALSE(staff_mark->id);
    EXPECT_TRUE(std::ranges::none_of(bar.markings, [](const auto &mark) {
        const auto *text = std::get_if<TextMark>(&mark.payload);
        return text && text->text == "layout copy";
    }));
}

constexpr std::string_view scoped_piano_fixture = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff/><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part>
<Staff id="1"><Measure><voice>
<Dynamic><eid>A_A</eid><subtype>p</subtype></Dynamic>
<Dynamic><eid>A_B</eid><subtype>mf</subtype><voiceAssignment>allInStaff</voiceAssignment></Dynamic>
<Expression><eid>A_C</eid><text>dolce</text><voiceAssignment>allInInstrument</voiceAssignment></Expression>
<Expression><eid>A_D</eid><text>espressivo</text><voiceAssignment>allInStaff</voiceAssignment></Expression>
<StaffText><eid>A_I</eid><text>simile</text></StaffText>
<Chord><eid>B_A</eid><durationType>whole</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note>
<Articulation><eid>B_B</eid><subtype>articStaccatoAbove</subtype></Articulation>
<Lyrics><eid>B_C</eid><text>ah</text></Lyrics></Chord>
</voice><voice>
<Dynamic><eid>A_E</eid><subtype>sfz</subtype><voiceAssignment>currentVoiceOnly</voiceAssignment></Dynamic>
<Expression><eid>A_F</eid><text>marcato</text><voiceAssignment>currentVoiceOnly</voiceAssignment></Expression>
<Chord><durationType>whole</durationType><Note><pitch>67</pitch><tpc>15</tpc></Note></Chord>
</voice></Measure></Staff>
<Staff id="2"><Measure><voice>
<StaffText><eid>A_J</eid><text>simile</text></StaffText>
<Chord><eid>C_A</eid><durationType>whole</durationType><Note><pitch>48</pitch><tpc>14</tpc></Note>
<Articulation><eid>C_B</eid><subtype>articStaccatoAbove</subtype></Articulation>
<Lyrics><eid>C_C</eid><text>ah</text></Lyrics></Chord>
</voice></Measure></Staff>
</Score></museScore>)";

TEST(MusicRead, VoiceAssignmentsCoverSelectedTracksAcrossThePart) {
    const auto document = open_fixture(scoped_piano_fixture);
    ASSERT_TRUE(document) << document.error().message;

    struct Case {
        MusicRange range;
        bool staff_wide;
        bool voice_only;
    };

    for (const auto &test : {Case{{.staves = {0}}, true, true}, Case{{.staves = {1}}, false, false},
                             Case{{.staves = {0}, .voices = {0}}, true, false}, Case{{.staves = {0}, .voices = {1}}, true, true},
                             Case{{.staves = {1}, .voices = {1}}, false, false}, Case{{.staves = {0, 1}}, true, true}}) {
        const auto window = (*document)->read_music(test.range);
        ASSERT_TRUE(window) << window.error().message;
        const auto &bar = window->measures.front();
        for (const auto &[id, scope, present] : {
                 std::tuple{"A_A", MarkScope::INSTRUMENT, true},
                 std::tuple{"A_C", MarkScope::INSTRUMENT, true},
                 std::tuple{"A_B", MarkScope::STAFF, test.staff_wide},
                 std::tuple{"A_D", MarkScope::STAFF, test.staff_wide},
                 std::tuple{"A_E", MarkScope::VOICE, test.voice_only},
                 std::tuple{"A_F", MarkScope::VOICE, test.voice_only},
             }) {
            SCOPED_TRACE(id);
            const auto *mark = find_mark(bar, id);
            EXPECT_EQ(mark != nullptr, present);
            if (!mark) continue;
            EXPECT_EQ(mark->scope, scope);
            EXPECT_EQ(mark->staff, 0);
            EXPECT_EQ(mark->voice, scope == MarkScope::VOICE ? 1 : -1);
        }
    }
}

TEST(MusicRead, LinkedStaffMarkingsRetainTheirOwnIdsAndAttachments) {
    const auto document = open_fixture(scoped_piano_fixture);
    ASSERT_TRUE(document) << document.error().message;
    auto &score = read_test::master_of(**document);
    score.staff(1)->link_to(score.staff(0));
    for (const auto &[upper, lower] :
         {std::pair{"B_A", "C_A"}, std::pair{"B_B", "C_B"}, std::pair{"B_C", "C_C"}, std::pair{"A_I", "A_J"}}) {
        const auto source = resolve(score, *element_id_from_string(upper));
        const auto copy = resolve(score, *element_id_from_string(lower));
        ASSERT_TRUE(source) << source.error().message;
        ASSERT_TRUE(copy) << copy.error().message;
        copy->object->link_to(source->object);
    }
    const auto dynamic = resolve(score, *element_id_from_string("A_A"));
    ASSERT_TRUE(dynamic) << dynamic.error().message;
    auto *original = mu::engraving::to_engraving_item(dynamic->object);
    auto *copy = original->clone();
    copy->set_track(mu::engraving::staff2track(1));
    mu::engraving::to_segment(original->ownership_parent())->add(copy);
    copy->link_to(original);
    const auto copy_id = from_engine(copy->assign_new_eid());

    const auto combined = (*document)->read_music({.staves = {0, 1}});
    ASSERT_TRUE(combined) << combined.error().message;
    for (int staff = 0; staff < 2; ++staff) {
        const auto single = (*document)->read_music({.staves = {staff}});
        ASSERT_TRUE(single) << single.error().message;
        EXPECT_NE(find_mark(single->measures.front(), "A_A"), nullptr);
        EXPECT_EQ(std::ranges::find(single->measures.front().markings, copy_id, &MarkMusic::id), single->measures.front().markings.end());
        const auto event = element_id_from_string(staff == 0 ? "B_A" : "C_A");
        const auto ids = staff == 0 ? std::array{"B_B", "B_C", "A_I"} : std::array{"C_B", "C_C", "A_J"};
        for (const auto *measure : {&single->measures.front(), &combined->measures.front()}) {
            for (const auto *id : ids) {
                SCOPED_TRACE(id);
                const auto *mark = find_mark(*measure, id);
                ASSERT_NE(mark, nullptr);
                EXPECT_EQ(mark->staff, staff);
                if (std::holds_alternative<TextMark>(mark->payload)) {
                    EXPECT_EQ(mark->scope, MarkScope::STAFF);
                    EXPECT_FALSE(mark->attachment);
                } else {
                    EXPECT_EQ(mark->scope, MarkScope::VOICE);
                    EXPECT_EQ(mark->voice, 0);
                    ASSERT_TRUE(mark->attachment);
                    EXPECT_EQ(mark->attachment->event, event);
                }
            }
        }
        EXPECT_EQ(find_mark(single->measures.front(), staff == 0 ? "C_C" : "B_C"), nullptr);
    }
}

TEST(MusicRead, LinkedSystemTextCopiesConsumeTheScanBudget) {
    const auto document = open_fixture(expressive_fixture);
    ASSERT_TRUE(document) << document.error().message;
    auto &master = read_test::master_of(**document);
    const auto item = resolve(master, *element_id_from_string("B_E"));
    ASSERT_TRUE(item) << item.error().message;
    auto *original = mu::engraving::to_text_base(item->object);
    auto *segment = mu::engraving::to_segment(original->ownership_parent());
    const std::string text(8 * 1024, 'a');
    original->set_plain_text(muse::String::from_std_string(text));
    for (int i = 0; i < 1024; ++i) {
        auto *copy = original->clone();
        copy->set_track(mu::engraving::staff2track(1));
        segment->add(copy);
        copy->link_to(original);
        std::ignore = copy->assign_new_eid();
        if (i == 1) {
            const auto small = (*document)->read_music({});
            ASSERT_TRUE(small) << small.error().message;
            EXPECT_EQ(std::ranges::count_if(small->measures.front().markings,
                                            [&](const auto &mark) {
                                                const auto *value = std::get_if<TextMark>(&mark.payload);
                                                return value && value->text == text;
                                            }),
                      1);
        }
    }
    const auto limited = (*document)->read_music({});
    read_test::expect_code(limited, ErrorCode::RESOURCE_LIMIT, "linked text scan work");
    const auto other_bar = (*document)->read_music({.first_measure = 1});
    ASSERT_TRUE(other_bar) << other_bar.error().message;
}

TEST(MusicRead, MarkingRowsAndUtf8TextConsumeTheWindowBudget) {
    std::string xml = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part><Staff id="1">)";
    for (int bar = 0; bar < 128; ++bar)
        xml += "<Measure><voice><Rest><durationType>measure</durationType><duration>1/1</duration></Rest></voice></Measure>";
    xml += "</Staff></Score></museScore>";
    std::string utf8;
    for (int i = 0; i < 2048; ++i) utf8 += "\xc3\xa9";
    for (const auto &[rows, text] : {std::pair{260, std::string("marked")}, std::pair{17, std::move(utf8)}}) {
        const auto document = open_fixture(xml);
        ASSERT_TRUE(document) << document.error().message;
        const auto content = muse::String::from_std_string(text);
        auto &master = read_test::master_of(**document);
        for (auto *measure = master.first_measure(); measure; measure = measure->next_measure()) {
            auto *segment = measure->first(mu::engraving::SegmentType::CHORD_REST);
            ASSERT_NE(segment, nullptr);
            for (int row = 0; row < rows; ++row) {
                auto *mark = mu::engraving::Factory::create_staff_text(segment);
                mark->set_track(0);
                mark->set_plain_text(content);
                segment->add(mark);
            }
        }
        const auto small = (*document)->read_music({});
        ASSERT_TRUE(small) << small.error().message;
        EXPECT_EQ(small->measures.front().markings.size(), static_cast<std::size_t>(rows));
        const auto large = (*document)->read_music({.measure_count = 128});
        read_test::expect_code(large, ErrorCode::RESOURCE_LIMIT, rows == 17 ? "UTF-8 marking bytes" : "marking rows");
    }
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
        EXPECT_EQ(volta.span.start.measure_index, 1);
        EXPECT_EQ(volta.span.end.measure_index, 1);
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

TEST(MusicRead, NarrowWindowKeepsDistantEndpointsWithoutReadingTheirTempoText) {
    constexpr int bar_count = 64;
    constexpr int tempos_per_bar = 33;
    constexpr int tempo_bytes = 4096;
    static_assert((bar_count - 1) * tempos_per_bar * tempo_bytes > 8 * 1024 * 1024);
    std::string xml = R"(<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part><Staff id="1">)";
    for (int bar = 0; bar < bar_count; ++bar)
        xml += "<Measure><voice><Chord><durationType>whole</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note>"
               "</Chord></voice></Measure>";
    xml += "</Staff></Score></museScore>";
    const auto document = open_fixture(xml);
    ASSERT_TRUE(document) << document.error().message;
    const auto edited = (*document)->edit({{AddSpanner{"Slur", RelativePosition{{0}}, RelativePosition{{bar_count - 1}}, {}}}});
    ASSERT_TRUE(edited) << edited.error().message;
    ASSERT_FALSE(edited->rejection) << edited->rejection->error.message;

    auto &score = read_test::master_of(**document);
    const auto content = muse::String::from_std_string(std::string(tempo_bytes, 'a'));
    auto *measure = score.first_measure();
    for (int bar = 0; measure; ++bar, measure = measure->next_measure()) {
        if (bar == 1) continue;
        auto *segment = measure->first(mu::engraving::SegmentType::CHORD_REST);
        ASSERT_NE(segment, nullptr);
        for (int row = 0; row < tempos_per_bar; ++row) {
            auto *tempo = mu::engraving::Factory::create_tempo_text(segment);
            tempo->set_track(0);
            tempo->set_xml_text(content);
            segment->add(tempo);
        }
    }
    const auto window = (*document)->read_music({.first_measure = 1});
    ASSERT_TRUE(window) << window.error().message;
    ASSERT_EQ(window->measures.size(), 1u);
    EXPECT_TRUE(window->measures.front().signature_changes.empty());
    ASSERT_EQ(window->spanners.size(), 1u);
    const auto &slur = window->spanners.front();
    EXPECT_EQ(slur.type, "Slur");
    EXPECT_EQ(slur.start.measure_index, 0);
    EXPECT_EQ(slur.end.measure_index, bar_count - 1);
}

TEST(MusicRead, OnlyOptInWindowsClampAtTheEndOfTheSelectedScore) {
    const auto document = open_fixture(clarinet_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto window = (*document)->read_music({.first_measure = 2, .measure_count = 4, .clamp_to_score_end = true});
    ASSERT_TRUE(window) << window.error().message;
    ASSERT_EQ(window->measures.size(), 1u);
    EXPECT_EQ(window->measures[0].measure.index, 2);

    const auto strict = (*document)->read_music({.first_measure = 2, .measure_count = 4});
    read_test::expect_code(strict, ErrorCode::INVALID_ARGUMENT, "count past the end");
    const auto past_end = (*document)->read_music({.first_measure = 3, .measure_count = 4, .clamp_to_score_end = true});
    read_test::expect_code(past_end, ErrorCode::INVALID_ARGUMENT, "starting bar past the end");
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
    EXPECT_EQ(line.subtype, "straight");
    EXPECT_EQ(line.start_element, element_id_from_string("A_B"));
    EXPECT_EQ(line.end_element, element_id_from_string("A_C"));
    EXPECT_EQ(line.start.measure_index, 0);
    EXPECT_EQ(line.end.measure_index, 2);
    const auto &bar = window->measures.at(0);
    const auto &body = std::get<RestMusic>(bar.voices.at(0).events.at(0).body);
    EXPECT_EQ(body.measure_repeat_count, 1);

    const auto full = (*document)->read_music({.measure_count = 3});
    ASSERT_TRUE(full) << full.error().message;
    const auto start = full->measures[0].voices.at(0).events.at(0).id;
    const auto end = full->measures[2].voices.at(0).events.at(0).id;
    const Batch batch{{
        SetProperty{line.id, "GLISS_TYPE", Enum{"1"}},
        SetProperty{line.id, "GLISS_TEXT", std::string("<i>glide</i>")},
        AddSpanner{"Slur", start, end, {{"up", Enum{"down"}}}},
        AddSpanner{"HairPin",
                   start,
                   end,
                   {{"hairpinCircledTip", true}, {"beginText", std::string("grow")}, {"continueText", std::string("(grow)")}}},
        AddSpanner{"Ottava", start, end, {{"OTTAVA_TYPE", 2}}},
        AddSpanner{"Trill", start, end, {{"TRILL_TYPE", 3}}},
        AddSpanner{"Pedal",
                   start,
                   end,
                   {{"beginText", std::string("<sym>keyboardPedalPed</sym>")},
                    {"continueText", std::string("<i>hold</i>")},
                    {"endText", std::string("<sym>keyboardPedalUp</sym>")},
                    {"beginHookType", Enum{"1"}},
                    {"endHookType", Enum{"2"}}}},
        AddSpanner{"TextLine",
                   start,
                   end,
                   {{"beginText", std::string("<b>rit.</b>")},
                    {"continueText", std::string("slow &amp; clear")},
                    {"endText", std::string("ord.")},
                    {"beginHookType", Enum{"2"}},
                    {"endHookType", Enum{"1"}}}},
    }};
    const auto edit = (*document)->edit(batch);
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    const auto crossing = (*document)->read_music({.first_measure = 1});
    ASSERT_TRUE(crossing) << crossing.error().message;
    ASSERT_EQ(crossing->spanners.size(), 7u);
    for (const auto &span : crossing->spanners) {
        SCOPED_TRACE(span.type);
        EXPECT_EQ(span.start.measure, full->measures[0].measure.id);
        EXPECT_EQ(span.end.measure, full->measures[2].measure.id);
        expect_fraction(span.start.offset, 0, 1);
        expect_fraction(span.end.offset, 0, 1);
        if (span.type == "Slur") {
            ASSERT_TRUE(span.detail);
            EXPECT_EQ(span.detail->direction, "down");
            EXPECT_EQ(span.start_element, start);
            EXPECT_EQ(span.end_element, end);
        } else if (span.type == "Glissando") {
            EXPECT_EQ(span.subtype, "wavy");
            ASSERT_TRUE(span.detail);
            EXPECT_EQ(span.detail->text, "glide");
            EXPECT_EQ(span.start_element, element_id_from_string("A_B"));
            EXPECT_EQ(span.end_element, element_id_from_string("A_C"));
        } else if (span.type == "HairPin") {
            EXPECT_EQ(span.subtype, "crescendo");
            ASSERT_TRUE(span.detail);
            EXPECT_TRUE(span.detail->niente);
            EXPECT_EQ(span.detail->begin_text, "grow");
            EXPECT_EQ(span.detail->continue_text, "(grow)");
        } else if (span.type == "Ottava") {
            EXPECT_EQ(span.subtype, "15ma");
            EXPECT_FALSE(span.detail);
        } else if (span.type == "Trill") {
            EXPECT_EQ(span.subtype, "prallprall");
            EXPECT_FALSE(span.detail);
        } else {
            ASSERT_TRUE(span.detail);
            if (span.type == "Pedal") {
                EXPECT_EQ(span.detail->begin_text, "Ped.");
                EXPECT_EQ(span.detail->continue_text, "hold");
                EXPECT_EQ(span.detail->end_text, "*");
                EXPECT_EQ(span.detail->begin_hook, "hook_90");
                EXPECT_EQ(span.detail->end_hook, "hook_45");
            } else {
                EXPECT_EQ(span.type, "TextLine");
                EXPECT_EQ(span.detail->begin_text, "rit.");
                EXPECT_EQ(span.detail->continue_text, "slow & clear");
                EXPECT_EQ(span.detail->end_text, "ord.");
                EXPECT_EQ(span.detail->begin_hook, "hook_45");
                EXPECT_EQ(span.detail->end_hook, "hook_90");
            }
        }
    }
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

TEST(MusicWrite, LinkedStaffLanesCannotOverlap) {
    const auto document =
        test::core_env().engine->open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document) << document.error().message;
    auto &master = read_test::master_of(**document);
    ASSERT_NE(master.staff(0)->links(), nullptr);
    ASSERT_EQ(master.staff(0)->links(), master.staff(1)->links());
    const auto bar = (*document)->outline()->measures.at(0).id;
    const auto revision = (*document)->revision();
    const auto *original = master.find_cr({0, 1}, 0);
    ASSERT_NE(original, nullptr);
    const auto edit = (*document)->write_phrase({.lanes = {
                                                     {{bar, {0, 1}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{c4}}}}},
                                                     {{bar, {0, 1}, 1, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{g4}}}}},
                                                 }});
    read_test::expect_code(edit, ErrorCode::INVALID_ARGUMENT, "overlapping linked tracks");
    ASSERT_FALSE(edit);
    EXPECT_NE(edit.error().message.find("lane 1: the lane overlaps"), std::string::npos);
    EXPECT_EQ((*document)->revision(), revision);
    EXPECT_EQ(master.find_cr({0, 1}, 0), original);
}

TEST(MusicWrite, OverlapSweepReportsEarliestConflictingLaneInInputOrder) {
    constexpr std::string_view source = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig><Rest><durationType>measure</durationType></Rest></voice></Measure>
</Staff></Score></museScore>)";
    const auto document = open_fixture(source);
    ASSERT_TRUE(document) << document.error().message;
    const auto bar = (*document)->outline()->measures.at(0).id;
    const auto revision = (*document)->revision();

    // Nested intervals in reverse tick order: lane 1 overlaps lane 0 and lane 2; lane 1 is earliest in input order.
    const auto nested = (*document)->write_phrase({.lanes = {
                                                       {{bar, {2, 4}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{c4}}}}},
                                                       {{bar, {0, 1}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{4, 4}, {{e4}}}}},
                                                       {{bar, {1, 4}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{g4}}}}},
                                                   }});
    read_test::expect_code(nested, ErrorCode::INVALID_ARGUMENT, "nested reverse-tick overlap");
    ASSERT_FALSE(nested);
    EXPECT_NE(nested.error().message.find("lane 1: the lane overlaps"), std::string::npos);

    // Expired earlier lane [0, 1/4] precedes active overlap between lanes 1 and 2.
    const auto after_expired = (*document)->write_phrase({.lanes = {
                                                              {{bar, {0, 1}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{c4}}}}},
                                                              {{bar, {2, 4}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{2, 4}, {{e4}}}}},
                                                              {{bar, {3, 4}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{g4}}}}},
                                                          }});
    read_test::expect_code(after_expired, ErrorCode::INVALID_ARGUMENT, "overlap after expired lane");
    ASSERT_FALSE(after_expired);
    EXPECT_NE(after_expired.error().message.find("lane 2: the lane overlaps"), std::string::npos);

    // Earlier overlap between lanes 0 and 1 must be reported ahead of a later invalid lane 2.
    const auto before_invalid = (*document)->write_phrase({.lanes = {
                                                               {{bar, {0, 1}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{c4}}}}},
                                                               {{bar, {0, 1}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{e4}}}}},
                                                               {{bar, {2, 4}, 0, 0}, PitchDomain::Concert, {PhraseEvent{{0, 1}, {{g4}}}}},
                                                           }});
    read_test::expect_code(before_invalid, ErrorCode::INVALID_ARGUMENT, "overlap before invalid lane");
    ASSERT_FALSE(before_invalid);
    EXPECT_NE(before_invalid.error().message.find("lane 1: the lane overlaps"), std::string::npos);
    EXPECT_EQ((*document)->revision(), revision);
}

TEST(MusicWrite, RestSplitWithLyricsRejectsAtomically) {
    constexpr std::string_view source = R"(<museScore version="4.70"><Score><Division>480</Division>
<Part id="1"><Staff/><trackName>Piano</trackName><Instrument id="piano"/></Part>
<Staff id="1"><Measure><voice><Rest><durationType>measure</durationType></Rest></voice></Measure>
<Measure><voice><Rest><durationType>measure</durationType><Lyrics><text>onset</text></Lyrics></Rest></voice></Measure>
</Staff></Score></museScore>)";
    const auto document = open_fixture(source);
    ASSERT_TRUE(document) << document.error().message;
    const auto bars = (*document)->outline()->measures;
    const auto revision = (*document)->revision();
    const auto check_lyrics = [&] {
        const auto &master = read_test::master_of(**document);
        const auto *rest = master.find_cr({1, 1}, 0);
        ASSERT_NE(rest, nullptr);
        ASSERT_TRUE(rest->is_rest());
        ASSERT_EQ(rest->lyrics().size(), 1u);
        EXPECT_EQ(rest->lyrics().front()->plain_text().to_std_string(), "onset");
    };
    check_lyrics();
    for (const auto &attempt : {
             PhraseLane{{bars[1].id, {1, 4}}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{g4}}}}},
             PhraseLane{{bars[1].id, {0, 1}}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{c4}}}}},
             PhraseLane{{bars[1].id, {0, 1}}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {}}}},
         }) {
        const auto edit = (*document)->write_phrase({.lanes = {
                                                         {{bars[0].id, {0, 1}}, PitchDomain::Concert, {PhraseEvent{{1, 4}, {{c4}}}}},
                                                         attempt,
                                                     }});
        ASSERT_TRUE(edit) << edit.error().message;
        ASSERT_TRUE(edit->rejection);
        EXPECT_NE(edit->rejection->error.message.find("rest with authored attachments"), std::string::npos);
        EXPECT_EQ(edit->applied, 0u);
        EXPECT_EQ((*document)->revision(), revision);
        EXPECT_EQ(chord_at(read_test::master_of(**document), {0, 1}, 0), nullptr);
        check_lyrics();
    }
}

TEST(MusicWrite, BarEndHairpinKeepsTheAnchorVoice) {
    const auto document = open_fixture(clarinet_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto bar = (*document)->outline()->measures.at(1).id;
    const auto secondary = (*document)->read_music({.first_measure = 1, .voices = {1}});
    ASSERT_TRUE(secondary) << secondary.error().message;
    const auto start = secondary->measures.at(0).voices.at(0).events.at(0).id;
    const auto edit = (*document)->edit({{AddSpannerToBarEnd{"HairPin", start, {bar}, {{"voiceAssignment", Enum{"currentVoiceOnly"}}}}}});
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    const auto voice_two = (*document)->read_music({.first_measure = 1, .voices = {1}});
    ASSERT_TRUE(voice_two) << voice_two.error().message;
    ASSERT_EQ(voice_two->spanners.size(), 1u);
    EXPECT_EQ(voice_two->spanners[0].type, "HairPin");
    EXPECT_EQ(voice_two->spanners[0].start.voice, 1);
    const auto voice_one = (*document)->read_music({.first_measure = 1, .voices = {0}});
    ASSERT_TRUE(voice_one) << voice_one.error().message;
    EXPECT_TRUE(voice_one->spanners.empty());
}

TEST(MusicWrite, GraceDurationUsesAnExactNotatedFraction) {
    const auto document = open_fixture(clarinet_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto pickup = (*document)->read_music({});
    ASSERT_TRUE(pickup) << pickup.error().message;
    const auto principal = pickup->measures.at(0).voices.at(0).events.at(0).id;
    const auto revision = (*document)->revision();
    for (const auto &invalid : {GraceEntry{GraceKind::Acciaccatura, {1, 3}, {{74}}}, GraceEntry{GraceKind::Acciaccatura, {1, 12}, {{74}}},
                                GraceEntry{GraceKind::Grace16, {1, 4}, {{74}}}}) {
        read_test::expect_code((*document)->edit({{AddGraceChord{principal, invalid}}}), ErrorCode::INVALID_ARGUMENT,
                               "invalid grace duration");
        EXPECT_EQ((*document)->revision(), revision);
    }
    const auto edit = (*document)->edit({{AddGraceChord{principal, {GraceKind::Acciaccatura, {3, 256}, {{74}}}}}});
    ASSERT_TRUE(edit) << edit.error().message;
    ASSERT_FALSE(edit->rejection) << edit->rejection->error.message;
    const auto music = (*document)->read_music({});
    ASSERT_TRUE(music) << music.error().message;
    const auto &chord = std::get<ChordMusic>(music->measures.at(0).voices.at(0).events.at(0).body);
    ASSERT_EQ(chord.before.size(), 1u);
    EXPECT_EQ(chord.before[0].kind, GraceKind::Acciaccatura);
    expect_fraction(chord.before[0].written_duration, 3, 256);
}

TEST(MusicWrite, StaleRevisionTakesPrecedenceOverInvalidLane) {
    const auto document = open_fixture(piano_fixture);
    ASSERT_TRUE(document) << document.error().message;
    const auto revision = (*document)->revision();
    const auto bar = (*document)->outline()->measures.at(0).id;
    const Phrase phrase{.lanes = {{{bar, {0, 1}, -1, 0}, PitchDomain::Concert, {PhraseEvent{{1, 8}, {{c4}}}}}}};
    read_test::expect_code((*document)->write_phrase(phrase), ErrorCode::INVALID_ARGUMENT, "invalid staff at the current revision");
    read_test::expect_code((*document)->write_phrase(phrase, {.expected_revision = revision + 1}), ErrorCode::REVISION_CONFLICT,
                           "invalid staff at a stale revision");
    EXPECT_EQ((*document)->revision(), revision);
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
