/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include <string>
#include <unordered_map>

#include "containers.h"
#include "types/string.h"

#include "mpe/mpetypes.h"

namespace muse::mpe {
static const std::unordered_map<ArticulationFamily, std::string> articulation_family_names{
    {ArticulationFamily::UNDEFINED, "Undefined"},
    {ArticulationFamily::KEYBOARDS, "KeyboardsArticulation"},
    {ArticulationFamily::STRINGS, "StringsArticulation"},
    {ArticulationFamily::WINDS, "WindsArticulation"},
    {ArticulationFamily::PERCUSSIONS, "PercussionsArticulation"},
    {ArticulationFamily::VOICES, "VoicesArticulation"},
};

static const std::unordered_map<ArticulationType, std::string> articulation_type_names{
    {ArticulationType::UNDEFINED, "Undefined"},
    {ArticulationType::STANDARD, "Standard"},
    {ArticulationType::STACCATO, "Staccato"},
    {ArticulationType::STACCATISSIMO, "Staccatissimo"},
    {ArticulationType::TENUTO, "Tenuto"},
    {ArticulationType::MARCATO, "Marcato"},
    {ArticulationType::ACCENT, "Accent"},
    {ArticulationType::SOFT_ACCENT, "SoftAccent"},
    {ArticulationType::LAISSEZ_VIBRER, "LaissezVibrer"},
    {ArticulationType::SUBITO, "Subito"},
    {ArticulationType::FADE_IN, "FadeIn"},
    {ArticulationType::FADE_OUT, "FadeOut"},
    {ArticulationType::HARMONIC, "Harmonic"},
    {ArticulationType::JAZZ_TONE, "JazzTone"},
    {ArticulationType::PALM_MUTE, "PalmMute"},
    {ArticulationType::MUTE, "Mute"},
    {ArticulationType::OPEN, "Open"},
    {ArticulationType::PIZZICATO, "Pizzicato"},
    {ArticulationType::SNAP_PIZZICATO, "SnapPizzicato"},
    {ArticulationType::RANDOM_PIZZICATO, "RandomPizzicato"},
    {ArticulationType::UP_BOW, "UpBow"},
    {ArticulationType::DOWN_BOW, "DownBow"},
    {ArticulationType::DETACHE, "Detache"},
    {ArticulationType::MARTELE, "Martele"},
    {ArticulationType::JETE, "Jete"},
    {ArticulationType::COL_LEGNO, "ColLegno"},
    {ArticulationType::SUL_PONT, "SulPonticello"},
    {ArticulationType::SUL_TASTO, "SulTasto"},
    {ArticulationType::GHOST_NOTE, "GhostNote"},
    {ArticulationType::CROSS_NOTE, "CrossNote"},
    {ArticulationType::CROSS_LARGE_NOTE, "CrossLargeNote"},
    {ArticulationType::CROSS_ORNATE_NOTE, "CrossOrnateNote"},
    {ArticulationType::CIRCLE_NOTE, "CircleNote"},
    {ArticulationType::CIRCLE_CROSS_NOTE, "CircleCrossNote"},
    {ArticulationType::CIRCLE_DOT_NOTE, "CircleDotNote"},
    {ArticulationType::TRIANGLE_LEFT_NOTE, "TriangleLeftNote"},
    {ArticulationType::TRIANGLE_RIGHT_NOTE, "TriangleRightNote"},
    {ArticulationType::TRIANGLE_UP_NOTE, "TriangleUpNote"},
    {ArticulationType::TRIANGLE_DOWN_NOTE, "TriangleDownNote"},
    {ArticulationType::TRIANGLE_ROUND_DOWN_NOTE, "TriangleRoundDownNote"},
    {ArticulationType::DIAMOND_NOTE, "DiamondNote"},
    {ArticulationType::MOON_NOTE, "MoonNote"},
    {ArticulationType::PLUS_NOTE, "PlusNote"},
    {ArticulationType::SLASH_NOTE, "SlashNote"},
    {ArticulationType::SQUARE_NOTE, "SquareNote"},
    {ArticulationType::SLASHED_BACKWARDS_NOTE, "SlashedBackwardsNote"},
    {ArticulationType::SLASHED_FORWARDS_NOTE, "SlashedForwardsNote"},
    {ArticulationType::FALL, "Fall"},
    {ArticulationType::QUICK_FALL, "QuickFall"},
    {ArticulationType::DOIT, "Doit"},
    {ArticulationType::PLOP, "Plop"},
    {ArticulationType::SCOOP, "Scoop"},
    {ArticulationType::BRASS_BEND, "BrassBend"},
    {ArticulationType::MULTIBEND, "Multibend"},
    {ArticulationType::SLIDE_OUT_DOWN, "SlideOutDown"},
    {ArticulationType::SLIDE_OUT_UP, "SlideOutUp"},
    {ArticulationType::SLIDE_IN_ABOVE, "SlideInAbove"},
    {ArticulationType::SLIDE_IN_BELOW, "SlideInBelow"},
    {ArticulationType::CRESCENDO, "Crescendo"},
    {ArticulationType::DIMINUENDO, "Diminuendo"},
    {ArticulationType::DISCRETE_GLISSANDO, "DiscreteGlissando"},
    {ArticulationType::CONTINUOUS_GLISSANDO, "ContinuousGlissando"},
    {ArticulationType::BREATH, "Breath"},
    {ArticulationType::LEGATO, "Legato"},
    {ArticulationType::PEDAL, "Pedal"},
    {ArticulationType::ARPEGGIO, "Arpeggio"},
    {ArticulationType::ARPEGGIO_UP, "ArpeggioUp"},
    {ArticulationType::ARPEGGIO_DOWN, "ArpeggioDown"},
    {ArticulationType::ARPEGGIO_STRAIGHT_UP, "ArpeggioStraightUp"},
    {ArticulationType::ARPEGGIO_STRAIGHT_DOWN, "ArpeggioStraightDown"},
    {ArticulationType::VIBRATO, "Vibrato"},
    {ArticulationType::WIDE_VIBRATO, "WideVibrato"},
    {ArticulationType::MOLTO_VIBRATO, "MoltoVibrato"},
    {ArticulationType::SENZA_VIBRATO, "SenzaVibrato"},
    {ArticulationType::TREMOLO8TH, "Tremolo8th"},
    {ArticulationType::TREMOLO16TH, "Tremolo16th"},
    {ArticulationType::TREMOLO32ND, "Tremolo32nd"},
    {ArticulationType::TREMOLO64TH, "Tremolo64th"},
    {ArticulationType::TREMOLO_BUZZ, "TremoloBuzz"},
    {ArticulationType::UPPER_MORDENT, "ShortTrill"},
    {ArticulationType::TRILL, "Trill"},
    {ArticulationType::TRILL_BAROQUE, "TrillBaroque"},
    {ArticulationType::LOWER_MORDENT, "Mordent"},
    {ArticulationType::PRALL_MORDENT, "PrallMordent"},
    {ArticulationType::UPPER_MORDENT_BAROQUE, "UpperMordentBaroque"},
    {ArticulationType::LOWER_MORDENT_BAROQUE, "LowerMordentBaroque"},
    {ArticulationType::MORDENT_WITH_UPPER_PREFIX, "MordentWithUpperPrefix"},
    {ArticulationType::UP_MORDENT, "UpMordent"},
    {ArticulationType::DOWN_MORDENT, "DownMordent"},
    {ArticulationType::TREMBLEMENT, "Tremblement"},
    {ArticulationType::UP_PRALL, "UpPrall"},
    {ArticulationType::PRALL_UP, "PrallUp"},
    {ArticulationType::PRALL_DOWN, "PrallDown"},
    {ArticulationType::LINE_PRALL, "LinePrall"},
    {ArticulationType::SLIDE, "Slide"},
    {ArticulationType::TURN, "Turn"},
    {ArticulationType::INVERTED_TURN, "InvertedTurn"},
    {ArticulationType::PRE_APPOGGIATURA, "PreAppoggiatura"},
    {ArticulationType::POST_APPOGGIATURA, "PostAppoggiatura"},
    {ArticulationType::ACCIACCATURA, "Acciaccatura"},
    {ArticulationType::TREMOLO_BAR, "TremoloBar"},
    {ArticulationType::DISTORTION, "Distortion"},
    {ArticulationType::OVERDRIVE, "Overdrive"},
    {ArticulationType::SLAP, "Slap"},
    {ArticulationType::POP, "Pop"},
    {ArticulationType::LEFT_HAND_TAPPING, "LeftHandTapping"},
    {ArticulationType::RIGHT_HAND_TAPPING, "RightHandTapping"},
    {ArticulationType::MALLET_BELL_ON_TABLE, "MalletBellOnTable"},
    {ArticulationType::MALLET_BELL_SUSPENDED, "MalletBellSuspended"},
    {ArticulationType::MALLET_LIFT, "MalletLift"},
    {ArticulationType::PLUCK, "Pluck"},
    {ArticulationType::PLUCK_LIFT, "PluckLift"},
    {ArticulationType::GYRO, "Gyro"},
    {ArticulationType::MARTELLATO, "Martellato"},
    {ArticulationType::MARTELLATO_LIFT, "MartellatoLift"},
    {ArticulationType::HAND_MARTELLATO, "HandMartellato"},
    {ArticulationType::MUTED_MARTELLATO, "MutedMartellato"},
    {ArticulationType::THUMB_DAMP, "ThumbDamp"},
    {ArticulationType::BRUSH_DAMP, "BrushDamp"},
    {ArticulationType::RING, "Ring"},
    {ArticulationType::RING_TOUCH, "RingTouch"},
    {ArticulationType::SINGING_BELL, "SingingBell"},
    {ArticulationType::SINGING_VIBRATE, "SingingVibrate"},
    {ArticulationType::SWING, "Swing"},
    {ArticulationType::ECHO, "Echo"},
};

inline ArticulationFamily articulation_family_from_string(const muse::String &str) {
    return muse::key(::muse::mpe::articulation_family_names, str.to_std_string(), ArticulationFamily::UNDEFINED);
}

inline ArticulationFamily articulation_family_from_string(const std::string &str) {
    return muse::key(::muse::mpe::articulation_family_names, str, ArticulationFamily::UNDEFINED);
}

inline const muse::String articulation_family_to_string(const ArticulationFamily family) {
    return muse::String::from_std_string(muse::value(::muse::mpe::articulation_family_names, family));
}

inline ArticulationType articulation_type_from_string(const muse::String &str) {
    return muse::key(::muse::mpe::articulation_type_names, str.to_std_string(), ArticulationType::UNDEFINED);
}

inline ArticulationType articulation_type_from_string(const std::string &str) {
    return muse::key(::muse::mpe::articulation_type_names, str, ArticulationType::UNDEFINED);
}

inline const muse::String articulation_type_to_string(const ArticulationType type) {
    return muse::String::from_std_string(muse::value(::muse::mpe::articulation_type_names, type));
}
} // namespace muse::mpe
