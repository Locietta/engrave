// SPDX-License-Identifier: GPL-3.0-only
#include "noteemitter.h"

#include <utility>

#include "playback/renderingcontext.h"

namespace mu::engraving {
using namespace muse::mpe;

void NoteEmitter::emit(const NoteEmitInput &input, const ArticulationMap &articulations,
                       std::span<const CurvePoint> custom_pitch_curve) const {
    if (track_) {
        emit_note(*track_, group_timestamp_, input, articulations, custom_pitch_curve, *progress_);
        return;
    }

    if (collector_) {
        const NoteTiming timing = compute_note_timing(input, articulations);
        if (collector_->points.empty()) {
            collector_->start_articulations.emplace(articulations);
        }
        collector_->points.push_back({timing.actual_timestamp, timing.actual_duration, input.nominal_pitch_level});
        return;
    }

    // The legacy renderer tests still assert against NoteEvent contexts.
    PitchCurve curve;
    for (const auto &[at, value] : custom_pitch_curve) {
        curve.emplace(at, value);
    }
    muse::mpe::NoteEvent event(input.nominal_timestamp, input.nominal_duration, input.voice_layer, input.staff_layer,
                               input.nominal_pitch_level, input.nominal_dynamic_level, articulations, input.bps,
                               input.required_velocity_fraction, std::move(curve));
    if (input.clamp_negative_timestamp && event.arrangement_ctx().actual_timestamp < 0) {
        ArrangementContext arrangement = event.arrangement_ctx();
        arrangement.actual_duration += arrangement.actual_timestamp;
        arrangement.actual_timestamp = 0;
        PitchContext pitch = event.pitch_ctx();
        ExpressionContext expression = event.expression_ctx();
        legacy_events_->emplace_back(std::in_place_type<muse::mpe::NoteEvent>, std::move(arrangement), std::move(pitch),
                                     std::move(expression));
    } else {
        legacy_events_->emplace_back(std::move(event));
    }
}

void NoteEmitter::emit(const NominalNoteCtx &ctx, const bool clamp_negative_timestamp) const {
    emit({ctx.timestamp, ctx.duration, static_cast<voice_layer_idx_t>(ctx.voice_idx), static_cast<staff_layer_idx_t>(ctx.staff_idx),
          ctx.pitch_level, ctx.dynamic_level, ctx.tempo.val, clamp_negative_timestamp, ctx.user_velocity_fraction},
         ctx.articulations);
}
} // namespace mu::engraving
