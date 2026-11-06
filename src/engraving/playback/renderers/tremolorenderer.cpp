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

#include "log.h"
#include "tremolorenderer.h"

#include "dom/chord.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"

#include "playback/metaparsers/notearticulationsparser.h"
#include "playback/utils/expressionutils.h"
#include "playback/utils/repeatutils.h"

#include "noterenderer.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static bool contain_equal_tremolo(const Chord *c1, const Chord *c2) {
    if (c1->tremolo_single_chord() && c2->tremolo_single_chord()) {
        return c1->tremolo_single_chord()->tremolo_type() == c2->tremolo_single_chord()->tremolo_type();
    }

    if (c1->tremolo_two_chord() && c2->tremolo_two_chord()) {
        return c1->tremolo_two_chord()->tremolo_type() == c2->tremolo_two_chord()->tremolo_type();
    }

    return false;
}

static timestamp_t tremolo_timestamp_from(const Note *note, const RenderingContext &ctx) {
    const Note *original_note = note;
    std::unordered_set<const Note *> notes{note};
    int position_tick_offset = ctx.position_tick_offset;

    if (note->incoming_partial_tie()) {
        const PartiallyTiedNoteInfo outgoing_note_info = find_outgoing_note_in_previous_repeat(note, ctx);
        if (outgoing_note_info.is_valid() && contain_equal_tremolo(outgoing_note_info.note->chord(), note->chord())) {
            note = outgoing_note_info.note;
            position_tick_offset = outgoing_note_info.repeat->utick - outgoing_note_info.repeat->tick;
        }
    }

    while (note && note->tie_back_non_partial() && note->tie_back_non_partial()->play_spanner()) {
        const Note *prev_note = note->tie_back_non_partial()->start_note();
        const Chord *prev_chord = prev_note ? prev_note->chord() : nullptr;

        if (!prev_chord || !contain_equal_tremolo(prev_chord, note->chord())) {
            break;
        }

        if (muse::contains(notes, prev_note)) {
            break; // prevents infinite loop
        }

        note = prev_note;
        notes.insert(note);
    }

    if (note && original_note != note) {
        return timestamp_from_ticks(ctx.score, note->tick().ticks() + position_tick_offset);
    }

    return ctx.nominal_timestamp;
}

static duration_t tremolo_duration(const Note *note, const timestamp_t tremolo_timestamp_from, const RenderingContext &ctx) {
    std::unordered_set<const Note *> notes{note};
    int position_tick_offset = ctx.position_tick_offset;

    if (note->outgoing_partial_tie()) {
        const PartiallyTiedNoteInfo incoming_note_info = find_incoming_note_in_next_repeat(note, ctx);
        if (incoming_note_info.is_valid() && contain_equal_tremolo(incoming_note_info.note->chord(), note->chord())) {
            note = incoming_note_info.note;
            position_tick_offset = incoming_note_info.repeat->utick - incoming_note_info.repeat->tick;
        }
    }

    while (note && note->tie_for_non_partial() && note->tie_for_non_partial()->play_spanner()) {
        const Note *next_note = note->tie_for_non_partial()->end_note();
        const Chord *next_chord = next_note ? next_note->chord() : nullptr;

        if (!next_chord || !contain_equal_tremolo(next_chord, note->chord())) {
            break;
        }

        if (muse::contains(notes, next_note)) {
            break; // prevents infinite loop
        }

        note = next_note;
        notes.insert(note);
    }

    if (note) {
        const int tick_to = note->chord()->end_tick().ticks();
        const timestamp_t tremolo_timestamp_to = timestamp_from_ticks(ctx.score, tick_to + position_tick_offset);
        return tremolo_timestamp_to - tremolo_timestamp_from;
    }

    return ctx.nominal_duration;
}

const ArticulationTypeSet &TremoloRenderer::supported_types() {
    static const mpe::ArticulationTypeSet types = {
        mpe::ArticulationType::TREMOLO8TH,  mpe::ArticulationType::TREMOLO16TH,  mpe::ArticulationType::TREMOLO32ND,
        mpe::ArticulationType::TREMOLO64TH, mpe::ArticulationType::TREMOLO_BUZZ,
    };

    return types;
}

void TremoloRenderer::do_render(const EngravingItem *item, const mpe::ArticulationType preferred_type, const RenderingContext &ctx,
                                mpe::PlaybackEventList &result) {
    const Chord *chord = item_cast<const Chord *>(item);
    IF_ASSERT_FAILED(chord) { return; }

    struct TremoloAdapter {
        const TremoloSingleChord *single = nullptr;
        const TremoloTwoChord *two = nullptr;

        TremoloAdapter(const Chord *ch) : single(ch->tremolo_single_chord()), two(ch->tremolo_two_chord()) {}

        bool has_tremolo() const { return single != nullptr || two != nullptr; }

        int lines() const { return single ? single->lines() : (two ? two->lines() : 0); }
    };

    TremoloAdapter tremolo = TremoloAdapter(chord);
    IF_ASSERT_FAILED(tremolo.has_tremolo()) { return; }

    // TODO: We need a member like articulationData.overallDurationTicks (ticks rather than duration),
    // so that we are not duplicating this calculation (see TremoloTwoMetaParser::doParse)
    //const ArticulationAppliedData& articulationData = context.commonArticulations.at(preferredType);
    int overall_duration_ticks = ctx.nominal_duration_ticks;
    if (tremolo.two && tremolo.two->chord1() && tremolo.two->chord2()) {
        overall_duration_ticks = tremolo.two->chord1()->actual_ticks().ticks() + tremolo.two->chord2()->actual_ticks().ticks();
    }

    int step_duration_ticks = 0;
    if (preferred_type == ArticulationType::TREMOLO_BUZZ) {
        step_duration_ticks = overall_duration_ticks;
    } else {
        step_duration_ticks = TremoloRenderer::step_duration_ticks(chord, tremolo.lines());
    }

    if (step_duration_ticks <= 0) {
        LOGE() << "Unable to render unsupported tremolo type";
        return;
    }

    // ... and use that here
    const int steps_count = std::round(overall_duration_ticks / (float) step_duration_ticks);
    if (steps_count == 0) {
        return;
    }

    step_duration_ticks = overall_duration_ticks / steps_count;

    TremoloTimeCache tremolo_time_cache;

    if (tremolo.two) {
        const Chord *first_tremolo_chord = tremolo.two->chord1();
        const Chord *second_tremolo_chord = tremolo.two->chord2();

        IF_ASSERT_FAILED(first_tremolo_chord && second_tremolo_chord) { return; }

        for (int i = 0; i < steps_count; ++i) {
            const Chord *current_chord = first_tremolo_chord;

            if (i % 2 != 0) {
                current_chord = second_tremolo_chord;
            }

            build_and_append_events(current_chord, preferred_type, step_duration_ticks,
                                    ctx.nominal_position_start_tick + i * step_duration_ticks, ctx, tremolo_time_cache, result);
        }

        return;
    }

    for (int i = 0; i < steps_count; ++i) {
        build_and_append_events(chord, preferred_type, step_duration_ticks, ctx.nominal_position_start_tick + i * step_duration_ticks, ctx,
                                tremolo_time_cache, result);
    }
}

int TremoloRenderer::step_duration_ticks(const Chord *chord, int tremolo_lines) {
    int ticks = Constants::division / (1 << (chord->beams() + tremolo_lines));
    if (ticks <= 0) {
        return 1;
    }
    return ticks * chord->time_stretch_factor();
}

void TremoloRenderer::build_and_append_events(const Chord *chord, const ArticulationType type, const int step_duration_ticks,
                                              const int start_tick, const RenderingContext &ctx, TremoloTimeCache &tremolo_cache,
                                              mpe::PlaybackEventList &result) {
    for (const Note *note : chord->notes()) {
        if (!NoteRenderer::should_render(note, ctx, ctx.common_articulations)) {
            continue;
        }

        auto note_tn_d =
            timestamp_and_duration_from_start_and_duration_ticks(ctx.score, start_tick, step_duration_ticks, ctx.position_tick_offset);

        NominalNoteCtx note_ctx(note, ctx);
        note_ctx.duration = note_tn_d.duration;
        note_ctx.timestamp = note_tn_d.timestamp;

        int utick = timestamp_to_tick(ctx.score, note_ctx.timestamp);
        note_ctx.dynamic_level = ctx.playback_ctx->appliable_dynamic_level(note->track(), utick);

        NoteArticulationsParser::build_note_articulation_map(note, ctx, note_ctx.articulations);

        const TimestampAndDuration &tremolo_tn_d = tremolo_time_and_duration(note, ctx, tremolo_cache);
        muse::mpe::ArticulationAppliedData &articulation_data = note_ctx.articulations.at(type);
        articulation_data.meta.timestamp = tremolo_tn_d.timestamp;
        articulation_data.meta.overall_duration = tremolo_tn_d.duration;

        update_articulation_boundaries(type, note_ctx.timestamp, note_ctx.duration, note_ctx.articulations);

        result.emplace_back(build_note_event(note_ctx));
    }
}

const TimestampAndDuration &TremoloRenderer::tremolo_time_and_duration(const Note *note, const RenderingContext &ctx,
                                                                       TremoloTimeCache &cache) {
    auto cache_it = cache.find(note);
    if (cache_it != cache.end()) {
        return cache_it->second;
    }

    TimestampAndDuration &tnd = cache[note];
    tnd.timestamp = tremolo_timestamp_from(note, ctx);
    tnd.duration = tremolo_duration(note, tnd.timestamp, ctx);

    return tnd;
}
