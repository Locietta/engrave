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

#include "playbackeventsrenderer.h"

#include "metaparsers/notearticulationsparser.h"
#include "playbackeventsrenderer_impl.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

using renderer_detail::make_standard_articulation_map;

void PlaybackEventsRenderer::render_fixed_note_event(const Note *note, const mpe::timestamp_t actual_timestamp,
                                                     const mpe::duration_t actual_duration, const mpe::dynamic_level_t actual_dynamic_level,
                                                     const PlaybackContextPtr playback_ctx, const mpe::ArticulationsProfilePtr profile,
                                                     mpe::PlaybackEventList &result) const {
    static const ArticulationMap articulations;

    const Score *score = note->score();
    const int duration_ticks = ticks_from_tempo_and_duration(Constants::default_tempo.val, actual_duration);
    const int tick = note->tick().ticks();
    const int utick = score ? score->repeat_list().tick2utick(tick) : tick;
    const int tick_offset = utick - tick;

    RenderingContext ctx{actual_timestamp,
                         actual_duration,
                         actual_dynamic_level,
                         tick,           /*nominalPositionStartTick*/
                         duration_ticks, /*nominalPositionEndTick*/
                         duration_ticks, /*nominalDurationTicks*/
                         tick_offset,
                         Constants::default_tempo,
                         TimeSigMap::default_time_signature,
                         articulations,
                         score,
                         profile,
                         playback_ctx};

    NoteArticulationsParser::parse_playing_technique(ctx, note->track(), ctx.common_articulations, false /*sustainAllowed*/);
    NoteArticulationsParser::parse_ghost_note(note, ctx, ctx.common_articulations);
    NoteArticulationsParser::parse_note_head(note, ctx, ctx.common_articulations);
    NoteArticulationsParser::parse_symbols(note, ctx, ctx.common_articulations);

    if (ctx.common_articulations.empty()) {
        ctx.common_articulations = make_standard_articulation_map(profile, actual_timestamp, actual_duration);
    } else {
        ctx.common_articulations.pre_calculate_average_data();
    }

    NominalNoteCtx note_ctx(note, ctx);
    result.emplace_back(build_note_event(note_ctx));
}

// The legacy map sink is the one this unit owns; other sinks instantiate in their own units
ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER(mu::engraving::EventsMapSink);
