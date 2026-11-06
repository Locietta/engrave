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

#include "global/realfn.h"
#include "mpe/mpetypes.h"

#include "../../dom/score.h"
#include "../../dom/repeatlist.h"
#include "../../dom/tempotimeline.h"

#include "../../types/constants.h"

namespace mu::engraving {
inline muse::mpe::timestamp_t timestamp_from_ticks(const Score *score, const int tick) {
    return score->tempo_timeline().utick2utime(tick) * 1000000;
}

inline int timestamp_to_tick(const Score *score, const muse::mpe::timestamp_t timestamp) {
    return score->tempo_timeline().utime2utick(timestamp / 1000000.f);
}

// Returns the pause at `tick` in microseconds, or 0
inline muse::mpe::duration_t pause_us(const Score *score, const int tick, const int tick_position_offset) {
    const TempoTimeline &timeline = score->tempo_timeline();
    const auto pause_it = timeline.pauses().find(tick + tick_position_offset);
    if (pause_it == timeline.pauses().end()) {
        return 0;
    }

    double secs = pause_it->second / timeline.tempo_multiplier().val;
    return muse::real_is_null(secs) ? 0 : secs * 1000000;
}

inline muse::mpe::duration_t duration_from_start_and_end_tick(const Score *score, const int start_tick, const int end_tick,
                                                              const int tick_position_offset) {
    muse::mpe::timestamp_t start_timestamp = timestamp_from_ticks(score, start_tick + tick_position_offset);
    muse::mpe::timestamp_t end_timestamp = timestamp_from_ticks(score, end_tick + tick_position_offset);
    muse::mpe::duration_t pause = pause_us(score, end_tick, tick_position_offset);

    return end_timestamp - start_timestamp - pause;
}

inline muse::mpe::duration_t duration_from_start_and_ticks(const Score *score, const int start_tick, const int duration_ticks,
                                                           const int tick_position_offset) {
    return duration_from_start_and_end_tick(score, start_tick, start_tick + duration_ticks, tick_position_offset);
}

inline muse::mpe::TimestampAndDuration timestamp_and_duration_from_start_and_duration_ticks(const Score *score, const int start_tick,
                                                                                            const int duration_ticks,
                                                                                            const int tick_position_offset) {
    int start_tick_with_offset = start_tick + tick_position_offset;
    muse::mpe::timestamp_t start_timestamp = timestamp_from_ticks(score, start_tick_with_offset);
    muse::mpe::timestamp_t end_timestamp = timestamp_from_ticks(score, start_tick_with_offset + duration_ticks);
    muse::mpe::duration_t pause = pause_us(score, start_tick + duration_ticks, tick_position_offset);
    muse::mpe::duration_t duration = end_timestamp - start_timestamp - pause;

    return {start_timestamp, duration};
}

inline muse::mpe::duration_t duration_from_tempo_and_ticks(const double beats_per_second, const int duration_ticks,
                                                           const int ticks_per_beat = Constants::division) {
    float beats_number = static_cast<float>(duration_ticks) / static_cast<float>(ticks_per_beat);

    return (beats_number / beats_per_second) * 1000000;
}

inline int ticks_from_tempo_and_duration(const double beats_per_second, const muse::mpe::duration_t duration,
                                         const int ticks_per_beat = Constants::division) {
    return (duration * beats_per_second * ticks_per_beat) / 1000000;
}

static constexpr int crotchet_ticks = Constants::division;
static constexpr int quaver_ticks = Constants::division / 2;
static constexpr int semiquaver_ticks = Constants::division / 4;
static constexpr int demisemiquaver_ticks = Constants::division / 8;

static const double prestissimo_bps_bound = muse::real_round(200 /*bpm*/ / 60.f /*secs*/, 2);
static const double presto_bps_bound = muse::real_round(168 /*bpm*/ / 60.f /*secs*/, 2);
static const double allegro_bps_bound = muse::real_round(120 /*bpm*/ / 60.f /*secs*/, 2);
static const double moderato_bps_bound = muse::real_round(108 /*bpm*/ / 60.f /*secs*/, 2);
static const double andante_bps_bound = muse::real_round(76 /*bpm*/ / 60.f /*secs*/, 2);
static const double adagio_bps_bound = muse::real_round(66 /*bpm*/ / 60.f /*secs*/, 2);
static const double lento_bps_bound = muse::real_round(40 /*bpm*/ / 60.f /*secs*/, 2);
static const double grave_bps_bound = muse::real_round(20 /*bpm*/ / 60.f /*secs*/, 2);
} // namespace mu::engraving
