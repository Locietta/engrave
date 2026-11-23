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

#pragma once

#include <functional>
#include <limits>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <gtl/btree.hpp>

#include "../../types/types.h"
#include "event.h"
#include "global/unique_fn.h"

namespace mu::engraving {
struct PitchWheelRenderer {
    struct PitchWheelFunction {
        int32_t m_start_tick = 0;
        int32_t m_end_tick = 0;
        muse::UniqueFn<int(uint32_t) const> func;
    };

    PitchWheelRenderer(PitchWheelSpecs input_wheel_spec);

    void add_pitch_wheel_function(PitchWheelFunction function, uint32_t channel, staff_idx_t staff_idx, MidiInstrumentEffect effect);

    EventsHolder render_pitch_wheel() const noexcept;

    static void generate_ranges(const std::vector<PitchWheelFunction> &functions, gtl::btree_map<int, int, std::greater<>> &ranges);

private:

    struct PitchWheelFunctions {
        int32_t start_tick = std::numeric_limits<int32_t>::max();
        int32_t end_tick = 0;
        std::vector<PitchWheelFunction> functions;
    };

    void render_channel_pitch_wheel(EventsHolder &pitch_wheel_events, const PitchWheelFunctions &functions,
                                    uint32_t channel) const noexcept;

    int32_t find_next_start_tick(const std::vector<PitchWheelFunction> &functions) const noexcept;

    int32_t calculate_pitch_bend(const std::vector<PitchWheelFunction> &functions, int32_t tick) const noexcept;

    gtl::btree_map<uint32_t /*channel*/, PitchWheelFunctions> functions_value;

    PitchWheelSpecs wheel_spec_value;

    ankerl::unordered_dense::map<int, MidiInstrumentEffect> effect_by_channel;
    ankerl::unordered_dense::map<int, staff_idx_t> staff_idx_by_channel;
};
} // namespace mu::engraving
