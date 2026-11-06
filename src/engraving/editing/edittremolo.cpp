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

#include "edittremolo.h"

#include "../dom/chord.h"
#include "../dom/measure.h"
#include "../dom/score.h"
#include "../dom/tremolotwochord.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   MoveTremolo
//---------------------------------------------------------

void MoveTremolo::redo() {
    // Find new tremolo chords
    Measure *m1 = score->tick2measure(chord1_tick);
    Measure *m2 = score->tick2measure(chord2_tick);
    IF_ASSERT_FAILED(m1 && m2) { return; }
    Chord *c1 = m1->find_chord(chord1_tick, track);
    Chord *c2 = m2->find_chord(chord2_tick, track);
    IF_ASSERT_FAILED(c1 && c2) { return; }

    // Remember the old tremolo chords
    old_c1 = trem->chord1();
    old_c2 = trem->chord2();

    // Move tremolo away from old chords
    trem->chord1()->set_tremolo_two_chord(nullptr);
    trem->chord2()->set_tremolo_two_chord(nullptr);

    // Delete old tremolo on c1 and c2, if present
    if (c1->tremolo_two_chord() && (c1->tremolo_two_chord() != trem)) {
        if (c2->tremolo_two_chord() == c1->tremolo_two_chord()) {
            c2->tremolo_two_chord()->set_chords(c1, c2);
        } else {
            c1->tremolo_two_chord()->set_chords(c1, nullptr);
        }
        TremoloTwoChord *old_tremolo = c1->tremolo_two_chord();
        c1->set_tremolo_two_chord(nullptr);
        delete old_tremolo;
    }
    if (c2->tremolo_two_chord() && (c2->tremolo_two_chord() != trem)) {
        c2->tremolo_two_chord()->set_chords(nullptr, c2);
        TremoloTwoChord *old_tremolo = c2->tremolo_two_chord();
        c2->set_tremolo_two_chord(nullptr);
        delete old_tremolo;
    }

    // Move tremolo to new chords
    c1->set_tremolo_two_chord(trem);
    c2->set_tremolo_two_chord(trem);
    trem->set_chords(c1, c2);
    trem->set_ownership_parent(c1);

    // Tremolo would cross barline, so remove it
    if (m1 != m2) {
        score->undo_remove_element(trem);
        return;
    }
    // One of the notes crosses a barline, so remove the tremolo
    if (c1->ticks() != c2->ticks()) {
        score->undo_remove_element(trem);
    }
}

void MoveTremolo::undo() {
    // Move tremolo to old position
    trem->chord1()->set_tremolo_two_chord(nullptr);
    trem->chord2()->set_tremolo_two_chord(nullptr);
    old_c1->set_tremolo_two_chord(trem);
    old_c2->set_tremolo_two_chord(trem);
    trem->set_chords(old_c1, old_c2);
    trem->set_ownership_parent(old_c1);
}
