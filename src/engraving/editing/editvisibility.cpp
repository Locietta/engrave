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

#include "../dom/accidental.h"
#include "../dom/beam.h"
#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/masterscore.h"
#include "../dom/note.h"
#include "../dom/notedot.h"
#include "../dom/ornament.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/select.h"
#include "../dom/trill.h"
#include "cmd.h"
#include "global/containers.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

void EditVisibility::set_selected_elements_visible(Transaction &tx, Score *score) {
    for (EngravingItem *e : score->selection().elements()) {
        tx.change_property(e, Pid::VISIBLE, true);
    }
}

void EditVisibility::set_selected_elements_invisible(Transaction &tx, Score *score) {
    for (EngravingItem *e : score->selection().elements()) {
        tx.change_property(e, Pid::VISIBLE, false);
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

void EditVisibility::change_chord_child_visible(Chord *chord, AuthoredChildKey key, bool visible) {
    const auto *entry = chord->authored_overrides.find(key, Pid::VISIBLE);
    if ((entry ? entry->value.to_bool() : true) == visible) return;
    if (note_change(chord, CommandType::CHANGE_PROPERTY))
        chord->master_score()->cmd_state().changes.changed_property_id_set.insert(Pid::VISIBLE);
    chord->authored_overrides.set({key, Pid::VISIBLE, visible});
    chord->trigger_layout();
}

void EditVisibility::change_dots_visible(EngravingItem *owner, int count, bool visible) {
    for (auto *object : owner->link_list_for_property_propagation()) {
        auto *linked = to_engraving_item(object);
        if (linked != owner && owner->property_propagation(linked, Pid::VISIBLE) != PropertyPropagation::PROPAGATE) continue;
        auto &table = *authored_override_table(linked);
        for (uint32_t i = 0; i < uint32_t(count); ++i) {
            const AuthoredChildKey key{AuthoredChildRole::DOT, i};
            const auto *appearance = table.find(key, Pid::APPEARANCE_LINKED_TO_MASTER);
            if (linked != owner && appearance && !appearance->value.to_bool()) continue;
            const auto *entry = table.find(key, Pid::VISIBLE);
            if ((entry ? entry->value.to_bool() : linked->visible()) == visible) continue;
            if (note_change(linked, CommandType::CHANGE_PROPERTY))
                linked->master_score()->cmd_state().changes.changed_property_id_set.insert(Pid::VISIBLE);
            for (uint32_t j = 0; j <= i; ++j) {
                const AuthoredChildKey ordinal{AuthoredChildRole::DOT, j};
                if (!table.carrier(ordinal)) table.set_carrier({ordinal, ElementType::NOTEDOT});
            }
            table.set({key, Pid::VISIBLE, visible});
            linked->trigger_layout();
        }
    }
}

static void undo_change_ornament_visibility(Ornament *ornament, bool visible);

static void undo_change_note_visibility(Note *note, bool visible) {
    EditVisibility::change_dots_visible(note, note->chord()->dots(), visible);
    note->undo_change_property(Pid::VISIBLE, visible);

    if (note->bend_back() || note->bend_for()) {
        note->set_override_bend_visibility_rules(true);
    } else {
        note->set_override_bend_visibility_rules(false);
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
        for (EngravingObject *obj : chord->link_list()) {
            Chord *linked_chord = to_chord(obj);
            const bool chord_has_visible_note_value = chord_has_visible_note(linked_chord);
            // note visibility can differ between scores, so check this score's own beam
            const Beam *linked_beam = linked_chord->beam();
            const bool beam_has_visible_note_value = chord_has_visible_note_value || (linked_beam && beam_has_visible_note(linked_beam));
            for (auto role : {AuthoredChildRole::STEM, AuthoredChildRole::HOOK, AuthoredChildRole::STEM_SLASH}) {
                const bool value = beam && role == AuthoredChildRole::STEM ? beam_has_visible_note_value : chord_has_visible_note_value;
                EditVisibility::change_chord_child_visible(linked_chord, role, value);
            }
            for (uint32_t i = 0; i < linked_chord->note_parentheses().size(); ++i) {
                const auto &notes = linked_chord->note_parentheses()[i]->notes();
                const bool value = std::ranges::any_of(notes, [](const Note *note) { return note->visible(); });
                for (auto role : {AuthoredChildRole::PAREN_LEFT, AuthoredChildRole::PAREN_RIGHT})
                    EditVisibility::change_chord_child_visible(linked_chord, {role, i, 0, 1}, value);
            }
            for (EngravingObject *child : linked_chord->get_children()) {
                if (child->is_stem() || child->is_hook() || child->is_stem_slash()) continue;
                const ElementType type = child->type();

                if (muse::contains(ignored_types, type)) {
                    continue;
                }

                if (beam) {
                    if (type == ElementType::BEAM) {
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
    EditVisibility::change_dots_visible(rest, rest->dots(), visible);
    rest->undo_change_property(Pid::VISIBLE, visible);
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
