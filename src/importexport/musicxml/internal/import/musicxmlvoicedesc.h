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

struct VoiceDesc {
    VoiceDesc();
    void incr_chord_rests(int s);
    int number_chord_rests() const;

    int number_chord_rests(int s) const { return (s >= 0 && s < engraving::max_staves) ? chord_rests_[s] : 0; }

    int preferred_staff() const; // Determine preferred staff for this voice

    void set_staff(int s) {
        if (s >= 0) {
            staff_ = s;
        }
    }

    int staff() const { return staff_; }

    void set_voice(int v) {
        if (v >= 0) {
            voice_ = v;
        }
    }

    int voice() const { return voice_; }

    void set_voice(int s, int v) {
        if (s >= 0 && s < engraving::max_staves) {
            voices_[s] = v;
        }
    }

    int voice(int s) const { return (s >= 0 && s < engraving::max_staves) ? voices_[s] : -1; }

    void set_overlap(bool b) { overlaps_ = b; }

    bool overlaps() const { return overlaps_; }

    void set_staff_alloc(int s, int i) {
        if (s >= 0 && s < engraving::max_staves) {
            staff_alloc_[s] = i;
        }
    }

    int staff_alloc(int s) const { return (s >= 0 && s < engraving::max_staves) ? staff_alloc_[s] : -1; }

    muse::String to_string() const;
private:
    int chord_rests_[engraving::max_staves]; // The number of chordrests on each MusicXML staff
    int staff_;                              // The MuseScore staff allocated
    int voice_;                              // The MuseScore voice allocated
    bool overlaps_;                          // This voice contains active notes in multiple staves at the same time
    int staff_alloc_
        [engraving::max_staves];        // For overlapping voices: voice is allocated on these staves (note: -2=unalloc -1=undef 1=alloc)
    int voices_[engraving::max_staves]; // For every voice allocated on the staff, the voice number
};

typedef std::map<int, VoiceDesc> VoiceList;
} // namespace mu::iex::musicxml
