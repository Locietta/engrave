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
    IntervalList regularIntervals;
    IntervalList collisionFreeIntervals;

    collectIntervals(regularIntervals, collisionFreeIntervals);

    tree_ = interval_tree::IntervalTree<Spanner *>(std::move(regularIntervals));
    collisionFreeTree_ = interval_tree::IntervalTree<Spanner *>(std::move(collisionFreeIntervals));
    dirty_ = false;
}

//---------------------------------------------------------
//   findContained
//---------------------------------------------------------

const SpannerMap::IntervalList &SpannerMap::findContained(int start, int stop, bool excludeCollisions) const {
    if (dirty_) {
        update();
    }

    results_.clear();

    if (excludeCollisions) {
        results_ = collisionFreeTree_.findContained(start, stop);
    } else {
        results_ = tree_.findContained(start, stop);
    }

    return results_;
}

//---------------------------------------------------------
//   findOverlapping
//---------------------------------------------------------

const SpannerMap::IntervalList &SpannerMap::findOverlapping(int start, int stop, bool excludeCollisions) const {
    if (dirty_) {
        update();
    }

    results_.clear();

    if (excludeCollisions) {
        results_ = collisionFreeTree_.findOverlapping(start, stop);
    } else {
        results_ = tree_.findOverlapping(start, stop);
    }

    return results_;
}

void SpannerMap::collectIntervals(IntervalList &regularIntervals, IntervalList &collisionFreeIntervals) const {
    using IntervalsByType = std::map<ElementType, IntervalList>;
    using IntervalsByPart = std::map<ID, IntervalsByType>;

    IntervalsByPart intervalsByPart;

    //!Note Because of the current UX of spanners adjustments spanners collision is a regular thing,
    //!     so we have to manage those cases when two similar spanners (e.g. Pedal line) are overlapping
    //!     with each other.
    constexpr int collidingSpannersPadding = 1;

    for (const auto &pair : *this) {
        Spanner *spanner = pair.second;

        int newSpannerStartTick = spanner->tick().ticks();
        int newSpannerEndTick = spanner->tick2().ticks();

        IntervalsByType &intervalsByType = intervalsByPart[spanner->part()->id()];
        IntervalList &intervalList = intervalsByType[spanner->type()];

        if (!intervalList.empty()) {
            auto lastIntervalIt = intervalList.rbegin();
            if (lastIntervalIt->stop >= newSpannerStartTick) {
                if (!lastIntervalIt->value->isLinked(spanner)) {
                    lastIntervalIt->stop = newSpannerStartTick - collidingSpannersPadding;
                }
            }
        }

        intervalList.emplace_back(interval_tree::Interval<Spanner *>(newSpannerStartTick, newSpannerEndTick, spanner));

        regularIntervals.push_back(interval_tree::Interval(newSpannerStartTick, newSpannerEndTick, spanner));
    }

    for (const auto &pair : intervalsByPart) {
        for (const auto &intervals : pair.second) {
            collisionFreeIntervals.insert(collisionFreeIntervals.end(), intervals.second.begin(), intervals.second.end());
        }
    }
}

//---------------------------------------------------------
//   addSpanner
//---------------------------------------------------------

void SpannerMap::addSpanner(Spanner *s) {
    insert(std::pair<int, Spanner *>(s->tick().ticks(), s));
    dirty_ = true;
}

//---------------------------------------------------------
//   removeSpanner
//---------------------------------------------------------

bool SpannerMap::removeSpanner(Spanner *s) {
    for (auto i = begin(); i != end(); ++i) {
        if (i->second == s) {
            erase(i);
            dirty_ = true;
            return true;
        }
    }
    LOGD("%s (%p) not found", s->typeName(), s);
    return false;
}

#ifndef NDEBUG
//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void SpannerMap::dump() const {
    LOGD("SpannerMap::dump");
    for (auto i = begin(); i != end(); ++i) {
        LOGD("   %5d: %s %p", i->first, i->second->typeName(), i->second);
    }
}

#endif
} // namespace mu::engraving
