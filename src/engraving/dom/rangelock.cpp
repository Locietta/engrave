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

#include "rangelock.h"

#include "log.h"
#include "measure.h"
#include "measurebase.h"

namespace mu::engraving {
RangeLock::RangeLock(MeasureBase *start, MeasureBase *end) {
    start_mb_ = start->is_measure() && to_measure(start)->is_mm_rest() ? to_measure(start)->mm_rest_first() : start;
    end_mb_ = end->is_measure() && to_measure(end)->is_mm_rest() ? to_measure(end)->mm_rest_last() : end;
    assert(start_mb_ && end_mb_ && start_mb_->is_before_or_equal(end_mb_));
}

bool RangeLock::contains(const MeasureBase *mb) const { return start_mb_->is_before_or_equal(mb) && mb->is_before_or_equal(end_mb_); }

void RangeLocks::add(const RangeLock *lock) {
    range_locks_.emplace(lock->start_mb(), lock);

#ifndef NDEBUG
    sanity_check();
#endif
}

void RangeLocks::remove(const RangeLock *lock) {
    range_locks_.erase(lock->start_mb());

#ifndef NDEBUG
    sanity_check();
#endif
}

const RangeLock *RangeLocks::lock_starting_at(const MeasureBase *mb) const {
    if (mb->is_measure() && to_measure(mb)->is_mm_rest()) mb = to_measure(mb)->mm_rest_first();
    auto iter = range_locks_.find(mb);
    return iter != range_locks_.end() ? iter->second : nullptr;
}

const RangeLock *RangeLocks::lock_containing(const MeasureBase *mb) const {
    if (mb->is_measure() && to_measure(mb)->is_mm_rest()) mb = to_measure(mb)->mm_rest_first();
    if (range_locks_.empty()) {
        return nullptr;
    }

    auto iter = range_locks_.lower_bound(mb);
    if (iter != range_locks_.begin() && (iter == range_locks_.end() || mb->is_before(iter->second->start_mb()))) {
        --iter;
    }

    const RangeLock *lock = iter->second;

    return lock->contains(mb) ? lock : nullptr;
}

std::vector<const RangeLock *> RangeLocks::locks_contained_in_range(const MeasureBase *start, const MeasureBase *end) const {
    if (start->is_measure() && to_measure(start)->is_mm_rest()) start = to_measure(start)->mm_rest_first();
    if (end->is_measure() && to_measure(end)->is_mm_rest()) end = to_measure(end)->mm_rest_last();
    std::vector<const RangeLock *> result;

    for (auto &pair : range_locks_) {
        const RangeLock *lock = pair.second;
        if (start->is_before_or_equal(lock->start_mb()) && lock->end_mb()->is_before_or_equal(end)) {
            result.push_back(lock);
        }
        if (lock->start_mb()->is_after(end)) {
            break;
        }
    }

    return result;
}

std::vector<const RangeLock *> RangeLocks::all_locks() const { return muse::values(range_locks_); }

#ifndef NDEBUG
void RangeLocks::sanity_check() {
    for (auto iter = range_locks_.begin(); iter != range_locks_.end();) {
        auto cur_iter = iter;
        auto next_iter = ++iter;
        if (next_iter == range_locks_.end()) {
            break;
        }

        const MeasureBase *cur_mb = cur_iter->first;
        const RangeLock *cur_lock = cur_iter->second;

        DO_ASSERT(cur_lock->start_mb() == cur_mb);

        const MeasureBase *next_mb = next_iter->first;
        const RangeLock *next_lock = next_iter->second;

        DO_ASSERT(next_lock->start_mb() == next_mb);
        DO_ASSERT(cur_mb->is_before(next_mb));
        DO_ASSERT(cur_lock->end_mb()->is_before(next_lock->start_mb()));
    }
}

void RangeLocks::dump() const {
    for (auto &pair : range_locks_) {
        const RangeLock *sl = pair.second;
        const Measure *start_measure = sl->start_mb()->is_measure() ? to_measure(sl->start_mb()) : sl->start_mb()->prev_measure();
        const Measure *end_measure = sl->end_mb()->is_measure() ? to_measure(sl->end_mb()) : sl->end_mb()->prev_measure();
        LOGD() << "RangeLock --- Start measure: " << (start_measure ? start_measure->measure_number() : -1)
               << ", End Measure: " << (end_measure ? end_measure->measure_number() : -1);
    }
}

#endif
} // namespace mu::engraving
