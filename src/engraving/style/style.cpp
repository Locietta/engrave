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

#include "style.h"

#include <string_view>

#include "compat/pageformat.h"
#include "defaultstyle.h"
#include "dom/mscore.h"
#include "dom/pedal.h"
#include "engraving/internal/engravingfontsprovider.h"
#include "log.h"
#include "rw/compat/compatutils.h"
#include "rw/compat/readchordlisthook.h"
#include "rw/xmlreader.h"
#include "rw/xmlwriter.h"
#include "types/constants.h"
#include "types/typesconv.h"

using namespace mu;
using namespace muse::io;
using namespace mu::engraving;

const PropertyValue &MStyle::value(Sid idx) const {
    if (idx == Sid::NOSTYLE) {
        static PropertyValue dummy;
        return dummy;
    }

    const PropertyValue &val = values_[size_t(idx)];
    if (val.is_valid()) {
        return val;
    }

    return StyleDef::style_values[size_t(idx)].default_value;
}

double MStyle::value_absolute(Sid idx) const {
    if (idx == Sid::NOSTYLE) {
        return 0.0;
    }

    return precomputed_values_[size_t(idx)];
}

void MStyle::set(const Sid t, const PropertyValue &val) {
    if (t == Sid::NOSTYLE) {
        return;
    }

    const size_t idx = size_t(t);
    values_[idx] = val;
    if (t == Sid::SPATIUM) {
        precompute_values();
    } else {
        if (StyleDef::style_values[idx].value_type() == PType::SPATIUM) {
            double spatium_value = value(Sid::SPATIUM).to_real();
            precomputed_values_[idx] = values_[idx].value<Spatium>().val() * spatium_value;
        }
    }
}

double MStyle::default_spatium() const { return StyleDef::style_values[static_cast<size_t>(Sid::SPATIUM)].default_value.to_double(); }

void MStyle::precompute_values() {
    double spatium_value = value(Sid::SPATIUM).to_real();
    for (const StyleDef::StyleValue &t : StyleDef::style_values) {
        if (t.value_type() == PType::SPATIUM) {
            precomputed_values_[t.idx()] = value(t.sid).value<Spatium>().val() * spatium_value;
        }
    }
}

bool MStyle::is_default(Sid idx) const { return value(idx) == DefaultStyle::resolve_style_defaults(default_style_version()).value(idx); }

void MStyle::set_default_style_version(const int defaults_version) { set(Sid::DEFAULTS_VERSION, defaults_version); }

int MStyle::default_style_version() const { return style_i(Sid::DEFAULTS_VERSION); }

bool MStyle::read_properties(XmlReader &e) {
    const AsciiStringView tag(e.name());

    for (const StyleDef::StyleValue &t : StyleDef::style_values) {
        Sid idx = t.sid;
        if (t.xml_name == tag) {
            PType type = t.value_type();
            switch (type) {
                case PType::SPATIUM: set(idx, Spatium(e.read_double())); break;
                case PType::REAL: set(idx, e.read_double()); break;
                case PType::BOOL: set(idx, bool(e.read_int())); break;
                case PType::INT: set(idx, e.read_int()); break;
                case PType::DIRECTION_V: set(idx, DirectionV(e.read_int())); break;
                case PType::STRING: set(idx, e.read_text()); break;
                case PType::ALIGN: {
                    Align align = TConv::from_xml(e.read_text(), Align());
                    set(idx, align);
                } break;
                case PType::ALIGN_H: {
                    AlignH align = TConv::from_xml(e.read_ascii_text(), AlignH::HCENTER);
                    set(idx, align);
                } break;
                case PType::POINT: {
                    double x = e.double_attribute("x", 0.0);
                    double y = e.double_attribute("y", 0.0);
                    set(idx, PointF(x, y));
                    e.read_text();
                } break;
                case PType::SIZE: {
                    double x = e.double_attribute("w", 0.0);
                    double y = e.double_attribute("h", 0.0);
                    set(idx, SizeF(x, y));
                    e.read_text();
                } break;
                case PType::SCALE: {
                    double sx = e.double_attribute("w", 0.0);
                    double sy = e.double_attribute("h", 0.0);
                    set(idx, ScaleF(sx, sy));
                    e.read_text();
                } break;
                case PType::COLOR: {
                    Color c;
                    c.set_red(e.int_attribute("r"));
                    c.set_green(e.int_attribute("g"));
                    c.set_blue(e.int_attribute("b"));
                    c.set_alpha(e.int_attribute("a", 255));
                    set(idx, c);
                    e.read_text();
                } break;
                case PType::PLACEMENT_V: set(idx, PlacementV(e.read_text().to_int())); break;
                case PType::PLACEMENT_H: set(idx, PlacementH(e.read_text().to_int())); break;
                case PType::DIRECTION_H: set(idx, DirectionH(e.read_text().to_int())); break;
                case PType::ORIENTATION: set(idx, TConv::from_xml(e.read_ascii_text(), Orientation::HORIZONTAL)); break;
                case PType::SHARED_LABEL_ORIENTATION:
                    set(idx, TConv::from_xml(e.read_ascii_text(), SharedLabelOrientation::HORIZONTAL));
                    break;
                case PType::HOOK_TYPE: set(idx, HookType(e.read_text().to_int())); break;
                case PType::LINE_TYPE: set(idx, TConv::from_xml(e.read_ascii_text(), LineType::SOLID)); break;
                case PType::CLEF_TO_BARLINE_POS: set(idx, ClefToBarlinePosition(e.read_int())); break;
                case PType::TIE_PLACEMENT: set(idx, TConv::from_xml(e.read_ascii_text(), TiePlacement::AUTO)); break;
                case PType::TIE_DOTS_PLACEMENT: set(idx, TConv::from_xml(e.read_ascii_text(), TieDotsPlacement::AUTO)); break;
                case PType::GLISS_STYLE: set(idx, GlissandoStyle(e.read_text().to_int())); break;
                case PType::GLISS_TYPE: set(idx, GlissandoType(e.read_text().to_int())); break;
                case PType::TIMESIG_PLACEMENT: set(idx, TConv::from_xml(e.read_ascii_text(), TimeSigPlacement::NORMAL)); break;
                case PType::TIMESIG_STYLE: set(idx, TConv::from_xml(e.read_ascii_text(), TimeSigStyle::NORMAL)); break;
                case PType::TIMESIG_MARGIN: set(idx, TConv::from_xml(e.read_ascii_text(), TimeSigVSMargin::RIGHT_ALIGN_TO_BARLINE)); break;
                case PType::NOTE_SPELLING_TYPE: set(idx, TConv::from_xml(e.read_ascii_text(), NoteSpellingType::STANDARD)); break;
                case PType::CHORD_PRESET_TYPE: set(idx, TConv::from_xml(e.read_ascii_text(), ChordStylePreset::STANDARD)); break;
                case PType::PARENTHESES_MODE: set(idx, TConv::from_xml(e.read_ascii_text(), ParenthesesMode::NONE)); break;
                case PType::LH_TAPPING_SYMBOL: set(idx, TConv::from_xml(e.read_ascii_text(), LHTappingSymbol::DOT)); break;
                case PType::RH_TAPPING_SYMBOL: set(idx, TConv::from_xml(e.read_ascii_text(), RHTappingSymbol::T)); break;
                case PType::TEXT_STYLE: set(idx, TConv::from_xml(e.read_ascii_text(), TextStyleType::DEFAULT)); break;
                case PType::PLAY_COUNT_PRESET: set(idx, TConv::from_xml(e.read_ascii_text(), RepeatPlayCountPreset::X_N)); break;
                case PType::MEASURE_NUMBER_PLACEMENT:
                    set(idx, TConv::from_xml(e.read_ascii_text(), MeasureNumberPlacement::ABOVE_SYSTEM));
                    break;
                case PType::INSTRUMENT_NAMES_ALIGN:
                    set(idx, TConv::from_xml(e.read_ascii_text(), InstrumentNamesAlign::RIGHT_RIGHT));
                    break;
                case PType::INSTRUMENT_NAMES_FORMAT:
                    set(idx, TConv::from_xml(e.read_ascii_text(), InstrumentNamesFormat::NAME_IN_TRANSP_NUM));
                    break;
                case PType::AUTO_ON_OFF: set(idx, TConv::from_xml(e.read_ascii_text(), AutoOnOff::AUTO)); break;
                default: ASSERT_X(u"unhandled type " + String::number(int(type)));
            }
            return true;
        }
    }
    if (read_style_val_compat(e)) {
        return true;
    }
    return false;
}

//---------------------------------------------------------
//   readStyleValCompat
//    Read obsolete style values which may appear in files
//    produced by older versions of MuseScore.
//---------------------------------------------------------

bool MStyle::read_style_val_compat(XmlReader &e) {
    const AsciiStringView tag(e.name());
    if (tag == "tempoOffset") { // pre-3.0-beta
        const double x = e.double_attribute("x", 0.0);
        const double y = e.double_attribute("y", 0.0);
        const PointF val(x, y);
        set(Sid::TEMPO_POS_ABOVE, val);
        set(Sid::TEMPO_POS_BELOW, val);
        e.read_text();
        return true;
    }
    if (read_text_style_val_compat(e)) {
        return true;
    }
    return false;
}

//---------------------------------------------------------
//   readTextStyleValCompat
//    Handle transition from separate bold, underline, strike
//    and italic style properties to the single *FontStyle
//    property set.
//---------------------------------------------------------

bool MStyle::read_text_style_val_compat(XmlReader &e) {
    static const std::array<std::pair<String, FontStyle>, 4> style_names_endings{{{u"FontBold", FontStyle::BOLD},
                                                                                  {u"FontItalic", FontStyle::ITALIC},
                                                                                  {u"FontUnderline", FontStyle::UNDERLINE},
                                                                                  {u"FontStrike", FontStyle::STRIKE}}};

    const String tag = String::from_ascii(e.name().ascii());
    FontStyle read_font_style = FontStyle::NORMAL;
    String type_name;
    for (auto &font_style : style_names_endings) {
        if (tag.ends_with(font_style.first)) {
            read_font_style = font_style.second;
            type_name = tag.mid(0, tag.size() - font_style.first.size());
            break;
        }
    }
    if (read_font_style == FontStyle::NORMAL) {
        return false;
    }

    const String new_font_style_name = type_name + u"FontStyle";
    const Sid sid = MStyle::style_idx(new_font_style_name);
    if (sid == Sid::NOSTYLE) {
        LOGW() << "readFontStyleValCompat: couldn't read text readFontStyle value:" << tag;
        return false;
    }

    const bool read_val = bool(e.read_text().to_int());
    const PropertyValue &val = value(sid);
    FontStyle new_font_style = val.is_valid() ? FontStyle(val.to_int()) : FontStyle::NORMAL;
    if (read_val) {
        new_font_style = new_font_style + read_font_style;
    } else {
        new_font_style = new_font_style - read_font_style;
    }

    set(sid, int(new_font_style));
    return true;
}

void MStyle::apply_compat_style_vals(int msc_version) {
    if (msc_version < 500) {
        set(Sid::WINDS_NAME_BY_GROUP, false);
        set(Sid::VOCALS_NAME_BY_GROUP, false);
        set(Sid::MASK_SLURS, false);
        set(Sid::MASK_TIES, false);
        set(Sid::DASH_BAR_WIDTH, value(Sid::BAR_WIDTH));
        set(Sid::LYRICS_STACKING_ORDER, int(LyricsStackingOrder::LYRICS_AFTER_PEDALS));
    }

    if (msc_version < 470) {
        set(Sid::DIVIDER_LEFT_ALIGN_TO_SYSTEM_BARLINE, false);
        set(Sid::DIVIDER_RIGHT_ALIGN_TO_SYSTEM_BARLINE, false);

        // Musical symbol size
        compat::CompatUtils::set_music_symbol_size470(*this);

        // Make sure new position styles are initially the same as align values
        // Exclude text styles which had align & position separated in 4.6
        compat::CompatUtils::set_position_styles_from_align(this,
                                                            {Sid::CHORD_SYMBOL_A_ALIGN, Sid::CHORD_SYMBOL_B_ALIGN, Sid::ROMAN_NUMERAL_ALIGN,
                                                             Sid::NASHVILLE_NUMBER_ALIGN, Sid::REPEAT_LEFT_ALIGN, Sid::REPEAT_RIGHT_ALIGN});

        if (value(Sid::CHORD_STYLE).value<ChordStylePreset>() == ChordStylePreset::JAZZ) {
            set(Sid::HARMONY_PAREN_USE_SMUFL_SYM, true);
        }
    }

    if (msc_version < 460) {
        bool vertical_chord_align =
            value(Sid::MAX_CHORD_SHIFT_ABOVE).value<Spatium>() != 0.0_sp || value(Sid::MAX_CHORD_SHIFT_BELOW).value<Spatium>() != 0.0_sp ||
            value(Sid::MAX_FRET_SHIFT_ABOVE).value<Spatium>() != 0.0_sp || value(Sid::MAX_FRET_SHIFT_BELOW).value<Spatium>() != 0.0_sp;
        set(Sid::VERTICALLY_ALIGN_CHORD_SYMBOLS, vertical_chord_align);
        // Make sure new position styles are initially the same as align values
        compat::CompatUtils::set_position_styles_from_align(this);

        if (value(Sid::MEASURE_NUMBER_POSITION).value<AlignH>() == AlignH::HCENTER) {
            set(Sid::MEASURE_NUMBER_H_PLACEMENT, AlignH::HCENTER);
        }

        if (value(Sid::PEDAL_PLACEMENT).value<PlacementV>() == PlacementV::BELOW &&
            value(Sid::PEDAL_HOOK_HEIGHT).value<Spatium>().val() < 0) {
            set(Sid::PEDAL_HOOK_HEIGHT, -value(Sid::PEDAL_HOOK_HEIGHT).value<Spatium>());
        }
        if (value(Sid::OTTAVA_HOOK_BELOW).value<Spatium>().val() < 0) {
            set(Sid::OTTAVA_HOOK_BELOW, -value(Sid::OTTAVA_HOOK_BELOW).value<Spatium>());
        }
        set(Sid::REPEAT_PLAY_COUNT_SHOW, false);

        set(Sid::HARMONY_HARMONY_DISTANCE, value(Sid::MIN_HARMONY_DISTANCE));
    }

    if (msc_version == 450) {
        // 450 spacing was a bit narrower
        set(Sid::SPACING_DENSITY, 1.30);
    }

    if (msc_version < 450) {
        // Didn't exist before 4.5. Default to false for compatibility.
        set(Sid::SCALE_RYTHMIC_SPACING_FOR_SMALL_NOTES, false);
        set(Sid::MASK_BARLINES_FOR_TEXT, false);
        set(Sid::SHOW_COURTESIES_REPEATS, false);
        set(Sid::SHOW_COURTESIES_OTHER_JUMPS, false);
        set(Sid::SHOW_COURTESIES_AFTER_CANCELLING_REPEATS, false);
        set(Sid::SHOW_COURTESIES_AFTER_CANCELLING_OTHER_JUMPS, false);
        set(Sid::CHANGES_BEFORE_BARLINE_REPEATS, false);
        set(Sid::CHANGES_BEFORE_BARLINE_OTHER_JUMPS, false);
    }

    if (msc_version < 420 && !MScore::test_mode) {
        // This style didn't exist before version 4.2. For files older than 4.2, defaults
        // to INSIDE for compatibility. For files 4.2 and newer, defaults to OUTSIDE.
        set(Sid::TIE_PLACEMENT_CHORD, TiePlacement::INSIDE);
    }
}

bool MStyle::read(IODevice *device, [[maybe_unused]] bool ign) {
    XmlReader e(device);
    while (e.read_next_start_element()) {
        if (e.name() == "museScore") {
            const String version_tag = e.attribute("version").remove(u'.');
            const int msc_version = version_tag.to_int();
            while (e.read_next_start_element()) {
                if (e.name() == "Style") {
                    read(e, nullptr, msc_version);
                } else {
                    e.unknown();
                }
            }
        }
    }
    return true;
}

bool MStyle::is_valid(IODevice *device) {
    XmlReader e(device);
    while (!e.is_error() && e.read_next_start_element()) {
        if (e.name() == "museScore") {
            while (e.read_next_start_element()) {
                if (e.name() == "Style") {
                    return true;
                }
            }
        }
    }
    return false;
}

void MStyle::read(XmlReader &e, compat::ReadChordListHook *read_chord_list_hook, int msc_version) {
    using namespace std::string_view_literals;

    while (e.read_next_start_element()) {
        const std::string_view tag(e.name());

        if (tag == "TextStyle"sv) {
            //readTextStyle206(this, e);        // obsolete
            e.read_text();
        } else if (tag == "ottavaHook"sv) { // obsolete, for 3.0dev bw. compatibility, should be removed in final release
            double y = std::abs(e.read_double());
            set(Sid::OTTAVA_HOOK_ABOVE, y);
            set(Sid::OTTAVA_HOOK_BELOW, -y);
        } else if (tag == "Spatium"sv || tag == "spatium"sv) {
            set(Sid::SPATIUM, e.read_double() * ::mu::engraving::dpmm);
        } else if (tag == "page-layout"sv) { // obsolete
            compat::read_page_format206(this, e);
        } else if (tag == "displayInConcertPitch"sv) {
            set(Sid::CONCERT_PITCH, e.read_bool());
        } else if (tag == "ChordList"sv) {
            if (read_chord_list_hook) {
                read_chord_list_hook->read(e);
            }
        } else if (tag == "lyricsDashMaxLegth"sv) { // pre-3.6 typo
            set(Sid::LYRICS_DASH_MAX_LENGTH, Spatium(e.read_double()));
        } else if (tag == "dontHidStavesInFirstSystm"sv) { // pre-3.6.3/4.0 typo
            set(Sid::DONT_HIDE_STAVES_IN_FIRST_SYSTEM, e.read_bool());
        } else if (tag == "minSpreadSpread"sv) { // pre-4.4 typo
            set(Sid::MIN_STAFF_SPREAD, Spatium(e.read_double()));
        } else if (tag == "maxSpreadSpread"sv) { // pre-4.4 typo
            set(Sid::MAX_STAFF_SPREAD, Spatium(e.read_double()));
        } else if (tag == "beamDistance"sv) { // beamDistance maps to useWideBeams in 4.0
            set(Sid::USE_WIDE_BEAMS, e.read_double() > 0.75);
        } else if (tag == "hairpinWidth"sv) { // pre-4.4 typo
            set(Sid::HAIRPIN_LINE_WIDTH, Spatium(e.read_double()));
        } else if (tag == "chordSymbolPosAbove"sv) { // pre-4.4 typo
            set(Sid::CHORD_SYMBOL_A_POS_ABOVE, e.read_point());
        } else if (tag == "chordSymbolPosBelow"sv) { // pre-4.4 typo
            set(Sid::CHORD_SYMBOL_A_POS_BELOW, e.read_point());
        } else if (tag == "dontHidStavesInFirstSystm"sv) { // pre-3.6.3/4.0 typo
            set(Sid::DONT_HIDE_STAVES_IN_FIRST_SYSTEM, e.read_bool());
        } else if (tag == "firstSystemInsNameVisibility"sv) { // pre-4.4 typo
            set(Sid::FIRST_SYSTEM_INST_NAME_VISIBILITY, e.read_int());
        } else if ((tag == "articulationMinDistance"sv || tag == "propertyDistanceHead"sv || tag == "propertyDistanceStem"sv ||
                    tag == "propertyDistance"sv || tag == "bracketDistance"sv) &&
                   msc_version < 400) {
            // Ignoring pre-4.0 articulation style and brackets distance settings. Using the new defaults instead
            e.skip_current_element();
        } else if (tag == "pedalListStyle"sv) { // pre-3.6.3/4.0 typo
            set(Sid::PEDAL_LINE_STYLE, TConv::from_xml(e.read_ascii_text(), LineType::SOLID));
        } else if (tag == "chordlineThickness"sv && msc_version < 410) {
            // Ignoring pre-4.1 value as it was wrong (it wasn't user-editable anyway)
            e.skip_current_element();
        } else if (tag == "pedalText"sv && msc_version < 420) {
            // Ignore old default
            String ped_text = e.read_text();
            if (!ped_text.empty()) {
                set(Sid::PEDAL_TEXT, ped_text);
            }
        } else if (tag == "pedalContinueText"sv && msc_version < 420) {
            // Ignore old default
            String ped_cont_text = e.read_text();
            if (!ped_cont_text.empty()) {
                set(Sid::PEDAL_TEXT, ped_cont_text);
            }
        } else if (tag == "ArpeggioNoteDistance"sv) { // pre-4.4 typo
            set(Sid::ARPEGGIO_NOTE_DISTANCE, Spatium(e.read_double()));
        } else if (tag == "ArpeggioAccidentalDistance"sv) { // pre-4.4 typo
            set(Sid::ARPEGGIO_ACCIDENTAL_DISTANCE, Spatium(e.read_double()));
        } else if (tag == "ArpeggioAccidentalDistanceMin"sv) { // pre-4.4 typo
            set(Sid::ARPEGGIO_ACCIDENTAL_DISTANCE_MIN, Spatium(e.read_double()));
        } else if (tag == "ArpeggioLineWidth"sv) { // pre-x.4 typo
            set(Sid::ARPEGGIO_LINE_WIDTH, Spatium(e.read_double()));
        } else if (tag == "ArpeggioHookLen"sv) { // pre-x.4 typo
            set(Sid::ARPEGGIO_HOOK_LEN, Spatium(e.read_double()));
        } else if (tag == "ArpeggioHiddenInStdIfTab"sv) { // pre-x.4 typo
            set(Sid::ARPEGGIO_HIDDEN_IN_STD_IF_TAB, e.read_bool());
        } else if ((tag == "slurEndWidth"sv || tag == "slurMidWidth"sv || tag == "slurDottedWidth"sv || tag == "slurMinDistance"sv) &&
                   msc_version < 430) {
            // Pre-4.3 scores used identical style values for slurs and ties.
            // When opening older scores, use the same values for both.
            double val_value = e.read_double();
            if (tag == "slurEndWidth"sv) {
                set(Sid::TIE_END_WIDTH, Spatium(val_value));
                set(Sid::SLUR_END_WIDTH, Spatium(val_value));
            } else if (tag == "slurMidWidth"sv) {
                set(Sid::TIE_MID_WIDTH, Spatium(val_value));
                set(Sid::SLUR_MID_WIDTH, Spatium(val_value));
            } else if (tag == "slurDottedWidth"sv) {
                set(Sid::TIE_DOTTED_WIDTH, Spatium(val_value));
                set(Sid::SLUR_DOTTED_WIDTH, Spatium(val_value));
            } else if (tag == "slurMinDistance"sv) {
                set(Sid::TIE_MIN_DISTANCE, Spatium(val_value));
                set(Sid::SLUR_MIN_DISTANCE, Spatium(val_value));
            }
        } else if (tag == "measureNumberOffset"sv && msc_version < 440) { // pre-4.4 typo
            set(Sid::MEASURE_NUMBER_POS_ABOVE, PointF(e.read_point()));
        } else if (tag == "measureNumberPosAbove"sv && msc_version < 440) { // pre-4.4 typo
            set(Sid::MM_REST_RANGE_POS_ABOVE, PointF(e.read_point()));
        } else if (tag == "ottavaTextAlign"sv) {
            // Pre-x.x (?) scores used identical style values for Above and Below
            // apparently the old default was "VCENTER",
            // so better ignore and take the new defaults
            e.skip_current_element();               // obsolete
        } else if (tag == "tremoloStrokeStyle"sv) { // pre-4.4 typo
            set(Sid::TREMOLO_STYLE, e.read_int());
        } else if (tag == "systemFontFace"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FONT_FACE, e.read_text());
        } else if (tag == "systemFontSize"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FONT_SIZE, e.read_double());
        } else if (tag == "systemFontSpatiumDependent"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FONT_SPATIUM_DEPENDENT, bool(e.read_int()));
        } else if (tag == "systemFontStyle"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FONT_STYLE, e.read_int());
        } else if (tag == "systemAlign"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_ALIGN, TConv::from_xml(e.read_text(), Align()));
        } else if (tag == "systemOffsetType"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_OFFSET_TYPE, e.read_int());
        } else if (tag == "systemPlacement"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_PLACEMENT, PlacementV(e.read_text().to_int()));
        } else if (tag == "systemPosAbove"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_POS_ABOVE, PointF(e.read_point()));
        } else if (tag == "systemPosBelow"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_POS_BELOW, PointF(e.read_point()));
        } else if (tag == "systemMinDistance"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_MIN_DISTANCE, Spatium(e.read_double()));
        } else if (tag == "systemFrameType"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FRAME_TYPE, e.read_int());
        } else if (tag == "systemFramePadding"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FRAME_PADDING, e.read_double());
        } else if (tag == "systemFrameWidth"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FRAME_WIDTH, e.read_double());
        } else if (tag == "systemFrameRound"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FRAME_ROUND, e.read_double());
        } else if (tag == "systemFrameFgColor"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FRAME_FG_COLOR, e.read_color());
        } else if (tag == "systemFrameBgColor"sv) { // pre-4.4 typo
            set(Sid::SYSTEM_TEXT_FRAME_BG_COLOR, e.read_color());
        } else if (tag == "staffFontFace"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FONT_FACE, e.read_text());
        } else if (tag == "staffFontSize"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FONT_SIZE, e.read_double());
        } else if (tag == "staffFontSpatiumDependent"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FONT_SPATIUM_DEPENDENT, e.read_bool());
        } else if (tag == "staffFontStyle"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FONT_STYLE, e.read_int());
        } else if (tag == "staffAlign"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_ALIGN, TConv::from_xml(e.read_text(), Align()));
        } else if (tag == "staffOffsetType"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_OFFSET_TYPE, e.read_int());
        } else if (tag == "staffPlacement"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_PLACEMENT, PlacementV(e.read_text().to_int()));
        } else if (tag == "staffTextPosAbove"sv // pre-4.4 typo, certainly before 3.6, even before 3.5
                   && msc_version <= 410) {     // so we might test for < 302, however version_ seems set to 410 here?!?
            double staff_text_pos_above_y = e.read_double();
            set(Sid::STAFF_TEXT_POS_ABOVE, PointF(0.0, staff_text_pos_above_y));
        } else if (tag == "staffPosAbove"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_POS_ABOVE, e.read_point());
        } else if (tag == "staffPosBelow"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_POS_BELOW, e.read_point());
        } else if (tag == "staffTextMinDistance"sv // pre-4.4 typo, certainly before 3.6, even before 3.5
                   && msc_version <= 410) {        // so we might test for < 302, however version_ seems set to 410 here?!?
            set(Sid::STAFF_TEXT_MIN_DISTANCE, Spatium(e.read_double()));
        } else if (tag == "staffMinDistance"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_MIN_DISTANCE, Spatium(e.read_double()));
        } else if (tag == "staffFrameType"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FRAME_TYPE, e.read_int());
        } else if (tag == "staffFramePadding"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FRAME_PADDING, e.read_double());
        } else if (tag == "staffFrameWidth"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FRAME_WIDTH, e.read_double());
        } else if (tag == "staffFrameRound"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FRAME_ROUND, e.read_double());
        } else if (tag == "staffFrameFgColor"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FRAME_FG_COLOR, e.read_color());
        } else if (tag == "staffFrameBgColor"sv) { // pre-4.4 typo
            set(Sid::STAFF_TEXT_FRAME_BG_COLOR, e.read_color());
        } else if (tag == "dymanicsShowTabCommon"sv) { // pre-4.4 typo in gp-style.mss
            set(Sid::DYNAMICS_SHOW_TAB_COMMON, bool(e.read_int()));
        } else if (tag == "tupletOufOfStaff"sv) {
            set(Sid::TUPLET_OUT_OF_STAFF, bool(e.read_int()));
        } else if (tag == "pedalBeginTextOffset"sv || tag == "letRingBeginTextOffset"sv || tag == "palmMuteBeginTextOffset"sv ||
                   tag == "defaultFontSpatiumDependent"sv || tag == "usePre_3_6_defaults"sv) {
            e.skip_current_element(); // obsolete
        } else if (tag == "articulationAnchorDefault"sv && msc_version < 410) {
            set(Sid::ARTICULATION_ANCHOR_DEFAULT, (int) compat::CompatUtils::translate_to_new_articulation_anchor(e.read_int()));
        } else if (tag == "articulationAnchorLuteFingering"sv && msc_version < 410) {
            set(Sid::ARTICULATION_ANCHOR_LUTE_FINGERING, (int) compat::CompatUtils::translate_to_new_articulation_anchor(e.read_int()));
        } else if (tag == "articulationAnchorOther"sv && msc_version < 410) {
            set(Sid::ARTICULATION_ANCHOR_OTHER, (int) compat::CompatUtils::translate_to_new_articulation_anchor(e.read_int()));
        } else if (tag == "lineEndToSystemEndDistance"sv) { // renamed in 4.5
            set(Sid::LINE_END_TO_BARLINE_DISTANCE, Spatium(e.read_double()));
        } else if (tag == "useStandardNoteNames"sv) { // These settings were collapsed into one enum in 4.6
            if (e.read_bool()) {
                set(Sid::CHORD_SYMBOL_SPELLING, NoteSpellingType::STANDARD);
            }
        } else if (tag == "useGermanNoteNames"sv) {
            if (e.read_bool()) {
                set(Sid::CHORD_SYMBOL_SPELLING, NoteSpellingType::GERMAN);
            }
        } else if (tag == "useFullGermanNoteNames"sv) {
            if (e.read_bool()) {
                set(Sid::CHORD_SYMBOL_SPELLING, NoteSpellingType::GERMAN_PURE);
            }
        } else if (tag == "useSolfeggioNoteNames"sv) {
            if (e.read_bool()) {
                set(Sid::CHORD_SYMBOL_SPELLING, NoteSpellingType::SOLFEGGIO);
            }
        } else if (tag == "useFrenchNoteNames"sv) {
            if (e.read_bool()) {
                set(Sid::CHORD_SYMBOL_SPELLING, NoteSpellingType::FRENCH);
            }
        } else if (tag == "chordModifierAdjust"sv && msc_version < 460) {
            set(Sid::CHORD_MODIFIER_ADJUST, compat::CompatUtils::convert_chord_ext_mod_units(e.read_double()));
        } else if (tag == "chordExtensionAdjust"sv && msc_version < 460) {
            set(Sid::CHORD_EXTENSION_ADJUST, compat::CompatUtils::convert_chord_ext_mod_units(e.read_double()));
        } else if (tag == "chordDescriptionFile"sv && msc_version < 460) {
            AsciiStringView val = e.read_ascii_text();
            if (val == "chords_std.xml") {
                set(Sid::CHORD_DESCRIPTION_FILE, String(u"chords_legacy.xml"));
            } else {
                set(Sid::CHORD_DESCRIPTION_FILE, String::from_ascii(val.ascii()));
            }
        } else if (tag == "chordStyle"sv && msc_version < 460) {
            AsciiStringView val = e.read_ascii_text();
            if (val == "std") {
                set(Sid::CHORD_STYLE, ChordStylePreset::LEGACY);
            } else {
                set(Sid::CHORD_STYLE, TConv::from_xml(val, ChordStylePreset::STANDARD));
            }
        } else if (tag == "fretFrets"sv && msc_version < 460) {
            e.skip_current_element();
        } else if (tag == "measureNumberHPlacement"sv && msc_version < 460) {
            // Before 460 PlacementH was used instead of AlignH, and was written as integer.
            // We can't directly map the integer to AlignH because it's enumerated differently.
            PlacementH h_placement = PlacementH(e.read_int());
            AlignH h_align = h_placement == PlacementH::LEFT   ? AlignH::LEFT :
                             h_placement == PlacementH::CENTER ? AlignH::HCENTER :
                                                                 AlignH::RIGHT;
            set(Sid::MEASURE_NUMBER_H_PLACEMENT, h_align);

            if (value(Sid::MEASURE_NUMBER_H_PLACEMENT).value<AlignH>() != AlignH::LEFT) {
                // In this case it was assumed to be centered on the measure
                set(Sid::MEASURE_NUMBER_ALIGN_TO_BARLINE, false);
            }
        } else if (tag == "mmRestRangeHPlacement"sv && msc_version < 460) {
            // Before 460 PlacementH was used instead of AlignH, and was written as integer.
            // We can't directly map the integer to AlignH because it's enumerated differently.
            PlacementH h_placement = PlacementH(e.read_int());
            AlignH h_align = h_placement == PlacementH::LEFT   ? AlignH::LEFT :
                             h_placement == PlacementH::CENTER ? AlignH::HCENTER :
                                                                 AlignH::RIGHT;
            set(Sid::MM_REST_RANGE_H_PLACEMENT, h_align);
        } else if (tag == "measureNumberAllStaves"sv || tag == "measureNumberAllStaffs"sv /*old typo*/) {
            bool all_staves = e.read_bool();
            set(Sid::MEASURE_NUMBER_PLACEMENT_MODE,
                all_staves ? MeasureNumberPlacement::ON_ALL_STAVES : MeasureNumberPlacement::ABOVE_SYSTEM);
        } else if (String s_tag = String::from_ascii(tag.data()); msc_version < 470 && s_tag.contains(u"FrameRound")) {
            auto i = std::find_if(StyleDef::style_values.begin(), StyleDef::style_values.end(),
                                  [&](const StyleDef::StyleValue &s) { return s.xml_name == tag; });
            if (i != StyleDef::style_values.end()) {
                const StyleDef::StyleValue &s = *i;
                set(s.sid, compat::CompatUtils::convert_pre470_frame_radius(e.read_double()));
            }
        } else if (!read_properties(e)) {
            e.unknown();
        }
    }

    apply_compat_style_vals(msc_version);

    if (read_chord_list_hook) {
        read_chord_list_hook->validate();
    }
}

bool MStyle::write(IODevice *device) {
    XmlWriter xml(device);
    xml.start_document();
    xml.start_element("museScore", {{"version", Constants::msc_version_str}});
    save(xml, false);
    xml.end_element();
    return true;
}

void MStyle::save(XmlWriter &xml, bool optimize) {
    xml.start_element("Style");

    for (const StyleDef::StyleValue &st : StyleDef::style_values) {
        Sid idx = st.sid;
        if (idx == Sid::SPATIUM) { // special handling for spatium
            continue;
        }
        if (optimize && is_default(idx)) {
            continue;
        }
        PType type = st.value_type();
        if (PType::SPATIUM == type) {
            xml.tag(st.xml_name, value(idx).value<Spatium>().val());
        } else if (PType::DIRECTION_V == type) {
            xml.tag(st.xml_name, int(value(idx).value<DirectionV>()));
        } else if (PType::ORIENTATION == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<Orientation>()));
        } else if (PType::SHARED_LABEL_ORIENTATION == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<SharedLabelOrientation>()));
        } else if (PType::ALIGN == type) {
            Align a = value(idx).value<Align>();
            // Don't write if it's the default value
            if (optimize && a == st.default_value.value<Align>()) {
                continue;
            }
            xml.tag(st.xml_name, TConv::to_xml(a));
        } else if (PType::ALIGN_H == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<AlignH>()));
        } else if (PType::LINE_TYPE == type) {
            xml.tag_property(st.xml_name, value(idx));
        } else if (PType::TIE_PLACEMENT == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<TiePlacement>()));
        } else if (PType::TIE_DOTS_PLACEMENT == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<TieDotsPlacement>()));
        } else if (PType::TIMESIG_PLACEMENT == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<TimeSigPlacement>()));
        } else if (PType::TIMESIG_STYLE == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<TimeSigStyle>()));
        } else if (PType::TIMESIG_MARGIN == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<TimeSigVSMargin>()));
        } else if (PType::CHORD_PRESET_TYPE == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<ChordStylePreset>()));
        } else if (PType::NOTE_SPELLING_TYPE == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<NoteSpellingType>()));
        } else if (PType::LH_TAPPING_SYMBOL == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<LHTappingSymbol>()));
        } else if (PType::RH_TAPPING_SYMBOL == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<RHTappingSymbol>()));
        } else if (PType::TEXT_STYLE == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<TextStyleType>()));
        } else if (PType::PARENTHESES_MODE == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<ParenthesesMode>()));
        } else if (PType::PLAY_COUNT_PRESET == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<RepeatPlayCountPreset>()));
        } else if (PType::MEASURE_NUMBER_PLACEMENT == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<MeasureNumberPlacement>()));
        } else if (PType::INSTRUMENT_NAMES_ALIGN == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<InstrumentNamesAlign>()));
        } else if (PType::INSTRUMENT_NAMES_FORMAT == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<InstrumentNamesFormat>()));
        } else if (PType::AUTO_ON_OFF == type) {
            xml.tag(st.xml_name, TConv::to_xml(value(idx).value<AutoOnOff>()));
        } else {
            PropertyValue val = value(idx);
            //! NOTE for compatibility
            if (val.is_enum()) {
                val = val.value<int>();
            }
            xml.tag_property(st.xml_name, val);
        }
    }

    xml.tag("spatium", value(Sid::SPATIUM).to_real() / ::mu::engraving::dpmm);
    xml.end_element();
}

// ====================================================
// Static
// ====================================================

PType MStyle::value_type(const Sid i) { return StyleDef::style_values[size_t(i)].value_type(); }

const char *MStyle::value_name(const Sid i) {
    if (i == Sid::NOSTYLE) {
        static const char *no_style = "no style";
        return no_style;
    }
    return StyleDef::style_values[size_t(i)].xml_name.ascii();
}

Sid MStyle::style_idx(const String &name) {
    muse::ByteArray ba = name.to_ascii();
    for (const StyleDef::StyleValue &st : StyleDef::style_values) {
        if (st.xml_name == ba.const_char()) {
            return st.sid;
        }
    }
    return Sid::NOSTYLE;
}
