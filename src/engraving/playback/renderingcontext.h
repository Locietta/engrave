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
#pragma once

#include "mpe/events.h"

#include "../dom/chord.h"
#include "../dom/note.h"
#include "../dom/sig.h"

#include "utils/arrangementutils.h"
#include "utils/pitchutils.h"
#include "playbackcontext.h"

namespace mu::engraving {
struct RenderingContext {
    muse::mpe::timestamp_t nominal_timestamp = 0;
    muse::mpe::duration_t nominal_duration = 0;
    muse::mpe::dynamic_level_t nominal_dynamic_level = 0;
    int nominal_position_start_tick = 0;
    int nominal_position_end_tick = 0;
    int nominal_duration_ticks = 0;
    int position_tick_offset = 0;

    BeatsPerSecond beats_per_second = 0;
    TimeSigFrac time_signature_fraction;

    muse::mpe::ArticulationMap common_articulations;

    const Score *score = nullptr;
    const muse::mpe::ArticulationsProfilePtr profile;
    const PlaybackContextPtr playback_ctx;

    bool is_valid() const {
        return score && profile && playback_ctx && beats_per_second > 0 && nominal_duration > 0 && nominal_duration_ticks > 0;
    }
};

inline RenderingContext build_rendering_ctx(const Chord *chord, const int tick_position_offset,
                                            const muse::mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                            const muse::mpe::ArticulationMap &articulations = {}) {
    int chord_pos_tick = chord->tick().ticks();
    int chord_duration_ticks = chord->actual_ticks().ticks();
    int chord_pos_tick_with_offset = chord_pos_tick + tick_position_offset;

    const Score *score = chord->score();

    auto chord_tn_d =
        timestamp_and_duration_from_start_and_duration_ticks(score, chord_pos_tick, chord_duration_ticks, tick_position_offset);

    BeatsPerSecond bps = score->multiplied_tempo_at_utick(chord_pos_tick_with_offset);
    TimeSigFrac time_signature_fraction = score->sigmap()->timesig(chord_pos_tick).timesig();

    RenderingContext ctx{chord_tn_d.timestamp,
                         chord_tn_d.duration,
                         playback_ctx->appliable_dynamic_level(chord->track(), chord_pos_tick_with_offset),
                         chord_pos_tick,
                         chord_pos_tick + chord_duration_ticks,
                         chord_duration_ticks,
                         tick_position_offset,
                         bps,
                         time_signature_fraction,
                         articulations,
                         score,
                         profile,
                         playback_ctx};

    return ctx;
}

struct NominalNoteCtx {
    voice_idx_t voice_idx = 0;
    staff_idx_t staff_idx = 0;
    muse::mpe::timestamp_t timestamp = 0;
    muse::mpe::duration_t duration = 0;
    BeatsPerSecond tempo = 0;
    muse::mpe::dynamic_level_t dynamic_level = 0;
    float user_velocity_fraction = 0.f;

    muse::mpe::pitch_level_t pitch_level = 0;

    RenderingContext chord_ctx;
    muse::mpe::ArticulationMap articulations;

    explicit NominalNoteCtx(const Note *note, const RenderingContext &ctx)
        : voice_idx(note->voice()), staff_idx(note->staff_idx()), timestamp(ctx.nominal_timestamp), duration(ctx.nominal_duration),
          tempo(ctx.beats_per_second), dynamic_level(ctx.nominal_dynamic_level), user_velocity_fraction(note->user_velocity_fraction()),
          pitch_level(note_pitch_level(note->playing_tpc(), note->playing_octave(), note->playing_tuning())), chord_ctx(ctx),
          articulations(ctx.common_articulations) {}
};

inline muse::mpe::NoteEvent build_note_event(const NominalNoteCtx &ctx, const muse::mpe::PitchCurve &pitch_curve = {}) {
    return muse::mpe::NoteEvent(ctx.timestamp, ctx.duration, static_cast<muse::mpe::voice_layer_idx_t>(ctx.voice_idx),
                                static_cast<muse::mpe::staff_layer_idx_t>(ctx.staff_idx), ctx.pitch_level, ctx.dynamic_level,
                                ctx.articulations, ctx.tempo.val, ctx.user_velocity_fraction, pitch_curve);
}
} // namespace mu::engraving
