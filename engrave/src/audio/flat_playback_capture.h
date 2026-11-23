// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Test-only capture around the flat MPE store of an export (playback DoD steps 4b-4c). This header and
// every call site are absent from ordinary production builds.
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include <string>

#include "engrave/src/global/fn.h"
#include "engraving/playback/flatplaybackscore.h"
#include "mpe/events.h"

namespace music {
struct FlatPlaybackCapture {
    // New-path dump A: serializes each NoteEvent as the builder freezes it (TrackEvents::note_dumps)
    using DumpNote = std::string(const muse::mpe::NoteEvent &);
    inline static UniqueFn<DumpNote> dump_note;
    using Projection = void(const mu::engraving::PlaybackScore &);
    inline static UniqueFn<Projection> projection;
};
} // namespace music
#endif
