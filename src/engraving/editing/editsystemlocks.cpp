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
#include "transaction/transaction.h"
#include "transaction/undoablecommand.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   AddSystemLock
//---------------------------------------------------------

struct AddSystemLock : UndoableCommand {
private:

    const RangeLock *system_lock_;
public:
    AddSystemLock(const RangeLock *system_lock) : system_lock_(system_lock) {}

    void undo() override {
        Score *score = system_lock_->start_mb()->score();
        score->remove_system_lock(system_lock_);
    }

    void redo() override {
        Score *score = system_lock_->start_mb()->score();
        score->add_system_lock(system_lock_);
    }

    void cleanup(bool was_done) override {
        if (!was_done) {
            delete system_lock_;
            system_lock_ = nullptr;
        }
    }

    UNDO_NAME("AddSystemLock")

    std::vector<EngravingObject *> object_items() const override { return {system_lock_->start_mb(), system_lock_->end_mb()}; }
};

//---------------------------------------------------------
//   RemoveSystemLock
//---------------------------------------------------------

struct RemoveSystemLock : UndoableCommand {
private:

    const RangeLock *system_lock_;
public:
    RemoveSystemLock(const RangeLock *system_lock) : system_lock_(system_lock) {}

    void undo() override {
        Score *score = system_lock_->start_mb()->score();
        score->add_system_lock(system_lock_);
    }

    void redo() override {
        Score *score = system_lock_->start_mb()->score();
        score->remove_system_lock(system_lock_);
    }

    void cleanup(bool was_done) override {
        if (was_done) {
            delete system_lock_;
            system_lock_ = nullptr;
        }
    }

    UNDO_NAME("RemoveSystemLock")

    std::vector<EngravingObject *> object_items() const override { return {system_lock_->start_mb(), system_lock_->end_mb()}; }
};

//---------------------------------------------------------
//   EditSystemLocks
//---------------------------------------------------------

void EditSystemLocks::undo_add_system_lock(Transaction &tx, const RangeLock *lock) {
    update_layout_breaks_on_add_system_lock(tx, lock);
    tx.push(new AddSystemLock(lock));
}

void EditSystemLocks::undo_remove_system_lock(Transaction &tx, const RangeLock *lock) { tx.push(new RemoveSystemLock(lock)); }

void EditSystemLocks::undo_remove_all_locks(Transaction &tx, Score *score) {
    std::vector<const RangeLock *> all_locks = score->system_locks()->all_locks(); // copy
    for (const RangeLock *lock : all_locks) {
        undo_remove_system_lock(tx, lock);
    }
}

void EditSystemLocks::toggle_system_lock(Transaction &tx, Score *score, const std::vector<System *> &systems) {
    // Systems may be replaced by the boundary; retain source anchors first.
    std::vector<MeasureBase *> starts;
    for (auto *system : systems) {
        auto *first = system->first();
        if (first && first->is_measure() && to_measure(first)->is_mm_rest()) first = to_measure(first)->mm_rest_first();
        if (first) starts.push_back(first);
    }
    score->ensure_layout();
    std::vector<System *> current;
    for (auto *first : starts) {
        auto *owner = first->is_measure() ? to_measure(first)->covering_mm_rest_or_this()->system() : first->system();
        if (owner && std::find(current.begin(), current.end(), owner) == current.end()) current.push_back(owner);
    }
    bool unlock_all = true;
    for (const System *system : current) {
        if (!system->is_locked()) {
            unlock_all = false;
            break;
        }
    }

    for (System *system : current) {
        MeasureBase *start_meas = system->first();
        const RangeLock *current_lock = score->system_locks()->lock_starting_at(start_meas);
        if (current_lock && unlock_all) {
            undo_remove_system_lock(tx, current_lock);
            continue;
        } else if (!current_lock && !unlock_all) {
            RangeLock *new_system_lock = new RangeLock(start_meas, system->last());
            undo_add_system_lock(tx, new_system_lock);
        }
    }
}

void EditSystemLocks::toggle_score_lock(Transaction &tx, Score *score) {
    score->ensure_layout();
    bool unlock_all = true;
    for (const System *system : score->systems()) {
        const MeasureBase *first = system->first();
        if (!(first->is_measure() || first->is_h_box())) {
            continue;
        }
        if (!system->is_locked()) {
            unlock_all = false;
            break;
        }
    }

    for (System *system : score->systems()) {
        MeasureBase *start_meas = system->first();
        if (!(start_meas->is_measure() || start_meas->is_h_box())) {
            continue;
        }
        const RangeLock *current_lock = score->system_locks()->lock_starting_at(start_meas);
        if (current_lock && unlock_all) {
            undo_remove_system_lock(tx, current_lock);
            continue;
        } else if (!current_lock && !unlock_all) {
            RangeLock *new_system_lock = new RangeLock(start_meas, system->last());
            undo_add_system_lock(tx, new_system_lock);
        }
    }
}

void EditSystemLocks::add_remove_system_locks(Transaction &tx, Score *score, int interval, bool lock) {
    score->ensure_layout();
    MeasureBase *start_measure = score->selection().start_measure_base();
    MeasureBase *end_measure = score->selection().end_measure_base();
    if (!end_measure) {
        end_measure = score->last_measure_mm();
    }

    if (!start_measure || !end_measure) {
        return;
    }

    if (lock) {
        for (const System *system : score->systems()) {
            if (system->last()->is_before(start_measure)) {
                continue;
            }
            if (system->first()->is_after(end_measure)) {
                break;
            }
            if (!system->is_locked()) {
                undo_add_system_lock(tx, new RangeLock(system->first(), system->last()));
            }
        }
        return;
    }

    std::vector<const RangeLock *> current_locks = score->system_locks()->locks_contained_in_range(start_measure, end_measure);
    for (const RangeLock *l : current_locks) {
        undo_remove_system_lock(tx, l);
    }

    if (interval == 0) {
        return;
    }

    int count = 0;
    MeasureBase *lock_start = nullptr;
    for (MeasureBase *mb = start_measure; mb; mb = mb->next_mm()) {
        if (count == 0) {
            lock_start = mb;
        }
        count++;
        if (count == interval || mb == end_measure) {
            undo_add_system_lock(tx, new RangeLock(lock_start, mb));
            lock_start = nullptr;
            count = 0;
        }
        if (mb == end_measure) {
            break;
        }
    }
}

void EditSystemLocks::make_into_system(Transaction &tx, Score *score, MeasureBase *first, MeasureBase *last) {
    const RangeLock *lock_containingfirst = score->system_locks()->lock_containing(first);
    const RangeLock *lock_containinglast = score->system_locks()->lock_containing(last);

    if (lock_containingfirst) {
        undo_remove_system_lock(tx, lock_containingfirst);
        if (lock_containingfirst->start_mb()->is_before(first)) {
            MeasureBase *one_before_first = first->prev_mm();
            RangeLock *new_lock_before = new RangeLock(lock_containingfirst->start_mb(), one_before_first);
            undo_add_system_lock(tx, new_lock_before);
        }
    }

    if (lock_containinglast) {
        if (lock_containinglast != lock_containingfirst) {
            undo_remove_system_lock(tx, lock_containinglast);
        }
        if (last->is_before(lock_containinglast->end_mb())) {
            MeasureBase *one_after_last = last->next_mm();
            RangeLock *new_lock_after = new RangeLock(one_after_last, lock_containinglast->end_mb());
            undo_add_system_lock(tx, new_lock_after);
        }
    }

    std::vector<const RangeLock *> locks_contained_in_range = score->system_locks()->locks_contained_in_range(first, last);
    for (const RangeLock *lock : locks_contained_in_range) {
        if (lock != lock_containingfirst && lock != lock_containinglast) {
            undo_remove_system_lock(tx, lock);
        }
    }

    MeasureBase *first_mb = first;
    MeasureBase *last_mb = last;

    // If a section break is within this range, create a new system lock after it
    for (MeasureBase *mb = first; mb && mb->is_before_or_equal(last); mb = mb->next_mm()) {
        if (!first_mb) {
            first_mb = mb;
        }
        last_mb = mb;

        if (mb->section_break()) {
            RangeLock *new_lock = new RangeLock(first_mb, last_mb);
            undo_add_system_lock(tx, new_lock);

            first_mb = nullptr;
            last_mb = nullptr;
        }
    }
    if (!first_mb || !last_mb) {
        return;
    }

    RangeLock *new_lock = new RangeLock(first_mb, last_mb);
    undo_add_system_lock(tx, new_lock);
}

void EditSystemLocks::move_measure_to_prev_system(Transaction &tx, Score *score, MeasureBase *m) {
    if (m->is_measure() && to_measure(m)->is_mm_rest()) m = to_measure(m)->mm_rest_first();
    score->ensure_layout();
    if (m->is_measure()) m = to_measure(m)->covering_mm_rest_or_this();
    const System *prev_system = m->prev_non_v_box_system();
    if (!prev_system) {
        return;
    }

    MeasureBase *prev_system_first_meas = prev_system->first();

    make_into_system(tx, score, prev_system_first_meas, m);
}

void EditSystemLocks::move_measure_to_next_system(Transaction &tx, Score *score, MeasureBase *m) {
    if (m->is_measure() && to_measure(m)->is_mm_rest()) m = to_measure(m)->mm_rest_first();
    score->ensure_layout();
    if (m->is_measure()) m = to_measure(m)->covering_mm_rest_or_this();
    const System *cur_system = m->system();
    MeasureBase *start_meas = cur_system->first();
    MeasureBase *system_cur_end_measure = cur_system->last();

    const System *next_system = m->next_non_v_box_system();
    if (!next_system) {
        return;
    }

    const RangeLock *cur_lock = score->system_locks()->lock_starting_at(start_meas);
    const RangeLock *next_sys_lock = score->system_locks()->lock_starting_at(next_system->first());

    MeasureBase *prev_meas = m->prev_mm();

    if (!prev_meas) {
        return;
    }

    const bool page_break = system_cur_end_measure->page_break();
    const bool end_of_page_lock = system_cur_end_measure->is_end_of_page_lock();
    if (page_break) {
        // Move page break to measure preceding selection
        system_cur_end_measure->undo_set_break(false, LayoutBreakType::PAGE);
        prev_meas->undo_set_break(true, LayoutBreakType::PAGE);
    }

    if (end_of_page_lock) {
        // Move page lock to end at measure preceding selection
        const RangeLock *page_lock = system_cur_end_measure->page_lock();
        MeasureBase *page_lock_start_mb_value = page_lock->start_mb();

        EditPageLocks::undo_remove_page_lock(tx, page_lock);

        if (page_lock_start_mb_value->is_before_or_equal(prev_meas)) {
            RangeLock *new_page_lock = new RangeLock(page_lock_start_mb_value, prev_meas);
            EditPageLocks::undo_add_page_lock(tx, new_page_lock);
        }
    }

    if (!cur_lock && !next_sys_lock) {
        if (!page_break && !end_of_page_lock && !prev_meas->system_lock()) {
            // Add a system break
            prev_meas->undo_set_break(true, LayoutBreakType::LINE);
        }
        return;
    }

    if (cur_lock && !next_sys_lock) {
        undo_remove_system_lock(tx, cur_lock);

        MeasureBase *first_measure = cur_system->first();
        MeasureBase *last_measure = m->prev();

        undo_add_system_lock(tx, new RangeLock(first_measure, last_measure));
        return;
    }

    MeasureBase *last_measure = next_system->last();
    make_into_system(tx, score, m, last_measure);
}

void EditSystemLocks::apply_lock_to_selection(Transaction &tx, Score *score) {
    score->ensure_layout();
    MeasureBase *first = nullptr;
    MeasureBase *last = nullptr;

    if (score->selection().is_range()) {
        first = score->selection().start_measure_base();
        last = score->selection().end_measure_base();
    } else {
        for (EngravingItem *el : score->selection().elements()) {
            if (el->is_system_lock_indicator()) {
                const RangeLock *lock = to_system_lock_indicator(el)->system_lock();
                first = lock->start_mb();
                last = lock->end_mb();
                break;
            }
            MeasureBase *mb = el->find_measure_base();
            if (!mb) {
                continue;
            }
            if (!first || mb->is_before(first)) {
                first = mb;
            }
            if (!last || mb->is_after(last)) {
                last = mb;
            }
        }
    }

    if (!first || !last) {
        return;
    }

    const RangeLock *lock_on_last = score->system_locks()->lock_containing(last);
    if (lock_on_last && lock_on_last->end_mb() == last) {
        undo_remove_system_lock(tx, lock_on_last);
    } else if (first != last) {
        make_into_system(tx, score, first, last);
    } else {
        make_into_system(tx, score, first->system()->first(), last);
    }
}

void EditSystemLocks::remove_system_locks_on_add_layout_break(Transaction &tx, Score *score, LayoutBreakType break_type,
                                                              const MeasureBase *measure) {
    IF_ASSERT_FAILED(break_type != LayoutBreakType::NOBREAK) {
        return; // NOBREAK not allowed on locked measures
    }

    const RangeLock *lock = score->system_locks()->lock_containing(measure);
    MeasureBase *lock_end_measure = lock ? lock->end_mb() : nullptr;
    if (lock && (break_type == LayoutBreakType::LINE || measure != lock_end_measure)) {
        undo_remove_system_lock(tx, lock);

        if (measure != lock_end_measure && measure->next()) {
            undo_add_system_lock(tx, new RangeLock(measure->next(), lock_end_measure));
        }
    }
}

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
