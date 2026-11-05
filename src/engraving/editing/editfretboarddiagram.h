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
class FretUndoData {
public:
    FretUndoData() = default;
    FretUndoData(FretDiagram *fd);

    void updateDiagram();

private:
    FretDiagram *diagram_ = nullptr;
    BarreMap barres_;
    MarkerMap markers_;
    DotMap dots_;

    int strings_ = 0;
    int frets_ = 0;
    int fretOffset_ = 0;
    int maxFrets_ = 0;
    bool showNut_ = true;
    bool showFingering_ = false;
    Orientation orientation_ = Orientation::VERTICAL;
    double userMag_ = 1.0;
};

class FretDataChange : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, FretDataChange)

    FretDiagram *diagram_ = nullptr;
    FretUndoData undoData_;
    String harmonyName_;

    void redo() override;
    void undo() override;

public:
    FretDataChange(FretDiagram *d, const String &harmonyName) : diagram_(d), harmonyName_(harmonyName) {}

    UNDO_TYPE(CommandType::FretDataChange)

    UNDO_NAME("FretDataChange")

    UNDO_CHANGED_OBJECTS({diagram_})
};

class FretDot : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, FretDot)

    FretDiagram *diagram = nullptr;
    int string = 0;
    int fret = 0;
    bool add = 0;
    FretDotType dtype;
    FretUndoData undoData;

    void redo() override;
    void undo() override;

public:
    FretDot(FretDiagram *d, int _string, int _fret, bool _add = false, FretDotType _dtype = FretDotType::NORMAL)
        : diagram(d), string(_string), fret(_fret), add(_add), dtype(_dtype) {}

    UNDO_TYPE(CommandType::FretDot)
    UNDO_NAME("FretDot")
    UNDO_CHANGED_OBJECTS({diagram})
};

class FretMarker : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, FretMarker)

    FretDiagram *diagram = nullptr;
    int string = 0;
    FretMarkerType mtype;
    FretUndoData undoData;

    void redo() override;
    void undo() override;

public:
    FretMarker(FretDiagram *d, int _string, FretMarkerType _mtype) : diagram(d), string(_string), mtype(_mtype) {}

    UNDO_TYPE(CommandType::FretMarker)
    UNDO_NAME("FretMarker")
    UNDO_CHANGED_OBJECTS({diagram})
};

class FretBarre : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, FretBarre)

    FretDiagram *diagram = nullptr;
    int string = 0;
    int fret = 0;
    bool add = 0;
    FretUndoData undoData;

    void redo() override;
    void undo() override;

public:
    FretBarre(FretDiagram *d, int _string, int _fret, bool _add = false) : diagram(d), string(_string), fret(_fret), add(_add) {}

    UNDO_TYPE(CommandType::FretBarre)
    UNDO_NAME("FretBarre")
    UNDO_CHANGED_OBJECTS({diagram})
};

class FretClear : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, FretClear)

    FretDiagram *diagram = nullptr;
    FretUndoData undoData;

    void redo() override;
    void undo() override;

public:
    FretClear(FretDiagram *d) : diagram(d) {}

    UNDO_TYPE(CommandType::FretClear)
    UNDO_NAME("FretClear")
    UNDO_CHANGED_OBJECTS({diagram})
};

class AddFretDiagramToFretBox : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, AddFretDiagramToFretBox)

    FretDiagram *fretDiagram_ = nullptr;
    size_t idx_ = muse::nidx;

    void redo() override;
    void undo() override;

public:
    AddFretDiagramToFretBox(FretDiagram *f, size_t idx);

    UNDO_TYPE(CommandType::AddFretDiagramToFretBox)
    UNDO_NAME("AddFretDiagramToFretBox")
    UNDO_CHANGED_OBJECTS({fretDiagram_})
};

class RemoveFretDiagramFromFretBox : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, RemoveFretDiagramFromFretBox)

    FretDiagram *fretDiagram_ = nullptr;
    size_t idx_ = muse::nidx;

    void redo() override;
    void undo() override;

public:
    RemoveFretDiagramFromFretBox(FretDiagram *f);

    UNDO_TYPE(CommandType::RemoveFretDiagramFromFretBox)
    UNDO_NAME("RemoveFretDiagramFromFretBox")
    UNDO_CHANGED_OBJECTS({fretDiagram_})
};
} // namespace mu::engraving
