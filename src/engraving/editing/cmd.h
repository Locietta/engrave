/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include <vector>

#include "../dom/engravingobject.h"
#include "../types/types.h"

namespace mu::engraving {
//---------------------------------------------------------
//   UpdateState
//---------------------------------------------------------

struct UpdateState {
    RectF refresh;           ///< area to update, canvas coordinates
    bool play_note = false;  ///< play selected note after command
    bool play_chord = false; ///< play whole chord for the selected note
    bool selection_changed = false;
};

//---------------------------------------------------------
//   CmdState
//
//    the following variables are reset on startCmd()
//    modified during cmd processing and used in endCmd() to
//    determine what to layout and what to repaint:
//---------------------------------------------------------

struct CmdState {
    LayoutFlags layout_flags;

    bool excerpts_changed = false;
    bool instruments_changed = false;

    void reset();

    UpdateMode update_mode() const { return update_mode_; }

    void set_update_mode(UpdateMode m);
    void assign_update_mode(UpdateMode m);

    bool layout_range() const { return update_mode_ == UpdateMode::LAYOUT; }

    bool update_all() const { return int(update_mode_) >= int(UpdateMode::UPDATE_ALL); }

    bool update_range() const { return update_mode_ == UpdateMode::UPDATE; }

    void set_tick(const Fraction &t);
    void set_staff(staff_idx_t staff);
    void set_element(const EngravingItem *e);
    void unset_element(const EngravingItem *e);

    Fraction start_tick() const { return start_tick_; }

    Fraction end_tick() const { return end_tick_; }

    staff_idx_t start_staff() const { return start_staff_; }

    staff_idx_t end_staff() const { return end_staff_; }

    const EngravingItem *element() const;

    void delete_later(EngravingObject *e);
    std::vector<EngravingObject *> take_postponed_deletions();

    void lock() { locked_ = true; }

    void unlock() { locked_ = false; }
#ifndef NDEBUG
    void dump();
#endif

private:
    void set_measure_base(const MeasureBase *mb);

    UpdateMode update_mode_ = UpdateMode::DO_NOTHING;
    Fraction start_tick_{-1, 1}; // start tick for mode LayoutTick
    Fraction end_tick_{-1, 1};   // end tick for mode LayoutTick
    staff_idx_t start_staff_ = muse::nidx;
    staff_idx_t end_staff_ = muse::nidx;
    const EngravingItem *el_ = nullptr;
    const MeasureBase *mb_ = nullptr;
    bool one_element_ = true;
    bool one_measure_base_ = true;
    std::vector<EngravingObject *> postponed_deletions_;

    bool locked_ = false;
};
} // namespace mu::engraving
