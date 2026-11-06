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
    fret_offset_ = diagram_->fret_offset_;
    max_frets_ = diagram_->max_frets_;
    show_nut_ = diagram_->show_nut_;
    orientation_ = diagram_->orientation_;

    user_mag_ = diagram_->user_mag_;

    show_fingering_ = diagram_->show_fingering_;
}

//---------------------------------------------------------
//   updateDiagram
//---------------------------------------------------------

void FretUndoData::update_diagram() {
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
    diagram_->fret_offset_ = fret_offset_;
    diagram_->max_frets_ = max_frets_;
    diagram_->show_nut_ = show_nut_;
    diagram_->orientation_ = orientation_;

    diagram_->user_mag_ = user_mag_;

    diagram_->show_fingering_ = show_fingering_;
}

//---------------------------------------------------------
//   FretDataChange
//---------------------------------------------------------

void FretDataChange::redo() {
    undo_data_ = FretUndoData(diagram_);

    diagram_->update_diagram(harmony_name_);
}

void FretDataChange::undo() { undo_data_.update_diagram(); }

//---------------------------------------------------------
//   FretDot
//---------------------------------------------------------

void FretDot::redo() {
    undo_data = FretUndoData(diagram);

    diagram->set_dot(string, fret, add, dtype);
    diagram->trigger_layout();
}

void FretDot::undo() {
    undo_data.update_diagram();
    diagram->trigger_layout();
}

//---------------------------------------------------------
//   FretMarker
//---------------------------------------------------------

void FretMarker::redo() {
    undo_data = FretUndoData(diagram);

    diagram->set_marker(string, mtype);
    diagram->trigger_layout();
}

void FretMarker::undo() {
    undo_data.update_diagram();
    diagram->trigger_layout();
}

//---------------------------------------------------------
//   FretBarre
//---------------------------------------------------------

void FretBarre::redo() {
    undo_data = FretUndoData(diagram);

    diagram->set_barre(string, fret, add);
    diagram->trigger_layout();
}

void FretBarre::undo() {
    undo_data.update_diagram();
    diagram->trigger_layout();
}

//---------------------------------------------------------
//   FretClear
//---------------------------------------------------------

void FretClear::redo() {
    undo_data = FretUndoData(diagram);

    diagram->clear();
    diagram->trigger_layout();
}

void FretClear::undo() {
    undo_data.update_diagram();
    diagram->trigger_layout();
}

//---------------------------------------------------------
//   AddFretDiagramToFretBox
//---------------------------------------------------------

AddFretDiagramToFretBox::AddFretDiagramToFretBox(FretDiagram *f, size_t idx) : fret_diagram_(f), idx_(idx) {}

void AddFretDiagramToFretBox::redo() {
    FBox *fbox = to_f_box(fret_diagram_->parent());
    fbox->add_at_idx(fret_diagram_, idx_);
}

void AddFretDiagramToFretBox::undo() {
    FBox *fbox = to_f_box(fret_diagram_->parent());
    fbox->remove(fret_diagram_);
}

//---------------------------------------------------------
//   RemoveFretDiagramFromFretBox
//---------------------------------------------------------

RemoveFretDiagramFromFretBox::RemoveFretDiagramFromFretBox(FretDiagram *f) : fret_diagram_(f) {
    FBox *fbox = to_f_box(f->parent());
    const ElementList &el = fbox->el();
    idx_ = muse::index_of(el, fret_diagram_);
}

void RemoveFretDiagramFromFretBox::redo() {
    FBox *fbox = to_f_box(fret_diagram_->parent());
    fbox->remove(fret_diagram_);
}

void RemoveFretDiagramFromFretBox::undo() {
    FBox *fbox = to_f_box(fret_diagram_->parent());
    fbox->add_at_idx(fret_diagram_, idx_);
}
