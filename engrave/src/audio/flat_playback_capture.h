// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Test-only capture around the flat MPE store of an export (playback DoD steps 4b-4c). This header and
// every call site are absent from ordinary production builds.
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "engraving/playback/flatplaybackscore.h"
#include "mpe/events.h"

namespace music {
struct FlatPlaybackCapture {
    using LegacyTracks = std::vector<std::pair<mu::engraving::InstrumentTrackId, const muse::mpe::PlaybackData *>>;

    // New-path dump A: serializes each NoteEvent as the builder freezes it (TrackEvents::note_dumps)
    inline static std::function<std::string(const muse::mpe::NoteEvent &)> dump_note;
    // Dump B: after the build, with the export's flat store and the data of every track of a legacy
    // PlaybackModel (the test oracle), loaded from the same score only when this hook is set, in id order
    inline static std::function<void(const mu::engraving::PlaybackScore &, const LegacyTracks &)> projection;
};
} // namespace music
#endif
