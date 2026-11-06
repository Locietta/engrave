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

#include "transaction/undoablecommand.h"

#include "../dom/measure.h"

namespace mu::engraving {
class InsertRemoveMeasures : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, InsertRemoveMeasures)

    MeasureBase *fm = nullptr;
    MeasureBase *lm = nullptr;

    static std::vector<Clef *> get_courtesy_clefs(Measure *m);

    bool move_stc_value = true;

protected:
    void remove_measures();
    void insert_measures();

public:
    InsertRemoveMeasures(MeasureBase *input_fm, MeasureBase *input_lm, bool input_move_stc)
        : fm(input_fm), lm(input_lm), move_stc_value(input_move_stc) {}

    virtual void undo() override = 0;
    virtual void redo() override = 0;
    UNDO_CHANGED_OBJECTS({fm, lm})
};

class RemoveMeasures : public InsertRemoveMeasures {
    OBJECT_ALLOCATOR(engraving, RemoveMeasures)
public:
    RemoveMeasures(MeasureBase *m1, MeasureBase *m2, bool input_move_stc = true) : InsertRemoveMeasures(m1, m2, input_move_stc) {}

    void undo() override { insert_measures(); }

    void redo() override { remove_measures(); }

    UNDO_TYPE(CommandType::REMOVE_MEASURES)
    UNDO_NAME("RemoveMeasures")
};

class InsertMeasures : public InsertRemoveMeasures {
    OBJECT_ALLOCATOR(engraving, InsertMeasures)
public:
    InsertMeasures(MeasureBase *m1, MeasureBase *m2, bool input_move_stc = true) : InsertRemoveMeasures(m1, m2, input_move_stc) {}

    void redo() override { insert_measures(); }

    void undo() override { remove_measures(); }

    UNDO_TYPE(CommandType::INSERT_MEASURES)
    UNDO_NAME("InsertMeasures")
};

class ChangeMeasureLen : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, ChangeMeasureLen)

    Measure *measure = nullptr;
    Fraction len;

    void flip() override;

public:
    ChangeMeasureLen(Measure *, Fraction);

    UNDO_TYPE(CommandType::CHANGE_MEASURE_LEN)
    UNDO_NAME("ChangeMeasureLen")
    UNDO_CHANGED_OBJECTS({measure})
};

class ChangeMMRest : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, ChangeMMRest)

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
// This is handled through undo commands above
class ChangeMMRestNext : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, ChangeMMRestNext)

    Measure *mmrest_ = nullptr;
    MeasureBase *next_ = nullptr;

    void flip() override;

public:
    ChangeMMRestNext(Measure *mmrest, MeasureBase *next) : mmrest_(mmrest), next_(next) {}

    UNDO_NAME("ChangeMMRestNext")
    UNDO_CHANGED_OBJECTS({mmrest_})
};

class ChangeMMRestPrev : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, ChangeMMRestPrev)

    Measure *mmrest_ = nullptr;
    MeasureBase *prev_ = nullptr;

    void flip() override;

public:
    ChangeMMRestPrev(Measure *mmrest, MeasureBase *prev) : mmrest_(mmrest), prev_(prev) {}

    UNDO_NAME("ChangeMMRestPrev")
    UNDO_CHANGED_OBJECTS({mmrest_})
};

class ChangeMeasureRepeatCount : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, ChangeMeasureRepeatCount)

    Measure *m = nullptr;
    int count = 0;
    staff_idx_t staff_idx_value = muse::nidx;

    void flip() override;

public:
    ChangeMeasureRepeatCount(Measure *input_m, int input_count, staff_idx_t input_staff_idx)
        : m(input_m), count(input_count), staff_idx_value(input_staff_idx) {}

    UNDO_TYPE(CommandType::CHANGE_MEASURE_REPEAT_COUNT)
    UNDO_NAME("ChangeMeasureRepeatCount")
    UNDO_CHANGED_OBJECTS({m})
};
} // namespace mu::engraving
