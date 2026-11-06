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

#include "playbackeventsrenderer.h"

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
#include "metaparsers/notearticulationsparser.h"
#include "renderers/chordarticulationsrenderer.h"
#include "types/bps.h"
#include "utils/arrangementutils.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static ArticulationMap make_standard_articulation_map(const ArticulationsProfilePtr profile, timestamp_t timestamp, duration_t duration) {
    IF_ASSERT_FAILED(profile) { return {}; }

    ArticulationMeta meta(ArticulationType::STANDARD, profile->pattern(ArticulationType::STANDARD), timestamp, duration, 0, 0);

    ArticulationMap articulations;
    articulations.emplace(ArticulationType::STANDARD, mpe::ArticulationAppliedData(std::move(meta), 0, mpe::hundred_percent));
    articulations.pre_calculate_average_data();

    return articulations;
}

static muse::mpe::NoteEvent build_metronome_event(const TimeSigFrac &time_sig, const double bps, const BeatType beat_type,
                                                  const muse::mpe::timestamp_t actual_timestamp,
                                                  const muse::mpe::ArticulationsProfilePtr profile) {
    int ticks_per_beat = time_sig.ticks() / time_sig.numerator();
    duration_t duration = duration_from_tempo_and_ticks(bps, ticks_per_beat);

    pitch_level_t event_pitch_level = beat_type == BeatType::DOWNBEAT ? pitch_level(PitchClass::E, 5) // high wood block
                                                                        :
                                                                        pitch_level(PitchClass::F, 5); // low wood block

    const ArticulationMap articulations = make_standard_articulation_map(profile, actual_timestamp, duration);

    return mpe::NoteEvent(actual_timestamp, duration, 0, 0, event_pitch_level, dynamic_level_from_type(mpe::DynamicType::MF), articulations,
                          bps);
}

void PlaybackEventsRenderer::render(const EngravingItem *item, const int tick_position_offset, const ArticulationsProfilePtr profile,
                                    const PlaybackContextPtr playback_ctx, PlaybackEventsMap &result) const {
    IF_ASSERT_FAILED(item->is_chord()) { return; }

    render_note_events(to_chord(item), tick_position_offset, profile, playback_ctx, result);
}

void PlaybackEventsRenderer::render(const EngravingItem *item, const mpe::timestamp_t actual_timestamp,
                                    const mpe::duration_t actual_duration, const mpe::dynamic_level_t actual_dynamic_level,
                                    const PlaybackContextPtr playback_ctx, const ArticulationsProfilePtr profile,
                                    PlaybackEventsMap &result) const {
    if (item->is_chord()) {
        const Chord *chord = to_chord(item);
        mpe::PlaybackEventList &events = result[actual_timestamp];

        for (const Note *note : chord->notes()) {
            render_fixed_note_event(note, actual_timestamp, actual_duration, actual_dynamic_level, playback_ctx, profile, events);
        }
    } else if (item->is_note()) {
        render_fixed_note_event(to_note(item), actual_timestamp, actual_duration, actual_dynamic_level, playback_ctx, profile,
                                result[actual_timestamp]);
    } else {
        UNREACHABLE;
    }
}

void PlaybackEventsRenderer::render_chord_symbol(const Harmony *chord_symbol, const int ticks_position_offset,
                                                 const mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                                 mpe::PlaybackEventsMap &result) const {
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
    PlaybackEventList &events = result[event_timestamp];

    int duration_ticks = realized.get_actual_duration(position_tick_with_offset).ticks();
    duration_t duration = timestamp_from_ticks(score, position_tick_with_offset + duration_ticks) - event_timestamp;

    voice_layer_idx_t voice_idx = static_cast<voice_layer_idx_t>(chord_symbol->voice());
    staff_layer_idx_t staff_idx = static_cast<staff_layer_idx_t>(chord_symbol->staff_idx());
    Key key = staff->key(chord_symbol->tick());

    ArticulationMap articulations = make_standard_articulation_map(profile, event_timestamp, duration);

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
}

void PlaybackEventsRenderer::render_chord_symbol(const Harmony *chord_symbol, const mpe::timestamp_t actual_timestamp,
                                                 const mpe::duration_t actual_duration, const mpe::dynamic_level_t actual_dynamic_level,
                                                 const ArticulationsProfilePtr profile, mpe::PlaybackEventsMap &result) const {
    if (!chord_symbol->is_realizable()) {
        return;
    }

    const Staff *staff = chord_symbol->staff();
    IF_ASSERT_FAILED(staff) { return; }

    const RealizedHarmony &realized = chord_symbol->get_realized_harmony();
    const RealizedHarmony::PitchMap &notes = realized.notes();

    PlaybackEventList &events = result[actual_timestamp];

    voice_layer_idx_t voice_idx = static_cast<voice_layer_idx_t>(chord_symbol->voice());
    staff_layer_idx_t staff_idx = static_cast<staff_layer_idx_t>(chord_symbol->staff_idx());
    Key key = staff->key(chord_symbol->tick());

    ArticulationMap articulations = make_standard_articulation_map(profile, actual_timestamp, actual_duration);

    for (auto it = notes.cbegin(); it != notes.cend(); ++it) {
        int pitch = it->first;
        int tpc = pitch2tpc(pitch, key, Prefer::NEAREST);
        int octave = playing_octave(pitch, tpc);
        pitch_level_t pitch_level = note_pitch_level(tpc, octave);

        events.emplace_back(mpe::NoteEvent(actual_timestamp, actual_duration, voice_idx, staff_idx, pitch_level, actual_dynamic_level,
                                           articulations, Constants::default_tempo.val));
    }
}

void PlaybackEventsRenderer::render_metronome(const Score *score, const Measure *measure, const int ticks_position_offset,
                                              const muse::mpe::ArticulationsProfilePtr profile, mpe::PlaybackEventsMap &result) const {
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

    // Tempo can change mid-measure (automation point or ramp), so re-read it for every click
    // rather than reusing the tempo from the start of the measure; the click grid itself stays fixed
    for (int tick = start_tick; tick < measure_end_tick; tick += step, rtick += step) {
        const BeatsPerSecond bps = score->multiplied_tempo_at_utick(tick + ticks_position_offset);

        timestamp_t event_timestamp = timestamp_from_ticks(score, tick + ticks_position_offset);
        BeatType beat_type = time_signature_fraction.rtick2beat_type(rtick);
        mpe::NoteEvent event = build_metronome_event(time_signature_fraction, bps.val, beat_type, event_timestamp, profile);

        result[event_timestamp].emplace_back(std::move(event));
    }
}

void PlaybackEventsRenderer::render_metronome(const Score *score, const int tick, const mpe::timestamp_t actual_timestamp,
                                              const muse::mpe::ArticulationsProfilePtr profile, mpe::PlaybackEventsMap &result) const {
    IF_ASSERT_FAILED(score) { return; }

    TimeSigFrac time_signature_fraction = score->sigmap()->timesig(tick).timesig();
    BeatsPerSecond bps = score->multiplied_tempo(Fraction::from_ticks(tick));
    BeatType beat_type = score->tick2beat_type(Fraction::from_ticks(tick));
    mpe::NoteEvent event = build_metronome_event(time_signature_fraction, bps.val, beat_type, actual_timestamp, profile);

    result[actual_timestamp].emplace_back(std::move(event));
}

void PlaybackEventsRenderer::render_count_in(const Score *score, const int start_tick, const muse::mpe::timestamp_t actual_timestamp,
                                             const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::PlaybackEventsMap &result,
                                             muse::mpe::duration_t &count_in_duration) const {
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

    for (int tick = 0; tick < end_tick; tick += step) {
        int rtick = tick % ticks_per_measure;
        BeatType beat_type = time_signature_fraction.rtick2beat_type(rtick);
        mpe::NoteEvent event = build_metronome_event(time_signature_fraction, bps.val, beat_type, event_timestamp, profile);

        result[event_timestamp].emplace_back(std::move(event));
        event_timestamp += step_duration;
    }

    count_in_duration = event_timestamp - actual_timestamp;
    if (remaining_ticks > 0) {
        count_in_duration -= duration_from_tempo_and_ticks(bps.val, remaining_ticks);
    }
}

void PlaybackEventsRenderer::render_note_events(const Chord *chord, const int tick_position_offset,
                                                const mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                                PlaybackEventsMap &result) const {
    IF_ASSERT_FAILED(chord) { return; }

    RenderingContext ctx = engraving::build_rendering_ctx(chord, tick_position_offset, profile, playback_ctx);

    if (!ChordFilter::is_item_playable(chord, ctx)) {
        return;
    }

    ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);

    PlaybackEventList new_events;
    ChordArticulationsRenderer::render(chord, ArticulationType::LAST, ctx, new_events);

    if (!new_events.empty()) {
        PlaybackEventList &list = result[ctx.nominal_timestamp];
        list.insert(list.end(), std::make_move_iterator(new_events.begin()), std::make_move_iterator(new_events.end()));
    }
}

void PlaybackEventsRenderer::render_fixed_note_event(const Note *note, const mpe::timestamp_t actual_timestamp,
                                                     const mpe::duration_t actual_duration, const mpe::dynamic_level_t actual_dynamic_level,
                                                     const PlaybackContextPtr playback_ctx, const mpe::ArticulationsProfilePtr profile,
                                                     mpe::PlaybackEventList &result) const {
    static const ArticulationMap articulations;

    const Score *score = note->score();
    const int duration_ticks = ticks_from_tempo_and_duration(Constants::default_tempo.val, actual_duration);
    const int tick = note->tick().ticks();
    const int utick = score ? score->repeat_list().tick2utick(tick) : tick;
    const int tick_offset = utick - tick;

    RenderingContext ctx{actual_timestamp,
                         actual_duration,
                         actual_dynamic_level,
                         tick,           /*nominalPositionStartTick*/
                         duration_ticks, /*nominalPositionEndTick*/
                         duration_ticks, /*nominalDurationTicks*/
                         tick_offset,
                         Constants::default_tempo,
                         TimeSigMap::default_time_signature,
                         articulations,
                         score,
                         profile,
                         playback_ctx};

    NoteArticulationsParser::parse_playing_technique(ctx, note->track(), ctx.common_articulations, false /*sustainAllowed*/);
    NoteArticulationsParser::parse_ghost_note(note, ctx, ctx.common_articulations);
    NoteArticulationsParser::parse_note_head(note, ctx, ctx.common_articulations);
    NoteArticulationsParser::parse_symbols(note, ctx, ctx.common_articulations);

    if (ctx.common_articulations.empty()) {
        ctx.common_articulations = make_standard_articulation_map(profile, actual_timestamp, actual_duration);
    } else {
        ctx.common_articulations.pre_calculate_average_data();
    }

    NominalNoteCtx note_ctx(note, ctx);
    result.emplace_back(build_note_event(note_ctx));
}
