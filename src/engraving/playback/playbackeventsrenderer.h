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

#include "renderingcontext.h"

namespace mu::engraving {
struct Chord;
struct Score;
struct Note;

struct PlaybackEventsRenderer {
    PlaybackEventsRenderer() = default;

    void render(const EngravingItem *item, const int tick_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                const PlaybackContextPtr playback_ctx, muse::mpe::PlaybackEventsMap &result) const;

    void render(const EngravingItem *item, const muse::mpe::timestamp_t actual_timestamp, const muse::mpe::duration_t actual_duration,
                const muse::mpe::dynamic_level_t actual_dynamic_level, const PlaybackContextPtr playback_ctx,
                const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const;

    void render_chord_symbol(const Harmony *chord_symbol, const int ticks_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                             const PlaybackContextPtr playback_ctx, muse::mpe::PlaybackEventsMap &result) const;
    void render_chord_symbol(const Harmony *chord_symbol, const muse::mpe::timestamp_t actual_timestamp,
                             const muse::mpe::duration_t actual_duration, const muse::mpe::dynamic_level_t actual_dynamic_level,
                             const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const;

    void render_metronome(const Score *score, const Measure *measure, const int ticks_position_offset,
                          const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const;

    void render_metronome(const Score *score, const int tick, const muse::mpe::timestamp_t actual_timestamp,
                          const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const;

    void render_count_in(const Score *score, const int tick, const muse::mpe::timestamp_t actual_timestamp,
                         const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result,
                         muse::mpe::duration_t &count_in_duration) const;

private:
    void render_note_events(const Chord *chord, const int tick_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                            const PlaybackContextPtr playback_ctx, muse::mpe::PlaybackEventsMap &result) const;

    void render_fixed_note_event(const Note *note, const muse::mpe::timestamp_t actual_timestamp,
                                 const muse::mpe::duration_t actual_duration, const muse::mpe::dynamic_level_t actual_dynamic_level,
                                 const PlaybackContextPtr playback_ctx, const muse::mpe::ArticulationsProfilePtr profile,
                                 muse::mpe::PlaybackEventList &result) const;
};
} // namespace mu::engraving
