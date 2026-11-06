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

#include "transaction/transaction.h"
#include "transaction/undoablecommand.h"

using namespace mu::engraving;

namespace {
//-------------------------------------------------------------------
// Undo commands
//-------------------------------------------------------------------

struct ConnectSharedPart : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, ConnectSharedPart)

    SharedPart *shared_part = nullptr;
    Part *origin_part = nullptr;

public:
    ConnectSharedPart(SharedPart *s, Part *o) : shared_part(s), origin_part(o) {}

    void undo() override { shared_part->remove_origin_part(origin_part); }

    void redo() override { shared_part->add_origin_part(origin_part); }

    UNDO_TYPE(CommandType::CONNECT_SHARED_PART)
    UNDO_NAME("Connect shared part")
};

struct DisconnectSharedPart : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, DisconnectSharedPart)

    SharedPart *shared_part = nullptr;
    Part *origin_part = nullptr;

public:
    DisconnectSharedPart(SharedPart *s, Part *o) : shared_part(s), origin_part(o) {}

    void undo() override { shared_part->add_origin_part(origin_part); }

    void redo() override { shared_part->remove_origin_part(origin_part); }

    UNDO_TYPE(CommandType::DISCONNECT_SHARED_PART)
    UNDO_NAME("Disconnect shared part")
};
} // namespace

//-------------------------------------------------------------------
// EditStaveSharing
//-------------------------------------------------------------------

void EditStaveSharing::toggle_stave_sharing(Transaction &tx, Score *score, bool on) {
    score->undo_change_style_val(Sid::ENABLE_STAVE_SHARING, on);

    if (on && score->shared_parts().empty()) {
        cmd_create_shared_staves(tx, score);
    }

    score->set_brackets_and_barlines();

    score->update();
}

void EditStaveSharing::cmd_remove_shared_staves(Score *score) {
    std::vector<Part *> parts = score->parts(); // COPY because we're about to remove elements

    for (Part *part : parts) {
        if (part->is_shared_part()) {
            score->cmd_remove_part(part);
        }
    }
}

void EditStaveSharing::cmd_create_shared_staves(Transaction &tx, Score *score) {
    StaveSharingGroups stave_sharing_groups = compute_groups(score);

    create_shared_parts(tx, stave_sharing_groups, score);
}

StaveSharingGroups EditStaveSharing::compute_groups(Score *score) {
    const std::vector<Part *> &parts = score->parts();

    StaveSharingGroups stave_sharing_groups;

    auto exclude_part = [](Part *part) {
        if (part->nstaves() > 1 || part->is_shared_part()) {
            return true;
        }

        Staff *first_staff = part->staves().front();
        const StaffType *staff_type = first_staff->staff_type();
        if (staff_type->group() != StaffGroup::STANDARD) {
            return true;
        }

        return false;
    };

    for (size_t part_idx = 0; part_idx < parts.size();) {
        Part *part = parts[part_idx];
        if (exclude_part(part)) {
            ++part_idx;
            continue;
        }

        Instrument *instr = part->instrument();
        String instr_group = instr->group();
        if (instr_group != "woodwinds" && instr_group != "brass") { // TODO: styles
            ++part_idx;
            continue;
        }

        StaveSharingGroup stave_sharing_group;

        size_t next_part_idx = part_idx;
        while (next_part_idx < parts.size()) {
            Part *next_part = parts[next_part_idx];
            if (exclude_part(next_part)) {
                break;
            }
            Instrument *next_instr = next_part->instrument();
            if (next_part != part && next_instr->id() != instr->id()) {
                break;
            }

            stave_sharing_group.push_back(next_part);
            ++next_part_idx;
        }

        if (stave_sharing_group.size() > 1) {
            stave_sharing_groups.push_back(stave_sharing_group);
        }

        part_idx = next_part_idx;
    }

    return stave_sharing_groups;
}

void EditStaveSharing::create_shared_parts(Transaction &tx, const StaveSharingGroups &groups, Score *score) {
    for (const StaveSharingGroup &group : groups) {
        SharedPart *shared_part = nullptr;
        for (Part *part : group) {
            if (part->shared_part() && !shared_part) {
                shared_part = part->shared_part();
                break;
            }
        }

        Part *first_part = group.front();
        const Instrument *instr = first_part->instrument();

        if (!shared_part) {
            size_t idx = muse::index_of(score->parts(), first_part);
            shared_part = create_shared_part(score, idx, instr);
        }

        for (Part *origin_part : group) {
            SharedPart *existing_shared_part = origin_part->shared_part();
            if (existing_shared_part && existing_shared_part != shared_part) {
                disconnect_shared_part(tx, existing_shared_part, origin_part);
                connect_shared_part(tx, shared_part, origin_part);
            } else if (!existing_shared_part) {
                connect_shared_part(tx, shared_part, origin_part);
            }
        }
    }
}

SharedPart *EditStaveSharing::create_shared_part(Score *score, size_t idx, const Instrument *instr) {
    SharedPart *shared_part = new SharedPart(score);
    score->undo_insert_part(shared_part, idx);

    Instrument *instrument = new Instrument(*instr);
    InstrumentLabel &label = instrument->instrument_label();
    label.set_allow_group_name(false);
    label.set_number(0);
    label.set_show_number_long(false);
    label.set_show_number_short(false);

    shared_part->set_instrument(instrument);

    return shared_part;
}

void EditStaveSharing::add_staff_to_shared_part(SharedPart *shared_part, const Part *origin_part, const KeyList &key_list,
                                                const StaffType *staff_type) {
    Staff *staff = Factory::create_staff(shared_part);
    staff->set_staff_type(Fraction(0, 1), *staff_type);
    staff->set_part(shared_part);

    Score *score = shared_part->score();
    staff->set_score(score);

    staff_idx_t rel_staff_idx = shared_part->nstaves();
    staff->set_default_clef_type(origin_part->instrument()->clef_type(0));

    score->undo_insert_staff(staff, rel_staff_idx);

    staff_idx_t abs_staff_idx = track2staff(shared_part->track_range().start_track) + rel_staff_idx;
    score->adjust_key_sigs(abs_staff_idx, abs_staff_idx + 1, key_list);
}

void EditStaveSharing::connect_shared_part(Transaction &tx, SharedPart *shared_part, Part *origin_part) {
    Score *score = origin_part->score();
    tx.push(new ConnectSharedPart(shared_part, origin_part));
    add_staff_to_shared_part(shared_part, origin_part, score->key_list(), origin_part->staff(0)->staff_type());
}

void EditStaveSharing::disconnect_shared_part(Transaction &tx, SharedPart *shared_part, Part *origin_part) {
    Score *score = origin_part->score();
    tx.push(new DisconnectSharedPart(shared_part, origin_part));
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
            tx.push(new DisconnectSharedPart(shared_part, origin_part));
        }
    }
}
