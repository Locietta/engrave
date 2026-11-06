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

#ifndef MU_ENGRAVING_MEASUREBASE_H
#define MU_ENGRAVING_MEASUREBASE_H

/**
 \file
 Definition of MeasureBase class.
*/

#include "engravingitem.h"

namespace mu::engraving {
struct Box;
struct LayoutBreak;
struct Measure;
struct Score;
struct System;
struct RangeLock;

//---------------------------------------------------------
//   Repeat
//---------------------------------------------------------

enum class Repeat : char { NONE = 0, END = 1, START = 2, MEASURE = 4, JUMP = 8 };

constexpr Repeat operator|(Repeat t1, Repeat t2) { return static_cast<Repeat>(static_cast<int>(t1) | static_cast<int>(t2)); }

constexpr bool operator&(Repeat t1, Repeat t2) { return static_cast<int>(t1) & static_cast<int>(t2); }

//---------------------------------------------------------
//   @@ MeasureBase
///    Virtual base class for Measure, HBox and VBox
//
//   @P lineBreak       bool        true if a system break is positioned on this measure
//   @P nextMeasure     Measure     the next Measure (read-only)
//   @P nextMeasureMM   Measure     the next multi-measure rest Measure (read-only)
//   @P pageBreak       bool        true if a page break is positioned on this measure
//   @P prevMeasure     Measure     the previous Measure (read-only)
//   @P prevMeasureMM   Measure     the previous multi-measure rest Measure (read-only)
//---------------------------------------------------------

struct MeasureBase : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, MeasureBase)

public:

    ~MeasureBase();

    //! The system this measure is currently laid out on; null if not placed.
    System *system() const { return system_; }

    void set_system(System *s) { system_ = s; }

    EngravingItem *layout_parent() const override;

    //! A measure or frame is owned by its score, or by the frame it is nested in.
    //! A system merely places it; see setSystem(). These overloads hide
    //! EngravingItem::setOwnershipParent, so that no other parent can be set by accident.
    void set_ownership_parent(Score *score);
    void set_ownership_parent(Box *box);

    System *prev_non_v_box_system() const;
    System *next_non_v_box_system() const;
    Page *page() const;
    Page *prev_page() const;
    Page *next_page() const;

    virtual void scan_elements(std::function<void(EngravingItem *)> func) override;

    virtual void set_score(Score *s) override;

    MeasureBase *next() const { return next_; }

    MeasureBase *next_mm() const;

    void set_next(MeasureBase *e) { next_ = e; }

    MeasureBase *prev() const { return prev_; }

    MeasureBase *prev_mm() const;

    void set_prev(MeasureBase *e) { prev_ = e; }

    MeasureBase *top() const;

    MeasureBase *get_in_score(Score *score, bool use_next_measure_fallback = false) const;

    Measure *next_measure() const;
    Measure *prev_measure() const;
    Measure *next_measure_mm() const;
    Measure *prev_measure_mm() const;

    ElementList &el() { return el_; }

    const ElementList &el() const { return el_; }

    const MeasureBase *mb_with_preceding_section_break() const;
    LayoutBreak *section_break_element() const;

    void undo_set_break(bool v, LayoutBreakType type);

    void undo_set_line_break(bool v) { undo_set_break(v, LayoutBreakType::LINE); }

    void undo_set_page_break(bool v) { undo_set_break(v, LayoutBreakType::PAGE); }

    void undo_set_section_break(bool v) { undo_set_break(v, LayoutBreakType::SECTION); }

    void undo_set_no_break(bool v) { undo_set_break(v, LayoutBreakType::NOBREAK); }

    void cleanup_layout_breaks(bool undo);

    virtual void move_ticks(const Fraction &diff) { set_tick(tick() + diff); }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    Fraction tick() const override;
    void set_tick(const Fraction &f);

    Fraction ticks() const { return len_; }

    void set_ticks(const Fraction &f) { len_ = f; }

    Fraction end_tick() const { return tick_ + len_; }

    void trigger_layout() const override;

    double pause() const;

    void clear_elements();
    ElementList take_elements();

    bool line_break() const { return flag(ElementFlag::LINE_BREAK); }

    void set_line_break(bool v) { set_flag(ElementFlag::LINE_BREAK, v); }

    bool page_break() const { return flag(ElementFlag::PAGE_BREAK); }

    void set_page_break(bool v) { set_flag(ElementFlag::PAGE_BREAK, v); }

    bool section_break() const { return flag(ElementFlag::SECTION_BREAK); }

    void set_section_break(bool v) { set_flag(ElementFlag::SECTION_BREAK, v); }

    bool no_break() const { return flag(ElementFlag::NO_BREAK); }

    void set_no_break(bool v) { set_flag(ElementFlag::NO_BREAK, v); }

    virtual void compute_min_width() {}

    int index() const;
    int measure_index() const;

    bool is_before(const EngravingItem *other) const override;
    bool is_before(const MeasureBase *other) const;

    bool is_before_or_equal(const MeasureBase *other) const { return other == this || is_before(other); }

    bool is_after(const MeasureBase *other) const { return !is_before_or_equal(other); }

    bool is_after_or_equal(const MeasureBase *other) const { return !is_before(other); }

    const RangeLock *system_lock() const;
    bool is_start_of_system_lock() const;
    bool is_end_of_system_lock() const;

    const RangeLock *page_lock() const;
    bool is_start_of_page_lock() const;
    bool is_end_of_page_lock() const;

protected:
    MeasureBase(const ElementType &type, Score *parent);
    MeasureBase(const MeasureBase &);

    Fraction len_{Fraction(0, 1)}; // actual length of measure

    ElementList el_; // Measure(/tick) relative -elements: with defined start time
                     // but outside the staff
private:
    MeasureBase *next_ = nullptr;
    MeasureBase *prev_ = nullptr;

    System *system_ = nullptr; // current layout placement; not owned, not copied

    Fraction tick_ = Fraction(0, 1);
};

//---------------------------------------------------------
//   MeasureBaseList
//---------------------------------------------------------

struct MeasureBaseList {
    MeasureBaseList();

    MeasureBase *first() const { return first_; }

    MeasureBase *last() const { return last_; }

    void clear();
    void add(MeasureBase *);
    void remove(MeasureBase *);
    void insert(MeasureBase *, MeasureBase *);
    void remove(MeasureBase *, MeasureBase *);
    void change(MeasureBase *o, MeasureBase *n);

    int size() const { return size_; }

    bool empty() const { return size_ == 0; }

    void append(MeasureBase *);

    void update_tick_index();

    Measure *measure_by_tick(int tick) const;
    MeasureBase *first_measure_base_at_tick(int tick) const;
    std::vector<MeasureBase *> measure_bases_at_tick(int tick) const;

private:
    void push_back(MeasureBase *m);
    void push_front(MeasureBase *m);

    int size_ = 0;
    MeasureBase *first_ = nullptr;
    MeasureBase *last_ = nullptr;

    // At a tick there can be any number of MeasureBases
    // There can only be one Measure
    // There can be any number of Boxes
    std::multimap<int, MeasureBase *> tick_index_;
};
} // namespace mu::engraving
#endif
