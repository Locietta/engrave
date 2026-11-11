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

#include <cassert>
#include <memory>
#include <variant>

#include "../types/symid.h"
#include "../types/types.h"
#include "global/logstream.h"
#include "global/types/string.h"

namespace mu::engraving {
struct Groups;
struct TDuration;

enum class PType : unsigned char {
    UNDEFINED = 0,
    // Base
    BOOL,
    INT,
    INT_VEC, // std::vector<int>
    SIZE_T,  // size_t
    REAL,
    STRING,

    // Geometry
    POINT, // point units, value saved as absolute or spatium depending on EngravingItem->sizeIsSpatiumDependent()
    SIZE,
    DRAW_PATH,
    SCALE,
    SPATIUM,
    ABSOLUTE,
    PAIR_REAL,

    // Draw
    SYMID,
    COLOR,
    ORNAMENT_STYLE,
    ORNAMENT_INTERVAL,
    ORNAMENT_SHOW_ACCIDENTAL,
    GLISS_STYLE,
    GLISS_TYPE,

    // Layout
    ALIGN,
    ALIGN_H,
    PLACEMENT_V,
    PLACEMENT_H,
    TEXT_PLACE,
    DIRECTION_V,
    DIRECTION_H,
    ORIENTATION,
    SHARED_LABEL_ORIENTATION,
    BEAM_MODE,
    ACCIDENTAL_ROLE,
    TIE_PLACEMENT,
    TIE_DOTS_PLACEMENT,

    TIMESIG_PLACEMENT,
    TIMESIG_STYLE,
    TIMESIG_MARGIN,
    NOTE_SPELLING_TYPE,
    CHORD_PRESET_TYPE,
    PARENTHESES_MODE,
    PLAY_COUNT_PRESET,

    // Sound
    FRACTION,
    DURATION_TYPE_WITH_DOTS,
    CHANGE_METHOD,
    PITCH_VALUES,
    TEMPO,

    // Types
    LAYOUTBREAK_TYPE,
    VELO_TYPE,
    BARLINE_TYPE,
    NOTEHEAD_TYPE,
    NOTEHEAD_SCHEME,
    NOTEHEAD_GROUP,
    CLEF_TYPE,
    CLEF_TO_BARLINE_POS,
    DYNAMIC_TYPE,
    DYNAMIC_SPEED,
    LINE_TYPE,
    HOOK_TYPE,
    KEY_MODE,
    TEXT_STYLE,
    PLAYTECH_TYPE,
    TEMPOCHANGE_TYPE,
    SLUR_STYLE_TYPE,
    NOTELINE_PLACEMENT_TYPE,
    LYRICS_DASH_SYSTEM_START_TYPE,
    PARTIAL_SPANNER_DIRECTION,
    MARKER_TYPE,
    ACTION_TYPE,

    LH_TAPPING_SYMBOL,
    RH_TAPPING_SYMBOL,
    VIBRATO_LINE_TYPE,

    VOICE_ASSIGNMENT,
    AUTO_ON_OFF,

    AUTO_CUSTOM_HIDE,

    MEASURE_NUMBER_PLACEMENT,
    CAPO_TRANSPOSE_MODE,

    INSTRUMENT_NAMES_ALIGN,
    INSTRUMENT_NAMES_FORMAT,
    INSTRUMENT_LABEL_VISIBILITY,

    // Other
    GROUPS,
};

struct PropertyValue {
    PropertyValue() = default;

    PropertyValue(const PropertyValue &) = default;

    PropertyValue &operator=(const PropertyValue &v) {
        if (this != &v) {
            PropertyValue copy(v);
            *this = std::move(copy);
        }
        return *this;
    }

    PropertyValue(PropertyValue &&v) noexcept : type_(v.type_), data_(std::move(v.data_)) { v.type_ = PType::UNDEFINED; }

    PropertyValue &operator=(PropertyValue &&v) noexcept {
        if (this != &v) {
            data_ = std::move(v.data_);
            type_ = v.type_;
            v.type_ = PType::UNDEFINED;
        }
        return *this;
    }

    // Base
    PropertyValue(bool v) : type_(PType::BOOL), data_(make_data<bool>(v)) {}

    PropertyValue(int v) : type_(PType::INT), data_(make_data<int>(v)) {}

    PropertyValue(const std::vector<int> &v) : type_(PType::INT_VEC), data_(make_data<std::vector<int>>(v)) {}

    PropertyValue(size_t v) : type_(PType::SIZE_T), data_(make_data<size_t>(v)) {}

    PropertyValue(double v) : type_(PType::REAL), data_(make_data<double>(v)) {}

    PropertyValue(const char *v) : type_(PType::STRING), data_(make_data<String>(String::from_utf8(v))) {}

    PropertyValue(const String &v) : type_(PType::STRING), data_(make_data<String>(v)) {}

    // Geometry
    PropertyValue(const PointF &v) : type_(PType::POINT), data_(make_data<PointF>(v)) {}

    PropertyValue(const PairF &v) : type_(PType::PAIR_REAL), data_(make_data<PairF>(v)) {}

    PropertyValue(const SizeF &v) : type_(PType::SIZE), data_(make_data<SizeF>(v)) {}

    PropertyValue(const PainterPath &v) : type_(PType::DRAW_PATH), data_(make_data<PainterPath>(v)) {}

    PropertyValue(const ScaleF &v) : type_(PType::SCALE), data_(make_data<ScaleF>(v)) {}

    PropertyValue(const Spatium &v) : type_(PType::SPATIUM), data_(make_data<Spatium>(v)) {}

    // Draw
    PropertyValue(SymId v) : type_(PType::SYMID), data_(make_data<SymId>(v)) {}

    PropertyValue(const Color &v) : type_(PType::COLOR), data_(make_data<Color>(v)) {}

    PropertyValue(OrnamentStyle v) : type_(PType::ORNAMENT_STYLE), data_(make_data<OrnamentStyle>(v)) {}

    PropertyValue(GlissandoStyle v) : type_(PType::GLISS_STYLE), data_(make_data<GlissandoStyle>(v)) {}

    PropertyValue(GlissandoType v) : type_(PType::GLISS_TYPE), data_(make_data<GlissandoType>(v)) {}

    // Layout
    PropertyValue(Align v) : type_(PType::ALIGN), data_(make_data<Align>(v)) {}

    PropertyValue(AlignH v) : type_(PType::ALIGN_H), data_(make_data<AlignH>(v)) {}

    PropertyValue(PlacementV v) : type_(PType::PLACEMENT_V), data_(make_data<PlacementV>(v)) {}

    PropertyValue(PlacementH v) : type_(PType::PLACEMENT_H), data_(make_data<PlacementH>(v)) {}

    PropertyValue(TextPlace v) : type_(PType::TEXT_PLACE), data_(make_data<TextPlace>(v)) {}

    PropertyValue(DirectionV v) : type_(PType::DIRECTION_V), data_(make_data<DirectionV>(v)) {}

    PropertyValue(DirectionH v) : type_(PType::DIRECTION_H), data_(make_data<DirectionH>(v)) {}

    PropertyValue(Orientation v) : type_(PType::ORIENTATION), data_(make_data<Orientation>(v)) {}

    PropertyValue(SharedLabelOrientation v) : type_(PType::SHARED_LABEL_ORIENTATION), data_(make_data<SharedLabelOrientation>(v)) {}

    PropertyValue(BeamMode v) : type_(PType::BEAM_MODE), data_(make_data<BeamMode>(v)) {}

    PropertyValue(const AccidentalRole &v) : type_(PType::ACCIDENTAL_ROLE), data_(make_data<AccidentalRole>(v)) {}

    PropertyValue(TiePlacement v) : type_(PType::TIE_PLACEMENT), data_(make_data<TiePlacement>(v)) {}

    PropertyValue(TieDotsPlacement v) : type_(PType::TIE_DOTS_PLACEMENT), data_(make_data<TieDotsPlacement>(v)) {}

    PropertyValue(TimeSigPlacement v) : type_(PType::TIMESIG_PLACEMENT), data_(make_data<TimeSigPlacement>(v)) {}

    PropertyValue(TimeSigStyle v) : type_(PType::TIMESIG_STYLE), data_(make_data<TimeSigStyle>(v)) {}

    PropertyValue(TimeSigVSMargin v) : type_(PType::TIMESIG_MARGIN), data_(make_data<TimeSigVSMargin>(v)) {}

    PropertyValue(NoteSpellingType v) : type_(PType::NOTE_SPELLING_TYPE), data_(make_data<NoteSpellingType>(v)) {}

    PropertyValue(const ChordStylePreset &v) : type_(PType::CHORD_PRESET_TYPE), data_(make_data<ChordStylePreset>(v)) {}

    PropertyValue(const ParenthesesMode &v) : type_(PType::PARENTHESES_MODE), data_(make_data<ParenthesesMode>(v)) {}

    PropertyValue(const RepeatPlayCountPreset &v) : type_(PType::PLAY_COUNT_PRESET), data_(make_data<RepeatPlayCountPreset>(v)) {}

    // Sound
    PropertyValue(const Fraction &v) : type_(PType::FRACTION), data_(make_data<Fraction>(v)) {}

    PropertyValue(const DurationTypeWithDots &v) : type_(PType::DURATION_TYPE_WITH_DOTS), data_(make_data<DurationTypeWithDots>(v)) {}

    PropertyValue(ChangeMethod v) : type_(PType::CHANGE_METHOD), data_(make_data<ChangeMethod>(v)) {}

    PropertyValue(const PitchValues &v) : type_(PType::PITCH_VALUES), data_(make_data<PitchValues>(v)) {}

    PropertyValue(const BeatsPerSecond &v) : type_(PType::TEMPO), data_(make_data<BeatsPerSecond>(v)) {}

    // Types
    PropertyValue(LayoutBreakType v) : type_(PType::LAYOUTBREAK_TYPE), data_(make_data<LayoutBreakType>(v)) {}

    PropertyValue(VeloType v) : type_(PType::VELO_TYPE), data_(make_data<VeloType>(v)) {}

    PropertyValue(BarLineType v) : type_(PType::BARLINE_TYPE), data_(make_data<BarLineType>(v)) {}

    PropertyValue(NoteHeadType v) : type_(PType::NOTEHEAD_TYPE), data_(make_data<NoteHeadType>(v)) {}

    PropertyValue(NoteHeadScheme v) : type_(PType::NOTEHEAD_SCHEME), data_(make_data<NoteHeadScheme>(v)) {}

    PropertyValue(NoteHeadGroup v) : type_(PType::NOTEHEAD_GROUP), data_(make_data<NoteHeadGroup>(v)) {}

    PropertyValue(ClefType v) : type_(PType::CLEF_TYPE), data_(make_data<ClefType>(v)) {}

    PropertyValue(ClefToBarlinePosition v) : type_(PType::CLEF_TO_BARLINE_POS), data_(make_data<ClefToBarlinePosition>(v)) {}

    PropertyValue(DynamicType v) : type_(PType::DYNAMIC_TYPE), data_(make_data<DynamicType>(v)) {}

    PropertyValue(DynamicSpeed v) : type_(PType::DYNAMIC_SPEED), data_(make_data<DynamicSpeed>(v)) {}

    PropertyValue(LineType v) : type_(PType::LINE_TYPE), data_(make_data<LineType>(v)) {}

    PropertyValue(HookType v) : type_(PType::HOOK_TYPE), data_(make_data<HookType>(v)) {}

    PropertyValue(KeyMode v) : type_(PType::KEY_MODE), data_(make_data<KeyMode>(v)) {}

    PropertyValue(TextStyleType v) : type_(PType::TEXT_STYLE), data_(make_data<TextStyleType>(v)) {}

    PropertyValue(PlayingTechniqueType v) : type_(PType::PLAYTECH_TYPE), data_(make_data<PlayingTechniqueType>(v)) {}

    PropertyValue(GradualTempoChangeType v) : type_(PType::TEMPOCHANGE_TYPE), data_(make_data<GradualTempoChangeType>(v)) {}

    PropertyValue(SlurStyleType v) : type_(PType::SLUR_STYLE_TYPE), data_(make_data<SlurStyleType>(v)) {}

    PropertyValue(const NoteLineEndPlacement &v) : type_(PType::NOTELINE_PLACEMENT_TYPE), data_(make_data<NoteLineEndPlacement>(v)) {}

    PropertyValue(ActionIconType v) : type_(PType::ACTION_TYPE), data_(make_data<ActionIconType>(v)) {}

    // Other
    PropertyValue(const GroupNodes &v) : type_(PType::GROUPS), data_(make_data<GroupNodes>(v)) {}

    PropertyValue(const OrnamentInterval &v) : type_(PType::ORNAMENT_INTERVAL), data_(make_data<OrnamentInterval>(v)) {}

    PropertyValue(const OrnamentShowAccidental &v) : type_(PType::ORNAMENT_SHOW_ACCIDENTAL), data_(make_data<OrnamentShowAccidental>(v)) {}

    PropertyValue(const LyricsDashSystemStart &v)
        : type_(PType::LYRICS_DASH_SYSTEM_START_TYPE), data_(make_data<LyricsDashSystemStart>(v)) {}

    PropertyValue(const PartialSpannerDirection &v)
        : type_(PType::PARTIAL_SPANNER_DIRECTION), data_(make_data<PartialSpannerDirection>(v)) {}

    PropertyValue(const LHTappingSymbol &v) : type_(PType::LH_TAPPING_SYMBOL), data_(make_data<LHTappingSymbol>(v)) {}

    PropertyValue(const RHTappingSymbol &v) : type_(PType::RH_TAPPING_SYMBOL), data_(make_data<RHTappingSymbol>(v)) {}

    PropertyValue(const VibratoType &v) : type_(PType::VIBRATO_LINE_TYPE), data_(make_data<VibratoType>(v)) {}

    PropertyValue(const VoiceAssignment &v) : type_(PType::VOICE_ASSIGNMENT), data_(make_data<VoiceAssignment>(v)) {}

    PropertyValue(const AutoOnOff &v) : type_(PType::AUTO_ON_OFF), data_(make_data<AutoOnOff>(v)) {}

    PropertyValue(const AutoCustomHide &v) : type_(PType::AUTO_CUSTOM_HIDE), data_(make_data<AutoCustomHide>(v)) {}

    PropertyValue(const MarkerType &v) : type_(PType::MARKER_TYPE), data_(make_data<MarkerType>(v)) {}

    PropertyValue(const MeasureNumberPlacement &v) : type_(PType::MEASURE_NUMBER_PLACEMENT), data_(make_data<MeasureNumberPlacement>(v)) {}

    PropertyValue(const CapoParams::TransposeMode &v) : type_(PType::CAPO_TRANSPOSE_MODE), data_(make_data<CapoParams::TransposeMode>(v)) {}

    PropertyValue(const InstrumentNamesAlign &v) : type_(PType::INSTRUMENT_NAMES_ALIGN), data_(make_data<InstrumentNamesAlign>(v)) {}

    PropertyValue(const InstrumentNamesFormat &v) : type_(PType::INSTRUMENT_NAMES_FORMAT), data_(make_data<InstrumentNamesFormat>(v)) {}

    PropertyValue(InstrumentLabelVisibility v)
        : type_(PType::INSTRUMENT_LABEL_VISIBILITY), data_(make_data<InstrumentLabelVisibility>(v)) {}

    bool is_valid() const;

    PType type() const;

    bool is_enum() const { return type_ != PType::UNDEFINED && std::holds_alternative<EnumValue>(data_); }

    template <typename T>
    T value() const {
        if (type_ == PType::UNDEFINED) {
            return T();
        }

        if constexpr (std::is_enum_v<T>) {
            if constexpr (requires { enum_type(T{}); }) {
                if (type_ == enum_type(T{})) return static_cast<T>(std::get<EnumValue>(data_).value);
            }
        }
        const T *at = get<T>();
        if (!at) {
            //! HACK Temporary hack for int to enum
            if constexpr (std::is_enum<T>::value) {
                if (PType::INT == type_) {
                    return static_cast<T>(value<int>());
                }
            }

            //! HACK Temporary hack for enum to int
            if constexpr (std::is_same<T, int>::value) {
                if (is_enum()) {
                    return std::get<EnumValue>(data_).value;
                }
            }

            //! HACK Temporary hack for bool to int
            if constexpr (std::is_same<T, int>::value) {
                if (PType::BOOL == type_) {
                    return value<bool>();
                }
            }

            //! HACK Temporary hack for int to bool
            if constexpr (std::is_same<T, bool>::value) {
                return value<int>();
            }

            //! HACK Temporary hack for int to size_t
            if constexpr (std::is_same<T, int>::value) {
                if (PType::SIZE_T == type_) {
                    return static_cast<int>(value<size_t>());
                }
            }

            //! HACK Temporary hack for real to Spatium
            if constexpr (std::is_same<T, Spatium>::value) {
                if (PType::REAL == type_) {
                    const double *srv = get<double>();
                    assert(srv);
                    return srv ? Spatium(*srv) : Spatium();
                }
            }

            //! HACK Temporary hack for Spatium to real
            if constexpr (std::is_same<T, double>::value) {
                if (PType::SPATIUM == type_) {
                    return value<Spatium>().val();
                }
            }

            //! HACK Temporary hack for Spatium to real
            if constexpr (std::is_same<T, double>::value) {
                if (PType::ABSOLUTE == type_) {
                    return value<double>();
                }
            }

            if constexpr (std::is_same<T, String>::value) {
                //! HACK Temporary hack for Fraction to String
                if (PType::FRACTION == type_) {
                    return value<Fraction>().to_string();
                }
            }
        }

        assert(at);
        if (!at) {
            return T();
        }
        return *at;
    }

    bool to_bool() const { return value<bool>(); }

    int to_int() const { return value<int>(); }

    double to_real() const { return value<double>(); }

    double to_double() const { return value<double>(); }

    bool operator==(const PropertyValue &v) const;

    inline bool operator!=(const PropertyValue &v) const { return !this->operator==(v); }

    template <typename T>
    static PropertyValue from_value(const T &v) {
        return PropertyValue(v);
    }


private:
    // Only the path exceeds the inline budget. Copies own independent paths.
    struct PathValue {
        std::unique_ptr<PainterPath> value;

        explicit PathValue(const PainterPath &v) : value(std::make_unique<PainterPath>(v)) {}

        PathValue(const PathValue &v) : value(v.value ? std::make_unique<PainterPath>(*v.value) : nullptr) {}

        PathValue(PathValue &&) noexcept = default;

        PathValue &operator=(const PathValue &v) {
            if (this != &v) {
                auto copy = PathValue(v);
                value.swap(copy.value);
            }
            return *this;
        }

        PathValue &operator=(PathValue &&) noexcept = default;

        bool operator==(const PathValue &v) const { return value && v.value ? *value == *v.value : bool(value) == bool(v.value); }
    };

    struct EnumValue {
        int value;
        bool operator==(const EnumValue &) const = default;
    };

    using Storage = std::variant<std::monostate, EnumValue, bool, int, std::vector<int>, size_t, double, String, PointF, PairF, SizeF,
                                 PathValue, ScaleF, Spatium, Color, Align, Fraction, DurationTypeWithDots, PitchValues, BeatsPerSecond,
                                 GroupNodes, OrnamentInterval>;

    static constexpr PType enum_type(SymId) { return PType::SYMID; }

    static constexpr PType enum_type(OrnamentStyle) { return PType::ORNAMENT_STYLE; }

    static constexpr PType enum_type(GlissandoStyle) { return PType::GLISS_STYLE; }

    static constexpr PType enum_type(GlissandoType) { return PType::GLISS_TYPE; }

    static constexpr PType enum_type(AlignH) { return PType::ALIGN_H; }

    static constexpr PType enum_type(PlacementV) { return PType::PLACEMENT_V; }

    static constexpr PType enum_type(PlacementH) { return PType::PLACEMENT_H; }

    static constexpr PType enum_type(TextPlace) { return PType::TEXT_PLACE; }

    static constexpr PType enum_type(DirectionV) { return PType::DIRECTION_V; }

    static constexpr PType enum_type(DirectionH) { return PType::DIRECTION_H; }

    static constexpr PType enum_type(Orientation) { return PType::ORIENTATION; }

    static constexpr PType enum_type(SharedLabelOrientation) { return PType::SHARED_LABEL_ORIENTATION; }

    static constexpr PType enum_type(BeamMode) { return PType::BEAM_MODE; }

    static constexpr PType enum_type(AccidentalRole) { return PType::ACCIDENTAL_ROLE; }

    static constexpr PType enum_type(TiePlacement) { return PType::TIE_PLACEMENT; }

    static constexpr PType enum_type(TieDotsPlacement) { return PType::TIE_DOTS_PLACEMENT; }

    static constexpr PType enum_type(TimeSigPlacement) { return PType::TIMESIG_PLACEMENT; }

    static constexpr PType enum_type(TimeSigStyle) { return PType::TIMESIG_STYLE; }

    static constexpr PType enum_type(TimeSigVSMargin) { return PType::TIMESIG_MARGIN; }

    static constexpr PType enum_type(NoteSpellingType) { return PType::NOTE_SPELLING_TYPE; }

    static constexpr PType enum_type(ChordStylePreset) { return PType::CHORD_PRESET_TYPE; }

    static constexpr PType enum_type(ParenthesesMode) { return PType::PARENTHESES_MODE; }

    static constexpr PType enum_type(RepeatPlayCountPreset) { return PType::PLAY_COUNT_PRESET; }

    static constexpr PType enum_type(ChangeMethod) { return PType::CHANGE_METHOD; }

    static constexpr PType enum_type(LayoutBreakType) { return PType::LAYOUTBREAK_TYPE; }

    static constexpr PType enum_type(VeloType) { return PType::VELO_TYPE; }

    static constexpr PType enum_type(BarLineType) { return PType::BARLINE_TYPE; }

    static constexpr PType enum_type(NoteHeadType) { return PType::NOTEHEAD_TYPE; }

    static constexpr PType enum_type(NoteHeadScheme) { return PType::NOTEHEAD_SCHEME; }

    static constexpr PType enum_type(NoteHeadGroup) { return PType::NOTEHEAD_GROUP; }

    static constexpr PType enum_type(ClefType) { return PType::CLEF_TYPE; }

    static constexpr PType enum_type(ClefToBarlinePosition) { return PType::CLEF_TO_BARLINE_POS; }

    static constexpr PType enum_type(DynamicType) { return PType::DYNAMIC_TYPE; }

    static constexpr PType enum_type(DynamicSpeed) { return PType::DYNAMIC_SPEED; }

    static constexpr PType enum_type(LineType) { return PType::LINE_TYPE; }

    static constexpr PType enum_type(HookType) { return PType::HOOK_TYPE; }

    static constexpr PType enum_type(KeyMode) { return PType::KEY_MODE; }

    static constexpr PType enum_type(TextStyleType) { return PType::TEXT_STYLE; }

    static constexpr PType enum_type(PlayingTechniqueType) { return PType::PLAYTECH_TYPE; }

    static constexpr PType enum_type(GradualTempoChangeType) { return PType::TEMPOCHANGE_TYPE; }

    static constexpr PType enum_type(SlurStyleType) { return PType::SLUR_STYLE_TYPE; }

    static constexpr PType enum_type(NoteLineEndPlacement) { return PType::NOTELINE_PLACEMENT_TYPE; }

    static constexpr PType enum_type(ActionIconType) { return PType::ACTION_TYPE; }

    static constexpr PType enum_type(OrnamentShowAccidental) { return PType::ORNAMENT_SHOW_ACCIDENTAL; }

    static constexpr PType enum_type(LyricsDashSystemStart) { return PType::LYRICS_DASH_SYSTEM_START_TYPE; }

    static constexpr PType enum_type(PartialSpannerDirection) { return PType::PARTIAL_SPANNER_DIRECTION; }

    static constexpr PType enum_type(LHTappingSymbol) { return PType::LH_TAPPING_SYMBOL; }

    static constexpr PType enum_type(RHTappingSymbol) { return PType::RH_TAPPING_SYMBOL; }

    static constexpr PType enum_type(VibratoType) { return PType::VIBRATO_LINE_TYPE; }

    static constexpr PType enum_type(VoiceAssignment) { return PType::VOICE_ASSIGNMENT; }

    static constexpr PType enum_type(AutoOnOff) { return PType::AUTO_ON_OFF; }

    static constexpr PType enum_type(AutoCustomHide) { return PType::AUTO_CUSTOM_HIDE; }

    static constexpr PType enum_type(MarkerType) { return PType::MARKER_TYPE; }

    static constexpr PType enum_type(MeasureNumberPlacement) { return PType::MEASURE_NUMBER_PLACEMENT; }

    static constexpr PType enum_type(CapoParams::TransposeMode) { return PType::CAPO_TRANSPOSE_MODE; }

    static constexpr PType enum_type(InstrumentNamesAlign) { return PType::INSTRUMENT_NAMES_ALIGN; }

    static constexpr PType enum_type(InstrumentNamesFormat) { return PType::INSTRUMENT_NAMES_FORMAT; }

    static constexpr PType enum_type(InstrumentLabelVisibility) { return PType::INSTRUMENT_LABEL_VISIBILITY; }

    template <typename T>
    static Storage make_data(const T &v) {
        if constexpr (std::is_enum_v<T>)
            return EnumValue{static_cast<int>(v)};
        else if constexpr (std::is_same_v<T, PainterPath>)
            return PathValue(v);
        else
            return v;
    }

    template <typename T, typename... Ts>
    static constexpr bool stored_type(std::variant<Ts...> *) {
        return (std::is_same_v<T, Ts> || ...);
    }

    template <typename T>
    const T *get() const {
        if constexpr (std::is_same_v<T, PainterPath>) {
            const auto *path = std::get_if<PathValue>(&data_);
            return path ? path->value.get() : nullptr;
        } else if constexpr (stored_type<T>(static_cast<Storage *>(nullptr))) {
            return std::get_if<T>(&data_);
        } else {
            return nullptr;
        }
    }

    PType type_ = PType::UNDEFINED;
    Storage data_;
};

static_assert(sizeof(PropertyValue) <= 48, "Keep PropertyValue inline, including a future 32-byte String");
} // namespace mu::engraving

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const mu::engraving::PropertyValue &) {
    s << "property(not implemented log output)";
    return s;
}
