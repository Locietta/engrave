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

#include "../dom/segment.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct AddElement : UndoableCommand {
private:

    EngravingItem *element = nullptr;

    void end_undo_redo(bool) const;
    void undo() override;
    void redo() override;

public:
    AddElement(EngravingItem *);

    EngravingItem *get_element() const { return element; }

    void cleanup(bool) override;
    const char *name() const override;

    bool matches_filter(UndoableCommandFilter f, const EngravingItem *target) const override;

    std::vector<EngravingObject *> object_items() const override;

    UNDO_TYPE(CommandType::ADD_ELEMENT)
};

struct RemoveElement : UndoableCommand {
private:

    EngravingItem *element = nullptr;

public:
    RemoveElement(EngravingItem *);
    void undo() override;
    void redo() override;
    void cleanup(bool) override;
    const char *name() const override;

    bool matches_filter(UndoableCommandFilter f, const EngravingItem *target) const override;

    std::vector<EngravingObject *> object_items() const override;

    UNDO_TYPE(CommandType::REMOVE_ELEMENT)
};

struct ChangeElement : UndoableCommand {
private:

    EngravingItem *old_element = nullptr;
    EngravingItem *new_element = nullptr;

    void flip() override;

public:
    ChangeElement(EngravingItem *old_element, EngravingItem *new_element);

    UNDO_TYPE(CommandType::CHANGE_ELEMENT)
    UNDO_NAME("ChangeElement")
    UNDO_CHANGED_OBJECTS({old_element, new_element})
};

struct ChangeParent : UndoableCommand {
private:

    EngravingItem *element = nullptr;
    EngravingItem *parent = nullptr;
    staff_idx_t staff_idx = muse::nidx;

    void flip() override;

public:
    ChangeParent(EngravingItem *e, EngravingItem *p, staff_idx_t si) : element(e), parent(p), staff_idx(si) {}

    UNDO_TYPE(CommandType::CHANGE_PARENT)
    UNDO_NAME("ChangeParent")
    UNDO_CHANGED_OBJECTS({element, parent})
};

struct ChangeSegmentParent : UndoableCommand {
private:

    Segment *segment = nullptr;
    Measure *parent = nullptr;
    Fraction tick;

    void flip() override;

public:
    ChangeSegmentParent(Segment *s, Measure *p, Fraction t) : segment(s), parent(p), tick(t) {}

    UNDO_TYPE(CommandType::CHANGE_PARENT)
    UNDO_NAME("ChangeSegmentParent")
    UNDO_CHANGED_OBJECTS({segment})
};

struct LinkUnlink : UndoableCommand {
private:

    bool must_delete{false};

protected:
    LinkedObjects *le = nullptr;
    EngravingObject *e = nullptr;

    void link();
    void unlink();

public:
    LinkUnlink() {}

    ~LinkUnlink();
};

struct Unlink : LinkUnlink {
public:
    Unlink(EngravingObject *);

    void undo() override { link(); }

    void redo() override { unlink(); }

    UNDO_TYPE(CommandType::UNLINK)
    UNDO_NAME("Unlink")
};

struct Link : LinkUnlink {
public:
    Link(EngravingObject *, EngravingObject *);

    void undo() override { unlink(); }

    void redo() override { link(); }

    UNDO_TYPE(CommandType::LINK)
    UNDO_NAME("Link")

    bool matches_filter(UndoableCommandFilter f, const EngravingItem *target) const override;
};
} // namespace mu::engraving
