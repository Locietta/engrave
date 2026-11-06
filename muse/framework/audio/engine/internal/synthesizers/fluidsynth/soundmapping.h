/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#ifndef MUSE_AUDIO_SOUNDMAPPING_H
#define MUSE_AUDIO_SOUNDMAPPING_H

#include <functional>

#include "log.h"
#include "midi/miditypes.h"
#include "mpe/events.h"

namespace muse::audio::synth {
struct SoundMappingKey {
    mpe::SoundId id = mpe::SoundId::UNDEFINED;
    mpe::SoundSubCategories sub_categories;

    bool operator<(const SoundMappingKey &other) const {
        if (id < other.id) {
            return true;
        }

        if (id > other.id) {
            return false;
        }

        return sub_categories < other.sub_categories;
    }
};

using ArticulationMapping = std::map<mpe::ArticulationType, midi::Program>;

static const auto &mapping_by_category(const mpe::SoundCategory category) {
    static const std::map<SoundMappingKey, midi::Programs> keyboards_mappings = {
        {{mpe::SoundId::CELESTA, {}}, {midi::Program(0, 8)}},
        {{mpe::SoundId::CLAVICHORD, {mpe::SoundSubCategory::BAROQUE}}, {midi::Program(0, 7)}},
        {{mpe::SoundId::CLAVICHORD, {mpe::SoundSubCategory::ELECTRIC}}, {midi::Program(0, 7)}},
        {{mpe::SoundId::ONDES_MARTENOT, {mpe::SoundSubCategory::ELECTRIC}}, {midi::Program(0, 79)}},
        {{mpe::SoundId::HARPSICHORD, {}}, {midi::Program(0, 6)}},
        {{mpe::SoundId::VIRGINAL, {mpe::SoundSubCategory::BAROQUE}}, {midi::Program(0, 6)}},
        {{mpe::SoundId::PIANO, {mpe::SoundSubCategory::ELECTRIC}}, {midi::Program(0, 4)}},
        {{mpe::SoundId::PIANO, {mpe::SoundSubCategory::GRAND}}, {midi::Program(0, 0)}},
        {{mpe::SoundId::PIANO, {mpe::SoundSubCategory::HONKY_TONK}}, {midi::Program(0, 3)}},
        {{mpe::SoundId::PIANO, {}}, {midi::Program(0, 0)}},
        {{mpe::SoundId::PIANO, {mpe::SoundSubCategory::TOY}}, {midi::Program(0, 8)}},
        {{mpe::SoundId::PIANO, {mpe::SoundSubCategory::UPRIGHT}}, {midi::Program(0, 0)}},

        {{mpe::SoundId::ORGAN, {mpe::SoundSubCategory::HAMMOND}}, {midi::Program(0, 16)}},
        {{mpe::SoundId::ORGAN, {}}, {midi::Program(0, 19)}},
        {{mpe::SoundId::ORGAN, {mpe::SoundSubCategory::PERCUSSIVE}}, {midi::Program(0, 17)}},
        {{mpe::SoundId::ORGAN, {mpe::SoundSubCategory::PIPED}}, {midi::Program(0, 19)}},
        {{mpe::SoundId::ORGAN, {mpe::SoundSubCategory::ROTARY}}, {midi::Program(0, 18)}},
        {{mpe::SoundId::ORGAN, {mpe::SoundSubCategory::REED}}, {midi::Program(0, 20)}},

        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::STRING}},
         {midi::Program(0, 50), midi::Program(0, 51)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::BRASS}},
         {midi::Program(0, 62), midi::Program(0, 63)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC}}, {midi::Program(0, 80)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_ATMOSPHERE}}, {midi::Program(0, 99)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_BRIGHTNESS}}, {midi::Program(0, 100)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_CRYSTAL}}, {midi::Program(0, 98)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_ECHOES}}, {midi::Program(0, 102)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_GOBLINS}}, {midi::Program(0, 101)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_RAIN}}, {midi::Program(0, 96)}},
        {{mpe::SoundId::SYNTHESIZER,
          {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_ATMOSPHERE, mpe::SoundSubCategory::FX_BRIGHTNESS,
           mpe::SoundSubCategory::FX_CRYSTAL, mpe::SoundSubCategory::FX_ECHOES, mpe::SoundSubCategory::FX_GOBLINS,
           mpe::SoundSubCategory::FX_RAIN}},
         {midi::Program(0, 80), midi::Program(0, 99), midi::Program(0, 100), midi::Program(0, 98), midi::Program(0, 102),
          midi::Program(0, 101), midi::Program(0, 96)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::SINE_WAVE}}, {midi::Program(8, 80)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::SQUARE_WAVE}}, {midi::Program(0, 80)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::SAWTOOTH_WAVE}}, {midi::Program(0, 81)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::NEW_AGE}}, {midi::Program(0, 88)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::PAD}}, {midi::Program(0, 88)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::WARM}}, {midi::Program(0, 89)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::POLYSYNTH}}, {midi::Program(0, 90)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::CHOIR}}, {midi::Program(0, 91)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::METALLIC}}, {midi::Program(0, 93)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::HALO}}, {midi::Program(0, 94)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::SWEEP}}, {midi::Program(0, 95)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_SOUND_TRACK}}, {midi::Program(0, 97)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FX_SCI_FI}}, {midi::Program(0, 103)}},
    };

    static const std::map<SoundMappingKey, midi::Programs> strings_mappings = {
        {{mpe::SoundId::HARP, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 46)}},

        {{mpe::SoundId::GUITAR,
          {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::NYLON, mpe::SoundSubCategory::SOPRANO, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 24)}},
        {{mpe::SoundId::GUITAR,
          {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::NYLON, mpe::SoundSubCategory::ALTO, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 24)}},
        {{mpe::SoundId::GUITAR,
          {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::NYLON, mpe::SoundSubCategory::BARITONE, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 24)}},
        {{mpe::SoundId::GUITAR,
          {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::NYLON, mpe::SoundSubCategory::CONTRA, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 24)}},

        {{mpe::SoundId::GUITAR, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 27)}},

        {{mpe::SoundId::GUITAR, {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 25)}},

        {{mpe::SoundId::GUITAR,
          {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::TWELVE_STRING,
           mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(8, 25)}},

        {{mpe::SoundId::GUITAR,
          {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::PEDAL, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 25)}},

        {{mpe::SoundId::GUITAR, {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 24)}},

        {{mpe::SoundId::GUITAR, {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::NYLON, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 24)}},

        {{mpe::SoundId::BASS_GUITAR, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::TENOR, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 33)}},
        {{mpe::SoundId::BASS_GUITAR, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 33)}},
        {{mpe::SoundId::BASS_GUITAR, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::FRETLESS, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 35)}},
        {{mpe::SoundId::BASS_GUITAR, {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 32)}},

        {{mpe::SoundId::BANJO, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 105)}},
        {{mpe::SoundId::BANJO, {mpe::SoundSubCategory::TENOR, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 105)}},
        {{mpe::SoundId::BANJO, {mpe::SoundSubCategory::IRISH, mpe::SoundSubCategory::TENOR, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 105)}},

        {{mpe::SoundId::UKULELE, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 24)}},
        {{mpe::SoundId::UKULELE, {mpe::SoundSubCategory::TENOR, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 24)}},
        {{mpe::SoundId::UKULELE, {mpe::SoundSubCategory::BARITONE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 24)}},

        {{mpe::SoundId::MANDOLIN, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(16, 25)}},
        {{mpe::SoundId::MANDOLIN,
          {
              mpe::SoundSubCategory::ALTO,
              mpe::SoundSubCategory::PLUCKED,
          }},
         {midi::Program(16, 25)}},
        {{mpe::SoundId::MANDOLIN, {mpe::SoundSubCategory::TENOR, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(16, 25)}},
        {{mpe::SoundId::MANDOLIN, {mpe::SoundSubCategory::OCTAVE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(16, 25)}},

        {{mpe::SoundId::MTN_DULCIMER, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 25)}},
        {{mpe::SoundId::MTN_DULCIMER, {mpe::SoundSubCategory::BARITONE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(16, 25)}},
        {{mpe::SoundId::MTN_DULCIMER, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(16, 25)}},

        {{mpe::SoundId::CAVAQUINHO, {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 25)}},

        {{mpe::SoundId::LUTE, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{mpe::SoundId::LUTE, {mpe::SoundSubCategory::TENOR, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{mpe::SoundId::LUTE, {mpe::SoundSubCategory::BAROQUE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{mpe::SoundId::LUTE, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{mpe::SoundId::LUTE, {mpe::SoundSubCategory::GREEK, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},

        {{mpe::SoundId::THEORBO, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 24)}},

        {{mpe::SoundId::BALALAIKA, {mpe::SoundSubCategory::PRIMA, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 107)}},
        {{mpe::SoundId::BALALAIKA, {mpe::SoundSubCategory::PICCOLO, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 107)}},
        {{mpe::SoundId::BALALAIKA, {mpe::SoundSubCategory::PRIMA, mpe::SoundSubCategory::ALTO, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(8, 107)}},
        {{mpe::SoundId::BALALAIKA,
          {
              mpe::SoundSubCategory::PRIMA,
              mpe::SoundSubCategory::BASS,
              mpe::SoundSubCategory::PLUCKED,
          }},
         {midi::Program(8, 107)}},
        {{mpe::SoundId::BALALAIKA,
          {
              mpe::SoundSubCategory::PRIMA,
              mpe::SoundSubCategory::CONTRA_BASS,
              mpe::SoundSubCategory::PLUCKED,
          }},
         {midi::Program(8, 107)}},
        {{mpe::SoundId::BALALAIKA,
          {
              mpe::SoundSubCategory::SECUNDA,
              mpe::SoundSubCategory::PLUCKED,
          }},
         {midi::Program(8, 107)}},

        {{mpe::SoundId::GUZHENG, {mpe::SoundSubCategory::CHINESE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 107)}},
        {{mpe::SoundId::GUQIN, {mpe::SoundSubCategory::CHINESE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 107)}},
        {{mpe::SoundId::PIPA, {mpe::SoundSubCategory::CHINESE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 106)}},
        {{mpe::SoundId::YANGQIN, {mpe::SoundSubCategory::CHINESE}}, {midi::Program(0, 15)}},
        {{mpe::SoundId::KOTO, {mpe::SoundSubCategory::JAPANESE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 107)}},
        {{mpe::SoundId::GAYAGEUM, {mpe::SoundSubCategory::KOREAN, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 107)}},
        {{mpe::SoundId::OUD, {mpe::SoundSubCategory::AFRICAN, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{mpe::SoundId::SHAMISEN, {mpe::SoundSubCategory::JAPANESE, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 106)}},
        {{mpe::SoundId::SITAR, {mpe::SoundSubCategory::INDIAN, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 104)}},
        {{mpe::SoundId::PRIM,
          {
              mpe::SoundSubCategory::PLUCKED,
          }},
         {midi::Program(0, 24)}},
        {{mpe::SoundId::BRAC, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 24)}},
        {{mpe::SoundId::BUGARIJA, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 24)}},
        {{mpe::SoundId::BERDA, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 32)}},
        {{mpe::SoundId::CELO, {mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 24)}},
        {{mpe::SoundId::BANDURRIA, {mpe::SoundSubCategory::SPANISH, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 24)}},
        {{mpe::SoundId::LAUD, {mpe::SoundSubCategory::SPANISH, mpe::SoundSubCategory::PLUCKED}}, {midi::Program(0, 24)}},

        {{mpe::SoundId::STRINGS_GROUP, {}}, {midi::Program(0, 48)}},
        {{mpe::SoundId::CONTRABASS, {mpe::SoundSubCategory::ORCHESTRAL}}, {midi::Program(0, 43)}},
        {{mpe::SoundId::CONTRABASS, {mpe::SoundSubCategory::ORCHESTRAL, mpe::SoundSubCategory::SECTION}}, {midi::Program(50, 48)}},
        {{mpe::SoundId::VIOLIN, {mpe::SoundSubCategory::ORCHESTRAL}}, {midi::Program(0, 40)}},
        {{mpe::SoundId::VIOLIN, {mpe::SoundSubCategory::ORCHESTRAL, mpe::SoundSubCategory::SECTION}}, {midi::Program(20, 48)}},
        {{mpe::SoundId::VIOLA, {mpe::SoundSubCategory::ORCHESTRAL}}, {midi::Program(0, 41)}},
        {{mpe::SoundId::VIOLA, {mpe::SoundSubCategory::ORCHESTRAL, mpe::SoundSubCategory::SECTION}}, {midi::Program(30, 48)}},
        {{mpe::SoundId::VIOLONCELLO, {mpe::SoundSubCategory::ORCHESTRAL}}, {midi::Program(0, 42)}},
        {{mpe::SoundId::VIOLONCELLO, {mpe::SoundSubCategory::ORCHESTRAL, mpe::SoundSubCategory::SECTION}}, {midi::Program(40, 48)}},

        {{mpe::SoundId::VIOL, {}}, {midi::Program(0, 40)}},
        {{mpe::SoundId::VIOL, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 41)}},
        {{mpe::SoundId::PARDESSUS_VIOL, {}}, {midi::Program(0, 40)}},
        {{mpe::SoundId::VIOL, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 41)}},
        {{mpe::SoundId::VIOL, {mpe::SoundSubCategory::BARITONE}}, {midi::Program(0, 42)}},
        {{mpe::SoundId::VIOLA_DA_GAMBA, {}}, {midi::Program(0, 42)}},
        {{mpe::SoundId::VIOLONE, {}}, {midi::Program(0, 43)}},

        {{mpe::SoundId::OCTOBASS, {}}, {midi::Program(0, 43)}},
        {{mpe::SoundId::TIMPLE_CANARIO,
          {mpe::SoundSubCategory::ACOUSTIC, mpe::SoundSubCategory::NYLON, mpe::SoundSubCategory::SPANISH, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 24)}},

        {{mpe::SoundId::ERHU, {mpe::SoundSubCategory::CHINESE}}, {midi::Program(0, 110)}},
        {{mpe::SoundId::NYCKELHARPA, {mpe::SoundSubCategory::SWEDISH}}, {midi::Program(0, 41)}},
        {{mpe::SoundId::AJAENG, {mpe::SoundSubCategory::KOREAN}}, {midi::Program(0, 110)}},

        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::PLUCKED}},
         {midi::Program(0, 38)}},
        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::BOWED}}, {midi::Program(0, 92)}},
    };

    static const std::map<SoundMappingKey, midi::Programs> winds_mappings = {
        {{mpe::SoundId::WINDS_GROUP, {}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::PICCOLO, {}}, {midi::Program(0, 72)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::TREBLE}}, {midi::Program(0, 72)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::CONTRA_ALTO}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::SUB_CONTRA_ALTO}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::DOUBLE_CONTRA_BASS}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::HYPER_BASS}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {mpe::SoundSubCategory::IRISH}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::FLUTE, {}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::TRAVERSO, {mpe::SoundSubCategory::BAROQUE}}, {midi::Program(0, 73)}},
        {{mpe::SoundId::DANSO, {}}, {midi::Program(0, 76)}},
        {{mpe::SoundId::DAEGEUM, {mpe::SoundSubCategory::KOREAN}}, {midi::Program(0, 73)}},

        {{mpe::SoundId::DIZI, {mpe::SoundSubCategory::CHINESE}}, {midi::Program(0, 72)}},

        {{mpe::SoundId::SHAKUHACHI, {mpe::SoundSubCategory::JAPANESE}}, {midi::Program(0, 77)}},
        {{mpe::SoundId::HICHIRIKI, {mpe::SoundSubCategory::JAPANESE}}, {midi::Program(0, 68)}},
        {{mpe::SoundId::FIFE, {}}, {midi::Program(0, 72)}},

        {{mpe::SoundId::WHISTLE, {mpe::SoundSubCategory::TIN}}, {midi::Program(0, 78)}},
        {{mpe::SoundId::WHISTLE, {mpe::SoundSubCategory::SLIDE}}, {midi::Program(0, 79)}},

        {{mpe::SoundId::FLAGEOLET, {}}, {midi::Program(0, 78)}},

        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::GARKLEIN}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::SOPRANINO}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::GREAT_BASS}}, {midi::Program(0, 74)}},
        {{mpe::SoundId::RECORDER, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 74)}},

        {{mpe::SoundId::OCARINA, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 79)}},
        {{mpe::SoundId::OCARINA, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 79)}},
        {{mpe::SoundId::OCARINA, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 79)}},

        {{mpe::SoundId::GEMSHORN, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 79)}},
        {{mpe::SoundId::GEMSHORN, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 79)}},
        {{mpe::SoundId::GEMSHORN, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 79)}},
        {{mpe::SoundId::GEMSHORN, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 79)}},

        {{mpe::SoundId::THEREMIN, {mpe::SoundSubCategory::ELECTRIC}}, {midi::Program(0, 79)}},

        {{mpe::SoundId::PAN_FLUTE, {}}, {midi::Program(0, 75)}},

        {{mpe::SoundId::QUENA, {}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::DULZAINA, {mpe::SoundSubCategory::CASTILIAN}}, {midi::Program(0, 68)}},

        {{mpe::SoundId::HECKELPHONE, {}}, {midi::Program(0, 68)}},
        {{mpe::SoundId::OBOE, {mpe::SoundSubCategory::BAROQUE}}, {midi::Program(0, 69)}},
        {{mpe::SoundId::OBOE, {}}, {midi::Program(0, 68)}},
        {{mpe::SoundId::OBOE, {mpe::SoundSubCategory::ENGLISH}}, {midi::Program(0, 69)}},
        {{mpe::SoundId::OBOE, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 68)}},
        {{mpe::SoundId::LUPOPHONE, {}}, {midi::Program(0, 68)}},

        {{mpe::SoundId::SHAWM, {mpe::SoundSubCategory::SOPRANINO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SHAWM, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SHAWM, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SHAWM, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SHAWM, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SHAWM, {mpe::SoundSubCategory::GREAT_BASS}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::CROMORNE, {mpe::SoundSubCategory::FRENCH, mpe::SoundSubCategory::BAROQUE, mpe::SoundSubCategory::REED}},
         {midi::Program(0, 67)}},

        {{mpe::SoundId::CRUMHORN, {}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CRUMHORN, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CRUMHORN, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CRUMHORN, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CRUMHORN, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CRUMHORN, {mpe::SoundSubCategory::GREAT_BASS}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::CORNAMUSE, {}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CORNAMUSE, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CORNAMUSE, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CORNAMUSE, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CORNAMUSE, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::KELHORN, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::KELHORN, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::KELHORN, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::KELHORN, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::KELHORN, {mpe::SoundSubCategory::GREAT_BASS}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::RAUSCHPFEIFE, {mpe::SoundSubCategory::SOPRANINO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::RAUSCHPFEIFE, {}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::RAUSCHPFEIFE, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::DUDUK, {mpe::SoundSubCategory::ARMENIAN}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::DUDUK, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::ARMENIAN}}, {midi::Program(0, 71)}},

        {{mpe::SoundId::SHENAI, {mpe::SoundSubCategory::INDIAN}}, {midi::Program(0, 111)}},

        {{mpe::SoundId::CLARINET, {}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::SOPRANINO}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::SOPRANINO, mpe::SoundSubCategory::IN_E_FLAT}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::SOPRANO, mpe::SoundSubCategory::IN_B_FLAT}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::IN_B_FLAT}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::CONTRA_ALTO}}, {midi::Program(0, 71)}},
        {{mpe::SoundId::CLARINET, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 71)}},

        {{mpe::SoundId::CHALUMEAU, {}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CHALUMEAU, {mpe::SoundSubCategory::SOPRANINO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CHALUMEAU, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CHALUMEAU, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CHALUMEAU, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::CHALUMEAU, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::XAPHOON, {}}, {midi::Program(0, 71)}},

        {{mpe::SoundId::TAROGATO, {mpe::SoundSubCategory::HUNGARIAN, mpe::SoundSubCategory::ROMANIAN}}, {midi::Program(0, 69)}},

        {{mpe::SoundId::OCTAVIN, {}}, {midi::Program(0, 69)}},

        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::SOPRANISSIMO}}, {midi::Program(0, 64)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::SOPRANINO}}, {midi::Program(0, 64)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 64)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::MEZZO_SOPRANO}}, {midi::Program(0, 65)}},
        {{mpe::SoundId::HECKELPHONE_CLARINET, {}}, {midi::Program(0, 68)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 65)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::MELODY}}, {midi::Program(0, 66)}},
        {{mpe::SoundId::SAXOPHONE, {}}, {midi::Program(0, 66)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 66)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::BARITONE}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SAXOPHONE, {mpe::SoundSubCategory::SUB_CONTRA_BASS}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::BASSOON, {}}, {midi::Program(0, 70)}},
        {{mpe::SoundId::CONTRABASSOON, {}}, {midi::Program(0, 70)}},
        {{mpe::SoundId::CONTRABASSOON, {mpe::SoundSubCategory::REED}}, {midi::Program(0, 70)}},
        {{mpe::SoundId::DULCIAN, {}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::RACKETT, {}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::SARRUSOPHONE, {mpe::SoundSubCategory::SOPRANINO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SARRUSOPHONE, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SARRUSOPHONE, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SARRUSOPHONE, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SARRUSOPHONE, {mpe::SoundSubCategory::BARITONE}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SARRUSOPHONE, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 67)}},
        {{mpe::SoundId::SARRUSOPHONE, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 67)}},

        {{mpe::SoundId::BAGPIPE, {}}, {midi::Program(0, 109)}},
        {{mpe::SoundId::ACCORDION, {}}, {midi::Program(0, 21)}},
        {{mpe::SoundId::BANDONEON, {}}, {midi::Program(0, 23)}},
        {{mpe::SoundId::CONCERTINA, {}}, {midi::Program(0, 21)}},

        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::DIATONIC}}, {midi::Program(0, 22)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::CHROMATIC}}, {midi::Program(0, 22)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::CHROMATIC, mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 22)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::CHROMATIC}}, {midi::Program(0, 22)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::HUANG}}, {midi::Program(0, 22)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::HOHNER}}, {midi::Program(0, 22)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::HUANG}}, {midi::Program(0, 22)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 22)}},

        {{mpe::SoundId::MELODICA, {}}, {midi::Program(0, 22)}},

        {{mpe::SoundId::SHO, {mpe::SoundSubCategory::JAPANESE}}, {midi::Program(0, 20)}},
        {{mpe::SoundId::SHENG, {}}, {midi::Program(0, 20)}},
        {{mpe::SoundId::SHENG, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 20)}},
        {{mpe::SoundId::SHENG, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 20)}},
        {{mpe::SoundId::SHENG, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 20)}},
        {{mpe::SoundId::SHENG, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 20)}},

        {{mpe::SoundId::BRASS_GROUP, {}}, {midi::Program(0, 61)}},

        {{mpe::SoundId::HORN, {}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::IN_A}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::IN_A_FLAT}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::IN_G}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::IN_E}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::IN_E_FLAT}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::IN_D}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::IN_C}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::FRENCH, mpe::SoundSubCategory::IN_F}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::ALTO, mpe::SoundSubCategory::IN_C}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::ALTO, mpe::SoundSubCategory::IN_B_FLAT}},
         {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::IN_B_FLAT}},
         {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::IN_C}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::VIENNA}}, {midi::Program(0, 60), midi::Program(0, 59)}},

        {{mpe::SoundId::TUBA, {mpe::SoundSubCategory::WAGNER}}, {midi::Program(0, 56)}},

        {{mpe::SoundId::CORNET, {}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::CORNET, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 56), midi::Program(0, 59)}},

        {{mpe::SoundId::SAXHORN, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::BARITONE}}, {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::BARITONE, mpe::SoundSubCategory::TREBLE}},
         {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::CENTRAL_EUROPEAN, mpe::SoundSubCategory::BARITONE, mpe::SoundSubCategory::TREBLE}},
         {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::HORN, {mpe::SoundSubCategory::BARITONE, mpe::SoundSubCategory::CENTRAL_EUROPEAN}},
         {midi::Program(0, 60), midi::Program(0, 59)}},
        {{mpe::SoundId::POSTHORN, {}}, {midi::Program(0, 60), midi::Program(0, 59)}},

        {{mpe::SoundId::TRUMPET, {}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::TRUMPET, {mpe::SoundSubCategory::PICCOLO}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::TRUMPET, {mpe::SoundSubCategory::POCKET}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::TRUMPET, {mpe::SoundSubCategory::SLIDE}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::TRUMPET, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::TRUMPET, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::TRUMPET, {mpe::SoundSubCategory::BAROQUE}}, {midi::Program(0, 56), midi::Program(0, 59)}},

        {{mpe::SoundId::EUPHONIUM_BUGLE, {}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::MELLOPHONE_BUGLE, {}}, {midi::Program(0, 57)}},

        {{mpe::SoundId::BUGLE, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::BUGLE, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::BUGLE, {mpe::SoundSubCategory::BARITONE}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::BUGLE, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 58)}},

        {{mpe::SoundId::FISCORN, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 56), midi::Program(0, 59)}},

        {{mpe::SoundId::FLUGELHORN, {}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::KUHLOHORN, {}}, {midi::Program(0, 56), midi::Program(0, 59)}},

        {{mpe::SoundId::OPHICLEIDE, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::OPHICLEIDE, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::OPHICLEIDE, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 58)}},

        {{mpe::SoundId::CORNETTINO, {}}, {midi::Program(0, 56)}},

        {{mpe::SoundId::CORNETT, {}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::CORNETT, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::CORNETT, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::CORNETT, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 56)}},

        {{mpe::SoundId::SERPENT, {}}, {midi::Program(0, 56)}},

        {{mpe::SoundId::TROMBONE, {}}, {midi::Program(0, 57), midi::Program(0, 59)}},
        {{mpe::SoundId::TROMBONE, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 57), midi::Program(0, 59)}},
        {{mpe::SoundId::TROMBONE, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 57), midi::Program(0, 59)}},
        {{mpe::SoundId::TROMBONE, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 57), midi::Program(0, 59)}},
        {{mpe::SoundId::TROMBONE, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 57), midi::Program(0, 59)}},
        {{mpe::SoundId::TROMBONE, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 57), midi::Program(0, 59)}},
        {{mpe::SoundId::TROMBONE, {mpe::SoundSubCategory::TREBLE}}, {midi::Program(0, 56), midi::Program(0, 59)}},

        {{mpe::SoundId::CIMBASSO, {}}, {midi::Program(0, 57)}},

        {{mpe::SoundId::SACKBUT, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 57)}},
        {{mpe::SoundId::SACKBUT, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 57)}},
        {{mpe::SoundId::SACKBUT, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 57)}},

        {{mpe::SoundId::EUPHONIUM, {}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::EUPHONIUM, {mpe::SoundSubCategory::TREBLE}}, {midi::Program(0, 58)}},

        {{mpe::SoundId::TUBA, {}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::TUBA, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::TUBA, {mpe::SoundSubCategory::TREBLE, mpe::SoundSubCategory::BASS}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::TUBA, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::TUBA, {mpe::SoundSubCategory::TREBLE}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::TUBA, {mpe::SoundSubCategory::SUB_CONTRA_BASS}}, {midi::Program(0, 58)}},

        {{mpe::SoundId::SOUSAPHONE, {}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::SOUSAPHONE, {mpe::SoundSubCategory::TREBLE}}, {midi::Program(0, 58)}},
        {{mpe::SoundId::HELICON, {}}, {midi::Program(0, 58)}},

        {{mpe::SoundId::CONCH, {}}, {midi::Program(0, 60)}},

        {{mpe::SoundId::HORAGAI, {mpe::SoundSubCategory::JAPANESE}}, {midi::Program(0, 60)}},

        {{mpe::SoundId::ALPHORN, {mpe::SoundSubCategory::ALPINE}}, {midi::Program(0, 60)}},
        {{mpe::SoundId::RAG_DUNG, {mpe::SoundSubCategory::TIBETAN}}, {midi::Program(0, 56), midi::Program(0, 59)}},
        {{mpe::SoundId::DIDGERIDOO, {mpe::SoundSubCategory::AUSTRALIAN}}, {midi::Program(0, 57)}},
        {{mpe::SoundId::SHOFAR, {}}, {midi::Program(0, 60)}},
        {{mpe::SoundId::VUVUZELA, {}}, {midi::Program(0, 56)}},
        {{mpe::SoundId::KLAXON_HORNS, {}}, {midi::Program(0, 84)}},
        {{mpe::SoundId::KAZOO, {}}, {midi::Program(0, 85)}}};

    static const std::map<SoundMappingKey, midi::Programs> percussion_mappings = {
        {{mpe::SoundId::TIMPANI, {}}, {midi::Program(0, 47)}},
        {{mpe::SoundId::ROTO_TOMS, {}}, {midi::Program(0, 117)}},
        {{mpe::SoundId::TUBAPHONE, {mpe::SoundSubCategory::METAL}}, {midi::Program(0, 12)}},

        {{mpe::SoundId::STEEL_DRUMS, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::SOPRANO}},
         {midi::Program(0, 114)}},
        {{mpe::SoundId::STEEL_DRUMS, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::ALTO}},
         {midi::Program(0, 114)}},
        {{mpe::SoundId::STEEL_DRUMS, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::TENOR}},
         {midi::Program(0, 114)}},
        {{mpe::SoundId::STEEL_DRUMS, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::STEEL}}, {midi::Program(0, 114)}},
        {{mpe::SoundId::STEEL_DRUMS, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::FOUR_PIECE}},
         {midi::Program(0, 114)}},
        {{mpe::SoundId::STEEL_DRUMS,
          {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::TENOR, mpe::SoundSubCategory::BASS}},
         {midi::Program(0, 114)}},
        {{mpe::SoundId::STEEL_DRUMS, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::STEEL, mpe::SoundSubCategory::BASS}},
         {midi::Program(0, 114)}},

        {{mpe::SoundId::VIBRAPHONE, {}}, {midi::Program(0, 11)}},
        {{mpe::SoundId::DULCIMER, {}}, {midi::Program(0, 15)}},
        {{mpe::SoundId::CIMBALOM, {}}, {midi::Program(0, 15)}},

        {{mpe::SoundId::XYLOMARIMBA, {}}, {midi::Program(0, 12)}},
        {{mpe::SoundId::MARIMBA, {}}, {midi::Program(0, 12)}},
        {{mpe::SoundId::MARIMBA, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 12)}},
        {{mpe::SoundId::MARIMBA, {mpe::SoundSubCategory::CONTRA_BASS}}, {midi::Program(0, 12)}},

        {{mpe::SoundId::CROTALES, {mpe::SoundSubCategory::METAL}}, {midi::Program(0, 10)}},

        {{mpe::SoundId::CARILLON, {mpe::SoundSubCategory::METAL}}, {midi::Program(0, 14)}},
        {{mpe::SoundId::GONG, {mpe::SoundSubCategory::METAL}}, {midi::Program(0, 14)}},
        {{mpe::SoundId::GONG, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::WIND}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::GONG, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::OPERA}}, {midi::Program(0, 14)}},

        {{mpe::SoundId::GLOCKENSPIEL, {}}, {midi::Program(0, 9)}},
        {{mpe::SoundId::GLOCKENSPIEL, {mpe::SoundSubCategory::SOPRANO, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 9)}},
        {{mpe::SoundId::GLOCKENSPIEL, {mpe::SoundSubCategory::ALTO, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 9)}},

        {{mpe::SoundId::METALLOPHONE, {mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 11)}},
        {{mpe::SoundId::METALLOPHONE, {mpe::SoundSubCategory::SOPRANO, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 11)}},
        {{mpe::SoundId::METALLOPHONE, {mpe::SoundSubCategory::ALTO, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 11)}},
        {{mpe::SoundId::METALLOPHONE, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 11)}},

        {{mpe::SoundId::XYLOPHONE, {}}, {midi::Program(0, 13)}},
        {{mpe::SoundId::XYLOPHONE, {mpe::SoundSubCategory::SOPRANO, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 13)}},
        {{mpe::SoundId::XYLOPHONE, {mpe::SoundSubCategory::ALTO, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 13)}},
        {{mpe::SoundId::XYLOPHONE, {mpe::SoundSubCategory::BASS, mpe::SoundSubCategory::ORFF}}, {midi::Program(0, 12)}},

        {{mpe::SoundId::FLEXATONE, {mpe::SoundSubCategory::METAL}}, {midi::Program(0, 14)}},

        {{mpe::SoundId::MUSICAL_SAW, {mpe::SoundSubCategory::METAL}}, {midi::Program(0, 92)}},

        {{mpe::SoundId::MUSICAL_GLASSES, {mpe::SoundSubCategory::GLASS}}, {midi::Program(0, 93)}},
        {{mpe::SoundId::HARMONICA, {mpe::SoundSubCategory::GLASS}}, {midi::Program(0, 92)}},

        {{mpe::SoundId::KALIMBA, {}}, {midi::Program(0, 108)}},
        {{mpe::SoundId::KALIMBA, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 108)}},
        {{mpe::SoundId::KALIMBA, {mpe::SoundSubCategory::TREBLE}}, {midi::Program(0, 108)}},
        {{mpe::SoundId::KALIMBA, {mpe::SoundSubCategory::METAL}}, {midi::Program(0, 108)}},

        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::BRAKE}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::BONGOS, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::TOM_TOMS, {mpe::SoundSubCategory::CHINESE}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::TOM_TOMS, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::KO_TSUZUMI, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::O_TSUZUMI, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::KAKKO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::SHIME_DAIKO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::MIYA_DAIKO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::TSURI_DAIKO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::OKEDO_DAIKO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::KANE, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::SHOKO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::JANGGU, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::BUK, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::SAMUL_BUK, {mpe::SoundSubCategory::KOREAN}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::SAMUL_JANGGU, {mpe::SoundSubCategory::KOREAN}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::SANJO_JANGGU, {mpe::SoundSubCategory::KOREAN}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::SORI_BUK, {mpe::SoundSubCategory::KOREAN}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::SOGO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::KKWAENGGWARI, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::JING, {}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::CONGA, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::XIAOGU, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::BANGU, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DAGU, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DALUO, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::XIAOLUO, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::BAN, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DABO, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::NAOBO, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::BANGZI, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DJEMBE, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DOUMBEK, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::CUICA, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CAJON, {}}, {midi::Program(128, 40)}},

        {{mpe::SoundId::DRUMSET, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::DRUMSET, {mpe::SoundSubCategory::FOUR_PIECE}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::DRUMSET, {mpe::SoundSubCategory::FIVE_PIECE}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::DRUMSET, {mpe::SoundSubCategory::ORCHESTRAL}}, {midi::Program(128, 48)}},

        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::BASS}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::SNARE}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::MILITARY}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::FRAME}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::SNARE, mpe::SoundSubCategory::PICCOLO}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::SLIT}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::TABLAS, {mpe::SoundSubCategory::INDIAN}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::TIMBALES, {}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::ANVIL, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::BELL_TREE, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::GONG, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::BOWL}}, {midi::Program(0, 11)}},
        {{mpe::SoundId::CHAIN, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::BELL, {mpe::SoundSubCategory::HAND}}, {midi::Program(0, 112)}},
        {{mpe::SoundId::CHIMES, {mpe::SoundSubCategory::HAND}}, {midi::Program(0, 112)}},
        {{mpe::SoundId::BELL, {mpe::SoundSubCategory::PLATE, mpe::SoundSubCategory::METAL}}, {midi::Program(0, 11)}},
        {{mpe::SoundId::BELL, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::BELL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::COW}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::BELL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::AGOGO}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::BELL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::SLEIGH}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::CYMBAL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::CRASH}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::CYMBAL, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::CYMBAL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::FINGER}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CYMBAL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::CHINA}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CYMBAL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::SPLASH}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CYMBAL, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::RIDE}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::HI_HAT, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::PIPE, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::IRON}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::MARK_TREE, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CASTANET, {mpe::SoundSubCategory::WOODEN}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CASTANET, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 48)}},

        {{mpe::SoundId::CHIMES, {}}, {midi::Program(0, 14)}},
        {{mpe::SoundId::CHIMES, {mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::WIND}}, {midi::Program(0, 14)}},
        {{mpe::SoundId::CHIMES, {mpe::SoundSubCategory::WOODEN, mpe::SoundSubCategory::WIND}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CHIMES, {mpe::SoundSubCategory::GLASS, mpe::SoundSubCategory::WIND}}, {midi::Program(0, 14)}},
        {{mpe::SoundId::CHIMES, {mpe::SoundSubCategory::SHELL, mpe::SoundSubCategory::WIND}}, {midi::Program(0, 14)}},

        {{mpe::SoundId::TAM_TAM, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::THUNDERSHEET, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::TRIANGLE, {mpe::SoundSubCategory::METAL}}, {midi::Program(128, 48)}},

        {{mpe::SoundId::CLAVES, {mpe::SoundSubCategory::WOODEN}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::GUIRO, {mpe::SoundSubCategory::WOODEN}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::BLOCK, {mpe::SoundSubCategory::WOODEN, mpe::SoundSubCategory::TEMPLE}}, {midi::Program(1, 115)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::WOODEN, mpe::SoundSubCategory::LOG}}, {midi::Program(1, 115)}},
        {{mpe::SoundId::BLOCK, {mpe::SoundSubCategory::WOODEN}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::BLOCK, {mpe::SoundSubCategory::SANDPAPER}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::CABASA, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::MARACA, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::QUIJADA, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::RATCHET, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::SHAKER, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::SHEKERE, {}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::STONES, {}}, {midi::Program(0, 13)}},
        {{mpe::SoundId::TAMBOURINE, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::TUBO, {}}, {midi::Program(128, 48)}},
        {{mpe::SoundId::VIBRASLAP, {}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::WHIP, {}}, {midi::Program(128, 56)}},
        {{mpe::SoundId::CANNON, {}}, {midi::Program(0, 127)}},
        {{mpe::SoundId::BIRD_CALL, {}}, {midi::Program(0, 123)}},
        {{mpe::SoundId::DRUM,
          {
              mpe::SoundSubCategory::OCEAN,
          }},
         {midi::Program(0, 122)}},

        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::MARCHING, mpe::SoundSubCategory::SNARE}}, {midi::Program(128, 56)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::MARCHING, mpe::SoundSubCategory::SNARE, mpe::SoundSubCategory::TENOR}},
         {midi::Program(128, 96)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::SHOW_STYLE, mpe::SoundSubCategory::SNARE, mpe::SoundSubCategory::TENOR}},
         {midi::Program(128, 0)}},
        {{mpe::SoundId::DRUM, {mpe::SoundSubCategory::MARCHING, mpe::SoundSubCategory::SNARE, mpe::SoundSubCategory::BASS}},
         {midi::Program(128, 59)}},

        {{mpe::SoundId::CYMBAL, {mpe::SoundSubCategory::MARCHING, mpe::SoundSubCategory::METAL, mpe::SoundSubCategory::CRASH}},
         {midi::Program(128, 58)}},

        {{mpe::SoundId::SNAP, {mpe::SoundSubCategory::FINGER}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::CLAP, {mpe::SoundSubCategory::HAND}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::SLAP, {mpe::SoundSubCategory::HAND}}, {midi::Program(128, 0)}},
        {{mpe::SoundId::STAMP, {mpe::SoundSubCategory::FOOT}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::TAIKO, {}}, {midi::Program(128, 0)}},

        {{mpe::SoundId::SYNTHESIZER, {mpe::SoundSubCategory::ELECTRIC, mpe::SoundSubCategory::PERCUSSIVE}}, {midi::Program(128, 0)}}};

    static const std::map<SoundMappingKey, midi::Programs> voice_mappings = {
        {{mpe::SoundId::CHOIR, {}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::SOPRANO, mpe::SoundSubCategory::BOY}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::SOPRANO}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::MEZZO_SOPRANO}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::COUNTER_TENOR}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::ALTO}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::CONTRA_ALTO}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::TENOR}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::BARITONE}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::BASS}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::FEMALE}}, {midi::Program(0, 52)}},
        {{mpe::SoundId::CHOIR, {mpe::SoundSubCategory::MALE}}, {midi::Program(0, 52)}}};

    switch (category) {
        case mpe::SoundCategory::KEYBOARDS: return keyboards_mappings;
        case mpe::SoundCategory::STRINGS: return strings_mappings;
        case mpe::SoundCategory::WINDS: return winds_mappings;
        case mpe::SoundCategory::PERCUSSIONS: return percussion_mappings;
        case mpe::SoundCategory::VOICES: return voice_mappings;
        default: static std::map<SoundMappingKey, midi::Programs> empty; return empty;
    }
}

inline const midi::Programs &find_programs(const mpe::PlaybackSetupData &setup_data) {
    const std::map<SoundMappingKey, midi::Programs> &mapping = mapping_by_category(setup_data.category);

    mpe::SoundSubCategories sub_category_set = setup_data.sound_sub_categories();
    muse::remove(sub_category_set, mpe::SoundSubCategory::PRIMARY);
    muse::remove(sub_category_set, mpe::SoundSubCategory::SECONDARY);

    mpe::SoundId sound_id = setup_data.sound_id();
    auto search = mapping.find({sound_id, sub_category_set});

    if (search != mapping.cend()) {
        return search->second;
    }

    static const midi::Programs fallback{midi::Program(0, 0)};
    return fallback;
}

inline const ArticulationMapping &articulation_sounds(const mpe::PlaybackSetupData &setup_data) {
    static const ArticulationMapping electric_guitar = {
        {mpe::ArticulationType::JAZZ_TONE, midi::Program(0, 26)},  {mpe::ArticulationType::PALM_MUTE, midi::Program(0, 28)},
        {mpe::ArticulationType::MUTE, midi::Program(0, 28)},       {mpe::ArticulationType::HARMONIC, midi::Program(0, 31)},
        {mpe::ArticulationType::DISTORTION, midi::Program(0, 30)}, {mpe::ArticulationType::OVERDRIVE, midi::Program(0, 29)}};

    static const ArticulationMapping acoustic_guitar = {{mpe::ArticulationType::JAZZ_TONE, midi::Program(0, 26)},
                                                        {mpe::ArticulationType::PALM_MUTE, midi::Program(0, 28)},
                                                        {mpe::ArticulationType::MUTE, midi::Program(0, 28)},
                                                        {mpe::ArticulationType::HARMONIC, midi::Program(8, 31)}};

    static const ArticulationMapping electric_bass_guitar = {{mpe::ArticulationType::SLAP, midi::Program(0, 36)},
                                                             {mpe::ArticulationType::POP, midi::Program(0, 37)},
                                                             {mpe::ArticulationType::PALM_MUTE, midi::Program(0, 28)},
                                                             {mpe::ArticulationType::MUTE, midi::Program(0, 28)}};

    static const ArticulationMapping acoustic_bass_guitar = {{mpe::ArticulationType::SLAP, midi::Program(0, 36)},
                                                             {mpe::ArticulationType::POP, midi::Program(0, 37)},
                                                             {mpe::ArticulationType::PIZZICATO, midi::Program(0, 32)},
                                                             {mpe::ArticulationType::PALM_MUTE, midi::Program(0, 32)},
                                                             {mpe::ArticulationType::MUTE, midi::Program(0, 32)}};

    static const ArticulationMapping basic_string_section = {{mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(0, 45)},
                                                             {mpe::ArticulationType::PIZZICATO, midi::Program(0, 45)},
                                                             {mpe::ArticulationType::PALM_MUTE, midi::Program(0, 45)},
                                                             {mpe::ArticulationType::MUTE, midi::Program(0, 49)}};

    static const ArticulationMapping basic_viol_section = {
        {mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(0, 45)}, {mpe::ArticulationType::PIZZICATO, midi::Program(0, 45)},
        {mpe::ArticulationType::PALM_MUTE, midi::Program(0, 45)},      {mpe::ArticulationType::MUTE, midi::Program(0, 49)},
        {mpe::ArticulationType::TREMOLO8TH, midi::Program(0, 44)},     {mpe::ArticulationType::TREMOLO16TH, midi::Program(0, 44)},
        {mpe::ArticulationType::TREMOLO32ND, midi::Program(0, 44)},    {mpe::ArticulationType::TREMOLO64TH, midi::Program(0, 44)},
    };

    static const ArticulationMapping violin_section = {
        {mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(20, 45)}, {mpe::ArticulationType::PIZZICATO, midi::Program(20, 45)},
        {mpe::ArticulationType::PALM_MUTE, midi::Program(20, 45)},      {mpe::ArticulationType::MUTE, midi::Program(20, 49)},
        {mpe::ArticulationType::TREMOLO8TH, midi::Program(20, 44)},     {mpe::ArticulationType::TREMOLO16TH, midi::Program(20, 44)},
        {mpe::ArticulationType::TREMOLO32ND, midi::Program(20, 44)},    {mpe::ArticulationType::TREMOLO64TH, midi::Program(20, 44)},
    };

    static const ArticulationMapping violin = {{mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(20, 45)},
                                               {mpe::ArticulationType::PIZZICATO, midi::Program(20, 45)},
                                               {mpe::ArticulationType::PALM_MUTE, midi::Program(20, 45)},
                                               {mpe::ArticulationType::MUTE, midi::Program(20, 40)}};

    static const ArticulationMapping viola_section = {
        {mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(30, 45)}, {mpe::ArticulationType::PIZZICATO, midi::Program(30, 45)},
        {mpe::ArticulationType::PALM_MUTE, midi::Program(30, 45)},      {mpe::ArticulationType::MUTE, midi::Program(30, 49)},
        {mpe::ArticulationType::TREMOLO8TH, midi::Program(30, 44)},     {mpe::ArticulationType::TREMOLO16TH, midi::Program(30, 44)},
        {mpe::ArticulationType::TREMOLO32ND, midi::Program(30, 44)},    {mpe::ArticulationType::TREMOLO64TH, midi::Program(30, 44)},
    };

    static const ArticulationMapping viola = {{mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(30, 45)},
                                              {mpe::ArticulationType::PIZZICATO, midi::Program(30, 45)},
                                              {mpe::ArticulationType::PALM_MUTE, midi::Program(30, 45)},
                                              {mpe::ArticulationType::MUTE, midi::Program(30, 41)}};

    static const ArticulationMapping violoncello_section = {
        {mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(40, 45)}, {mpe::ArticulationType::PIZZICATO, midi::Program(40, 45)},
        {mpe::ArticulationType::PALM_MUTE, midi::Program(40, 45)},      {mpe::ArticulationType::MUTE, midi::Program(40, 49)},
        {mpe::ArticulationType::TREMOLO8TH, midi::Program(40, 44)},     {mpe::ArticulationType::TREMOLO16TH, midi::Program(40, 44)},
        {mpe::ArticulationType::TREMOLO32ND, midi::Program(40, 44)},    {mpe::ArticulationType::TREMOLO64TH, midi::Program(40, 44)},
    };

    static const ArticulationMapping violoncello = {{mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(40, 45)},
                                                    {mpe::ArticulationType::PIZZICATO, midi::Program(40, 45)},
                                                    {mpe::ArticulationType::PALM_MUTE, midi::Program(40, 45)},
                                                    {mpe::ArticulationType::MUTE, midi::Program(40, 49)}};

    static const ArticulationMapping contrabass_section = {
        {mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(50, 45)}, {mpe::ArticulationType::PIZZICATO, midi::Program(50, 45)},
        {mpe::ArticulationType::PALM_MUTE, midi::Program(50, 45)},      {mpe::ArticulationType::MUTE, midi::Program(50, 49)},
        {mpe::ArticulationType::TREMOLO8TH, midi::Program(50, 44)},     {mpe::ArticulationType::TREMOLO16TH, midi::Program(50, 44)},
        {mpe::ArticulationType::TREMOLO32ND, midi::Program(50, 44)},    {mpe::ArticulationType::TREMOLO64TH, midi::Program(50, 44)},
    };

    static const ArticulationMapping contrabass = {
        {mpe::ArticulationType::SNAP_PIZZICATO, midi::Program(50, 45)},
        {mpe::ArticulationType::PIZZICATO, midi::Program(50, 45)},
        {mpe::ArticulationType::PALM_MUTE, midi::Program(50, 45)},
        {mpe::ArticulationType::MUTE, midi::Program(50, 43)},
    };

    static const ArticulationMapping brass = {{mpe::ArticulationType::MUTE, midi::Program(0, 59)}};

    mpe::SoundId sound_id = setup_data.sound_id();

    if (sound_id == mpe::SoundId::GUITAR) {
        if (setup_data.contains(mpe::SoundSubCategory::ACOUSTIC)) {
            return acoustic_guitar;
        }

        if (setup_data.contains(mpe::SoundSubCategory::ELECTRIC)) {
            return electric_guitar;
        }
    }

    if (sound_id == mpe::SoundId::BASS_GUITAR) {
        if (setup_data.contains(mpe::SoundSubCategory::ACOUSTIC)) {
            return acoustic_bass_guitar;
        }

        if (setup_data.contains(mpe::SoundSubCategory::ELECTRIC)) {
            return electric_bass_guitar;
        }
    }

    if (sound_id == mpe::SoundId::VIOLIN && setup_data.contains(mpe::SoundSubCategory::SECTION)) {
        return violin_section;
    }

    if (sound_id == mpe::SoundId::VIOLIN) {
        return violin;
    }

    if (sound_id == mpe::SoundId::VIOLA && setup_data.contains(mpe::SoundSubCategory::SECTION)) {
        return viola_section;
    }

    if (sound_id == mpe::SoundId::VIOLA) {
        return viola;
    }

    if (sound_id == mpe::SoundId::VIOLONCELLO && setup_data.contains(mpe::SoundSubCategory::SECTION)) {
        return violoncello_section;
    }

    if (sound_id == mpe::SoundId::VIOLONCELLO) {
        return violoncello;
    }

    if (sound_id == mpe::SoundId::CONTRABASS && setup_data.contains(mpe::SoundSubCategory::SECTION)) {
        return contrabass_section;
    }

    if (sound_id == mpe::SoundId::CONTRABASS) {
        return contrabass;
    }

    if (setup_data.category == mpe::SoundCategory::STRINGS) {
        static const std::unordered_set<mpe::SoundId> viol_section{mpe::SoundId::VIOL, mpe::SoundId::PARDESSUS_VIOL,
                                                                   mpe::SoundId::VIOLA_DA_GAMBA, mpe::SoundId::VIOLONE};

        if (muse::contains(viol_section, sound_id)) {
            return basic_viol_section;
        }

        return basic_string_section;
    }

    if (setup_data.category == mpe::SoundCategory::WINDS) {
        static const std::unordered_set<mpe::SoundId> brass_section{mpe::SoundId::BUGLE,   mpe::SoundId::EUPHONIUM, mpe::SoundId::HORN,
                                                                    mpe::SoundId::TRUMPET, mpe::SoundId::TROMBONE,  mpe::SoundId::TUBA};

        if (muse::contains(brass_section, sound_id)) {
            return brass;
        }
    }

    static ArticulationMapping empty;
    return empty;
}

static const mpe::ArticulationTypeSet sustain_pedal_cc_supported_types{mpe::ArticulationType::PEDAL};

static const mpe::ArticulationTypeSet sostenuto_pedal_cc_supported_types{
    mpe::ArticulationType::LAISSEZ_VIBRER,
};

static const mpe::ArticulationTypeSet bend_supported_types = {
    mpe::ArticulationType::BRASS_BEND,     mpe::ArticulationType::MULTIBEND,
    mpe::ArticulationType::SLIDE_OUT_UP,   mpe::ArticulationType::CONTINUOUS_GLISSANDO,
    mpe::ArticulationType::FALL,           mpe::ArticulationType::QUICK_FALL,
    mpe::ArticulationType::DOIT,           mpe::ArticulationType::PLOP,
    mpe::ArticulationType::SCOOP,          mpe::ArticulationType::SLIDE_OUT_DOWN,
    mpe::ArticulationType::SLIDE_IN_ABOVE, mpe::ArticulationType::SLIDE_IN_BELOW};

struct ChannelMap {
    using ChannelMapping = std::pair<midi::channel_t, midi::Program>;
    using VoiceMappings = std::map<mpe::ArticulationType, ChannelMapping>;

    void init(const mpe::PlaybackSetupData &setup_data, const std::optional<midi::Program> &program_override,
              const midi::channel_t max_channels) {
        max_channels_ = max_channels;
        program_override_ = program_override;
        uses_staff_layers_ = setup_data.supports_single_note_dynamics;

        if (program_override_.has_value()) {
            resolve_channel(0, mpe::ArticulationType::STANDARD, program_override_.value());
        }

        standard_programs_ = find_programs(setup_data);
        articulation_mapping_ = articulation_sounds(setup_data);

        if (standard_programs_.empty()) {
            return;
        }

        //!Note ensuring that the default channel being pre-initialized
        const midi::Program &standard_program = standard_programs_.at(0);
        resolve_channel(0, mpe::ArticulationType::STANDARD, standard_program);
    }

    midi::channel_t resolve_channel_for_event(const mpe::NoteEvent &event) {
        //! NOTE: without single-note dynamics, voices don't need their own channel per staff
        const mpe::staff_layer_idx_t staff_idx = uses_staff_layers_ ? event.arrangement_ctx().staff_layer_index : 0;
        const mpe::layer_idx_t layer_idx = mpe::make_layer_idx(staff_idx, event.arrangement_ctx().voice_layer_index);

        if (program_override_.has_value()) {
            return resolve_channel(layer_idx, mpe::ArticulationType::STANDARD, program_override_.value());
        }

        if (standard_programs_.empty()) {
            return 0;
        }

        const midi::Program &standard_program = standard_programs_.at(0);

        //! NOTE: always resolve the layer's own channel first: it must exist so that articulation
        //! channels below have somewhere to fall back to once the channel budget runs out
        const midi::channel_t standard_channel = resolve_channel(layer_idx, mpe::ArticulationType::STANDARD, standard_program);

        if (event.expression_ctx().articulations.contains(mpe::ArticulationType::STANDARD) ||
            event.expression_ctx().articulations.empty() || articulation_mapping_.empty()) {
            return standard_channel;
        }

        for (const auto &pair : event.expression_ctx().articulations) {
            auto search = articulation_mapping_.find(pair.first);
            if (search == articulation_mapping_.cend()) {
                continue;
            }

            return resolve_channel(layer_idx, search->first, search->second, standard_channel, standard_program);
        }

        return standard_channel;
    }

    midi::channel_t channel_count() const { return channel_count_; }

    const std::map<mpe::layer_idx_t, VoiceMappings> &data() const { return data_; }

    // Called when a new channel is mapped, before its first event; the
    // synthesizer that owns the mapping programs the channel.
    std::function<void(midi::channel_t, const midi::Program &)> on_channel_added;

private:
    //! Resolves the channel for a (layer, articulation) pair. Same-program articulations share a channel;
    //! once maxChannels_ is exhausted, new ones fold onto the layer's own channel (fallbackChannel/fallbackProgram)
    midi::channel_t resolve_channel(const mpe::layer_idx_t layer_idx, const mpe::ArticulationType type, const midi::Program &program,
                                    const std::optional<midi::channel_t> &fallback_channel = std::nullopt,
                                    const std::optional<midi::Program> &fallback_program = std::nullopt) {
        VoiceMappings &mapping = data_[layer_idx];

        auto it = mapping.find(type);
        if (it != mapping.cend()) {
            return it->second.first;
        }

        for (const auto &entry : mapping) {
            if (entry.second.second == program) {
                mapping.insert({type, entry.second});
                return entry.second.first;
            }
        }

        if (channel_count_ < max_channels_) {
            const midi::channel_t channel_idx = channel_count_++;
            mapping.insert({type, {channel_idx, program}});
            if (on_channel_added) {
                on_channel_added(channel_idx, program);
            }
            return channel_idx;
        }

        //! NOTE: channel budget exhausted; prefer reusing a channel that already has this program
        //! over folding onto fallbackChannel, which may carry an unrelated layer's program
        for (const auto &[other_layer_idx, other_mapping] : data_) {
            if (other_layer_idx == layer_idx) {
                continue;
            }

            for (const auto &entry : other_mapping) {
                if (entry.second.second == program) {
                    mapping.insert({type, entry.second});
                    return entry.second.first;
                }
            }
        }

        const midi::channel_t channel_idx = fallback_channel.value_or(0);
        const midi::Program actual_program = fallback_program.value_or(program);
        mapping.insert({type, {channel_idx, actual_program}});

        LOGW() << "Channel budget of " << max_channels_ << " exhausted: layer " << layer_idx << ", articulation " << static_cast<int>(type)
               << " reused channel " << channel_idx << " (wanted program " << program.bank << "/" << program.program << ", got "
               << actual_program.bank << "/" << actual_program.program << ")";

        return channel_idx;
    }

    std::map<mpe::layer_idx_t, VoiceMappings> data_;
    midi::channel_t channel_count_ = 0;
    midi::channel_t max_channels_ = 16;

    bool uses_staff_layers_ = false;
    std::optional<midi::Program> program_override_;
    midi::Programs standard_programs_;
    ArticulationMapping articulation_mapping_;
};
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_SOUNDMAPPING_H
