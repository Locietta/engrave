/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "noterenderer.h"

#include "dom/arpeggio.h"
#include "dom/note.h"
#include "dom/staff.h"
#include "dom/swing.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "glissandosrenderer.h"
#include "playback/metaparsers/chordarticulationsparser.h"
#include "playback/metaparsers/notearticulationsparser.h"
#include "playback/utils/repeatutils.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

bool NoteRenderer::should_render(const Note *note, const RenderingContext &ctx, const muse::mpe::ArticulationMap &articulations) {
    if (!note->play()) {
        return false;
    }

    const Tie *tie = note->tie_back();

    if (tie && tie->play_spanner()) {
        const Note *start_note = tie->start_note();
        const Note *end_note = tie->end_note();
        if (!start_note || !end_note) {
            return false;
        }

        if (tie->is_partial_tie()) {
            // Play the partially tied note if there is no outgoing note in the previous repeat
            if (!find_outgoing_note_in_previous_repeat(note, ctx).is_valid()) {
                return true;
            }
        }

        const Chord *start_chord = start_note->chord();
        const Chord *end_chord = end_note->chord();

        // Helper function to check if tremolo should play
        auto should_tremolo_play = [](const Chord *chord) -> bool {
            if (chord->tremolo_type() == TremoloType::INVALID_TREMOLO) {
                return false;
            }

            const TremoloSingleChord *single_tremolo = chord->tremolo_single_chord();
            if (single_tremolo) {
                return single_tremolo->play_tremolo();
            }

            const TremoloTwoChord *two_tremolo = chord->tremolo_two_chord();
            if (two_tremolo) {
                return two_tremolo->play_tremolo();
            }

            return false;
        };

        // Only render tied notes if tremolo is actually enabled for playback
        if (should_tremolo_play(start_chord) || should_tremolo_play(end_chord)) {
            return true;
        }

        if (start_chord->arpeggio() && end_chord->arpeggio()) {
            return false;
        }

        //!Note Checking whether the tied note has any multi-note articulation attached
        //!     If so, we can't ignore such note
        for (const auto &pair : articulations) {
            if (muse::mpe::is_multi_note_articulation(pair.first) && !muse::mpe::is_ranged_articulation(pair.first)) {
                return true;
            }
        }

        const auto &intervals =
            start_chord->score()->spanner_map().find_overlapping(start_chord->tick().ticks(), start_chord->end_tick().ticks(),
                                                                 /*excludeCollisions*/ true);
        for (const auto &interval : intervals) {
            const Spanner *sp = interval.value;
            if (sp->is_trill() && sp->play_spanner() && sp->end_element() == start_chord) {
                return true;
            }
        }

        return false;
    }

    return true;
}

void NoteRenderer::render(const Note *note, const RenderingContext &ctx, mpe::PlaybackEventList &result) {
    IF_ASSERT_FAILED(note) { return; }

    NominalNoteCtx note_ctx = build_nominal_note_ctx(note, ctx);
    if (!should_render(note, ctx, note_ctx.articulations)) {
        return;
    }

    const Tie *tie_for = note->tie_for();
    if (tie_for && tie_for->play_spanner()) {
        if (tie_for->is_partial_tie()) {
            render_partial_tie(note, note_ctx);
        } else if (!tie_for->is_laissez_vib()) {
            render_normal_tie(note, note_ctx);
        }
    }

    apply_swing_if_need(note, note_ctx);

    if (note_ctx.articulations.contains(ArticulationType::DISCRETE_GLISSANDO)) {
        GlissandosRenderer::render_discrete_glissando(note, note_ctx, result);
        return;
    }

    if (note_ctx.articulations.contains(ArticulationType::CONTINUOUS_GLISSANDO)) {
        GlissandosRenderer::render_continuous_glissando(note, note_ctx, result);
        return;
    }

    mpe::NoteEvent ev = build_note_event(note_ctx);

    if (ev.arrangement_ctx().actual_timestamp >= 0) {
        result.emplace_back(std::move(ev));
    } else {
        ArrangementContext arr_ctx = ev.arrangement_ctx();
        arr_ctx.actual_duration = arr_ctx.actual_duration + arr_ctx.actual_timestamp;
        arr_ctx.actual_timestamp = 0;

        PitchContext pitch_ctx = ev.pitch_ctx();
        ExpressionContext exp_ctx = ev.expression_ctx();

        result.emplace_back(mpe::NoteEvent(std::move(arr_ctx), std::move(pitch_ctx), std::move(exp_ctx)));
    }
}

void NoteRenderer::render_partial_tie(const Note *outgoing_note, NominalNoteCtx &outgoing_note_ctx) {
    const RenderingContext &outgoing_chord_ctx = outgoing_note_ctx.chord_ctx;
    const PartiallyTiedNoteInfo incoming_note_info = find_incoming_note_in_next_repeat(outgoing_note, outgoing_chord_ctx);
    if (!incoming_note_info.is_valid()) {
        return;
    }

    const int incoming_note_position_tick_offset = incoming_note_info.repeat->utick - incoming_note_info.repeat->tick;
    RenderingContext incoming_chord_ctx = build_rendering_ctx(incoming_note_info.note->chord(), incoming_note_position_tick_offset,
                                                              outgoing_chord_ctx.profile, outgoing_chord_ctx.playback_ctx);

    ChordArticulationsParser::build_chord_articulation_map(incoming_note_info.note->chord(), incoming_chord_ctx,
                                                           incoming_chord_ctx.common_articulations);

    NominalNoteCtx incoming_note_ctx = build_nominal_note_ctx(incoming_note_info.note, incoming_chord_ctx);

    if (should_render(incoming_note_info.note, incoming_chord_ctx, incoming_note_ctx.articulations)) {
        return;
    }

    const Tie *tie_for = incoming_note_info.note->tie_for();
    if (tie_for && tie_for->play_spanner() && !tie_for->is_laissez_vib()) {
        render_normal_tie(incoming_note_info.note, incoming_note_ctx);
    }

    add_tied_note(incoming_note_ctx, outgoing_note_ctx);
    update_articulation_boundaries(outgoing_note_ctx.timestamp, outgoing_note_ctx.duration, outgoing_note_ctx.articulations);
}

void NoteRenderer::render_normal_tie(const Note *first_note, NominalNoteCtx &first_note_ctx) {
    std::unordered_set<const Note *> rendered_notes{first_note};

    const RenderingContext &first_chord_ctx = first_note_ctx.chord_ctx;
    const Tie *curr_tie = first_note->tie_for();

    while (curr_tie && curr_tie->play_spanner()) {
        const Note *curr_note = curr_tie->end_note();
        if (!curr_note || !curr_note->play()) {
            break;
        }

        if (muse::contains(rendered_notes, curr_note)) {
            break; // prevents infinite loop
        }

        if (!notes_in_same_repeat(first_chord_ctx.score, first_note, curr_note, first_chord_ctx.position_tick_offset)) {
            const TieJumpPointList *jump_points = first_note->tie_jump_points();
            if (jump_points && !jump_points->empty()) {
                render_partial_tie(first_note, first_note_ctx);
            }
            break;
        }

        const Chord *chord = curr_note->chord();
        if (!chord) {
            break;
        }

        RenderingContext curr_chord_ctx =
            build_rendering_ctx(chord, first_chord_ctx.position_tick_offset, first_chord_ctx.profile, first_chord_ctx.playback_ctx);
        ChordArticulationsParser::build_chord_articulation_map(chord, curr_chord_ctx, curr_chord_ctx.common_articulations);

        const NominalNoteCtx curr_note_ctx = build_nominal_note_ctx(curr_note, curr_chord_ctx);
        if (should_render(curr_note, curr_chord_ctx, curr_note_ctx.articulations)) {
            if (curr_note_ctx.articulations.contains(ArticulationType::DISCRETE_GLISSANDO)) {
                first_note_ctx.duration += GlissandosRenderer::discrete_glissando_step_duration(curr_note, curr_note_ctx.duration);
            }

            break;
        }

        add_tied_note(curr_note_ctx, first_note_ctx);

        curr_tie = curr_note->tie_for();
        rendered_notes.insert(curr_note);
    }

    if (first_note_ctx.articulations.size() > 1) {
        first_note_ctx.articulations.erase(mpe::ArticulationType::STANDARD);
    }

    update_articulation_boundaries(first_note_ctx.timestamp, first_note_ctx.duration, first_note_ctx.articulations);
}

void NoteRenderer::add_tied_note(const NominalNoteCtx &tied_note_ctx, NominalNoteCtx &first_note_ctx) {
    if (tied_note_ctx.articulations.size() == 1) {
        if (tied_note_ctx.articulations.begin()->first == mpe::ArticulationType::STANDARD) {
            first_note_ctx.duration += tied_note_ctx.duration;
            return;
        }
    }

    const float avg_duration_factor = percentage_to_factor(tied_note_ctx.articulations.average_duration_factor());
    first_note_ctx.duration += tied_note_ctx.duration * avg_duration_factor;

    // Ignore these articulations so we won't re-apply them to the total duration
    static const ArticulationTypeSet articulation_to_ignore_types{
        ArticulationType::STACCATO,
        ArticulationType::STACCATISSIMO,
    };

    for (const auto &pair : tied_note_ctx.articulations) {
        if (!muse::contains(articulation_to_ignore_types, pair.first)) {
            first_note_ctx.articulations.insert(pair);
        }
    }
}

void NoteRenderer::update_articulation_boundaries(const timestamp_t note_timestamp, const duration_t note_duration,
                                                  ArticulationMap &articulations) {
    const timestamp_t note_timestamp_to = note_timestamp + note_duration;
    IF_ASSERT_FAILED(note_timestamp_to > 0) { return; }

    for (const auto &pair : articulations) {
        const ArticulationAppliedData &articulation = pair.second;

        const duration_percentage_t occupied_from = mpe::occupied_percentage(articulation.meta.timestamp, note_timestamp_to);
        const duration_percentage_t occupied_to =
            mpe::occupied_percentage(articulation.meta.timestamp + articulation.meta.overall_duration, note_timestamp_to);

        articulations.update_occupied_range(pair.first, occupied_from, occupied_to);
    }
}

void NoteRenderer::apply_swing_if_need(const Note *note, NominalNoteCtx &note_ctx) {
    const Chord *chord = note->chord();
    if (!chord || chord->tuplet()) {
        return;
    }

    const SwingParameters swing = chord->staff()->swing(chord->tick());
    if (!swing.is_on()) {
        return;
    }

    //! NOTE: Swing must be applied to the "raw" note duration, but not to the additional duration (e.g, from a tied note)
    const Swing::ChordDurationAdjustment swing_duration_adjustment = Swing::apply_swing(chord, swing);
    const duration_t additional_duration = note_ctx.duration - note_ctx.chord_ctx.nominal_duration;
    note_ctx.timestamp = note_ctx.timestamp + note_ctx.chord_ctx.nominal_duration * swing_duration_adjustment.remaining_duration_multiplier;
    note_ctx.duration = note_ctx.chord_ctx.nominal_duration * swing_duration_adjustment.duration_multiplier + additional_duration;
}

NominalNoteCtx NoteRenderer::build_nominal_note_ctx(const Note *note, const RenderingContext &ctx) {
    NominalNoteCtx note_ctx(note, ctx);
    NoteArticulationsParser::build_note_articulation_map(note, ctx, note_ctx.articulations);

    return note_ctx;
}
