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

#ifndef MU_ENGRAVING_SWING_H
#define MU_ENGRAVING_SWING_H

namespace mu::engraving {
struct Chord;
struct SwingParameters;

struct Swing {
    struct ChordDurationAdjustment {
        double remaining_duration_multiplier = 0.;
        double duration_multiplier = 0.;

        bool is_null() const;
    };

    static ChordDurationAdjustment apply_swing(const Chord *chord, const SwingParameters &params);
    static void swing_adjust_params(const Chord *chord, const SwingParameters &params, int &on_time, int &gate_time);
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_SWING_H
