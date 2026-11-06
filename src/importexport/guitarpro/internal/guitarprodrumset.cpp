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

#include "guitarprodrumset.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/instrument.h"
#include "engraving/types/typesconv.h"

namespace mu::iex::guitarpro::drumset {
//---------------------------------------------------------
//   initGuitarProDrumset
//---------------------------------------------------------

void init_guitar_pro_drumset() {
    using namespace mu::engraving;

    if (!gp_drumset) {
        gp_drumset = new Drumset();
        for (int i = 0; i < ::mu::engraving::drum_instruments; ++i) {
            gp_drumset->drum(i).name.clear();
            gp_drumset->drum(i).notehead = NoteHeadGroup::HEAD_INVALID;
            gp_drumset->drum(i).line = 0;
            gp_drumset->drum(i).shortcut.clear();
            gp_drumset->drum(i).voice = 0;
            gp_drumset->drum(i).stem_direction = DirectionV::UP;
            gp_drumset->drum(i).panel_row = -1;
            gp_drumset->drum(i).panel_column = -1;
        }
        // new drumset determined via guitar pro (third argument specifies position on staff, 10 = C3, 9 = D3, 8 = E3,...)

        gp_drumset->drum(27) = DrumInstrument(TConv::user_name(DrumNum(27)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(28) = DrumInstrument(TConv::user_name(DrumNum(28)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(29) = DrumInstrument(TConv::user_name(DrumNum(29)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);

        gp_drumset->drum(30) = DrumInstrument(TConv::user_name(DrumNum(30)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(31) = DrumInstrument(TConv::user_name(DrumNum(31)), NoteHeadGroup::HEAD_CROSS, 3, DirectionV::UP);
        gp_drumset->drum(32) = DrumInstrument(TConv::user_name(DrumNum(32)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(33) = DrumInstrument(TConv::user_name(DrumNum(33)), NoteHeadGroup::HEAD_CROSS, 3, DirectionV::UP);
        gp_drumset->drum(34) = DrumInstrument(TConv::user_name(DrumNum(34)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(35) = DrumInstrument(TConv::user_name(DrumNum(35)), NoteHeadGroup::HEAD_NORMAL, 8, DirectionV::UP);
        gp_drumset->drum(36) = DrumInstrument(TConv::user_name(DrumNum(36)), NoteHeadGroup::HEAD_NORMAL, 7, DirectionV::UP);
        gp_drumset->drum(37) = DrumInstrument(TConv::user_name(DrumNum(37)), NoteHeadGroup::HEAD_CROSS, 3, DirectionV::UP);
        gp_drumset->drum(38) = DrumInstrument(TConv::user_name(DrumNum(38)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(39) = DrumInstrument(TConv::user_name(DrumNum(39)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);

        gp_drumset->drum(40) = DrumInstrument(TConv::user_name(DrumNum(40)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(41) = DrumInstrument(TConv::user_name(DrumNum(41)), NoteHeadGroup::HEAD_NORMAL, 6, DirectionV::UP);
        gp_drumset->drum(42) = DrumInstrument(TConv::user_name(DrumNum(42)), NoteHeadGroup::HEAD_CROSS, -1, DirectionV::UP);
        gp_drumset->drum(43) = DrumInstrument(TConv::user_name(DrumNum(43)), NoteHeadGroup::HEAD_NORMAL, 6, DirectionV::UP);
        gp_drumset->drum(44) = DrumInstrument(TConv::user_name(DrumNum(44)), NoteHeadGroup::HEAD_CROSS, 9, DirectionV::UP);
        gp_drumset->drum(45) = DrumInstrument(TConv::user_name(DrumNum(45)), NoteHeadGroup::HEAD_NORMAL, 5, DirectionV::UP);
        gp_drumset->drum(46) = DrumInstrument(TConv::user_name(DrumNum(46)), NoteHeadGroup::HEAD_XCIRCLE, -1, DirectionV::UP);
        gp_drumset->drum(47) = DrumInstrument(TConv::user_name(DrumNum(47)), NoteHeadGroup::HEAD_NORMAL, 4, DirectionV::UP);
        gp_drumset->drum(48) = DrumInstrument(TConv::user_name(DrumNum(48)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
        gp_drumset->drum(49) = DrumInstrument(TConv::user_name(DrumNum(49)), NoteHeadGroup::HEAD_CROSS, -2, DirectionV::UP);

        gp_drumset->drum(50) = DrumInstrument(TConv::user_name(DrumNum(50)), NoteHeadGroup::HEAD_NORMAL, 1, DirectionV::UP);
        gp_drumset->drum(51) = DrumInstrument(TConv::user_name(DrumNum(51)), NoteHeadGroup::HEAD_CROSS, 0, DirectionV::UP);
        gp_drumset->drum(52) = DrumInstrument(TConv::user_name(DrumNum(52)), NoteHeadGroup::HEAD_HEAVY_CROSS_HAT, -3, DirectionV::UP);
        gp_drumset->drum(53) = DrumInstrument(TConv::user_name(DrumNum(53)), NoteHeadGroup::HEAD_DIAMOND, 0, DirectionV::UP);
        gp_drumset->drum(54) = DrumInstrument(TConv::user_name(DrumNum(54)), NoteHeadGroup::HEAD_CROSS, 2, DirectionV::UP);
        gp_drumset->drum(55) = DrumInstrument(TConv::user_name(DrumNum(55)), NoteHeadGroup::HEAD_CROSS, -2, DirectionV::UP);
        gp_drumset->drum(56) = DrumInstrument(TConv::user_name(DrumNum(56)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::UP);
        gp_drumset->drum(57) = DrumInstrument(TConv::user_name(DrumNum(57)), NoteHeadGroup::HEAD_CROSS, -3, DirectionV::UP);
        gp_drumset->drum(58) = DrumInstrument(TConv::user_name(DrumNum(58)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(59) = DrumInstrument(TConv::user_name(DrumNum(59)), NoteHeadGroup::HEAD_CROSS, 2, DirectionV::UP);

        gp_drumset->drum(60) = DrumInstrument(TConv::user_name(DrumNum(60)), NoteHeadGroup::HEAD_NORMAL, 8, DirectionV::UP);
        gp_drumset->drum(61) = DrumInstrument(TConv::user_name(DrumNum(61)), NoteHeadGroup::HEAD_NORMAL, 9, DirectionV::UP);
        gp_drumset->drum(62) = DrumInstrument(TConv::user_name(DrumNum(62)), NoteHeadGroup::HEAD_CROSS, 5, DirectionV::UP);
        gp_drumset->drum(63) = DrumInstrument(TConv::user_name(DrumNum(63)), NoteHeadGroup::HEAD_CROSS, 4, DirectionV::UP);
        gp_drumset->drum(64) = DrumInstrument(TConv::user_name(DrumNum(64)), NoteHeadGroup::HEAD_CROSS, 6, DirectionV::UP);
        gp_drumset->drum(65) = DrumInstrument(TConv::user_name(DrumNum(65)), NoteHeadGroup::HEAD_CROSS, 8, DirectionV::UP);
        gp_drumset->drum(66) = DrumInstrument(TConv::user_name(DrumNum(66)), NoteHeadGroup::HEAD_CROSS, 9, DirectionV::UP);
        gp_drumset->drum(67) = DrumInstrument(TConv::user_name(DrumNum(67)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(68) = DrumInstrument(TConv::user_name(DrumNum(68)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(69) = DrumInstrument(TConv::user_name(DrumNum(69)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);

        gp_drumset->drum(70) = DrumInstrument(TConv::user_name(DrumNum(70)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(71) = DrumInstrument(TConv::user_name(DrumNum(71)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(72) = DrumInstrument(TConv::user_name(DrumNum(72)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(73) = DrumInstrument(TConv::user_name(DrumNum(73)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(74) = DrumInstrument(TConv::user_name(DrumNum(74)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(75) = DrumInstrument(TConv::user_name(DrumNum(75)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(76) = DrumInstrument(TConv::user_name(DrumNum(76)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(77) = DrumInstrument(TConv::user_name(DrumNum(77)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(78) = DrumInstrument(TConv::user_name(DrumNum(78)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(79) = DrumInstrument(TConv::user_name(DrumNum(79)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);

        gp_drumset->drum(80) = DrumInstrument(TConv::user_name(DrumNum(80)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(81) = DrumInstrument(TConv::user_name(DrumNum(81)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(82) = DrumInstrument(TConv::user_name(DrumNum(82)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(83) = DrumInstrument(TConv::user_name(DrumNum(83)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(84) = DrumInstrument(TConv::user_name(DrumNum(84)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(85) = DrumInstrument(TConv::user_name(DrumNum(85)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(86) = DrumInstrument(TConv::user_name(DrumNum(86)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);
        gp_drumset->drum(87) = DrumInstrument(TConv::user_name(DrumNum(87)), NoteHeadGroup::HEAD_NORMAL, 3, DirectionV::UP);

        gp_drumset->drum(91) = DrumInstrument(TConv::user_name(DrumNum(91)), NoteHeadGroup::HEAD_DIAMOND, 3, DirectionV::UP);
        gp_drumset->drum(92) = DrumInstrument(TConv::user_name(DrumNum(46)), NoteHeadGroup::HEAD_CROSS, -1, DirectionV::UP);
        gp_drumset->drum(93) = DrumInstrument(TConv::user_name(DrumNum(93)), NoteHeadGroup::HEAD_CROSS, 0, DirectionV::UP);

        //Additional clutch presets (midi by default can't play this)
        gp_drumset->drum(99) = DrumInstrument(TConv::user_name(DrumNum(99)), NoteHeadGroup::HEAD_TRIANGLE_UP, 1, DirectionV::UP);
        gp_drumset->drum(102) = DrumInstrument(TConv::user_name(DrumNum(102)), NoteHeadGroup::HEAD_TRIANGLE_UP, -1, DirectionV::UP);
    }
}

void init_guitar_pro_percussion_set(const GpDrumSet &ds) {
    using namespace mu::engraving;
    switch (ds.idx) {
        case DrumSetType::DRUMS: break;
        case DrumSetType::AGOGO:
            if (!gp_agogo_set) {
                gp_agogo_set = new Drumset;
                gp_agogo_set->drum(67) = DrumInstrument(TConv::user_name(DrumNum(67)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
                gp_agogo_set->drum(68) = DrumInstrument(TConv::user_name(DrumNum(68)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
            }
            break;
        case DrumSetType::HAND_CLAP:
            if (!gp_hand_clap_set) {
                gp_hand_clap_set = new Drumset;
                gp_hand_clap_set->drum(39) = DrumInstrument(TConv::user_name(DrumNum(39)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::TAMBOURINE:
            if (!gp_tambourine_set) {
                gp_tambourine_set = new Drumset;
                gp_tambourine_set->drum(54) =
                    DrumInstrument(TConv::user_name(DrumNum(54)), NoteHeadGroup::HEAD_TRIANGLE_UP, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::COWBELL:
            if (!gp_cowbell_set) {
                gp_cowbell_set = new Drumset;
                gp_cowbell_set->drum(56) =
                    DrumInstrument(TConv::user_name(DrumNum(56)), NoteHeadGroup::HEAD_TRIANGLE_UP, 2, DirectionV::DOWN);
                gp_cowbell_set->drum(99) =
                    DrumInstrument(TConv::user_name(DrumNum(99)), NoteHeadGroup::HEAD_TRIANGLE_UP, 4, DirectionV::UP);
                gp_cowbell_set->drum(102) =
                    DrumInstrument(TConv::user_name(DrumNum(102)), NoteHeadGroup::HEAD_TRIANGLE_UP, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::VIBRASLAP:
            if (!gp_vibraslap_set) {
                gp_vibraslap_set = new Drumset;
                gp_vibraslap_set->drum(58) = DrumInstrument(TConv::user_name(DrumNum(58)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::BONGOS:
            if (!gp_bongos_set) {
                gp_bongos_set = new Drumset;
                gp_bongos_set->drum(60) = DrumInstrument(TConv::user_name(DrumNum(60)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
                gp_bongos_set->drum(61) = DrumInstrument(TConv::user_name(DrumNum(61)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::CONGAS:
            if (!gp_congas_set) {
                gp_congas_set = new Drumset;
                gp_congas_set->drum(62) = DrumInstrument(TConv::user_name(DrumNum(62)), NoteHeadGroup::HEAD_CROSS, 0, DirectionV::DOWN);
                gp_congas_set->drum(63) = DrumInstrument(TConv::user_name(DrumNum(63)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
                gp_congas_set->drum(64) = DrumInstrument(TConv::user_name(DrumNum(64)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
            }
            break;
        case DrumSetType::TIMBALE:
            if (!gp_timbales_set) {
                gp_timbales_set = new Drumset;
                gp_timbales_set->drum(65) = DrumInstrument(TConv::user_name(DrumNum(65)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
                gp_timbales_set->drum(66) = DrumInstrument(TConv::user_name(DrumNum(66)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
            }
            break;
        case DrumSetType::CABASA:
            if (!gp_cabasa_set) {
                gp_cabasa_set = new Drumset;
                gp_cabasa_set->drum(69) = DrumInstrument(TConv::user_name(DrumNum(69)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::MARACAS:
            if (!gp_maracas_set) {
                gp_maracas_set = new Drumset;
                gp_maracas_set->drum(70) = DrumInstrument(TConv::user_name(DrumNum(70)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::WHISTLE:
            if (!gp_whistle_set) {
                gp_whistle_set = new Drumset;
                gp_whistle_set->drum(71) = DrumInstrument(TConv::user_name(DrumNum(71)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
                gp_whistle_set->drum(72) = DrumInstrument(TConv::user_name(DrumNum(72)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
            }
            break;
        case DrumSetType::GUIRO:
            if (!gp_guiro_set) {
                gp_guiro_set = new Drumset;
                gp_guiro_set->drum(73) = DrumInstrument(TConv::user_name(DrumNum(73)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
                gp_guiro_set->drum(74) = DrumInstrument(TConv::user_name(DrumNum(74)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::CLAVES:
            if (!gp_claves_set) {
                gp_claves_set = new Drumset;
                gp_claves_set->drum(75) = DrumInstrument(TConv::user_name(DrumNum(75)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::WOODBLOCK:
            if (!gp_woodblock_set) {
                gp_woodblock_set = new Drumset;
                gp_woodblock_set->drum(76) = DrumInstrument(TConv::user_name(DrumNum(76)), NoteHeadGroup::HEAD_NORMAL, 2, DirectionV::UP);
                gp_woodblock_set->drum(77) = DrumInstrument(TConv::user_name(DrumNum(77)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::CUICA:
            if (!gp_cuica_set) {
                gp_cuica_set = new Drumset;
                gp_cuica_set->drum(78) = DrumInstrument(TConv::user_name(DrumNum(78)), NoteHeadGroup::HEAD_CROSS, 0, DirectionV::DOWN);
                gp_cuica_set->drum(79) = DrumInstrument(TConv::user_name(DrumNum(79)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::TRIANGLE:
            if (!gp_triangle_set) {
                gp_triangle_set = new Drumset;
                gp_triangle_set->drum(80) = DrumInstrument(TConv::user_name(DrumNum(80)), NoteHeadGroup::HEAD_CROSS, 0, DirectionV::DOWN);
                gp_triangle_set->drum(81) = DrumInstrument(TConv::user_name(DrumNum(81)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::SHAKER:
            if (!gp_shaker_set) {
                gp_shaker_set = new Drumset;
                gp_shaker_set->drum(82) = DrumInstrument(TConv::user_name(DrumNum(82)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::JINGLE_BELL:
            if (!gp_jingle_bell_set) {
                gp_jingle_bell_set = new Drumset;
                gp_jingle_bell_set->drum(83) =
                    DrumInstrument(TConv::user_name(DrumNum(83)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::BELL_TREE:
            if (!gp_bell_tree_set) {
                gp_bell_tree_set = new Drumset;
                gp_bell_tree_set->drum(84) = DrumInstrument(TConv::user_name(DrumNum(84)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::CASTANETS:
            if (!gp_castanets_set) {
                gp_castanets_set = new Drumset;
                gp_castanets_set->drum(85) = DrumInstrument(TConv::user_name(DrumNum(85)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
            }
            break;
        case DrumSetType::SURDO:
            if (!gp_surdo_set) {
                gp_surdo_set = new Drumset;
                gp_surdo_set->drum(86) = DrumInstrument(TConv::user_name(DrumNum(86)), NoteHeadGroup::HEAD_NORMAL, 0, DirectionV::DOWN);
                gp_surdo_set->drum(87) = DrumInstrument(TConv::user_name(DrumNum(87)), NoteHeadGroup::HEAD_CROSS, 0, DirectionV::DOWN);
            }
            break;
    }
}

void set_instrument_drumset(mu::engraving::Instrument *instrument, const GpDrumSet &ds) {
    switch (ds.idx) {
        case DrumSetType::DRUMS: instrument->set_drumset(gp_drumset); break;
        case DrumSetType::AGOGO: instrument->set_drumset(gp_agogo_set); break;
        case DrumSetType::HAND_CLAP: instrument->set_drumset(gp_hand_clap_set); break;
        case DrumSetType::TAMBOURINE: instrument->set_drumset(gp_tambourine_set); break;
        case DrumSetType::COWBELL: instrument->set_drumset(gp_cowbell_set); break;
        case DrumSetType::VIBRASLAP: instrument->set_drumset(gp_vibraslap_set); break;
        case DrumSetType::BONGOS: instrument->set_drumset(gp_bongos_set); break;
        case DrumSetType::CONGAS: instrument->set_drumset(gp_congas_set); break;
        case DrumSetType::TIMBALE: instrument->set_drumset(gp_timbales_set); break;
        case DrumSetType::CABASA: instrument->set_drumset(gp_cabasa_set); break;
        case DrumSetType::MARACAS: instrument->set_drumset(gp_maracas_set); break;
        case DrumSetType::WHISTLE: instrument->set_drumset(gp_whistle_set); break;
        case DrumSetType::GUIRO: instrument->set_drumset(gp_guiro_set); break;
        case DrumSetType::CLAVES: instrument->set_drumset(gp_claves_set); break;
        case DrumSetType::WOODBLOCK: instrument->set_drumset(gp_woodblock_set); break;
        case DrumSetType::CUICA: instrument->set_drumset(gp_cuica_set); break;
        case DrumSetType::TRIANGLE: instrument->set_drumset(gp_triangle_set); break;
        case DrumSetType::SHAKER: instrument->set_drumset(gp_shaker_set); break;
        case DrumSetType::JINGLE_BELL: instrument->set_drumset(gp_jingle_bell_set); break;
        case DrumSetType::BELL_TREE: instrument->set_drumset(gp_bell_tree_set); break;
        case DrumSetType::CASTANETS: instrument->set_drumset(gp_castanets_set); break;
        case DrumSetType::SURDO: instrument->set_drumset(gp_surdo_set); break;
    }
}
} // namespace mu::iex::guitarpro::drumset
