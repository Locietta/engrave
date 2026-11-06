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

#ifndef MU_ENGRAVING_VELOCITYMAP_H
#define MU_ENGRAVING_VELOCITYMAP_H

#include <map>

#include "../../types/types.h"
#include "global/allocator.h"

/**
 \file
 Definition of class VelocityMap.
*/

namespace mu::engraving {
//---------------------------------------------------------
///   VelocityEvent
///   item in VelocityMap
//---------------------------------------------------------

enum class VelocityEventType : char { DYNAMIC, HAIRPIN, INVALID };

struct VelocityEvent {
    VelocityEvent() {}

    VelocityEvent(int val) : value_(val), type_(VelocityEventType::DYNAMIC) {}

    VelocityEvent(Fraction s, Fraction e, int diff, ChangeMethod m, ChangeDirection d)
        : value_(diff), type_(VelocityEventType::HAIRPIN), length_(e - s), method_(m), direction_(d) {}

    bool operator==(const VelocityEvent &event) const;

    bool operator!=(const VelocityEvent &event) const { return !(operator==(event)); }

private:
    friend struct VelocityMap;

    int value_ = 0;
    VelocityEventType type_ = VelocityEventType::INVALID;
    Fraction length_;
    ChangeMethod method_ = ChangeMethod::NORMAL;
    ChangeDirection direction_ = ChangeDirection::INCREASING;
    int cached_start_val_ = -1;
    int cached_end_val_ = -1;
};

//---------------------------------------------------------
//  VelocityMap
///  List of changes in a value.
//---------------------------------------------------------

typedef std::vector<std::pair<Fraction, Fraction>> EndPointsVector;

struct VelocityMap : std::multimap<Fraction, VelocityEvent> {
private:
    OBJECT_ALLOCATOR(engraving, VelocityMap)
public:
    VelocityMap() {}

    int val(Fraction tick) const;
    std::vector<std::pair<Fraction, Fraction>> changes_in_range(Fraction stick, Fraction etick) const;

    void add_dynamic(Fraction tick, int value);
    void add_hairpin(Fraction stick, Fraction etick, int change, ChangeMethod method, ChangeDirection direction);
    void setup();

    static int interpolate(Fraction &event_tick, VelocityEvent &event, Fraction &tick);

private:

    struct ChangeMethodItem {
        ChangeMethod method;
        const char *name;
    };

    static bool compare_hairpin_events(VelocityEvent &a, VelocityEvent &b) { return a.length_ > b.length_; }

    void sort_hairpins();
    void resolve_hairpin_collisions();
    void resolve_dynamic_inside_hairpin_collisions();
    void adjust_colliding_hairpins_length(std::vector<bool> &starts_in_hairpin, EndPointsVector &end_points);
    bool dynamic_exists_on_tick(Fraction tick) const;
    VelocityEvent dynamic_event_for_tick(Fraction tick) const;
    void add_missing_dynamics_after_hairpins();
    void fill_hairpins_cache();

    static constexpr int default_value = 80;
    static constexpr int min_value = 1;
    static constexpr int max_value = 127;
    static constexpr int step = 16;
};
} // namespace mu::engraving
#endif
