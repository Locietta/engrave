/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "musicxmlvoicedesc.h"

using namespace mu::engraving;
using namespace mu::iex::musicxml;
//---------------------------------------------------------
//   VoiceDesc
//---------------------------------------------------------

VoiceDesc::VoiceDesc() : staff_(-1), voice_(-1), overlaps_(false) {
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        chord_rests_[i] = 0;
        staff_alloc_[i] = -1;
        voices_[i] = -1;
    }
}

void VoiceDesc::incr_chord_rests(int s) {
    if (0 <= s && s < ::mu::engraving::max_staves) {
        chord_rests_[s]++;
    }
}

int VoiceDesc::number_chord_rests() const {
    int res = 0;
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        res += chord_rests_[i];
    }
    return res;
}

int VoiceDesc::preferred_staff() const {
    int max = 0;
    int res = 0;
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        if (chord_rests_[i] > max) {
            max = chord_rests_[i];
            res = i;
        }
    }
    return res;
}

String VoiceDesc::to_string() const {
    String res = u"[";
    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        res += String(u" %1").arg(chord_rests_[i]);
    }
    res += String(u" ] overlaps %1").arg(overlaps_);
    if (overlaps_) {
        res += u" staffAlloc [";
        for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
            res += String(u" %1").arg(staff_alloc_[i]);
        }
        res += u" ] voices [";
        for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
            res += String(u" %1").arg(voices_[i]);
        }
        res += u" ]";
    } else {
        res += String(u" staff %1 voice %2").arg(staff_ + 1).arg(voice_ + 1);
    }
    return res;
}
