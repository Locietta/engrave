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
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct Score;
struct Segment;
struct Transaction;

struct EditSpanner {
    static void add_spanner(Transaction &tx, Score *score, Spanner *spanner, const PointF &pos, bool system_staves_only = false);
    static void add_spanner(Transaction &tx, Score *score, Spanner *spanner, staff_idx_t staff_idx, Segment *start_segment,
                            Segment *end_segment, bool ctrl_modifier = false);
};

struct ChangeSpannerElements : UndoableCommand {
private:

    Spanner *spanner = nullptr;
    EngravingItem *start_element = nullptr;
    EngravingItem *end_element = nullptr;

    void flip() override;

public:
    ChangeSpannerElements(Spanner *s, EngravingItem *se, EngravingItem *ee) : spanner(s), start_element(se), end_element(ee) {}

    UNDO_TYPE(CommandType::CHANGE_SPANNER_ELEMENTS)
    UNDO_NAME("ChangeSpannerElements")
    UNDO_CHANGED_OBJECTS({spanner})
};

struct ChangeStartEndSpanner : UndoableCommand {
private:

    Spanner *spanner = nullptr;
    EngravingItem *start = nullptr;
    EngravingItem *end = nullptr;

    void flip() override;

public:
    ChangeStartEndSpanner(Spanner *sp, EngravingItem *s, EngravingItem *e) : spanner(sp), start(s), end(e) {}

    UNDO_TYPE(CommandType::CHANGE_START_END_SPANNER)
    UNDO_NAME("ChangeStartEndSpanner")
    UNDO_CHANGED_OBJECTS({spanner})
};
} // namespace mu::engraving
