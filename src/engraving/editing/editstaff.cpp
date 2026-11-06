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

#include "editstaff.h"

#include "../dom/guitarbend.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/score.h"
#include "../dom/staff.h"
#include "../dom/stafflines.h"
#include "../dom/stafftype.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   InsertStaff
//---------------------------------------------------------

InsertStaff::InsertStaff(Staff *p, staff_idx_t input_ridx) {
    staff = p;
    ridx = input_ridx;
}

void InsertStaff::undo() { staff->score()->remove_staff(staff); }

void InsertStaff::redo() { staff->score()->insert_staff(staff, ridx); }

void InsertStaff::cleanup(bool was_done) {
    if (!was_done) {
        delete staff;
        staff = nullptr;
    }
}

//---------------------------------------------------------
//   RemoveStaff
//---------------------------------------------------------

RemoveStaff::RemoveStaff(Staff *p) {
    staff = p;
    ridx = staff->rstaff();
    was_system_object_staff = staff->is_system_object_staff();
}

void RemoveStaff::undo() {
    staff->score()->insert_staff(staff, ridx);
    if (was_system_object_staff) {
        staff->score()->add_system_object_staff(staff);
    }
}

void RemoveStaff::redo() {
    staff->score()->remove_staff(staff);
    if (was_system_object_staff) {
        staff->score()->remove_system_object_staff(staff);
    }
}

void RemoveStaff::cleanup(bool was_done) {
    if (was_done) {
        delete staff;
        staff = nullptr;
    }
}

//---------------------------------------------------------
//   AddSystemObjectStaff
//---------------------------------------------------------

AddSystemObjectStaff::AddSystemObjectStaff(Staff *s) : staff(s) {}

void AddSystemObjectStaff::undo() { staff->score()->remove_system_object_staff(staff); }

void AddSystemObjectStaff::redo() { staff->score()->add_system_object_staff(staff); }

//---------------------------------------------------------
//   RemoveSystemObjectStaff
//---------------------------------------------------------

RemoveSystemObjectStaff::RemoveSystemObjectStaff(Staff *s) : staff(s) {}

void RemoveSystemObjectStaff::undo() { staff->score()->add_system_object_staff(staff); }

void RemoveSystemObjectStaff::redo() { staff->score()->remove_system_object_staff(staff); }

//---------------------------------------------------------
//   InsertMStaff
//---------------------------------------------------------

InsertMStaff::InsertMStaff(Measure *m, MStaff *ms, staff_idx_t i) {
    measure = m;
    mstaff = ms;
    idx = i;
}

void InsertMStaff::undo() { measure->remove_m_staff(mstaff, idx); }

void InsertMStaff::redo() { measure->insert_m_staff(mstaff, idx); }

//---------------------------------------------------------
//   RemoveMStaff
//---------------------------------------------------------

RemoveMStaff::RemoveMStaff(Measure *m, MStaff *ms, int i) {
    measure = m;
    mstaff = ms;
    idx = i;
}

void RemoveMStaff::undo() { measure->insert_m_staff(mstaff, idx); }

void RemoveMStaff::redo() { measure->remove_m_staff(mstaff, idx); }

//---------------------------------------------------------
//   InsertStaves
//---------------------------------------------------------

InsertStaves::InsertStaves(Measure *m, staff_idx_t input_a, staff_idx_t input_b) {
    measure = m;
    a = input_a;
    b = input_b;
}

void InsertStaves::undo() { measure->remove_staves(a, b); }

void InsertStaves::redo() { measure->insert_staves(a, b); }

//---------------------------------------------------------
//   RemoveStaves
//---------------------------------------------------------

RemoveStaves::RemoveStaves(Measure *m, staff_idx_t input_a, staff_idx_t input_b) {
    measure = m;
    a = input_a;
    b = input_b;
}

void RemoveStaves::undo() { measure->insert_staves(a, b); }

void RemoveStaves::redo() { measure->remove_staves(a, b); }

//---------------------------------------------------------
//   SortStaves
//---------------------------------------------------------

SortStaves::SortStaves(Score *s, const std::vector<staff_idx_t> &l) {
    score = s;

    for (staff_idx_t i = 0; i < l.size(); i++) {
        rlist.push_back(muse::index_of(l, i));
    }
    list = l;
}

void SortStaves::redo() { score->sort_staves(list); }

void SortStaves::undo() { score->sort_staves(rlist); }

//---------------------------------------------------------
//   ChangeStaff
//---------------------------------------------------------

ChangeStaff::ChangeStaff(Staff *input_staff) : staff(input_staff) {
    visible = staff->visible();
    clef_type_value = staff->default_clef_type();
    user_dist = staff->user_dist();
    cutaway = staff->cutaway();
    hide_system_bar_line_value = staff->hide_system_bar_line();
    merge_matching_rests = staff->merge_matching_rests();
    reflect_transposition_in_linked_tab_value = staff->reflect_transposition_in_linked_tab();
}

ChangeStaff::ChangeStaff(Staff *input_staff, bool input_visible, ClefTypeList clef_type, Spatium user_dist, bool input_cutaway,
                         bool hide_system_bar_line, AutoOnOff merge_matching_rests, bool reflect_transposition_in_linked_tab) {
    staff = input_staff;
    visible = input_visible;
    clef_type_value = clef_type;
    this->user_dist = user_dist;
    cutaway = input_cutaway;
    hide_system_bar_line_value = hide_system_bar_line;
    this->merge_matching_rests = merge_matching_rests;
    reflect_transposition_in_linked_tab_value = reflect_transposition_in_linked_tab;
}

//---------------------------------------------------------
//   flip
//---------------------------------------------------------

void ChangeStaff::flip() {
    bool old_visible = staff->visible();
    ClefTypeList old_clef_type = staff->default_clef_type();
    Spatium old_user_dist = staff->user_dist();
    bool old_cutaway = staff->cutaway();
    bool old_hide_system_bar_line = staff->hide_system_bar_line();
    AutoOnOff old_merge_matching_rests = staff->merge_matching_rests();
    bool old_reflect_transposition_in_linked_tab = staff->reflect_transposition_in_linked_tab();

    staff->set_visible(visible);
    staff->set_default_clef_type(clef_type_value);
    staff->set_user_dist(user_dist);
    staff->set_cutaway(cutaway);
    staff->set_hide_system_bar_line(hide_system_bar_line_value);
    staff->set_merge_matching_rests(merge_matching_rests);
    staff->set_reflect_transposition_in_linked_tab(reflect_transposition_in_linked_tab_value);

    visible = old_visible;
    clef_type_value = old_clef_type;
    user_dist = old_user_dist;
    cutaway = old_cutaway;
    hide_system_bar_line_value = old_hide_system_bar_line;
    merge_matching_rests = old_merge_matching_rests;
    reflect_transposition_in_linked_tab_value = old_reflect_transposition_in_linked_tab;

    staff->trigger_layout();
    staff->master_score()->rebuild_midi_mapping();
}

//---------------------------------------------------------
//   ChangeStaffType::flip
//---------------------------------------------------------

void ChangeStaffType::flip() {
    StaffType old_staff_type = *staff->staff_type(tick);

    staff->set_staff_type(tick, staff_type);

    bool invisible_changed = old_staff_type.invisible() != staff_type.invisible();
    bool from_tab_to_standard = old_staff_type.is_tab_staff() && !staff_type.is_tab_staff();

    staff_type = old_staff_type;

    Score *score = staff->score();
    if (invisible_changed) {
        staff_idx_t staff_idx = staff->idx();
        for (Measure *m = score->first_measure(); m; m = m->next_measure()) {
            m->staff_lines(staff_idx)->set_visible(!staff->is_lines_invisible(tick));
        }
    }

    if (from_tab_to_standard) {
        GuitarBend::adapt_bends_from_tab_to_standard_staff(staff);
    }

    staff->trigger_layout();
}

//---------------------------------------------------------
//   ChangeMStaffProperties
//---------------------------------------------------------

ChangeMStaffProperties::ChangeMStaffProperties(Measure *m, staff_idx_t i, bool v, bool s)
    : measure(m), staff_idx(i), visible(v), stemless(s) {}

void ChangeMStaffProperties::flip() {
    bool v = measure->visible(staff_idx);
    bool s = measure->stemless(staff_idx);
    measure->set_staff_visible(staff_idx, visible);
    measure->set_staff_stemless(staff_idx, stemless);
    visible = v;
    stemless = s;
}

//---------------------------------------------------------
//   ChangeMStaffHideIfEmpty
//---------------------------------------------------------

ChangeMStaffHideIfEmpty::ChangeMStaffHideIfEmpty(Measure *m, staff_idx_t i, AutoOnOff h) : measure(m), staff_idx(i), hide_if_empty(h) {}

void ChangeMStaffHideIfEmpty::flip() {
    AutoOnOff h = measure->hide_staff_if_empty(staff_idx);
    measure->set_hide_staff_if_empty(staff_idx, hide_if_empty);
    measure->trigger_layout(staff_idx);
    hide_if_empty = h;
}
