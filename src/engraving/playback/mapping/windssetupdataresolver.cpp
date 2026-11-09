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

#include "windssetupdataresolver.h"

#include <array>

#include "playback/utils/sortedtable.h"

using namespace mu::engraving;
using namespace muse::mpe;

PlaybackSetupData WindsSetupDataResolver::do_resolve(const Instrument *instrument) {
    static const auto setup_data_map = mu::engraving::make_sorted_table<std::string, PlaybackSetupData>({
        {"winds", {SoundId::WINDS_GROUP, SoundCategory::WINDS}},
        {"eb-piccolo", {SoundId::PICCOLO, SoundCategory::WINDS}},
        {"db-piccolo", {SoundId::PICCOLO, SoundCategory::WINDS}},
        {"piccolo", {SoundId::PICCOLO, SoundCategory::WINDS}},
        {"treble-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::TREBLE}}},
        {"soprano-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"bass-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"contra-alto-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::CONTRA_ALTO}}},
        {"contrabass-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},
        {"subcontra-alto-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::SUB_CONTRA_ALTO}}},
        {"double-contrabass-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::DOUBLE_CONTRA_BASS}}},
        {"hyperbass-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::HYPER_BASS}}},
        {"irish-flute", {SoundId::FLUTE, SoundCategory::WINDS, {SoundSubCategory::IRISH}}},
        {"flute", {SoundId::FLUTE, SoundCategory::WINDS}},
        {"traverso", {SoundId::TRAVERSO, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},
        {"danso", {SoundId::DANSO, SoundCategory::WINDS}},
        {"daegeum", {SoundId::DAEGEUM, SoundCategory::WINDS, {SoundSubCategory::KOREAN}}},

        {"a-dizi", {SoundId::DIZI, SoundCategory::WINDS, {SoundSubCategory::CHINESE}}},
        {"g-dizi", {SoundId::DIZI, SoundCategory::WINDS, {SoundSubCategory::CHINESE}}},
        {"f-dizi", {SoundId::DIZI, SoundCategory::WINDS, {SoundSubCategory::CHINESE}}},
        {"e-dizi", {SoundId::DIZI, SoundCategory::WINDS, {SoundSubCategory::CHINESE}}},
        {"d-dizi", {SoundId::DIZI, SoundCategory::WINDS, {SoundSubCategory::CHINESE}}},
        {"c-dizi", {SoundId::DIZI, SoundCategory::WINDS, {SoundSubCategory::CHINESE}}},

        {"shakuhachi", {SoundId::SHAKUHACHI, SoundCategory::WINDS, {SoundSubCategory::JAPANESE}}},
        {"hichiriki", {SoundId::HICHIRIKI, SoundCategory::WINDS, {SoundSubCategory::JAPANESE}}},
        {"fife", {SoundId::FIFE, SoundCategory::WINDS}},

        {"d-tin-whistle", {SoundId::WHISTLE, SoundCategory::WINDS, {SoundSubCategory::TIN}}},
        {"c-tin-whistle", {SoundId::WHISTLE, SoundCategory::WINDS, {SoundSubCategory::TIN}}},
        {"bflat-tin-whistle", {SoundId::WHISTLE, SoundCategory::WINDS, {SoundSubCategory::TIN}}},
        {"slide-whistle", {SoundId::WHISTLE, SoundCategory::WINDS, {SoundSubCategory::SLIDE}}},

        {"french-flageolet", {SoundId::FLAGEOLET, SoundCategory::WINDS}},
        {"english-flageolet", {SoundId::FLAGEOLET, SoundCategory::WINDS}},
        {"flageolet", {SoundId::FLAGEOLET, SoundCategory::WINDS}},

        {"garklein-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::GARKLEIN}}},
        {"sopranino-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"soprano-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"recorder", {SoundId::RECORDER, SoundCategory::WINDS}},
        {"alto-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"bass-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"tenor-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"greatbass-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::GREAT_BASS}}},
        {"contrabass-recorder", {SoundId::RECORDER, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},

        {"g-soprano-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"f-soprano-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"c-soprano-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"bb-soprano-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"g-alto-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"f-alto-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"c-alto-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"bb-alto-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"c-bass-ocarina", {SoundId::OCARINA, SoundCategory::WINDS, {SoundSubCategory::BASS}}},

        {"gemshorn", {SoundId::GEMSHORN, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"soprano-gemshorn", {SoundId::GEMSHORN, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-gemshorn", {SoundId::GEMSHORN, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-gemshorn", {SoundId::GEMSHORN, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-gemshorn", {SoundId::GEMSHORN, SoundCategory::WINDS, {SoundSubCategory::BASS}}},

        {"theremin", {SoundId::THEREMIN, SoundCategory::WINDS, {SoundSubCategory::ELECTRIC}}},

        {"pan-flute", {SoundId::PAN_FLUTE, SoundCategory::WINDS}},

        {"quena", {SoundId::QUENA, SoundCategory::WINDS}},
        {"c-quena", {SoundId::QUENA, SoundCategory::WINDS}},
        {"g-quena", {SoundId::QUENA, SoundCategory::WINDS}},
        {"f-quena", {SoundId::QUENA, SoundCategory::WINDS}},
        {"d-quena", {SoundId::QUENA, SoundCategory::WINDS}},

        {"fs-castilian-dulzaina", {SoundId::DULZAINA, SoundCategory::WINDS, {SoundSubCategory::CASTILIAN}}},

        {"piccolo-heckelphone", {SoundId::HECKELPHONE, SoundCategory::WINDS}},
        {"heckelphone", {SoundId::HECKELPHONE, SoundCategory::WINDS}},
        {"piccolo-oboe", {SoundId::OBOE, SoundCategory::WINDS}},
        {"baroque-oboe", {SoundId::OBOE, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},
        {"oboe", {SoundId::OBOE, SoundCategory::WINDS}},
        {"oboe-d'amore", {SoundId::OBOE, SoundCategory::WINDS}},
        {"oboe-da-caccia", {SoundId::OBOE, SoundCategory::WINDS}},
        {"english-horn", {SoundId::OBOE, SoundCategory::WINDS, {SoundSubCategory::ENGLISH}}},
        {"bass-oboe", {SoundId::OBOE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"lupophone", {SoundId::LUPOPHONE, SoundCategory::WINDS}},

        {"sopranino-shawm", {SoundId::SHAWM, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"soprano-shawm", {SoundId::SHAWM, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-shawm", {SoundId::SHAWM, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-shawm", {SoundId::SHAWM, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-shawm", {SoundId::SHAWM, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"great-bass-shawm", {SoundId::SHAWM, SoundCategory::WINDS, {SoundSubCategory::GREAT_BASS}}},

        {"cromorne",
         {SoundId::CROMORNE, SoundCategory::WINDS, {SoundSubCategory::FRENCH, SoundSubCategory::BAROQUE, SoundSubCategory::REED}}},

        {"crumhorn", {SoundId::CRUMHORN, SoundCategory::WINDS}},
        {"soprano-crumhorn", {SoundId::CRUMHORN, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-crumhorn", {SoundId::CRUMHORN, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-crumhorn", {SoundId::CRUMHORN, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-crumhorn", {SoundId::CRUMHORN, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"greatbass-crumhorn", {SoundId::CRUMHORN, SoundCategory::WINDS, {SoundSubCategory::GREAT_BASS}}},

        {"cornamuse", {SoundId::CORNAMUSE, SoundCategory::WINDS}},
        {"soprano-cornamuse", {SoundId::CORNAMUSE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-cornamuse", {SoundId::CORNAMUSE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-cornamuse", {SoundId::CORNAMUSE, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-cornamuse", {SoundId::CORNAMUSE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},

        {"soprano-kelhorn", {SoundId::KELHORN, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-kelhorn", {SoundId::KELHORN, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-kelhorn", {SoundId::KELHORN, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-kelhorn", {SoundId::KELHORN, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"greatbass-kelhorn", {SoundId::KELHORN, SoundCategory::WINDS, {SoundSubCategory::GREAT_BASS}}},

        {"sopranino-rauschpfeife", {SoundId::RAUSCHPFEIFE, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"rauschpfeife", {SoundId::RAUSCHPFEIFE, SoundCategory::WINDS}},
        {"soprano-rauschpfeife", {SoundId::RAUSCHPFEIFE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},

        {"f-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"e-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"d-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"c-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"b-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"bb-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"a-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"g-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::ARMENIAN}}},
        {"a-bass-duduk", {SoundId::DUDUK, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::ARMENIAN}}},

        {"shenai", {SoundId::SHENAI, SoundCategory::WINDS, {SoundSubCategory::INDIAN}}},

        {"piccolo-clarinet", {SoundId::CLARINET, SoundCategory::WINDS}},
        {"soprano-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"eb-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO, SoundSubCategory::IN_E_FLAT}}},
        {"d-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"c-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"bb-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANO, SoundSubCategory::IN_B_FLAT}}},
        {"clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANO, SoundSubCategory::IN_B_FLAT}}},
        {"a-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"g-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"basset-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"basset-horn", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"bass-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::IN_B_FLAT}}},
        {"bb-bass-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::IN_B_FLAT}}},
        {"bb-bass-clarinet-bass-clef", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::IN_B_FLAT}}},
        {"a-bass-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"a-bass-clarinet-bass-clef", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"contra-alto-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::CONTRA_ALTO}}},
        {"contrabass-clarinet", {SoundId::CLARINET, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},

        {"sopranino-chalumeau", {SoundId::CHALUMEAU, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"soprano-chalumeau", {SoundId::CHALUMEAU, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-chalumeau", {SoundId::CHALUMEAU, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"chalumeau", {SoundId::CHALUMEAU, SoundCategory::WINDS}},
        {"tenor-chalumeau", {SoundId::CHALUMEAU, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-chalumeau", {SoundId::CHALUMEAU, SoundCategory::WINDS, {SoundSubCategory::BASS}}},

        {"d-xaphoon", {SoundId::XAPHOON, SoundCategory::WINDS}},
        {"xaphoon", {SoundId::XAPHOON, SoundCategory::WINDS}},
        {"bb-xaphoon", {SoundId::XAPHOON, SoundCategory::WINDS}},
        {"g-xaphoon", {SoundId::XAPHOON, SoundCategory::WINDS}},

        {"tarogato", {SoundId::TAROGATO, SoundCategory::WINDS, {SoundSubCategory::HUNGARIAN, SoundSubCategory::ROMANIAN}}},

        {"octavin", {SoundId::OCTAVIN, SoundCategory::WINDS}},

        {"sopranissimo-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANISSIMO}}},
        {"sopranino-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"aulochrome", {SoundId::SAXOPHONE, SoundCategory::WINDS}},
        {"soprano-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"mezzo-soprano-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::MEZZO_SOPRANO}}},
        {"heckelphone-clarinet", {SoundId::HECKELPHONE_CLARINET, SoundCategory::WINDS}},
        {"alto-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"melody-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::MELODY}}},
        {"saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"tenor-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"baritone-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::BARITONE}}},
        {"bass-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"contrabass-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},
        {"subcontrabass-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::SUB_CONTRA_BASS}}},

        {"bassoon", {SoundId::BASSOON, SoundCategory::WINDS}},
        {"contrabassoon", {SoundId::CONTRABASSOON, SoundCategory::WINDS}},
        {"reed-contrabass", {SoundId::CONTRABASSOON, SoundCategory::WINDS, {SoundSubCategory::REED}}},
        {"dulcian", {SoundId::DULCIAN, SoundCategory::WINDS}},
        {"rackett", {SoundId::RACKETT, SoundCategory::WINDS}},

        {"sopranino-sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANINO}}},
        {"sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"soprano-sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"baritone-sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::BARITONE}}},
        {"bass-sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"contrabass-sarrusophone", {SoundId::SARRUSOPHONE, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},

        {"bagpipe", {SoundId::BAGPIPE, SoundCategory::WINDS}},
        {"accordion", {SoundId::ACCORDION, SoundCategory::WINDS}},
        {"bandoneon", {SoundId::BANDONEON, SoundCategory::WINDS}},
        {"concertina", {SoundId::CONCERTINA, SoundCategory::WINDS}},

        {"harmonica", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-d10high-g", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-d10f", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-d10d", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-d10c", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-d10a", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-d10g", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-d10low-d", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::DIATONIC}}},
        {"harmonica-c12c", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::CHROMATIC}}},
        {"harmonica-c12g", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::CHROMATIC}}},
        {"harmonica-c12tenor-c", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::CHROMATIC, SoundSubCategory::TENOR}}},
        {"harmonica-c14c", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::CHROMATIC}}},
        {"harmonica-c16c", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::CHROMATIC}}},
        {"harmonica-chordet", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::HUANG}}},
        {"bass-harmonica-hohner", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::HOHNER}}},
        {"bass-harmonica-huang", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::HUANG}}},
        {"bass-harmonica", {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::BASS}}},

        {"melodica", {SoundId::MELODICA, SoundCategory::WINDS}},

        {"sho", {SoundId::SHO, SoundCategory::WINDS, {SoundSubCategory::JAPANESE}}},
        {"alto-sheng", {SoundId::SHENG, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-sheng", {SoundId::SHENG, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-sheng", {SoundId::SHENG, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"sheng", {SoundId::SHENG, SoundCategory::WINDS}},
        {"soprano-sheng", {SoundId::SHENG, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},

        {"brass", {SoundId::BRASS_GROUP, SoundCategory::WINDS}},

        {"c-horn-alto", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::ALTO, SoundSubCategory::IN_C}}},
        {"bb-horn-alto", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::ALTO, SoundSubCategory::IN_B_FLAT}}},
        {"a-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::IN_A}}},
        {"ab-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::IN_A_FLAT}}},
        {"g-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::IN_G}}},
        {"e-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::IN_E}}},
        {"eb-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::IN_E_FLAT}}},
        {"d-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::IN_D}}},
        {"horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::FRENCH, SoundSubCategory::IN_F}}},
        {"c-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::IN_C}}},
        {"c-horn-bass", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::IN_C}}},
        {"bb-horn-basso", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::IN_B_FLAT}}},
        {"vienna-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::VIENNA}}},

        {"bb-wagner-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::WAGNER}}},
        {"f-wagner-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::WAGNER}}},
        {"wagner-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::WAGNER}}},

        {"eb-cornet", {SoundId::CORNET, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"c-cornet", {SoundId::CORNET, SoundCategory::WINDS}},
        {"bb-cornet", {SoundId::CORNET, SoundCategory::WINDS}},
        {"a-cornet", {SoundId::CORNET, SoundCategory::WINDS}},

        {"saxhorn", {SoundId::SAXHORN, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"f-alto-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"eb-alto-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"baritone-horn", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::BARITONE}}},
        {"baritone-horn-treble", {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::BARITONE, SoundSubCategory::TREBLE}}},
        {"baritone-horn-central-europe-treble",
         {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::CENTRAL_EUROPEAN, SoundSubCategory::BARITONE, SoundSubCategory::TREBLE}}},
        {"baritone-horn-central-europe",
         {SoundId::HORN, SoundCategory::WINDS, {SoundSubCategory::BARITONE, SoundSubCategory::CENTRAL_EUROPEAN}}},
        {"posthorn", {SoundId::POSTHORN, SoundCategory::WINDS}},

        {"bb-piccolo-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::PICCOLO}}},
        {"piccolo-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::PICCOLO}}},
        {"a-piccolo-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::PICCOLO}}},
        {"f-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"e-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"eb-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"d-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"c-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"bb-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"a-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS}},
        {"pocket-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::POCKET}}},
        {"slide-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::SLIDE}}},
        {"tenor-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"eb-bass-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"c-bass-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"bass-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"bb-bass-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"f-baroque-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},
        {"eb-baroque-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},
        {"d-baroque-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},
        {"c-baroque-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},
        {"bb-baroque-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},
        {"baroque-trumpet", {SoundId::TRUMPET, SoundCategory::WINDS, {SoundSubCategory::BAROQUE}}},

        {"bugle", {SoundId::BUGLE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"soprano-bugle", {SoundId::BUGLE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-bugle", {SoundId::BUGLE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"mellophone", {SoundId::MELLOPHONE_BUGLE, SoundCategory::WINDS}},
        {"baritone-bugle", {SoundId::BUGLE, SoundCategory::WINDS, {SoundSubCategory::BARITONE}}},
        {"euphonium-bugle", {SoundId::EUPHONIUM_BUGLE, SoundCategory::WINDS}},
        {"mellophon-bugle", {SoundId::MELLOPHONE_BUGLE, SoundCategory::WINDS}},
        {"contrabass-bugle", {SoundId::BUGLE, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},

        {"fiscorn", {SoundId::FISCORN, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"flugelhorn", {SoundId::FLUGELHORN, SoundCategory::WINDS}},
        {"kuhlohorn", {SoundId::KUHLOHORN, SoundCategory::WINDS}},

        {"f-alto-ophicleide", {SoundId::OPHICLEIDE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"eb-alto-ophicleide", {SoundId::OPHICLEIDE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"ophicleide", {SoundId::OPHICLEIDE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"c-bass-ophicleide", {SoundId::OPHICLEIDE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"bb-bass-ophicleide", {SoundId::OPHICLEIDE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"eb-contrabass-ophicleide", {SoundId::OPHICLEIDE, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},

        {"cornettino", {SoundId::CORNETTINO, SoundCategory::WINDS}},
        {"cornett", {SoundId::CORNETT, SoundCategory::WINDS}},
        {"soprano-cornett", {SoundId::CORNETT, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-cornett", {SoundId::CORNETT, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-cornett", {SoundId::CORNETT, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},

        {"serpent", {SoundId::SERPENT, SoundCategory::WINDS}},

        {"soprano-trombone", {SoundId::TROMBONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANO}}},
        {"alto-trombone", {SoundId::TROMBONE, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-trombone", {SoundId::TROMBONE, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"trombone", {SoundId::TROMBONE, SoundCategory::WINDS}},
        {"trombone-treble", {SoundId::TROMBONE, SoundCategory::WINDS, {SoundSubCategory::TREBLE}}},
        {"contrabass-trombone", {SoundId::TROMBONE, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},
        {"bass-trombone", {SoundId::TROMBONE, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"cimbasso", {SoundId::CIMBASSO, SoundCategory::WINDS}},

        {"alto-sackbut", {SoundId::SACKBUT, SoundCategory::WINDS, {SoundSubCategory::ALTO}}},
        {"tenor-sackbut", {SoundId::SACKBUT, SoundCategory::WINDS, {SoundSubCategory::TENOR}}},
        {"bass-sackbut", {SoundId::SACKBUT, SoundCategory::WINDS, {SoundSubCategory::BASS}}},

        {"euphonium", {SoundId::EUPHONIUM, SoundCategory::WINDS}},
        {"euphonium-treble", {SoundId::EUPHONIUM, SoundCategory::WINDS, {SoundSubCategory::TREBLE}}},

        {"tuba", {SoundId::TUBA, SoundCategory::WINDS}},
        {"f-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"eb-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"bass-eb-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"bass-f-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::BASS}}},
        {"eb-tuba-treble", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::TREBLE, SoundSubCategory::BASS}}},
        {"c-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},
        {"bb-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::CONTRA_BASS}}},
        {"bb-tuba-treble", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::TREBLE}}},
        {"subcontrabass-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::SUB_CONTRA_BASS}}},

        {"bb-sousaphone", {SoundId::SOUSAPHONE, SoundCategory::WINDS}},
        {"bb-sousaphone-treble", {SoundId::SOUSAPHONE, SoundCategory::WINDS, {SoundSubCategory::TREBLE}}},
        {"sousaphone", {SoundId::SOUSAPHONE, SoundCategory::WINDS}},
        {"helicon", {SoundId::HELICON, SoundCategory::WINDS}},

        {"conch", {SoundId::CONCH, SoundCategory::WINDS}},
        {"horagai", {SoundId::HORAGAI, SoundCategory::WINDS, {SoundSubCategory::JAPANESE}}},

        {"alphorn", {SoundId::ALPHORN, SoundCategory::WINDS, {SoundSubCategory::ALPINE}}},
        {"rag-dung", {SoundId::RAG_DUNG, SoundCategory::WINDS, {SoundSubCategory::TIBETAN}}},
        {"didgeridoo", {SoundId::DIDGERIDOO, SoundCategory::WINDS, {SoundSubCategory::AUSTRALIAN}}},
        {"shofar", {SoundId::SHOFAR, SoundCategory::WINDS}},
        {"vuvuzela", {SoundId::VUVUZELA, SoundCategory::WINDS}},
        {"tuned-klaxon-horns", {SoundId::KLAXON_HORNS, SoundCategory::WINDS}},
        {"kazoo", {SoundId::KAZOO, SoundCategory::WINDS}},
    });

    auto search = setup_data_map.find(instrument->id().to_std_string());
    if (search == setup_data_map.cend()) {
        static PlaybackSetupData empty;
        return empty;
    }

    static constexpr auto support_primary_and_secondary_categories = mu::engraving::sorted_values<SoundId>({
        SoundId::FLUTE,
    });

    if (muse::contains(support_primary_and_secondary_categories, search->second.sound_id())) {
        SoundSubCategory category = instrument->is_primary() ? SoundSubCategory::PRIMARY : SoundSubCategory::SECONDARY;
        PlaybackSetupData setup_data = search->second;
        setup_data.add(category);
        return setup_data;
    }

    return search->second;
}
