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

#include "global/allocator.h"

#include "../../types/types.h"

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

class VelocityEvent {
public:
    VelocityEvent() {}

    VelocityEvent(int val) : value_(val), type_(VelocityEventType::DYNAMIC) {}

    VelocityEvent(Fraction s, Fraction e, int diff, ChangeMethod m, ChangeDirection d)
        : value_(diff), type_(VelocityEventType::HAIRPIN), length_(e - s), method_(m), direction_(d) {}

    bool operator==(const VelocityEvent &event) const;

    bool operator!=(const VelocityEvent &event) const { return !(operator==(event)); }

private:
    friend class VelocityMap;

    int value_ = 0;
    VelocityEventType type_ = VelocityEventType::INVALID;
    Fraction length_;
    ChangeMethod method_ = ChangeMethod::NORMAL;
    ChangeDirection direction_ = ChangeDirection::INCREASING;
    int cachedStartVal_ = -1;
    int cachedEndVal_ = -1;
};

//---------------------------------------------------------
//  VelocityMap
///  List of changes in a value.
//---------------------------------------------------------

typedef std::vector<std::pair<Fraction, Fraction>> EndPointsVector;

class VelocityMap : public std::multimap<Fraction, VelocityEvent> {
    OBJECT_ALLOCATOR(engraving, VelocityMap)
public:
    VelocityMap() {}

    int val(Fraction tick) const;
    std::vector<std::pair<Fraction, Fraction>> changesInRange(Fraction stick, Fraction etick) const;

    void addDynamic(Fraction tick, int value);
    void addHairpin(Fraction stick, Fraction etick, int change, ChangeMethod method, ChangeDirection direction);
    void setup();

    static int interpolate(Fraction &eventTick, VelocityEvent &event, Fraction &tick);

private:

    struct ChangeMethodItem {
        ChangeMethod method;
        const char *name;
    };

    static bool compareHairpinEvents(VelocityEvent &a, VelocityEvent &b) { return a.length_ > b.length_; }

    void sortHairpins();
    void resolveHairpinCollisions();
    void resolveDynamicInsideHairpinCollisions();
    void adjustCollidingHairpinsLength(std::vector<bool> &startsInHairpin, EndPointsVector &endPoints);
    bool dynamicExistsOnTick(Fraction tick) const;
    VelocityEvent dynamicEventForTick(Fraction tick) const;
    void addMissingDynamicsAfterHairpins();
    void fillHairpinsCache();

    static constexpr int DEFAULT_VALUE = 80;
    static constexpr int MIN_VALUE = 1;
    static constexpr int MAX_VALUE = 127;
    static constexpr int STEP = 16;
};
} // namespace mu::engraving
#endif
