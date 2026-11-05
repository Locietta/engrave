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
    RectF refresh;          ///< area to update, canvas coordinates
    bool playNote = false;  ///< play selected note after command
    bool playChord = false; ///< play whole chord for the selected note
    bool selectionChanged = false;
};

//---------------------------------------------------------
//   CmdState
//
//    the following variables are reset on startCmd()
//    modified during cmd processing and used in endCmd() to
//    determine what to layout and what to repaint:
//---------------------------------------------------------

class CmdState {
public:
    LayoutFlags layoutFlags;

    bool excerptsChanged = false;
    bool instrumentsChanged = false;

    void reset();

    UpdateMode updateMode() const { return updateMode_; }

    void setUpdateMode(UpdateMode m);
    void _setUpdateMode(UpdateMode m);

    bool layoutRange() const { return updateMode_ == UpdateMode::Layout; }

    bool updateAll() const { return int(updateMode_) >= int(UpdateMode::UpdateAll); }

    bool updateRange() const { return updateMode_ == UpdateMode::Update; }

    void setTick(const Fraction &t);
    void setStaff(staff_idx_t staff);
    void setElement(const EngravingItem *e);
    void unsetElement(const EngravingItem *e);

    Fraction startTick() const { return startTick_; }

    Fraction endTick() const { return endTick_; }

    staff_idx_t startStaff() const { return startStaff_; }

    staff_idx_t endStaff() const { return endStaff_; }

    const EngravingItem *element() const;

    void deleteLater(EngravingObject *e);
    std::vector<EngravingObject *> takePostponedDeletions();

    void lock() { locked_ = true; }

    void unlock() { locked_ = false; }
#ifndef NDEBUG
    void dump();
#endif

private:
    void setMeasureBase(const MeasureBase *mb);

    UpdateMode updateMode_ = UpdateMode::DoNothing;
    Fraction startTick_{-1, 1}; // start tick for mode LayoutTick
    Fraction endTick_{-1, 1};   // end tick for mode LayoutTick
    staff_idx_t startStaff_ = muse::nidx;
    staff_idx_t endStaff_ = muse::nidx;
    const EngravingItem *el_ = nullptr;
    const MeasureBase *mb_ = nullptr;
    bool oneElement_ = true;
    bool oneMeasureBase_ = true;
    std::vector<EngravingObject *> postponedDeletions_;

    bool locked_ = false;
};
} // namespace mu::engraving
