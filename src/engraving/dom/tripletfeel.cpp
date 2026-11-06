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

#include "tripletfeel.h"

#include "score.h"
#include "types/string.h"
#include "types/translatablestring.h"

namespace mu::engraving {
static const ElementStyle triplet_feel_style{{Sid::SUB_TITLE_FONT_SIZE, Pid::FONT_SIZE}, // TODO: add styles
                                             {Sid::TEMPO_FONT_STYLE, Pid::FONT_STYLE}};

static const std::map<TripletFeelType, String> tuplet_symbols = {
    {TripletFeelType::TRIPLET_8TH, String(u"%1textBlackNoteShortStem%2"
                                          "%1textCont8thBeamShortStem%2"
                                          "%1textBlackNoteFrac8thShortStem%2"
                                          " = "
                                          "%1textBlackNoteShortStem%2"
                                          "%1textTupletBracketStartShortStem%2"
                                          "%1textTuplet3ShortStem%2"
                                          "%1note8thUp%2"
                                          "%1textTupletBracketEndShortStem%2")},

    {TripletFeelType::TRIPLET_16TH, String(u"%1textBlackNoteShortStem%2"
                                           "%1textCont16thBeamShortStem%2"
                                           "%1textBlackNoteFrac16thShortStem%2"
                                           " = "
                                           "%1textBlackNoteShortStem%2"
                                           "%1textTupletBracketStartShortStem%2"
                                           "%1textCont8thBeamShortStem%2"
                                           "%1textTuplet3ShortStem%2"
                                           "%1textBlackNoteFrac16thShortStem%2"
                                           "%1textTupletBracketEndShortStem%2")},

    {TripletFeelType::DOTTED_8TH, String(u"%1textBlackNoteShortStem%2"
                                         "%1textCont8thBeamShortStem%2"
                                         "%1textBlackNoteFrac8thShortStem%2"
                                         " = "
                                         "%1textBlackNoteShortStem%2"
                                         "%1textCont8thBeamShortStem%2"
                                         "%1textAugmentationDot%2"
                                         "%1textCont8thBeamShortStem%2"
                                         "%1textBlackNoteFrac16thShortStem%2")},

    {TripletFeelType::DOTTED_16TH, String(u"%1textBlackNoteShortStem%2"
                                          "%1textCont16thBeamShortStem%2"
                                          "%1textBlackNoteFrac16thShortStem%2"
                                          " = "
                                          "%1textBlackNoteLongStem%2"
                                          "%1textCont16thBeamLongStem%2"
                                          "%1textAugmentationDot%2"
                                          "%1textCont16thBeamLongStem%2"
                                          "%1textBlackNoteFrac32ndLongStem%2")},

    {TripletFeelType::SCOTTISH_8TH, String(u"%1textBlackNoteShortStem%2"
                                           "%1textCont8thBeamShortStem%2"
                                           "%1textBlackNoteFrac8thShortStem%2"
                                           " = "
                                           "%1textBlackNoteShortStem%2"
                                           "%1textCont16thBeamShortStem%2"
                                           "%1textBlackNoteFrac8thShortStem%2"
                                           "%1textAugmentationDot%2")},

    {TripletFeelType::SCOTTISH_16TH, String(u"%1textBlackNoteShortStem%2"
                                            "%1textCont16thBeamShortStem%2"
                                            "%1textBlackNoteFrac16thShortStem%2"
                                            " = "
                                            "%1textBlackNoteLongStem%2"
                                            "%1textCont32ndBeamLongStem%2"
                                            "%1textBlackNoteFrac16thLongStem%2"
                                            "%1textAugmentationDot%2")},

    {TripletFeelType::NONE, String(u"%1textBlackNoteShortStem%2"
                                   "%1textCont8thBeamShortStem%2"
                                   "%1textBlackNoteFrac8thShortStem%2"
                                   " = "
                                   "%1textBlackNoteShortStem%2"
                                   "%1textCont8thBeamShortStem%2"
                                   "%1textBlackNoteFrac8thShortStem%2")}};

static const std::map<TripletFeelType, TranslatableString> triplet_feel_names = {
    {TripletFeelType::TRIPLET_8TH, TranslatableString("engraving/tripletfeel", "Triplet 8th")},
    {TripletFeelType::TRIPLET_16TH, TranslatableString("engraving/tripletfeel", "Triplet 16th")},
    {TripletFeelType::DOTTED_8TH, TranslatableString("engraving/tripletfeel", "Dotted 8th")},
    {TripletFeelType::DOTTED_16TH, TranslatableString("engraving/tripletfeel", "Dotted 16th")},
    {TripletFeelType::SCOTTISH_8TH, TranslatableString("engraving/tripletfeel", "Scottish 8th")},
    {TripletFeelType::SCOTTISH_16TH, TranslatableString("engraving/tripletfeel", "Scottish 16th")},
    {TripletFeelType::NONE, TranslatableString("engraving/tripletfeel", "No triplet feel")}};

TripletFeel::TripletFeel(DummyParentOr<Segment> parent, TripletFeelType triplet_fill_type)
    : SystemText(parent, TextStyleType::SYSTEM, ElementType::TRIPLET_FEEL) {
    triplet_feel_type_ = triplet_fill_type;
    set_triplet_property();
    init_element_style(&::mu::engraving::triplet_feel_style);
}

void TripletFeel::set_triplet_property() {
    set_swing(true);

    switch (triplet_feel_type_) {
        case TripletFeelType::TRIPLET_8TH:
            set_swing_parameters(Constants::division / ::mu::engraving::TripletFeel::eight_division,
                                 ::mu::engraving::TripletFeel::triplet_ratio);
            break;

        case TripletFeelType::TRIPLET_16TH:
            set_swing_parameters(Constants::division / ::mu::engraving::TripletFeel::sixteenth_division,
                                 ::mu::engraving::TripletFeel::triplet_ratio);
            break;

        case TripletFeelType::DOTTED_8TH:
            set_swing_parameters(Constants::division / ::mu::engraving::TripletFeel::eight_division,
                                 ::mu::engraving::TripletFeel::dotted_ratio);
            break;

        case TripletFeelType::DOTTED_16TH:
            set_swing_parameters(Constants::division / ::mu::engraving::TripletFeel::sixteenth_division,
                                 ::mu::engraving::TripletFeel::dotted_ratio);
            break;

        case TripletFeelType::SCOTTISH_8TH:
            set_swing_parameters(Constants::division / ::mu::engraving::TripletFeel::eight_division,
                                 ::mu::engraving::TripletFeel::scottish_ratio);
            break;

        case TripletFeelType::SCOTTISH_16TH:
            set_swing_parameters(Constants::division / ::mu::engraving::TripletFeel::sixteenth_division,
                                 ::mu::engraving::TripletFeel::scottish_ratio);
            break;

        case TripletFeelType::NONE: set_swing_parameters(0, ::mu::engraving::TripletFeel::zero_ratio); break;
    }

    set_xml_text(::mu::engraving::tuplet_symbols.at(triplet_feel_type_).arg(String(u"<sym>staffPosLower3</sym><sym>"), String(u"</sym>")));
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TripletFeel::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::FONT_SIZE: return style().style_v(Sid::SUB_TITLE_FONT_SIZE);

        case Pid::FONT_STYLE: return style().style_v(Sid::TEMPO_FONT_STYLE);

        default: return SystemText::property_default(property_id);
    }
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid TripletFeel::get_property_style(Pid id) const {
    switch (id) {
        case Pid::FONT_SIZE: return Sid::SUB_TITLE_FONT_SIZE;

        case Pid::FONT_STYLE: return Sid::TEMPO_FONT_STYLE;
        default: break;
    }

    return SystemText::get_property_style(id);
}

//---------------------------------------------------------
//   typeUserName
//---------------------------------------------------------

TranslatableString TripletFeel::subtype_user_name() const { return ::mu::engraving::triplet_feel_names.at(triplet_feel_type_); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

} // namespace mu::engraving
