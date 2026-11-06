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

#include "chordarticulationsparser.h"

#include "dom/arpeggio.h"
#include "dom/breath.h"
#include "dom/chord.h"
#include "dom/chordline.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/spanner.h"
#include "dom/tapping.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "internal/annotationsmetaparser.h"
#include "internal/arpeggiometaparser.h"
#include "internal/chordlinemetaparser.h"
#include "internal/gracenotesmetaparser.h"
#include "internal/spannersmetaparser.h"
#include "internal/symbolsmetaparser.h"
#include "internal/tremolometaparser.h"
#include "log.h"
#include "playback/filters/spannerfilter.h"
#include "playback/utils/arrangementutils.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

void ChordArticulationsParser::build_chord_articulation_map(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    if (!chord || !ctx.is_valid()) {
        LOGE() << "Unable to render playback events of invalid chord";
        return;
    }

    parse(chord, ctx, result);

    for (const auto &pair : result) {
        if (is_single_note_articulation(pair.first)) {
            continue;
        }

        duration_percentage_t occupied_from =
            occupied_percentage(ctx.nominal_timestamp - pair.second.meta.timestamp, pair.second.meta.overall_duration);
        duration_percentage_t occupied_to = occupied_percentage(ctx.nominal_timestamp + ctx.nominal_duration - pair.second.meta.timestamp,
                                                                pair.second.meta.overall_duration);

        result.update_occupied_range(pair.first, occupied_from, occupied_to);
    }

    result.pre_calculate_average_data();
}

void ChordArticulationsParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->is_chord()) { return; }

    const Chord *chord = to_chord(item);

    parse_spanners(chord, ctx, result);
    parse_annotations(chord, ctx, result);
    parse_tremolo(chord, ctx, result);
    parse_arpeggio(chord, ctx, result);
    parse_grace_notes(chord, ctx, result);
    parse_chord_line(chord, ctx, result);
    parse_articulation_symbols(chord, ctx, result);
    parse_tapping(chord, ctx, result);
    parse_breath(chord, ctx, result);
}

void ChordArticulationsParser::parse_spanners(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    const SpannerMap &spanner_map = ctx.score->spanner_map();
    if (spanner_map.empty()) {
        return;
    }

    auto intervals = spanner_map.find_overlapping(ctx.nominal_position_start_tick, ctx.nominal_position_end_tick,
                                                  /*excludeCollisions*/ true);

    for (const auto &interval : intervals) {
        const Spanner *spanner = interval.value;

        if (!SpannersMetaParser::is_able_to_parse(spanner)) {
            continue;
        }

        if (SpannerFilter::is_multi_staff_spanner(spanner)) {
            if (spanner->part() != chord->part()) {
                continue;
            }
        } else {
            if (spanner->staff_idx() != chord->staff_idx()) {
                continue;
            }
        }

        if (!SpannerFilter::is_item_playable(spanner, ctx)) {
            continue;
        }

        RenderingContext spanner_context = ctx;
        spanner_context.nominal_timestamp = timestamp_from_ticks(ctx.score, interval.start + ctx.position_tick_offset);
        spanner_context.nominal_position_start_tick = interval.start;
        spanner_context.nominal_duration_ticks = SpannerFilter::spanner_actual_duration_ticks(spanner, interval.stop - interval.start);
        spanner_context.nominal_position_end_tick = spanner_context.nominal_position_start_tick + spanner_context.nominal_duration_ticks;

        SpannersMetaParser::parse(spanner, spanner_context, result);
    }
}

void ChordArticulationsParser::parse_articulation_symbols(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    for (const Articulation *articulation : chord->articulations()) {
        SymbolsMetaParser::parse(articulation, ctx, result);
    }
}

void ChordArticulationsParser::parse_annotations(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    for (const EngravingItem *annotation : chord->segment()->annotations()) {
        if (annotation->staff_idx() != chord->staff_idx()) {
            continue;
        }

        AnnotationsMetaParser::parse(annotation, ctx, result);
    }
}

void ChordArticulationsParser::parse_tremolo(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    // single chord
    {
        const TremoloSingleChord *tremolo_single = chord->tremolo_single_chord();
        if (tremolo_single && tremolo_single->play_tremolo()) {
            TremoloSingleMetaParser::parse(tremolo_single, ctx, result);
        }
    }

    // two chord
    {
        const TremoloTwoChord *tremolo_two = chord->tremolo_two_chord();
        if (tremolo_two && tremolo_two->play_tremolo()) {
            TremoloTwoMetaParser::parse(tremolo_two, ctx, result);
        }
    }
}

void ChordArticulationsParser::parse_arpeggio(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    const Arpeggio *arpeggio = chord->arpeggio();
    if (!arpeggio || arpeggio->is_chord_bracket()) {
        return;
    }

    if (chord->notes().empty()) {
        return;
    }

    ArpeggioMetaParser::parse(arpeggio, ctx, result);
}

void ChordArticulationsParser::parse_grace_notes(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    for (const Chord *grace_chord : chord->grace_notes()) {
        GraceNotesMetaParser::parse(grace_chord, ctx, result);
    }
}

void ChordArticulationsParser::parse_chord_line(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    const ChordLine *chord_line = chord->chord_line();
    if (!chord_line || !chord_line->play_chord_line()) {
        return;
    }

    ChordLineMetaParser::parse(chord_line, ctx, result);
}

void ChordArticulationsParser::parse_tapping(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    const Tapping *tapping = chord->tapping();
    if (!tapping || !tapping->play_articulation()) {
        return;
    }

    mpe::ArticulationType type = mpe::ArticulationType::UNDEFINED;

    switch (tapping->hand()) {
        case TappingHand::LEFT: type = mpe::ArticulationType::LEFT_HAND_TAPPING; break;
        case TappingHand::RIGHT: type = mpe::ArticulationType::RIGHT_HAND_TAPPING; break;
        case TappingHand::INVALID: break;
    }

    if (type == mpe::ArticulationType::UNDEFINED) {
        return;
    }

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type);
    if (pattern.empty()) {
        return;
    }

    append_articulation_data(mpe::ArticulationMeta(type, pattern, ctx.nominal_timestamp, ctx.nominal_duration), result);
}

void ChordArticulationsParser::parse_breath(const Chord *chord, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(ArticulationType::BREATH);
    if (pattern.empty()) {
        return;
    }

    const Breath *breath = chord->has_breath_mark();
    if (!breath) {
        return;
    }

    const mpe::timestamp_t timestamp = timestamp_from_ticks(ctx.score, breath->tick().ticks() + ctx.position_tick_offset);
    const mpe::duration_t duration = breath->pause() * 1000000;
    append_articulation_data(mpe::ArticulationMeta(ArticulationType::BREATH, pattern, timestamp, duration), result);
}
