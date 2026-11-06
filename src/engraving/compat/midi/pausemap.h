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

#ifndef MU_ENGRAVING_PAUSEMAP_H
#define MU_ENGRAVING_PAUSEMAP_H

#include <map>

/**
 \file
 Definition of class PauseMap.
*/

namespace mu::engraving {
class Score;

//---------------------------------------------------
//   PauseMap
//    MIDI files cannot contain pauses so need to insert
//    extra ticks extra ticks and tempo changes instead.
//---------------------------------------------------
class PauseMap : std::map<int, int> {
public:
    void calculate(const engraving::Score *s);

    inline int tick_with_pauses(int utick) const { return utick + offset_at_u_tick(utick); }

    const std::map<int, double> &tempo_events() const { return tempo_events_; }

private:
    int offset_at_u_tick(int utick) const;

    std::map<int, double> tempo_events_;
};
} // namespace mu::engraving
#endif
