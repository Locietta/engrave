/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"

#include "engraving/dom/guitarbend.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/tremolobar.h"

namespace mu::engraving {
struct TConv {
    TConv() = default;

    static String to_xml(const std::vector<int> &v);
    static std::vector<int> from_xml(const String &tag, const std::vector<int> &def);

    static String to_xml(const std::vector<string_idx_t> &v);
    static std::vector<string_idx_t> from_xml(const String &tag, const std::vector<string_idx_t> &def);

    static const TranslatableString &user_name(ElementType v);
    static const TranslatableString &capitalized_user_name(ElementType v);

    static AsciiStringView to_xml(ElementType v);
    static ElementType from_xml(const AsciiStringView &tag, ElementType def, bool silent = false);

    static String to_xml(AlignH v);
    static String to_xml(AlignV v);
    static String to_xml(Align v);
    static Align from_xml(const String &str, Align def);
    static AlignH from_xml(const AsciiStringView &str, AlignH def);
    static AlignV from_xml(const AsciiStringView &str, AlignV def);

    static String to_xml(OrnamentInterval interval);
    static OrnamentInterval from_xml(const String &str, OrnamentInterval def);
    static IntervalStep from_xml(const AsciiStringView &tag, IntervalStep def);
    static IntervalType from_xml(const AsciiStringView &tag, IntervalType def);

    static String translated_user_name(SymId v);
    static AsciiStringView to_xml(SymId v);
    static SymId from_xml(const AsciiStringView &tag, SymId def);

    static String translated_user_name(Orientation v);
    static AsciiStringView to_xml(Orientation v);
    static Orientation from_xml(const AsciiStringView &tag, Orientation def);

    static String translated_user_name(SharedLabelOrientation v);
    static AsciiStringView to_xml(SharedLabelOrientation v);
    static SharedLabelOrientation from_xml(const AsciiStringView &tag, SharedLabelOrientation def);

    static String translated_user_name(NoteHeadType v);
    static AsciiStringView to_xml(NoteHeadType v);
    static NoteHeadType from_xml(const AsciiStringView &tag, NoteHeadType def);
    static String translated_user_name(NoteHeadScheme v);
    static AsciiStringView to_xml(NoteHeadScheme v);
    static NoteHeadScheme from_xml(const AsciiStringView &tag, NoteHeadScheme def);
    static String translated_user_name(InstrumentLabelVisibility v);
    static AsciiStringView to_xml(InstrumentLabelVisibility v);
    static InstrumentLabelVisibility from_xml(const AsciiStringView &tag, InstrumentLabelVisibility def);
    static const TranslatableString &user_name(NoteHeadGroup v);
    static String translated_user_name(NoteHeadGroup v);
    static AsciiStringView to_xml(NoteHeadGroup v);
    static NoteHeadGroup from_xml(const AsciiStringView &tag, NoteHeadGroup def);

    static const TranslatableString &user_name(ClefType v);
    static String translated_user_name(ClefType v);
    static AsciiStringView to_xml(ClefType v);
    static ClefType from_xml(const AsciiStringView &tag, ClefType def);

    static SymId sym_id(DynamicType v);
    static DynamicType dynamic_type(SymId v);
    static DynamicType dynamic_type(const AsciiStringView &string);
    static bool dynamic_valid(const AsciiStringView &tag);
    static const TranslatableString &user_name(DynamicType v);
    static String translated_user_name(DynamicType v);
    static AsciiStringView to_xml(DynamicType v);
    static DynamicType from_xml(const AsciiStringView &tag, DynamicType def);
    static String translated_user_name(DynamicSpeed v);
    static AsciiStringView to_xml(DynamicSpeed v);
    static DynamicSpeed from_xml(const AsciiStringView &tag, DynamicSpeed def);

    static String translated_user_name(HookType v);
    static String to_xml(HookType v);
    static HookType from_xml(const AsciiStringView &tag, HookType def);

    static AsciiStringView to_xml(LineType v);
    static LineType from_xml(const AsciiStringView &tag, LineType def);

    static String translated_user_name(KeyMode v);
    static AsciiStringView to_xml(KeyMode v);
    static KeyMode from_xml(const AsciiStringView &tag, KeyMode def);

    static const TranslatableString &user_name(TextStyleType v);
    static String translated_user_name(TextStyleType v);
    static AsciiStringView to_xml(TextStyleType v);
    static TextStyleType from_xml(const AsciiStringView &tag, TextStyleType def);

    static AsciiStringView to_xml(ChangeMethod v);
    static ChangeMethod from_xml(const AsciiStringView &tag, ChangeMethod def);
    static std::map<int /*positionTick*/, int> easing_value_curve(const int ticks_duration, const int steps_count, const int amplitude,
                                                                  const ChangeMethod method);
    static std::map<int /*positionTick*/, double> easing_value_curve(const int ticks_duration, const int steps_count,
                                                                     const double amplitude, const ChangeMethod method);

    static String to_xml(const PitchValue &v);

    static const char *user_name(AccidentalVal accidental, bool full);
    static String to_xml(AccidentalRole v);
    static AccidentalRole from_xml(const AsciiStringView &tag, AccidentalRole def);

    static String to_xml(BeatsPerSecond v, int precision);
    static BeatsPerSecond from_xml(const AsciiStringView &tag, BeatsPerSecond def);

    static String translated_user_name(DurationType v);
    static AsciiStringView to_xml(DurationType v);
    static DurationType from_xml(const AsciiStringView &tag, DurationType def);

    static const TranslatableString &user_name(PlayingTechniqueType v);
    static AsciiStringView to_xml(PlayingTechniqueType v);
    static PlayingTechniqueType from_xml(const AsciiStringView &tag, PlayingTechniqueType def);

    static const TranslatableString &user_name(GradualTempoChangeType v);
    static AsciiStringView to_xml(GradualTempoChangeType v);
    static GradualTempoChangeType from_xml(const AsciiStringView &tag, GradualTempoChangeType def);

    static AsciiStringView to_xml(OrnamentStyle v);
    static OrnamentStyle from_xml(const AsciiStringView &str, OrnamentStyle def);

    static AsciiStringView to_xml(PlacementV v);
    static PlacementV from_xml(const AsciiStringView &str, PlacementV def);
    static AsciiStringView to_xml(PlacementH v);
    static PlacementH from_xml(const AsciiStringView &str, PlacementH def);

    static AsciiStringView to_xml(TextPlace v);
    static TextPlace from_xml(const AsciiStringView &str, TextPlace def);

    static String translated_user_name(DirectionV v);
    static AsciiStringView to_xml(DirectionV v);
    static DirectionV from_xml(const AsciiStringView &str, DirectionV def);
    static String translated_user_name(DirectionH v);
    static AsciiStringView to_xml(DirectionH v);
    static DirectionH from_xml(const AsciiStringView &str, DirectionH def);

    static const TranslatableString &user_name(LayoutBreakType v);
    static AsciiStringView to_xml(LayoutBreakType v);
    static LayoutBreakType from_xml(const AsciiStringView &str, LayoutBreakType def);

    static AsciiStringView to_xml(VeloType v);
    static VeloType from_xml(const AsciiStringView &str, VeloType def);

    static AsciiStringView to_xml(BeamMode v);
    static BeamMode from_xml(const AsciiStringView &str, BeamMode def);

    static AsciiStringView to_xml(GlissandoStyle v);
    static GlissandoStyle from_xml(const AsciiStringView &str, GlissandoStyle def);

    static AsciiStringView to_xml(BarLineType v);
    static BarLineType from_xml(const AsciiStringView &str, BarLineType def);

    static const TranslatableString &user_name(TremoloType v);
    static AsciiStringView to_xml(TremoloType v);
    static TremoloType from_xml(const AsciiStringView &str, TremoloType def);

    static AsciiStringView to_xml(TremoloBarType v);
    static TremoloBarType from_xml(const AsciiStringView &str, TremoloBarType def);

    static const TranslatableString &user_name(BracketType v);
    static String translated_user_name(BracketType v);
    static AsciiStringView to_xml(BracketType v);
    static BracketType from_xml(const AsciiStringView &str, BracketType def);

    static const TranslatableString &user_name(ArpeggioType v);
    static AsciiStringView to_xml(ArpeggioType v);
    static ArpeggioType from_xml(const AsciiStringView &tag, ArpeggioType def);

    static const TranslatableString &user_name(EmbellishmentType v);
    static String translated_user_name(EmbellishmentType v);
    static String to_xml(EmbellishmentType v);
    static EmbellishmentType from_xml(const AsciiStringView &tag, EmbellishmentType def);
    static StringList embellishment_notes(EmbellishmentType v);
    static size_t embellishments_count();

    static const TranslatableString &user_name(ChordLineType v, bool straight, bool wavy);
    static AsciiStringView to_xml(ChordLineType v);
    static ChordLineType from_xml(const AsciiStringView &tag, ChordLineType def);

    static AsciiStringView to_xml(GuitarBendType v);
    static GuitarBendType from_xml(const AsciiStringView &tag, GuitarBendType def);

    static AsciiStringView to_xml(NoteCaseType v);
    static NoteCaseType from_xml(const AsciiStringView &tag, NoteCaseType def);

    static const String &user_name(DrumNum v);

    static const TranslatableString &user_name(GlissandoType v);
    static AsciiStringView to_xml(GlissandoType v);
    static GlissandoType from_xml(const AsciiStringView &tag, GlissandoType def);

    static const TranslatableString &user_name(JumpType v);
    static String translated_user_name(JumpType v);

    static const TranslatableString &user_name(MarkerType v);
    static String translated_user_name(MarkerType v);
    static AsciiStringView to_xml(MarkerType v);
    static MarkerType from_xml(const AsciiStringView &tag, MarkerType def);

    static String translated_user_name(StaffGroup v);
    static AsciiStringView to_xml(StaffGroup v);
    static StaffGroup from_xml(const AsciiStringView &tag, StaffGroup def);

    static const TranslatableString &user_name(StaffTypes v);
    static String translated_user_name(StaffTypes v);
    static AsciiStringView to_xml(StaffTypes v);
    static StaffTypes from_xml(const AsciiStringView &tag, StaffTypes def);

    static const TranslatableString &user_name(TrillType v);
    static String translated_user_name(TrillType v);
    static AsciiStringView to_xml(TrillType v);
    static TrillType from_xml(const AsciiStringView &tag, TrillType def);

    static const TranslatableString &user_name(VibratoType v);
    static String translated_user_name(VibratoType v);
    static AsciiStringView to_xml(VibratoType v);
    static VibratoType from_xml(const AsciiStringView &tag, VibratoType def);

    static const TranslatableString &user_name(ArticulationTextType v);
    static String text(ArticulationTextType v);
    static AsciiStringView to_xml(ArticulationTextType v);
    static ArticulationTextType from_xml(const AsciiStringView &tag, ArticulationTextType def);

    static AsciiStringView to_xml(LyricsSyllabic v);
    static LyricsSyllabic from_xml(const AsciiStringView &tag, LyricsSyllabic def);

    static AsciiStringView to_xml(LyricsDashSystemStart v);
    static LyricsDashSystemStart from_xml(const AsciiStringView &tag, LyricsDashSystemStart def);

    static const TranslatableString &user_name(Key v, bool is_atonal = false, bool is_custom = false);
    static String translated_user_name(Key v, bool is_atonal = false, bool is_custom = false);

    static AsciiStringView to_xml(TiePlacement interval);
    static TiePlacement from_xml(const AsciiStringView &str, TiePlacement def);

    static AsciiStringView to_xml(TieDotsPlacement placement);
    static TieDotsPlacement from_xml(const AsciiStringView &str, TieDotsPlacement def);

    static AsciiStringView to_xml(VoiceAssignment voice_appl);
    static VoiceAssignment from_xml(const AsciiStringView &str, VoiceAssignment def);

    static AsciiStringView to_xml(AutoOnOff auto_on_off);
    static AutoOnOff from_xml(const AsciiStringView &str, AutoOnOff def);

    static AsciiStringView to_xml(CapoParams::TransposeMode auto_on_off);
    static CapoParams::TransposeMode from_xml(const AsciiStringView &str, CapoParams::TransposeMode def);

    static AsciiStringView to_xml(PartialSpannerDirection v);
    static PartialSpannerDirection from_xml(const AsciiStringView &str, PartialSpannerDirection def);

    static AsciiStringView to_xml(TimeSigPlacement time_sig_pos);
    static TimeSigPlacement from_xml(const AsciiStringView &str, TimeSigPlacement def);

    static AsciiStringView to_xml(TimeSigStyle time_sig_style);
    static TimeSigStyle from_xml(const AsciiStringView &str, TimeSigStyle def);

    static AsciiStringView to_xml(TimeSigVSMargin time_sig_vs_margin);
    static TimeSigVSMargin from_xml(const AsciiStringView &str, TimeSigVSMargin def);

    static AsciiStringView to_xml(NoteSpellingType note_spelling_type);
    static NoteSpellingType from_xml(const AsciiStringView &str, NoteSpellingType def);

    static AsciiStringView to_xml(ChordStylePreset chord_style_preset);
    static ChordStylePreset from_xml(const AsciiStringView &str, ChordStylePreset def);

    static AsciiStringView to_xml(TappingHand tapping_hand);
    static TappingHand from_xml(const AsciiStringView &str, TappingHand def);

    static AsciiStringView to_xml(LHTappingSymbol lh);
    static LHTappingSymbol from_xml(const AsciiStringView &str, LHTappingSymbol def);

    static AsciiStringView to_xml(RHTappingSymbol rh);
    static RHTappingSymbol from_xml(const AsciiStringView &str, RHTappingSymbol def);

    static AsciiStringView to_xml(ParenthesesMode pm);
    static ParenthesesMode from_xml(const AsciiStringView &str, ParenthesesMode def);

    static const TranslatableString &user_name(RepeatPlayCountPreset v);
    static String translated_user_name(RepeatPlayCountPreset v);
    static AsciiStringView to_xml(RepeatPlayCountPreset repeat_preset);
    static RepeatPlayCountPreset from_xml(const AsciiStringView &str, RepeatPlayCountPreset def);

    static AsciiStringView to_xml(AutoCustomHide auto_on_off);
    static AutoCustomHide from_xml(const AsciiStringView &str, AutoCustomHide def);

    static AsciiStringView to_xml(MeasureNumberPlacement v);
    static MeasureNumberPlacement from_xml(const AsciiStringView &str, MeasureNumberPlacement def);

    static AsciiStringView to_xml(InstrumentNamesAlign v);
    static InstrumentNamesAlign from_xml(const AsciiStringView &str, InstrumentNamesAlign def);

    static AsciiStringView to_xml(InstrumentNamesFormat v);
    static InstrumentNamesFormat from_xml(const AsciiStringView &str, InstrumentNamesFormat def);

    static const TranslatableString &user_name(ActionIconType v);
    static char16_t icon_code(ActionIconType v);
    static AsciiStringView to_xml(ActionIconType v);
    static ActionIconType from_xml(const AsciiStringView &str, ActionIconType def);
};
} // namespace mu::engraving
