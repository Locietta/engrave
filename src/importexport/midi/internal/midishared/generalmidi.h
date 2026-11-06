/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include <cstdint>
#include <optional>
#include <string_view>

namespace mu::iex::midi {
// General MIDI Level 1 program numbers
enum class GM1Program : std::uint8_t {
    // 0-7: Piano
    ACOUSTIC_GRAND_PIANO = 0,
    BRIGHT_ACOUSTIC_PIANO,
    ELECTRIC_GRAND_PIANO,
    HONKY_TONK_PIANO,
    ELECTRIC_PIANO1,
    ELECTRIC_PIANO2,
    HARPSICHORD,
    CLAVI,
    // 8-15: Chromatic Percussion
    CELESTA,
    GLOCKENSPIEL,
    MUSIC_BOX,
    VIBRAPHONE,
    MARIMBA,
    XYLOPHONE,
    TUBULAR_BELLS,
    DULCIMER,
    // 16-23: Organ
    DRAWBAR_ORGAN,
    PERCUSSIVE_ORGAN,
    ROCK_ORGAN,
    CHURCH_ORGAN,
    REED_ORGAN,
    ACCORDION,
    HARMONICA,
    TANGO_ACCORDION,
    // 24-31: Guitar
    ACOUSTIC_GUITAR_NYLON,
    ACOUSTIC_GUITAR_STEEL,
    ELECTRIC_GUITAR_JAZZ,
    ELECTRIC_GUITAR_CLEAN,
    ELECTRIC_GUITAR_MUTED,
    OVERDRIVEN_GUITAR,
    DISTORTION_GUITAR,
    GUITAR_HARMONICS,
    // 32-39: Bass
    ACOUSTIC_BASS,
    ELECTRIC_BASS_FINGER,
    ELECTRIC_BASS_PICK,
    FRETLESS_BASS,
    SLAP_BASS1,
    SLAP_BASS2,
    SYNTH_BASS1,
    SYNTH_BASS2,
    // 40-47: Strings
    VIOLIN,
    VIOLA,
    CELLO,
    CONTRABASS,
    TREMOLO_STRINGS,
    PIZZICATO_STRINGS,
    ORCHESTRAL_HARP,
    TIMPANI,
    // 48-55: Ensemble
    STRING_ENSEMBLE1,
    STRING_ENSEMBLE2,
    SYNTH_STRINGS1,
    SYNTH_STRINGS2,
    CHOIR_AAHS,
    VOICE_OOHS,
    SYNTH_VOICE,
    ORCHESTRA_HIT,
    // 56-63: Brass
    TRUMPET,
    TROMBONE,
    TUBA,
    MUTED_TRUMPET,
    FRENCH_HORN,
    BRASS_SECTION,
    SYNTH_BRASS1,
    SYNTH_BRASS2,
    // 64-71: Reed
    SOPRANO_SAX,
    ALTO_SAX,
    TENOR_SAX,
    BARITONE_SAX,
    OBOE,
    ENGLISH_HORN,
    BASSOON,
    CLARINET,
    // 72-79: Pipe
    PICCOLO,
    FLUTE,
    RECORDER,
    PAN_FLUTE,
    BLOWN_BOTTLE,
    SHAKUHACHI,
    WHISTLE,
    OCARINA,
    // 80-87: Synth Lead
    LEAD1_SQUARE,
    LEAD2_SAWTOOTH,
    LEAD3_CALLIOPE,
    LEAD4_CHIFF,
    LEAD5_CHARANG,
    LEAD6_VOICE,
    LEAD7_FIFTHS,
    LEAD8_BASS_AND_LEAD,
    // 88-95: Synth Pad
    PAD1_NEW_AGE,
    PAD2_WARM,
    PAD3_POLYSYNTH,
    PAD4_CHOIR,
    PAD5_BOWED,
    PAD6_METALLIC,
    PAD7_HALO,
    PAD8_SWEEP,
    // 96-103: Synth Effects
    F_X1_RAIN,
    F_X2_SOUNDTRACK,
    F_X3_CRYSTAL,
    F_X4_ATMOSPHERE,
    F_X5_BRIGHTNESS,
    F_X6_GOBLINS,
    F_X7_ECHOES,
    F_X8_SCI_FI,
    // 104-111: Ethnic
    SITAR,
    BANJO,
    SHAMISEN,
    KOTO,
    KALIMBA,
    BAG_PIPE,
    FIDDLE,
    SHANAI,
    // 112-119: Percussive
    TINKLE_BELL,
    AGOGO,
    STEEL_DRUMS,
    WOODBLOCK,
    TAIKO_DRUM,
    MELODIC_TOM,
    SYNTH_DRUM,
    REVERSE_CYMBAL,
    // 120-127: Sound Effects
    GUITAR_FRET_NOISE,
    BREATH_NOISE,
    SEASHORE,
    BIRD_TWEET,
    TELEPHONE_RING,
    HELICOPTER,
    APPLAUSE,
    GUNSHOT,
};

constexpr std::optional<GM1Program> to_gm1_program(const std::uint8_t v) {
    if (v > 127) {
        return std::nullopt;
    }

    return GM1Program{v};
}

constexpr std::uint8_t to_midi_data(const GM1Program p) { return static_cast<std::uint8_t>(p); }

std::string_view get_midi_name(GM1Program);
} // namespace mu::iex::midi
