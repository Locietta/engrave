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
    for (int i = 0; i < MAX_STAVES; ++i) {
        chordRests_[i] = 0;
        staffAlloc_[i] = -1;
        voices_[i] = -1;
    }
}

void VoiceDesc::incrChordRests(int s) {
    if (0 <= s && s < MAX_STAVES) {
        chordRests_[s]++;
    }
}

int VoiceDesc::numberChordRests() const {
    int res = 0;
    for (int i = 0; i < MAX_STAVES; ++i) {
        res += chordRests_[i];
    }
    return res;
}

int VoiceDesc::preferredStaff() const {
    int max = 0;
    int res = 0;
    for (int i = 0; i < MAX_STAVES; ++i) {
        if (chordRests_[i] > max) {
            max = chordRests_[i];
            res = i;
        }
    }
    return res;
}

String VoiceDesc::toString() const {
    String res = u"[";
    for (int i = 0; i < MAX_STAVES; ++i) {
        res += String(u" %1").arg(chordRests_[i]);
    }
    res += String(u" ] overlaps %1").arg(overlaps_);
    if (overlaps_) {
        res += u" staffAlloc [";
        for (int i = 0; i < MAX_STAVES; ++i) {
            res += String(u" %1").arg(staffAlloc_[i]);
        }
        res += u" ] voices [";
        for (int i = 0; i < MAX_STAVES; ++i) {
            res += String(u" %1").arg(voices_[i]);
        }
        res += u" ]";
    } else {
        res += String(u" staff %1 voice %2").arg(staff_ + 1).arg(voice_ + 1);
    }
    return res;
}
