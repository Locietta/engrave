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
#include "renderers/noteemitter.h"
#include "types/bps.h"
#include "utils/arrangementutils.h"

namespace mu::engraving {
namespace {
muse::mpe::ArticulationMap make_standard_articulation_map(const muse::mpe::ArticulationsProfilePtr profile,
                                                          muse::mpe::timestamp_t timestamp, muse::mpe::duration_t duration) {
    using namespace muse;
    using namespace muse::mpe;

    IF_ASSERT_FAILED(profile) { return {}; }

    ArticulationMeta meta(ArticulationType::STANDARD, profile->pattern_ptr(ArticulationType::STANDARD), timestamp, duration, 0, 0);

    ArticulationMap articulations;
    articulations.emplace(ArticulationType::STANDARD, mpe::ArticulationAppliedData(std::move(meta), 0, mpe::hundred_percent));
    articulations.pre_calculate_average_data();

    return articulations;
}

void emit_metronome(const TimeSigFrac &time_sig, const double bps, const BeatType beat_type, const muse::mpe::timestamp_t actual_timestamp,
                    const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::TrackEvents &track, muse::mpe::FreezeProgress &progress) {
    using namespace muse;
    using namespace muse::mpe;

    int ticks_per_beat = time_sig.ticks() / time_sig.numerator();
    duration_t duration = duration_from_tempo_and_ticks(bps, ticks_per_beat);

    // The downbeat uses the higher wood block.
    const pitch_level_t pitch = beat_type == BeatType::DOWNBEAT ? pitch_level(PitchClass::E, 5) : pitch_level(PitchClass::F, 5);
    const ArticulationMap articulations = make_standard_articulation_map(profile, actual_timestamp, duration);
    emit_note(track, actual_timestamp, {actual_timestamp, duration, 0, 0, pitch, dynamic_level_from_type(mpe::DynamicType::MF), bps},
              articulations, {}, progress);
}
} // namespace

void PlaybackEventsRenderer::render(const EngravingItem *item, const int tick_position_offset,
                                    const muse::mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                    muse::mpe::TrackEvents &track, muse::mpe::FreezeProgress &progress) const {
    IF_ASSERT_FAILED(item->is_chord()) { return; }

    render_note_events(to_chord(item), tick_position_offset, profile, playback_ctx, track, progress);
}

void PlaybackEventsRenderer::render(const EngravingItem *item, const muse::mpe::timestamp_t actual_timestamp,
                                    const muse::mpe::duration_t actual_duration, const muse::mpe::dynamic_level_t actual_dynamic_level,
                                    const PlaybackContextPtr playback_ctx, const muse::mpe::ArticulationsProfilePtr profile,
                                    muse::mpe::TrackEvents &track, muse::mpe::FreezeProgress &progress) const {
    using namespace muse;
    using namespace muse::mpe;

    if (item->is_chord()) {
        const Chord *chord = to_chord(item);
        if (chord->notes().empty()) {
            emit_empty_group(track, actual_timestamp, progress);
        }
        const NoteEmitter emitter{track, actual_timestamp, progress};
        for (const Note *note : chord->notes()) {
            render_fixed_note_event(note, actual_timestamp, actual_duration, actual_dynamic_level, playback_ctx, profile, emitter);
        }
    } else if (item->is_note()) {
        render_fixed_note_event(to_note(item), actual_timestamp, actual_duration, actual_dynamic_level, playback_ctx, profile,
                                NoteEmitter{track, actual_timestamp, progress});
    } else {
        UNREACHABLE;
    }
}

void PlaybackEventsRenderer::render_chord_symbol(const Harmony *chord_symbol, const int ticks_position_offset,
                                                 const muse::mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                                 muse::mpe::TrackEvents &track, muse::mpe::FreezeProgress &progress) const {
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

    int duration_ticks = realized.get_actual_duration(position_tick_with_offset).ticks();
    duration_t duration = timestamp_from_ticks(score, position_tick_with_offset + duration_ticks) - event_timestamp;

    voice_layer_idx_t voice_idx = static_cast<voice_layer_idx_t>(chord_symbol->voice());
    staff_layer_idx_t staff_idx = static_cast<staff_layer_idx_t>(chord_symbol->staff_idx());
    Key key = staff->key(chord_symbol->tick());

    ArticulationMap articulations = make_standard_articulation_map(profile, event_timestamp, duration);

    double bps = score->multiplied_tempo_at_utick(position_tick_with_offset).val;

    if (notes.empty()) {
        emit_empty_group(track, event_timestamp, progress);
    }
    const NoteEmitter emitter{track, event_timestamp, progress};
    for (auto it = notes.cbegin(); it != notes.cend(); ++it) {
        int pitch = it->first;
        int tpc = pitch2tpc(pitch, key, Prefer::NEAREST);
        int octave = playing_octave(pitch, tpc);
        emitter.emit({event_timestamp, duration, voice_idx, staff_idx, note_pitch_level(tpc, octave),
                      playback_ctx->appliable_dynamic_level(chord_symbol->track(), position_tick_with_offset), bps},
                     articulations);
    }
}

void PlaybackEventsRenderer::render_metronome(const Score *score, const Measure *measure, const int ticks_position_offset,
                                              const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::TrackEvents &track,
                                              muse::mpe::FreezeProgress &progress) const {
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

    // Tempo can change mid-measure (automation point or ramp), so re-read it for every click
    // rather than reusing the tempo from the start of the measure; the click grid itself stays fixed
    for (int tick = start_tick; tick < measure_end_tick; tick += step, rtick += step) {
        const BeatsPerSecond bps = score->multiplied_tempo_at_utick(tick + ticks_position_offset);

        timestamp_t event_timestamp = timestamp_from_ticks(score, tick + ticks_position_offset);
        BeatType beat_type = time_signature_fraction.rtick2beat_type(rtick);
        emit_metronome(time_signature_fraction, bps.val, beat_type, event_timestamp, profile, track, progress);
    }
}

void PlaybackEventsRenderer::render_count_in(const Score *score, const int start_tick, const muse::mpe::timestamp_t actual_timestamp,
                                             const muse::mpe::ArticulationsProfilePtr profile, muse::mpe::TrackEvents &track,
                                             muse::mpe::FreezeProgress &progress, muse::mpe::duration_t &count_in_duration) const {
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

    for (int tick = 0; tick < end_tick; tick += step) {
        int rtick = tick % ticks_per_measure;
        BeatType beat_type = time_signature_fraction.rtick2beat_type(rtick);
        emit_metronome(time_signature_fraction, bps.val, beat_type, event_timestamp, profile, track, progress);
        event_timestamp += step_duration;
    }

    count_in_duration = event_timestamp - actual_timestamp;
    if (remaining_ticks > 0) {
        count_in_duration -= duration_from_tempo_and_ticks(bps.val, remaining_ticks);
    }
}

void PlaybackEventsRenderer::render_note_events(const Chord *chord, const int tick_position_offset,
                                                const muse::mpe::ArticulationsProfilePtr profile, const PlaybackContextPtr playback_ctx,
                                                muse::mpe::TrackEvents &track, muse::mpe::FreezeProgress &progress) const {
    using namespace muse;
    using namespace muse::mpe;

    IF_ASSERT_FAILED(chord) { return; }

    RenderingContext ctx = engraving::build_rendering_ctx(chord, tick_position_offset, profile, playback_ctx);

    if (!ChordFilter::is_item_playable(chord, ctx)) {
        return;
    }

    ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);

    ChordArticulationsRenderer::render(chord, ArticulationType::LAST, ctx, NoteEmitter{track, ctx.nominal_timestamp, progress});
}

using namespace muse;
using namespace muse::mpe;

void PlaybackEventsRenderer::render_fixed_note_event(const Note *note, const mpe::timestamp_t actual_timestamp,
                                                     const mpe::duration_t actual_duration, const mpe::dynamic_level_t actual_dynamic_level,
                                                     const PlaybackContextPtr playback_ctx, const mpe::ArticulationsProfilePtr profile,
                                                     const NoteEmitter &emitter) const {
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

    emitter.emit(NominalNoteCtx{note, ctx});
}

} // namespace mu::engraving
