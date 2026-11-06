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

#include "spannersmetaparser.h"

#include "dom/glissando.h"
#include "dom/note.h"
#include "dom/pedal.h"
#include "dom/spanner.h"
#include "dom/trill.h"
#include "playback/filters/spannerfilter.h"
#include "playback/utils/pitchutils.h"

using namespace mu::engraving;
using namespace muse;

bool SpannersMetaParser::is_able_to_parse(const EngravingItem *spanner_item) {
    static const std::unordered_set<ElementType> supported_types{
        ElementType::SLUR,  ElementType::HAMMER_ON_PULL_OFF, ElementType::PEDAL,       ElementType::LET_RING, ElementType::PALM_MUTE,
        ElementType::TRILL, ElementType::GLISSANDO,          ElementType::GUITAR_BEND, ElementType::VIBRATO,
    };

    return muse::contains(supported_types, spanner_item->type());
}

void SpannersMetaParser::do_parse(const EngravingItem *item, const RenderingContext &spanner_ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->is_spanner()) { return; }

    const Spanner *spanner = to_spanner(item);
    if (!spanner->play_spanner()) {
        return;
    }

    const int overall_duration_ticks = SpannerFilter::spanner_actual_duration_ticks(spanner, spanner_ctx.nominal_duration_ticks);

    mpe::ArticulationType type = mpe::ArticulationType::UNDEFINED;
    mpe::pitch_level_t overall_pitch_range = 0;
    mpe::dynamic_level_t overall_dynamic_range = 0;

    switch (spanner->type()) {
        case ElementType::SLUR:
        case ElementType::HAMMER_ON_PULL_OFF: type = mpe::ArticulationType::LEGATO; break;
        case ElementType::PEDAL:
        case ElementType::LET_RING: type = mpe::ArticulationType::PEDAL; break;
        case ElementType::PALM_MUTE: type = mpe::ArticulationType::PALM_MUTE; break;
        case ElementType::GUITAR_BEND: type = mpe::ArticulationType::MULTIBEND; break;
        case ElementType::VIBRATO: type = mpe::ArticulationType::VIBRATO; break;
        case ElementType::TRILL: {
            const Trill *trill = to_trill(spanner);

            if (trill->trill_type() == TrillType::TRILL_LINE) {
                type = mpe::ArticulationType::TRILL;
            } else if (trill->trill_type() == TrillType::UPPRALL_LINE) {
                type = mpe::ArticulationType::UP_PRALL;
            } else if (trill->trill_type() == TrillType::DOWNPRALL_LINE) {
                type = mpe::ArticulationType::PRALL_DOWN;
            } else if (trill->trill_type() == TrillType::PRALLPRALL_LINE) {
                type = mpe::ArticulationType::LINE_PRALL;
            }
            break;
        }
        case ElementType::GLISSANDO: {
            const Glissando *glissando = to_glissando(spanner);
            const Note *start_note = to_note(glissando->start_element());
            const Note *end_note = to_note(glissando->end_element());

            if (!start_note || !end_note) {
                break;
            }

            if (glissando->glissando_style() == GlissandoStyle::PORTAMENTO) {
                type = mpe::ArticulationType::CONTINUOUS_GLISSANDO;
            } else {
                type = mpe::ArticulationType::DISCRETE_GLISSANDO;
            }

            int start_note_tpc = start_note->playing_tpc();
            int end_note_tpc = end_note->playing_tpc();

            mpe::PitchClass start_note_pitch_class = pitch_class_from_tpc(start_note_tpc);
            mpe::PitchClass end_note_pitch_class = pitch_class_from_tpc(end_note_tpc);

            mpe::octave_t start_note_octave =
                actual_octave(start_note->playing_octave(), start_note_pitch_class, tpc2alter(start_note_tpc));
            mpe::octave_t end_note_octave = actual_octave(end_note->playing_octave(), end_note_pitch_class, tpc2alter(end_note_tpc));

            overall_pitch_range = mpe::pitch_level_diff(end_note_pitch_class, end_note_octave, start_note_pitch_class, start_note_octave);

            break;
        }
        default: break;
    }

    if (type == mpe::ArticulationType::UNDEFINED) {
        return;
    }

    const mpe::ArticulationPattern &pattern = spanner_ctx.profile->pattern(type);
    if (pattern.empty()) {
        return;
    }

    mpe::ArticulationMeta articulation_meta;
    articulation_meta.type = type;
    articulation_meta.pattern = pattern;
    articulation_meta.timestamp = spanner_ctx.nominal_timestamp;
    articulation_meta.overall_pitch_changes_range = overall_pitch_range;
    articulation_meta.overall_dynamic_changes_range = overall_dynamic_range;
    articulation_meta.overall_duration = duration_from_start_and_ticks(spanner_ctx.score, spanner_ctx.nominal_position_start_tick,
                                                                       overall_duration_ticks, spanner_ctx.position_tick_offset);

    append_articulation_data(std::move(articulation_meta), result);
}
