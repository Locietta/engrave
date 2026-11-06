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

#include "hook.h"

#include "chord.h"
#include "log.h"
#include "style/style.h"

using namespace mu;
using namespace mu::engraving;

Hook::Hook(DummyParentOr<Chord> parent) : Symbol(ElementType::HOOK, parent, ElementFlag::NOTHING) { set_z(int(type()) * 100); }

EngravingItem *Hook::element_base() const { return chord(); }

void Hook::set_hook_type(int i) {
    bool straight = style().style_b(Sid::USE_STRAIGHT_NOTE_FLAGS);
    hook_type_ = i;
    set_sym(sym_id_for_hook_index(i, straight));
}

PointF Hook::smufl_anchor() const {
    return sym_smufl_anchor(sym_, chord()->up() ? SmuflAnchorId::STEM_UP_NW : SmuflAnchorId::STEM_DOWN_SW);
}

SymId Hook::sym_id_for_hook_index(int index, bool straight) {
    switch (index) {
        case 0: return SymId::NO_SYM;
        case 1: return straight ? SymId::FLAG8TH_UP_STRAIGHT : SymId::FLAG8TH_UP;
        case 2: return straight ? SymId::FLAG16TH_UP_STRAIGHT : SymId::FLAG16TH_UP;
        case 3: return straight ? SymId::FLAG32ND_UP_STRAIGHT : SymId::FLAG32ND_UP;
        case 4: return straight ? SymId::FLAG64TH_UP_STRAIGHT : SymId::FLAG64TH_UP;
        case 5: return straight ? SymId::FLAG128TH_UP_STRAIGHT : SymId::FLAG128TH_UP;
        case 6: return straight ? SymId::FLAG256TH_UP_STRAIGHT : SymId::FLAG256TH_UP;
        case 7: return straight ? SymId::FLAG512TH_UP_STRAIGHT : SymId::FLAG512TH_UP;
        case 8: return straight ? SymId::FLAG1024TH_UP_STRAIGHT : SymId::FLAG1024TH_UP;

        case -1: return straight ? SymId::FLAG8TH_DOWN_STRAIGHT : SymId::FLAG8TH_DOWN;
        case -2: return straight ? SymId::FLAG16TH_DOWN_STRAIGHT : SymId::FLAG16TH_DOWN;
        case -3: return straight ? SymId::FLAG32ND_DOWN_STRAIGHT : SymId::FLAG32ND_DOWN;
        case -4: return straight ? SymId::FLAG64TH_DOWN_STRAIGHT : SymId::FLAG64TH_DOWN;
        case -5: return straight ? SymId::FLAG128TH_DOWN_STRAIGHT : SymId::FLAG128TH_DOWN;
        case -6: return straight ? SymId::FLAG256TH_DOWN_STRAIGHT : SymId::FLAG256TH_DOWN;
        case -7: return straight ? SymId::FLAG512TH_DOWN_STRAIGHT : SymId::FLAG512TH_DOWN;
        case -8: return straight ? SymId::FLAG1024TH_DOWN_STRAIGHT : SymId::FLAG1024TH_DOWN;
        default: LOGE() << "No hook/flag for hook index: " << index; break;
    }

    return SymId::NO_SYM;
}
