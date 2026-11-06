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

#include "tremolometaparser.h"

#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/chord.h"

#include "mpe/mpetypes.h"
#include "../../renderingcontext.h"

using namespace mu::engraving;
using namespace muse;

static mpe::ArticulationType to_articulation_type(TremoloType type) {
    switch (type) {
        case TremoloType::R8:
        case TremoloType::C8: return mpe::ArticulationType::TREMOLO8TH;
        case TremoloType::R16:
        case TremoloType::C16: return mpe::ArticulationType::TREMOLO16TH;
        case TremoloType::R32:
        case TremoloType::C32: return mpe::ArticulationType::TREMOLO32ND;
        case TremoloType::R64:
        case TremoloType::C64:
        // Playback currently caps tremolo subdivisions at 64th; 128th and 256th
        // tremolos intentionally reuse the 64th articulation pattern.
        case TremoloType::R128:
        case TremoloType::C128:
        case TremoloType::R256:
        case TremoloType::C256: return mpe::ArticulationType::TREMOLO64TH;
        case TremoloType::BUZZ_ROLL: return mpe::ArticulationType::TREMOLO_BUZZ;
        case TremoloType::INVALID_TREMOLO: break;
    }

    return mpe::ArticulationType::UNDEFINED;
}

void TremoloSingleMetaParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->type() == ElementType::TREMOLO_SINGLECHORD) { return; }

    const TremoloSingleChord *tremolo = item_cast<const TremoloSingleChord *>(item);

    mpe::ArticulationType type = to_articulation_type(tremolo->tremolo_type());
    if (type == mpe::ArticulationType::UNDEFINED) {
        return;
    }

    int overall_duration_ticks = ctx.nominal_duration_ticks;

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type);
    if (pattern.empty()) {
        return;
    }

    mpe::ArticulationMeta articulation_meta;
    articulation_meta.type = type;
    articulation_meta.pattern = pattern;
    articulation_meta.timestamp = ctx.nominal_timestamp;
    articulation_meta.overall_duration =
        timestamp_from_ticks(ctx.score, ctx.nominal_position_start_tick + overall_duration_ticks) - ctx.nominal_timestamp;

    append_articulation_data(std::move(articulation_meta), result);
}

void TremoloTwoMetaParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->type() == ElementType::TREMOLO_TWOCHORD) { return; }

    const TremoloTwoChord *tremolo = item_cast<const TremoloTwoChord *>(item);

    const Chord *chord2 = tremolo->chord2();
    IF_ASSERT_FAILED(chord2) { return; }

    if (chord2->tick().ticks() == ctx.nominal_position_start_tick) {
        return;
    }

    mpe::ArticulationType type = to_articulation_type(tremolo->tremolo_type());
    if (type == mpe::ArticulationType::UNDEFINED) {
        return;
    }

    int overall_duration_ticks = ctx.nominal_duration_ticks;
    if (tremolo->chord1() && tremolo->chord2()) {
        overall_duration_ticks = tremolo->chord1()->actual_ticks().ticks() + tremolo->chord2()->actual_ticks().ticks();
    }

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type);
    if (pattern.empty()) {
        return;
    }

    mpe::ArticulationMeta articulation_meta;
    articulation_meta.type = type;
    articulation_meta.pattern = pattern;
    articulation_meta.timestamp = ctx.nominal_timestamp;
    articulation_meta.overall_duration =
        timestamp_from_ticks(ctx.score, ctx.nominal_position_start_tick + overall_duration_ticks) - ctx.nominal_timestamp;

    append_articulation_data(std::move(articulation_meta), result);
}
