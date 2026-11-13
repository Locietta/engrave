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

void mu::engraving::apply_fret_data_change(FretDiagram *diagram_, const String &harmony_name_) {
    note_change(diagram_, CommandType::FRET_DATA_CHANGE);

    diagram_->update_diagram(harmony_name_);
}

void mu::engraving::apply_fret_dot(FretDiagram *diagram, int string, int fret, bool add, FretDotType dtype) {
    note_change(diagram, CommandType::FRET_DOT);

    diagram->set_dot(string, fret, add, dtype);
    diagram->trigger_layout();
}

void mu::engraving::apply_fret_marker(FretDiagram *diagram, int string, FretMarkerType mtype) {
    note_change(diagram, CommandType::FRET_MARKER);

    diagram->set_marker(string, mtype);
    diagram->trigger_layout();
}

void mu::engraving::apply_fret_barre(FretDiagram *diagram, int string, int fret, bool add) {
    note_change(diagram, CommandType::FRET_BARRE);

    diagram->set_barre(string, fret, add);
    diagram->trigger_layout();
}

void mu::engraving::apply_fret_clear(FretDiagram *diagram) {
    note_change(diagram, CommandType::FRET_CLEAR);

    diagram->clear();
    diagram->trigger_layout();
}
