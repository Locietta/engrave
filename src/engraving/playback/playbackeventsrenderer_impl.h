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

//! NOTE: the definitions of the PlaybackEventsRenderer sink templates. A translation unit that uses the
//! renderer with its own sink includes this header (implicit instantiation), or instantiates the
//! sink once with ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER(Sink) and lets other units link to it.
//! The renderer implementation is never edited to add a sink

#include "dom/chord.h"
#include "dom/harmony.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/sig.h"
#include "dom/staff.h"
#include "dom/utils.h"
#include "filters/chordfilter.h"
#include "log.h"
#include "metaparsers/chordarticulationsparser.h"
#include "playbackeventsrenderer.h"
#include "renderers/chordarticulationsrenderer.h"
#include "types/bps.h"
#include "utils/arrangementutils.h"

namespace mu::engraving {
namespace renderer_detail {
inline muse::mpe::ArticulationMap make_standard_articulation_map(const muse::mpe::ArticulationsProfilePtr profile,
                                                                 muse::mpe::timestamp_t timestamp, muse::mpe::duration_t duration) {
    using namespace muse;
    using namespace muse::mpe;

    IF_ASSERT_FAILED(profile) { return {}; }

    ArticulationMeta meta(ArticulationType::STANDARD, profile->pattern(ArticulationType::STANDARD), timestamp, duration, 0, 0);

    ArticulationMap articulations;
    articulations.emplace(ArticulationType::STANDARD, mpe::ArticulationAppliedData(std::move(meta), 0, mpe::hundred_percent));
    articulations.pre_calculate_average_data();

    return articulations;
}

inline muse::mpe::NoteEvent build_metronome_event(const TimeSigFrac &time_sig, const double bps, const BeatType beat_type,
                                                  const muse::mpe::timestamp_t actual_timestamp,
                                                  const muse::mpe::ArticulationsProfilePtr profile) {
    using namespace muse;
    using namespace muse::mpe;

    int ticks_per_beat = time_sig.ticks() / time_sig.numerator();
    duration_t duration = duration_from_tempo_and_ticks(bps, ticks_per_beat);

    pitch_level_t event_pitch_level = beat_type == BeatType::DOWNBEAT ? pitch_level(PitchClass::E, 5) // high wood block
                                                                        :
                                                                        pitch_level(PitchClass::F, 5); // low wood block

    const ArticulationMap articulations = make_standard_articulation_map(profile, actual_timestamp, duration);

    return mpe::NoteEvent(actual_timestamp, duration, 0, 0, event_pitch_level, dynamic_level_from_type(mpe::DynamicType::MF), articulations,
                          bps);
}
} // namespace renderer_detail

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render(const EngravingItem *item, const int tick_position_offset,
                                    const muse::mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                    Sink &sink) const {
    IF_ASSERT_FAILED(item->is_chord()) { return; }

    render_note_events(to_chord(item), tick_position_offset, profile, playback_ctx, sink);
}

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render(const EngravingItem *item, const muse::mpe::timestamp_t actual_timestamp,
                                    const muse::mpe::duration_t actual_duration, const muse::mpe::dynamic_level_t actual_dynamic_level,
                                    const PlaybackContextPtr playback_ctx, const muse::mpe::ArticulationsProfilePtr profile,
                                    Sink &sink) const {
    using namespace muse;
    using namespace muse::mpe;

    if (item->is_chord()) {
        const Chord *chord = to_chord(item);
        mpe::PlaybackEventList events;

        for (const Note *note : chord->notes()) {
            render_fixed_note_event(note, actual_timestamp, actual_duration, actual_dynamic_level, playback_ctx, profile, events);
        }

        sink(actual_timestamp, std::move(events));
    } else if (item->is_note()) {
        mpe::PlaybackEventList events;
        render_fixed_note_event(to_note(item), actual_timestamp, actual_duration, actual_dynamic_level, playback_ctx, profile, events);
        sink(actual_timestamp, std::move(events));
    } else {
        UNREACHABLE;
    }
}

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render_chord_symbol(const Harmony *chord_symbol, const int ticks_position_offset,
                                                 const muse::mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                                 Sink &sink) const {
    using namespace muse;
    using namespace muse::mpe;

    if (!chord_symbol->is_realizable()) {
        return;
    }

    const Staff *staff = chord_symbol->staff();
    IF_ASSERT_FAILED(staff) { return; }

    const RealizedHarmony &realized = chord_symbol->get_realized_harmony();
    const RealizedHarmony::PitchMap &notes = realized.notes();

    const Score *score = chord_symbol->score();
    int position_tick = chord_symbol->tick().ticks();
    int position_tick_with_offset = position_tick + ticks_position_offset;

    timestamp_t event_timestamp = timestamp_from_ticks(score, position_tick_with_offset);
    PlaybackEventList events;

    int duration_ticks = realized.get_actual_duration(position_tick_with_offset).ticks();
    duration_t duration = timestamp_from_ticks(score, position_tick_with_offset + duration_ticks) - event_timestamp;

    voice_layer_idx_t voice_idx = static_cast<voice_layer_idx_t>(chord_symbol->voice());
    staff_layer_idx_t staff_idx = static_cast<staff_layer_idx_t>(chord_symbol->staff_idx());
    Key key = staff->key(chord_symbol->tick());

    ArticulationMap articulations = renderer_detail::make_standard_articulation_map(profile, event_timestamp, duration);

    double bps = score->multiplied_tempo_at_utick(position_tick_with_offset).val;

    for (auto it = notes.cbegin(); it != notes.cend(); ++it) {
        int pitch = it->first;
        int tpc = pitch2tpc(pitch, key, Prefer::NEAREST);
        int octave = playing_octave(pitch, tpc);
        pitch_level_t pitch_level = note_pitch_level(tpc, octave);

        events.emplace_back(mpe::NoteEvent(event_timestamp, duration, voice_idx, staff_idx, pitch_level,
                                           playback_ctx->appliable_dynamic_level(chord_symbol->track(), position_tick_with_offset),
                                           articulations, bps));
    }

    sink(event_timestamp, std::move(events));
}

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render_chord_symbol(const Harmony *chord_symbol, const muse::mpe::timestamp_t actual_timestamp,
                                                 const muse::mpe::duration_t actual_duration,
                                                 const muse::mpe::dynamic_level_t actual_dynamic_level,
                                                 const muse::mpe::ArticulationsProfilePtr profile, Sink &sink) const {
    using namespace muse;
    using namespace muse::mpe;

    if (!chord_symbol->is_realizable()) {
        return;
    }

    const Staff *staff = chord_symbol->staff();
    IF_ASSERT_FAILED(staff) { return; }

    const RealizedHarmony &realized = chord_symbol->get_realized_harmony();
    const RealizedHarmony::PitchMap &notes = realized.notes();

    PlaybackEventList events;

    voice_layer_idx_t voice_idx = static_cast<voice_layer_idx_t>(chord_symbol->voice());
    staff_layer_idx_t staff_idx = static_cast<staff_layer_idx_t>(chord_symbol->staff_idx());
    Key key = staff->key(chord_symbol->tick());

    ArticulationMap articulations = renderer_detail::make_standard_articulation_map(profile, actual_timestamp, actual_duration);

    for (auto it = notes.cbegin(); it != notes.cend(); ++it) {
        int pitch = it->first;
        int tpc = pitch2tpc(pitch, key, Prefer::NEAREST);
        int octave = playing_octave(pitch, tpc);
        pitch_level_t pitch_level = note_pitch_level(tpc, octave);

        events.emplace_back(mpe::NoteEvent(actual_timestamp, actual_duration, voice_idx, staff_idx, pitch_level, actual_dynamic_level,
                                           articulations, Constants::default_tempo.val));
    }

    sink(actual_timestamp, std::move(events));
}

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render_metronome(const Score *score, const Measure *measure, const int ticks_position_offset,
                                              const muse::mpe::ArticulationsProfilePtr profile, Sink &sink) const {
    using namespace muse;
    using namespace muse::mpe;

    IF_ASSERT_FAILED(score) { return; }

    int measure_start_tick = measure->tick().ticks();
    int measure_end_tick = measure->end_tick().ticks();

    TimeSigFrac time_signature_fraction = score->sigmap()->timesig(measure_start_tick).nominal();

    const BeatsPerSecond measure_bps = score->multiplied_tempo_at_utick(measure_start_tick + ticks_position_offset);
    const int step = time_signature_fraction.is_beated_compound(measure_bps.val) ? time_signature_fraction.beat_ticks() :
                                                                                   time_signature_fraction.d_unit_ticks();

    int start_tick = measure_start_tick;
    int rtick = 0;

    if (measure->is_anacrusis()) {
        int remaining_ticks = measure->ticks().ticks() % step;
        start_tick += remaining_ticks;
        rtick = remaining_ticks + time_signature_fraction.ticks_per_measure() - measure->ticks().ticks();
    }

    PlaybackEventList events;

    // Tempo can change mid-measure (automation point or ramp), so re-read it for every click
    // rather than reusing the tempo from the start of the measure; the click grid itself stays fixed
    for (int tick = start_tick; tick < measure_end_tick; tick += step, rtick += step) {
        const BeatsPerSecond bps = score->multiplied_tempo_at_utick(tick + ticks_position_offset);

        timestamp_t event_timestamp = timestamp_from_ticks(score, tick + ticks_position_offset);
        BeatType beat_type = time_signature_fraction.rtick2beat_type(rtick);
        mpe::NoteEvent event =
            renderer_detail::build_metronome_event(time_signature_fraction, bps.val, beat_type, event_timestamp, profile);

        events.clear();
        events.emplace_back(std::move(event));
        sink(event_timestamp, std::move(events));
    }
}

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render_metronome(const Score *score, const int tick, const muse::mpe::timestamp_t actual_timestamp,
                                              const muse::mpe::ArticulationsProfilePtr profile, Sink &sink) const {
    using namespace muse;
    using namespace muse::mpe;

    IF_ASSERT_FAILED(score) { return; }

    TimeSigFrac time_signature_fraction = score->sigmap()->timesig(tick).timesig();
    BeatsPerSecond bps = score->multiplied_tempo(Fraction::from_ticks(tick));
    BeatType beat_type = score->tick2beat_type(Fraction::from_ticks(tick));
    mpe::NoteEvent event = renderer_detail::build_metronome_event(time_signature_fraction, bps.val, beat_type, actual_timestamp, profile);

    PlaybackEventList events;
    events.emplace_back(std::move(event));
    sink(actual_timestamp, std::move(events));
}

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render_count_in(const Score *score, const int start_tick, const muse::mpe::timestamp_t actual_timestamp,
                                             const muse::mpe::ArticulationsProfilePtr profile, Sink &sink,
                                             muse::mpe::duration_t &count_in_duration) const {
    using namespace muse;
    using namespace muse::mpe;

    const Measure *measure = score->tick2measure(Fraction::from_ticks(start_tick));
    if (!measure) {
        return;
    }

    int measure_start_tick = measure->tick().ticks();
    TimeSigFrac time_signature_fraction = score->sigmap()->timesig(measure_start_tick).nominal();
    BeatsPerSecond bps = score->multiplied_tempo(Fraction::from_ticks(measure_start_tick));
    int ticks_per_measure = time_signature_fraction.ticks_per_measure();

    int step =
        time_signature_fraction.is_beated_compound(bps.val) ? time_signature_fraction.beat_ticks() : time_signature_fraction.d_unit_ticks();

    duration_t step_duration = duration_from_tempo_and_ticks(bps.val, step);

    // Add extra clicks if...
    int end_tick = ticks_per_measure + (start_tick - measure_start_tick); // ... not starting playback at beginning of measure
    int remaining_ticks = 0;

    if (measure->is_anacrusis()) { // ... measure is incomplete (anacrusis)
        int measure_ticks = measure->ticks().ticks();
        end_tick += ticks_per_measure - measure_ticks;
        remaining_ticks = measure_ticks % step;
    }

    MeasureBeat measure_beat = find_beat(score, start_tick);
    int closest_main_beat_tick = score->sigmap()->bar2tick(measure_beat.measure_index, std::ceil(measure_beat.beat));
    remaining_ticks += closest_main_beat_tick - start_tick;

    timestamp_t event_timestamp = actual_timestamp;
    PlaybackEventList events;

    for (int tick = 0; tick < end_tick; tick += step) {
        int rtick = tick % ticks_per_measure;
        BeatType beat_type = time_signature_fraction.rtick2beat_type(rtick);
        mpe::NoteEvent event =
            renderer_detail::build_metronome_event(time_signature_fraction, bps.val, beat_type, event_timestamp, profile);

        events.clear();
        events.emplace_back(std::move(event));
        sink(event_timestamp, std::move(events));
        event_timestamp += step_duration;
    }

    count_in_duration = event_timestamp - actual_timestamp;
    if (remaining_ticks > 0) {
        count_in_duration -= duration_from_tempo_and_ticks(bps.val, remaining_ticks);
    }
}

template <PlaybackEventSink Sink>
void PlaybackEventsRenderer::render_note_events(const Chord *chord, const int tick_position_offset,
                                                const muse::mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                                Sink &sink) const {
    using namespace muse;
    using namespace muse::mpe;

    IF_ASSERT_FAILED(chord) { return; }

    RenderingContext ctx = engraving::build_rendering_ctx(chord, tick_position_offset, profile, playback_ctx);

    if (!ChordFilter::is_item_playable(chord, ctx)) {
        return;
    }

    ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);

    PlaybackEventList new_events;
    ChordArticulationsRenderer::render(chord, ArticulationType::LAST, ctx, new_events);

    if (!new_events.empty()) {
        sink(ctx.nominal_timestamp, std::move(new_events));
    }
}
} // namespace mu::engraving

//! Explicitly instantiates every renderer entry point for Sink; use once per sink type, at namespace scope
#define ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER(Sink)                                                                                 \
    template void mu::engraving::PlaybackEventsRenderer::render(const mu::engraving::EngravingItem *, const int,                           \
                                                                const muse::mpe::ArticulationsProfilePtr,                                  \
                                                                const mu::engraving::PlaybackContextPtr, Sink &) const;                    \
    template void mu::engraving::PlaybackEventsRenderer::render(                                                                           \
        const mu::engraving::EngravingItem *, const muse::mpe::timestamp_t, const muse::mpe::duration_t, const muse::mpe::dynamic_level_t, \
        const mu::engraving::PlaybackContextPtr, const muse::mpe::ArticulationsProfilePtr, Sink &) const;                                  \
    template void mu::engraving::PlaybackEventsRenderer::render_chord_symbol(const mu::engraving::Harmony *, const int,                    \
                                                                             const muse::mpe::ArticulationsProfilePtr,                     \
                                                                             const mu::engraving::PlaybackContextPtr, Sink &) const;       \
    template void mu::engraving::PlaybackEventsRenderer::render_chord_symbol(                                                              \
        const mu::engraving::Harmony *, const muse::mpe::timestamp_t, const muse::mpe::duration_t, const muse::mpe::dynamic_level_t,       \
        const muse::mpe::ArticulationsProfilePtr, Sink &) const;                                                                           \
    template void mu::engraving::PlaybackEventsRenderer::render_metronome(                                                                 \
        const mu::engraving::Score *, const mu::engraving::Measure *, const int, const muse::mpe::ArticulationsProfilePtr, Sink &) const;  \
    template void mu::engraving::PlaybackEventsRenderer::render_metronome(                                                                 \
        const mu::engraving::Score *, const int, const muse::mpe::timestamp_t, const muse::mpe::ArticulationsProfilePtr, Sink &) const;    \
    template void mu::engraving::PlaybackEventsRenderer::render_count_in(                                                                  \
        const mu::engraving::Score *, const int, const muse::mpe::timestamp_t, const muse::mpe::ArticulationsProfilePtr, Sink &,           \
        muse::mpe::duration_t &) const
