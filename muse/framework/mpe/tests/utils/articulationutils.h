/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#ifndef MUSE_MPE_TESTS_ARTICULATIONSPROFILEEXAMPLE_H
#define MUSE_MPE_TESTS_ARTICULATIONSPROFILEEXAMPLE_H

#include "mpe/mpetypes.h"

namespace muse::mpe::tests {
inline ArrangementPattern create_arrangement_pattern(const duration_percentage_t duration_factor,
                                                     const duration_percentage_t timestamp_offset) {
    ArrangementPattern result;
    result.duration_factor_value = duration_factor;
    result.timestamp_offset_value = timestamp_offset;
    return result;
}

inline PitchPattern create_simple_pitch_pattern(const pitch_level_t increment_diff = 0) {
    PitchPattern result;

    for (size_t i = 0; i < ArticulationMap::expected_size; ++i) {
        result.pitch_offset_map.insert_or_assign(static_cast<int>(i) * ::muse::mpe::ten_percent, static_cast<int>(i) * increment_diff);
    }

    return result;
}

inline ExpressionPattern create_simple_expression_pattern(const dynamic_level_t amplitude_level) {
    ExpressionPattern result;

    double amplitude_sqrt = std::sqrt(amplitude_level);

    for (size_t i = 0; i < ArticulationMap::expected_size; ++i) {
        duration_percentage_t current_pos = static_cast<int>(i) * ::muse::mpe::ten_percent;
        dynamic_level_t value =
            amplitude_level -
            static_cast<dynamic_level_t>(
                std::pow((2 * (amplitude_sqrt / static_cast<float>(::muse::mpe::hundred_percent)) * current_pos) - amplitude_sqrt, 2));

        result.dynamic_offset_map.insert_or_assign(current_pos, value);
    }

    return result;
}
} // namespace muse::mpe::tests

#endif // MUSE_MPE_TESTS_ARTICULATIONSPROFILEEXAMPLE_H
