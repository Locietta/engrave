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

#include "../dom/instrument.h"
#include "../dom/part.h"
#include "../dom/score.h"

namespace mu::engraving {
struct InstrumentTemplate;
struct ScoreOrder;
struct Staff;
struct StaffType;
enum class PreferSharpFlat : char;
enum class StaffTypes : signed char;

struct EditPart {

    static void set_part_visible(Score *score, Part *part, bool visible);
    static void set_staff_visible(Score *score, Staff *staff, bool visible);
    static void set_part_sharp_flat(Score *score, Part *part, PreferSharpFlat sharp_flat);
    static void set_staff_type(Score *score, Staff *staff, StaffTypes type_id);

    static void remove_parts(Score *score, const std::vector<Part *> &parts);
    static void remove_staves(Score *score, const std::vector<Staff *> &staves);
    static void move_parts(Score *score, const std::vector<Part *> &source_parts, Part *destination_part, bool insert_after);
    static void move_staves(Score *score, const std::vector<Staff *> &source_staves, Staff *destination_staff, bool insert_after);

    static void add_system_objects(Score *score, const std::vector<Staff *> &staves);
    static void remove_system_objects(Score *score, const std::vector<Staff *> &staves);
    static void move_system_objects(Score *score, Staff *source_staff, Staff *destination_staff);

    static Staff *append_staff(Score *score, Part *destination_part);
    static Staff *append_linked_staff(Score *score, Staff *source_staff, Part *destination_part);

    static bool set_voice_visible(Score *score, Staff *staff, int voice_index, bool visible);

    static void insert_part(Score *score, const InstrumentTemplate *templ, size_t index);
    static void replace_part(Score *score, Part *old_part, const InstrumentTemplate *templ);

    static void set_score_order(Score *score, const ScoreOrder &order);

private:
    static void do_append_staff(Score *score, Staff *staff, Part *destination_part, bool create_rests = true);
};
} // namespace mu::engraving
