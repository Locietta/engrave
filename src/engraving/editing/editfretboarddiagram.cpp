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

#include "editfretboarddiagram.h"

#include "../dom/box.h"
#include "../dom/fret.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   updateStored
//---------------------------------------------------------

FretUndoData::FretUndoData(FretDiagram *fd) {
    // We need to store the old barres and markers, since predicting how
    // adding dots, markers, barres etc. will change things is too difficult.
    // Update linked fret diagrams:
    diagram_ = fd;
    dots_ = diagram_->dots_;
    markers_ = diagram_->markers_;
    barres_ = diagram_->barres_;

    strings_ = diagram_->strings_;
    frets_ = diagram_->frets_;
    fretOffset_ = diagram_->fretOffset_;
    maxFrets_ = diagram_->maxFrets_;
    showNut_ = diagram_->showNut_;
    orientation_ = diagram_->orientation_;

    userMag_ = diagram_->userMag_;

    showFingering_ = diagram_->showFingering_;
}

//---------------------------------------------------------
//   updateDiagram
//---------------------------------------------------------

void FretUndoData::updateDiagram() {
    if (!diagram_) {
        ASSERT_X("Tried to undo fret diagram change without ever setting diagram!");
        return;
    }

    // Reset every fret diagram property of the changed diagram
    // FretUndoData is a friend of FretDiagram so has access to these private members
    diagram_->barres_ = barres_;
    diagram_->markers_ = markers_;
    diagram_->dots_ = dots_;

    diagram_->strings_ = strings_;
    diagram_->frets_ = frets_;
    diagram_->fretOffset_ = fretOffset_;
    diagram_->maxFrets_ = maxFrets_;
    diagram_->showNut_ = showNut_;
    diagram_->orientation_ = orientation_;

    diagram_->userMag_ = userMag_;

    diagram_->showFingering_ = showFingering_;
}

//---------------------------------------------------------
//   FretDataChange
//---------------------------------------------------------

void FretDataChange::redo() {
    undoData_ = FretUndoData(diagram_);

    diagram_->updateDiagram(harmonyName_);
}

void FretDataChange::undo() { undoData_.updateDiagram(); }

//---------------------------------------------------------
//   FretDot
//---------------------------------------------------------

void FretDot::redo() {
    undoData = FretUndoData(diagram);

    diagram->setDot(string, fret, add, dtype);
    diagram->triggerLayout();
}

void FretDot::undo() {
    undoData.updateDiagram();
    diagram->triggerLayout();
}

//---------------------------------------------------------
//   FretMarker
//---------------------------------------------------------

void FretMarker::redo() {
    undoData = FretUndoData(diagram);

    diagram->setMarker(string, mtype);
    diagram->triggerLayout();
}

void FretMarker::undo() {
    undoData.updateDiagram();
    diagram->triggerLayout();
}

//---------------------------------------------------------
//   FretBarre
//---------------------------------------------------------

void FretBarre::redo() {
    undoData = FretUndoData(diagram);

    diagram->setBarre(string, fret, add);
    diagram->triggerLayout();
}

void FretBarre::undo() {
    undoData.updateDiagram();
    diagram->triggerLayout();
}

//---------------------------------------------------------
//   FretClear
//---------------------------------------------------------

void FretClear::redo() {
    undoData = FretUndoData(diagram);

    diagram->clear();
    diagram->triggerLayout();
}

void FretClear::undo() {
    undoData.updateDiagram();
    diagram->triggerLayout();
}

//---------------------------------------------------------
//   AddFretDiagramToFretBox
//---------------------------------------------------------

AddFretDiagramToFretBox::AddFretDiagramToFretBox(FretDiagram *f, size_t idx) : fretDiagram_(f), idx_(idx) {}

void AddFretDiagramToFretBox::redo() {
    FBox *fbox = toFBox(fretDiagram_->parent());
    fbox->addAtIdx(fretDiagram_, idx_);
}

void AddFretDiagramToFretBox::undo() {
    FBox *fbox = toFBox(fretDiagram_->parent());
    fbox->remove(fretDiagram_);
}

//---------------------------------------------------------
//   RemoveFretDiagramFromFretBox
//---------------------------------------------------------

RemoveFretDiagramFromFretBox::RemoveFretDiagramFromFretBox(FretDiagram *f) : fretDiagram_(f) {
    FBox *fbox = toFBox(f->parent());
    const ElementList &el = fbox->el();
    idx_ = muse::indexOf(el, fretDiagram_);
}

void RemoveFretDiagramFromFretBox::redo() {
    FBox *fbox = toFBox(fretDiagram_->parent());
    fbox->remove(fretDiagram_);
}

void RemoveFretDiagramFromFretBox::undo() {
    FBox *fbox = toFBox(fretDiagram_->parent());
    fbox->addAtIdx(fretDiagram_, idx_);
}
