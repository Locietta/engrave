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

/**
 \file
 Implementation of class VelocityMap.
*/

#include "velocitymap.h"

#include <cmath>
#include <cassert>

#include "containers.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   interpolateVelocity
///   the maths looks complex, but is just a series of graph transformations.
///   You can see these graphically at: https://www.desmos.com/calculator/kk89ficmjk
//---------------------------------------------------------

int VelocityMap::interpolate(Fraction &eventTick, VelocityEvent &event, Fraction &tick) {
    assert(event.type_ == VelocityEventType::HAIRPIN);

    // Prevent zero-division error
    if (event.cachedStartVal_ == event.cachedEndVal_ || event.length_.isZero()) {
        return event.cachedStartVal_;
    }

    // Ticks to change expression over
    int exprTicks = event.length_.ticks();
    int exprDiff = event.cachedEndVal_ - event.cachedStartVal_;

    std::function<int(int)> valueFunction;
    switch (event.method_) {
        case ChangeMethod::EXPONENTIAL:
            // Due to the nth-root, exponential functions do not flip with negative values, and cause errors,
            // so treat it as a piecewise function.
            if (exprDiff > 0) {
                valueFunction = [&](int ct) {
                    return int(pow(pow((exprDiff + 1), 1.0 / double(exprTicks)), // the exprTicks root of d+1
                                   double(ct)                                    // to the power of the current tick (exponential)
                                   ) -
                               1);
                };
            } else {
                valueFunction = [&](int ct) {
                    return -int(pow(pow((-exprDiff + 1), 1.0 / double(exprTicks)), // the exprTicks root of 1-d
                                    double(ct)                                     // again to the power of ct
                                    ) +
                                1);
                };
            }
            break;
        // Uses sin x transformed, which _does_ flip with negative numbers
        case ChangeMethod::EASE_IN_OUT:
            valueFunction = [&](int ct) {
                return int((double(exprDiff) / 2.0) * (sin(double(ct) * (double(M_PI / double(exprTicks))) - double(M_PI / 2.0)) + 1));
            };
            break;
        case ChangeMethod::EASE_IN:
            valueFunction = [&](int ct) {
                return int(double(exprDiff) * (sin(double(ct - double(exprTicks)) * (double(M_PI / double(2 * exprTicks)))) + 1));
            };
            break;
        case ChangeMethod::EASE_OUT:
            valueFunction = [&](int ct) { return int(double(exprDiff) * sin(double(ct) * (double(M_PI / double(2 * exprTicks))))); };
            break;
        case ChangeMethod::NORMAL:
        default: valueFunction = [&](int ct) { return int(double(exprDiff) * (double(ct) / double(exprTicks))); }; break;
    }

    return event.cachedStartVal_ + valueFunction(tick.ticks() - eventTick.ticks());
}

//---------------------------------------------------------
//   val
///   return value at tick position.
//---------------------------------------------------------

int VelocityMap::val(Fraction tick) const {
    auto eventIter = upper_bound(tick);
    if (eventIter == begin()) {
        return DEFAULT_VALUE;
    }

    eventIter--;
    bool foundHairpin = false;
    VelocityEvent hairpinFound = eventIter->second; // only used to init
    Fraction hairpinFoundStartTick = eventIter->first;
    auto values = equal_range(hairpinFoundStartTick);
    for (auto it = values.first; it != values.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::HAIRPIN) {
            foundHairpin = true;
            hairpinFound = event;
        }
    }

    if (!foundHairpin) {
        // Last event must be a dynamic, since there are max two events at one tick
        return eventIter->second.value_;
    }

    if (tick >= (hairpinFoundStartTick + hairpinFound.length_)) {
        return hairpinFound.cachedEndVal_;
    } else {
        // Do some maths!
        return interpolate(hairpinFoundStartTick, hairpinFound, tick); // NOTE:JT check should hairpinFound be eventIter.value()
    }
}

//---------------------------------------------------------
//   addDynamic
//---------------------------------------------------------

void VelocityMap::addDynamic(Fraction tick, int value) { insert({tick, VelocityEvent(value)}); }

//---------------------------------------------------------
//   addHairpin
///   A `change` of 0 means that the change in velocity should be calculated from the next dynamic
///   velocity event.
//---------------------------------------------------------

void VelocityMap::addHairpin(Fraction stick, Fraction etick, int change, ChangeMethod method, ChangeDirection direction) {
    change = std::abs(change);
    change *= (direction == ChangeDirection::INCREASING) ? 1 : -1;
    insert({stick, VelocityEvent(stick, etick, change, method, direction)});
}

//---------------------------------------------------------
//   sortHairpins
///   put the hairpins in size order if they start at the same point
//---------------------------------------------------------

void VelocityMap::sortHairpins() {
    for (auto &tick : muse::uniqueKeys(*this)) {
        // hairpinEvents will contain all the hairpins at this tick
        std::vector<VelocityEvent> hairpinEvents;
        auto values = this->equal_range(tick);
        for (auto it = values.first; it != values.second; ++it) {
            auto &event = it->second;
            if (event.type_ == VelocityEventType::DYNAMIC) {
                continue;
            }
            hairpinEvents.push_back(event);
        }

        if (int(hairpinEvents.size()) > 1) {
            // Sort hairpinEvents so that the longest hairpins come first -
            // this is important for when we remove hairpins/dynamics enclosed within other
            // hairpins during stage 1.
            std::sort(hairpinEvents.begin(), hairpinEvents.end(), VelocityMap::compareHairpinEvents);
            for (auto &event : hairpinEvents) {
                insert({tick, event});
            }
        }
    }
}

//---------------------------------------------------------
//   resolveHairpinCollisions
///   remove any hairpins that are completely enclosed within other hairpins
//---------------------------------------------------------

void VelocityMap::resolveHairpinCollisions() {
    Fraction currentHairpinEnd = Fraction(-1, 1); // end point of hairpin we're in
    bool inHairpin = false;                       // whether we're in a hairpin or not

    // Keep a record of the endpoints
    EndPointsVector endPoints;
    std::vector<bool> startsInHairpin;

    auto i = begin();
    while (i != end()) {
        Fraction tick = i->first;
        VelocityEvent &event = i->second;
        Fraction etick = tick + event.length_;

        // Reset if we've left the hairpin we were in
        if (currentHairpinEnd < tick) {
            inHairpin = false;
        }

        if (event.type_ == VelocityEventType::HAIRPIN) {
            if (inHairpin) {
                if (etick <= currentHairpinEnd) {
                    // delete, this event is enveloped
                    i = erase(i);
                    // don't add to the end points
                    continue;
                } else {
                    currentHairpinEnd = etick;
                    startsInHairpin.push_back(true);
                }
            } else {
                currentHairpinEnd = etick;
                inHairpin = true;
                startsInHairpin.push_back(false);
            }

            endPoints.push_back(std::make_pair(tick, etick));
        }

        i++;
    }

    adjustCollidingHairpinsLength(startsInHairpin, endPoints);
}

//---------------------------------------------------------
//   adjustCollidingHairpinsLength
///   readjust lengths of any colliding hairpins
//---------------------------------------------------------

void VelocityMap::adjustCollidingHairpinsLength(std::vector<bool> &startsInHairpin, EndPointsVector &endPoints) {
    // moveTo stores the events that need to be moved to a Fraction position
    std::map<Fraction, VelocityEvent> moveTo;
    auto i = begin();
    int j = -1;
    while (i != end()) {
        Fraction tick = i->first;
        VelocityEvent &event = i->second;
        if (event.type_ != VelocityEventType::HAIRPIN) {
            i++;
            continue;
        }

        j++;
        if (!startsInHairpin[j]) {
            i++;
            continue;
        }

        // Take a copy of the event and remove it
        Fraction newTick = endPoints[j - 1].second;
        event.length_ -= (newTick - tick);
        moveTo[newTick] = event;
        i = erase(i);
    }

    // Re-insert the events that we need to move in their new positions
    for (auto k = moveTo.begin(); k != moveTo.end(); k++) {
        insert({k->first, k->second});
    }
}

//---------------------------------------------------------
//   resolveDynamicInsideHairpinCollisions
///   if dynamic is inside the hairpin, shorten hairpin length up to this dynamic
//---------------------------------------------------------

void VelocityMap::resolveDynamicInsideHairpinCollisions() {
    auto lastEventIt = end();
    auto i = begin();
    while (i != end()) {
        VelocityEvent &event = i->second;
        if (event.type_ == VelocityEventType::DYNAMIC && lastEventIt != end() && lastEventIt->second.type_ == VelocityEventType::HAIRPIN) {
            Fraction dynamicTick = i->first;
            Fraction lastHairpinStart = lastEventIt->first;
            Fraction lastHairpinEnd = lastHairpinStart + lastEventIt->second.length_;
            if (lastHairpinEnd > dynamicTick) {
                lastEventIt->second.length_ = dynamicTick - lastHairpinStart;
            }
        }

        lastEventIt = i;
        i++;
    }
}

bool VelocityMap::dynamicExistsOnTick(Fraction tick) const {
    auto values = equal_range(tick);
    for (auto it = values.first; it != values.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            return true;
        }
    }

    return false;
}

VelocityEvent VelocityMap::dynamicEventForTick(Fraction tick) const {
    auto equalValues = equal_range(tick);
    for (auto it = equalValues.first; it != equalValues.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            return event;
        }
    }

    if (equalValues.first == begin()) {
        return VelocityEvent();
    }

    auto previousIt = std::prev(equalValues.first);
    auto smallerValues = equal_range(previousIt->first);
    for (auto it = smallerValues.first; it != smallerValues.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            return event;
        }
    }

    return VelocityEvent();
}

void VelocityMap::addMissingDynamicsAfterHairpins() {
    auto nextDynamicVal = [](int previousDynamicValue, ChangeDirection hairpinDirection) {
        int newVal = (hairpinDirection == ChangeDirection::INCREASING ? previousDynamicValue + VelocityMap::STEP :
                                                                        previousDynamicValue - VelocityMap::STEP);
        return std::clamp(newVal, VelocityMap::MIN_VALUE, VelocityMap::MAX_VALUE);
    };

    for (auto it = begin(); it != end(); it++) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::HAIRPIN && event.value_ == 0) {
            Fraction startHairpinTick = it->first;
            Fraction endHairpinTick = it->first + it->second.length_;
            VelocityEvent dynamicOnCurrentTick = dynamicEventForTick(startHairpinTick);

            if (!dynamicExistsOnTick(endHairpinTick)) {
                if (dynamicOnCurrentTick.type_ != VelocityEventType::INVALID) {
                    int newVal = nextDynamicVal(dynamicOnCurrentTick.value_, event.direction_);
                    insert({endHairpinTick, VelocityEvent(newVal)});
                }
            } else {
                VelocityEvent dynamicOnEndTick = dynamicEventForTick(endHairpinTick);
                int velocityJump = dynamicOnEndTick.value_ - dynamicOnCurrentTick.value_;

                if ((event.direction_ == ChangeDirection::INCREASING && velocityJump < 0) ||
                    (event.direction_ == ChangeDirection::DECREASING && velocityJump > 0)) {
                    int newVal = nextDynamicVal(dynamicOnCurrentTick.value_, event.direction_);
                    event.value_ = newVal - dynamicOnCurrentTick.value_;
                }
            }
        }
    }
}

//---------------------------------------------------------
//   fillHairpinsCache
///   cache start and end values for each hairpin
//---------------------------------------------------------

void VelocityMap::fillHairpinsCache() {
    for (auto i = begin(); i != end(); i++) {
        Fraction tick = i->first;
        auto &event = i->second;
        if (event.type_ != VelocityEventType::HAIRPIN) {
            continue;
        }

        // Phase 1: cache a start value for the hairpin
        // Try and get a dynamic at the tick of this hairpin
        bool foundDynamic = false;
        auto values = this->equal_range(tick);
        for (auto it = values.first; it != values.second; ++it) {
            auto &currentChangeEvent = it->second;
            if (currentChangeEvent.type_ == VelocityEventType::DYNAMIC) {
                event.cachedStartVal_ = currentChangeEvent.value_;
                foundDynamic = true;
                break;
            }
        }

        // If there isn't a dynamic, use from the last event:
        //  - the cached end value if it's a hairpin
        //  - the value if it's a dynamic
        if (!foundDynamic) {
            if (i != begin()) {
                auto prevChangeEventIter = i;
                prevChangeEventIter--;

                // Look for a hairpin first
                bool foundHairpin = false;
                auto prevValues = this->equal_range(prevChangeEventIter->first);
                for (auto it = prevValues.first; it != prevValues.second; ++it) {
                    auto &prevChangeEvent = it->second;
                    if (prevChangeEvent.type_ == VelocityEventType::HAIRPIN) {
                        event.cachedStartVal_ = prevChangeEvent.cachedEndVal_;
                        foundHairpin = true;
                        break;
                    }
                }

                if (!foundHairpin) {
                    // prevChangeEventIter must point to a dynamic in this case
                    event.cachedStartVal_ = prevChangeEventIter->second.value_;
                }
            } else {
                event.cachedStartVal_ = DEFAULT_VALUE;
            }
        }

        // Phase 2: cache an end value for the hairpin
        // If there's no set velocity change:
        if (event.value_ == 0) {
            auto nextChangeEventIter = i;
            nextChangeEventIter++;
            // There's a chance that the next event is a dynamic at the same tick as the
            // start of the current hairpin. If so, get the next event, which is assured
            // to be a different (larger) tick
            if (nextChangeEventIter != end() && nextChangeEventIter->first == tick) {
                nextChangeEventIter++;
            }

            // If this is the last event, there is no change
            if (nextChangeEventIter == end()) {
                event.cachedEndVal_ = event.cachedStartVal_;
            } else {
                // Search for a dynamic event at the next event point
                bool foundDynamic2 = false;
                auto nextValues = this->equal_range(nextChangeEventIter->first);
                for (auto it = nextValues.first; it != nextValues.second; ++it) {
                    auto &nextChangeEvent = it->second;
                    if (nextChangeEvent.type_ == VelocityEventType::DYNAMIC) {
                        event.cachedEndVal_ = nextChangeEvent.value_;
                        foundDynamic2 = true;
                        break;
                    }
                }

                // We haven't found a dynamic, so there must be a hairpin. What does the user want?
                // A good guess would to be to interpolate, but that might get complex, so just ignore
                // this hairpin and set the ending to be the same as the start.
                // TODO: implementing some form of smart interpolation would be nice.
                if (!foundDynamic2) {
                    event.cachedEndVal_ = event.cachedStartVal_;
                }
            }
        } else {
            event.cachedEndVal_ = event.cachedStartVal_ + event.value_;
        }

        // And finally... if something's wrong, make it not wrong
        if ((event.cachedStartVal_ > event.cachedEndVal_ && event.direction_ == ChangeDirection::INCREASING) ||
            (event.cachedStartVal_ < event.cachedEndVal_ && event.direction_ == ChangeDirection::DECREASING)) {
            event.cachedEndVal_ = event.cachedStartVal_;
        }
    }
}

//---------------------------------------------------------
//   setup
//---------------------------------------------------------

void VelocityMap::setup() {
    sortHairpins();
    resolveHairpinCollisions();
    resolveDynamicInsideHairpinCollisions();
    addMissingDynamicsAfterHairpins();
    fillHairpinsCache();
}

//---------------------------------------------------------
//   changesInRange
///   returns a list of changes in a range, and their start and end points
//---------------------------------------------------------

std::vector<std::pair<Fraction, Fraction>> VelocityMap::changesInRange(Fraction stick, Fraction etick) const {
    std::vector<std::pair<Fraction, Fraction>> tempChanges;

    // Force a new event on every noteon, in case the velocity has changed
    tempChanges.push_back(std::make_pair(stick, stick));
    for (auto iter = lower_bound(stick); iter != end(); iter++) {
        Fraction tick = iter->first;
        if (tick > etick) {
            break;
        }

        auto &event = iter->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            tempChanges.push_back(std::make_pair(tick, tick));
        } else if (event.type_ == VelocityEventType::HAIRPIN) {
            Fraction eventEtick = tick + event.length_;
            Fraction useEtick = eventEtick > etick ? etick : eventEtick;
            tempChanges.push_back(std::make_pair(tick, useEtick));
        }
    }

    // And also go back one and try to find hairpin coming into this range
    auto iter = lower_bound(stick);
    if (iter != begin()) {
        iter--;
        auto &event = iter->second;
        if (event.type_ == VelocityEventType::HAIRPIN) {
            Fraction eventEtick = iter->first + event.length_;
            if (eventEtick > stick) {
                tempChanges.push_back(std::make_pair(stick, eventEtick));
            }
        }
    }
    return tempChanges;
}

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool VelocityEvent::operator==(const VelocityEvent &event) const {
    return value_ == event.value_ && type_ == event.type_ && length_ == event.length_ && method_ == event.method_ &&
           direction_ == event.direction_;
}
} // namespace mu::engraving
