/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "undoablecommand.h"

#include "dom/chord.h"
#include "dom/fret.h"
#include "dom/harmony.h"
#include "dom/note.h"

using namespace mu::engraving;

std::vector<EngravingObject *> mu::engraving::compound_objects(EngravingObject *object) {
    std::vector<EngravingObject *> objects;

    if (object->is_chord()) {
        const Chord *chord = to_chord(object);
        for (const Note *note : chord->notes()) {
            for (Note *compound_note : note->compound_notes()) {
                objects.push_back(compound_note);
            }
        }
    } else if (object->is_note()) {
        const Note *note = to_note(object);
        for (Note *compound_note : note->compound_notes()) {
            objects.push_back(compound_note);
        }
    } else if (object->is_fret_diagram()) {
        const FretDiagram *fret = to_fret_diagram(object);
        if (Harmony *harmony = fret->harmony()) {
            objects.push_back(harmony);
        }
    }

    objects.push_back(object);

    return objects;
}

void UndoableCommand::undo() { flip(); }

void UndoableCommand::redo() { flip(); }
