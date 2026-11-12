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

#include "../dom/measure.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
// Source measure-chain operations; failure-only records live in MutationJournal.
void insert_measure_chain(MeasureBase *first, MeasureBase *last, bool move_staff_type_changes);
void remove_measure_chain(MeasureBase *first, MeasureBase *last, bool move_staff_type_changes);
void apply_measure_length(Measure *measure, Fraction length);
void apply_measure_repeat_count(Measure *measure, int count, staff_idx_t staff);

struct ChangeMMRest : UndoableCommand {
private:

    Measure *m;
    Measure *mmrest;

    void flip() override;

public:
    ChangeMMRest(Measure *input_m, Measure *mmr) : m(input_m), mmrest(mmr) {}

    UNDO_TYPE(CommandType::CHANGE_MM_REST)
    UNDO_NAME("ChangeMMRest")
    UNDO_CHANGED_OBJECTS({m, mmrest})
};

// ChangeMMRestNext & ChangeMMRestPrev SHOULD ONLY BE USED FOR MMRests
// Setting next_ & prev_ involves the score's MeasureList for regular measures
// Source measure links use the chain operations above.
struct ChangeMMRestNext : UndoableCommand {
private:

    Measure *mmrest_ = nullptr;
    MeasureBase *next_ = nullptr;

    void flip() override;

public:
    ChangeMMRestNext(Measure *mmrest, MeasureBase *next) : mmrest_(mmrest), next_(next) {}

    UNDO_NAME("ChangeMMRestNext")
    UNDO_CHANGED_OBJECTS({mmrest_})
};

struct ChangeMMRestPrev : UndoableCommand {
private:

    Measure *mmrest_ = nullptr;
    MeasureBase *prev_ = nullptr;

    void flip() override;

public:
    ChangeMMRestPrev(Measure *mmrest, MeasureBase *prev) : mmrest_(mmrest), prev_(prev) {}

    UNDO_NAME("ChangeMMRestPrev")
    UNDO_CHANGED_OBJECTS({mmrest_})
};

} // namespace mu::engraving
