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

#include "bendsrenderer.h"

#include <algorithm>

#include "dom/guitarbend.h"
#include "dom/note.h"
#include "global/hashmap.h"
#include "gracechordcontext.h"
#include "noterenderer.h"
#include "playback/metaparsers/chordarticulationsparser.h"
#include "playback/utils/expressionutils.h"
#include "playback/utils/sortedtable.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static constexpr bool ignore_unplayable = false;

static const Chord *principal_chord(const Chord *chord) {
    if (chord->is_grace()) {
        const EngravingObject *parent = chord->ownership_parent();
        if (parent && parent->is_chord()) {
            return to_chord(parent);
        }
    }

    return chord;
}

static const GuitarBend *current_bend(const Note *curr_note) {
    const GuitarBend *dive_back = curr_note->dive_back();
    if (dive_back && dive_back->play_spanner() && dive_back->bend_type() == GuitarBendType::SCOOP) {
        return dive_back;
    }

    return curr_note->bend_for();
}

static const Note *next_note(const Note *curr_note, const GuitarBend *curr_bend) {
    return curr_bend && curr_bend->end_note() != curr_note ? curr_bend->end_note() : nullptr;
}

static mpe::pitch_level_t pitch_offset(const GuitarBend *bend) { return mpe::pitch_level_step / 2 * bend->bend_amount_in_quarter_tones(); }

bool BendsRenderer::is_multibend_part(const Note *note) {
    if (!note->tie_for() && !note->tie_back()) {
        return note->bend_for() || note->bend_back();
    }

    const Note *curr_note = note->first_tied_note(::ignore_unplayable);
    muse::HashSet<const Note *> visited;

    while (curr_note && curr_note->play()) {
        if (!visited.insert(curr_note).second) {
            break;
        }

        if (curr_note->bend_for() || note->bend_back()) {
            return true;
        }

        const Tie *tie_for = curr_note->tie_for();
        if (!tie_for || !tie_for->play_spanner()) {
            return false;
        }

        curr_note = tie_for->end_note();
    }

    return false;
}

void BendsRenderer::render(const Note *note, const RenderingContext &ctx, const NoteEmitter &emitter) {
    IF_ASSERT_FAILED(note) { return; }

    //! NOTE: ignore the grace note and render only the principal note
    if (note->is_pre_bend_or_dive_start()) {
        return;
    }

    if (const GuitarBend *bend_back = note->bend_back()) {
        static constexpr auto should_render_note_with_bend_back = mu::engraving::sorted_values<GuitarBendType>({
            GuitarBendType::PRE_BEND,
            GuitarBendType::PRE_DIVE,
            GuitarBendType::SCOOP,
        });

        if (!muse::contains(should_render_note_with_bend_back, bend_back->bend_type())) {
            return;
        }
    }

    if (!NoteRenderer::should_render(note, ctx, ctx.common_articulations)) {
        return;
    }

    render_multibend(note, ctx, emitter);
}

void BendsRenderer::render_multibend(const Note *start_note, const RenderingContext &start_note_ctx, const NoteEmitter &emitter) {
    const Note *curr_note = start_note;
    const GuitarBend *curr_bend = nullptr;
    muse::HashSet<const Note *> visited;

    BendCollector collector;
    const NoteEmitter collect{collector};
    BendTimeFactorMap bend_time_factor_map;

    while (curr_note) {
        if (!visited.insert(curr_note).second) {
            break;
        }

        RenderingContext curr_note_ctx = build_rendering_context(curr_note, start_note_ctx);
        if (!curr_note->tie_back() || !curr_note->tie_back()->play_spanner()) {
            render_note(curr_note, curr_note_ctx, collect);
        }

        curr_bend = current_bend(curr_note);

        // No bend on this note, but it's tied to the next one — check that note as well
        if (!curr_bend && curr_note->tie_for() && curr_note->tie_for()->play_spanner()) {
            curr_note = curr_note->tie_for()->end_note();
            continue;
        }

        if (!curr_bend || !curr_bend->play_spanner() || collector.points.empty()) {
            break;
        }

        if (curr_bend->bend_type() == GuitarBendType::SLIGHT_BEND) {
            render_slight_bend(curr_note, curr_bend, curr_note_ctx, collector);
            break;
        }

        if (curr_bend->bend_type() == GuitarBendType::DIP) {
            render_dip(curr_note, curr_bend, curr_note_ctx, collector);

            if (curr_note->tie_for() && curr_note->tie_for()->play_spanner()) {
                curr_note = curr_note->tie_for()->end_note();
                continue;
            }
            break;
        }

        if (curr_bend->bend_type() == GuitarBendType::SCOOP) {
            render_scoop(curr_note, curr_bend, curr_note_ctx, collector);
            curr_bend = curr_note->bend_for();
            curr_note = next_note(curr_note, curr_bend);
            continue;
        }

        const BendPoint &last_point = collector.points.back();
        const timestamp_t timestamp_to = last_point.actual_timestamp + last_point.actual_duration;
        bend_time_factor_map.insert_or_assign(timestamp_to, time_factors(curr_bend));

        curr_note = next_note(curr_note, curr_bend);
    }

    if (!collector.points.empty()) {
        emit_bend(start_note, start_note_ctx, collector, bend_time_factor_map, emitter);
    }
}

void BendsRenderer::render_note(const Note *note, const RenderingContext &ctx, const NoteEmitter &emitter) {
    const mpe::ArticulationType grace_type = select_grace_type(ctx.common_articulations);
    if (grace_type != mpe::ArticulationType::UNDEFINED) {
        // This note is either a grace note or a principal note for other grace notes
        const GraceChordCtx grace_ctx = GraceChordCtx::build_ctx(principal_chord(note->chord()), grace_type, ctx);

        if (note->is_grace()) {
            render_grace_note(note, grace_ctx, emitter);
        } else {
            NoteRenderer::render(note, grace_ctx.principal_chord_ctx, emitter);
        }

        return;
    }

    NoteRenderer::render(note, ctx, emitter);
}

void BendsRenderer::render_grace_note(const Note *note, const GraceChordCtx &ctx, const NoteEmitter &emitter) {
    for (const auto *list : {&ctx.grace_chord_ctx_list, &ctx.after_grace_chord_ctx_list}) {
        for (const auto &pair : *list) {
            for (const Note *grace_note : pair.first->notes()) {
                if (note == grace_note) {
                    NoteRenderer::render(note, pair.second, emitter);
                    return;
                }
            }
        }
    }
}

static pitch_level_t nominal_pitch(const Note *note) {
    return note_pitch_level(note->playing_tpc(), note->playing_octave(), note->playing_tuning());
}

void BendsRenderer::render_slight_bend(const Note *note, const GuitarBend *bend, const RenderingContext &ctx, BendCollector &collector) {
    const pitch_level_t origin_pitch = nominal_pitch(note);
    const BendTimeFactors factors = time_factors(bend);

    if (!real_is_null(factors.start_factor)) {
        const timestamp_t start = ctx.nominal_timestamp + ctx.nominal_duration * factors.start_factor;
        collector.points.push_back({start, 0, origin_pitch});
    }

    const timestamp_t end = ctx.nominal_timestamp + ctx.nominal_duration * factors.end_factor;
    collector.points.push_back({end, 0, origin_pitch + pitch_offset(bend)});
}

void BendsRenderer::render_dip(const Note *note, const GuitarBend *bend, const RenderingContext &ctx, BendCollector &collector) {
    const pitch_level_t origin_pitch = nominal_pitch(note);
    const BendTimeFactors factors = time_factors(bend);

    const timestamp_t start = ctx.nominal_timestamp + ctx.nominal_duration * factors.start_factor;
    collector.points.push_back({start, 0, origin_pitch});

    const timestamp_t target = ctx.nominal_timestamp + ctx.nominal_duration * factors.target_factor;
    collector.points.push_back({target, 0, origin_pitch + pitch_offset(bend)});

    if (!real_is_equal(factors.target_factor, factors.end_factor)) {
        const timestamp_t end = ctx.nominal_timestamp + ctx.nominal_duration * factors.end_factor;
        collector.points.push_back({end, 0, origin_pitch});
    }
}

void BendsRenderer::render_scoop(const Note *note, const GuitarBend *bend, const RenderingContext &ctx, BendCollector &collector) {
    const pitch_level_t origin_pitch = nominal_pitch(note);
    const pitch_level_t scooped_pitch = origin_pitch + pitch_offset(bend);
    const BendTimeFactors factors = time_factors(bend);

    collector.points.push_back({ctx.nominal_timestamp, 0, scooped_pitch});

    if (!real_is_null(factors.start_factor)) {
        const timestamp_t start = ctx.nominal_timestamp + ctx.nominal_duration * factors.start_factor;
        collector.points.push_back({start, 0, scooped_pitch});
    }

    const timestamp_t end = ctx.nominal_timestamp + ctx.nominal_duration * factors.end_factor;
    collector.points.push_back({end, 0, origin_pitch});
}

BendsRenderer::BendTimeFactors BendsRenderer::time_factors(const GuitarBend *bend) {
    const float start_factor = std::clamp(bend->start_time_factor(), 0.f, 1.f);
    const float end_factor = std::clamp(bend->end_time_factor(), 0.f, 1.f);

    float target_factor = end_factor;
    if (bend->target_time_factor().has_value()) {
        target_factor = std::clamp(bend->target_time_factor().value(), 0.f, 1.f);
    }

    IF_ASSERT_FAILED(real_is_equal_or_less(start_factor, end_factor) && real_is_equal_or_less(start_factor, target_factor) &&
                     real_is_equal_or_less(target_factor, end_factor)) {
        return BendTimeFactors{0.f, 1.f, 1.f};
    }

    return BendTimeFactors{start_factor, target_factor, end_factor};
}

RenderingContext BendsRenderer::build_rendering_context(const Note *note, const RenderingContext &initial_ctx) {
    // Use the principal chord to build the context
    const Chord *chord = principal_chord(note->chord());

    // Grace markers come only from this principal's own graces (parsed below), never from the chain's first chord
    mpe::ArticulationMap inherited = initial_ctx.common_articulations;
    for (const mpe::ArticulationType type : grace_type_precedence) {
        inherited.erase(type);
    }

    RenderingContext ctx = engraving::build_rendering_ctx(chord, initial_ctx.position_tick_offset, initial_ctx.profile,
                                                          initial_ctx.playback_ctx, std::move(inherited));

    ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);

    return ctx;
}

void BendsRenderer::emit_bend(const Note *start_note, const RenderingContext &start_note_ctx, BendCollector &collector,
                              const BendTimeFactorMap &time_factor_map, const NoteEmitter &emitter) {
    ArticulationMap &articulations = *collector.start_articulations;
    const BendPoint &first = collector.points.front();
    const timestamp_t timestamp = first.actual_timestamp;
    duration_t duration = first.actual_duration;
    const pitch_level_t pitch = nominal_pitch(start_note);

    PitchOffsets pitch_offsets;
    pitch_offsets.reserve(collector.points.size());

    auto multibend_it = articulations.find(ArticulationType::MULTIBEND);
    if (multibend_it != articulations.end()) {
        pitch_offsets.emplace_back(multibend_it->second.meta.timestamp, 0);
    }

    for (const BendPoint &point : std::span<const BendPoint>{collector.points}.subspan(1)) {
        if (point.actual_duration != 0) {
            duration = point.actual_timestamp + point.actual_duration - timestamp;
        }
        pitch_offsets.emplace_back(point.actual_timestamp, point.nominal_pitch_level - pitch);
    }

    if (multibend_it != articulations.end()) {
        ArticulationMeta &meta = multibend_it->second.meta;
        meta.timestamp = timestamp;
        meta.overall_duration = duration;
    }

    const auto curve = build_pitch_curve(timestamp, duration, pitch_offsets, time_factor_map);
    // The merged note applies the first note's articulation timing again to the combined duration.
    emitter.emit({timestamp, duration, static_cast<voice_layer_idx_t>(start_note->voice()),
                  static_cast<staff_layer_idx_t>(start_note->staff_idx()), pitch, start_note_ctx.nominal_dynamic_level,
                  start_note_ctx.beats_per_second.val, false, start_note->user_velocity_fraction()},
                 articulations, curve);
}

amc::vector<CurvePoint> BendsRenderer::build_pitch_curve(timestamp_t note_timestamp, duration_t note_duration,
                                                         const PitchOffsets &pitch_offsets, const BendTimeFactorMap &time_factor_map) {
    auto find_factors_at_time = [&time_factor_map](mpe::timestamp_t time) -> const BendTimeFactors & {
        auto it = muse::find_less_or_equal(time_factor_map, time);
        if (it == time_factor_map.end()) {
            static const BendTimeFactors dummy;
            return dummy;
        }

        return it->second;
    };

    amc::vector<CurvePoint> result{{0, 0}};
    const auto insert_or_assign = [&result](duration_percentage_t at, pitch_level_t value) {
        const auto pos = std::lower_bound(result.begin(), result.end(), at,
                                          [](const CurvePoint &point, duration_percentage_t key) { return point.at < key; });
        if (pos != result.end() && pos->at == at) {
            pos->value = value;
        } else {
            result.insert(pos, CurvePoint{at, value});
        }
    };

    mpe::percentage_t prev_nominal_offset_precent = 0;

    for (const auto &pair : pitch_offsets) {
        const BendTimeFactors &factors = find_factors_at_time(pair.first);
        const float ratio = static_cast<float>(pair.first - note_timestamp) / note_duration;

        const mpe::percentage_t nominal_offset_percent = static_cast<mpe::percentage_t>(ratio * 100.f) * mpe::one_percent;
        const mpe::percentage_t nominal_percent_diff = nominal_offset_percent - prev_nominal_offset_precent;

        const mpe::percentage_t actual_offset_start_percent = prev_nominal_offset_precent + nominal_percent_diff * factors.start_factor;
        const mpe::percentage_t actual_offset_end_percent = prev_nominal_offset_precent + nominal_percent_diff * factors.end_factor;

        prev_nominal_offset_precent = nominal_offset_percent;

        insert_or_assign(std::min(actual_offset_start_percent, mpe::hundred_percent), result.back().value);
        insert_or_assign(std::min(actual_offset_end_percent, mpe::hundred_percent), pair.second);
    }

    return result;
}
