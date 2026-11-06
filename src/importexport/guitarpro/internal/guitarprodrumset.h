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

#ifndef MU_IMPORTEXPORT_GUITARPRODRUMSET_H
#define MU_IMPORTEXPORT_GUITARPRODRUMSET_H

#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace mu::engraving {
struct Drumset;
struct Instrument;
} // namespace mu::engraving

namespace mu::iex::guitarpro::drumset {
enum class DrumSetType : uint16_t {
    DRUMS = 0,
    AGOGO,
    HAND_CLAP,
    TAMBOURINE,
    COWBELL,
    VIBRASLAP,
    BONGOS,
    CONGAS,
    TIMBALE,
    CABASA,
    MARACAS,
    WHISTLE,
    GUIRO,
    CLAVES,
    WOODBLOCK,
    CUICA,
    TRIANGLE,
    SHAKER,
    JINGLE_BELL,
    BELL_TREE,
    CASTANETS,
    SURDO
};

struct GpDrumSet {
    std::string_view name;
    DrumSetType idx;
    uint8_t num_lines;
};

static const std::unordered_map<std::string_view, GpDrumSet> perc_staff_lines_from_instrument = {
    {"Agogo", {"Agogo", DrumSetType::AGOGO, 2}},
    {"Hand Clap", {"Hand Clap", DrumSetType::HAND_CLAP, 1}},
    {"Tambourine", {"Tambourine", DrumSetType::TAMBOURINE, 1}},
    {"Cowbell", {"Cowbell", DrumSetType::COWBELL, 3}},
    {"Vibraslap", {"Vibraslap", DrumSetType::VIBRASLAP, 1}},
    {"Bongos", {"Bongos", DrumSetType::BONGOS, 2}},
    {"Congas", {"Congas", DrumSetType::CONGAS, 2}},
    {"Timbale", {"Timbale", DrumSetType::TIMBALE, 2}},
    {"Cabasa", {"Cabasa", DrumSetType::CABASA, 1}},
    {"Maracas", {"Maracas", DrumSetType::MARACAS, 2}},
    {"Whistle", {"Whistle", DrumSetType::WHISTLE, 2}},
    {"Guiro", {"Guiro", DrumSetType::GUIRO, 2}},
    {"Claves", {"Claves", DrumSetType::CLAVES, 1}},
    {"Woodblock", {"Woodblock", DrumSetType::WOODBLOCK, 2}},
    {"Cuica", {"Cuica", DrumSetType::CUICA, 1}},
    {"Triangle", {"Triangle", DrumSetType::TRIANGLE, 1}},
    {"Shaker", {"Shaker", DrumSetType::SHAKER, 1}},
    {"Jingle Bell", {"Jingle Bell", DrumSetType::JINGLE_BELL, 1}},
    {"Bell Tree", {"Bell Tree", DrumSetType::BELL_TREE, 1}},
    {"Castanets", {"Castanets", DrumSetType::CASTANETS, 1}},
    {"Surdo", {"Surdo", DrumSetType::SURDO, 1}},
};

inline mu::engraving::Drumset *gp_drumset = nullptr;
inline mu::engraving::Drumset *gp_agogo_set = nullptr;
inline mu::engraving::Drumset *gp_hand_clap_set = nullptr;
inline mu::engraving::Drumset *gp_tambourine_set = nullptr;
inline mu::engraving::Drumset *gp_cowbell_set = nullptr;
inline mu::engraving::Drumset *gp_vibraslap_set = nullptr;
inline mu::engraving::Drumset *gp_bongos_set = nullptr;
inline mu::engraving::Drumset *gp_congas_set = nullptr;
inline mu::engraving::Drumset *gp_timbales_set = nullptr;
inline mu::engraving::Drumset *gp_cabasa_set = nullptr;
inline mu::engraving::Drumset *gp_maracas_set = nullptr;
inline mu::engraving::Drumset *gp_whistle_set = nullptr;
inline mu::engraving::Drumset *gp_guiro_set = nullptr;
inline mu::engraving::Drumset *gp_claves_set = nullptr;
inline mu::engraving::Drumset *gp_woodblock_set = nullptr;
inline mu::engraving::Drumset *gp_cuica_set = nullptr;
inline mu::engraving::Drumset *gp_triangle_set = nullptr;
inline mu::engraving::Drumset *gp_shaker_set = nullptr;
inline mu::engraving::Drumset *gp_jingle_bell_set = nullptr;
inline mu::engraving::Drumset *gp_bell_tree_set = nullptr;
inline mu::engraving::Drumset *gp_castanets_set = nullptr;
inline mu::engraving::Drumset *gp_surdo_set = nullptr;

void init_guitar_pro_drumset();
void init_guitar_pro_percussion_set(const GpDrumSet &ds);
void set_instrument_drumset(mu::engraving::Instrument *instrument, const GpDrumSet &ds);
} // namespace mu::iex::guitarpro::drumset
#endif
