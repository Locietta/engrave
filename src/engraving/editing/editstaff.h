/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "../dom/measure.h"
#include "../dom/score.h"
#include "../dom/staff.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {

struct InsertMStaff : UndoableCommand {
private:

    Measure *measure = nullptr;
    MStaff *mstaff = nullptr;
    staff_idx_t idx = muse::nidx;

public:
    InsertMStaff(Measure *, MStaff *, staff_idx_t);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::INSERT_M_STAFF)
    UNDO_NAME("InsertMStaff")
    UNDO_CHANGED_OBJECTS({measure})
};

struct RemoveMStaff : UndoableCommand {
private:

    Measure *measure = nullptr;
    MStaff *mstaff = nullptr;
    int idx = 0;

public:
    RemoveMStaff(Measure *, MStaff *, int);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::REMOVE_M_STAFF)
    UNDO_NAME("RemoveMStaff")
    UNDO_CHANGED_OBJECTS({measure})
};

struct InsertStaves : UndoableCommand {
private:

    Measure *measure = nullptr;
    staff_idx_t a = muse::nidx;
    staff_idx_t b = muse::nidx;

public:
    InsertStaves(Measure *, staff_idx_t, staff_idx_t);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::INSERT_STAVES)
    UNDO_NAME("InsertStaves")
    UNDO_CHANGED_OBJECTS({measure})
};

struct RemoveStaves : UndoableCommand {
private:

    Measure *measure = nullptr;
    staff_idx_t a = muse::nidx;
    staff_idx_t b = muse::nidx;

public:
    RemoveStaves(Measure *, staff_idx_t, staff_idx_t);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::REMOVE_STAVES)
    UNDO_NAME("RemoveStaves")
    UNDO_CHANGED_OBJECTS({measure})
};

} // namespace mu::engraving
