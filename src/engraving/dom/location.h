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
struct EngravingItem;

enum class Pid : short;

//---------------------------------------------------------
//   Location
//---------------------------------------------------------

struct Location {
    constexpr Location(int staff, int voice, int measure, Fraction frac, int grace_index, int note, bool rel)
        : staff_(staff), voice_(voice), measure_(measure), frac_(frac), grace_index_(grace_index), note_(note), rel_(rel) {}

    static constexpr Location absolute() { return Location(INT_MIN, INT_MIN, INT_MIN, Fraction(INT_MIN, 1), INT_MIN, INT_MIN, false); }

    static constexpr Location relative() { return Location(0, 0, 0, Fraction(0, 1), INT_MIN, 0, true); }

    void to_absolute(const Location &ref);
    void to_relative(const Location &ref);

    bool is_absolute() const { return !rel_; }

    bool is_relative() const { return rel_; }

    int staff() const { return staff_; }

    void set_staff(int staff) { staff_ = staff; }

    int voice() const { return voice_; }

    void set_voice(int voice) { voice_ = voice; }

    int track() const;
    void set_track(int track);

    int measure() const { return measure_; }

    void set_measure(int measure) { measure_ = measure; }

    Fraction frac() const { return frac_; }

    void set_frac(Fraction frac) { frac_ = frac; }

    int grace_index() const { return grace_index_; }

    void set_grace_index(int index) { grace_index_ = index; }

    int note() const { return note_; }

    void set_note(int note) { note_ = note; }

    void fill_for_element(const EngravingItem *e, bool absfrac = true);
    void fill_position_for_element(const EngravingItem *e, bool absfrac = true);
    static Location for_element(const EngravingItem *e, bool absfrac = true);
    static Location position_for_element(const EngravingItem *e, bool absfrac = true);
    static PropertyValue get_location_property(Pid pid, const EngravingItem *start, const EngravingItem *end);

    void set_is_time_tick(bool v) { is_time_tick_ = v; }

    bool is_time_tick() const { return is_time_tick_; }

    bool operator==(const Location &other) const;

    bool operator!=(const Location &other) const { return !(*this == other); }

private:

    static int track(const EngravingItem *e);
    static int measure(const EngravingItem *e);
    static int grace_index(const EngravingItem *e);
    static int note(const EngravingItem *e);

    int staff_ = 0;
    int voice_ = 0;
    int measure_ = 0;
    Fraction frac_;
    int grace_index_ = 0;
    int note_ = 0;
    bool rel_ = false;
    bool is_time_tick_ = false;
};
} // namespace mu::engraving
