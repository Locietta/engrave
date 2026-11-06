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

#include <map>

#include "measurebase.h"

namespace mu::engraving {
struct RangeLock {
    RangeLock(MeasureBase *start, MeasureBase *end) : start_mb_(start), end_mb_(end) {
        assert(start_mb_->is_before(end_mb_) || start_mb_ == end_mb_);
    }

    MeasureBase *start_mb() const { return start_mb_; }

    MeasureBase *end_mb() const { return end_mb_; }

    bool contains(const MeasureBase *mb) const;

private:
    MeasureBase *start_mb_;
    MeasureBase *end_mb_;
};

struct RangeLocks {
    void add(const RangeLock *lock);
    void remove(const RangeLock *lock);

    void remove_lock_starting_at(const MeasureBase *mb) { range_locks_.erase(mb); }

    void clear() { range_locks_.clear(); }

    const RangeLock *lock_starting_at(const MeasureBase *mb) const;
    const RangeLock *lock_containing(const MeasureBase *mb) const;
    std::vector<const RangeLock *> locks_contained_in_range(const MeasureBase *start, const MeasureBase *end) const;

    std::vector<const RangeLock *> all_locks() const;

private:
#ifndef NDEBUG
    void sanity_check();
    void dump() const;
#endif

    struct Ordering {
        bool operator()(const MeasureBase *a, const MeasureBase *b) const { return a->is_before(b); }
    };

    std::map<const MeasureBase *, const RangeLock *, Ordering> range_locks_;
};
} // namespace mu::engraving
