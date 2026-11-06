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
#include "generalmidi.h"

#include <array>
#include <string_view>

using namespace std::literals;

namespace mu::iex::midi { namespace {
struct Gm1ProgramData {
    GM1Program program;
    std::string_view midi_name;
};

static constexpr std::array g_m1_program_data = {
    Gm1ProgramData{GM1Program::ACOUSTIC_GRAND_PIANO, "Grand Piano"sv},
    Gm1ProgramData{GM1Program::BRIGHT_ACOUSTIC_PIANO, "Bright Piano"sv},
    Gm1ProgramData{GM1Program::ELECTRIC_GRAND_PIANO, "E.Grand"sv},
    Gm1ProgramData{GM1Program::HONKY_TONK_PIANO, "Honky-tonk"sv},
    Gm1ProgramData{GM1Program::ELECTRIC_PIANO1, "E.Piano"sv},
    Gm1ProgramData{GM1Program::ELECTRIC_PIANO2, "E.Piano 2"sv},
    Gm1ProgramData{GM1Program::HARPSICHORD, "Harpsichord"sv},
    Gm1ProgramData{GM1Program::CLAVI, "Clav."sv},
    Gm1ProgramData{GM1Program::CELESTA, "Celesta"sv},
    Gm1ProgramData{GM1Program::GLOCKENSPIEL, "Glockenspiel"sv},
    Gm1ProgramData{GM1Program::MUSIC_BOX, "Music Box"sv},
    Gm1ProgramData{GM1Program::VIBRAPHONE, "Vibraphone"sv},
    Gm1ProgramData{GM1Program::MARIMBA, "Marimba"sv},
    Gm1ProgramData{GM1Program::XYLOPHONE, "Xylophone"sv},
    Gm1ProgramData{GM1Program::TUBULAR_BELLS, "Tubular Bells"sv},
    Gm1ProgramData{GM1Program::DULCIMER, "Dulcimer"sv},
    Gm1ProgramData{GM1Program::DRAWBAR_ORGAN, "Drawbar Organ"sv},
    Gm1ProgramData{GM1Program::PERCUSSIVE_ORGAN, "Perc. Organ"sv},
    Gm1ProgramData{GM1Program::ROCK_ORGAN, "Rock Organ"sv},
    Gm1ProgramData{GM1Program::CHURCH_ORGAN, "Church Organ"sv},
    Gm1ProgramData{GM1Program::REED_ORGAN, "Reed Organ"sv},
    Gm1ProgramData{GM1Program::ACCORDION, "Akkordion"sv},
    Gm1ProgramData{GM1Program::HARMONICA, "Harmonica"sv},
    Gm1ProgramData{GM1Program::TANGO_ACCORDION, "Bandoneon"sv},
    Gm1ProgramData{GM1Program::ACOUSTIC_GUITAR_NYLON, "Nylon Gtr."sv},
    Gm1ProgramData{GM1Program::ACOUSTIC_GUITAR_STEEL, "Steel Gtr."sv},
    Gm1ProgramData{GM1Program::ELECTRIC_GUITAR_JAZZ, "Jazz Guitar"sv},
    Gm1ProgramData{GM1Program::ELECTRIC_GUITAR_CLEAN, "Clean Guitar"sv},
    Gm1ProgramData{GM1Program::ELECTRIC_GUITAR_MUTED, "Muted Guitar"sv},
    Gm1ProgramData{GM1Program::OVERDRIVEN_GUITAR, "Overdrive Gtr"sv},
    Gm1ProgramData{GM1Program::DISTORTION_GUITAR, "DistortionGtr"sv},
    Gm1ProgramData{GM1Program::GUITAR_HARMONICS, "Gtr.Harmonics"sv},
    Gm1ProgramData{GM1Program::ACOUSTIC_BASS, "Acoustic Bass"sv},
    Gm1ProgramData{GM1Program::ELECTRIC_BASS_FINGER, "Fingered Bass"sv},
    Gm1ProgramData{GM1Program::ELECTRIC_BASS_PICK, "Picked Bass"sv},
    Gm1ProgramData{GM1Program::FRETLESS_BASS, "Fretless Bass"sv},
    Gm1ProgramData{GM1Program::SLAP_BASS1, "Slap Bass 1"sv},
    Gm1ProgramData{GM1Program::SLAP_BASS2, "Slap Bass 2"sv},
    Gm1ProgramData{GM1Program::SYNTH_BASS1, "Synth Bass 1"sv},
    Gm1ProgramData{GM1Program::SYNTH_BASS2, "Synth Bass 2"sv},
    Gm1ProgramData{GM1Program::VIOLIN, "Violin"sv},
    Gm1ProgramData{GM1Program::VIOLA, "Viola"sv},
    Gm1ProgramData{GM1Program::CELLO, "Cello"sv},
    Gm1ProgramData{GM1Program::CONTRABASS, "Contrabass"sv},
    Gm1ProgramData{GM1Program::TREMOLO_STRINGS, "Tremolo Str."sv},
    Gm1ProgramData{GM1Program::PIZZICATO_STRINGS, "PizzicatoStr."sv},
    Gm1ProgramData{GM1Program::ORCHESTRAL_HARP, "Harp"sv},
    Gm1ProgramData{GM1Program::TIMPANI, "Timpani"sv},
    Gm1ProgramData{GM1Program::STRING_ENSEMBLE1, "Strings 1"sv},
    Gm1ProgramData{GM1Program::STRING_ENSEMBLE2, "Strings 2"sv},
    Gm1ProgramData{GM1Program::SYNTH_STRINGS1, "Syn.Strings 1"sv},
    Gm1ProgramData{GM1Program::SYNTH_STRINGS2, "Syn.Strings 2"sv},
    Gm1ProgramData{GM1Program::CHOIR_AAHS, "Choir Aahs"sv},
    Gm1ProgramData{GM1Program::VOICE_OOHS, "Voice Oohs"sv},
    Gm1ProgramData{GM1Program::SYNTH_VOICE, "Synth Voice"sv},
    Gm1ProgramData{GM1Program::ORCHESTRA_HIT, "Orchestra Hit"sv},
    Gm1ProgramData{GM1Program::TRUMPET, "Trumpet"sv},
    Gm1ProgramData{GM1Program::TROMBONE, "Trombone"sv},
    Gm1ProgramData{GM1Program::TUBA, "Tuba"sv},
    Gm1ProgramData{GM1Program::MUTED_TRUMPET, "Muted Trumpet"sv},
    Gm1ProgramData{GM1Program::FRENCH_HORN, "French Horn"sv},
    Gm1ProgramData{GM1Program::BRASS_SECTION, "Brass Section"sv},
    Gm1ProgramData{GM1Program::SYNTH_BRASS1, "Synth Brass 1"sv},
    Gm1ProgramData{GM1Program::SYNTH_BRASS2, "Synth Brass 2"sv},
    Gm1ProgramData{GM1Program::SOPRANO_SAX, "Soprano Sax"sv},
    Gm1ProgramData{GM1Program::ALTO_SAX, "Alto Sax"sv},
    Gm1ProgramData{GM1Program::TENOR_SAX, "Tenor Sax"sv},
    Gm1ProgramData{GM1Program::BARITONE_SAX, "Baritone Sax"sv},
    Gm1ProgramData{GM1Program::OBOE, "Oboe"sv},
    Gm1ProgramData{GM1Program::ENGLISH_HORN, "English Horn"sv},
    Gm1ProgramData{GM1Program::BASSOON, "Bassoon"sv},
    Gm1ProgramData{GM1Program::CLARINET, "Clarinet"sv},
    Gm1ProgramData{GM1Program::PICCOLO, "Piccolo"sv},
    Gm1ProgramData{GM1Program::FLUTE, "Flute"sv},
    Gm1ProgramData{GM1Program::RECORDER, "Recorder"sv},
    Gm1ProgramData{GM1Program::PAN_FLUTE, "Pan Flute"sv},
    Gm1ProgramData{GM1Program::BLOWN_BOTTLE, "Blown Bottle"sv},
    Gm1ProgramData{GM1Program::SHAKUHACHI, "Shakuhachi"sv},
    Gm1ProgramData{GM1Program::WHISTLE, "Whistle"sv},
    Gm1ProgramData{GM1Program::OCARINA, "Ocarina"sv},
    Gm1ProgramData{GM1Program::LEAD1_SQUARE, "Square Wave"sv},
    Gm1ProgramData{GM1Program::LEAD2_SAWTOOTH, "Saw Wave"sv},
    Gm1ProgramData{GM1Program::LEAD3_CALLIOPE, "Calliope"sv},
    Gm1ProgramData{GM1Program::LEAD4_CHIFF, "Chiffer Lead"sv},
    Gm1ProgramData{GM1Program::LEAD5_CHARANG, "Charang"sv},
    Gm1ProgramData{GM1Program::LEAD6_VOICE, "Solo Vox"sv},
    Gm1ProgramData{GM1Program::LEAD7_FIFTHS, "Fifth Saw"sv},
    Gm1ProgramData{GM1Program::LEAD8_BASS_AND_LEAD, "Bass Lead"sv},
    Gm1ProgramData{GM1Program::PAD1_NEW_AGE, "New Age Pad"sv},
    Gm1ProgramData{GM1Program::PAD2_WARM, "Warm Pad"sv},
    Gm1ProgramData{GM1Program::PAD3_POLYSYNTH, "Polysynth Pad"sv},
    Gm1ProgramData{GM1Program::PAD4_CHOIR, "Choir Pad"sv},
    Gm1ProgramData{GM1Program::PAD5_BOWED, "Bowed Pad"sv},
    Gm1ProgramData{GM1Program::PAD6_METALLIC, "Metallic Pad"sv},
    Gm1ProgramData{GM1Program::PAD7_HALO, "Halo Pad"sv},
    Gm1ProgramData{GM1Program::PAD8_SWEEP, "Sweep Pad"sv},
    Gm1ProgramData{GM1Program::F_X1_RAIN, "Rain"sv},
    Gm1ProgramData{GM1Program::F_X2_SOUNDTRACK, "Soundtrack"sv},
    Gm1ProgramData{GM1Program::F_X3_CRYSTAL, "Crystal"sv},
    Gm1ProgramData{GM1Program::F_X4_ATMOSPHERE, "Athmosphere"sv},
    Gm1ProgramData{GM1Program::F_X5_BRIGHTNESS, "Brightness"sv},
    Gm1ProgramData{GM1Program::F_X6_GOBLINS, "Goblins"sv},
    Gm1ProgramData{GM1Program::F_X7_ECHOES, "Echoes"sv},
    Gm1ProgramData{GM1Program::F_X8_SCI_FI, "Sci-Fi"sv},
    Gm1ProgramData{GM1Program::SITAR, "Sitar"sv},
    Gm1ProgramData{GM1Program::BANJO, "Banjo"sv},
    Gm1ProgramData{GM1Program::SHAMISEN, "Shamisen"sv},
    Gm1ProgramData{GM1Program::KOTO, "Koto"sv},
    Gm1ProgramData{GM1Program::KALIMBA, "Kalimba"sv},
    Gm1ProgramData{GM1Program::BAG_PIPE, "Bagpipe"sv},
    Gm1ProgramData{GM1Program::FIDDLE, "Fiddle"sv},
    Gm1ProgramData{GM1Program::SHANAI, "Shanai"sv},
    Gm1ProgramData{GM1Program::TINKLE_BELL, "Tinkle Bell"sv},
    Gm1ProgramData{GM1Program::AGOGO, "Agogo"sv},
    Gm1ProgramData{GM1Program::STEEL_DRUMS, "Steel Drums"sv},
    Gm1ProgramData{GM1Program::WOODBLOCK, "Woodblock"sv},
    Gm1ProgramData{GM1Program::TAIKO_DRUM, "Taiko Drum"sv},
    Gm1ProgramData{GM1Program::MELODIC_TOM, "Melodic Drum"sv},
    Gm1ProgramData{GM1Program::SYNTH_DRUM, "Synth Drum"sv},
    Gm1ProgramData{GM1Program::REVERSE_CYMBAL, "Rev. Cymbal"sv},
    Gm1ProgramData{GM1Program::GUITAR_FRET_NOISE, "GtrFret Noise"sv},
    Gm1ProgramData{GM1Program::BREATH_NOISE, "Breath Noise"sv},
    Gm1ProgramData{GM1Program::SEASHORE, "Seashore"sv},
    Gm1ProgramData{GM1Program::BIRD_TWEET, "Bird Tweed"sv},
    Gm1ProgramData{GM1Program::TELEPHONE_RING, "Telephone"sv},
    Gm1ProgramData{GM1Program::HELICOPTER, "Helicopter"sv},
    Gm1ProgramData{GM1Program::APPLAUSE, "Applaus"sv},
    Gm1ProgramData{GM1Program::GUNSHOT, "Gunshot"sv},
};
static_assert(::mu::iex::midi::g_m1_program_data.size() == 128);
}} // namespace mu::iex::midi

std::string_view mu::iex::midi::get_midi_name(const GM1Program program) {
    const auto &data = ::mu::iex::midi::g_m1_program_data[static_cast<std::size_t>(program)];

    return data.midi_name;
}
