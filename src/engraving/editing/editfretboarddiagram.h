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

#include "../dom/fret.h"

namespace mu::engraving {
struct FretUndoData {
    FretUndoData() = default;
    FretUndoData(FretDiagram *fd);

    void update_diagram();

private:
    FretDiagram *diagram_ = nullptr;
    BarreMap barres_;
    MarkerMap markers_;
    DotMap dots_;

    int strings_ = 0;
    int frets_ = 0;
    int fret_offset_ = 0;
    int max_frets_ = 0;
    bool show_nut_ = true;
    bool show_fingering_ = false;
    Orientation orientation_ = Orientation::VERTICAL;
    double user_mag_ = 1.0;
};

struct FretDataChange : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, FretDataChange)

    FretDiagram *diagram_ = nullptr;
    FretUndoData undo_data_;
    String harmony_name_;

    void redo() override;
    void undo() override;

public:
    FretDataChange(FretDiagram *d, const String &harmony_name) : diagram_(d), harmony_name_(harmony_name) {}

    UNDO_TYPE(CommandType::FRET_DATA_CHANGE)

    UNDO_NAME("FretDataChange")

    UNDO_CHANGED_OBJECTS({diagram_})
};

struct FretDot : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, FretDot)

    FretDiagram *diagram = nullptr;
    int string = 0;
    int fret = 0;
    bool add = 0;
    FretDotType dtype;
    FretUndoData undo_data;

    void redo() override;
    void undo() override;

public:
    FretDot(FretDiagram *d, int input_string, int input_fret, bool input_add = false, FretDotType input_dtype = FretDotType::NORMAL)
        : diagram(d), string(input_string), fret(input_fret), add(input_add), dtype(input_dtype) {}

    UNDO_TYPE(CommandType::FRET_DOT)
    UNDO_NAME("FretDot")
    UNDO_CHANGED_OBJECTS({diagram})
};

struct FretMarker : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, FretMarker)

    FretDiagram *diagram = nullptr;
    int string = 0;
    FretMarkerType mtype;
    FretUndoData undo_data;

    void redo() override;
    void undo() override;

public:
    FretMarker(FretDiagram *d, int input_string, FretMarkerType input_mtype) : diagram(d), string(input_string), mtype(input_mtype) {}

    UNDO_TYPE(CommandType::FRET_MARKER)
    UNDO_NAME("FretMarker")
    UNDO_CHANGED_OBJECTS({diagram})
};

struct FretBarre : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, FretBarre)

    FretDiagram *diagram = nullptr;
    int string = 0;
    int fret = 0;
    bool add = 0;
    FretUndoData undo_data;

    void redo() override;
    void undo() override;

public:
    FretBarre(FretDiagram *d, int input_string, int input_fret, bool input_add = false)
        : diagram(d), string(input_string), fret(input_fret), add(input_add) {}

    UNDO_TYPE(CommandType::FRET_BARRE)
    UNDO_NAME("FretBarre")
    UNDO_CHANGED_OBJECTS({diagram})
};

struct FretClear : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, FretClear)

    FretDiagram *diagram = nullptr;
    FretUndoData undo_data;

    void redo() override;
    void undo() override;

public:
    FretClear(FretDiagram *d) : diagram(d) {}

    UNDO_TYPE(CommandType::FRET_CLEAR)
    UNDO_NAME("FretClear")
    UNDO_CHANGED_OBJECTS({diagram})
};

struct AddFretDiagramToFretBox : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, AddFretDiagramToFretBox)

    FretDiagram *fret_diagram_ = nullptr;
    size_t idx_ = muse::nidx;

    void redo() override;
    void undo() override;

public:
    AddFretDiagramToFretBox(FretDiagram *f, size_t idx);

    UNDO_TYPE(CommandType::ADD_FRET_DIAGRAM_TO_FRET_BOX)
    UNDO_NAME("AddFretDiagramToFretBox")
    UNDO_CHANGED_OBJECTS({fret_diagram_})
};

struct RemoveFretDiagramFromFretBox : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, RemoveFretDiagramFromFretBox)

    FretDiagram *fret_diagram_ = nullptr;
    size_t idx_ = muse::nidx;

    void redo() override;
    void undo() override;

public:
    RemoveFretDiagramFromFretBox(FretDiagram *f);

    UNDO_TYPE(CommandType::REMOVE_FRET_DIAGRAM_FROM_FRET_BOX)
    UNDO_NAME("RemoveFretDiagramFromFretBox")
    UNDO_CHANGED_OBJECTS({fret_diagram_})
};
} // namespace mu::engraving
