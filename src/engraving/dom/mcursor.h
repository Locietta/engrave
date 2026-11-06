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

#ifndef MU_ENGRAVING_MCURSOR_H
#define MU_ENGRAVING_MCURSOR_H

#include "../types/types.h"
#include "types/string.h"

namespace mu::engraving {
struct EngravingServices;
struct MasterScore;
struct TDuration;
struct TimeSig;
struct Chord;
enum class Key : signed char;

//---------------------------------------------------------
//   MCursor
//---------------------------------------------------------

struct MCursor {
    MCursor(MasterScore *s = 0);
    void create_score(const EngravingServices &services, const String &s);

    void add_part(const String &instrument);
    Chord *add_chord(int pitch, const TDuration &duration);
    void add_key_sig(Key);
    TimeSig *add_time_sig(const Fraction &);

    void move(int track, const Fraction &tick);

    MasterScore *score() const { return score_; }

    void set_score(MasterScore *s) { score_ = s; }

    void set_time_sig(Fraction f) { sig_ = f; }

private:

    void create_measures();

    MasterScore *score_ = nullptr;
    Fraction tick_;
    int track_ = 0;
    Fraction sig_;
};
} // namespace mu::engraving
#endif
