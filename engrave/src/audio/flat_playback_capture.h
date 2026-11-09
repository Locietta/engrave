// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Test-only dual execution of the flat MPE store (playback DoD step 4b). This header and every
// call site are absent from ordinary production builds.
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include <functional>
#include <utility>
#include <vector>

#include "engraving/playback/flatplaybackscore.h"
#include "mpe/events.h"

namespace music {
struct FlatPlaybackCapture {
    using LegacyTracks = std::vector<std::pair<mu::engraving::InstrumentTrackId, const muse::mpe::PlaybackData *>>;

    // Load the model with a flat store fed by the same walk as the legacy map
    inline static bool dual = false;
    // After load, with the flat store and the legacy data of every model track in export order
    inline static std::function<void(const mu::engraving::PlaybackScore &, const LegacyTracks &)> projection;
};
} // namespace music
#endif
