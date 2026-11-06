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

#include "symbolsmetaparser.h"

#include "dom/articulation.h"
#include "log.h"

using namespace mu::engraving;
using namespace muse;

static mpe::ArticulationType text_type_to_articulation_type(ArticulationTextType text_type) {
    switch (text_type) {
        case ArticulationTextType::NO_TEXT: return mpe::ArticulationType::STANDARD;
        case ArticulationTextType::SLAP: return mpe::ArticulationType::SLAP;
        case ArticulationTextType::POP: return mpe::ArticulationType::POP;
        case ArticulationTextType::TD: return mpe::ArticulationType::THUMB_DAMP;
        case ArticulationTextType::BD: return mpe::ArticulationType::BRUSH_DAMP;
        case ArticulationTextType::RT: return mpe::ArticulationType::RING_TOUCH;
        case ArticulationTextType::PL: return mpe::ArticulationType::PLUCK;
        case ArticulationTextType::SB: return mpe::ArticulationType::SINGING_BELL;
        case ArticulationTextType::VIB: return mpe::ArticulationType::SINGING_VIBRATE;
    }

    UNREACHABLE;
    return mpe::ArticulationType::UNDEFINED;
}

mpe::ArticulationTypeSet SymbolsMetaParser::symbol_to_articulations(SymId sym_id, OrnamentStyle ornament_style) {
    mpe::ArticulationTypeSet types;

    switch (sym_id) {
        case SymId::ARTIC_ACCENT_ABOVE:
        case SymId::ARTIC_ACCENT_BELOW: types.emplace(mpe::ArticulationType::ACCENT); break;
        case SymId::ARTIC_ACCENT_STACCATO_ABOVE:
        case SymId::ARTIC_ACCENT_STACCATO_BELOW:
            types.emplace(mpe::ArticulationType::ACCENT);
            types.emplace(mpe::ArticulationType::STACCATO);
            break;
        case SymId::ARTIC_LAISSEZ_VIBRER_ABOVE:
        case SymId::ARTIC_LAISSEZ_VIBRER_BELOW: types.emplace(mpe::ArticulationType::LAISSEZ_VIBRER); break;
        case SymId::ARTIC_MARCATO_ABOVE:
        case SymId::ARTIC_MARCATO_BELOW: types.emplace(mpe::ArticulationType::MARCATO); break;
        case SymId::ARTIC_MARCATO_STACCATO_ABOVE:
        case SymId::ARTIC_MARCATO_STACCATO_BELOW:
            types.emplace(mpe::ArticulationType::MARCATO);
            types.emplace(mpe::ArticulationType::STACCATO);
            break;
        case SymId::ARTIC_MARCATO_TENUTO_ABOVE:
        case SymId::ARTIC_MARCATO_TENUTO_BELOW:
            types.emplace(mpe::ArticulationType::MARCATO);
            types.emplace(mpe::ArticulationType::TENUTO);
            break;
        case SymId::ARTIC_SOFT_ACCENT_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_BELOW: types.emplace(mpe::ArticulationType::SOFT_ACCENT); break;
        case SymId::ARTIC_SOFT_ACCENT_STACCATO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_STACCATO_BELOW:
            types.emplace(mpe::ArticulationType::SOFT_ACCENT);
            types.emplace(mpe::ArticulationType::STACCATO);
            break;
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_BELOW:
            types.emplace(mpe::ArticulationType::SOFT_ACCENT);
            types.emplace(mpe::ArticulationType::TENUTO);
            break;
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_STACCATO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_STACCATO_BELOW:
            types.emplace(mpe::ArticulationType::SOFT_ACCENT);
            types.emplace(mpe::ArticulationType::TENUTO);
            types.emplace(mpe::ArticulationType::STACCATO);
            break;
        case SymId::ARTIC_STACCATISSIMO_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_BELOW:
        case SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_STROKE_BELOW:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_BELOW: types.emplace(mpe::ArticulationType::STACCATISSIMO); break;
        case SymId::ARTIC_STACCATO_ABOVE:
        case SymId::ARTIC_STACCATO_BELOW: types.emplace(mpe::ArticulationType::STACCATO); break;
        case SymId::ARTIC_TENUTO_ABOVE:
        case SymId::ARTIC_TENUTO_BELOW: types.emplace(mpe::ArticulationType::TENUTO); break;
        case SymId::ARTIC_TENUTO_STACCATO_ABOVE:
        case SymId::ARTIC_TENUTO_STACCATO_BELOW:
            types.emplace(mpe::ArticulationType::TENUTO);
            types.emplace(mpe::ArticulationType::STACCATO);
            break;
        case SymId::ARTIC_TENUTO_ACCENT_ABOVE:
        case SymId::ARTIC_TENUTO_ACCENT_BELOW:
            types.emplace(mpe::ArticulationType::TENUTO);
            types.emplace(mpe::ArticulationType::ACCENT);
            break;
        case SymId::DYNAMIC_SFORZANDO:
        case SymId::DYNAMIC_SFORZANDO1:
        case SymId::DYNAMIC_SFORZANDO_PIANISSIMO:
        case SymId::DYNAMIC_SFORZANDO_PIANO:
        case SymId::DYNAMIC_SFORZATO:
        case SymId::DYNAMIC_SFORZATO_FF:
        case SymId::DYNAMIC_SFORZATO_PIANO: types.emplace(mpe::ArticulationType::SUBITO); break;
        case SymId::GUITAR_FADE_IN: types.emplace(mpe::ArticulationType::FADE_IN); break;
        case SymId::GUITAR_VOLUME_SWELL: types.emplace(mpe::ArticulationType::VOLUME_SWELL); break;
        case SymId::GUITAR_FADE_OUT: types.emplace(mpe::ArticulationType::FADE_OUT); break;
        case SymId::STRINGS_HALF_HARMONIC:
        case SymId::STRINGS_HARMONIC: types.emplace(mpe::ArticulationType::HARMONIC); break;
        case SymId::STRINGS_MUTE_ON:
        case SymId::ELEC_MUTE:
        case SymId::BRASS_MUTE_HALF_CLOSED:
        case SymId::BRASS_MUTE_CLOSED:
        case SymId::BRASS_HARMON_MUTE_STEM_HALF_RIGHT:
        case SymId::BRASS_HARMON_MUTE_STEM_HALF_LEFT:
        case SymId::BRASS_HARMON_MUTE_CLOSED: types.emplace(mpe::ArticulationType::MUTE); break;
        case SymId::BRASS_HARMON_MUTE_STEM_OPEN:
        case SymId::BRASS_MUTE_OPEN:
        case SymId::GUITAR_HALF_OPEN_PEDAL:
        case SymId::GUITAR_OPEN_PEDAL:
        case SymId::VOCAL_MOUTH_OPEN:
        case SymId::VOCAL_MOUTH_SLIGHTLY_OPEN:
        case SymId::VOCAL_MOUTH_WIDE_OPEN:
        case SymId::WIND_OPEN_HOLE:
        case SymId::ELEC_UNMUTE:
        case SymId::STRINGS_MUTE_OFF: types.emplace(mpe::ArticulationType::OPEN); break;
        case SymId::PLUCKED_LEFT_HAND_PIZZICATO: types.emplace(mpe::ArticulationType::PIZZICATO); break;
        case SymId::PLUCKED_SNAP_PIZZICATO_ABOVE:
        case SymId::PLUCKED_SNAP_PIZZICATO_BELOW: types.emplace(mpe::ArticulationType::SNAP_PIZZICATO); break;
        case SymId::STRINGS_UP_BOW: types.emplace(mpe::ArticulationType::UP_BOW); break;
        case SymId::STRINGS_DOWN_BOW: types.emplace(mpe::ArticulationType::DOWN_BOW); break;
        case SymId::STRINGS_JETE_ABOVE:
        case SymId::STRINGS_JETE_BELOW: types.emplace(mpe::ArticulationType::JETE); break;
        case SymId::NOTEHEAD_X_WHOLE:
        case SymId::NOTEHEAD_X_BLACK:
        case SymId::NOTEHEAD_X_DOUBLE_WHOLE:
        case SymId::NOTEHEAD_WHOLE_WITH_X:
        case SymId::NOTEHEAD_VOID_WITH_X: types.emplace(mpe::ArticulationType::CROSS_NOTE); break;
        case SymId::NOTEHEAD_SLASH_X: types.emplace(mpe::ArticulationType::CROSS_LARGE_NOTE); break;
        case SymId::NOTEHEAD_X_ORNATE:
        case SymId::NOTEHEAD_X_ORNATE_ELLIPSE: types.emplace(mpe::ArticulationType::CROSS_ORNATE_NOTE); break;
        case SymId::NOTEHEAD_CIRCLE_SLASH:
        case SymId::NOTEHEAD_CIRCLED_BLACK:
        case SymId::NOTEHEAD_CIRCLED_HALF:
        case SymId::NOTEHEAD_CIRCLED_BLACK_LARGE:
        case SymId::NOTEHEAD_CIRCLED_DOUBLE_WHOLE:
        case SymId::NOTEHEAD_CIRCLED_DOUBLE_WHOLE_LARGE:
        case SymId::NOTEHEAD_CIRCLED_HALF_LARGE:
        case SymId::NOTEHEAD_CIRCLED_WHOLE:
        case SymId::NOTEHEAD_CIRCLED_WHOLE_LARGE: types.emplace(mpe::ArticulationType::CIRCLE_NOTE); break;
        case SymId::NOTEHEAD_CIRCLE_X:
        case SymId::NOTEHEAD_CIRCLE_X_DOUBLE_WHOLE:
        case SymId::NOTEHEAD_CIRCLE_X_HALF:
        case SymId::NOTEHEAD_CIRCLE_X_WHOLE: types.emplace(mpe::ArticulationType::CIRCLE_CROSS_NOTE); break;
        case SymId::NOTEHEAD_ROUND_WHITE_WITH_DOT:
        case SymId::NOTEHEAD_ROUND_WHITE_WITH_DOT_LARGE: types.emplace(mpe::ArticulationType::CIRCLE_DOT_NOTE); break;
        case SymId::NOTEHEAD_DIAMOND_BLACK:
        case SymId::NOTEHEAD_DIAMOND_BLACK_OLD:
        case SymId::NOTEHEAD_DIAMOND_BLACK_WIDE:
        case SymId::NOTEHEAD_DIAMOND_DOUBLE_WHOLE:
        case SymId::NOTEHEAD_DIAMOND_HALF_WIDE:
        case SymId::NOTEHEAD_DIAMOND_DOUBLE_WHOLE_OLD:
        case SymId::NOTEHEAD_DIAMOND_WHOLE:
        case SymId::NOTEHEAD_DIAMOND_WHOLE_OLD:
        case SymId::NOTEHEAD_DIAMOND_HALF:
        case SymId::NOTEHEAD_DIAMOND_HALF_FILLED:
        case SymId::NOTEHEAD_DIAMOND_HALF_OLD: types.emplace(mpe::ArticulationType::DIAMOND_NOTE); break;
        case SymId::NOTE_SHAPE_MOON_BLACK:
        case SymId::NOTE_SHAPE_MOON_DOUBLE_WHOLE:
        case SymId::NOTE_SHAPE_MOON_WHITE: types.emplace(mpe::ArticulationType::MOON_NOTE); break;
        case SymId::NOTEHEAD_TRIANGLE_LEFT_BLACK:
        case SymId::NOTEHEAD_TRIANGLE_LEFT_WHITE: types.emplace(mpe::ArticulationType::TRIANGLE_LEFT_NOTE); break;
        case SymId::NOTEHEAD_TRIANGLE_RIGHT_BLACK:
        case SymId::NOTEHEAD_TRIANGLE_RIGHT_WHITE:
        case SymId::NOTE_SHAPE_TRIANGLE_RIGHT_WHITE:
        case SymId::NOTE_SHAPE_TRIANGLE_RIGHT_BLACK:
        case SymId::NOTE_SHAPE_TRIANGLE_RIGHT_DOUBLE_WHOLE: types.emplace(mpe::ArticulationType::TRIANGLE_RIGHT_NOTE); break;
        case SymId::NOTEHEAD_TRIANGLE_DOWN_BLACK:
        case SymId::NOTEHEAD_TRIANGLE_DOWN_DOUBLE_WHOLE:
        case SymId::NOTEHEAD_TRIANGLE_DOWN_HALF:
        case SymId::NOTEHEAD_TRIANGLE_DOWN_WHITE:
        case SymId::NOTEHEAD_TRIANGLE_DOWN_WHOLE: types.emplace(mpe::ArticulationType::TRIANGLE_DOWN_NOTE); break;
        case SymId::NOTEHEAD_TRIANGLE_UP_BLACK:
        case SymId::NOTEHEAD_TRIANGLE_UP_DOUBLE_WHOLE:
        case SymId::NOTEHEAD_TRIANGLE_UP_HALF:
        case SymId::NOTEHEAD_TRIANGLE_UP_RIGHT_BLACK:
        case SymId::NOTEHEAD_TRIANGLE_UP_RIGHT_WHITE:
        case SymId::NOTEHEAD_TRIANGLE_UP_WHITE:
        case SymId::NOTEHEAD_TRIANGLE_UP_WHOLE:
        case SymId::NOTE_SHAPE_TRIANGLE_UP_BLACK:
        case SymId::NOTE_SHAPE_TRIANGLE_UP_WHITE:
        case SymId::NOTE_SHAPE_TRIANGLE_UP_DOUBLE_WHOLE: types.emplace(mpe::ArticulationType::TRIANGLE_UP_NOTE); break;
        case SymId::NOTEHEAD_TRIANGLE_ROUND_DOWN_BLACK:
        case SymId::NOTEHEAD_TRIANGLE_ROUND_DOWN_WHITE:
        case SymId::NOTE_SHAPE_TRIANGLE_ROUND_BLACK:
        case SymId::NOTE_SHAPE_TRIANGLE_ROUND_WHITE:
        case SymId::NOTE_SHAPE_TRIANGLE_ROUND_DOUBLE_WHOLE: types.emplace(mpe::ArticulationType::TRIANGLE_ROUND_DOWN_NOTE); break;
        case SymId::NOTEHEAD_PLUS_BLACK:
        case SymId::NOTEHEAD_PLUS_DOUBLE_WHOLE:
        case SymId::NOTEHEAD_PLUS_HALF:
        case SymId::NOTEHEAD_PLUS_WHOLE: types.emplace(mpe::ArticulationType::PLUS_NOTE); break;
        case SymId::NOTEHEAD_SLASH_WHITE_WHOLE:
        case SymId::NOTEHEAD_SLASH_WHITE_HALF:
        case SymId::NOTEHEAD_SLASH_HORIZONTAL_ENDS:
        case SymId::NOTEHEAD_SLASH_WHITE_DOUBLE_WHOLE: types.emplace(mpe::ArticulationType::SLASH_NOTE); break;
        case SymId::NOTE_SHAPE_SQUARE_BLACK:
        case SymId::NOTE_SHAPE_SQUARE_WHITE:
        case SymId::NOTE_SHAPE_SQUARE_DOUBLE_WHOLE: types.emplace(mpe::ArticulationType::SQUARE_NOTE); break;
        case SymId::NOTEHEAD_SLASHED_WHOLE1:
        case SymId::NOTEHEAD_SLASHED_HALF1:
        case SymId::NOTEHEAD_SLASHED_BLACK1:
        case SymId::NOTEHEAD_SLASHED_DOUBLE_WHOLE1: types.emplace(mpe::ArticulationType::SLASHED_FORWARDS_NOTE); break;
        case SymId::NOTEHEAD_SLASHED_WHOLE2:
        case SymId::NOTEHEAD_SLASHED_HALF2:
        case SymId::NOTEHEAD_SLASHED_BLACK2:
        case SymId::NOTEHEAD_SLASHED_DOUBLE_WHOLE2: types.emplace(mpe::ArticulationType::SLASHED_BACKWARDS_NOTE); break;
        case SymId::BRASS_SCOOP: types.emplace(mpe::ArticulationType::SCOOP); break;
        case SymId::BRASS_PLOP: types.emplace(mpe::ArticulationType::PLOP); break;
        case SymId::BRASS_FALL_LIP_LONG:
        case SymId::BRASS_FALL_LIP_MEDIUM:
        case SymId::BRASS_FALL_LIP_SHORT:
        case SymId::BRASS_FALL_ROUGH_LONG:
        case SymId::BRASS_FALL_ROUGH_MEDIUM:
        case SymId::BRASS_FALL_ROUGH_SHORT: types.emplace(mpe::ArticulationType::QUICK_FALL); break;
        case SymId::BRASS_FALL_SMOOTH_LONG:
        case SymId::BRASS_FALL_SMOOTH_MEDIUM:
        case SymId::BRASS_FALL_SMOOTH_SHORT: types.emplace(mpe::ArticulationType::FALL); break;
        case SymId::BRASS_DOIT_LONG:
        case SymId::BRASS_DOIT_MEDIUM:
        case SymId::BRASS_DOIT_SHORT: types.emplace(mpe::ArticulationType::DOIT); break;
        case SymId::BRASS_BEND: types.emplace(mpe::ArticulationType::BRASS_BEND); break;
        case SymId::DYNAMIC_CRESCENDO_HAIRPIN: types.emplace(mpe::ArticulationType::CRESCENDO); break;
        case SymId::DYNAMIC_DIMINUENDO_HAIRPIN: types.emplace(mpe::ArticulationType::DIMINUENDO); break;
        case SymId::ORNAMENT_UP_PRALL: types.emplace(mpe::ArticulationType::UP_PRALL); break;
        case SymId::ORNAMENT_PRALL_DOWN: types.emplace(mpe::ArticulationType::PRALL_DOWN); break;
        case SymId::ORNAMENT_PRALL_UP: types.emplace(mpe::ArticulationType::PRALL_UP); break;
        case SymId::ORNAMENT_LINE_PRALL: types.emplace(mpe::ArticulationType::LINE_PRALL); break;
        case SymId::ORNAMENT_PRALL_MORDENT: types.emplace(mpe::ArticulationType::PRALL_MORDENT); break;
        case SymId::ORNAMENT_UP_MORDENT: types.emplace(mpe::ArticulationType::UP_MORDENT); break;
        case SymId::ORNAMENT_MORDENT:
        case SymId::ORNAMENT_PINCE_COUPERIN: types.emplace(mpe::ArticulationType::LOWER_MORDENT); break;
        case SymId::ORNAMENT_DOWN_MORDENT: types.emplace(mpe::ArticulationType::DOWN_MORDENT); break;
        case SymId::ORNAMENT_TURN:
        case SymId::ORNAMENT_TURN_UP:
        case SymId::ORNAMENT_HAYDN:
        case SymId::BRASS_JAZZ_TURN: types.emplace(mpe::ArticulationType::TURN); break;
        case SymId::ORNAMENT_TURN_INVERTED:
        case SymId::ORNAMENT_TURN_UP_S:
        case SymId::ORNAMENT_TURN_SLASH: types.emplace(mpe::ArticulationType::INVERTED_TURN); break;
        case SymId::ORNAMENT_TRILL:
        case SymId::ORNAMENT_SHAKE3:
        case SymId::ORNAMENT_SHAKE_MUFFAT1:
            if (ornament_style == OrnamentStyle::DEFAULT) {
                types.emplace(mpe::ArticulationType::TRILL);
            } else {
                types.emplace(mpe::ArticulationType::TRILL_BAROQUE);
            }
            break;
        case SymId::ORNAMENT_SHORT_TRILL:
            if (ornament_style == OrnamentStyle::DEFAULT) {
                types.emplace(mpe::ArticulationType::UPPER_MORDENT);
            } else {
                types.emplace(mpe::ArticulationType::UPPER_MORDENT_BAROQUE);
            }
            break;
        case SymId::ORNAMENT_TREMBLEMENT:
        case SymId::ORNAMENT_TREMBLEMENT_COUPERIN: types.emplace(mpe::ArticulationType::TREMBLEMENT); break;
        case SymId::GRACE_NOTE_ACCIACCATURA_STEM_DOWN:
        case SymId::GRACE_NOTE_ACCIACCATURA_STEM_UP:
        case SymId::GRACE_NOTE_SLASH_STEM_DOWN:
        case SymId::GRACE_NOTE_SLASH_STEM_UP: types.emplace(mpe::ArticulationType::ACCIACCATURA); break;
        case SymId::GRACE_NOTE_APPOGGIATURA_STEM_DOWN:
        case SymId::GRACE_NOTE_APPOGGIATURA_STEM_UP: types.emplace(mpe::ArticulationType::PRE_APPOGGIATURA); break;
        case SymId::GLISSANDO_UP:
        case SymId::GLISSANDO_DOWN: types.emplace(mpe::ArticulationType::DISCRETE_GLISSANDO); break;
        case SymId::WIGGLE_ARPEGGIATO_DOWN:
        case SymId::WIGGLE_ARPEGGIATO_DOWN_ARROW: types.emplace(mpe::ArticulationType::ARPEGGIO_DOWN); break;
        case SymId::WIGGLE_ARPEGGIATO_UP:
        case SymId::WIGGLE_ARPEGGIATO_UP_ARROW: types.emplace(mpe::ArticulationType::ARPEGGIO_UP); break;
        case SymId::WIGGLE_ARPEGGIATO_DOWN_SWASH: types.emplace(mpe::ArticulationType::ARPEGGIO_STRAIGHT_DOWN); break;
        case SymId::WIGGLE_ARPEGGIATO_UP_SWASH: types.emplace(mpe::ArticulationType::ARPEGGIO_STRAIGHT_UP); break;
        case SymId::WIGGLE_VIBRATO_LARGE_FASTER:
        case SymId::WIGGLE_VIBRATO_LARGE_FASTER_STILL:
        case SymId::WIGGLE_VIBRATO_LARGE_FASTEST:
        case SymId::WIGGLE_VIBRATO_LARGE_SLOW:
        case SymId::WIGGLE_VIBRATO_LARGE_SLOWER:
        case SymId::WIGGLE_VIBRATO_LARGE_SLOWEST:
        case SymId::WIGGLE_VIBRATO_LARGEST_FAST:
        case SymId::WIGGLE_VIBRATO_LARGEST_FASTER:
        case SymId::WIGGLE_VIBRATO_LARGEST_FASTER_STILL:
        case SymId::WIGGLE_VIBRATO_LARGEST_FASTEST:
        case SymId::WIGGLE_VIBRATO_LARGEST_SLOW:
        case SymId::WIGGLE_VIBRATO_LARGEST_SLOWEST:
        case SymId::WIGGLE_VIBRATO_MEDIUM_FAST:
        case SymId::WIGGLE_VIBRATO_MEDIUM_FASTER:
        case SymId::WIGGLE_VIBRATO_MEDIUM_FASTER_STILL:
        case SymId::WIGGLE_VIBRATO_MEDIUM_FASTEST:
        case SymId::WIGGLE_VIBRATO_MEDIUM_SLOW:
        case SymId::WIGGLE_VIBRATO_MEDIUM_SLOWEST:
        case SymId::WIGGLE_VIBRATO_SMALL_FAST:
        case SymId::WIGGLE_VIBRATO_SMALL_FASTER:
        case SymId::WIGGLE_VIBRATO_SMALL_FASTER_STILL:
        case SymId::WIGGLE_VIBRATO_SMALL_FASTEST:
        case SymId::WIGGLE_VIBRATO_SMALL_SLOW:
        case SymId::WIGGLE_VIBRATO_SMALL_SLOWER:
        case SymId::WIGGLE_VIBRATO_SMALL_SLOWEST:
        case SymId::WIGGLE_VIBRATO_SMALLEST_FAST:
        case SymId::WIGGLE_VIBRATO_SMALLEST_FASTER:
        case SymId::WIGGLE_VIBRATO_SMALLEST_FASTER_STILL:
        case SymId::WIGGLE_VIBRATO_SMALLEST_FASTEST:
        case SymId::WIGGLE_VIBRATO_SMALLEST_SLOW:
        case SymId::WIGGLE_VIBRATO_SMALLEST_SLOWER:
        case SymId::WIGGLE_VIBRATO_SMALLEST_SLOWEST:
        case SymId::WIGGLE_VIBRATO_START:
        case SymId::WIGGLE_VIBRATO_LARGE_FAST:
        case SymId::WIGGLE_VIBRATO: types.emplace(mpe::ArticulationType::VIBRATO); break;
        case SymId::WIGGLE_VIBRATO_WIDE: types.emplace(mpe::ArticulationType::WIDE_VIBRATO); break;
        case SymId::HANDBELLS_MALLET_BELL_ON_TABLE: types.emplace(mpe::ArticulationType::MALLET_BELL_ON_TABLE); break;
        case SymId::HANDBELLS_MALLET_BELL_SUSPENDED: types.emplace(mpe::ArticulationType::MALLET_BELL_SUSPENDED); break;
        case SymId::HANDBELLS_MALLET_LFT: types.emplace(mpe::ArticulationType::MALLET_LIFT); break;
        case SymId::HANDBELLS_PLUCK_LIFT: types.emplace(mpe::ArticulationType::PLUCK_LIFT); break;
        case SymId::HANDBELLS_GYRO: types.emplace(mpe::ArticulationType::GYRO); break;
        case SymId::HANDBELLS_MARTELLATO: types.emplace(mpe::ArticulationType::MARTELLATO); break;
        case SymId::HANDBELLS_MARTELLATO_LIFT: types.emplace(mpe::ArticulationType::MARTELLATO_LIFT); break;
        case SymId::HANDBELLS_HAND_MARTELLATO: types.emplace(mpe::ArticulationType::HAND_MARTELLATO); break;
        case SymId::HANDBELLS_MUTED_MARTELLATO: types.emplace(mpe::ArticulationType::MUTED_MARTELLATO); break;
        default: break;
    }

    return types;
}

void SymbolsMetaParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->is_articulation_family()) { return; }

    const Articulation *articulation_symbol = to_articulation(item);
    if (!articulation_symbol->play_articulation()) {
        return;
    }

    mpe::ArticulationTypeSet types;

    if (articulation_symbol->text_type() != ArticulationTextType::NO_TEXT) {
        types.insert(text_type_to_articulation_type(articulation_symbol->text_type()));
    } else {
        types = symbol_to_articulations(articulation_symbol->sym_id(), articulation_symbol->ornament_style());
    }

    for (mpe::ArticulationType type : types) {
        const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type, mpe::ArticulationType::STANDARD);
        if (pattern.empty()) {
            continue;
        }

        append_articulation_data(mpe::ArticulationMeta(type, pattern, ctx.nominal_timestamp, ctx.nominal_duration), result);
    }
}
