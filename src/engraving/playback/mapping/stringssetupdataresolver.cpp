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

#include "stringssetupdataresolver.h"

#include <array>

#include "playback/utils/sortedtable.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

PlaybackSetupData StringsSetupDataResolver::do_resolve(const Instrument *instrument) {
    static const auto setup_data_map = mu::engraving::make_sorted_table<std::string, mpe::PlaybackSetupData>({
        {"harp", {SoundId::HARP, SoundCategory::STRINGS, {mpe::SoundSubCategory::PLUCKED}}},
        {"cavaquinho",
         {SoundId::CAVAQUINHO, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::STEEL, SoundSubCategory::PLUCKED}}},
        {"cavaquinho-tablature",
         {SoundId::CAVAQUINHO, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::STEEL, SoundSubCategory::PLUCKED}}},
        {"soprano-guitar",
         {SoundId::GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::SOPRANO, SoundSubCategory::PLUCKED}}},
        {"alto-guitar",
         {SoundId::GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::ALTO, SoundSubCategory::PLUCKED}}},
        {"baritone-guitar",
         {SoundId::GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::BARITONE, SoundSubCategory::PLUCKED}}},
        {"contra-guitar",
         {SoundId::GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::CONTRA, SoundSubCategory::PLUCKED}}},

        {"electric-guitar", {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"electric-guitar-treble-clef", {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"electric-guitar-tablature", {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},

        {"guitar-steel",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::STEEL, SoundSubCategory::PLUCKED}}},
        {"guitar-steel-treble-clef",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::STEEL, SoundSubCategory::PLUCKED}}},
        {"guitar-steel-tablature",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::STEEL, SoundSubCategory::PLUCKED}}},
        {"pedal-steel-guitar",
         {SoundId::GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::PEDAL, SoundSubCategory::STEEL, SoundSubCategory::PLUCKED}}},

        {"guitar-nylon",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::PLUCKED}}},
        {"guitar-nylon-treble-clef",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::PLUCKED}}},
        {"guitar-nylon-tablature",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::PLUCKED}}},
        {"7-string-guitar",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::PLUCKED}}},
        {"7-string-guitar-tablature",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::PLUCKED}}},
        {"11-string-alto-guitar",
         {SoundId::GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::ALTO, SoundSubCategory::PLUCKED}}},
        {"12-string-guitar",
         {SoundId::GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::STEEL, SoundSubCategory::TWELVE_STRING, SoundSubCategory::PLUCKED}}},

        {"5-string-electric-bass-high-c",
         {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"5-string-electric-bass-tab-high-c",
         {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"5-string-electric-bass-tab",
         {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"5-string-electric-bass", {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"6-string-electric-bass", {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"6-string-electric-bass-tab",
         {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"bass-guitar", {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"bass-guitar-tablature", {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"electric-bass-4-str-tab",
         {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"electric-bass", {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}}},
        {"fretless-electric-bass",
         {SoundId::BASS_GUITAR,
          SoundCategory::STRINGS,
          {SoundSubCategory::ELECTRIC, SoundSubCategory::FRETLESS, SoundSubCategory::PLUCKED}}},
        {"acoustic-bass", {SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::PLUCKED}}},

        {"banjo", {SoundId::BANJO, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"tenor-banjo", {SoundId::BANJO, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"banjo-tablature", {SoundId::BANJO, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"irish-tenor-banjo",
         {SoundId::BANJO, SoundCategory::STRINGS, {SoundSubCategory::IRISH, SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"irish-tenor-banjo-tablature",
         {SoundId::BANJO, SoundCategory::STRINGS, {SoundSubCategory::IRISH, SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},

        {"ukulele", {SoundId::UKULELE, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"ukulele-4-str-tab", {SoundId::UKULELE, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"ukulele-low-g", {SoundId::UKULELE, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"tenor-ukulele", {SoundId::UKULELE, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"baritone-ukulele", {SoundId::UKULELE, SoundCategory::STRINGS, {SoundSubCategory::BARITONE, SoundSubCategory::PLUCKED}}},

        {"mandolin", {SoundId::MANDOLIN, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"mandolin-tablature", {SoundId::MANDOLIN, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"alto-mandola", {SoundId::MANDOLIN, SoundCategory::STRINGS, {SoundSubCategory::ALTO, SoundSubCategory::PLUCKED}}},
        {"mandola", {SoundId::MANDOLIN, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"tenor-mandola", {SoundId::MANDOLIN, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"octave-mandolin", {SoundId::MANDOLIN, SoundCategory::STRINGS, {SoundSubCategory::OCTAVE, SoundSubCategory::PLUCKED}}},
        {"mandocello", {SoundId::MANDOLIN, SoundCategory::STRINGS, {SoundSubCategory::OCTAVE, SoundSubCategory::PLUCKED}}},

        {"mtn-dulcimer-std", {SoundId::MTN_DULCIMER, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"mtn-dulcimer-std-chrom-tab", {SoundId::MTN_DULCIMER, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"mtn-dulcimer-baritone",
         {SoundId::MTN_DULCIMER,
          SoundCategory::STRINGS,
          {
              SoundSubCategory::BARITONE,
              SoundSubCategory::PLUCKED,
          }}},
        {"mtn-dulcimer-bartn-chrom-tab",
         {SoundId::MTN_DULCIMER, SoundCategory::STRINGS, {SoundSubCategory::BARITONE, SoundSubCategory::PLUCKED}}},
        {"mtn-dulcimer-bass", {SoundId::MTN_DULCIMER, SoundCategory::STRINGS, {SoundSubCategory::BASS, SoundSubCategory::PLUCKED}}},
        {"mtn-dulcimer-bass-chrom-tab",
         {SoundId::MTN_DULCIMER, SoundCategory::STRINGS, {SoundSubCategory::BASS, SoundSubCategory::PLUCKED}}},

        {"lute", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"lute-tablature", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"ren.-tenor-lute-5-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"ren.-tenor-lute-6-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"ren.-tenor-lute-7-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"ren.-tenor-lute-8-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"ren.-tenor-lute-9-course",
         {SoundId::LUTE,
          SoundCategory::STRINGS,
          {
              SoundSubCategory::TENOR,
              SoundSubCategory::PLUCKED,
          }}},
        {"ren.-tenor-lute-10-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::TENOR, SoundSubCategory::PLUCKED}}},
        {"baroque-lute-13-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::BAROQUE, SoundSubCategory::PLUCKED}}},
        {"archlute-14-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"bouzouki-3-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::GREEK, SoundSubCategory::PLUCKED}}},
        {"bouzouki-4-course", {SoundId::LUTE, SoundCategory::STRINGS, {SoundSubCategory::GREEK, SoundSubCategory::PLUCKED}}},

        {"theorbo-14-course", {SoundId::THEORBO, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},

        {"balalaika", {SoundId::BALALAIKA, SoundCategory::STRINGS, {SoundSubCategory::PRIMA, SoundSubCategory::PLUCKED}}},
        {"balalaika-piccolo", {SoundId::BALALAIKA, SoundCategory::STRINGS, {SoundSubCategory::PICCOLO, SoundSubCategory::PLUCKED}}},
        {"balalaika-prima", {SoundId::BALALAIKA, SoundCategory::STRINGS, {SoundSubCategory::PRIMA, SoundSubCategory::PLUCKED}}},
        {"balalaika-alto",
         {SoundId::BALALAIKA, SoundCategory::STRINGS, {SoundSubCategory::PRIMA, SoundSubCategory::ALTO, SoundSubCategory::PLUCKED}}},
        {"balalaika-bass",
         {SoundId::BALALAIKA, SoundCategory::STRINGS, {SoundSubCategory::PRIMA, SoundSubCategory::BASS, SoundSubCategory::PLUCKED}}},
        {"balalaika-contrabass",
         {SoundId::BALALAIKA, SoundCategory::STRINGS, {SoundSubCategory::PRIMA, SoundSubCategory::CONTRA_BASS, SoundSubCategory::PLUCKED}}},
        {"balalaika-secunda", {SoundId::BALALAIKA, SoundCategory::STRINGS, {SoundSubCategory::SECUNDA, SoundSubCategory::PLUCKED}}},

        {"guzheng", {SoundId::GUZHENG, SoundCategory::STRINGS, {SoundSubCategory::CHINESE, SoundSubCategory::PLUCKED}}},
        {"guqin", {SoundId::GUQIN, SoundCategory::STRINGS, {SoundSubCategory::CHINESE, SoundSubCategory::PLUCKED}}},
        {"pipa", {SoundId::PIPA, SoundCategory::STRINGS, {SoundSubCategory::CHINESE, SoundSubCategory::PLUCKED}}},
        {"yangqin", {SoundId::YANGQIN, SoundCategory::STRINGS, {SoundSubCategory::CHINESE}}},
        {"gayageum", {SoundId::GAYAGEUM, SoundCategory::STRINGS, {SoundSubCategory::KOREAN, SoundSubCategory::PLUCKED}}},
        {"koto", {SoundId::KOTO, SoundCategory::STRINGS, {SoundSubCategory::JAPANESE, SoundSubCategory::PLUCKED}}},
        {"oud", {SoundId::OUD, SoundCategory::STRINGS, {SoundSubCategory::AFRICAN, SoundSubCategory::PLUCKED}}},
        {"shamisen", {SoundId::SHAMISEN, SoundCategory::STRINGS, {SoundSubCategory::JAPANESE, SoundSubCategory::PLUCKED}}},
        {"sitar", {SoundId::SITAR, SoundCategory::STRINGS, {SoundSubCategory::INDIAN, SoundSubCategory::PLUCKED}}},
        {"prim",
         {SoundId::PRIM,
          SoundCategory::STRINGS,
          {
              SoundSubCategory::PLUCKED,
          }}},
        {"brac", {SoundId::BRAC, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"bugarija", {SoundId::BUGARIJA, SoundCategory::STRINGS, {mpe::SoundSubCategory::PLUCKED}}},
        {"berda", {SoundId::BERDA, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"celo", {SoundId::CELO, SoundCategory::STRINGS, {SoundSubCategory::PLUCKED}}},
        {"bandurria", {SoundId::BANDURRIA, SoundCategory::STRINGS, {SoundSubCategory::SPANISH, SoundSubCategory::PLUCKED}}},
        {"bandurria-tablature", {SoundId::BANDURRIA, SoundCategory::STRINGS, {SoundSubCategory::SPANISH, SoundSubCategory::PLUCKED}}},
        {"laud", {SoundId::LAUD, SoundCategory::STRINGS, {SoundSubCategory::SPANISH, SoundSubCategory::PLUCKED}}},
        {"laud-tablature", {SoundId::LAUD, SoundCategory::STRINGS, {SoundSubCategory::SPANISH, SoundSubCategory::PLUCKED}}},

        {"strings", {SoundId::STRINGS_GROUP, SoundCategory::STRINGS}},
        {"double-bass", {SoundId::CONTRABASS, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL}}},
        {"contrabass", {SoundId::CONTRABASS, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL}}},
        {"contrabasses", {SoundId::CONTRABASS, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL, SoundSubCategory::SECTION}}},
        {"violin", {SoundId::VIOLIN, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL}}},
        {"violins", {SoundId::VIOLIN, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL, SoundSubCategory::SECTION}}},
        {"viola", {SoundId::VIOLA, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL}}},
        {"violas", {SoundId::VIOLA, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL, SoundSubCategory::SECTION}}},
        {"violoncello", {SoundId::VIOLONCELLO, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL}}},
        {"violoncellos", {SoundId::VIOLONCELLO, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL, SoundSubCategory::SECTION}}},

        {"treble-viol", {SoundId::VIOL, SoundCategory::STRINGS}},
        {"alto-viol", {SoundId::VIOL, SoundCategory::STRINGS, {SoundSubCategory::ALTO}}},
        {"pardessus-de-viole", {SoundId::PARDESSUS_VIOL, SoundCategory::STRINGS}},
        {"tenor-viol", {SoundId::VIOL, SoundCategory::STRINGS, {SoundSubCategory::TENOR}}},
        {"baryton", {SoundId::VIOL, SoundCategory::STRINGS, {SoundSubCategory::BARITONE}}},
        {"viola-da-gamba", {SoundId::VIOLA_DA_GAMBA, SoundCategory::STRINGS}},
        {"viola-da-gamba-tablature", {SoundId::VIOLA_DA_GAMBA, SoundCategory::STRINGS}},
        {"violone", {SoundId::VIOLONE, SoundCategory::STRINGS}},
        {"d-violone", {SoundId::VIOLONE, SoundCategory::STRINGS}},

        {"octobass", {SoundId::OCTOBASS, SoundCategory::STRINGS}},
        {"erhu", {SoundId::ERHU, SoundCategory::STRINGS, {SoundSubCategory::CHINESE}}},
        {"nyckelharpa", {SoundId::NYCKELHARPA, SoundCategory::STRINGS, {SoundSubCategory::SWEDISH}}},
        {"ajaeng", {SoundId::AJAENG, SoundCategory::STRINGS, {SoundSubCategory::KOREAN}}},

        {"bass-synthesizer",
         {SoundId::SYNTHESIZER, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::BASS, SoundSubCategory::PLUCKED}}},
        {"bowed-synth", {SoundId::SYNTHESIZER, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::BOWED}}},
        {"timple-canario",
         {SoundId::TIMPLE_CANARIO,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::SPANISH, SoundSubCategory::PLUCKED}}},
        {"timple-canario-tablature",
         {SoundId::TIMPLE_CANARIO,
          SoundCategory::STRINGS,
          {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::SPANISH, SoundSubCategory::PLUCKED}}},
    });

    auto search = setup_data_map.find(instrument->id().to_std_string());
    if (search == setup_data_map.cend()) {
        static PlaybackSetupData empty;
        return empty;
    }

    static constexpr auto support_primary_and_secondary_categories = mu::engraving::sorted_values<SoundId>({SoundId::VIOLIN});

    if (muse::contains(support_primary_and_secondary_categories, search->second.sound_id())) {
        SoundSubCategory category = instrument->is_primary() ? SoundSubCategory::PRIMARY : SoundSubCategory::SECONDARY;
        PlaybackSetupData setup_data = search->second;
        setup_data.add(category);
        return setup_data;
    }

    return search->second;
}
