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
#pragma once

#include <vector>

namespace mu::engraving {
struct Instrument;
struct KeyList;
struct Part;
struct Score;
struct SharedPart;
struct StaffType;
struct Transaction;

using StaveSharingGroup = std::vector<Part *>;
using StaveSharingGroups = std::vector<StaveSharingGroup>;

struct EditStaveSharing {
    static void toggle_stave_sharing(Transaction &tx, Score *score, bool on);
    static void handle_remove_part(Transaction &tx, Part *part);

private:
    static void cmd_create_shared_staves(Transaction &tx, Score *score);
    static void cmd_remove_shared_staves(Score *score);

    static StaveSharingGroups compute_groups(Score *score);
    static void create_shared_parts(Transaction &tx, const StaveSharingGroups &groups, Score *score);
    static SharedPart *create_shared_part(Score *score, size_t idx, const Instrument *instr);
    static void add_staff_to_shared_part(SharedPart *shared_part, const Part *origin_part, const KeyList &key_list,
                                         const StaffType *staff_type);

    static void connect_shared_part(Transaction &tx, SharedPart *shared_part, Part *origin_part);
    static void disconnect_shared_part(Transaction &tx, SharedPart *shared_part, Part *origin_part);
};
} // namespace mu::engraving
