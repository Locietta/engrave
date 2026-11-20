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

#include <array>
#include <optional>
#include <utility>

#include "engraving/types/types.h"

namespace mu::engraving {
// Normalized dynamic levels follow the MPE percentages, in 5% steps from 17.5%.
constexpr std::optional<real_t> ordinary_dynamic_value(DynamicType type) {
    constexpr std::array<double, 14> levels{
        0.175, 0.225, 0.275, 0.325, 0.375, 0.425, 0.475, 0.525, 0.575, 0.625, 0.675, 0.725, 0.775, 0.825,
    };
    if (type == DynamicType::N) {
        return real_t::make(0.000);
    }
    constexpr auto first = static_cast<size_t>(DynamicType::PPPPPP);
    static_assert(static_cast<size_t>(DynamicType::FFFFFF) - first + 1 == levels.size());
    const auto index = static_cast<size_t>(type) - first;
    if (index < levels.size()) {
        return real_t::make(levels[index]);
    }
    return std::nullopt;
}

constexpr std::optional<real_t> single_note_dynamic_value(DynamicType type) {
    switch (type) {
        case DynamicType::SF:
        case DynamicType::SFZ:
        case DynamicType::RFZ:
        case DynamicType::RF: return ordinary_dynamic_value(DynamicType::F);
        case DynamicType::SFF:
        case DynamicType::SFFZ: return ordinary_dynamic_value(DynamicType::FF);
        case DynamicType::SFFF:
        case DynamicType::SFFFZ: return ordinary_dynamic_value(DynamicType::FFF);
        default: return std::nullopt;
    }
}

constexpr std::optional<std::pair<real_t, real_t>> compound_dynamic_value(DynamicType type) {
    constexpr real_t p = *ordinary_dynamic_value(DynamicType::P);
    constexpr real_t pp = *ordinary_dynamic_value(DynamicType::PP);
    constexpr real_t f = *ordinary_dynamic_value(DynamicType::F);
    switch (type) {
        case DynamicType::FP:
        case DynamicType::SFP: return {{f, p}};
        case DynamicType::PF: return {{p, f}};
        case DynamicType::SFPP: return {{f, pp}};
        default: return std::nullopt;
    }
}
} // namespace mu::engraving
