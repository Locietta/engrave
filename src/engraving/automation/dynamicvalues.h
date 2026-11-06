/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include <unordered_map>
#include <utility>

#include "engraving/types/types.h"

namespace mu::engraving {
// Normalized [0.0, 1.0] dynamic levels, aligned with MPE dynamic level percentages (5% steps starting at 17.5%)
inline const std::unordered_map<DynamicType, real_t> ordinary_dynamic_values{
    {DynamicType::N, 0.000},    {DynamicType::PPPPPP, 0.175}, {DynamicType::PPPPP, 0.225},  {DynamicType::PPPP, 0.275},
    {DynamicType::PPP, 0.325},  {DynamicType::PP, 0.375},     {DynamicType::P, 0.425},      {DynamicType::MP, 0.475},
    {DynamicType::MF, 0.525},   {DynamicType::F, 0.575},      {DynamicType::FF, 0.625},     {DynamicType::FFF, 0.675},
    {DynamicType::FFFF, 0.725}, {DynamicType::FFFFF, 0.775},  {DynamicType::FFFFFF, 0.825},
};

inline const std::unordered_map<DynamicType, real_t> single_note_dynamic_values{
    {DynamicType::SF, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::F)},
    {DynamicType::SFZ, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::F)},
    {DynamicType::SFF, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::FF)},
    {DynamicType::SFFZ, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::FF)},
    {DynamicType::SFFF, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::FFF)},
    {DynamicType::SFFFZ, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::FFF)},
    {DynamicType::RFZ, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::F)},
    {DynamicType::RF, ::mu::engraving::ordinary_dynamic_values.at(DynamicType::F)},
};

inline const std::unordered_map<DynamicType, std::pair<real_t, real_t>> compound_dynamic_values{
    {DynamicType::FP,
     {::mu::engraving::ordinary_dynamic_values.at(DynamicType::F), ::mu::engraving::ordinary_dynamic_values.at(DynamicType::P)}},
    {DynamicType::PF,
     {::mu::engraving::ordinary_dynamic_values.at(DynamicType::P), ::mu::engraving::ordinary_dynamic_values.at(DynamicType::F)}},
    {DynamicType::SFP,
     {::mu::engraving::ordinary_dynamic_values.at(DynamicType::F), ::mu::engraving::ordinary_dynamic_values.at(DynamicType::P)}},
    {DynamicType::SFPP,
     {::mu::engraving::ordinary_dynamic_values.at(DynamicType::F), ::mu::engraving::ordinary_dynamic_values.at(DynamicType::PP)}},
};
} // namespace mu::engraving
