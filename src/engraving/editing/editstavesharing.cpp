/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "editstavesharing.h"

#include "dom/factory.h"
#include "dom/instrtemplate.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/sharedpart.h"
#include "dom/staff.h"
#include "engraving/editing/cmd.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

void EditStaveSharing::disconnect_shared_part(Transaction &tx, SharedPart *shared_part, Part *origin_part) {
    Score *score = origin_part->score();
    tx.change_shared_origin(shared_part, origin_part, false);
    score->undo_remove_staff(shared_part->staves().back());
}

void EditStaveSharing::handle_remove_part(Transaction &tx, Part *part) {
    Score *score = part->score();

    if (SharedPart *shared_part = part->shared_part()) {
        disconnect_shared_part(tx, shared_part, part);
        if (shared_part->origin_parts().empty()) {
            score->cmd_remove_part(shared_part);
        }
    } else if (part->is_shared_part()) {
        shared_part = to_shared_part(part);
        std::vector<Part *> origin_parts = shared_part->origin_parts();
        for (Part *origin_part : origin_parts) {
            tx.change_shared_origin(shared_part, origin_part, false);
        }
    }
}
