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

#include <vector>
#include <algorithm>

#include "global/allocator.h"

namespace mu::engraving {
//---------------------------------------------------------
//    NoteEvent
//---------------------------------------------------------

class NoteEvent {
public:
    constexpr static int NOTE_LENGTH = 1000;
    constexpr static double GHOST_VELOCITY_MULTIPLIER = 0.6;
    constexpr static double DEFAULT_VELOCITY_MULTIPLIER = 1.0;
    constexpr static int SLIDE_AMOUNT = 3;

    NoteEvent() {}

    NoteEvent(int a, int b, int c, double d = 1.0, double play = true, int offset = 0)
        : pitch_(a), ontime_(b), len_(c), velocityMultiplier_(d), play_(play), offset_(offset) {}

    int pitch() const { return pitch_; }

    int ontime() const { return ontime_; }

    int offtime() const { return ontime_ + len_; }

    int len() const { return len_; }

    double velocityMultiplier() const { return velocityMultiplier_; }

    bool play() const { return play_; }

    int offset() const { return offset_; }

    bool slide() const { return slide_; }

    void setPitch(int v) { pitch_ = v; }

    void setOntime(int v) { ontime_ = v; }

    void setLen(int v) { len_ = v; }

    void setVelocityMultiplier(double velocityMultiplier) { velocityMultiplier_ = velocityMultiplier; }

    void setOffset(int v) { offset_ = v; }

    void setSlide(bool slide) { slide_ = slide; }

    bool operator==(const NoteEvent &) const;

private:
    int pitch_ = 0;                                           // relative pitch to note pitch
    int ontime_ = 0;                                          // one unit is 1/1000 of nominal note len
    int len_ = NOTE_LENGTH;                                   // one unit is 1/1000 of nominal note len
    double velocityMultiplier_ = DEFAULT_VELOCITY_MULTIPLIER; // can be used to lower sound (0-1)
    bool play_ = true;                                        // when 'false', note event is used only for length calculation
    int offset_ = 0;
    bool slide_ = false; // event is slide or glissando
};

//---------------------------------------------------------
//   NoteEventList
//---------------------------------------------------------

class NoteEventList : public std::vector<NoteEvent> {
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
