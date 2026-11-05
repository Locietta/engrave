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
#pragma once

#include "engraving/dom/mscore.h"

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   VoiceDesc
//---------------------------------------------------------

/**
 The description of a single voice in a MusicXML part.
*/

class VoiceDesc {
public:
    VoiceDesc();
    void incrChordRests(int s);
    int numberChordRests() const;

    int numberChordRests(int s) const { return (s >= 0 && s < engraving::MAX_STAVES) ? chordRests_[s] : 0; }

    int preferredStaff() const; // Determine preferred staff for this voice

    void setStaff(int s) {
        if (s >= 0) {
            staff_ = s;
        }
    }

    int staff() const { return staff_; }

    void setVoice(int v) {
        if (v >= 0) {
            voice_ = v;
        }
    }

    int voice() const { return voice_; }

    void setVoice(int s, int v) {
        if (s >= 0 && s < engraving::MAX_STAVES) {
            voices_[s] = v;
        }
    }

    int voice(int s) const { return (s >= 0 && s < engraving::MAX_STAVES) ? voices_[s] : -1; }

    void setOverlap(bool b) { overlaps_ = b; }

    bool overlaps() const { return overlaps_; }

    void setStaffAlloc(int s, int i) {
        if (s >= 0 && s < engraving::MAX_STAVES) {
            staffAlloc_[s] = i;
        }
    }

    int staffAlloc(int s) const { return (s >= 0 && s < engraving::MAX_STAVES) ? staffAlloc_[s] : -1; }

    muse::String toString() const;
private:
    int chordRests_[engraving::MAX_STAVES]; // The number of chordrests on each MusicXML staff
    int staff_;                             // The MuseScore staff allocated
    int voice_;                             // The MuseScore voice allocated
    bool overlaps_;                         // This voice contains active notes in multiple staves at the same time
    int staffAlloc_
        [engraving::MAX_STAVES];        // For overlapping voices: voice is allocated on these staves (note: -2=unalloc -1=undef 1=alloc)
    int voices_[engraving::MAX_STAVES]; // For every voice allocated on the staff, the voice number
};

typedef std::map<int, VoiceDesc> VoiceList;
} // namespace mu::iex::musicxml
