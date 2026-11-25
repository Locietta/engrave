// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <optional>
#include <span>

#include <amc/vector.hpp>

#include "mpe/flatevents.h"

namespace mu::engraving {
struct NominalNoteCtx;

struct BendPoint {
    muse::mpe::timestamp_t actual_timestamp;
    muse::mpe::duration_t actual_duration;
    muse::mpe::pitch_level_t nominal_pitch_level;
};

struct BendCollector {
    amc::vector<BendPoint> points;
    std::optional<muse::mpe::ArticulationMap> start_articulations;
};

struct NoteEmitter {
    NoteEmitter(muse::mpe::TrackEvents &track, muse::mpe::timestamp_t group_timestamp, muse::mpe::FreezeProgress &progress)
        : track_(&track), progress_(&progress), group_timestamp_(group_timestamp) {}

    explicit NoteEmitter(BendCollector &collector) : collector_(&collector) {}

    explicit NoteEmitter(muse::mpe::PlaybackEventList &legacy_events) : legacy_events_(&legacy_events) {}

    void emit(const muse::mpe::NoteEmitInput &input, const muse::mpe::ArticulationMap &articulations,
              std::span<const muse::mpe::CurvePoint> custom_pitch_curve = {}) const;
    void emit(const NominalNoteCtx &ctx, bool clamp_negative_timestamp = false) const;

private:
    muse::mpe::TrackEvents *track_ = nullptr;
    muse::mpe::FreezeProgress *progress_ = nullptr;
    muse::mpe::timestamp_t group_timestamp_ = 0;
    BendCollector *collector_ = nullptr;
    muse::mpe::PlaybackEventList *legacy_events_ = nullptr;
};
} // namespace mu::engraving
