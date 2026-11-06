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

#include "dom/measure.h"
#include "dom/note.h"
#include "dom/repeatlist.h"
#include "playback/renderingcontext.h"

namespace mu::engraving {
struct PartiallyTiedNoteInfo {
    const RepeatSegment *repeat = nullptr;
    const Note *note = nullptr;

    bool is_valid() const { return repeat && note; }
};

inline PartiallyTiedNoteInfo find_outgoing_note_in_previous_repeat(const Note *incoming_note, const RenderingContext &ctx) {
    const RepeatList &repeats = ctx.score->repeat_list();
    auto curr_repeat_it = repeats.find_repeat_segment_from_u_tick(ctx.nominal_position_start_tick + ctx.position_tick_offset);
    if (curr_repeat_it == repeats.cbegin() || curr_repeat_it == repeats.cend()) {
        return PartiallyTiedNoteInfo();
    }

    if (ctx.nominal_position_start_tick + ctx.position_tick_offset != (*curr_repeat_it)->utick) {
        return PartiallyTiedNoteInfo(); // the incoming note is not the first note in this repeat
    }

    auto prev_repeat_it = std::prev(curr_repeat_it);
    const RepeatSegment *prev_repeat = (*prev_repeat_it);
    const Measure *last_measure = prev_repeat->last_measure();
    if (!last_measure) {
        return PartiallyTiedNoteInfo();
    }

    const ChordRest *last_chord_rest = last_measure->last_chord_rest(incoming_note->track());
    if (!last_chord_rest || !last_chord_rest->is_chord()) {
        return PartiallyTiedNoteInfo();
    }

    const Note *outgoing_note = to_chord(last_chord_rest)->find_note(incoming_note->pitch());
    const Tie *tie = outgoing_note ? outgoing_note->tie_for() : nullptr;
    if (tie && tie->play_spanner()) {
        return {prev_repeat, outgoing_note};
    }

    return PartiallyTiedNoteInfo();
}

inline PartiallyTiedNoteInfo find_incoming_note_in_next_repeat(const Note *outgoing_note, const RenderingContext &ctx) {
    const RepeatList &repeats = ctx.score->repeat_list();
    auto curr_repeat_it = repeats.find_repeat_segment_from_u_tick(ctx.nominal_position_start_tick + ctx.position_tick_offset);
    if (curr_repeat_it == repeats.cend()) {
        return PartiallyTiedNoteInfo();
    }

    const RepeatSegment *curr_repeat = (*curr_repeat_it);
    const int curr_repeat_last_tick = curr_repeat->utick + curr_repeat->len();
    if (ctx.nominal_position_start_tick + ctx.position_tick_offset != curr_repeat_last_tick - ctx.nominal_duration_ticks) {
        return PartiallyTiedNoteInfo(); // the outgoing note is not the last note in this repeat
    }

    auto next_repeat_it = std::next(curr_repeat_it);
    if (next_repeat_it == repeats.cend()) {
        return PartiallyTiedNoteInfo();
    }

    const RepeatSegment *next_repeat = (*next_repeat_it);
    const Measure *first_measure = next_repeat->first_measure();
    if (!first_measure) {
        return PartiallyTiedNoteInfo();
    }

    const ChordRest *first_chord_rest = first_measure->first_chord_rest(outgoing_note->track());
    if (!first_chord_rest || !first_chord_rest->is_chord()) {
        return PartiallyTiedNoteInfo();
    }

    const Note *incoming_note = to_chord(first_chord_rest)->find_note(outgoing_note->pitch());
    const Tie *tie = incoming_note ? incoming_note->tie_back() : nullptr;
    if (tie && tie->play_spanner()) {
        return {next_repeat, incoming_note};
    }

    return PartiallyTiedNoteInfo();
}

inline bool notes_in_same_repeat(const Score *score, const Note *note1, const Note *note2, const int tick_position_offset) {
    const RepeatList &repeats = score->repeat_list();
    if (repeats.size() == 1) {
        return true;
    }

    const int first_note_tick = note1->tick().ticks();
    auto repeat_it = repeats.find_repeat_segment_from_u_tick(first_note_tick + tick_position_offset);
    if (repeat_it == repeats.end()) {
        return true;
    }

    const RepeatSegment *repeat = (*repeat_it);
    const int second_note_tick = note2->tick().ticks();

    if (repeat->tick > second_note_tick || repeat->end_tick() <= second_note_tick) {
        return false;
    }

    return true;
}
} // namespace mu::engraving
