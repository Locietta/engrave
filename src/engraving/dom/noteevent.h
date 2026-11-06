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

#ifndef MU_ENGRAVING_NOTEEVENT_H
#define MU_ENGRAVING_NOTEEVENT_H

#include <algorithm>
#include <vector>

#include "global/allocator.h"

namespace mu::engraving {
//---------------------------------------------------------
//    NoteEvent
//---------------------------------------------------------

struct NoteEvent {
    constexpr static int note_length = 1000;
    constexpr static double ghost_velocity_multiplier = 0.6;
    constexpr static double default_velocity_multiplier = 1.0;
    constexpr static int slide_amount = 3;

    NoteEvent() {}

    NoteEvent(int a, int b, int c, double d = 1.0, double play = true, int offset = 0)
        : pitch_(a), ontime_(b), len_(c), velocity_multiplier_(d), play_(play), offset_(offset) {}

    int pitch() const { return pitch_; }

    int ontime() const { return ontime_; }

    int offtime() const { return ontime_ + len_; }

    int len() const { return len_; }

    double velocity_multiplier() const { return velocity_multiplier_; }

    bool play() const { return play_; }

    int offset() const { return offset_; }

    bool slide() const { return slide_; }

    void set_pitch(int v) { pitch_ = v; }

    void set_ontime(int v) { ontime_ = v; }

    void set_len(int v) { len_ = v; }

    void set_velocity_multiplier(double velocity_multiplier) { velocity_multiplier_ = velocity_multiplier; }

    void set_offset(int v) { offset_ = v; }

    void set_slide(bool slide) { slide_ = slide; }

    bool operator==(const NoteEvent &) const;

private:
    int pitch_ = 0;                                                                        // relative pitch to note pitch
    int ontime_ = 0;                                                                       // one unit is 1/1000 of nominal note len
    int len_ = ::mu::engraving::NoteEvent::note_length;                                    // one unit is 1/1000 of nominal note len
    double velocity_multiplier_ = ::mu::engraving::NoteEvent::default_velocity_multiplier; // can be used to lower sound (0-1)
    bool play_ = true; // when 'false', note event is used only for length calculation
    int offset_ = 0;
    bool slide_ = false; // event is slide or glissando
};

//---------------------------------------------------------
//   NoteEventList
//---------------------------------------------------------

struct NoteEventList : std::vector<NoteEvent> {
private:
    OBJECT_ALLOCATOR(engraving, NoteEventList)
public:
    NoteEventList();

    int offtime() {
        return empty() ? 0 : std::max_element(cbegin(), cend(), [](const NoteEvent &n1, const NoteEvent &n2) {
                                 return n1.offtime() < n2.offtime();
                             })->offtime();
    }
};
} // namespace mu::engraving
#endif
