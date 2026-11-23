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

#ifndef MU_ENGRAVING_SPANNERMAP_H
#define MU_ENGRAVING_SPANNERMAP_H

#include "engraving/thirdparty/intervaltree/IntervalTree.h"

#include "global/btree.h"

namespace mu::engraving {
struct Spanner;

//---------------------------------------------------------
//   SpannerMap
//---------------------------------------------------------

struct SpannerMap : private muse::BTreeMultiMap<int, Spanner *> {
    using Map = muse::BTreeMultiMap<int, Spanner *>;

    using IntervalList = std::vector<interval_tree::Interval<Spanner *>>;

    const IntervalList &find_contained(int start, int stop, bool exclude_collisions = false) const;
    const IntervalList &find_overlapping(int start, int stop, bool exclude_collisions = false) const;

    const Map &map() const { return *this; }

    void collect_intervals(IntervalList &regular_intervals, IntervalList &collision_free_intervals) const;

    using Map::cbegin;
    using Map::cend;
    using Map::crbegin;
    using Map::crend;
    using Map::empty;

    void add_spanner(Spanner *s);
    bool remove_spanner(Spanner *s);

    void clear() {
        Map::clear();
        dirty_ = true;
    }

    void update() const;

    void set_dirty() const { dirty_ = true; } // must be called if a spanner changes start/length
#ifndef NDEBUG
    void dump() const;
#endif

private:

    mutable bool dirty_ = true;
    mutable interval_tree::IntervalTree<Spanner *> tree_;
    mutable interval_tree::IntervalTree<Spanner *> collision_free_tree_;
    mutable std::vector<interval_tree::Interval<Spanner *>> results_;
};
} // namespace mu::engraving

#endif
