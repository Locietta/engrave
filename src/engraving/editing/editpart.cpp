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

#include "editpart.h"

#include "../dom/excerpt.h"
#include "../dom/factory.h"
#include "../dom/instrchange.h"
#include "../dom/instrtemplate.h"
#include "../dom/masterscore.h"
#include "../dom/mscore.h"
#include "../dom/part.h"
#include "../dom/score.h"
#include "../dom/scoreorder.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "../dom/stafftype.h"
#include "../dom/stringtunings.h"
#include "../dom/utils.h"
#include "log.h"
#include "transaction/transaction.h"
#include "transpose.h"

using namespace mu::engraving;

void EditPart::remove_parts(Score *score, const std::vector<Part *> &parts) {
    if (!score || parts.empty()) {
        return;
    }

    for (Part *part : parts) {
        score->cmd_remove_part(part);
    }

    score->set_brackets_and_barlines();
}

void EditPart::remove_staves(Score *score, const std::vector<Staff *> &staves) {
    if (!score || staves.empty()) {
        return;
    }

    for (Staff *staff : staves) {
        score->cmd_remove_staff(staff->idx());
    }

    score->set_brackets_and_barlines();
}

void EditPart::do_append_staff(Score *score, Staff *staff, Part *destination_part, bool create_rests) {
    if (!score || !staff || !destination_part) {
        return;
    }

    staff_idx_t staff_local_index = destination_part->nstaves();
    KeyList key_list = *destination_part->staff(staff_local_index - 1)->key_list();

    staff->set_score(score);
    staff->set_part(destination_part);

    score->undo_insert_staff(staff, staff_local_index, create_rests);

    staff_idx_t staff_global_index = staff->idx();
    score->adjust_key_sigs(staff_global_index, staff_global_index + 1, key_list);

    score->update_braces_and_barlines(destination_part, staff_local_index);

    destination_part->instrument()->set_clef_type(staff_local_index, staff->default_clef_type());
}

Staff *EditPart::append_staff(Score *score, Part *destination_part) {
    if (!score || !destination_part) {
        return nullptr;
    }

    Staff *staff = Factory::create_staff(destination_part);
    do_append_staff(score, staff, destination_part);
    return staff;
}
