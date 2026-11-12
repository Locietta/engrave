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

#include "../dom/measure.h"

using namespace mu::engraving;

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
