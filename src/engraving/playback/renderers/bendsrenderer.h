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
#pragma once

#include "mpe/events.h"

namespace mu::engraving {
struct Note;
struct GuitarBend;
struct Score;
struct RenderingContext;
struct GraceChordCtx;

struct BendsRenderer {
    static bool is_multibend_part(const Note *note);

    static void render(const Note *note, const RenderingContext &ctx, muse::mpe::PlaybackEventList &result);

private:
    struct BendTimeFactors {
        float start_factor = 0.f;
        float target_factor = 1.f;
        float end_factor = 1.f;
    };

    using PitchOffsets = std::vector<std::pair<muse::mpe::timestamp_t, muse::mpe::pitch_level_t>>;
    using BendTimeFactorMap = std::map<muse::mpe::timestamp_t, BendTimeFactors>;

    static void render_multibend(const Note *start_note, const RenderingContext &start_note_ctx, muse::mpe::PlaybackEventList &result);
    static void render_note(const Note *note, const RenderingContext &ctx, muse::mpe::PlaybackEventList &result);
    static void render_grace_note(const Note *note, const GraceChordCtx &ctx, muse::mpe::PlaybackEventList &result);
    static void render_slight_bend(const Note *note, const GuitarBend *bend, const RenderingContext &ctx,
                                   muse::mpe::PlaybackEventList &result);
    static void render_scoop(const Note *note, const GuitarBend *bend, const RenderingContext &ctx, muse::mpe::PlaybackEventList &result);
    static void render_dip(const Note *note, const GuitarBend *bend, const RenderingContext &ctx, muse::mpe::PlaybackEventList &result);

    static BendTimeFactors time_factors(const GuitarBend *bend);

    static RenderingContext build_rendering_context(const Note *note, const RenderingContext &initial_ctx);
    static muse::mpe::NoteEvent build_bend_event(const Note *start_note, const RenderingContext &start_note_ctx,
                                                 const muse::mpe::PlaybackEventList &bend_note_events,
                                                 const BendTimeFactorMap &time_factor_map);
    static muse::mpe::PitchCurve build_pitch_curve(muse::mpe::timestamp_t bend_start_time, muse::mpe::duration_t total_bend_duration,
                                                   const PitchOffsets &pitch_offsets, const BendTimeFactorMap &time_factor_map);
};
} // namespace mu::engraving
