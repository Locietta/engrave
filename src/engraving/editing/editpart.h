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

    static void remove_parts(Score *score, const std::vector<Part *> &parts);
    static void remove_staves(Score *score, const std::vector<Staff *> &staves);

    static Staff *append_staff(Score *score, Part *destination_part);

private:
    static void do_append_staff(Score *score, Staff *staff, Part *destination_part, bool create_rests = true);
};
} // namespace mu::engraving
