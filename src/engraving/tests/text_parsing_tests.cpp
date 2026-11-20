// SPDX-License-Identifier: GPL-3.0-only

#include <memory>

#include <gtest/gtest.h>

#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/tempotext.h"
#include "importexport/musicxml/internal/import/importmusicxmlpass1.h"
#include "importexport/musicxml/internal/import/importmusicxmltext.h"
#include "importexport/musicxml/internal/shared/musicxmlsupport.h"
#include "utils/scorecomp.h"
#include "utils/scorerw.h"

using namespace mu::engraving;
using muse::ByteArray;

namespace mx = mu::iex::musicxml;

TEST(TextParsing, DecimalXmlEntities) {
    EXPECT_EQ(String::decode_xml_entities(u"é &#65;&#00066; &#65; &#9837;"), u"é AB A ♭");
    EXPECT_EQ(String::decode_xml_entities(u"&#; &#x41; &amp; &#-1; &#65 &#１２;"), u"&#; &#x41; &amp; &#-1; &#65 &#１２;");
    EXPECT_EQ(String::decode_xml_entities(u"&#2147483648;&#999999999999999999999;&#67;"), u"&#2147483648;&#999999999999999999999;C");
    EXPECT_EQ(String::decode_xml_entities(u"&#38;#68;"), u"&#68;");
    const auto zero = String::decode_xml_entities(u"a&#0;b");
    EXPECT_EQ(zero.view(), std::u16string_view(u"a\0b", 3));
}

TEST(TextParsing, TempoDurationUsesUtf16AndTablePriority) {
    int length = -1;
    TDuration duration;
    EXPECT_EQ(TempoText::find_tempo_duration(u"\uECA5 Très 🎵 \uECA5 \t\uECB7", length, duration), 10);
    EXPECT_EQ(length, 4);
    EXPECT_EQ(duration.type(), DurationType::V_QUARTER);
    EXPECT_EQ(duration.dots(), 1);

    EXPECT_EQ(TempoText::find_tempo_duration(u"♫ sans tempo", length, duration), -1);
    EXPECT_EQ(length, 0);
    EXPECT_FALSE(duration.is_valid());
}

TEST(TextParsing, TempoRepeatedEquations) {
    const std::unique_ptr<MasterScore> score(compat::ScoreAccess::create_master_score(ScoreRW::services()));
    const std::unique_ptr<TempoText> tempo(Factory::create_tempo_text(score->dummy()));
    tempo->set_xml_text(u"\uECA5 = \uECA3, \uECA5 = 90, \uECA5 = 75");
    tempo->update_tempo();
    EXPECT_FALSE(tempo->is_relative());
    EXPECT_DOUBLE_EQ(tempo->tempo().val, 1.5);
    EXPECT_DOUBLE_EQ(tempo->tempo_bpm(), 90.0);

    tempo->set_xml_text(u"\uECA5 = \uECA7, \uECA5 = \uECA3");
    tempo->update_tempo();
    EXPECT_TRUE(tempo->is_relative());
    EXPECT_DOUBLE_EQ(tempo->relative_value(), 2.0);
}

TEST(TextParsing, DynamicSymbolsAndWordBoundaries) {
    const std::unique_ptr<MasterScore> score(compat::ScoreAccess::create_master_score(ScoreRW::services()));
    const std::unique_ptr<Dynamic> dynamic(Factory::create_dynamic(score->dummy()));

    const String forte_piano = u"Léger <sym>dynamicForte</sym><sym>dynamicPiano</sym> pp";
    dynamic->set_dynamic_type(forte_piano);
    EXPECT_EQ(dynamic->dynamic_type(), DynamicType::FP);
    EXPECT_EQ(dynamic->xml_text(), forte_piano);

    dynamic->set_dynamic_type(u"_ff mf2 p> en mp");
    EXPECT_EQ(dynamic->dynamic_type(), DynamicType::MP);
    EXPECT_EQ(dynamic->xml_text(), u"_ff mf2 p> en <sym>dynamicMezzo</sym><sym>dynamicPiano</sym>");

    dynamic->set_dynamic_type(u"<sym>unknown</sym><sym>dynamicPiano</sym> zzz ff");
    EXPECT_EQ(dynamic->dynamic_type(), DynamicType::FF);
    dynamic->set_dynamic_type(u"<sym>unfinished\nf");
    EXPECT_EQ(dynamic->dynamic_type(), DynamicType::F);
}

TEST(TextParsing, MusicXmlRepeatDirections) {
    EXPECT_EQ(mx::parse_repeat_direction(u"d.c.al fine").type, u"daCapoAlFine");
    EXPECT_EQ(mx::parse_repeat_direction(u"da capo al codaiii").type, u"daCapoAlCoda");
    EXPECT_TRUE(mx::parse_repeat_direction(u"da capoal fine").type.empty());
    EXPECT_EQ(mx::parse_repeat_direction(u"del segno. ii (note) al fine").type, u"dalSegnoAlFine");
    EXPECT_EQ(mx::parse_repeat_direction(u"segno segno ii").type, u"segno");
    EXPECT_EQ(mx::parse_repeat_direction(u"to coda coda").type, u"toCoda");
    EXPECT_TRUE(mx::parse_repeat_direction(u"dal segno.").type.empty());
    EXPECT_TRUE(mx::parse_repeat_direction(u"coda iiii").type.empty());

    const auto strict = mx::parse_repeat_direction(u"d.s.s. ii(note) al coda iii", false);
    EXPECT_EQ(strict.type, u"dalSegnoAlCoda");
    EXPECT_EQ(strict.segno_name, u"s.s.");
    EXPECT_EQ(strict.segno_number, u"ii");
    EXPECT_EQ(strict.coda_number, u"iii");
    EXPECT_EQ(mx::parse_repeat_direction(u"d.s. ii (note) al coda iii").type, u"dalSegnoAlCoda");
    EXPECT_TRUE(mx::parse_repeat_direction(u"d.s. ii (note) al coda iii", false).type.empty());
}

TEST(TextParsing, MusicXmlFingeringLines) {
    EXPECT_TRUE(mx::matches_fingering(u"\t– 1-3\n\u00A0p\nm–a "));
    EXPECT_TRUE(mx::matches_fingering(u"4\n\n5"));
    EXPECT_FALSE(mx::matches_fingering(u"1 2"));
    EXPECT_FALSE(mx::matches_fingering(u"1 \n2"));
    EXPECT_FALSE(mx::matches_fingering(u"1\n  2"));
    EXPECT_FALSE(mx::matches_fingering(u"6"));
    EXPECT_FALSE(mx::matches_fingering(u"\t\u00A0"));
}

TEST(TextParsing, MusicXmlTranspositionCleanup) {
    const auto [track, trait] = mx::process_instr_name(u"Bari Sax in\u00A0E♭ (1, 2). / doubled");
    EXPECT_EQ(track, u"Baritone Saxophone ");
    EXPECT_EQ(trait, u"E♭");
    const auto [clarinet, no_trait] = mx::process_instr_name(u"Clarinet (3) in G");
    EXPECT_EQ(clarinet, u"Clarinet  G");
    EXPECT_TRUE(no_trait.empty());
}

TEST(TextParsing, MusicXmlFormattingTags) {
    EXPECT_EQ(mx::strip_formatting_tags(u"é<i>one</i><b>two</b><x<y>three"), u"éonetwothree");
    EXPECT_EQ(mx::strip_formatting_tags(u"a<unfinished\nb<i>c</i>\r<d"), u"a<unfinished\nbc\r<d");
    EXPECT_EQ(mx::strip_formatting_tags(u"<>\n>"), u"\n>");
}

TEST(TextParsing, MusicXmlCreditsAndSubtitles) {
    EXPECT_TRUE(mx::is_likely_credit_text(u"Music, &amp; Words by Ada Byrd", false));
    EXPECT_FALSE(mx::is_likely_credit_text(u"MUSIC BY Ada Byrd", false));
    EXPECT_TRUE(mx::is_likely_credit_text(u"MUSIC BY Ada Byrd", true));
    EXPECT_FALSE(mx::is_likely_credit_text(u"by ", true));
    EXPECT_TRUE(mx::is_likely_subtitle_text(u"Title (From the opera)", false));
    EXPECT_FALSE(mx::is_likely_subtitle_text(u"Title (FROM the opera)", false));
    EXPECT_TRUE(mx::is_likely_subtitle_text(u"Title (FROM the opera)", true));
    EXPECT_TRUE(mx::is_likely_subtitle_text(u"Sonata Op. 19", false));
    EXPECT_FALSE(mx::is_likely_subtitle_text(u"Sonata Op.  19", false));
}

TEST(TextParsing, MusicXmlTempoSubstitutionUsesCurrentFont) {
    for (const bool met_first : {false, true}) {
        const auto check_met_times = [] {
            String words = u"Ó. = c. 30";
            const auto tempo = mx::substitute_tempo_note(words, false, u"MetTimes Plain");
            ASSERT_TRUE(tempo);
            EXPECT_DOUBLE_EQ(*tempo, 90.0);
            EXPECT_EQ(words, u"<sym>metNoteHalfUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = c. 30");
        };
        if (met_first) {
            check_met_times();
        }
        String words = u"q = 90, e = 160";
        const auto tempo = mx::substitute_tempo_note(words, true, u"");
        ASSERT_TRUE(tempo);
        EXPECT_DOUBLE_EQ(*tempo, 80.0);
        EXPECT_EQ(words, u"q = 90, <sym>metNote8thUp</sym> = 160");
        if (!met_first) {
            check_met_times();
        }
    }
    String multiline = u"q = 60\ne = 120";
    const auto tempo = mx::substitute_tempo_note(multiline, true, u"");
    ASSERT_TRUE(tempo);
    EXPECT_DOUBLE_EQ(*tempo, 60.0);
    EXPECT_EQ(multiline, u"<sym>metNoteQuarterUp</sym> = 60\n<sym>metNoteQuarterUp</sym> = 120");

    String words = u"q = 120";
    EXPECT_FALSE(mx::substitute_tempo_note(words, false, u"MetTimes Plain"));
    EXPECT_EQ(words, u"q = 120");
}

TEST(TextParsing, ScoreComparisonCanonicalizesDefinedIdentities) {
    const ByteArray xml("<linkedTo>A+_z/</linkedTo><eid>Q_1</eid><eid>A+_z/</eid><sharedPart>Q_1</sharedPart>"
                        "<endElement>X_Y</endElement><eid>X_Y</NoteEID><eid>A_Z_Z</eid><eid>_Z</eid>"
                        "<eid attr='a'>Q_1</eid><id>Q_1</id>");
    const ByteArray expected("<linkedTo>id0</linkedTo><eid>id1</eid><eid>id0</eid><sharedPart>id1</sharedPart>"
                             "<endElement>X_Y</endElement><eid>X_Y</NoteEID><eid>A_Z_Z</eid><eid>_Z</eid>"
                             "<eid attr='a'>Q_1</eid><id>Q_1</id>");
    EXPECT_EQ(ScoreComp::canonical_ids(xml), expected);
}
