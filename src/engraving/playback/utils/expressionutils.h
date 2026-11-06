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

#ifndef MU_ENGRAVING_DYNAMICUTILS_H
#define MU_ENGRAVING_DYNAMICUTILS_H

#include "../types/types.h"
#include "mpe/mpetypes.h"

namespace mu::engraving {
constexpr muse::mpe::dynamic_level_t natural_dynamic_level = muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::NATURAL);

inline const std::unordered_map<DynamicType, muse::mpe::dynamic_level_t> ordinary_dynamic_levels{
    {DynamicType::N, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::PPPPPPPPP)},
    {DynamicType::PPPPPP, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::PPPPPP)},
    {DynamicType::PPPPP, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::PPPPP)},
    {DynamicType::PPPP, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::PPPP)},
    {DynamicType::PPP, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::PPP)},
    {DynamicType::PP, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::PP)},
    {DynamicType::P, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::P)},
    {DynamicType::MP, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::MP)},
    {DynamicType::MF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::MF)},
    {DynamicType::F, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::F)},
    {DynamicType::FF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FF)},
    {DynamicType::FFF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FFF)},
    {DynamicType::FFFF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FFFF)},
    {DynamicType::FFFFF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FFFFF)},
    {DynamicType::FFFFFF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FFFFFF)}};

inline bool is_ordinary_dynamic_type(const DynamicType type) {
    return ::mu::engraving::ordinary_dynamic_levels.find(type) != ::mu::engraving::ordinary_dynamic_levels.cend();
}

inline muse::mpe::dynamic_level_t
dynamic_level_from_ordinary_type(const DynamicType type,
                                 const muse::mpe::dynamic_level_t def_level = ::mu::engraving::natural_dynamic_level) {
    auto search = ::mu::engraving::ordinary_dynamic_levels.find(type);
    if (search != ::mu::engraving::ordinary_dynamic_levels.cend()) {
        return search->second;
    }

    return def_level;
}

inline const std::unordered_map<DynamicType, muse::mpe::dynamic_level_t> single_note_dynamic_levels{
    {DynamicType::SF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::F)},
    {DynamicType::SFZ, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::F)},
    {DynamicType::SFF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FF)},
    {DynamicType::SFFZ, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FF)},
    {DynamicType::SFFF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FFF)},
    {DynamicType::SFFFZ, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::FFF)},
    {DynamicType::RFZ, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::F)},
    {DynamicType::RF, muse::mpe::dynamic_level_from_type(muse::mpe::DynamicType::F)}};

inline bool is_single_note_dynamic_type(const DynamicType type) {
    return ::mu::engraving::single_note_dynamic_levels.find(type) != ::mu::engraving::single_note_dynamic_levels.cend();
}

inline muse::mpe::dynamic_level_t
dynamic_level_from_single_note_type(const DynamicType type,
                                    const muse::mpe::dynamic_level_t def_level = ::mu::engraving::natural_dynamic_level) {
    auto search = ::mu::engraving::single_note_dynamic_levels.find(type);
    if (search != ::mu::engraving::single_note_dynamic_levels.cend()) {
        return search->second;
    }

    return def_level;
}

struct CompoundDynamic {
    DynamicType from = DynamicType::OTHER;
    DynamicType to = DynamicType::OTHER;

    bool is_valid() const { return from != DynamicType::OTHER && to != DynamicType::OTHER; }
};

inline const std::unordered_map<DynamicType, CompoundDynamic> compound_dynamics{{DynamicType::FP, {DynamicType::F, DynamicType::P}},
                                                                                {DynamicType::PF, {DynamicType::P, DynamicType::F}},
                                                                                {DynamicType::SFP, {DynamicType::F, DynamicType::P}},
                                                                                {DynamicType::SFPP, {DynamicType::F, DynamicType::PP}}};

inline bool is_compound_dynamic_type(const DynamicType type) {
    return ::mu::engraving::compound_dynamics.find(type) != ::mu::engraving::compound_dynamics.cend();
}

inline const CompoundDynamic &compound_dynamic_from_type(const DynamicType type) {
    auto search = ::mu::engraving::compound_dynamics.find(type);
    if (search != ::mu::engraving::compound_dynamics.cend()) {
        return search->second;
    }

    static CompoundDynamic empty;
    return empty;
}

inline muse::mpe::ArticulationType articulation_from_play_tech_type(const PlayingTechniqueType technique) {
    switch (technique) {
        case PlayingTechniqueType::NATURAL: return muse::mpe::ArticulationType::STANDARD;
        case PlayingTechniqueType::PIZZICATO: return muse::mpe::ArticulationType::PIZZICATO;
        case PlayingTechniqueType::OPEN: return muse::mpe::ArticulationType::OPEN;
        case PlayingTechniqueType::MUTE: return muse::mpe::ArticulationType::MUTE;
        case PlayingTechniqueType::TREMOLO: return muse::mpe::ArticulationType::TREMOLO64TH;
        case PlayingTechniqueType::DETACHE: return muse::mpe::ArticulationType::DETACHE;
        case PlayingTechniqueType::MARTELE: return muse::mpe::ArticulationType::MARTELE;
        case PlayingTechniqueType::COL_LEGNO: return muse::mpe::ArticulationType::COL_LEGNO;
        case PlayingTechniqueType::SUL_PONTICELLO: return muse::mpe::ArticulationType::SUL_PONT;
        case PlayingTechniqueType::SUL_TASTO: return muse::mpe::ArticulationType::SUL_TASTO;
        case PlayingTechniqueType::DISTORTION: return muse::mpe::ArticulationType::DISTORTION;
        case PlayingTechniqueType::OVERDRIVE: return muse::mpe::ArticulationType::OVERDRIVE;
        case PlayingTechniqueType::HARMONICS: return muse::mpe::ArticulationType::HARMONIC;
        case PlayingTechniqueType::JAZZ_TONE: return muse::mpe::ArticulationType::JAZZ_TONE;
        case PlayingTechniqueType::VIBRATO: return muse::mpe::ArticulationType::VIBRATO;
        case PlayingTechniqueType::LEGATO: return muse::mpe::ArticulationType::LEGATO;
        case PlayingTechniqueType::HANDBELLS_SWING: return muse::mpe::ArticulationType::SWING;
        case PlayingTechniqueType::HANDBELLS_SWING_UP: return muse::mpe::ArticulationType::SWING;
        case PlayingTechniqueType::HANDBELLS_SWING_DOWN: return muse::mpe::ArticulationType::SWING;
        case PlayingTechniqueType::HANDBELLS_ECHO1: return muse::mpe::ArticulationType::ECHO;
        case PlayingTechniqueType::HANDBELLS_ECHO2: return muse::mpe::ArticulationType::ECHO;
        case PlayingTechniqueType::HANDBELLS_LV: return muse::mpe::ArticulationType::PEDAL;
        case PlayingTechniqueType::HANDBELLS_DAMP: return muse::mpe::ArticulationType::STANDARD;
        case PlayingTechniqueType::HANDBELLS_R: return muse::mpe::ArticulationType::RING;
        case PlayingTechniqueType::UNDEFINED: return muse::mpe::ArticulationType::UNDEFINED;
    }

    return muse::mpe::ArticulationType::UNDEFINED;
}

static const muse::mpe::ArticulationTypeSet grace_note_articulation_types{
    muse::mpe::ArticulationType::ACCIACCATURA,
    muse::mpe::ArticulationType::POST_APPOGGIATURA,
    muse::mpe::ArticulationType::PRE_APPOGGIATURA,
};

inline bool is_grace_note_placed_before_principal_note(const muse::mpe::ArticulationType type) {
    return type == muse::mpe::ArticulationType::ACCIACCATURA || type == muse::mpe::ArticulationType::PRE_APPOGGIATURA;
}

inline void update_articulation_boundaries(const muse::mpe::ArticulationType type, const muse::mpe::timestamp_t nominal_timestamp,
                                           const muse::mpe::duration_t nominal_duration, muse::mpe::ArticulationMap &articulations) {
    if (articulations.empty()) {
        return;
    }

    const muse::mpe::ArticulationAppliedData &articulation_data = articulations.at(type);

    muse::mpe::timestamp_t articulation_occupied_from = nominal_timestamp - articulation_data.meta.timestamp;
    muse::mpe::timestamp_t articulation_occupied_to = nominal_timestamp + nominal_duration - articulation_data.meta.timestamp;

    articulations.update_occupied_range(type,
                                        muse::mpe::occupied_percentage(articulation_occupied_from, articulation_data.meta.overall_duration),
                                        muse::mpe::occupied_percentage(articulation_occupied_to, articulation_data.meta.overall_duration));
}
} // namespace mu::engraving

#endif // MU_ENGRAVING_DYNAMICUTILS_H
