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

#pragma once

#include <climits>

#include "../types/propertyvalue.h"

namespace mu::engraving {
class EngravingItem;

enum class Pid : short;

//---------------------------------------------------------
//   Location
//---------------------------------------------------------

class Location {
public:
    constexpr Location(int staff, int voice, int measure, Fraction frac, int graceIndex, int note, bool rel)
        : staff_(staff), voice_(voice), measure_(measure), frac_(frac), graceIndex_(graceIndex), note_(note), rel_(rel) {}

    static constexpr Location absolute() { return Location(INT_MIN, INT_MIN, INT_MIN, Fraction(INT_MIN, 1), INT_MIN, INT_MIN, false); }

    static constexpr Location relative() { return Location(0, 0, 0, Fraction(0, 1), INT_MIN, 0, true); }

    void toAbsolute(const Location &ref);
    void toRelative(const Location &ref);

    bool isAbsolute() const { return !rel_; }

    bool isRelative() const { return rel_; }

    int staff() const { return staff_; }

    void setStaff(int staff) { staff_ = staff; }

    int voice() const { return voice_; }

    void setVoice(int voice) { voice_ = voice; }

    int track() const;
    void setTrack(int track);

    int measure() const { return measure_; }

    void setMeasure(int measure) { measure_ = measure; }

    Fraction frac() const { return frac_; }

    void setFrac(Fraction frac) { frac_ = frac; }

    int graceIndex() const { return graceIndex_; }

    void setGraceIndex(int index) { graceIndex_ = index; }

    int note() const { return note_; }

    void setNote(int note) { note_ = note; }

    void fillForElement(const EngravingItem *e, bool absfrac = true);
    void fillPositionForElement(const EngravingItem *e, bool absfrac = true);
    static Location forElement(const EngravingItem *e, bool absfrac = true);
    static Location positionForElement(const EngravingItem *e, bool absfrac = true);
    static PropertyValue getLocationProperty(Pid pid, const EngravingItem *start, const EngravingItem *end);

    void setIsTimeTick(bool v) { isTimeTick_ = v; }

    bool isTimeTick() const { return isTimeTick_; }

    bool operator==(const Location &other) const;

    bool operator!=(const Location &other) const { return !(*this == other); }

private:

    static int track(const EngravingItem *e);
    static int measure(const EngravingItem *e);
    static int graceIndex(const EngravingItem *e);
    static int note(const EngravingItem *e);

    int staff_ = 0;
    int voice_ = 0;
    int measure_ = 0;
    Fraction frac_;
    int graceIndex_ = 0;
    int note_ = 0;
    bool rel_ = false;
    bool isTimeTick_ = false;
};
} // namespace mu::engraving
