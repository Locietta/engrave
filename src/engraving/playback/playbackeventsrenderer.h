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
#include "playbackeventsink.h"
#include "renderingcontext.h"

namespace mu::engraving {
struct Chord;
struct Score;
struct Note;

//! NOTE: every top-level function emits its completed per-chord (or per-click) event lists through
//! a PlaybackEventSink. The sink template definitions are in playbackeventsrenderer_impl.h: a unit
//! using another sink includes it or instantiates the sink with ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER.
//! The PlaybackEventsMap overloads go through EventsMapSink, instantiated in playbackeventsrenderer.cpp
struct PlaybackEventsRenderer {
    PlaybackEventsRenderer() = default;

    template <PlaybackEventSink Sink>
    void render(const EngravingItem *item, const int tick_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                const PlaybackContextPtr playback_ctx, Sink &sink) const;

    template <PlaybackEventSink Sink>
    void render(const EngravingItem *item, const muse::mpe::timestamp_t actual_timestamp, const muse::mpe::duration_t actual_duration,
                const muse::mpe::dynamic_level_t actual_dynamic_level, const PlaybackContextPtr playback_ctx,
                const muse::mpe::ArticulationsProfilePtr profile, Sink &sink) const;

    template <PlaybackEventSink Sink>
    void render_chord_symbol(const Harmony *chord_symbol, const int ticks_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                             const PlaybackContextPtr playback_ctx, Sink &sink) const;
    template <PlaybackEventSink Sink>
    void render_chord_symbol(const Harmony *chord_symbol, const muse::mpe::timestamp_t actual_timestamp,
                             const muse::mpe::duration_t actual_duration, const muse::mpe::dynamic_level_t actual_dynamic_level,
                             const muse::mpe::ArticulationsProfilePtr profile, Sink &sink) const;

    template <PlaybackEventSink Sink>
    void render_metronome(const Score *score, const Measure *measure, const int ticks_position_offset,
                          const muse::mpe::ArticulationsProfilePtr profile, Sink &sink) const;

    template <PlaybackEventSink Sink>
    void render_metronome(const Score *score, const int tick, const muse::mpe::timestamp_t actual_timestamp,
                          const muse::mpe::ArticulationsProfilePtr profile, Sink &sink) const;

    template <PlaybackEventSink Sink>
    void render_count_in(const Score *score, const int tick, const muse::mpe::timestamp_t actual_timestamp,
                         const muse::mpe::ArticulationsProfilePtr profile, Sink &sink, muse::mpe::duration_t &count_in_duration) const;

    void render(const EngravingItem *item, const int tick_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                const PlaybackContextPtr playback_ctx, muse::mpe::PlaybackEventsMap &result) const {
        EventsMapSink sink{result};
        render(item, tick_position_offset, profile, playback_ctx, sink);
    }

    void render(const EngravingItem *item, const muse::mpe::timestamp_t actual_timestamp, const muse::mpe::duration_t actual_duration,
                const muse::mpe::dynamic_level_t actual_dynamic_level, const PlaybackContextPtr playback_ctx,
                const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const {
        EventsMapSink sink{result};
        render(item, actual_timestamp, actual_duration, actual_dynamic_level, playback_ctx, profile, sink);
    }

    void render_chord_symbol(const Harmony *chord_symbol, const int ticks_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                             const PlaybackContextPtr playback_ctx, muse::mpe::PlaybackEventsMap &result) const {
        EventsMapSink sink{result};
        render_chord_symbol(chord_symbol, ticks_position_offset, profile, playback_ctx, sink);
    }

    void render_chord_symbol(const Harmony *chord_symbol, const muse::mpe::timestamp_t actual_timestamp,
                             const muse::mpe::duration_t actual_duration, const muse::mpe::dynamic_level_t actual_dynamic_level,
                             const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const {
        EventsMapSink sink{result};
        render_chord_symbol(chord_symbol, actual_timestamp, actual_duration, actual_dynamic_level, profile, sink);
    }

    void render_metronome(const Score *score, const Measure *measure, const int ticks_position_offset,
                          const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const {
        EventsMapSink sink{result};
        render_metronome(score, measure, ticks_position_offset, profile, sink);
    }

    void render_metronome(const Score *score, const int tick, const muse::mpe::timestamp_t actual_timestamp,
                          const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result) const {
        EventsMapSink sink{result};
        render_metronome(score, tick, actual_timestamp, profile, sink);
    }

    void render_count_in(const Score *score, const int tick, const muse::mpe::timestamp_t actual_timestamp,
                         const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result,
                         muse::mpe::duration_t &count_in_duration) const {
        EventsMapSink sink{result};
        render_count_in(score, tick, actual_timestamp, profile, sink, count_in_duration);
    }

private:
    template <PlaybackEventSink Sink>
    void render_note_events(const Chord *chord, const int tick_position_offset, const muse::mpe::ArticulationsProfilePtr profile,
                            const PlaybackContextPtr playback_ctx, Sink &sink) const;

    void render_fixed_note_event(const Note *note, const muse::mpe::timestamp_t actual_timestamp,
                                 const muse::mpe::duration_t actual_duration, const muse::mpe::dynamic_level_t actual_dynamic_level,
                                 const PlaybackContextPtr playback_ctx, const muse::mpe::ArticulationsProfilePtr profile,
                                 muse::mpe::PlaybackEventList &result) const;
};
} // namespace mu::engraving
