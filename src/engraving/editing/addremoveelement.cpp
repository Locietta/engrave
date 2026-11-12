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

#include "addremoveelement.h"

#include "../dom/arpeggio.h"
#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/keysig.h"
#include "../dom/linkedobjects.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/spanner.h"
#include "../dom/staff.h"
#include "../dom/system.h"
#include "../dom/tie.h"
#include "../dom/tremolotwochord.h"
#include "../dom/tuplet.h"
#include "containers.h"

using namespace mu::engraving;

static void undo_remove_tuplet(DurationElement *cr) {
    if (cr->tuplet()) {
        cr->tuplet()->remove(cr);
        if (cr->tuplet()->elements().empty()) {
            undo_remove_tuplet(cr->tuplet());
        }
    }
}

static void undo_add_tuplet(DurationElement *cr) {
    if (cr->tuplet()) {
        cr->tuplet()->add(cr);
        if (cr->tuplet()->elements().size() == 1) {
            undo_add_tuplet(cr->tuplet());
        }
    }
}

void mu::engraving::validate_element_add(EngravingItem &item) {
    auto *e = &item;
    DO_ASSERT_X(!e->generated(), String(u"Generated item %1 passed to AddElement").arg(String::from_ascii(e->type_name())));
    DO_ASSERT_X(e->parent()->score() == e->score(),
                String(u"Item %1 is in a different score to its parent %2")
                    .arg(String::from_ascii(e->type_name()), String::from_ascii(e->parent()->type_name())));
}

static void update_element_membership(EngravingItem *element, bool removing) {
    if (element->is_chord_rest()) {
        if (removing) {
            undo_remove_tuplet(to_chord_rest(element));
        } else {
            undo_add_tuplet(to_chord_rest(element));
        }
    } else if (element->is_clef()) {
        element->trigger_layout();
        element->score()->set_layout(element->staff()->next_clef_tick(element->tick()), element->staff_idx());
    } else if (element->is_key_sig()) {
        element->trigger_layout();
        element->score()->set_layout(element->staff()->next_key_tick(element->tick()), element->staff_idx());
    }
}

void mu::engraving::revert_element_add(EngravingItem &item) {
    auto *element = &item;
    Score *score = element->score();
    EngravingItem::disconnect_shared_tree(element);

    if (!element->is_tuplet()) {
        score->remove_element(element);
    }

    update_element_membership(element, true);
}

void mu::engraving::apply_element_add(EngravingItem &item) {
    auto *element = &item;
    Score *score = element->score();

    if (!element->is_tuplet()) {
        score->add_element(element);
    }

    update_element_membership(element, false);
}

void mu::engraving::revert_element_remove(EngravingItem &item) {
    auto *element = &item;
    Score *score = element->score();

    if (!element->is_tuplet()) {
        score->add_element(element);
    }

    if (element->is_chord_rest()) {
        if (element->is_chord()) {
            Chord *chord = to_chord(element);
            for (Note *note : chord->notes()) {
                note->connect_tied_notes();
            }
        }
        undo_add_tuplet(to_chord_rest(element));
    } else if (element->is_clef()) {
        score->set_layout(element->staff()->next_clef_tick(element->tick()), element->staff_idx());
    } else if (element->is_key_sig()) {
        score->set_layout(element->staff()->next_key_tick(element->tick()), element->staff_idx());
    }
}

void mu::engraving::apply_element_remove(EngravingItem &item) {
    auto *element = &item;
    Score *score = element->score();

    EngravingItem::disconnect_shared_tree(element);
    if (!element->is_tuplet()) {
        score->remove_element(element);
    }

    if (element->is_chord_rest()) {
        undo_remove_tuplet(to_chord_rest(element));
        if (element->is_chord()) {
            Chord *chord = to_chord(element);
            for (Note *note : chord->notes()) {
                note->disconnect_tied_notes();
            }
        }
    } else if (element->is_clef()) {
        score->set_layout(element->staff()->next_clef_tick(element->tick()), element->staff_idx());
    } else if (element->is_key_sig()) {
        score->set_layout(element->staff()->next_key_tick(element->tick()), element->staff_idx());
    }
}

static void remove_note(const Note *note) {
    Score *score = note->score();
    Tie *tie_for = note->tie_for();
    Tie *tie_back = note->tie_back();
    if (tie_for && tie_for->end_note()) {
        score->do_undo_remove_element(tie_for);
    }
    if (tie_back) {
        if (tie_back->tie_jump_points() && tie_back->tie_jump_points()->size() > 1) {
            Tie::change_tie_type(tie_back);
        } else {
            score->do_undo_remove_element(tie_back);
        }
    }
    for (Spanner *s : note->spanner_back()) {
        score->do_undo_remove_element(s);
    }
    for (Spanner *s : note->spanner_for()) {
        score->do_undo_remove_element(s);
    }
}

void mu::engraving::prepare_element_removal(EngravingItem &item) {
    auto *e = &item;
    DO_ASSERT_X(!e->generated(), String(u"Generated item %1 passed to RemoveElement").arg(String::from_ascii(e->type_name())));
    auto *element = e;

    Score *score = element->score();
    if (element->is_chord_rest()) {
        ChordRest *cr = to_chord_rest(element);
        if (cr->tuplet() && cr->tuplet()->elements().size() <= 1) {
            score->do_undo_remove_element(cr->tuplet());
        }
        if (e->is_chord()) {
            Chord *chord = to_chord(e);
            // remove tremolo between 2 notes
            if (chord->tremolo_two_chord()) {
                TremoloTwoChord *tremolo = chord->tremolo_two_chord();
                score->do_undo_remove_element(tremolo);
            }
            // Move arpeggio down to next available note
            if (chord->arpeggio()) {
                chord->arpeggio()->rebase_start_anchor(AnchorRebaseDirection::DOWN);
            } else {
                // If this chord is the end of an arpeggio, move the end of the arpeggio upwards to the next available chord
                Arpeggio *span_arp = chord->span_arpeggio();
                if (span_arp && chord->track() == span_arp->end_track()) {
                    span_arp->rebase_end_anchor(AnchorRebaseDirection::UP);
                }
            }
            for (const Note *note : chord->notes()) {
                remove_note(note);
            }
        }
    } else if (element->is_note()) {
        // Removing an individual note within a chord
        const Note *note = to_note(element);
        remove_note(note);
    }
}

void mu::engraving::apply_element_replace(EngravingItem &before, EngravingItem &after) {
    auto *old_element = &before;
    auto *new_element = &after;
    const LinkedObjects *links = old_element->links();
    if (links) {
        new_element->link_to(old_element);
        old_element->unlink();
    }

    new_element->set_track(old_element->track());
    if (new_element->is_spanner()) {
        to_spanner(new_element)->set_track2(to_spanner(old_element)->track2());
    }

    Score *score = old_element->score();
    if (!score->selection().is_range()) {
        if (old_element->selected()) {
            score->deselect(old_element);
        }
        if (new_element->selected()) {
            score->select(new_element, SelectType::ADD);
        }
    }

    if (old_element->ownership_parent() == nullptr) {
        new_element->set_score(score);
        score->remove_element(old_element);
        score->add_element(new_element);
    } else {
        old_element->ownership_parent_item()->change(old_element, new_element);
    }

    if (new_element->is_key_sig()) {
        KeySig *ks = to_key_sig(new_element);
        if (!ks->generated()) {
            ks->staff()->set_key(ks->tick(), ks->key_sig_event());
        }
    }

    if (new_element->is_spanner_segment()) {
        SpannerSegment *os = to_spanner_segment(old_element);
        SpannerSegment *ns = to_spanner_segment(new_element);
        if (os->system()) {
            os->system()->remove(os);
        }
        if (ns->system()) {
            ns->system()->add(ns);
        }
    }

    new_element->trigger_layout();
    old_element->trigger_layout();
}

void mu::engraving::apply_element_reparent(EngravingItem &item, EngravingItem &destination, track_idx_t track) {
    auto *parent = item.ownership_parent_item();
    parent->remove(&item);
    item.set_ownership_parent(&destination);
    item.set_track(track);
    destination.add(&item);
    item.trigger_layout();
}

void mu::engraving::apply_segment_reparent(Segment &segment, Measure &destination, Fraction tick) {
    segment.measure()->remove(&segment);
    segment.set_ownership_parent(&destination);
    segment.set_rtick(tick);
    destination.add(&segment);
    segment.trigger_layout();
}
