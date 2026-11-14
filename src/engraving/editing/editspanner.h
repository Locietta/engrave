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

#include "../dom/spanner.h"
#include "engraving/editing/cmd.h"

namespace mu::engraving {
struct Score;
struct Segment;
struct Transaction;

struct EditSpanner {

    static void add_spanner(Transaction &tx, Score *score, Spanner *spanner, staff_idx_t staff_idx, Segment *start_segment,
                            Segment *end_segment, bool ctrl_modifier = false);
};

void apply_change_spanner_elements(Spanner *s, EngravingItem *se, EngravingItem *ee);

void apply_change_start_end_spanner(Spanner *sp, EngravingItem *s, EngravingItem *e);
} // namespace mu::engraving
