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

#ifndef MU_ENGRAVING_CLEFLIST_H
#define MU_ENGRAVING_CLEFLIST_H

#include "clef.h"

namespace mu::engraving {
//---------------------------------------------------------
//   ClefList
//---------------------------------------------------------

struct ClefList : std::map<int, ClefTypeList> {
private:
    OBJECT_ALLOCATOR(engraving, ClefList)
public:
    ClefList() {}

    ClefTypeList clef(int tick) const;
    void set_clef(int tick, ClefTypeList);
    int next_clef_tick(int tick) const;
    int current_clef_tick(int tick) const;
};
} // namespace mu::engraving
#endif
