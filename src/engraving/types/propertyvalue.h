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

#include <memory>
#include <cassert>

#include "global/types/string.h"
#include "global/logstream.h"

#include "../types/types.h"
#include "../types/symid.h"

namespace mu::engraving {
class Groups;
class TDuration;

enum class P_TYPE : unsigned char {
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

class PropertyValue {
public:
    PropertyValue() = default;

    // Base
    PropertyValue(bool v) : type_(P_TYPE::BOOL), data_(make_data<bool>(v)) {}

    PropertyValue(int v) : type_(P_TYPE::INT), data_(make_data<int>(v)) {}

    PropertyValue(const std::vector<int> &v) : type_(P_TYPE::INT_VEC), data_(make_data<std::vector<int>>(v)) {}

    PropertyValue(size_t v) : type_(P_TYPE::SIZE_T), data_(make_data<size_t>(v)) {}

    PropertyValue(double v) : type_(P_TYPE::REAL), data_(make_data<double>(v)) {}

    PropertyValue(const char *v) : type_(P_TYPE::STRING), data_(make_data<String>(String::fromUtf8(v))) {}

    PropertyValue(const String &v) : type_(P_TYPE::STRING), data_(make_data<String>(v)) {}

    // Geometry
    PropertyValue(const PointF &v) : type_(P_TYPE::POINT), data_(make_data<PointF>(v)) {}

    PropertyValue(const PairF &v) : type_(P_TYPE::PAIR_REAL), data_(make_data<PairF>(v)) {}

    PropertyValue(const SizeF &v) : type_(P_TYPE::SIZE), data_(make_data<SizeF>(v)) {}

    PropertyValue(const PainterPath &v) : type_(P_TYPE::DRAW_PATH), data_(make_data<PainterPath>(v)) {}

    PropertyValue(const ScaleF &v) : type_(P_TYPE::SCALE), data_(make_data<ScaleF>(v)) {}

    PropertyValue(const Spatium &v) : type_(P_TYPE::SPATIUM), data_(make_data<Spatium>(v)) {}

    // Draw
    PropertyValue(SymId v) : type_(P_TYPE::SYMID), data_(make_data<SymId>(v)) {}

    PropertyValue(const Color &v) : type_(P_TYPE::COLOR), data_(make_data<Color>(v)) {}

    PropertyValue(OrnamentStyle v) : type_(P_TYPE::ORNAMENT_STYLE), data_(make_data<OrnamentStyle>(v)) {}

    PropertyValue(GlissandoStyle v) : type_(P_TYPE::GLISS_STYLE), data_(make_data<GlissandoStyle>(v)) {}

    PropertyValue(GlissandoType v) : type_(P_TYPE::GLISS_TYPE), data_(make_data<GlissandoType>(v)) {}

    // Layout
    PropertyValue(Align v) : type_(P_TYPE::ALIGN), data_(make_data<Align>(v)) {}

    PropertyValue(AlignH v) : type_(P_TYPE::ALIGN_H), data_(make_data<AlignH>(v)) {}

    PropertyValue(PlacementV v) : type_(P_TYPE::PLACEMENT_V), data_(make_data<PlacementV>(v)) {}

    PropertyValue(PlacementH v) : type_(P_TYPE::PLACEMENT_H), data_(make_data<PlacementH>(v)) {}

    PropertyValue(TextPlace v) : type_(P_TYPE::TEXT_PLACE), data_(make_data<TextPlace>(v)) {}

    PropertyValue(DirectionV v) : type_(P_TYPE::DIRECTION_V), data_(make_data<DirectionV>(v)) {}

    PropertyValue(DirectionH v) : type_(P_TYPE::DIRECTION_H), data_(make_data<DirectionH>(v)) {}

    PropertyValue(Orientation v) : type_(P_TYPE::ORIENTATION), data_(make_data<Orientation>(v)) {}

    PropertyValue(SharedLabelOrientation v) : type_(P_TYPE::SHARED_LABEL_ORIENTATION), data_(make_data<SharedLabelOrientation>(v)) {}

    PropertyValue(BeamMode v) : type_(P_TYPE::BEAM_MODE), data_(make_data<BeamMode>(v)) {}

    PropertyValue(const AccidentalRole &v) : type_(P_TYPE::ACCIDENTAL_ROLE), data_(make_data<AccidentalRole>(v)) {}

    PropertyValue(TiePlacement v) : type_(P_TYPE::TIE_PLACEMENT), data_(make_data<TiePlacement>(v)) {}

    PropertyValue(TieDotsPlacement v) : type_(P_TYPE::TIE_DOTS_PLACEMENT), data_(make_data<TieDotsPlacement>(v)) {}

    PropertyValue(TimeSigPlacement v) : type_(P_TYPE::TIMESIG_PLACEMENT), data_(make_data<TimeSigPlacement>(v)) {}

    PropertyValue(TimeSigStyle v) : type_(P_TYPE::TIMESIG_STYLE), data_(make_data<TimeSigStyle>(v)) {}

    PropertyValue(TimeSigVSMargin v) : type_(P_TYPE::TIMESIG_MARGIN), data_(make_data<TimeSigVSMargin>(v)) {}

    PropertyValue(NoteSpellingType v) : type_(P_TYPE::NOTE_SPELLING_TYPE), data_(make_data<NoteSpellingType>(v)) {}

    PropertyValue(const ChordStylePreset &v) : type_(P_TYPE::CHORD_PRESET_TYPE), data_(make_data<ChordStylePreset>(v)) {}

    PropertyValue(const ParenthesesMode &v) : type_(P_TYPE::PARENTHESES_MODE), data_(make_data<ParenthesesMode>(v)) {}

    PropertyValue(const RepeatPlayCountPreset &v) : type_(P_TYPE::PLAY_COUNT_PRESET), data_(make_data<RepeatPlayCountPreset>(v)) {}

    // Sound
    PropertyValue(const Fraction &v) : type_(P_TYPE::FRACTION), data_(make_data<Fraction>(v)) {}

    PropertyValue(const DurationTypeWithDots &v) : type_(P_TYPE::DURATION_TYPE_WITH_DOTS), data_(make_data<DurationTypeWithDots>(v)) {}

    PropertyValue(ChangeMethod v) : type_(P_TYPE::CHANGE_METHOD), data_(make_data<ChangeMethod>(v)) {}

    PropertyValue(const PitchValues &v) : type_(P_TYPE::PITCH_VALUES), data_(make_data<PitchValues>(v)) {}

    PropertyValue(const BeatsPerSecond &v) : type_(P_TYPE::TEMPO), data_(make_data<BeatsPerSecond>(v)) {}

    // Types
    PropertyValue(LayoutBreakType v) : type_(P_TYPE::LAYOUTBREAK_TYPE), data_(make_data<LayoutBreakType>(v)) {}

    PropertyValue(VeloType v) : type_(P_TYPE::VELO_TYPE), data_(make_data<VeloType>(v)) {}

    PropertyValue(BarLineType v) : type_(P_TYPE::BARLINE_TYPE), data_(make_data<BarLineType>(v)) {}

    PropertyValue(NoteHeadType v) : type_(P_TYPE::NOTEHEAD_TYPE), data_(make_data<NoteHeadType>(v)) {}

    PropertyValue(NoteHeadScheme v) : type_(P_TYPE::NOTEHEAD_SCHEME), data_(make_data<NoteHeadScheme>(v)) {}

    PropertyValue(NoteHeadGroup v) : type_(P_TYPE::NOTEHEAD_GROUP), data_(make_data<NoteHeadGroup>(v)) {}

    PropertyValue(ClefType v) : type_(P_TYPE::CLEF_TYPE), data_(make_data<ClefType>(v)) {}

    PropertyValue(ClefToBarlinePosition v) : type_(P_TYPE::CLEF_TO_BARLINE_POS), data_(make_data<ClefToBarlinePosition>(v)) {}

    PropertyValue(DynamicType v) : type_(P_TYPE::DYNAMIC_TYPE), data_(make_data<DynamicType>(v)) {}

    PropertyValue(DynamicSpeed v) : type_(P_TYPE::DYNAMIC_SPEED), data_(make_data<DynamicSpeed>(v)) {}

    PropertyValue(LineType v) : type_(P_TYPE::LINE_TYPE), data_(make_data<LineType>(v)) {}

    PropertyValue(HookType v) : type_(P_TYPE::HOOK_TYPE), data_(make_data<HookType>(v)) {}

    PropertyValue(KeyMode v) : type_(P_TYPE::KEY_MODE), data_(make_data<KeyMode>(v)) {}

    PropertyValue(TextStyleType v) : type_(P_TYPE::TEXT_STYLE), data_(make_data<TextStyleType>(v)) {}

    PropertyValue(PlayingTechniqueType v) : type_(P_TYPE::PLAYTECH_TYPE), data_(make_data<PlayingTechniqueType>(v)) {}

    PropertyValue(GradualTempoChangeType v) : type_(P_TYPE::TEMPOCHANGE_TYPE), data_(make_data<GradualTempoChangeType>(v)) {}

    PropertyValue(SlurStyleType v) : type_(P_TYPE::SLUR_STYLE_TYPE), data_(make_data<SlurStyleType>(v)) {}

    PropertyValue(const NoteLineEndPlacement &v) : type_(P_TYPE::NOTELINE_PLACEMENT_TYPE), data_(make_data<NoteLineEndPlacement>(v)) {}

    PropertyValue(ActionIconType v) : type_(P_TYPE::ACTION_TYPE), data_(make_data<ActionIconType>(v)) {}

    // Other
    PropertyValue(const GroupNodes &v) : type_(P_TYPE::GROUPS), data_(make_data<GroupNodes>(v)) {}

    PropertyValue(const OrnamentInterval &v) : type_(P_TYPE::ORNAMENT_INTERVAL), data_(make_data<OrnamentInterval>(v)) {}

    PropertyValue(const OrnamentShowAccidental &v) : type_(P_TYPE::ORNAMENT_SHOW_ACCIDENTAL), data_(make_data<OrnamentShowAccidental>(v)) {}

    PropertyValue(const LyricsDashSystemStart &v)
        : type_(P_TYPE::LYRICS_DASH_SYSTEM_START_TYPE), data_(make_data<LyricsDashSystemStart>(v)) {}

    PropertyValue(const PartialSpannerDirection &v)
        : type_(P_TYPE::PARTIAL_SPANNER_DIRECTION), data_(make_data<PartialSpannerDirection>(v)) {}

    PropertyValue(const LHTappingSymbol &v) : type_(P_TYPE::LH_TAPPING_SYMBOL), data_(make_data<LHTappingSymbol>(v)) {}

    PropertyValue(const RHTappingSymbol &v) : type_(P_TYPE::RH_TAPPING_SYMBOL), data_(make_data<RHTappingSymbol>(v)) {}

    PropertyValue(const VibratoType &v) : type_(P_TYPE::VIBRATO_LINE_TYPE), data_(make_data<VibratoType>(v)) {}

    PropertyValue(const VoiceAssignment &v) : type_(P_TYPE::VOICE_ASSIGNMENT), data_(make_data<VoiceAssignment>(v)) {}

    PropertyValue(const AutoOnOff &v) : type_(P_TYPE::AUTO_ON_OFF), data_(make_data<AutoOnOff>(v)) {}

    PropertyValue(const AutoCustomHide &v) : type_(P_TYPE::AUTO_CUSTOM_HIDE), data_(make_data<AutoCustomHide>(v)) {}

    PropertyValue(const MarkerType &v) : type_(P_TYPE::MARKER_TYPE), data_(make_data<MarkerType>(v)) {}

    PropertyValue(const MeasureNumberPlacement &v) : type_(P_TYPE::MEASURE_NUMBER_PLACEMENT), data_(make_data<MeasureNumberPlacement>(v)) {}

    PropertyValue(const CapoParams::TransposeMode &v)
        : type_(P_TYPE::CAPO_TRANSPOSE_MODE), data_(make_data<CapoParams::TransposeMode>(v)) {}

    PropertyValue(const InstrumentNamesAlign &v) : type_(P_TYPE::INSTRUMENT_NAMES_ALIGN), data_(make_data<InstrumentNamesAlign>(v)) {}

    PropertyValue(const InstrumentNamesFormat &v) : type_(P_TYPE::INSTRUMENT_NAMES_FORMAT), data_(make_data<InstrumentNamesFormat>(v)) {}

    PropertyValue(InstrumentLabelVisibility v)
        : type_(P_TYPE::INSTRUMENT_LABEL_VISIBILITY), data_(make_data<InstrumentLabelVisibility>(v)) {}

    bool isValid() const;

    P_TYPE type() const;

    bool isEnum() const { return data_ ? data_->isEnum() : false; }

    template <typename T>
    T value() const {
        if (type_ == P_TYPE::UNDEFINED) {
            return T();
        }

        assert(data_);
        if (!data_) {
            return T();
        }

        Arg<T> *at = get<T>();
        if (!at) {
            //! HACK Temporary hack for int to enum
            if constexpr (std::is_enum<T>::value) {
                if (P_TYPE::INT == type_) {
                    return static_cast<T>(value<int>());
                }
            }

            //! HACK Temporary hack for enum to int
            if constexpr (std::is_same<T, int>::value) {
                if (data_->isEnum()) {
                    return data_->enumToInt();
                }
            }

            //! HACK Temporary hack for bool to int
            if constexpr (std::is_same<T, int>::value) {
                if (P_TYPE::BOOL == type_) {
                    return value<bool>();
                }
            }

            //! HACK Temporary hack for int to bool
            if constexpr (std::is_same<T, bool>::value) {
                return value<int>();
            }

            //! HACK Temporary hack for int to size_t
            if constexpr (std::is_same<T, int>::value) {
                if (P_TYPE::SIZE_T == type_) {
                    return static_cast<int>(value<size_t>());
                }
            }

            //! HACK Temporary hack for real to Spatium
            if constexpr (std::is_same<T, Spatium>::value) {
                if (P_TYPE::REAL == type_) {
                    Arg<double> *srv = get<double>();
                    assert(srv);
                    return srv ? Spatium(srv->v) : Spatium();
                }
            }

            //! HACK Temporary hack for Spatium to real
            if constexpr (std::is_same<T, double>::value) {
                if (P_TYPE::SPATIUM == type_) {
                    return value<Spatium>().val();
                }
            }

            //! HACK Temporary hack for Spatium to real
            if constexpr (std::is_same<T, double>::value) {
                if (P_TYPE::ABSOLUTE == type_) {
                    return value<double>();
                }
            }

            if constexpr (std::is_same<T, String>::value) {
                //! HACK Temporary hack for Fraction to String
                if (P_TYPE::FRACTION == type_) {
                    return value<Fraction>().toString();
                }
            }
        }

        assert(at);
        if (!at) {
            return T();
        }
        return at->v;
    }

    bool toBool() const { return value<bool>(); }

    int toInt() const { return value<int>(); }

    double toReal() const { return value<double>(); }

    double toDouble() const { return value<double>(); }

    bool operator==(const PropertyValue &v) const;

    inline bool operator!=(const PropertyValue &v) const { return !this->operator==(v); }

    template <typename T>
    static PropertyValue fromValue(const T &v) {
        return PropertyValue(v);
    }


private:
    struct IArg {
        virtual ~IArg() = default;

        virtual bool equal(const IArg *a) const = 0;

        virtual bool isEnum() const = 0;
        virtual int enumToInt() const = 0;
    };

    template <typename T>
    struct Arg : public IArg {
        T v;

        Arg(const T &v) : IArg(), v(v) {}

        bool equal(const IArg *a) const override {
            assert(a);
            const Arg<T> *at = dynamic_cast<const Arg<T> *>(a);
            assert(at);
            return at ? at->v == v : false;
        }

        //! HACK Temporary hack for enum to int
        bool isEnum() const override { return std::is_enum<T>::value; }

        int enumToInt() const override {
            if constexpr (std::is_enum<T>::value) {
                return static_cast<int>(v);
            } else {
                return -1;
            }
        }
    };

    template <typename T>
    inline std::shared_ptr<IArg> make_data(const T &v) const {
        return std::shared_ptr<IArg>(new Arg<T>(v));
    }

    template <typename T>
    inline Arg<T> *get() const {
        return dynamic_cast<Arg<T> *>(data_.get());
    }

    P_TYPE type_ = P_TYPE::UNDEFINED;
    std::shared_ptr<IArg> data_ = nullptr;
};
} // namespace mu::engraving

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const mu::engraving::PropertyValue &) {
    s << "property(not implemented log output)";
    return s;
}
