/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
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

#include "editvisibility.h"

#include "global/containers.h"

#include "../dom/accidental.h"
#include "../dom/beam.h"
#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/note.h"
#include "../dom/notedot.h"
#include "../dom/ornament.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/select.h"
#include "../dom/trill.h"

#include "editproperty.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

void EditVisibility::set_selected_elements_visible(Transaction &tx, Score *score) {
    for (EngravingItem *e : score->selection().elements()) {
        tx.push(new ChangeProperty(e, Pid::VISIBLE, true));
    }
}

void EditVisibility::set_selected_elements_invisible(Transaction &tx, Score *score) {
    for (EngravingItem *e : score->selection().elements()) {
        tx.push(new ChangeProperty(e, Pid::VISIBLE, false));
    }
}

void EditVisibility::toggle_visible(Transaction &tx, Score *score) {
    bool all_visible = true;

    for (EngravingItem *item : score->selection().elements()) {
        if (!item->get_property(Pid::VISIBLE).to_bool()) {
            all_visible = false;
            break;
        }
    }

    bool new_visible = !all_visible;

    for (EngravingItem *item : score->selection().elements()) {
        undo_change_visible(tx, item, new_visible);
    }
}

static bool chord_has_visible_note(const Chord *chord) {
    for (const Note *note : chord->notes()) {
        if (note->visible()) {
            return true;
        }
    }

    return false;
}

static bool beam_has_visible_note(const Beam *beam) {
    for (const EngravingItem *item : beam->elements()) {
        if (item->is_chord() && chord_has_visible_note(to_chord(item))) {
            return true;
        }
    }

    return false;
}

static void undo_change_ornament_visibility(Ornament *ornament, bool visible);

static void undo_change_note_visibility(Note *note, bool visible) {
    note->undo_change_property(Pid::VISIBLE, visible);

    if (note->bend_back() || note->bend_for()) {
        note->set_override_bend_visibility_rules(true);
    } else {
        note->set_override_bend_visibility_rules(false);
    }

    for (NoteDot *dot : note->dots()) {
        dot->undo_change_property(Pid::VISIBLE, visible);
    }

    for (EngravingItem *e : note->el()) {
        e->undo_change_property(Pid::VISIBLE, visible);
    }

    if (note->accidental()) {
        note->accidental()->undo_change_property(Pid::VISIBLE, visible);
    }

    Chord *note_chord = note->chord();
    Beam *beam = note_chord->beam();
    std::vector<Chord *> chords;

    if (beam) {
        for (EngravingItem *item : beam->elements()) {
            if (item->is_chord()) {
                chords.push_back(to_chord(item));
            }
        }
    } else {
        chords.push_back(note_chord);
    }

    static const std::unordered_set<ElementType> ignored_types{
        ElementType::NOTE,        ElementType::LYRICS, ElementType::SLUR, ElementType::HAMMER_ON_PULL_OFF,
        ElementType::CHORD,       // grace notes
        ElementType::LEDGER_LINE, // temporary objects, impossible to change visibility
    };

    for (const Chord *chord : chords) {
        for (const EngravingObject *obj : chord->link_list()) {
            const Chord *linked_chord = to_chord(obj);
            const bool chord_has_visible_note_value = chord_has_visible_note(linked_chord);
            // note visibility can differ between scores, so check this score's own beam
            const Beam *linked_beam = linked_chord->beam();
            const bool beam_has_visible_note_value = chord_has_visible_note_value || (linked_beam && beam_has_visible_note(linked_beam));
            for (EngravingObject *child : linked_chord->get_children()) {
                const ElementType type = child->type();

                if (muse::contains(ignored_types, type)) {
                    continue;
                }

                if (beam) {
                    if (type == ElementType::STEM || type == ElementType::BEAM) {
                        child->undo_change_property(Pid::VISIBLE, beam_has_visible_note_value);
                        continue;
                    }
                }
                if (child->is_ornament()) {
                    undo_change_ornament_visibility(to_ornament(child), visible);
                } else {
                    child->undo_change_property(Pid::VISIBLE, chord_has_visible_note_value);
                }
            }
        }
    }
}

static void undo_change_rest_visibility(Rest *rest, bool visible) {
    rest->undo_change_property(Pid::VISIBLE, visible);

    for (NoteDot *dot : rest->dot_list()) {
        dot->undo_change_property(Pid::VISIBLE, visible);
    }
}

static void undo_change_ornament_visibility(Ornament *ornament, bool visible) {
    ornament->undo_change_property(Pid::VISIBLE, visible);
    Chord *cue_note_chord = ornament->cue_note_chord();
    if (cue_note_chord) {
        undo_change_note_visibility(cue_note_chord->up_note(), visible);
    }
    if (ornament->accidental_above()) {
        ornament->accidental_above()->undo_change_property(Pid::VISIBLE, visible);
    }
    if (ornament->accidental_below()) {
        ornament->accidental_below()->undo_change_property(Pid::VISIBLE, visible);
    }
}

void EditVisibility::undo_change_visible(Transaction &, EngravingItem *item, bool visible) {
    if (item->is_note()) {
        undo_change_note_visibility(to_note(item), visible);
    } else if (item->is_rest()) {
        undo_change_rest_visibility(to_rest(item), visible);
    } else if (item->is_ornament()) {
        undo_change_ornament_visibility(to_ornament(item), visible);
    } else if (item->is_trill_segment()) {
        item->undo_change_property(Pid::VISIBLE, visible);
        Ornament *orn = to_trill_segment(item)->trill()->ornament();
        if (orn) {
            undo_change_ornament_visibility(orn, visible);
        }
    } else {
        item->undo_change_property(Pid::VISIBLE, visible);
    }
}
