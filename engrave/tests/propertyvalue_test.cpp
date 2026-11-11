// SPDX-License-Identifier: GPL-3.0-only
#include "engraving/types/propertyvalue.h"

#include <cstdio>

#include <gtest/gtest.h>

using namespace mu::engraving;

TEST(PropertyValue, ConversionsAndEquality) {
    EXPECT_FALSE(PropertyValue{}.is_valid());
    EXPECT_EQ(PropertyValue{}.value<int>(), 0);
    EXPECT_EQ(PropertyValue(BeamMode::AUTO).value<BeamMode>(), BeamMode::AUTO);
    EXPECT_TRUE(PropertyValue(BeamMode::AUTO).is_enum());
    EXPECT_FALSE(PropertyValue(1).is_enum());
    EXPECT_EQ(PropertyValue(BeamMode::AUTO).to_int(), static_cast<int>(BeamMode::AUTO));
    EXPECT_EQ(PropertyValue(3).value<BeamMode>(), static_cast<BeamMode>(3));
    EXPECT_EQ(PropertyValue(true).to_int(), 1);
    EXPECT_TRUE(PropertyValue(2).to_bool());
    EXPECT_EQ(PropertyValue(size_t(7)).to_int(), 7);
    EXPECT_EQ(PropertyValue(2.5).value<Spatium>().val(), 2.5);
    EXPECT_EQ(PropertyValue(Spatium(2.5)).to_real(), 2.5);
    EXPECT_EQ(PropertyValue(Fraction(1, 2)).value<String>(), Fraction(1, 2).to_string());
    EXPECT_EQ(PropertyValue(true), PropertyValue(2));
    EXPECT_EQ(PropertyValue(BeamMode::AUTO), PropertyValue(static_cast<int>(BeamMode::AUTO)));
    EXPECT_EQ(PropertyValue(Spatium(2.5)), PropertyValue(2.5));
    EXPECT_NE(PropertyValue(Fraction(1, 2)), PropertyValue(Fraction(2, 4)));
#ifdef NDEBUG
    EXPECT_EQ(PropertyValue(2.5).to_int(), 0);
    EXPECT_EQ(PropertyValue(2).value<String>(), String());
    EXPECT_EQ(PropertyValue(BeamMode::AUTO).value<PlacementV>(), PlacementV{});
#endif
}

TEST(PropertyValue, IndependentCopiesAndAssignment) {
    PropertyValue a(std::vector<int>{1, 2, 3});
    auto b = a;
    auto values = b.value<std::vector<int>>();
    values[0] = 9;
    b = values;
    EXPECT_EQ(a.value<std::vector<int>>()[0], 1);
    EXPECT_EQ(b.value<std::vector<int>>()[0], 9);
    PainterPath path;
    path.move_to(1, 2);
    path.line_to(3, 4);
    PropertyValue original(path);
    auto copy = original;
    EXPECT_EQ(original, copy);
    auto changed = copy.value<PainterPath>();
    changed.line_to(5, 6);
    copy = changed;
    EXPECT_NE(original, copy);
    original = original;
    EXPECT_EQ(original.value<PainterPath>(), path);
    auto moved = std::move(copy);
    EXPECT_TRUE(moved.is_valid());
    copy = original;
    EXPECT_EQ(copy, original);
    std::printf("sizeof(PropertyValue)=%zu\n", sizeof(PropertyValue));
}

namespace {
template <typename T>
void check_enum(PType type) {
    const auto e = static_cast<T>(1);
    PropertyValue v(e);
    EXPECT_EQ(v.type(), type);
    EXPECT_TRUE(v.is_enum());
    EXPECT_EQ(v.value<T>(), e);
    EXPECT_EQ(v.to_int(), 1);
}
} // namespace

TEST(PropertyValue, EveryEnumRetainsItsTag) {
    check_enum<SymId>(PType::SYMID);
    check_enum<OrnamentStyle>(PType::ORNAMENT_STYLE);
    check_enum<GlissandoStyle>(PType::GLISS_STYLE);
    check_enum<GlissandoType>(PType::GLISS_TYPE);
    check_enum<AlignH>(PType::ALIGN_H);
    check_enum<PlacementV>(PType::PLACEMENT_V);
    check_enum<PlacementH>(PType::PLACEMENT_H);
    check_enum<TextPlace>(PType::TEXT_PLACE);
    check_enum<DirectionV>(PType::DIRECTION_V);
    check_enum<DirectionH>(PType::DIRECTION_H);
    check_enum<Orientation>(PType::ORIENTATION);
    check_enum<SharedLabelOrientation>(PType::SHARED_LABEL_ORIENTATION);
    check_enum<BeamMode>(PType::BEAM_MODE);
    check_enum<AccidentalRole>(PType::ACCIDENTAL_ROLE);
    check_enum<TiePlacement>(PType::TIE_PLACEMENT);
    check_enum<TieDotsPlacement>(PType::TIE_DOTS_PLACEMENT);
    check_enum<TimeSigPlacement>(PType::TIMESIG_PLACEMENT);
    check_enum<TimeSigStyle>(PType::TIMESIG_STYLE);
    check_enum<TimeSigVSMargin>(PType::TIMESIG_MARGIN);
    check_enum<NoteSpellingType>(PType::NOTE_SPELLING_TYPE);
    check_enum<ChordStylePreset>(PType::CHORD_PRESET_TYPE);
    check_enum<ParenthesesMode>(PType::PARENTHESES_MODE);
    check_enum<RepeatPlayCountPreset>(PType::PLAY_COUNT_PRESET);
    check_enum<ChangeMethod>(PType::CHANGE_METHOD);
    check_enum<LayoutBreakType>(PType::LAYOUTBREAK_TYPE);
    check_enum<VeloType>(PType::VELO_TYPE);
    check_enum<BarLineType>(PType::BARLINE_TYPE);
    check_enum<NoteHeadType>(PType::NOTEHEAD_TYPE);
    check_enum<NoteHeadScheme>(PType::NOTEHEAD_SCHEME);
    check_enum<NoteHeadGroup>(PType::NOTEHEAD_GROUP);
    check_enum<ClefType>(PType::CLEF_TYPE);
    check_enum<ClefToBarlinePosition>(PType::CLEF_TO_BARLINE_POS);
    check_enum<DynamicType>(PType::DYNAMIC_TYPE);
    check_enum<DynamicSpeed>(PType::DYNAMIC_SPEED);
    check_enum<LineType>(PType::LINE_TYPE);
    check_enum<HookType>(PType::HOOK_TYPE);
    check_enum<KeyMode>(PType::KEY_MODE);
    check_enum<TextStyleType>(PType::TEXT_STYLE);
    check_enum<PlayingTechniqueType>(PType::PLAYTECH_TYPE);
    check_enum<GradualTempoChangeType>(PType::TEMPOCHANGE_TYPE);
    check_enum<SlurStyleType>(PType::SLUR_STYLE_TYPE);
    check_enum<NoteLineEndPlacement>(PType::NOTELINE_PLACEMENT_TYPE);
    check_enum<ActionIconType>(PType::ACTION_TYPE);
    check_enum<OrnamentShowAccidental>(PType::ORNAMENT_SHOW_ACCIDENTAL);
    check_enum<LyricsDashSystemStart>(PType::LYRICS_DASH_SYSTEM_START_TYPE);
    check_enum<PartialSpannerDirection>(PType::PARTIAL_SPANNER_DIRECTION);
    check_enum<LHTappingSymbol>(PType::LH_TAPPING_SYMBOL);
    check_enum<RHTappingSymbol>(PType::RH_TAPPING_SYMBOL);
    check_enum<VibratoType>(PType::VIBRATO_LINE_TYPE);
    check_enum<VoiceAssignment>(PType::VOICE_ASSIGNMENT);
    check_enum<AutoOnOff>(PType::AUTO_ON_OFF);
    check_enum<AutoCustomHide>(PType::AUTO_CUSTOM_HIDE);
    check_enum<MarkerType>(PType::MARKER_TYPE);
    check_enum<MeasureNumberPlacement>(PType::MEASURE_NUMBER_PLACEMENT);
    check_enum<CapoParams::TransposeMode>(PType::CAPO_TRANSPOSE_MODE);
    check_enum<InstrumentNamesAlign>(PType::INSTRUMENT_NAMES_ALIGN);
    check_enum<InstrumentNamesFormat>(PType::INSTRUMENT_NAMES_FORMAT);
    check_enum<InstrumentLabelVisibility>(PType::INSTRUMENT_LABEL_VISIBILITY);
}

TEST(StringValue, IndependentStorageAndMoves) {
    String original(u"a long string beyond the short buffer");
    char16_t &first = original[0];
    String copy = original;
    first = u'A';
    EXPECT_EQ(copy, u"a long string beyond the short buffer");
    copy.append(copy);
    EXPECT_EQ(original, u"A long string beyond the short buffer");
    EXPECT_EQ(copy.size(), 2 * original.size());
    String moved = std::move(copy);
    copy = u"reused";
    EXPECT_EQ(copy, u"reused");
    EXPECT_EQ(moved.size(), 2 * original.size());
    String empty;
    empty.append(u'x');
    EXPECT_EQ(empty, u"x");
    std::printf("sizeof(String)=%zu sizeof(PropertyValue)=%zu\n", sizeof(String), sizeof(PropertyValue));
}
