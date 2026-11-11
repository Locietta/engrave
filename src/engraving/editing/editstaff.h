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
struct InsertStaff : UndoableCommand {
private:

    Staff *staff = nullptr;
    staff_idx_t ridx = muse::nidx;

public:
    InsertStaff(Staff *, staff_idx_t idx);
    void undo() override;
    void redo() override;
    void cleanup(bool) override;

    UNDO_TYPE(CommandType::INSERT_STAFF)
    UNDO_NAME("InsertStaff")
    UNDO_CHANGED_OBJECTS({staff})
};

struct RemoveStaff : UndoableCommand {
private:

    Staff *staff = nullptr;
    staff_idx_t ridx = muse::nidx;
    bool was_system_object_staff = false;

public:
    RemoveStaff(Staff *);
    void undo() override;
    void redo() override;
    void cleanup(bool) override;

    UNDO_TYPE(CommandType::REMOVE_STAFF)
    UNDO_NAME("RemoveStaff")
    UNDO_CHANGED_OBJECTS({staff})
};

struct AddSystemObjectStaff : UndoableCommand {
private:

    Staff *staff = nullptr;

public:
    AddSystemObjectStaff(Staff *);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::ADD_SYSTEM_OBJECT_STAFF)
    UNDO_NAME("AddSystemObjectStaff")
    UNDO_CHANGED_OBJECTS({staff})
};

struct RemoveSystemObjectStaff : UndoableCommand {
private:

    Staff *staff = nullptr;

public:
    RemoveSystemObjectStaff(Staff *);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::REMOVE_SYSTEM_OBJECT_STAFF)
    UNDO_NAME("RemoveSystemObjectStaff")
    UNDO_CHANGED_OBJECTS({staff})
};

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

struct SortStaves : UndoableCommand {
private:

    Score *score = nullptr;
    std::vector<staff_idx_t> list;
    std::vector<staff_idx_t> rlist;

public:
    SortStaves(Score *, const std::vector<staff_idx_t> &);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::SORT_STAVES)
    UNDO_NAME("SortStaves")
    UNDO_CHANGED_OBJECTS({score})
};

//---------------------------------------------------------
//   ChangeStaff
//---------------------------------------------------------

struct ChangeStaff : UndoableCommand {
private:

    Staff *staff = nullptr;

    bool visible = false;
    ClefTypeList clef_type_value;
    Spatium user_dist = 0.0_sp;
    bool cutaway = false;
    bool hide_system_bar_line_value = false;
    AutoOnOff merge_matching_rests = AutoOnOff::AUTO;
    bool reflect_transposition_in_linked_tab_value = false;

    void flip() override;

public:
    ChangeStaff(Staff *);

    ChangeStaff(Staff *, bool input_visible, ClefTypeList input_clef_type, Spatium user_dist, bool input_cutaway,
                bool input_hide_system_bar_line, AutoOnOff merge_rests, bool input_reflect_transposition_in_linked_tab);

    UNDO_TYPE(CommandType::CHANGE_STAFF)
    UNDO_NAME("ChangeStaff")
    UNDO_CHANGED_OBJECTS({staff})
};

//---------------------------------------------------------
//   ChangeStaffType
//---------------------------------------------------------

struct ChangeStaffType : UndoableCommand {
private:

    Staff *staff = nullptr;
    StaffType staff_type;
    Fraction tick;

    void flip() override;

public:
    ChangeStaffType(Staff *s, const StaffType &t, Fraction tick = Fraction(0, 1)) : staff(s), staff_type(t), tick(tick) {}

    UNDO_TYPE(CommandType::CHANGE_STAFF_TYPE)
    UNDO_NAME("ChangeStaffType")
    UNDO_CHANGED_OBJECTS({staff})
};

struct ChangeMStaffProperties : UndoableCommand {
private:

    Measure *measure = nullptr;
    staff_idx_t staff_idx = 0;
    bool visible = false;
    bool stemless = false;

    void flip() override;

public:
    ChangeMStaffProperties(Measure *, staff_idx_t staff_idx, bool visible, bool stemless);

    UNDO_TYPE(CommandType::CHANGE_M_STAFF_PROPERTIES)
    UNDO_NAME("ChangeMStaffProperties")
    UNDO_CHANGED_OBJECTS({measure})
};

struct ChangeMStaffHideIfEmpty : UndoableCommand {
private:

    Measure *measure = nullptr;
    staff_idx_t staff_idx = 0;
    AutoOnOff hide_if_empty = AutoOnOff::AUTO;

    void flip() override;

public:
    ChangeMStaffHideIfEmpty(Measure *, staff_idx_t staff_idx, AutoOnOff hide_if_empty);

    UNDO_TYPE(CommandType::CHANGE_M_STAFF_HIDE_IF_EMPTY)
    UNDO_NAME("ChangeMStaffHideIfEmpty")
    UNDO_CHANGED_OBJECTS({measure})
};
} // namespace mu::engraving
