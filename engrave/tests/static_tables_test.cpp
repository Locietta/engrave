// SPDX-License-Identifier: GPL-3.0-only
#include <stdexcept>

#include <gtest/gtest.h>

#include "engraving/automation/dynamicvalues.h"
#include "engraving/dom/barline.h"
#include "engraving/types/symnames.h"
#include "engraving/types/typesconv.h"
#include "global/types/translatablestring.h"

using namespace mu::engraving;
using muse::AsciiStringView;
using muse::String;
using muse::TranslatableString;

TEST(TranslatableString, FormatsIntegersStringsAndNestedArguments) {
    const TranslatableString verse("test", "Verse %1");
    const auto third_verse = verse.arg(3);
    EXPECT_EQ(third_verse.translated(), String(u"Verse 3"));
    EXPECT_EQ(verse.translated(), String(u"Verse %1"));
    EXPECT_EQ(TranslatableString::untranslatable("%1 section").arg(String(u"Brass")).translated(), String(u"Brass section"));

    const TranslatableString locator("test", "%2: %1", "rehearsal mark", 2);
    const auto location = locator.arg(third_verse, String(u"Rehearsal"));
    EXPECT_EQ(location.translated(), String(u"Rehearsal: Verse 3"));
    EXPECT_EQ(location.translated(4), String(u"Rehearsal: Verse 3"));
    EXPECT_TRUE(location.is_translatable());
    EXPECT_STREQ(location.disambiguation, "rehearsal mark");
    EXPECT_EQ(location.default_n, 2);
    EXPECT_EQ(locator.arg(String(u"Verse 3")).arg(String(u"Rehearsal")), location);
    EXPECT_NE(locator.arg(verse.arg(4), String(u"Rehearsal")), location);
}

TEST(TranslatableString, PreservesEmptyKeysAndLiteralArgumentText) {
    EXPECT_TRUE(TranslatableString().arg(3).is_empty());

    const TranslatableString label("test", "%1");
    const auto blank = label.arg(String());
    EXPECT_FALSE(blank.is_empty());
    EXPECT_TRUE(blank.translated().empty());

    const String literal(std::u16string_view(u"A\0B", 3));
    const auto formatted = label.arg(literal);
    EXPECT_EQ(formatted.translated(), literal);
    EXPECT_NE(formatted, TranslatableString("test", literal));
}

TEST(StaticTables, ElementNamesAndPluralMetadata) {
    EXPECT_TRUE(TConv::to_xml(ElementType::BAR_LINE) == "BarLine");
    EXPECT_EQ(TConv::from_xml("BarLine", ElementType::INVALID), ElementType::BAR_LINE);
    EXPECT_EQ(TConv::from_xml("barline", ElementType::NOTE, true), ElementType::NOTE);

    const auto &name = TConv::user_name(ElementType::BAR_LINE);
    EXPECT_EQ(name.str, String(u"barline(s)"));
    EXPECT_STREQ(name.context, "engraving");
    EXPECT_EQ(name.default_n, 1);
    EXPECT_EQ(name.disambiguation, nullptr);
    EXPECT_EQ(TConv::capitalized_user_name(ElementType::BAR_LINE).str, String(u"Barline(s)"));
    EXPECT_EQ(TConv::user_name(ElementType::ROOT_ITEM).context, nullptr);
    EXPECT_EQ(&name, &TConv::user_name(ElementType::BAR_LINE));
#ifdef NDEBUG
    const auto unknown = static_cast<ElementType>(255);
    EXPECT_TRUE(TConv::to_xml(unknown).empty());
    EXPECT_TRUE(TConv::user_name(unknown).is_empty());
#endif
}

TEST(StaticTables, NoteHeadsAndClefsKeepNumericAliases) {
    EXPECT_TRUE(TConv::to_xml(NoteHeadGroup::HEAD_CROSS) == "cross");
    EXPECT_EQ(TConv::from_xml("cross", NoteHeadGroup::HEAD_NORMAL), NoteHeadGroup::HEAD_CROSS);
    EXPECT_EQ(TConv::from_xml("1", NoteHeadGroup::HEAD_NORMAL), NoteHeadGroup::HEAD_CROSS);
    EXPECT_EQ(TConv::from_xml("CROSS", NoteHeadGroup::HEAD_PLUS), NoteHeadGroup::HEAD_PLUS);
    const auto &notehead = TConv::user_name(NoteHeadGroup::HEAD_SWISS_RUDIMENTS_DOUBLE);
    EXPECT_EQ(notehead.str, String(u"Swiss Rudiments Doubl\u00e9"));
    EXPECT_STREQ(notehead.context, "engraving/noteheadgroup");
    EXPECT_EQ(notehead.default_n, -1);

    EXPECT_TRUE(TConv::to_xml(ClefType::G15_MB) == "G15mb");
    EXPECT_EQ(TConv::from_xml("G15mb", ClefType::INVALID), ClefType::G15_MB);
    EXPECT_EQ(TConv::from_xml("1", ClefType::INVALID), ClefType::G15_MB);
    EXPECT_EQ(TConv::from_xml("unknown", ClefType::F), ClefType::F);
    EXPECT_EQ(TConv::translated_user_name(ClefType::G), String(u"Treble clef"));
    EXPECT_STREQ(TConv::user_name(ClefType::G).context, "engraving/cleftype");
}

TEST(StaticTables, TextStyleNamesKeepLegacyAliases) {
    EXPECT_TRUE(TConv::to_xml(TextStyleType::LYRICIST) == "poet");
    EXPECT_EQ(TConv::from_xml("poet", TextStyleType::DEFAULT), TextStyleType::LYRICIST);
    EXPECT_EQ(TConv::from_xml("Lyricist", TextStyleType::DEFAULT), TextStyleType::LYRICIST);
    EXPECT_EQ(TConv::from_xml("Technique", TextStyleType::DEFAULT), TextStyleType::EXPRESSION);
    EXPECT_EQ(TConv::from_xml("12", TextStyleType::DEFAULT), TextStyleType::DYNAMICS);
    const auto &name = TConv::user_name(TextStyleType::LYRICIST);
    EXPECT_EQ(name.str, String(u"Lyricist"));
    EXPECT_STREQ(name.context, "engraving");
    EXPECT_EQ(name.default_n, -1);
#ifdef NDEBUG
    EXPECT_EQ(TConv::from_xml("unknown", TextStyleType::TITLE), TextStyleType::TITLE);
#endif
}

TEST(StaticTables, TremoloLabelsAndLegacyTextPlacement) {
    EXPECT_TRUE(TConv::to_xml(TremoloType::R16) == "r16");
    EXPECT_EQ(TConv::from_xml("r16", TremoloType::INVALID_TREMOLO), TremoloType::R16);
    const auto &label = TConv::user_name(TremoloType::R16);
    EXPECT_EQ(label.str, String(u"16th through stem"));
    EXPECT_STREQ(label.context, "engraving/tremolotype");
    EXPECT_EQ(label.default_n, -1);
    EXPECT_EQ(&label, &TConv::user_name(TremoloType::R16));
    EXPECT_TRUE(TConv::user_name(TremoloType::INVALID_TREMOLO).is_empty());

    EXPECT_TRUE(TConv::to_xml(TextPlace::BELOW) == "below");
    EXPECT_EQ(TConv::from_xml("2", TextPlace::AUTO), TextPlace::BELOW);
#ifdef NDEBUG
    EXPECT_EQ(TConv::from_xml("unknown", TextPlace::ABOVE), TextPlace::ABOVE);
#endif
}

TEST(StaticTables, EmbellishmentOrdinalsAndNoteSequences) {
    constexpr auto double_grace = static_cast<EmbellishmentType>(9);
    EXPECT_EQ(TConv::to_xml(double_grace), String(u"9"));
    EXPECT_EQ(TConv::from_xml("9", static_cast<EmbellishmentType>(0)), double_grace);
    EXPECT_EQ(TConv::from_xml("unknown", double_grace), double_grace);
    auto notes = TConv::embellishment_notes(double_grace);
    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0], String(u"D"));
    EXPECT_EQ(notes[1], String(u"LG"));
    const auto &name = TConv::user_name(double_grace);
    EXPECT_EQ(name.str, String(u"Double grace"));
    EXPECT_STREQ(name.context, "engraving/bagpipeembellishment");
    EXPECT_EQ(name.default_n, -1);
    EXPECT_EQ(TConv::embellishments_count(), 214u);
    EXPECT_THROW(TConv::embellishment_notes(static_cast<EmbellishmentType>(255)), std::out_of_range);
}

TEST(StaticTables, SymbolLookupsAndFallbacks) {
    EXPECT_TRUE(SymNames::name_for_sym_id(SymId::ACCIDENTAL_SHARP) == "accidentalSharp");
    EXPECT_EQ(SymNames::sym_id_by_name(AsciiStringView("accidentalSharp")), SymId::ACCIDENTAL_SHARP);
    EXPECT_EQ(SymNames::sym_id_by_name(String(u"accidentalSharp")), SymId::ACCIDENTAL_SHARP);
    EXPECT_EQ(SymNames::sym_id_by_name(String(u"accidentalSh\u00e4rp"), SymId::SPACE), SymId::SPACE);
    EXPECT_EQ(SymNames::sym_id_by_name(AsciiStringView("AccidentalSharp"), SymId::SPACE), SymId::SPACE);
    EXPECT_EQ(SymNames::sym_id_by_name(AsciiStringView("accidentalSharpX", 15)), SymId::ACCIDENTAL_SHARP);
    EXPECT_EQ(SymNames::sym_id_by_old_name("outside whole rest"), SymId::REST_WHOLE_LEGER_LINE);
    EXPECT_EQ(SymNames::sym_id_by_old_name("Outside whole rest"), SymId::NO_SYM);
    EXPECT_EQ(SymNames::sym_id_by_old_name("clefs.tab2"), SymId::SIX_STRING_TAB_CLEF_SERIF);

    constexpr auto bad_id = static_cast<SymId>(-1);
    EXPECT_THROW(SymNames::name_for_sym_id(bad_id), std::out_of_range);
    EXPECT_THROW(SymNames::user_name_for_sym_id(bad_id), std::out_of_range);
    EXPECT_TRUE(SymNames::name_for_sym_id(SymId::LAST_SYM).empty());
    EXPECT_TRUE(SymNames::user_name_for_sym_id(SymId::LAST_SYM).is_empty());
}

TEST(StaticTables, SymbolLabelsAndBarlines) {
    const auto &label = SymNames::user_name_for_sym_id(SymId::BARLINE_SINGLE);
    EXPECT_EQ(label.str, String(u"Single barline"));
    EXPECT_STREQ(label.context, "engraving/sym");
    EXPECT_EQ(&label, &SymNames::user_name_for_sym_id(SymId::BARLINE_SINGLE));
    EXPECT_EQ(BarLine::translated_user_type_name(BarLineType::NORMAL), String(u"Single barline"));

    const auto &tab = SymNames::user_name_for_sym_id(SymId::FOUR_STRING_TAB_CLEF);
    EXPECT_EQ(tab.str, String(u"4-string tab clef"));
    EXPECT_EQ(tab.context, nullptr);
    EXPECT_EQ(SymNames::translated_user_name_for_sym_id(SymId::CSYM_ACCIDENTAL_SHARP), String(u"Sharp"));
    EXPECT_EQ(SymNames::sym_id_by_user_name(String(u"Sharp")), SymId::ACCIDENTAL_SHARP);
    EXPECT_EQ(SymNames::sym_id_by_user_name(String(u"unknown symbol label")), SymId::NO_SYM);
}

TEST(StaticTables, NormalizedDynamics) {
    EXPECT_EQ(ordinary_dynamic_value(DynamicType::N).value().raw(), 0.000);
    EXPECT_EQ(ordinary_dynamic_value(DynamicType::PPPPPP).value().raw(), 0.175);
    EXPECT_EQ(ordinary_dynamic_value(DynamicType::MF).value().raw(), 0.525);
    EXPECT_EQ(ordinary_dynamic_value(DynamicType::FFFFFF).value().raw(), 0.825);
    EXPECT_FALSE(ordinary_dynamic_value(DynamicType::OTHER));
    EXPECT_FALSE(ordinary_dynamic_value(DynamicType::FP));

    EXPECT_EQ(single_note_dynamic_value(DynamicType::RFZ).value().raw(), 0.575);
    EXPECT_EQ(single_note_dynamic_value(DynamicType::SFF).value().raw(), 0.625);
    EXPECT_EQ(single_note_dynamic_value(DynamicType::SFFFZ).value().raw(), 0.675);
    EXPECT_FALSE(single_note_dynamic_value(DynamicType::FZ));

    const auto fp = compound_dynamic_value(DynamicType::FP).value();
    EXPECT_EQ(fp.first.raw(), 0.575);
    EXPECT_EQ(fp.second.raw(), 0.425);
    const auto pf = compound_dynamic_value(DynamicType::PF).value();
    EXPECT_EQ(pf.first.raw(), 0.425);
    EXPECT_EQ(pf.second.raw(), 0.575);
    const auto sfpp = compound_dynamic_value(DynamicType::SFPP).value();
    EXPECT_EQ(sfpp.first.raw(), 0.575);
    EXPECT_EQ(sfpp.second.raw(), 0.375);
    EXPECT_FALSE(compound_dynamic_value(DynamicType::SFZ));
}
