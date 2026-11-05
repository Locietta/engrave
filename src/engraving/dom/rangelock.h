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
class RangeLock {
public:
    RangeLock(MeasureBase *start, MeasureBase *end) : startMB_(start), endMB_(end) {
        assert(startMB_->isBefore(endMB_) || startMB_ == endMB_);
    }

    MeasureBase *startMB() const { return startMB_; }

    MeasureBase *endMB() const { return endMB_; }

    bool contains(const MeasureBase *mb) const;

private:
    MeasureBase *startMB_;
    MeasureBase *endMB_;
};

class RangeLocks {
public:
    void add(const RangeLock *lock);
    void remove(const RangeLock *lock);

    void removeLockStartingAt(const MeasureBase *mb) { rangeLocks_.erase(mb); }

    void clear() { rangeLocks_.clear(); }

    const RangeLock *lockStartingAt(const MeasureBase *mb) const;
    const RangeLock *lockContaining(const MeasureBase *mb) const;
    std::vector<const RangeLock *> locksContainedInRange(const MeasureBase *start, const MeasureBase *end) const;

    std::vector<const RangeLock *> allLocks() const;

private:
#ifndef NDEBUG
    void sanityCheck();
    void dump() const;
#endif

    struct Ordering {
        bool operator()(const MeasureBase *a, const MeasureBase *b) const { return a->isBefore(b); }
    };

    std::map<const MeasureBase *, const RangeLock *, Ordering> rangeLocks_;
};
} // namespace mu::engraving
