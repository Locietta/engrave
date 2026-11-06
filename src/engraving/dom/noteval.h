/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "mscore.h"
#include "pitchspelling.h"
#include "../types/types.h"

namespace mu::engraving {
//---------------------------------------------------------
//   NoteVal
///    helper structure
///   \cond PLUGIN_API \private \endcond
//---------------------------------------------------------
struct NoteVal {
    int pitch = -1;
    int velocity_override = 0;
    int tpc1 = Tpc::TPC_INVALID;
    int tpc2 = Tpc::TPC_INVALID;
    int fret = ::mu::engraving::invalid_fret_index;
    int string = ::mu::engraving::invalid_string_index;
    NoteHeadGroup head_group = NoteHeadGroup::HEAD_NORMAL;

    NoteVal() {}

    NoteVal(int p) : pitch(p) {}

    bool operator==(const NoteVal &v) const {
        return pitch == v.pitch && velocity_override == v.velocity_override && tpc1 == v.tpc1 && tpc2 == v.tpc2 && fret == v.fret &&
               string == v.string && head_group == v.head_group;
    }

    int tpc(bool concert_pitch) const { return concert_pitch ? tpc1 : tpc2; }

    bool is_rest() const { return pitch == -1; }
};

using NoteValList = std::vector<NoteVal>;
} // namespace mu::engraving
