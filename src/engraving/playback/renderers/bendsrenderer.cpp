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

#include "noterenderer.h"
#include "gracechordcontext.h"

#include "playback/metaparsers/chordarticulationsparser.h"
#include "playback/utils/expressionutils.h"

#include "dom/note.h"
#include "dom/guitarbend.h"

using namespace mu::engraving;
using namespace muse;

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
    std::unordered_set<const Note *> visited;

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

void BendsRenderer::render(const Note *note, const RenderingContext &ctx, mpe::PlaybackEventList &result) {
    IF_ASSERT_FAILED(note) { return; }

    //! NOTE: ignore the grace note and render only the principal note
    if (note->is_pre_bend_or_dive_start()) {
        return;
    }

    if (const GuitarBend *bend_back = note->bend_back()) {
        static const std::unordered_set<GuitarBendType> should_render_note_with_bend_back{
            GuitarBendType::PRE_BEND,
            GuitarBendType::PRE_DIVE,
            GuitarBendType::SCOOP,
        };

        if (!muse::contains(should_render_note_with_bend_back, bend_back->bend_type())) {
            return;
        }
    }

    if (!NoteRenderer::should_render(note, ctx, ctx.common_articulations)) {
        return;
    }

    render_multibend(note, ctx, result);
}

void BendsRenderer::render_multibend(const Note *start_note, const RenderingContext &start_note_ctx, mpe::PlaybackEventList &result) {
    const Note *curr_note = start_note;
    const GuitarBend *curr_bend = nullptr;
    std::unordered_set<const Note *> visited;

    mpe::PlaybackEventList bend_events;
    BendTimeFactorMap bend_time_factor_map;

    while (curr_note) {
        if (!visited.insert(curr_note).second) {
            break;
        }

        RenderingContext curr_note_ctx = build_rendering_context(curr_note, start_note_ctx);
        if (!curr_note->tie_back() || !curr_note->tie_back()->play_spanner()) {
            render_note(curr_note, curr_note_ctx, bend_events);
        }

        curr_bend = current_bend(curr_note);

        // No bend on this note, but it's tied to the next one — check that note as well
        if (!curr_bend && curr_note->tie_for() && curr_note->tie_for()->play_spanner()) {
            curr_note = curr_note->tie_for()->end_note();
            continue;
        }

        if (!curr_bend || !curr_bend->play_spanner() || bend_events.empty()) {
            break;
        }

        if (curr_bend->bend_type() == GuitarBendType::SLIGHT_BEND) {
            render_slight_bend(curr_note, curr_bend, curr_note_ctx, bend_events);
            break;
        }

        if (curr_bend->bend_type() == GuitarBendType::DIP) {
            render_dip(curr_note, curr_bend, curr_note_ctx, bend_events);

            if (curr_note->tie_for() && curr_note->tie_for()->play_spanner()) {
                curr_note = curr_note->tie_for()->end_note();
                continue;
            }
            break;
        }

        if (curr_bend->bend_type() == GuitarBendType::SCOOP) {
            render_scoop(curr_note, curr_bend, curr_note_ctx, bend_events);
            curr_bend = curr_note->bend_for();
            curr_note = next_note(curr_note, curr_bend);
            continue;
        }

        const mpe::PlaybackEvent &new_event = bend_events.back();
        if (std::holds_alternative<mpe::NoteEvent>(new_event)) {
            const mpe::ArrangementContext &arrangement_ctx = std::get<mpe::NoteEvent>(new_event).arrangement_ctx();
            const mpe::timestamp_t timestamp_to = arrangement_ctx.actual_timestamp + arrangement_ctx.actual_duration;
            bend_time_factor_map.insert_or_assign(timestamp_to, time_factors(curr_bend));
        }

        curr_note = next_note(curr_note, curr_bend);
    }

    if (!bend_events.empty()) {
        mpe::NoteEvent event = build_bend_event(start_note, start_note_ctx, bend_events, bend_time_factor_map);
        result.emplace_back(std::move(event));
    }
}

void BendsRenderer::render_note(const Note *note, const RenderingContext &ctx, mpe::PlaybackEventList &result) {
    for (const auto &pair : ctx.common_articulations) {
        if (!muse::contains(::mu::engraving::grace_note_articulation_types, pair.first)) {
            continue;
        }

        // This note is either a grace note or a principal note for other grace notes
        const GraceChordCtx grace_ctx = GraceChordCtx::build_ctx(principal_chord(note->chord()), pair.first, ctx);

        if (note->is_grace()) {
            render_grace_note(note, grace_ctx, result);
        } else {
            NoteRenderer::render(note, grace_ctx.principal_chord_ctx, result);
        }

        return;
    }

    NoteRenderer::render(note, ctx, result);
}

void BendsRenderer::render_grace_note(const Note *note, const GraceChordCtx &ctx, mpe::PlaybackEventList &result) {
    for (const auto &pair : ctx.grace_chord_ctx_list) {
        for (const Note *grace_note : pair.first->notes()) {
            if (note == grace_note) {
                NoteRenderer::render(note, pair.second, result);
                return;
            }
        }
    }
}

void BendsRenderer::render_slight_bend(const Note *note, const GuitarBend *bend, const RenderingContext &ctx,
                                       mpe::PlaybackEventList &result) {
    NominalNoteCtx slight_note_ctx(note, ctx);
    slight_note_ctx.duration = 0; // aux notes have no duration

    const BendTimeFactors factors = time_factors(bend);

    // Hold the origin pitch
    if (!real_is_null(factors.start_factor)) {
        slight_note_ctx.timestamp = ctx.nominal_timestamp + ctx.nominal_duration * factors.start_factor;
        result.emplace_back(build_note_event(slight_note_ctx));
    }

    // Go up
    slight_note_ctx.timestamp = ctx.nominal_timestamp + ctx.nominal_duration * factors.end_factor;
    slight_note_ctx.pitch_level += pitch_offset(bend);
    result.emplace_back(build_note_event(slight_note_ctx));
}

void BendsRenderer::render_dip(const Note *note, const GuitarBend *bend, const RenderingContext &ctx, mpe::PlaybackEventList &result) {
    NominalNoteCtx dip_note_ctx(note, ctx);
    dip_note_ctx.duration = 0; // aux notes have no duration

    const mpe::pitch_level_t origin_pitch_level = dip_note_ctx.pitch_level;
    const BendTimeFactors factors = time_factors(bend);

    // Hold the origin pitch
    dip_note_ctx.timestamp = ctx.nominal_timestamp + ctx.nominal_duration * factors.start_factor;
    result.emplace_back(build_note_event(dip_note_ctx));

    // Go down / up
    dip_note_ctx.pitch_level += pitch_offset(bend);
    dip_note_ctx.timestamp = ctx.nominal_timestamp + ctx.nominal_duration * factors.target_factor;
    result.emplace_back(build_note_event(dip_note_ctx));

    // Go back to the origin pitch
    if (!real_is_equal(factors.target_factor, factors.end_factor)) {
        dip_note_ctx.timestamp = ctx.nominal_timestamp + ctx.nominal_duration * factors.end_factor;
        dip_note_ctx.pitch_level = origin_pitch_level;
        result.emplace_back(build_note_event(dip_note_ctx));
    }
}

void BendsRenderer::render_scoop(const Note *note, const GuitarBend *bend, const RenderingContext &ctx, mpe::PlaybackEventList &result) {
    NominalNoteCtx scoop_note_ctx(note, ctx);
    scoop_note_ctx.duration = 0; // aux notes have no duration

    const mpe::pitch_level_t origin_pitch_level = scoop_note_ctx.pitch_level;
    const BendTimeFactors factors = time_factors(bend);

    // Start with a pitch offset
    scoop_note_ctx.pitch_level += pitch_offset(bend);
    result.emplace_back(build_note_event(scoop_note_ctx));

    // Hold it
    if (!real_is_null(factors.start_factor)) {
        scoop_note_ctx.timestamp = ctx.nominal_timestamp + ctx.nominal_duration * factors.start_factor;
        result.emplace_back(build_note_event(scoop_note_ctx));
    }

    // Go back to the origin pitch
    scoop_note_ctx.timestamp = ctx.nominal_timestamp + ctx.nominal_duration * factors.end_factor;
    scoop_note_ctx.pitch_level = origin_pitch_level;
    result.emplace_back(build_note_event(scoop_note_ctx));
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

    RenderingContext ctx = engraving::build_rendering_ctx(chord, initial_ctx.position_tick_offset, initial_ctx.profile,
                                                          initial_ctx.playback_ctx, initial_ctx.common_articulations);

    ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);

    return ctx;
}

mpe::NoteEvent BendsRenderer::build_bend_event(const Note *start_note, const RenderingContext &start_note_ctx,
                                               const mpe::PlaybackEventList &bend_note_events, const BendTimeFactorMap &time_factor_map) {
    NominalNoteCtx note_ctx(start_note, start_note_ctx);

    const mpe::NoteEvent &start_note_event = std::get<mpe::NoteEvent>(bend_note_events.front());
    note_ctx.articulations = start_note_event.expression_ctx().articulations;
    note_ctx.timestamp = start_note_event.arrangement_ctx().actual_timestamp;
    note_ctx.duration = start_note_event.arrangement_ctx().actual_duration;

    PitchOffsets pitch_offsets;
    pitch_offsets.reserve(bend_note_events.size());

    auto multibend_it = note_ctx.articulations.find(mpe::ArticulationType::MULTIBEND);
    if (multibend_it != note_ctx.articulations.end()) {
        const mpe::ArticulationMeta &meta = multibend_it->second.meta;
        pitch_offsets.emplace_back(meta.timestamp, 0);
    }

    for (size_t i = 1; i < bend_note_events.size(); ++i) {
        const mpe::PlaybackEvent &event = bend_note_events.at(i);
        if (!std::holds_alternative<mpe::NoteEvent>(event)) {
            continue;
        }

        const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
        const mpe::ArrangementContext &arrangement_ctx = note_event.arrangement_ctx();

        if (arrangement_ctx.actual_duration != 0) {
            note_ctx.duration = arrangement_ctx.actual_timestamp + arrangement_ctx.actual_duration - note_ctx.timestamp;
        }

        const mpe::pitch_level_t offset = note_event.pitch_ctx().nominal_pitch_level - note_ctx.pitch_level;
        pitch_offsets.emplace_back(arrangement_ctx.actual_timestamp, offset);
    }

    if (multibend_it != note_ctx.articulations.end()) {
        mpe::ArticulationMeta &meta = multibend_it->second.meta;
        meta.timestamp = note_ctx.timestamp;
        meta.overall_duration = note_ctx.duration;
    }

    mpe::PitchCurve curve = build_pitch_curve(note_ctx.timestamp, note_ctx.duration, pitch_offsets, time_factor_map);
    mpe::NoteEvent result = build_note_event(note_ctx, curve);

    return result;
}

mpe::PitchCurve BendsRenderer::build_pitch_curve(mpe::timestamp_t note_timestamp, mpe::duration_t note_duration,
                                                 const PitchOffsets &pitch_offsets, const BendTimeFactorMap &time_factor_map) {
    auto find_factors_at_time = [&time_factor_map](mpe::timestamp_t time) -> const BendTimeFactors & {
        auto it = muse::find_less_or_equal(time_factor_map, time);
        if (it == time_factor_map.end()) {
            static const BendTimeFactors dummy;
            return dummy;
        }

        return it->second;
    };

    mpe::PitchCurve result;
    result.emplace(0, 0);

    mpe::percentage_t prev_nominal_offset_precent = 0;

    for (const auto &pair : pitch_offsets) {
        const BendTimeFactors &factors = find_factors_at_time(pair.first);
        const float ratio = static_cast<float>(pair.first - note_timestamp) / note_duration;

        const mpe::percentage_t nominal_offset_percent = static_cast<mpe::percentage_t>(ratio * 100.f) * mpe::one_percent;
        const mpe::percentage_t nominal_percent_diff = nominal_offset_percent - prev_nominal_offset_precent;

        const mpe::percentage_t actual_offset_start_percent = prev_nominal_offset_precent + nominal_percent_diff * factors.start_factor;
        const mpe::percentage_t actual_offset_end_percent = prev_nominal_offset_precent + nominal_percent_diff * factors.end_factor;

        prev_nominal_offset_precent = nominal_offset_percent;

        const auto &prev_offset = result.rbegin();
        result.insert_or_assign(std::min(actual_offset_start_percent, mpe::hundred_percent), prev_offset->second);
        result.insert_or_assign(std::min(actual_offset_end_percent, mpe::hundred_percent), pair.second);
    }

    return result;
}
