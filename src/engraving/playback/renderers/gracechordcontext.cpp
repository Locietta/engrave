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

#include "gracechordcontext.h"

#include "playback/utils/expressionutils.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static duration_t grace_notes_total_duration(const std::vector<Chord *> &grace_chords, const BeatsPerSecond &bps) {
    duration_t result = 0;

    for (const Chord *grace_chord : grace_chords) {
        result += duration_from_tempo_and_ticks(bps.val, grace_chord->duration_type_ticks().ticks());
    }

    return result;
}

static duration_t grace_notes_max_available_duration(const ArticulationType type, const RenderingContext &ctx,
                                                     const size_t grace_notes_count) {
    const duration_t halved_duration = 0.5 * ctx.nominal_duration;
    const duration_t two_thirds_duration = (2 * ctx.nominal_duration) / 3;

    if ((type == ArticulationType::POST_APPOGGIATURA || (type == ArticulationType::PRE_APPOGGIATURA && grace_notes_count == 1))) {
        if (ctx.time_signature_fraction.is_compound() && ctx.nominal_duration_ticks > ::mu::engraving::quaver_ticks) {
            return two_thirds_duration;
        } else {
            return halved_duration;
        }
    }

    const duration_t min_acciacatura_duration =
        duration_from_tempo_and_ticks(ctx.beats_per_second.val, ::mu::engraving::demisemiquaver_ticks / 2);
    return std::min(min_acciacatura_duration * static_cast<duration_t>(grace_notes_count), halved_duration);
}

static timestamp_t grace_notes_start_timestamp(const mpe::ArticulationType type, const mpe::duration_t available_duration,
                                               const mpe::timestamp_t nominal_timestamp) {
    if (type == ArticulationType::POST_APPOGGIATURA) {
        return nominal_timestamp - available_duration;
    }

    return nominal_timestamp;
}

static timestamp_t principal_notes_start_timestamp(const mpe::ArticulationType type, const mpe::duration_t grace_notes_duration,
                                                   const mpe::timestamp_t nominal_timestamp) {
    if (type == ArticulationType::PRE_APPOGGIATURA || type == ArticulationType::ACCIACCATURA) {
        return nominal_timestamp + grace_notes_duration;
    }

    return nominal_timestamp;
}

static RenderingContext build_grace_rendering_ctx(const RenderingContext &base_ctx, const timestamp_t timestamp,
                                                  const duration_t duration) {
    RenderingContext result(base_ctx);
    result.nominal_timestamp = timestamp;
    result.nominal_duration = duration;
    result.nominal_position_start_tick = timestamp_to_tick(result.score, timestamp) - result.position_tick_offset;
    result.nominal_position_end_tick = timestamp_to_tick(result.score, timestamp + duration) - result.position_tick_offset;
    result.nominal_duration_ticks = result.nominal_position_end_tick - result.nominal_position_start_tick;

    return result;
}

using GraceChordCtxList = std::vector<std::pair<const Chord *, RenderingContext>>;

static void append_grace_chord_ctxs(const std::vector<Chord *> &grace_chords, const double duration_factor, timestamp_t timestamp,
                                    const RenderingContext &ctx, GraceChordCtxList &result) {
    for (const Chord *grace_chord : grace_chords) {
        const int duration_ticks = grace_chord->duration_type_ticks().ticks();
        const duration_t duration =
            muse::real_round(duration_factor * duration_from_tempo_and_ticks(ctx.beats_per_second.val, duration_ticks), 0);

        RenderingContext grace_note_ctx = build_grace_rendering_ctx(ctx, timestamp, duration);
        result.emplace_back(std::make_pair(grace_chord, grace_note_ctx));

        timestamp += duration;
    }
}

//! Graces before and after one principal: each group takes its own rule's duration; if together they exceed half
//! the principal, both shrink proportionally to fill exactly half. The principal keeps the rest.
static GraceChordCtx build_two_group_ctx(const Chord *chord, const ArticulationType before_type, const std::vector<Chord *> &after_chords,
                                         const RenderingContext &ctx) {
    const std::vector<Chord *> before_chords = chord->grace_notes_before(true /*filterUnplayable*/);

    const duration_t before_notated = grace_notes_total_duration(before_chords, ctx.beats_per_second);
    const duration_t after_notated = grace_notes_total_duration(after_chords, ctx.beats_per_second);
    duration_t before_duration = std::min(grace_notes_max_available_duration(before_type, ctx, before_chords.size()), before_notated);
    duration_t after_duration =
        std::min(grace_notes_max_available_duration(ArticulationType::POST_APPOGGIATURA, ctx, after_chords.size()), after_notated);

    const duration_t halved_duration = 0.5 * ctx.nominal_duration;
    if (before_duration + after_duration > halved_duration) {
        before_duration = muse::real_round(static_cast<double>(before_duration) * halved_duration / (before_duration + after_duration), 0);
        after_duration = halved_duration - before_duration;
    }

    GraceChordCtx result{
        build_grace_rendering_ctx(ctx, ctx.nominal_timestamp + before_duration, ctx.nominal_duration - before_duration - after_duration),
        {},
        {}};
    result.principal_chord_ctx.common_articulations.erase(before_type);
    result.principal_chord_ctx.common_articulations.erase(ArticulationType::POST_APPOGGIATURA);

    append_grace_chord_ctxs(before_chords, double(before_duration) / before_notated, ctx.nominal_timestamp, ctx,
                            result.grace_chord_ctx_list);
    append_grace_chord_ctxs(after_chords, double(after_duration) / after_notated,
                            ctx.nominal_timestamp + ctx.nominal_duration - after_duration, ctx, result.after_grace_chord_ctx_list);

    return result;
}

GraceChordCtx GraceChordCtx::build_ctx(const Chord *chord, const mpe::ArticulationType type, const RenderingContext &ctx) {
    if (is_grace_note_placed_before_principal_note(type) && ctx.common_articulations.contains(ArticulationType::POST_APPOGGIATURA)) {
        const std::vector<Chord *> after_chords = chord->grace_notes_after(true /*filterUnplayable*/);
        if (!after_chords.empty()) {
            return build_two_group_ctx(chord, type, after_chords, ctx);
        }
    }

    std::vector<Chord *> grace_chords;

    const bool is_placed_before_principal = is_grace_note_placed_before_principal_note(type);
    if (is_placed_before_principal) {
        grace_chords = chord->grace_notes_before(true /*filterUnplayable*/);
    } else {
        grace_chords = chord->grace_notes_after(true /*filterUnplayable*/);
    }

    const duration_t available_grace_notes_duration = grace_notes_max_available_duration(type, ctx, grace_chords.size());
    const duration_t accumulated_grace_notes_duration = grace_notes_total_duration(grace_chords, ctx.beats_per_second);
    const duration_t actual_grace_notes_duration = std::min(available_grace_notes_duration, accumulated_grace_notes_duration);
    const timestamp_t principal_chord_timestamp = principal_notes_start_timestamp(type, actual_grace_notes_duration, ctx.nominal_timestamp);
    const duration_t principal_chord_duration = ctx.nominal_duration - actual_grace_notes_duration;

    GraceChordCtx result{build_grace_rendering_ctx(ctx, principal_chord_timestamp, principal_chord_duration), {}, {}};
    result.principal_chord_ctx.common_articulations.erase(type);

    const double grace_notes_duration_factor = double(actual_grace_notes_duration) / accumulated_grace_notes_duration;
    const timestamp_t grace_notes_time_offset = is_placed_before_principal ? 0 : ctx.nominal_duration;

    const timestamp_t grace_chord_timestamp =
        grace_notes_start_timestamp(type, actual_grace_notes_duration, ctx.nominal_timestamp + grace_notes_time_offset);
    append_grace_chord_ctxs(grace_chords, grace_notes_duration_factor, grace_chord_timestamp, ctx, result.grace_chord_ctx_list);

    return result;
}
