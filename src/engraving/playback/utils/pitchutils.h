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

#include "engraving/dom/pitchspelling.h"
#include "mpe/mpetypes.h"

namespace mu::engraving {
inline muse::mpe::PitchClass pitch_class_from_tpc(const int tpc) {
    switch (tpc) {
        // C
        case TPC_D_BB:
        case TPC_C:
        case TPC_B_S:
        case TPC_A_SSS: return muse::mpe::PitchClass::C;
        case TPC_E_BBB:
        case TPC_D_B:
        case TPC_C_S:
        case TPC_B_SS: return muse::mpe::PitchClass::C_SHARP;

        // D
        case TPC_F_BBB:
        case TPC_E_BB:
        case TPC_D:
        case TPC_C_SS:
        case TPC_B_SSS: return muse::mpe::PitchClass::D;
        case TPC_F_BB:
        case TPC_E_B:
        case TPC_D_S:
        case TPC_C_SSS: return muse::mpe::PitchClass::D_SHARP;

        // E
        case TPC_G_BBB:
        case TPC_F_B:
        case TPC_E:
        case TPC_D_SS: return muse::mpe::PitchClass::E;

        // F
        case TPC_G_BB:
        case TPC_F:
        case TPC_E_S:
        case TPC_D_SSS: return muse::mpe::PitchClass::F;
        case TPC_A_BBB:
        case TPC_G_B:
        case TPC_F_S:
        case TPC_E_SS: return muse::mpe::PitchClass::F_SHARP;

        // G
        case TPC_A_BB:
        case TPC_G:
        case TPC_F_SS:
        case TPC_E_SSS: return muse::mpe::PitchClass::G;
        case TPC_B_BBB:
        case TPC_A_B:
        case TPC_G_S:
        case TPC_F_SSS: return muse::mpe::PitchClass::G_SHARP;

        // A
        case TPC_C_BBB:
        case TPC_B_BB:
        case TPC_A:
        case TPC_G_SS: return muse::mpe::PitchClass::A;
        case TPC_C_BB:
        case TPC_B_B:
        case TPC_A_S:
        case TPC_G_SSS: return muse::mpe::PitchClass::A_SHARP;

        // B
        case TPC_D_BBB:
        case TPC_C_B:
        case TPC_B:
        case TPC_A_SS: return muse::mpe::PitchClass::B;

        default: return muse::mpe::PitchClass::UNDEFINED;
    }
}

inline muse::mpe::octave_t actual_octave(const int nominal_octave, const muse::mpe::PitchClass nominal_pitch_class,
                                         const AccidentalVal accidental) {
    int shift = static_cast<int>(nominal_pitch_class) - static_cast<int>(accidental);

    constexpr int lower_bound = 0;
    constexpr int upper_bound = muse::mpe::steps_per_octave - 1;

    if (shift < lower_bound) {
        return static_cast<muse::mpe::octave_t>(nominal_octave + 1);
    }

    if (shift > upper_bound) {
        return static_cast<muse::mpe::octave_t>(nominal_octave - 1);
    }

    return static_cast<muse::mpe::octave_t>(nominal_octave);
}

inline muse::mpe::pitch_level_t note_pitch_level(const int note_tpc, const int note_octave, const double tuning_cents = 0.0) {
    muse::mpe::PitchClass pitch_class = pitch_class_from_tpc(note_tpc);

    double tuning_factor = tuning_cents / 100.0;

    muse::mpe::pitch_level_t result = muse::mpe::pitch_level(pitch_class, actual_octave(note_octave, pitch_class, tpc2alter(note_tpc)));
    result += tuning_factor * muse::mpe::pitch_level_step;

    return result;
}
} // namespace mu::engraving
