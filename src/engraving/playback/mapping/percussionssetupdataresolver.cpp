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

#include "log.h"
#include "percussionssetupdataresolver.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

PlaybackSetupData PercussionsSetupDataResolver::do_resolve(const Instrument *instrument) {
    static const std::unordered_map<std::string, mpe::PlaybackSetupData> setup_data_map = {
        {"timpani", {SoundId::TIMPANI, SoundCategory::PERCUSSIONS}},
        {"roto-toms", {SoundId::ROTO_TOMS, SoundCategory::PERCUSSIONS}},
        {"tubaphone", {SoundId::TUBAPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"soprano-steel-drums",
         {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL, SoundSubCategory::SOPRANO}}},
        {"alto-steel-drums",
         {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL, SoundSubCategory::ALTO}}},
        {"tenor-steel-drums",
         {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL, SoundSubCategory::TENOR}}},
        {"guitar-steel-drums", {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL}}},
        {"cello-steel-drums", {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL}}},
        {"cello-steel-drums-four",
         {SoundId::STEEL_DRUMS,
          SoundCategory::PERCUSSIONS,
          {SoundSubCategory::METAL, SoundSubCategory::STEEL, SoundSubCategory::FOUR_PIECE}}},
        {"tenor-bass-steel-drums",
         {SoundId::STEEL_DRUMS,
          SoundCategory::PERCUSSIONS,
          {SoundSubCategory::METAL, SoundSubCategory::STEEL, SoundSubCategory::TENOR, SoundSubCategory::BASS}}},
        {"steel-drums", {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL}}},
        {"bass-steel-drums",
         {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL, SoundSubCategory::BASS}}},
        {"glockenspiel", {SoundId::GLOCKENSPIEL, SoundCategory::PERCUSSIONS}},
        {"xylophone", {SoundId::XYLOPHONE, SoundCategory::PERCUSSIONS}},
        {"xylomarimba", {SoundId::XYLOMARIMBA, SoundCategory::PERCUSSIONS}},
        {"vibraphone", {SoundId::VIBRAPHONE, SoundCategory::PERCUSSIONS}},
        {"dulcimer", {SoundId::DULCIMER, SoundCategory::PERCUSSIONS}},
        {"cimbalom", {SoundId::CIMBALOM, SoundCategory::PERCUSSIONS}},
        {"marimba", {SoundId::MARIMBA, SoundCategory::PERCUSSIONS}},
        {"marimba-single", {SoundId::MARIMBA, SoundCategory::PERCUSSIONS}},
        {"bass-marimba", {SoundId::MARIMBA, SoundCategory::PERCUSSIONS, {SoundSubCategory::BASS}}},
        {"contrabass-marimba", {SoundId::MARIMBA, SoundCategory::PERCUSSIONS, {SoundSubCategory::CONTRA_BASS}}},

        {"crotales", {SoundId::CROTALES, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"almglocken", {SoundId::KALIMBA, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"tubular-bells", {SoundId::CHIMES, SoundCategory::PERCUSSIONS}},
        {"carillon", {SoundId::CARILLON, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"tuned-gongs", {SoundId::GONG, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"opera-gong", {SoundId::GONG, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::OPERA}}},
        {"wind-gong", {SoundId::GONG, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::WIND}}},
        {"hand-bells", {SoundId::BELL, SoundCategory::PERCUSSIONS, {SoundSubCategory::HAND}}},
        {"hand-chimes", {SoundId::CHIMES, SoundCategory::PERCUSSIONS, {SoundSubCategory::HAND}}},

        {"orff-soprano-glockenspiel",
         {SoundId::GLOCKENSPIEL, SoundCategory::PERCUSSIONS, {SoundSubCategory::SOPRANO, SoundSubCategory::ORFF}}},
        {"orff-alto-glockenspiel", {SoundId::GLOCKENSPIEL, SoundCategory::PERCUSSIONS, {SoundSubCategory::ALTO, SoundSubCategory::ORFF}}},
        {"orff-soprano-metallophone",
         {SoundId::METALLOPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::SOPRANO, SoundSubCategory::ORFF}}},
        {"orff-soprano-xylophone", {SoundId::XYLOPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::SOPRANO, SoundSubCategory::ORFF}}},
        {"metallophone", {SoundId::METALLOPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::ORFF}}},
        {"orff-alto-metallophone", {SoundId::METALLOPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::ALTO, SoundSubCategory::ORFF}}},
        {"orff-alto-xylophone", {SoundId::XYLOPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::ALTO, SoundSubCategory::ORFF}}},
        {"orff-bass-metallophone", {SoundId::METALLOPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::BASS, SoundSubCategory::ORFF}}},
        {"orff-bass-xylophone", {SoundId::XYLOPHONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::BASS, SoundSubCategory::ORFF}}},

        {"flexatone", {SoundId::FLEXATONE, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"musical-saw", {SoundId::MUSICAL_SAW, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},

        {"musical-glasses", {SoundId::MUSICAL_GLASSES, SoundCategory::PERCUSSIONS, {SoundSubCategory::GLASS}}},
        {"glass-harmonica", {SoundId::HARMONICA, SoundCategory::PERCUSSIONS, {SoundSubCategory::GLASS}}},

        {"alto-kalimba", {SoundId::KALIMBA, SoundCategory::PERCUSSIONS, {SoundSubCategory::ALTO}}},
        {"kalimba", {SoundId::KALIMBA, SoundCategory::PERCUSSIONS}},
        {"treble-kalimba", {SoundId::KALIMBA, SoundCategory::PERCUSSIONS, {SoundSubCategory::TREBLE}}},

        {"automobile-brake-drums", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::BRAKE}}},
        {"bongos", {SoundId::BONGOS, SoundCategory::PERCUSSIONS}},
        {"chinese-tom-toms", {SoundId::TOM_TOMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::CHINESE}}},
        {"bass-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::BASS}}},
        {"snare-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::SNARE}}},
        {"tom-toms", {SoundId::TOM_TOMS, SoundCategory::PERCUSSIONS}},
        {"ko-tsuzumi", {SoundId::KO_TSUZUMI, SoundCategory::PERCUSSIONS}},
        {"o-tsuzumi", {SoundId::O_TSUZUMI, SoundCategory::PERCUSSIONS}},
        {"kakko", {SoundId::KAKKO, SoundCategory::PERCUSSIONS}},
        {"shime-daiko", {SoundId::SHIME_DAIKO, SoundCategory::PERCUSSIONS}},
        {"miya-daiko", {SoundId::MIYA_DAIKO, SoundCategory::PERCUSSIONS}},
        {"tsuri-daiko", {SoundId::TSURI_DAIKO, SoundCategory::PERCUSSIONS}},
        {"okedo-daiko", {SoundId::OKEDO_DAIKO, SoundCategory::PERCUSSIONS}},
        {"kane", {SoundId::KANE, SoundCategory::PERCUSSIONS}},
        {"shoko", {SoundId::SHOKO, SoundCategory::PERCUSSIONS}},
        {"congas", {SoundId::CONGA, SoundCategory::PERCUSSIONS}},
        {"xiaogu", {SoundId::XIAOGU, SoundCategory::PERCUSSIONS}},
        {"bangu", {SoundId::BANGU, SoundCategory::PERCUSSIONS}},
        {"dagu", {SoundId::DAGU, SoundCategory::PERCUSSIONS}},
        {"daluo", {SoundId::DALUO, SoundCategory::PERCUSSIONS}},
        {"xiaoluo", {SoundId::XIAOLUO, SoundCategory::PERCUSSIONS}},
        {"ban", {SoundId::BAN, SoundCategory::PERCUSSIONS}},
        {"dabo", {SoundId::DABO, SoundCategory::PERCUSSIONS}},
        {"naobo", {SoundId::NAOBO, SoundCategory::PERCUSSIONS}},
        {"bangzi", {SoundId::BANGZI, SoundCategory::PERCUSSIONS}},
        {"janggu", {SoundId::JANGGU, SoundCategory::PERCUSSIONS}},
        {"samul-janggu", {SoundId::SAMUL_JANGGU, SoundCategory::PERCUSSIONS, {SoundSubCategory::KOREAN}}},
        {"sanjo-janggu", {SoundId::SANJO_JANGGU, SoundCategory::PERCUSSIONS, {SoundSubCategory::KOREAN}}},
        {"buk", {SoundId::BUK, SoundCategory::PERCUSSIONS}},
        {"samul-buk", {SoundId::SAMUL_BUK, SoundCategory::PERCUSSIONS, {SoundSubCategory::KOREAN}}},
        {"sori-buk", {SoundId::SORI_BUK, SoundCategory::PERCUSSIONS, {SoundSubCategory::KOREAN}}},
        {"sogo", {SoundId::SOGO, SoundCategory::PERCUSSIONS}},
        {"kkwaenggwari", {SoundId::KKWAENGGWARI, SoundCategory::PERCUSSIONS}},
        {"jing", {SoundId::JING, SoundCategory::PERCUSSIONS}},
        {"djembe", {SoundId::DJEMBE, SoundCategory::PERCUSSIONS}},
        {"doumbek", {SoundId::DOUMBEK, SoundCategory::PERCUSSIONS}},
        {"cuica", {SoundId::CUICA, SoundCategory::PERCUSSIONS}},
        {"cajon", {SoundId::CAJON, SoundCategory::PERCUSSIONS}},
        {"drumset", {SoundId::DRUMSET, SoundCategory::PERCUSSIONS}},
        {"drum-kit-4", {SoundId::DRUMSET, SoundCategory::PERCUSSIONS, {SoundSubCategory::FOUR_PIECE}}},
        {"drum-kit-5", {SoundId::DRUMSET, SoundCategory::PERCUSSIONS, {SoundSubCategory::FIVE_PIECE}}},
        {"military-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::MILITARY}}},
        {"frame-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::FRAME}}},
        {"piccolo-snare-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::SNARE, SoundSubCategory::PICCOLO}}},
        {"slit-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::SLIT}}},
        {"tablas", {SoundId::TABLAS, SoundCategory::PERCUSSIONS, {SoundSubCategory::INDIAN}}},
        {"timbales", {SoundId::TIMBALES, SoundCategory::PERCUSSIONS}},

        {"anvil", {SoundId::ANVIL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"bell-plate", {SoundId::BELL, SoundCategory::PERCUSSIONS, {SoundSubCategory::PLATE, SoundSubCategory::METAL}}},
        {"bell-tree", {SoundId::BELL_TREE, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"bells", {SoundId::BELL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"bowl-gongs", {SoundId::GONG, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::BOWL}}},
        {"chains", {SoundId::CHAIN, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"china-cymbal", {SoundId::CYMBAL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::CHINA}}},
        {"cowbell", {SoundId::BELL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::COW}}},
        {"agogo-bells", {SoundId::BELL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::AGOGO}}},
        {"crash-cymbal", {SoundId::CYMBAL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::CRASH}}},
        {"cymbal", {SoundId::CYMBAL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"finger-cymbals", {SoundId::CYMBAL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::FINGER}}},
        {"hi-hat", {SoundId::HI_HAT, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"iron-pipes", {SoundId::PIPE, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::IRON}}},
        {"mark-tree", {SoundId::MARK_TREE, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"metal-castanets", {SoundId::CASTANET, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"metal-wind-chimes", {SoundId::CHIMES, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::WIND}}},
        {"ride-cymbal", {SoundId::CYMBAL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::RIDE}}},
        {"sleigh-bells", {SoundId::BELL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::SLEIGH}}},
        {"splash-cymbal", {SoundId::CYMBAL, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::SPLASH}}},
        {"tam-tam", {SoundId::TAM_TAM, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"thundersheet", {SoundId::THUNDERSHEET, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},
        {"triangle", {SoundId::TRIANGLE, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL}}},

        {"castanets", {SoundId::CASTANET, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN}}},
        {"claves", {SoundId::CLAVES, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN}}},
        {"guiro", {SoundId::GUIRO, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN}}},
        {"temple-blocks", {SoundId::BLOCK, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN, SoundSubCategory::TEMPLE}}},
        {"log-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN, SoundSubCategory::LOG}}},
        {"ocean-drum", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::OCEAN}}},
        {"wood-blocks", {SoundId::BLOCK, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN}}},
        {"wooden-wind-chimes", {SoundId::CHIMES, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN, SoundSubCategory::WIND}}},
        {"bamboo-wind-chimes", {SoundId::CHIMES, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN, SoundSubCategory::WIND}}},

        {"cabasa", {SoundId::CABASA, SoundCategory::PERCUSSIONS}},
        {"maracas", {SoundId::MARACA, SoundCategory::PERCUSSIONS}},
        {"quijada", {SoundId::QUIJADA, SoundCategory::PERCUSSIONS}},
        {"ratchet", {SoundId::RATCHET, SoundCategory::PERCUSSIONS}},
        {"shaker", {SoundId::SHAKER, SoundCategory::PERCUSSIONS}},
        {"shekere", {SoundId::SHEKERE, SoundCategory::PERCUSSIONS}},
        {"sandpaper-blocks", {SoundId::BLOCK, SoundCategory::PERCUSSIONS, {SoundSubCategory::SANDPAPER}}},
        {"glass-wind-chimes", {SoundId::CHIMES, SoundCategory::PERCUSSIONS, {SoundSubCategory::GLASS, SoundSubCategory::WIND}}},
        {"shell-wind-chimes", {SoundId::CHIMES, SoundCategory::PERCUSSIONS, {SoundSubCategory::SHELL, SoundSubCategory::WIND}}},
        {"percussion", {SoundId::DRUMSET, SoundCategory::PERCUSSIONS, {SoundSubCategory::ORCHESTRAL}}},
        {"stones", {SoundId::STONES, SoundCategory::PERCUSSIONS}},
        {"tambourine", {SoundId::TAMBOURINE, SoundCategory::PERCUSSIONS}},
        {"tubo", {SoundId::TUBO, SoundCategory::PERCUSSIONS}},
        {"vibraslap", {SoundId::VIBRASLAP, SoundCategory::PERCUSSIONS}},
        {"whip", {SoundId::WHIP, SoundCategory::PERCUSSIONS}},
        {"cannon", {SoundId::CANNON, SoundCategory::PERCUSSIONS}},
        {"bird-call", {SoundId::BIRD_CALL, SoundCategory::PERCUSSIONS}},

        {"marching-snare", {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::MARCHING, SoundSubCategory::SNARE}}},
        {"marching-tenor-drums",
         {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::MARCHING, SoundSubCategory::SNARE, SoundSubCategory::TENOR}}},
        {"marching-show-tenors",
         {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::SHOW_STYLE, SoundSubCategory::SNARE, SoundSubCategory::TENOR}}},
        {"marching-bass-drums",
         {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::MARCHING, SoundSubCategory::SNARE, SoundSubCategory::BASS}}},
        {"marching-cymbals",
         {SoundId::CYMBAL, SoundCategory::PERCUSSIONS, {SoundSubCategory::MARCHING, SoundSubCategory::METAL, SoundSubCategory::CRASH}}},

        {"finger-snap", {SoundId::SNAP, SoundCategory::PERCUSSIONS, {SoundSubCategory::FINGER}}},
        {"hand-clap", {SoundId::CLAP, SoundCategory::PERCUSSIONS, {SoundSubCategory::HAND}}},
        {"slap", {SoundId::SLAP, SoundCategory::PERCUSSIONS, {SoundSubCategory::HAND}}},
        {"stamp", {SoundId::STAMP, SoundCategory::PERCUSSIONS, {SoundSubCategory::FOOT}}},

        {"taiko", {SoundId::TAIKO, SoundCategory::PERCUSSIONS}},

        {"percussion-synthesizer",
         {SoundId::SYNTHESIZER, SoundCategory::PERCUSSIONS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PERCUSSIVE}}},
    };

    auto search = setup_data_map.find(instrument->id().to_std_string());
    if (search == setup_data_map.cend()) {
        if (instrument->use_drumset()) {
            LOGW() << "Unable to resolve setup data for instrument, id: " << instrument->id() << ", family: " << instrument->family()
                   << "; fallback to drumset";

            static const PlaybackSetupData drumset_fallback{SoundId::DRUMSET, SoundCategory::PERCUSSIONS};

            return drumset_fallback;
        }

        static const PlaybackSetupData empty;
        return empty;
    }

    return search->second;
}
