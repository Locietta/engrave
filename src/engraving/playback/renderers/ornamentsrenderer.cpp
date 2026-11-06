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

#include "ornamentsrenderer.h"

#include "realfn.h"

#include "dom/ornament.h"
#include "dom/utils.h"

#include "playback/metaparsers/notearticulationsparser.h"
#include "playback/utils/expressionutils.h"

#include "noterenderer.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

namespace mu::engraving {
struct IntervalsInfo {
    bool interval_below_is_auto = false;
    bool interval_above_is_auto = false;

    Interval interval_below;
    Interval interval_above;
};

struct DisclosurePattern {
    int prefix_duration_ticks = 0;
    std::vector<mpe::pitch_level_t> prefix_pitch_offsets;

    bool is_alterations_repeat_allowed = false;
    std::vector<mpe::pitch_level_t> alteration_step_pitch_offsets;

    int suffix_duration_ticks = 0;
    std::vector<mpe::pitch_level_t> suffix_pitch_offsets;

    struct DurationBoundaries {
        float low_tempo_duration_ticks = 0.f;
        float medium_tempo_duration_ticks = 0.f;
        float high_tempo_duration_ticks = 0.f;
    };

    DurationBoundaries boundaries;

    float sub_note_duration_ticks(const double bps) const;
    DisclosurePattern build_actual_pattern(const Note *note, const IntervalsInfo &intervals_info, const double bps) const;

private:
    void update_pitch_offsets(const Note *note, const IntervalsInfo &intervals_info, std::vector<mpe::pitch_level_t> &pitch_offsets);
};

static IntervalsInfo make_intervals_info(const OrnamentInterval &below, const OrnamentInterval &above) {
    IntervalsInfo result;
    result.interval_below_is_auto = below.type == IntervalType::AUTO;
    result.interval_above_is_auto = above.type == IntervalType::AUTO;
    result.interval_below = Interval::from_ornament_interval(below);
    result.interval_above = Interval::from_ornament_interval(above);

    return result;
}
} // namespace mu::engraving

static const std::unordered_map<ArticulationType, DisclosurePattern> disclosure_rules = {
    {ArticulationType::TRILL,
     {/*prefixDurationTicks*/ 0,
      /*prefixPitchOffsets*/ {},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {0, ::muse::mpe::pitch_level_step},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::quaver_ticks / 10.f, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::TRILL_BAROQUE,
     {/*prefixDurationTicks*/ 0,
      /*prefixPitchOffsets*/ {},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 2,
      /*suffixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::quaver_ticks / 10.f, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::LINE_PRALL,
     {/*prefixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 3,
      /*prefixPitchOffsets*/ {2 * ::muse::mpe::pitch_level_step, 2 * ::muse::mpe::pitch_level_step, 2 * ::muse::mpe::pitch_level_step},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 2,
      /*suffixPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::demisemiquaver_ticks, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::demisemiquaver_ticks}}},
    {ArticulationType::UP_PRALL,
     {/*prefixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*prefixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*suffixPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::demisemiquaver_ticks, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::demisemiquaver_ticks}}},
    {ArticulationType::UP_MORDENT,
     {/*prefixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*prefixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*suffixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::UPPER_MORDENT,
     {/*prefixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 2,
      /*prefixPitchOffsets*/ {0, ::muse::mpe::pitch_level_step},
      /*isAlterationsRepeatAllowed*/ false,
      /*alterationStepPitchOffsets*/ {0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::demisemiquaver_ticks / 2.f, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::LOWER_MORDENT,
     {/*prefixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 2,
      /*prefixPitchOffsets*/ {0, -::muse::mpe::pitch_level_step},
      /*isAlterationsRepeatAllowed*/ false,
      /*alterationStepPitchOffsets*/ {0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::demisemiquaver_ticks / 2.f, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::UPPER_MORDENT_BAROQUE,
     {/*prefixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 3,
      /*prefixPitchOffsets*/ {::muse::mpe::pitch_level_step, 0, ::muse::mpe::pitch_level_step},
      /*isAlterationsRepeatAllowed*/ false,
      /*alterationStepPitchOffsets*/ {0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::demisemiquaver_ticks / 2.f, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::MORDENT_WITH_UPPER_PREFIX,
     {/*prefixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 4,
      /*prefixPitchOffsets*/ {::muse::mpe::pitch_level_step, ::muse::mpe::pitch_level_step, ::muse::mpe::pitch_level_step, 0},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::DOWN_MORDENT,
     {/*prefixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 4,
      /*prefixPitchOffsets*/ {::muse::mpe::pitch_level_step, ::muse::mpe::pitch_level_step, ::muse::mpe::pitch_level_step, 0},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*suffixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::PRALL_UP,
     {/*prefixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*prefixPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*suffixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::PRALL_DOWN,
     {/*prefixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*prefixPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*isAlterationsRepeatAllowed*/ true,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 4,
      /*suffixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0, 0, 0},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::TURN,
     {/*prefixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 3,
      /*prefixPitchOffsets*/ {::muse::mpe::pitch_level_step, 0, -::muse::mpe::pitch_level_step},
      /*isAlterationsRepeatAllowed*/ false,
      /*alterationStepPitchOffsets*/ {0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::demisemiquaver_ticks / 2.f, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::INVERTED_TURN,
     {/*prefixDurationTicks*/ ::mu::engraving::demisemiquaver_ticks * 3,
      /*prefixPitchOffsets*/ {-::muse::mpe::pitch_level_step, 0, ::muse::mpe::pitch_level_step},
      /*isAlterationsRepeatAllowed*/ false,
      /*alterationStepPitchOffsets*/ {0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::demisemiquaver_ticks / 2.f, ::mu::engraving::demisemiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::TREMBLEMENT,
     {/*prefixDurationTicks*/ ::mu::engraving::semiquaver_ticks * 2,
      /*prefixPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*isAlterationsRepeatAllowed*/ false,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
    {ArticulationType::PRALL_MORDENT,
     {/*prefixDurationTicks*/ 0,
      /*prefixPitchOffsets*/ {},
      /*isAlterationsRepeatAllowed*/ false,
      /*alterationStepPitchOffsets*/ {::muse::mpe::pitch_level_step, 0, -::muse::mpe::pitch_level_step, 0},
      /*suffixDurationTicks*/ 0,
      /*suffixPitchOffsets*/ {},
      /*minSupportedNoteDurationTicks*/
      {::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks, ::mu::engraving::semiquaver_ticks}}},
};

const ArticulationTypeSet &OrnamentsRenderer::supported_types() {
    static const ArticulationTypeSet types = {ArticulationType::TRILL,
                                              ArticulationType::TRILL_BAROQUE,
                                              ArticulationType::LINE_PRALL,
                                              ArticulationType::UP_PRALL,
                                              ArticulationType::UP_MORDENT,
                                              ArticulationType::UPPER_MORDENT,
                                              ArticulationType::LOWER_MORDENT,
                                              ArticulationType::MORDENT_WITH_UPPER_PREFIX,
                                              ArticulationType::DOWN_MORDENT,
                                              ArticulationType::PRALL_UP,
                                              ArticulationType::PRALL_DOWN,
                                              ArticulationType::TURN,
                                              ArticulationType::INVERTED_TURN,
                                              ArticulationType::TREMBLEMENT,
                                              ArticulationType::PRALL_MORDENT,
                                              ArticulationType::UPPER_MORDENT_BAROQUE,
                                              ArticulationType::LOWER_MORDENT_BAROQUE};

    return types;
}

void OrnamentsRenderer::do_render(const EngravingItem *item, const ArticulationType preferred_type, const RenderingContext &ctx,
                                  mpe::PlaybackEventList &result) {
    const Chord *chord = to_chord(item);
    IF_ASSERT_FAILED(chord) { return; }

    auto search = ::disclosure_rules.find(preferred_type);
    if (search == ::disclosure_rules.end()) {
        return;
    }

    IntervalsInfo intervals_info;
    bool is_articulation = false;

    if (Ornament *ornament = chord->find_ornament(true)) {
        intervals_info = make_intervals_info(ornament->interval_below(), ornament->interval_above());
        is_articulation = muse::contains(chord->articulations(), to_articulation(ornament));
    } else {
        intervals_info = make_intervals_info(::mu::engraving::default_ornament_interval, ::mu::engraving::default_ornament_interval);
    }

    const DisclosurePattern &nominal_pattern = search->second;

    for (const Note *note : chord->notes()) {
        if (!NoteRenderer::should_render(note, ctx, ctx.common_articulations)) {
            continue;
        }

        RenderingContext ornament_ctx(ctx);
        if (is_articulation && note->tie_for()) {
            apply_tied_notes_duration(note, preferred_type, ornament_ctx);
        }

        NominalNoteCtx note_ctx(note, ornament_ctx);
        NoteArticulationsParser::build_note_articulation_map(note, note_ctx.chord_ctx, note_ctx.articulations);

        convert(preferred_type, nominal_pattern.build_actual_pattern(note, intervals_info, ornament_ctx.beats_per_second.val),
                std::move(note_ctx), result);
    }
}

void OrnamentsRenderer::apply_tied_notes_duration(const Note *note, const ArticulationType ornament_type, RenderingContext &ctx) {
    const Note *last_tied_note = note->last_tied_note(false);
    if (!last_tied_note || last_tied_note == note) {
        return;
    }

    ctx.nominal_position_end_tick = last_tied_note->chord()->end_tick().ticks();
    ctx.nominal_duration =
        timestamp_from_ticks(ctx.score, ctx.nominal_position_end_tick + ctx.position_tick_offset) - ctx.nominal_timestamp;
    ctx.nominal_duration_ticks = ctx.nominal_position_end_tick - ctx.nominal_position_start_tick;

    mpe::ArticulationMeta &meta = ctx.common_articulations.at(ornament_type).meta;
    meta.overall_duration = ctx.nominal_duration;
}

void OrnamentsRenderer::convert(const ArticulationType type, const DisclosurePattern &pattern, NominalNoteCtx &&note_ctx,
                                mpe::PlaybackEventList &result) {
    if (note_ctx.chord_ctx.nominal_duration_ticks <= pattern.boundaries.low_tempo_duration_ticks) {
        result.emplace_back(build_note_event(note_ctx));
        return;
    }

    // convert prefix
    if (!pattern.prefix_pitch_offsets.empty()) {
        create_events(type, note_ctx, 1, pattern.prefix_duration_ticks, note_ctx.chord_ctx.nominal_duration_ticks,
                      pattern.prefix_pitch_offsets, result);
    }

    // convert body
    if (!pattern.alteration_step_pitch_offsets.empty()) {
        int alterations_count = 1;

        if (pattern.is_alterations_repeat_allowed) {
            alterations_count =
                alterations_number_by_tempo(note_ctx.chord_ctx.beats_per_second.val, note_ctx.chord_ctx.nominal_duration_ticks, pattern);
        }

        if (alterations_count == 0) {
            result.emplace_back(build_note_event(note_ctx));
        } else {
            create_events(type, note_ctx, alterations_count,
                          note_ctx.chord_ctx.nominal_duration_ticks - pattern.prefix_duration_ticks - pattern.suffix_duration_ticks,
                          note_ctx.chord_ctx.nominal_duration_ticks, pattern.alteration_step_pitch_offsets, result);
        }
    }

    // convert suffix
    if (!pattern.suffix_pitch_offsets.empty()) {
        create_events(type, note_ctx, 1, pattern.suffix_duration_ticks, note_ctx.chord_ctx.nominal_duration_ticks,
                      pattern.suffix_pitch_offsets, result);
    }
}

int OrnamentsRenderer::alterations_number_by_tempo(const double beats_per_seconds, const int principal_note_duration_ticks,
                                                   const DisclosurePattern &pattern) {
    float sub_notes_count = principal_note_duration_ticks / pattern.sub_note_duration_ticks(beats_per_seconds);

    return static_cast<int>(std::max(sub_notes_count / 2, 0.f));
}

void OrnamentsRenderer::create_events(const ArticulationType type, NominalNoteCtx &note_ctx, const int alterations_count,
                                      const int available_duration_ticks, const int overall_duration_ticks,
                                      const std::vector<mpe::pitch_level_t> &pitch_offsets, mpe::PlaybackEventList &result) {
    float available_duration_ratio = available_duration_ticks / static_cast<float>(overall_duration_ticks);

    size_t total_notes_count = alterations_count * pitch_offsets.size();
    float duration_step = (note_ctx.duration * available_duration_ratio) / total_notes_count;

    track_idx_t track_idx = staff2track(note_ctx.staff_idx, note_ctx.voice_idx);

    for (int alteration_step = 0; alteration_step < alterations_count; ++alteration_step) {
        for (size_t alteration_sub_note_idx = 0; alteration_sub_note_idx < pitch_offsets.size(); ++alteration_sub_note_idx) {
            NominalNoteCtx sub_note_ctx(note_ctx);
            sub_note_ctx.duration = duration_step;
            sub_note_ctx.pitch_level += pitch_offsets.at(alteration_sub_note_idx);

            int utick = timestamp_to_tick(sub_note_ctx.chord_ctx.score, sub_note_ctx.timestamp);
            sub_note_ctx.dynamic_level = note_ctx.chord_ctx.playback_ctx->appliable_dynamic_level(track_idx, utick);

            update_articulation_boundaries(type, sub_note_ctx.timestamp, sub_note_ctx.duration, sub_note_ctx.articulations);

            result.emplace_back(build_note_event(sub_note_ctx));

            note_ctx.timestamp += duration_step;
        }
    }
}

float DisclosurePattern::sub_note_duration_ticks(const double bps) const {
    if (muse::real_is_equal_or_more(bps, ::mu::engraving::prestissimo_bps_bound)) {
        return boundaries.high_tempo_duration_ticks;
    }

    if (muse::real_is_equal_or_more(bps, ::mu::engraving::moderato_bps_bound)) {
        return boundaries.medium_tempo_duration_ticks;
    }

    return boundaries.low_tempo_duration_ticks;
}

DisclosurePattern DisclosurePattern::build_actual_pattern(const Note *note, const IntervalsInfo &intervals_info, const double bps) const {
    DisclosurePattern result = *this;

    result.update_pitch_offsets(note, intervals_info, result.prefix_pitch_offsets);
    result.update_pitch_offsets(note, intervals_info, result.alteration_step_pitch_offsets);
    result.update_pitch_offsets(note, intervals_info, result.suffix_pitch_offsets);

    float sub_note_ticks = sub_note_duration_ticks(bps);

    result.prefix_duration_ticks = muse::real_round(prefix_pitch_offsets.size() * sub_note_ticks, 0);
    result.suffix_duration_ticks = muse::real_round(suffix_pitch_offsets.size() * sub_note_ticks, 0);

    return result;
}

void DisclosurePattern::update_pitch_offsets(const Note *note, const IntervalsInfo &intervals_info,
                                             std::vector<mpe::pitch_level_t> &pitch_offsets) {
    for (pitch_level_t &pitch_offset : pitch_offsets) {
        if (pitch_offset == 0) {
            continue;
        }

        semitone_t semitones = 0;

        if (pitch_offset < 0) {
            if (intervals_info.interval_below_is_auto) {
                semitones = std::abs(chromatic_pitch_steps(note, note, -intervals_info.interval_below.diatonic));
            } else {
                semitones = intervals_info.interval_below.chromatic;
            }
        } else if (pitch_offset > 0) {
            if (intervals_info.interval_above_is_auto) {
                semitones = std::abs(chromatic_pitch_steps(note, note, intervals_info.interval_above.diatonic));
            } else {
                semitones = intervals_info.interval_above.chromatic;
            }
        }

        pitch_offset *= semitones;
    }
}
