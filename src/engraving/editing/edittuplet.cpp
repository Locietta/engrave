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

#include "edittuplet.h"

#include <algorithm>
#include <unordered_set>

#include "../dom/chord.h"
#include "../dom/chordrest.h"
#include "../dom/input.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/select.h"
#include "log.h"

using namespace mu::engraving;

static Fraction dividing_ratio(const ChordRest *chord_rest, const Fraction &ratio, bool auto_base_len) {
    Fraction result = ratio;
    if (auto_base_len) {
        result.set_denominator(Tuplet::compute_tuplet_denominator(ratio.numerator(), chord_rest->ticks()));
    }
    return result;
}

// Selected chords/rests a tuplet can start at, in score order
static std::vector<ChordRest *> selected_tuplet_chord_rests(const Score *score) {
    // Score order, not pointer order
    const std::set<ChordRest *> selected_chord_rests = score->get_selected_chord_rests();
    std::vector<ChordRest *> sorted_chord_rests(selected_chord_rests.begin(), selected_chord_rests.end());
    std::sort(sorted_chord_rests.begin(), sorted_chord_rests.end(), [](const ChordRest *a, const ChordRest *b) {
        return a->tick() != b->tick() ? a->tick() < b->tick() : a->track() < b->track();
    });

    std::vector<ChordRest *> chord_rests;

    // Make sure each linked group is handled only once
    std::unordered_set<const LinkedObjects *> handled_links;

    for (ChordRest *chord_rest : sorted_chord_rests) {
        if (chord_rest->links() && !handled_links.insert(chord_rest->links()).second) {
            continue;
        }

        if (!chord_rest->is_grace() && !(chord_rest->is_chord() && to_chord(chord_rest)->is_trill_cue_note())) {
            chord_rests.push_back(chord_rest);
        }
    }

    return chord_rests;
}

static void select_tuplet_start(Score *score, const Tuplet *tuplet) {
    if (tuplet->elements().empty()) {
        return;
    }

    DurationElement *first_element = tuplet->elements().front();
    // SINGLE also moves the input position
    if (first_element->is_chord()) {
        const std::vector<Note *> &notes = to_chord(first_element)->notes();
        IF_ASSERT_FAILED(!notes.empty()) { score->select(first_element, SelectType::SINGLE); }
        else {
            score->select(notes.front(), SelectType::SINGLE);
            score->select({notes.begin() + 1, notes.end()}, SelectType::ADD);
        }
    } else {
        score->select(first_element, SelectType::SINGLE);
    }
}

std::vector<Tuplet *> EditTuplet::add_tuplets(Transaction &, Score *score, const Fraction &ratio, TupletNumberType number_type,
                                              TupletBracketType bracket_type, bool auto_base_len) {
    Selection &selection = score->selection();
    const bool was_range_selection = selection.is_range();
    const Fraction start_tick = selection.tick_start();
    const Fraction end_tick = selection.tick_end();
    const staff_idx_t start_staff = selection.staff_start();
    const staff_idx_t end_staff = selection.staff_end();
    Box *start_box = selection.start_box();
    Box *end_box = selection.end_box();

    std::vector<Tuplet *> tuplets;
    for (ChordRest *chord_rest : selected_tuplet_chord_rests(score)) {
        if (Tuplet *tuplet = score->add_tuplet(chord_rest, dividing_ratio(chord_rest, ratio, auto_base_len), number_type, bracket_type)) {
            tuplets.push_back(tuplet);
        }
    }

    if (was_range_selection) {
        selection.set_range_ticks(start_tick, end_tick, start_staff, end_staff, start_box, end_box);
        selection.update_selected_elements();
    } else if (!tuplets.empty()) {
        select_tuplet_start(score, tuplets.front());
    }

    if (!tuplets.empty()) {
        score->input_state().set_duration(tuplets.front()->base_len());
    }

    return tuplets;
}
