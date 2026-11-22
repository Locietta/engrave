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

#include "editsystemlocks.h"

#include <algorithm>

#include "../dom/measure.h"
#include "../dom/measurebase.h"
#include "../dom/rangelock.h"
#include "../dom/score.h"
#include "../dom/system.h"
#include "../dom/systemlockindicator.h"
#include "editpagelocks.h"
#include "engraving/editing/cmd.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

static void apply_add_system_lock(const RangeLock *lock) {
    auto *score = lock->start_mb()->score();
    note_change(lock->start_mb());
    note_change(lock->end_mb());
    score->add_system_lock(lock);
}

static void apply_remove_system_lock(const RangeLock *lock) {
    auto *score = lock->start_mb()->score();
    note_change(lock->start_mb());
    note_change(lock->end_mb());
    defer_root(*score, lock);
    score->remove_system_lock(lock);
}

void EditSystemLocks::undo_add_system_lock(Transaction &tx, const RangeLock *lock) {
    update_layout_breaks_on_add_system_lock(tx, lock);
    apply_add_system_lock(lock);
}

void EditSystemLocks::undo_remove_system_lock(Transaction &tx, const RangeLock *lock) { apply_remove_system_lock(lock); }

void EditSystemLocks::update_layout_breaks_on_add_system_lock(Transaction &tx, const RangeLock *lock) {
    bool move_section_break = false;
    bool move_page_break = false;
    MeasureBase *page_lock_start_mb_value = nullptr;
    for (MeasureBase *mb = lock->start_mb(); mb && mb->is_before_or_equal(lock->end_mb()); mb = mb->next_mm()) {
        mb->undo_set_break(false, LayoutBreakType::LINE);
        if (mb != lock->end_mb()) {
            // Move existing page breaks and section breaks to the end of the new range
            move_section_break |= mb->section_break();
            move_page_break |= mb->page_break();
            mb->undo_set_break(false, LayoutBreakType::SECTION);
            mb->undo_set_break(false, LayoutBreakType::PAGE);
        } else {
            // End of system lock
            // System lock and no-break would conflict
            mb->undo_set_break(false, LayoutBreakType::NOBREAK);
        }

        if (mb != lock->start_mb() && mb->is_start_of_page_lock()) {
            // Page lock starting within range of new system lock
            // Move start of page lock to measure following end of system lock
            // (a page lock starting together with the system lock does not conflict with it)
            const RangeLock *page_lock = mb->page_lock();
            MeasureBase *page_lock_end_mb = page_lock->end_mb();

            if (page_lock_end_mb->is_after(lock->end_mb()) && lock->end_mb()->next()) {
                EditPageLocks::undo_remove_page_lock(tx, page_lock);
                RangeLock *new_page_lock = new RangeLock(lock->end_mb()->next(), page_lock_end_mb);
                EditPageLocks::undo_add_page_lock(tx, new_page_lock);
            }
        }

        if (mb->is_end_of_page_lock()) {
            // Create an updated page lock which extends to the end of the new range
            const RangeLock *page_lock = mb->page_lock();
            if (!page_lock_start_mb_value) {
                page_lock_start_mb_value = page_lock->start_mb();
            }
            EditPageLocks::undo_remove_page_lock(tx, page_lock);
        }
    }

    if (page_lock_start_mb_value) {
        RangeLock *new_page_lock = new RangeLock(page_lock_start_mb_value, lock->end_mb());
        EditPageLocks::undo_add_page_lock(tx, new_page_lock);
    }
    if (move_section_break) {
        lock->end_mb()->undo_set_break(true, LayoutBreakType::SECTION);
    }
    if (move_page_break) {
        lock->end_mb()->undo_set_break(true, LayoutBreakType::PAGE);
    }
}

void EditSystemLocks::remove_system_locks_on_remove_measures(Transaction &tx, Score *score, const MeasureBase *m1, const MeasureBase *m2) {
    std::vector<const RangeLock *> all_sys_locks = score->system_locks()->all_locks();
    for (const RangeLock *lock : all_sys_locks) {
        MeasureBase *lock_start = lock->start_mb();
        MeasureBase *lock_end = lock->end_mb();
        bool lock_start_is_in_range = lock_start->is_after_or_equal(m1) && lock_start->is_before_or_equal(m2);
        bool lock_end_is_in_range = lock_end->is_after_or_equal(m1) && lock_end->is_before_or_equal(m2);
        if (lock_start_is_in_range || lock_end_is_in_range) {
            undo_remove_system_lock(tx, lock);
        }
        if (lock_start_is_in_range && !lock_end_is_in_range) {
            MeasureBase *new_lock_start = m2->next_measure();
            if (new_lock_start) {
                undo_add_system_lock(tx, new RangeLock(new_lock_start, lock_end));
            }
        } else if (!lock_start_is_in_range && lock_end_is_in_range) {
            MeasureBase *new_lock_end = m1->prev_measure();
            if (new_lock_end) {
                undo_add_system_lock(tx, new RangeLock(lock_start, new_lock_end));
            }
        }
    }
}

void EditSystemLocks::remove_system_locks_containing_mm_rests(Transaction &tx, Score *score) {
    std::vector<const RangeLock *> all_locks = score->system_locks()->all_locks(); // copy
    for (const RangeLock *lock : all_locks) {
        for (MeasureBase *mb = lock->start_mb(); mb; mb = mb->next()) {
            if (mb->is_measure() && to_measure(mb)->mm_rest()) {
                undo_remove_system_lock(tx, lock);
                break;
            }
            if (mb->is_after(lock->end_mb())) {
                break;
            }
        }
    }
}

void EditSystemLocks::update_system_locks_on_create_mm_rests(Transaction &tx, Score *score, Measure *first, Measure *last) {
    // NOTE: this must be done during layout as the mmRests get created.

    for (const RangeLock *lock : score->system_locks()->locks_contained_in_range(first, last)) {
        // These locks are inside the range of the mmRest so remove them
        undo_remove_system_lock(tx, lock);
    }

    const RangeLock *lock_on_first = score->system_locks()->lock_containing(first);
    const RangeLock *lock_on_last = score->system_locks()->lock_containing(last);

    if (lock_on_first) {
        MeasureBase *start_mb = lock_on_first->start_mb();
        MeasureBase *end_mb = lock_on_first->end_mb();

        if (start_mb->is_before(first)) {
            if (end_mb->is_before_or_equal(last)) {
                end_mb = first->mm_rest();
            } else {
                return;
            }
        } else {
            start_mb = first->mm_rest();
        }

        if (start_mb != lock_on_first->start_mb() || end_mb != lock_on_first->end_mb()) {
            undo_remove_system_lock(tx, lock_on_first);
            undo_add_system_lock(tx, new RangeLock(start_mb, end_mb));
        }
    }

    if (!lock_on_last || lock_on_last == lock_on_first) {
        return;
    }

    MeasureBase *start_mb = lock_on_last->start_mb();
    MeasureBase *end_mb = lock_on_last->end_mb();
    assert(start_mb->is_after(first) && end_mb->is_after(last));

    undo_remove_system_lock(tx, lock_on_last);
    start_mb = last->next_mm();
    undo_add_system_lock(tx, new RangeLock(start_mb, end_mb));
}
