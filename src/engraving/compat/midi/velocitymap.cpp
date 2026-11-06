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

int VelocityMap::interpolate(Fraction &event_tick, VelocityEvent &event, Fraction &tick) {
    assert(event.type_ == VelocityEventType::HAIRPIN);

    // Prevent zero-division error
    if (event.cached_start_val_ == event.cached_end_val_ || event.length_.is_zero()) {
        return event.cached_start_val_;
    }

    // Ticks to change expression over
    int expr_ticks = event.length_.ticks();
    int expr_diff = event.cached_end_val_ - event.cached_start_val_;

    std::function<int(int)> value_function;
    switch (event.method_) {
        case ChangeMethod::EXPONENTIAL:
            // Due to the nth-root, exponential functions do not flip with negative values, and cause errors,
            // so treat it as a piecewise function.
            if (expr_diff > 0) {
                value_function = [&](int ct) {
                    return int(pow(pow((expr_diff + 1), 1.0 / double(expr_ticks)), // the exprTicks root of d+1
                                   double(ct)                                      // to the power of the current tick (exponential)
                                   ) -
                               1);
                };
            } else {
                value_function = [&](int ct) {
                    return -int(pow(pow((-expr_diff + 1), 1.0 / double(expr_ticks)), // the exprTicks root of 1-d
                                    double(ct)                                       // again to the power of ct
                                    ) +
                                1);
                };
            }
            break;
        // Uses sin x transformed, which _does_ flip with negative numbers
        case ChangeMethod::EASE_IN_OUT:
            value_function = [&](int ct) {
                return int((double(expr_diff) / 2.0) * (sin(double(ct) * (double(M_PI / double(expr_ticks))) - double(M_PI / 2.0)) + 1));
            };
            break;
        case ChangeMethod::EASE_IN:
            value_function = [&](int ct) {
                return int(double(expr_diff) * (sin(double(ct - double(expr_ticks)) * (double(M_PI / double(2 * expr_ticks)))) + 1));
            };
            break;
        case ChangeMethod::EASE_OUT:
            value_function = [&](int ct) { return int(double(expr_diff) * sin(double(ct) * (double(M_PI / double(2 * expr_ticks))))); };
            break;
        case ChangeMethod::NORMAL:
        default: value_function = [&](int ct) { return int(double(expr_diff) * (double(ct) / double(expr_ticks))); }; break;
    }

    return event.cached_start_val_ + value_function(tick.ticks() - event_tick.ticks());
}

//---------------------------------------------------------
//   val
///   return value at tick position.
//---------------------------------------------------------

int VelocityMap::val(Fraction tick) const {
    auto event_iter = upper_bound(tick);
    if (event_iter == begin()) {
        return ::mu::engraving::VelocityMap::default_value;
    }

    event_iter--;
    bool found_hairpin = false;
    VelocityEvent hairpin_found = event_iter->second; // only used to init
    Fraction hairpin_found_start_tick = event_iter->first;
    auto values = equal_range(hairpin_found_start_tick);
    for (auto it = values.first; it != values.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::HAIRPIN) {
            found_hairpin = true;
            hairpin_found = event;
        }
    }

    if (!found_hairpin) {
        // Last event must be a dynamic, since there are max two events at one tick
        return event_iter->second.value_;
    }

    if (tick >= (hairpin_found_start_tick + hairpin_found.length_)) {
        return hairpin_found.cached_end_val_;
    } else {
        // Do some maths!
        return interpolate(hairpin_found_start_tick, hairpin_found, tick); // NOTE:JT check should hairpinFound be eventIter.value()
    }
}

//---------------------------------------------------------
//   addDynamic
//---------------------------------------------------------

void VelocityMap::add_dynamic(Fraction tick, int value) { insert({tick, VelocityEvent(value)}); }

//---------------------------------------------------------
//   addHairpin
///   A `change` of 0 means that the change in velocity should be calculated from the next dynamic
///   velocity event.
//---------------------------------------------------------

void VelocityMap::add_hairpin(Fraction stick, Fraction etick, int change, ChangeMethod method, ChangeDirection direction) {
    change = std::abs(change);
    change *= (direction == ChangeDirection::INCREASING) ? 1 : -1;
    insert({stick, VelocityEvent(stick, etick, change, method, direction)});
}

//---------------------------------------------------------
//   sortHairpins
///   put the hairpins in size order if they start at the same point
//---------------------------------------------------------

void VelocityMap::sort_hairpins() {
    for (auto &tick : muse::unique_keys(*this)) {
        // hairpinEvents will contain all the hairpins at this tick
        std::vector<VelocityEvent> hairpin_events;
        auto values = this->equal_range(tick);
        for (auto it = values.first; it != values.second; ++it) {
            auto &event = it->second;
            if (event.type_ == VelocityEventType::DYNAMIC) {
                continue;
            }
            hairpin_events.push_back(event);
        }

        if (int(hairpin_events.size()) > 1) {
            // Sort hairpinEvents so that the longest hairpins come first -
            // this is important for when we remove hairpins/dynamics enclosed within other
            // hairpins during stage 1.
            std::sort(hairpin_events.begin(), hairpin_events.end(), VelocityMap::compare_hairpin_events);
            for (auto &event : hairpin_events) {
                insert({tick, event});
            }
        }
    }
}

//---------------------------------------------------------
//   resolveHairpinCollisions
///   remove any hairpins that are completely enclosed within other hairpins
//---------------------------------------------------------

void VelocityMap::resolve_hairpin_collisions() {
    Fraction current_hairpin_end = Fraction(-1, 1); // end point of hairpin we're in
    bool in_hairpin = false;                        // whether we're in a hairpin or not

    // Keep a record of the endpoints
    EndPointsVector end_points;
    std::vector<bool> starts_in_hairpin;

    auto i = begin();
    while (i != end()) {
        Fraction tick = i->first;
        VelocityEvent &event = i->second;
        Fraction etick = tick + event.length_;

        // Reset if we've left the hairpin we were in
        if (current_hairpin_end < tick) {
            in_hairpin = false;
        }

        if (event.type_ == VelocityEventType::HAIRPIN) {
            if (in_hairpin) {
                if (etick <= current_hairpin_end) {
                    // delete, this event is enveloped
                    i = erase(i);
                    // don't add to the end points
                    continue;
                } else {
                    current_hairpin_end = etick;
                    starts_in_hairpin.push_back(true);
                }
            } else {
                current_hairpin_end = etick;
                in_hairpin = true;
                starts_in_hairpin.push_back(false);
            }

            end_points.push_back(std::make_pair(tick, etick));
        }

        i++;
    }

    adjust_colliding_hairpins_length(starts_in_hairpin, end_points);
}

//---------------------------------------------------------
//   adjustCollidingHairpinsLength
///   readjust lengths of any colliding hairpins
//---------------------------------------------------------

void VelocityMap::adjust_colliding_hairpins_length(std::vector<bool> &starts_in_hairpin, EndPointsVector &end_points) {
    // moveTo stores the events that need to be moved to a Fraction position
    std::map<Fraction, VelocityEvent> move_to;
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
        if (!starts_in_hairpin[j]) {
            i++;
            continue;
        }

        // Take a copy of the event and remove it
        Fraction new_tick = end_points[j - 1].second;
        event.length_ -= (new_tick - tick);
        move_to[new_tick] = event;
        i = erase(i);
    }

    // Re-insert the events that we need to move in their new positions
    for (auto k = move_to.begin(); k != move_to.end(); k++) {
        insert({k->first, k->second});
    }
}

//---------------------------------------------------------
//   resolveDynamicInsideHairpinCollisions
///   if dynamic is inside the hairpin, shorten hairpin length up to this dynamic
//---------------------------------------------------------

void VelocityMap::resolve_dynamic_inside_hairpin_collisions() {
    auto last_event_it = end();
    auto i = begin();
    while (i != end()) {
        VelocityEvent &event = i->second;
        if (event.type_ == VelocityEventType::DYNAMIC && last_event_it != end() &&
            last_event_it->second.type_ == VelocityEventType::HAIRPIN) {
            Fraction dynamic_tick = i->first;
            Fraction last_hairpin_start = last_event_it->first;
            Fraction last_hairpin_end = last_hairpin_start + last_event_it->second.length_;
            if (last_hairpin_end > dynamic_tick) {
                last_event_it->second.length_ = dynamic_tick - last_hairpin_start;
            }
        }

        last_event_it = i;
        i++;
    }
}

bool VelocityMap::dynamic_exists_on_tick(Fraction tick) const {
    auto values = equal_range(tick);
    for (auto it = values.first; it != values.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            return true;
        }
    }

    return false;
}

VelocityEvent VelocityMap::dynamic_event_for_tick(Fraction tick) const {
    auto equal_values = equal_range(tick);
    for (auto it = equal_values.first; it != equal_values.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            return event;
        }
    }

    if (equal_values.first == begin()) {
        return VelocityEvent();
    }

    auto previous_it = std::prev(equal_values.first);
    auto smaller_values = equal_range(previous_it->first);
    for (auto it = smaller_values.first; it != smaller_values.second; ++it) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            return event;
        }
    }

    return VelocityEvent();
}

void VelocityMap::add_missing_dynamics_after_hairpins() {
    auto next_dynamic_val = [](int previous_dynamic_value, ChangeDirection hairpin_direction) {
        int new_val = (hairpin_direction == ChangeDirection::INCREASING ? previous_dynamic_value + VelocityMap::step :
                                                                          previous_dynamic_value - VelocityMap::step);
        return std::clamp(new_val, VelocityMap::min_value, VelocityMap::max_value);
    };

    for (auto it = begin(); it != end(); it++) {
        auto &event = it->second;
        if (event.type_ == VelocityEventType::HAIRPIN && event.value_ == 0) {
            Fraction start_hairpin_tick = it->first;
            Fraction end_hairpin_tick = it->first + it->second.length_;
            VelocityEvent dynamic_on_current_tick = dynamic_event_for_tick(start_hairpin_tick);

            if (!dynamic_exists_on_tick(end_hairpin_tick)) {
                if (dynamic_on_current_tick.type_ != VelocityEventType::INVALID) {
                    int new_val = next_dynamic_val(dynamic_on_current_tick.value_, event.direction_);
                    insert({end_hairpin_tick, VelocityEvent(new_val)});
                }
            } else {
                VelocityEvent dynamic_on_end_tick = dynamic_event_for_tick(end_hairpin_tick);
                int velocity_jump = dynamic_on_end_tick.value_ - dynamic_on_current_tick.value_;

                if ((event.direction_ == ChangeDirection::INCREASING && velocity_jump < 0) ||
                    (event.direction_ == ChangeDirection::DECREASING && velocity_jump > 0)) {
                    int new_val = next_dynamic_val(dynamic_on_current_tick.value_, event.direction_);
                    event.value_ = new_val - dynamic_on_current_tick.value_;
                }
            }
        }
    }
}

//---------------------------------------------------------
//   fillHairpinsCache
///   cache start and end values for each hairpin
//---------------------------------------------------------

void VelocityMap::fill_hairpins_cache() {
    for (auto i = begin(); i != end(); i++) {
        Fraction tick = i->first;
        auto &event = i->second;
        if (event.type_ != VelocityEventType::HAIRPIN) {
            continue;
        }

        // Phase 1: cache a start value for the hairpin
        // Try and get a dynamic at the tick of this hairpin
        bool found_dynamic = false;
        auto values = this->equal_range(tick);
        for (auto it = values.first; it != values.second; ++it) {
            auto &current_change_event = it->second;
            if (current_change_event.type_ == VelocityEventType::DYNAMIC) {
                event.cached_start_val_ = current_change_event.value_;
                found_dynamic = true;
                break;
            }
        }

        // If there isn't a dynamic, use from the last event:
        //  - the cached end value if it's a hairpin
        //  - the value if it's a dynamic
        if (!found_dynamic) {
            if (i != begin()) {
                auto prev_change_event_iter = i;
                prev_change_event_iter--;

                // Look for a hairpin first
                bool found_hairpin = false;
                auto prev_values = this->equal_range(prev_change_event_iter->first);
                for (auto it = prev_values.first; it != prev_values.second; ++it) {
                    auto &prev_change_event = it->second;
                    if (prev_change_event.type_ == VelocityEventType::HAIRPIN) {
                        event.cached_start_val_ = prev_change_event.cached_end_val_;
                        found_hairpin = true;
                        break;
                    }
                }

                if (!found_hairpin) {
                    // prevChangeEventIter must point to a dynamic in this case
                    event.cached_start_val_ = prev_change_event_iter->second.value_;
                }
            } else {
                event.cached_start_val_ = ::mu::engraving::VelocityMap::default_value;
            }
        }

        // Phase 2: cache an end value for the hairpin
        // If there's no set velocity change:
        if (event.value_ == 0) {
            auto next_change_event_iter = i;
            next_change_event_iter++;
            // There's a chance that the next event is a dynamic at the same tick as the
            // start of the current hairpin. If so, get the next event, which is assured
            // to be a different (larger) tick
            if (next_change_event_iter != end() && next_change_event_iter->first == tick) {
                next_change_event_iter++;
            }

            // If this is the last event, there is no change
            if (next_change_event_iter == end()) {
                event.cached_end_val_ = event.cached_start_val_;
            } else {
                // Search for a dynamic event at the next event point
                bool found_dynamic2 = false;
                auto next_values = this->equal_range(next_change_event_iter->first);
                for (auto it = next_values.first; it != next_values.second; ++it) {
                    auto &next_change_event = it->second;
                    if (next_change_event.type_ == VelocityEventType::DYNAMIC) {
                        event.cached_end_val_ = next_change_event.value_;
                        found_dynamic2 = true;
                        break;
                    }
                }

                // We haven't found a dynamic, so there must be a hairpin. What does the user want?
                // A good guess would to be to interpolate, but that might get complex, so just ignore
                // this hairpin and set the ending to be the same as the start.
                // TODO: implementing some form of smart interpolation would be nice.
                if (!found_dynamic2) {
                    event.cached_end_val_ = event.cached_start_val_;
                }
            }
        } else {
            event.cached_end_val_ = event.cached_start_val_ + event.value_;
        }

        // And finally... if something's wrong, make it not wrong
        if ((event.cached_start_val_ > event.cached_end_val_ && event.direction_ == ChangeDirection::INCREASING) ||
            (event.cached_start_val_ < event.cached_end_val_ && event.direction_ == ChangeDirection::DECREASING)) {
            event.cached_end_val_ = event.cached_start_val_;
        }
    }
}

//---------------------------------------------------------
//   setup
//---------------------------------------------------------

void VelocityMap::setup() {
    sort_hairpins();
    resolve_hairpin_collisions();
    resolve_dynamic_inside_hairpin_collisions();
    add_missing_dynamics_after_hairpins();
    fill_hairpins_cache();
}

//---------------------------------------------------------
//   changesInRange
///   returns a list of changes in a range, and their start and end points
//---------------------------------------------------------

std::vector<std::pair<Fraction, Fraction>> VelocityMap::changes_in_range(Fraction stick, Fraction etick) const {
    std::vector<std::pair<Fraction, Fraction>> temp_changes;

    // Force a new event on every noteon, in case the velocity has changed
    temp_changes.push_back(std::make_pair(stick, stick));
    for (auto iter = lower_bound(stick); iter != end(); iter++) {
        Fraction tick = iter->first;
        if (tick > etick) {
            break;
        }

        auto &event = iter->second;
        if (event.type_ == VelocityEventType::DYNAMIC) {
            temp_changes.push_back(std::make_pair(tick, tick));
        } else if (event.type_ == VelocityEventType::HAIRPIN) {
            Fraction event_etick = tick + event.length_;
            Fraction use_etick = event_etick > etick ? etick : event_etick;
            temp_changes.push_back(std::make_pair(tick, use_etick));
        }
    }

    // And also go back one and try to find hairpin coming into this range
    auto iter = lower_bound(stick);
    if (iter != begin()) {
        iter--;
        auto &event = iter->second;
        if (event.type_ == VelocityEventType::HAIRPIN) {
            Fraction event_etick = iter->first + event.length_;
            if (event_etick > stick) {
                temp_changes.push_back(std::make_pair(stick, event_etick));
            }
        }
    }
    return temp_changes;
}

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool VelocityEvent::operator==(const VelocityEvent &event) const {
    return value_ == event.value_ && type_ == event.type_ && length_ == event.length_ && method_ == event.method_ &&
           direction_ == event.direction_;
}
} // namespace mu::engraving
