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

#include "glissandosrenderer.h"

#include "dom/glissando.h"

#include "playback/renderingcontext.h"
#include "playback/utils/expressionutils.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static std::vector<int> pitch_steps(const Note *note) {
    const Glissando *glissando = nullptr;
    for (const Spanner *spanner : note->spanner_for()) {
        if (spanner->is_glissando()) {
            glissando = to_glissando(spanner);
            break;
        }
    }

    if (!glissando) {
        return {};
    }

    std::vector<int> pitch_steps;
    Glissando::pitch_steps(glissando, pitch_steps);

    return pitch_steps;
}

muse::mpe::duration_t GlissandosRenderer::discrete_glissando_step_duration(const Note *note, const duration_t note_duration) {
    const std::vector<int> steps = pitch_steps(note);
    if (steps.empty()) {
        return 0;
    }

    return note_duration / static_cast<float>(steps.size());
}

void GlissandosRenderer::render_discrete_glissando(const Note *note, NominalNoteCtx &ctx, mpe::PlaybackEventList &result) {
    const std::vector<int> steps = pitch_steps(note);
    if (steps.empty()) {
        return;
    }

    const size_t steps_count = steps.size();
    const float duration_step = ctx.duration / static_cast<float>(steps_count);
    size_t first_step = 0;

    if (note->tie_back()) {
        mpe::ArticulationMeta &meta = ctx.articulations.at(ArticulationType::DISCRETE_GLISSANDO).meta;
        meta.timestamp += duration_step;
        meta.overall_duration -= duration_step;
        first_step = 1;
    }

    for (size_t i = first_step; i < steps_count; ++i) {
        NominalNoteCtx note_ctx(ctx);
        note_ctx.duration = duration_step;
        note_ctx.timestamp += i * duration_step;
        note_ctx.pitch_level += steps.at(i) * mpe::pitch_level_step;

        const int utick = timestamp_to_tick(note_ctx.chord_ctx.score, note_ctx.timestamp);
        note_ctx.dynamic_level = ctx.chord_ctx.playback_ctx->appliable_dynamic_level(note->track(), utick);

        update_articulation_boundaries(ArticulationType::DISCRETE_GLISSANDO, note_ctx.timestamp, note_ctx.duration, note_ctx.articulations);

        result.emplace_back(build_note_event(note_ctx));
    }
}

void GlissandosRenderer::render_continuous_glissando(const Note *, NominalNoteCtx &ctx, muse::mpe::PlaybackEventList &result) {
    result.emplace_back(build_note_event(ctx));
}
