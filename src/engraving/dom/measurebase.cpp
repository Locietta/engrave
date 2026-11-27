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

#include "measurebase.h"

#include "../editing/editpagelocks.h"
#include "../editing/editsystemlocks.h"
#include "box.h"
#include "engraving/dom/dummyparent.h"
#include "factory.h"
#include "layoutbreak.h"
#include "log.h"
#include "measure.h"
#include "page.h"
#include "score.h"
#include "staff.h"
#include "stafftypechange.h"
#include "system.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   MeasureBase
//---------------------------------------------------------

MeasureBase::MeasureBase(const ElementType &type, Score *parent) : EngravingItem(type, parent) {}

void MeasureBase::set_ownership_parent(Score *score) { EngravingItem::set_ownership_parent(score); }

void MeasureBase::set_ownership_parent(Box *box) { EngravingItem::set_ownership_parent(box); }

EngravingItem *MeasureBase::layout_parent() const {
    if (auto *placed = system()) return placed;
    // e.g. a horizontal frame nested inside a vertical frame is placed within its parent frame
    return EngravingItem::layout_parent();
}

MeasureBase::MeasureBase(const MeasureBase &m) : MeasureBase(m.type(), m) {}

MeasureBase::MeasureBase(ElementType type, const MeasureBase &m) : EngravingItem(type, m) {
    next_ = m.next_;
    prev_ = m.prev_;
    tick_ = m.tick_;

    for (EngravingItem *e : m.el_) {
        add(e->clone());
    }
}

//---------------------------------------------------------
//   clearElements
//---------------------------------------------------------

void MeasureBase::clear_elements() {
    muse::delete_all(el_);
    el_.clear();
}

//---------------------------------------------------------
//   takeElements
//---------------------------------------------------------

ElementList MeasureBase::take_elements() {
    for (auto *item : el_) source_child_removed(*this, item);
    ElementList l;
    l.swap(el_);
    return l;
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void MeasureBase::set_score(Score *score) {
    EngravingItem::set_score(score);
    for (EngravingItem *e : el_) {
        e->set_score(score);
    }
}

//---------------------------------------------------------
//   MeasureBase
//---------------------------------------------------------

MeasureBase::~MeasureBase() {
    if (auto *placed = system()) placed->remove_measure(this);
    muse::delete_all(el_);
}

System *MeasureBase::prev_non_v_box_system() const {
    const System *curr_system = system();
    const MeasureBase *first_in_curr_system = curr_system ? curr_system->first() : nullptr;
    IF_ASSERT_FAILED(first_in_curr_system) { return nullptr; }

    // TODO: This logic is coupled to moveMeasureToPrevSystem - it should be possible to
    // move a MeasureBase from below a VBox (or n VBoxes) to above. Once that's achieved,
    // we can remove lastIsValid and simply iterate until we find a "non VBox system"...
    const MeasureBase *last_in_prev_system = first_in_curr_system->prev_mm();
    const bool last_is_valid = last_in_prev_system && !last_in_prev_system->is_v_box_base();

    return last_is_valid ? last_in_prev_system->system() : nullptr;
}

System *MeasureBase::next_non_v_box_system() const {
    const System *curr_system = system();
    const MeasureBase *last_in_curr_system = curr_system ? curr_system->last() : nullptr;
    IF_ASSERT_FAILED(last_in_curr_system) { return nullptr; }

    // TODO: This logic is coupled to moveMeasureToNextSystem - it should be possible to
    // move a MeasureBase from above a VBox (or n VBoxes) to below. Once that's achieved,
    // we can remove firstIsValid and simply iterate until we find a "non VBox system"...
    const MeasureBase *first_in_next_system = last_in_curr_system->next_mm();
    const bool first_is_valid = first_in_next_system && !first_in_next_system->is_v_box_base();

    return first_is_valid ? first_in_next_system->system() : nullptr;
}

Page *MeasureBase::page() const { return system() ? system()->page() : nullptr; }

Page *MeasureBase::prev_page() const {
    Page *cur_page = page();
    IF_ASSERT_FAILED(cur_page) { return nullptr; }

    Page *prev_page = cur_page;
    for (const MeasureBase *mb = this; mb && prev_page == cur_page; mb = mb->prev_mm()) {
        prev_page = mb->system()->page();
    }

    return prev_page != cur_page ? prev_page : nullptr;
}

Page *MeasureBase::next_page() const {
    Page *cur_page = page();
    IF_ASSERT_FAILED(cur_page) { return nullptr; }

    Page *next_page = cur_page;
    for (const MeasureBase *mb = this; mb && next_page == cur_page; mb = mb->next_mm()) {
        next_page = mb->system()->page();
    }

    return next_page != cur_page ? next_page : nullptr;
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a el to MeasureBase
//---------------------------------------------------------

void MeasureBase::add(EngravingItem *e) {
    if (e->ownership_parent() != this) {
        e->set_ownership_parent(this);
    }

    el_.push_back(e);
    source_child_added(*this, e);
    if (e->is_layout_break())
        sync_break_flags(to_layout_break(e)->is_section_break());
    else
        trigger_layout();
    e->added();
}

//---------------------------------------------------------
//   remove
///   Remove EngravingItem \a el from MeasureBase.
//---------------------------------------------------------

void MeasureBase::remove(EngravingItem *el) {
    if (!el_.remove(el)) {
        LOGD("MeasureBase(%p)::remove(%s,%p) not found", this, el->type_name(), el);
    } else {
        source_child_removed(*this, el);
        el->removed();
    }

    if (el->is_layout_break()) {
        sync_break_flags(to_layout_break(el)->is_section_break());
    } else {
        trigger_layout();
        if (next()) next()->trigger_layout();
    }
}

//---------------------------------------------------------
//   nextMeasure
//---------------------------------------------------------

Measure *MeasureBase::next_measure() const {
    MeasureBase *m = next_;
    for (;;) {
        if (m == 0 || m->is_measure()) {
            break;
        }
        m = m->next_;
    }
    return to_measure(m);
}

//---------------------------------------------------------
//   nextMeasureMM
//---------------------------------------------------------

Measure *MeasureBase::next_measure_mm() const {
    Measure *mm = next_measure();
    if (mm && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && mm->has_mm_rest()) {
        return mm->mm_rest();
    }
    return mm;
}

//---------------------------------------------------------
//   prevMeasure
//---------------------------------------------------------

Measure *MeasureBase::prev_measure() const {
    MeasureBase *m = prev();
    while (m) {
        if (m->is_measure()) {
            return to_measure(m);
        }
        m = m->prev();
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevMeasureMM
//---------------------------------------------------------

Measure *MeasureBase::prev_measure_mm() const {
    Measure *m = prev_measure();
    if (m) {
        return m->covering_mm_rest_or_this();
    }

    return nullptr;
}

const MeasureBase *MeasureBase::mb_with_preceding_section_break() const {
    // we're trying to find the MeasureBase that determines
    // if the next one after this starts a new section
    // if this is a measure, it's the one that determines this
    // but if it is a frame, we may need to look backwards
    const MeasureBase *mb = this;
    while (mb && !mb->is_measure() && !mb->section_break()) {
        mb = mb->prev();
    }
    return mb;
}

//---------------------------------------------------------
//   pause
//---------------------------------------------------------

double MeasureBase::pause() const {
    const LayoutBreak *layout_break = section_break_element();
    return layout_break ? layout_break->pause() : 0.0;
}

//---------------------------------------------------------
//   top
//---------------------------------------------------------

MeasureBase *MeasureBase::top() const {
    const MeasureBase *mb = this;
    while (mb->ownership_parent()) {
        if (mb->ownership_parent()->is_measure_base()) {
            mb = to_measure_base(mb->ownership_parent());
        } else {
            break;
        }
    }
    return const_cast<MeasureBase *>(mb);
}

//---------------------------------------------------------
//   getInScore
//---------------------------------------------------------

MeasureBase *MeasureBase::get_in_score(Score *score, bool use_next_measure_fallback) const {
    MeasureBase *new_mb = nullptr;
    if (!is_measure() && !exclude_from_other_parts()) {
        for (auto e : link_list()) {
            if (e->score() == score) {
                new_mb = to_measure_base(e);
                break;
            }
        }
    }
    if (is_measure() || (!new_mb && use_next_measure_fallback)) {
        new_mb = score->tick2measure(tick());
    }
    if (!new_mb) {
        LOGD("measure base not found in score");
    }
    return new_mb;
}

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

Fraction MeasureBase::tick() const {
    const MeasureBase *mb = top();
    return mb ? mb->tick_ : Fraction(-1, 1);
}

void MeasureBase::set_tick(const Fraction &f) {
    if (tick_ != f) touch_source_subtree(*this);
    tick_ = f;
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void MeasureBase::trigger_layout() const {
    // for measurebases within other measurebases (e.g., hbox within vbox), use top level
    const MeasureBase *mb = top();
    // avoid triggering layout before getting added to a score
    if (mb->prev() || mb->next()) {
        score()->set_layout(mb->tick(), muse::nidx, mb);
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void MeasureBase::scan_elements(muse::FnRef<void(EngravingItem *)> func) {
    if (is_measure()) {
        for (EngravingItem *e : el_) {
            staff_idx_t staff_idx = e->staff_idx();
            if (staff_idx != muse::nidx && staff_idx >= score()->staves().size()) {
                LOGD("MeasureBase::scanElements: bad staffIdx %zu in element %s", staff_idx, e->type_name());
            }
            if ((e->track() == muse::nidx) || e->system_flag() || to_measure(this)->visible(staff_idx)) {
                e->scan_elements(func);
            }
        }
    } else {
        for (EngravingItem *e : el_) {
            e->scan_elements(func);
        }
    }
    if (is_box()) {
        func(this);
    }
}

//---------------------------------------------------------
//   first
//---------------------------------------------------------

MeasureBase *Score::first() const { return measures_.first(); }

//---------------------------------------------------------
//   last
//---------------------------------------------------------

MeasureBase *Score::last() const { return measures_.last(); }

void MeasureBase::sync_break_flags(bool section_changed) {
    const bool was_section = section_break();
    bool line = false;
    bool page = false;
    bool section = false;
    bool no_break = false;
    for (const auto *item : el_) {
        if (!item->is_layout_break()) continue;
        const auto *br = to_layout_break(item);
        line |= br->is_line_break();
        page |= br->is_page_break();
        section |= br->is_section_break();
        no_break |= br->is_no_break();
    }
    set_line_break(line);
    set_page_break(page);
    set_section_break(section);
    set_no_break(no_break);
    if (section_changed || section != was_section) {
        score()->invalidate_repeat_list();
        trigger_layout_to_end();
    }
    trigger_layout();
    if (next()) next()->trigger_layout();
}

void MeasureBase::undo_set_break(bool v, LayoutBreakType type) {
    MeasureBase *mb = (is_measure() && to_measure(this)->is_mm_rest()) ? to_measure(this)->mm_rest_last() : this;
    std::vector<EngravingItem *> remove;
    bool present = false;
    for (auto *item : mb->el()) {
        if (!item->is_layout_break()) continue;
        const auto existing = to_layout_break(item)->layout_break_type();
        present |= existing == type;
        if ((!v && existing == type) || (v && existing != type && !breaks_coexist(existing, type))) remove.push_back(item);
    }
    for (auto *item : remove) {
        if (!v && score()->is_master() && type == LayoutBreakType::SECTION) {
            std::vector<LayoutBreak *> linked_sections;
            for (auto *linked : item->link_list()) {
                if (linked == item || !linked->is_layout_break()) continue;
                auto *lb = to_layout_break(linked);
                if (lb->is_section_break() && lb->is_property_linked_to_master(Pid::LAYOUT_BREAK)) linked_sections.push_back(lb);
            }
            // Removal disconnects links, so collect the eligible copies first.
            for (auto *lb : linked_sections) lb->score()->undo_remove_element(lb, false);
        }
        score()->undo_remove_element(item, false);
    }
    if (v && !present) {
        auto *lb = Factory::create_layout_break(mb);
        lb->set_layout_break_type(type);
        lb->set_track(0);
        score()->undo_add_element(lb);
    }
}

void MeasureBase::cleanup_layout_breaks(bool undo) {
    std::vector<EngravingItem *> retained;
    std::vector<EngravingItem *> to_delete;
    for (auto *e : el()) {
        if (!e->is_layout_break()) continue;
        const auto incoming = to_layout_break(e)->layout_break_type();
        // Import order determines precedence, independently of the cached flags.
        std::erase_if(retained, [&](auto *previous) {
            if (breaks_coexist(to_layout_break(previous)->layout_break_type(), incoming)) return false;
            to_delete.push_back(previous);
            return true;
        });
        retained.push_back(e);
    }
    for (EngravingItem *e : to_delete) {
        if (undo) {
            score()->undo_remove_element(e, false);
        } else {
            remove(e);
            delete e;
        }
    }
}

//---------------------------------------------------------
//   nextMM
//---------------------------------------------------------

MeasureBase *MeasureBase::next_mm() const {
    if (next_ && next_->is_measure() && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && to_measure(next_)->has_mm_rest()) {
        return to_measure(next_)->mm_rest();
    }
    return next_;
}

//---------------------------------------------------------
//   prevMM
//---------------------------------------------------------

MeasureBase *MeasureBase::prev_mm() const {
    if (prev_ && prev_->is_measure() && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS)) {
        return const_cast<Measure *>(to_measure(prev_)->covering_mm_rest_or_this());
    }
    return prev_;
}

//---------------------------------------------------------
//   index
//---------------------------------------------------------

int MeasureBase::index() const {
    int idx = 0;
    MeasureBase *m = score()->first();
    Measure *mm_rest_first = nullptr;
    if (is_measure() && to_measure(this)->is_mm_rest()) {
        mm_rest_first = to_measure(this)->mm_rest_first();
    }
    while (m) {
        if (m == this || m == mm_rest_first) {
            return idx;
        }
        m = m->next();
        ++idx;
    }
    return -1;
}

//---------------------------------------------------------
//   measureIndex
//    returns index of measure counting only Measures but
//    skipping other MeasureBase descendants
//---------------------------------------------------------

int MeasureBase::measure_index() const {
    int idx = 0;
    MeasureBase *m = score()->first_measure();
    Measure *mm_rest_first = nullptr;
    if (is_measure() && to_measure(this)->is_mm_rest()) {
        mm_rest_first = to_measure(this)->mm_rest_first();
    }
    while (m) {
        if (m == this || m == mm_rest_first) {
            return idx;
        }
        m = m->next();
        if (m && m->is_measure()) {
            ++idx;
        }
    }
    return -1;
}

bool MeasureBase::is_before(const EngravingItem *other) const {
    if (other->is_measure_base()) {
        const MeasureBase *other_mb = to_measure_base(other);
        return is_before(other_mb);
    }

    return EngravingItem::is_before(other);
}

bool MeasureBase::is_before(const MeasureBase *other) const {
    if (this == other) {
        return false;
    }

    Fraction other_tick = other->tick();
    if (other_tick != tick_) {
        return tick_ < other_tick;
    }

    if (this->is_measure() && other->is_measure()) {
        // (this == other) has already been excluded, so this is only
        // possible if one is the overlying mmRest starting on the other.
        // Let's set by convention that the mmRest isBefore the underlying measure.
        return to_measure(this)->is_mm_rest();
    }

    bool other_is_mm_rest = other->is_measure() && to_measure(other)->is_mm_rest();
    for (const MeasureBase *mb = other_is_mm_rest ? next_mm() : next(); mb && mb->tick() == tick_;
         mb = other_is_mm_rest ? mb->next_mm() : mb->next()) {
        if (mb == other) {
            return true;
        }
    }

    return false;
}

static const MeasureBase *display_lock_endpoint(const MeasureBase *source, bool end) {
    if (!source->is_measure()) return source;
    const auto *display = to_measure(source)->covering_mm_rest_or_this();
    if (display == source || end || display->mm_rest_first() == source) return display;
    return display->next_mm();
}

static const RangeLock *display_lock_at(const RangeLocks &locks, const MeasureBase *base, bool start_only) {
    const auto *display = base->is_measure() ? to_measure(base)->covering_mm_rest_or_this() : base;
    for (const auto *lock : locks.all_locks()) {
        const auto *start = display_lock_endpoint(lock->start_mb(), false);
        const auto *end = display_lock_endpoint(lock->end_mb(), true);
        // A lock wholly inside one collapsed range has no display boundary.
        if (!start || start->is_after(end) || (start == end && start->is_measure() && to_measure(start)->is_mm_rest())) continue;
        if (start_only ? start == display : start->is_before_or_equal(display) && display->is_before_or_equal(end)) return lock;
    }
    return nullptr;
}

const RangeLock *MeasureBase::system_lock() const { return display_lock_at(*score()->system_locks(), this, false); }

bool MeasureBase::is_start_of_system_lock() const { return display_lock_at(*score()->system_locks(), this, true); }

bool MeasureBase::is_end_of_system_lock() const {
    const auto *lock = system_lock();
    const auto *display = is_measure() ? to_measure(this)->covering_mm_rest_or_this() : this;
    return lock && display_lock_endpoint(lock->end_mb(), true) == display;
}

const RangeLock *MeasureBase::page_lock() const { return display_lock_at(*score()->page_locks(), this, false); }

bool MeasureBase::is_start_of_page_lock() const { return display_lock_at(*score()->page_locks(), this, true); }

bool MeasureBase::is_end_of_page_lock() const {
    const auto *lock = page_lock();
    const auto *display = is_measure() ? to_measure(this)->covering_mm_rest_or_this() : this;
    return lock && display_lock_endpoint(lock->end_mb(), true) == display;
}

//---------------------------------------------------------
//   sectionBreakElement
//---------------------------------------------------------

LayoutBreak *MeasureBase::section_break_element() const {
    if (section_break()) {
        for (EngravingItem *e : el()) {
            if (e->is_layout_break() && to_layout_break(e)->is_section_break()) {
                return to_layout_break(e);
            }
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   MeasureBaseList
//---------------------------------------------------------

MeasureBaseList::MeasureBaseList(Score *owner) : owner_(owner) {
    first_ = nullptr;
    last_ = nullptr;
    size_ = 0;
}

void MeasureBaseList::clear() {
    if (owner_) source_collection_cleared(*owner_, SourceCollection::MEASURES);
    first_ = nullptr;
    last_ = nullptr;
    size_ = 0;
    tick_index_.clear();
}

//---------------------------------------------------------
//   push_back
//---------------------------------------------------------

void MeasureBaseList::push_back(MeasureBase *m) {
    ++size_;
    if (last_) {
        last_->set_next(m);
        m->set_prev(last_);
        m->set_next(0);
    } else {
        first_ = m;
        m->set_prev(0);
        m->set_next(0);
    }
    last_ = m;
    if (owner_) source_child_added(*owner_, m);
}

//---------------------------------------------------------
//   push_front
//---------------------------------------------------------

void MeasureBaseList::push_front(MeasureBase *m) {
    ++size_;
    if (first_) {
        first_->set_prev(m);
        m->set_next(first_);
        m->set_prev(0);
    } else {
        last_ = m;
        m->set_prev(0);
        m->set_next(0);
    }
    first_ = m;
    if (owner_) source_child_added(*owner_, m);
}

//---------------------------------------------------------
//   add
//    insert m before m->next()
//---------------------------------------------------------

void MeasureBaseList::add(MeasureBase *m) {
    MeasureBase *el = m->next();
    if (el == 0) {
        append(m);
        return;
    }
    if (el == first_) {
        push_front(m);
        return;
    }
    ++size_;
    m->set_prev(el->prev());
    el->prev()->set_next(m);
    el->set_prev(m);
    if (owner_) source_child_added(*owner_, m);
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

// Detached measures remain alive until batch cleanup. Their display overlays are no
// longer geometry for source items that survived elsewhere in the score.
static void clear_measure_layout_copies(MeasureBase *base) {
    if (base->is_measure()) {
        auto *measure = to_measure(base);
        if (auto *display = measure->mm_rest()) measure->score()->clear_layout_copies(*display);
    }
}

void MeasureBaseList::remove(MeasureBase *m) {
    if (owner_) source_child_removed(*owner_, m);
    clear_measure_layout_copies(m);
    --size_;
    if (m->prev()) {
        m->prev()->set_next(m->next());
    } else {
        first_ = m->next();
    }
    if (m->next()) {
        m->next()->set_prev(m->prev());
    } else {
        last_ = m->prev();
    }
}

//---------------------------------------------------------
//   insert
//---------------------------------------------------------

void MeasureBaseList::insert(MeasureBase *fm, MeasureBase *lm) {
    for (MeasureBase *m = fm;; m = m->next()) {
        if (owner_) source_child_added(*owner_, m);
        if (m == lm) break;
    }
    for (MeasureBase *m = fm; m != lm; m = m->next()) {
        ++size_;
    }
    ++size_;
    MeasureBase *pm = fm->prev();
    if (pm) {
        pm->set_next(fm);
    } else {
        first_ = fm;
    }
    MeasureBase *nm = lm->next();
    if (nm) {
        nm->set_prev(lm);
    } else {
        last_ = lm;
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void MeasureBaseList::remove(MeasureBase *fm, MeasureBase *lm) {
    for (MeasureBase *measure = fm;; measure = measure->next()) {
        if (owner_) source_child_removed(*owner_, measure);
        clear_measure_layout_copies(measure);
        if (measure == lm) break;
    }
    for (MeasureBase *m = fm; m != lm; m = m->next()) {
        --size_;
    }
    --size_;
    MeasureBase *pm = fm->prev();
    MeasureBase *nm = lm->next();
    if (pm) {
        pm->set_next(nm);
    } else {
        first_ = nm;
    }
    if (nm) {
        nm->set_prev(pm);
    } else {
        last_ = pm;
    }
}

//---------------------------------------------------------
//   change
//---------------------------------------------------------

void MeasureBaseList::change(MeasureBase *ob, MeasureBase *nb) {
    if (owner_) source_child_removed(*owner_, ob);
    if (owner_) source_child_added(*owner_, nb);
    clear_measure_layout_copies(ob);
    nb->set_prev(ob->prev());
    nb->set_next(ob->next());
    if (ob->prev()) {
        ob->prev()->set_next(nb);
    }
    if (ob->next()) {
        ob->next()->set_prev(nb);
    }
    if (ob == last_) {
        last_ = nb;
    }
    if (ob == first_) {
        first_ = nb;
    }
    if (nb->is_box()) {
        nb->set_system(ob->system());
    }
    for (EngravingItem *e : nb->el()) {
        e->set_ownership_parent(nb);
    }
}

//---------------------------------------------------------
//   append
//    append measure to the end of the list and update
//    tick index
//---------------------------------------------------------

void MeasureBaseList::append(MeasureBase *m) {
    assert(!m->next());
    assert(!m->prev() || m->prev() == last_);

    push_back(m);
    tick_index_.emplace(std::make_pair(m->tick().ticks(), m));
}

Measure *MeasureBaseList::measure_by_tick(int tick) const {
    if (empty() || tick > last_->end_tick().ticks()) {
        return nullptr;
    }

    auto it = tick_index_.upper_bound(tick);

    if (it == tick_index_.begin()) {
        MeasureBase *mb = it->second;

        if (mb->is_measure()) {
            return to_measure(mb);
        }
        return nullptr;
    }

    --it;
    for (;; --it) {
        if (it == tick_index_.begin()) {
            MeasureBase *mb = it->second;

            if (mb->is_measure()) {
                return to_measure(mb);
            }
            return nullptr;
        }

        MeasureBase *mb = it->second;
        if (!mb) {
            break;
        }

        if (mb->is_measure()) {
            return to_measure(mb);
        }
    }

    return nullptr;
}

MeasureBase *MeasureBaseList::first_measure_base_at_tick(int tick) const {
    if (empty() || tick > last_->end_tick().ticks()) {
        return nullptr;
    }

    auto it = tick_index_.upper_bound(tick);

    if (it != tick_index_.begin()) {
        --it;
    }
    return it->second;
}

std::vector<MeasureBase *> MeasureBaseList::measure_bases_at_tick(int tick) const {
    std::vector<MeasureBase *> result;
    if (empty() || tick > last_->end_tick().ticks()) {
        return result;
    }

    result = muse::values(tick_index_, tick);

    return result;
}

void MeasureBaseList::update_tick_index() {
    tick_index_.clear();

    for (MeasureBase *mb = first_; mb; mb = mb->next()) {
        tick_index_.emplace(std::make_pair(mb->tick().ticks(), mb));
    }
}
