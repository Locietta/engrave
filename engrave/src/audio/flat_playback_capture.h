// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Test-only capture around the flat MPE store of an export (playback DoD steps 4b-4c). This header and
// every call site are absent from ordinary production builds.
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include <string>

#include "engraving/playback/flatplaybackscore.h"
#include "global/unique_fn.h"

namespace music {
struct FlatPlaybackCapture {
    using DumpNote = std::string(const muse::mpe::TrackEvents &, const muse::mpe::NoteRecord &, const muse::mpe::ArticulationMap &);
    inline static muse::UniqueFn<DumpNote> dump_note;
    using Projection = void(const mu::engraving::PlaybackScore &);
    inline static muse::UniqueFn<Projection> projection;
};
} // namespace music
#endif
