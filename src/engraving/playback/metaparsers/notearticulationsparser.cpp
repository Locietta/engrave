/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "notearticulationsparser.h"

#include "dom/laissezvib.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/spanner.h"
#include "internal/spannersmetaparser.h"
#include "internal/symbolsmetaparser.h"
#include "log.h"
#include "playback/utils/arrangementutils.h"
#include "playback/utils/expressionutils.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

void NoteArticulationsParser::build_note_articulation_map(const Note *note, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    if (!note || !ctx.is_valid()) {
        LOGE() << "Unable to render playback events of invalid note";
        return;
    }

    parse(note, ctx, result);

    if (result.empty()) {
        append_articulation_data({mpe::ArticulationType::STANDARD, ctx.profile->pattern(mpe::ArticulationType::STANDARD),
                                  ctx.nominal_timestamp, ctx.nominal_duration, 0, 0},
                                 result);
    }

    result.pre_calculate_average_data();
}

void NoteArticulationsParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->is_note()) { return; }

    const Note *note = to_note(item);

    if (!note || !note->play()) {
        return;
    }

    parse_playing_technique(ctx, note->track(), result);
    parse_ghost_note(note, ctx, result);
    parse_note_head(note, ctx, result);
    parse_symbols(note, ctx, result);
    parse_laissez_vibrer(note, ctx, result);
    parse_spanners(note, ctx, result);
}

ArticulationType NoteArticulationsParser::articulation_type_by_notehead_group(const NoteHeadGroup notehead_group) {
    switch (notehead_group) {
        case NoteHeadGroup::HEAD_CROSS: return mpe::ArticulationType::CROSS_NOTE;

        case NoteHeadGroup::HEAD_CIRCLED_LARGE:
        case NoteHeadGroup::HEAD_CIRCLED: return mpe::ArticulationType::CIRCLE_NOTE;

        case NoteHeadGroup::HEAD_XCIRCLE: return mpe::ArticulationType::CIRCLE_CROSS_NOTE;

        case NoteHeadGroup::HEAD_TRIANGLE_DOWN: return mpe::ArticulationType::TRIANGLE_DOWN_NOTE;
        case NoteHeadGroup::HEAD_TRIANGLE_UP: return mpe::ArticulationType::TRIANGLE_UP_NOTE;

        case NoteHeadGroup::HEAD_DIAMOND:
        case NoteHeadGroup::HEAD_DIAMOND_OLD: return mpe::ArticulationType::DIAMOND_NOTE;

        case NoteHeadGroup::HEAD_PLUS: return mpe::ArticulationType::PLUS_NOTE;

        case NoteHeadGroup::HEAD_SLASH: return mpe::ArticulationType::SLASH_NOTE;

        case NoteHeadGroup::HEAD_SLASHED1: return mpe::ArticulationType::SLASHED_FORWARDS_NOTE;

        case NoteHeadGroup::HEAD_SLASHED2: return mpe::ArticulationType::SLASHED_BACKWARDS_NOTE;

        case NoteHeadGroup::HEAD_DO: return mpe::ArticulationType::TRIANGLE_UP_NOTE;

        case NoteHeadGroup::HEAD_RE: return mpe::ArticulationType::MOON_NOTE;

        case NoteHeadGroup::HEAD_FA: return mpe::ArticulationType::TRIANGLE_RIGHT_NOTE;

        case NoteHeadGroup::HEAD_LA: return mpe::ArticulationType::SQUARE_NOTE;

        case NoteHeadGroup::HEAD_TI: return mpe::ArticulationType::TRIANGLE_ROUND_DOWN_NOTE;

        default: return mpe::ArticulationType::UNDEFINED;
    }
}

void NoteArticulationsParser::parse_playing_technique(const RenderingContext &ctx, const track_idx_t track_idx,
                                                      mpe::ArticulationMap &result, bool sustain_allowed) {
    const int chord_pos_tick_with_offset = ctx.nominal_position_start_tick + ctx.position_tick_offset;
    const std::pair<timestamp_t, PlayingTechniqueType> tech = ctx.playback_ctx->playing_technique(track_idx, chord_pos_tick_with_offset);
    if (tech.second == PlayingTechniqueType::HANDBELLS_LV && !sustain_allowed) {
        return;
    }

    const mpe::ArticulationType articulation_type = articulation_from_play_tech_type(tech.second);
    if (articulation_type == ArticulationType::STANDARD || articulation_type == ArticulationType::UNDEFINED) {
        return;
    }

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(articulation_type);
    if (pattern.empty()) {
        return;
    }

    timestamp_t timestamp = ctx.nominal_timestamp;
    duration_t duration = ctx.nominal_duration;

    if (tech.second == PlayingTechniqueType::HANDBELLS_LV) {
        const timestamp_t damp_time =
            ctx.playback_ctx->find_playing_technique_timestamp(track_idx, PlayingTechniqueType::HANDBELLS_DAMP, chord_pos_tick_with_offset);
        timestamp = tech.first;
        duration = damp_time > 0 ? damp_time - timestamp : mpe::infinite_duration;
    }

    append_articulation_data({articulation_type, pattern, timestamp, duration, 0, 0}, result);
}

void NoteArticulationsParser::parse_ghost_note(const Note *note, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    if (!note->ghost() && !note->parenthesis_info()) {
        return;
    }

    append_articulations({mpe::ArticulationType::GHOST_NOTE}, ctx, result);
}

void NoteArticulationsParser::parse_note_head(const Note *note, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    mpe::ArticulationType type_by_note_head_group = articulation_type_by_notehead_group(note->head_group());

    if (type_by_note_head_group != mpe::ArticulationType::UNDEFINED) {
        append_articulations({type_by_note_head_group}, ctx, result);
    } else if (note->ldata()->cached_notehead_sym.has_value()) {
        SymId sym_id = note->ldata()->cached_notehead_sym.value(); // fastest way to get the notehead symbol
        ArticulationTypeSet types = SymbolsMetaParser::symbol_to_articulations(sym_id);
        append_articulations(types, ctx, result);
    }
}

void NoteArticulationsParser::parse_symbols(const Note *note, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    for (const EngravingItem *item : note->el()) {
        if (item && item->is_symbol()) {
            ArticulationTypeSet types = SymbolsMetaParser::symbol_to_articulations(to_symbol(item)->sym());
            append_articulations(types, ctx, result);
        }
    }
}

void NoteArticulationsParser::parse_laissez_vibrer(const Note *note, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    const LaissezVib *laissez_vib = note->laissez_vib();
    if (!laissez_vib || !laissez_vib->play_spanner()) {
        return;
    }

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(mpe::ArticulationType::LAISSEZ_VIBRER);
    if (pattern.empty()) {
        return;
    }

    const Measure *note_measure = note->find_measure();
    if (!note_measure) {
        return;
    }

    const Measure *next_measure = note_measure->next_measure();
    const Fraction end_tick = next_measure ? next_measure->end_tick() : note_measure->end_tick();
    const timestamp_t end_time = timestamp_from_ticks(ctx.score, end_tick.ticks() + ctx.position_tick_offset);

    append_articulation_data(
        mpe::ArticulationMeta(mpe::ArticulationType::LAISSEZ_VIBRER, pattern, ctx.nominal_timestamp, end_time - ctx.nominal_timestamp),
        result);
}

void NoteArticulationsParser::parse_spanners(const Note *note, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    for (const Spanner *spanner : note->spanner_for()) {
        int spanner_from = spanner->tick().ticks();
        int spanner_to = spanner_from + std::abs(spanner->ticks().ticks());
        int spanner_duration_ticks = spanner_to - spanner_from;

        if (spanner_duration_ticks == 0) {
            spanner_duration_ticks = ctx.nominal_duration_ticks;
        }

        auto spanner_tn_d =
            timestamp_and_duration_from_start_and_duration_ticks(ctx.score, spanner_from, spanner_duration_ticks, ctx.position_tick_offset);

        RenderingContext spanner_context = ctx;
        spanner_context.nominal_timestamp = spanner_tn_d.timestamp;
        spanner_context.nominal_duration = spanner_tn_d.duration;
        spanner_context.nominal_position_start_tick = spanner_from;
        spanner_context.nominal_duration_ticks = spanner_duration_ticks;

        SpannersMetaParser::parse(spanner, spanner_context, result);
    }
}

void NoteArticulationsParser::append_articulations(const mpe::ArticulationTypeSet &types, const RenderingContext &ctx,
                                                   mpe::ArticulationMap &result) {
    for (mpe::ArticulationType type : types) {
        if (type == mpe::ArticulationType::UNDEFINED) {
            continue;
        }

        const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type);
        if (pattern.empty()) {
            continue;
        }

        append_articulation_data(mpe::ArticulationMeta(type, pattern, ctx.nominal_timestamp, ctx.nominal_duration), result);
    }
}
