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

#include "spannermap.h"
#include "spanner.h"
#include "part.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   SpannerMap
//---------------------------------------------------------

SpannerMap::SpannerMap() : std::multimap<int, Spanner *>() { dirty_ = true; }

//---------------------------------------------------------
//   update
//   updates the internal lookup tree, not the map itself
//---------------------------------------------------------

void SpannerMap::update() const {
    TRACEFUNC;
    IntervalList regular_intervals;
    IntervalList collision_free_intervals;

    collect_intervals(regular_intervals, collision_free_intervals);

    tree_ = interval_tree::IntervalTree<Spanner *>(std::move(regular_intervals));
    collision_free_tree_ = interval_tree::IntervalTree<Spanner *>(std::move(collision_free_intervals));
    dirty_ = false;
}

//---------------------------------------------------------
//   findContained
//---------------------------------------------------------

const SpannerMap::IntervalList &SpannerMap::find_contained(int start, int stop, bool exclude_collisions) const {
    if (dirty_) {
        update();
    }

    results_.clear();

    if (exclude_collisions) {
        results_ = collision_free_tree_.findContained(start, stop);
    } else {
        results_ = tree_.findContained(start, stop);
    }

    return results_;
}

//---------------------------------------------------------
//   findOverlapping
//---------------------------------------------------------

const SpannerMap::IntervalList &SpannerMap::find_overlapping(int start, int stop, bool exclude_collisions) const {
    if (dirty_) {
        update();
    }

    results_.clear();

    if (exclude_collisions) {
        results_ = collision_free_tree_.findOverlapping(start, stop);
    } else {
        results_ = tree_.findOverlapping(start, stop);
    }

    return results_;
}

void SpannerMap::collect_intervals(IntervalList &regular_intervals, IntervalList &collision_free_intervals) const {
    using IntervalsByType = std::map<ElementType, IntervalList>;
    using IntervalsByPart = std::map<ID, IntervalsByType>;

    IntervalsByPart intervals_by_part;

    //!Note Because of the current UX of spanners adjustments spanners collision is a regular thing,
    //!     so we have to manage those cases when two similar spanners (e.g. Pedal line) are overlapping
    //!     with each other.
    constexpr int colliding_spanners_padding = 1;

    for (const auto &pair : *this) {
        Spanner *spanner = pair.second;

        int new_spanner_start_tick = spanner->tick().ticks();
        int new_spanner_end_tick = spanner->tick2().ticks();

        IntervalsByType &intervals_by_type = intervals_by_part[spanner->part()->id()];
        IntervalList &interval_list = intervals_by_type[spanner->type()];

        if (!interval_list.empty()) {
            auto last_interval_it = interval_list.rbegin();
            if (last_interval_it->stop >= new_spanner_start_tick) {
                if (!last_interval_it->value->is_linked(spanner)) {
                    last_interval_it->stop = new_spanner_start_tick - colliding_spanners_padding;
                }
            }
        }

        interval_list.emplace_back(interval_tree::Interval<Spanner *>(new_spanner_start_tick, new_spanner_end_tick, spanner));

        regular_intervals.push_back(interval_tree::Interval(new_spanner_start_tick, new_spanner_end_tick, spanner));
    }

    for (const auto &pair : intervals_by_part) {
        for (const auto &intervals : pair.second) {
            collision_free_intervals.insert(collision_free_intervals.end(), intervals.second.begin(), intervals.second.end());
        }
    }
}

//---------------------------------------------------------
//   addSpanner
//---------------------------------------------------------

void SpannerMap::add_spanner(Spanner *s) {
    insert(std::pair<int, Spanner *>(s->tick().ticks(), s));
    dirty_ = true;
}

//---------------------------------------------------------
//   removeSpanner
//---------------------------------------------------------

bool SpannerMap::remove_spanner(Spanner *s) {
    for (auto i = begin(); i != end(); ++i) {
        if (i->second == s) {
            erase(i);
            dirty_ = true;
            return true;
        }
    }
    LOGD("%s (%p) not found", s->type_name(), s);
    return false;
}

#ifndef NDEBUG
//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void SpannerMap::dump() const {
    LOGD("SpannerMap::dump");
    for (auto i = begin(); i != end(); ++i) {
        LOGD("   %5d: %s %p", i->first, i->second->type_name(), i->second);
    }
}

#endif
} // namespace mu::engraving
