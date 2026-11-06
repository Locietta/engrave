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

#include "../dom/excerpt.h"
#include "../dom/masterscore.h"
#include "../dom/part.h"

namespace mu::engraving {
class AddExcerpt : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, AddExcerpt)

    Excerpt *excerpt = nullptr;
    bool delete_excerpt = false;

public:
    AddExcerpt(Excerpt *ex);
    ~AddExcerpt() override;

    void undo() override;
    void redo() override;

    std::vector<EngravingObject *> object_items() const override;

    UNDO_TYPE(CommandType::ADD_EXCERPT)
    UNDO_NAME("AddExcerpt")
};

class RemoveExcerpt : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, RemoveExcerpt)

    Excerpt *excerpt = nullptr;
    size_t index = muse::nidx;
    bool delete_excerpt = false;

public:
    RemoveExcerpt(Excerpt *ex);
    ~RemoveExcerpt() override;

    void undo() override;
    void redo() override;

    std::vector<EngravingObject *> object_items() const override;

    UNDO_TYPE(CommandType::REMOVE_EXCERPT)
    UNDO_NAME("RemoveExcerpt")
};

class SwapExcerpt : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, SwapExcerpt)

    MasterScore *score = nullptr;
    int pos1 = 0;
    int pos2 = 0;

    void flip() override;

public:
    SwapExcerpt(MasterScore *s, int p1, int p2) : score(s), pos1(p1), pos2(p2) {}

    UNDO_TYPE(CommandType::SWAP_EXCERPT)
    UNDO_NAME("SwapExcerpt")
    UNDO_CHANGED_OBJECTS({score})
};

class ChangeExcerptTitle : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, ChangeExcerptTitle)

    Excerpt *excerpt = nullptr;
    String title;

    void flip() override;

public:
    ChangeExcerptTitle(Excerpt *x, const String &t) : excerpt(x), title(t) {}

    UNDO_TYPE(CommandType::CHANGE_EXCERPT_TITLE)
    UNDO_NAME("ChangeExcerptTitle")
};

class AddPartToExcerpt : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, AddPartToExcerpt)

    Excerpt *excerpt_ = nullptr;
    Part *part_ = nullptr;
    size_t target_part_idx_ = 0;

public:
    AddPartToExcerpt(Excerpt *e, Part *p, size_t target_part_idx);
    void undo() override;
    void redo() override;
    void cleanup(bool was_done) override;

    UNDO_TYPE(CommandType::ADD_PART_TO_EXCERPT)
    UNDO_NAME("AddPartToExcerpt")
    UNDO_CHANGED_OBJECTS({part_})
};
} // namespace mu::engraving
