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

#include "types/string.h"
#include "../types/types.h"

namespace mu::engraving {
struct EngravingServices;
class MasterScore;
class TDuration;
class TimeSig;
class Chord;
enum class Key : signed char;

//---------------------------------------------------------
//   MCursor
//---------------------------------------------------------

class MCursor {
public:
    MCursor(MasterScore *s = 0);
    void createScore(const EngravingServices &services, const String &s);

    void addPart(const String &instrument);
    Chord *addChord(int pitch, const TDuration &duration);
    void addKeySig(Key);
    TimeSig *addTimeSig(const Fraction &);

    void move(int track, const Fraction &tick);

    MasterScore *score() const { return score_; }

    void setScore(MasterScore *s) { score_ = s; }

    void setTimeSig(Fraction f) { sig_ = f; }

private:

    void createMeasures();

    MasterScore *score_ = nullptr;
    Fraction tick_;
    int track_ = 0;
    Fraction sig_;
};
} // namespace mu::engraving
#endif
