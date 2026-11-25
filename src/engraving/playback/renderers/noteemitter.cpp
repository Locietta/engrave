// SPDX-License-Identifier: GPL-3.0-only
#include "noteemitter.h"

#include "playback/renderingcontext.h"

namespace mu::engraving {
using namespace muse::mpe;

void NoteEmitter::emit(const NoteEmitInput &input, const ArticulationMap &articulations,
                       std::span<const CurvePoint> custom_pitch_curve) const {
    if (track_) {
        emit_note(*track_, group_timestamp_, input, articulations, custom_pitch_curve, *progress_);
        return;
    }

    const NoteTiming timing = compute_note_timing(input, articulations);
    if (collector_->points.empty()) {
        collector_->start_articulations.emplace(articulations);
    }
    collector_->points.push_back({timing.actual_timestamp, timing.actual_duration, input.nominal_pitch_level});
}

void NoteEmitter::emit(const NominalNoteCtx &ctx, const bool clamp_negative_timestamp) const {
    emit({ctx.timestamp, ctx.duration, static_cast<voice_layer_idx_t>(ctx.voice_idx), static_cast<staff_layer_idx_t>(ctx.staff_idx),
          ctx.pitch_level, ctx.dynamic_level, ctx.tempo.val, clamp_negative_timestamp, ctx.user_velocity_fraction},
         ctx.articulations);
}
} // namespace mu::engraving
