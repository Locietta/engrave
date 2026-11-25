// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../types/types.h"
#include "global/fn.h"
#include "global/unique_fn.h"
#include "mpe/articulationprofilebundle.h"
#include "mpe/events.h"
#include "mpe/flatevents.h"
#include "playbackcontext.h"
#include "playbackeventsink.h"

namespace mu::engraving {
struct Score;

//! NOTE: the flat playback store of one export: one TrackEvents per legacy model track, sorted by id.
//! A track is created exactly when the legacy model creates its map entry during the shared walk
struct PlaybackScore {
    PlaybackContextPtr context;
    std::vector<std::pair<InstrumentTrackId, muse::mpe::TrackEvents>> tracks;

    //! Finds or creates the track; the reference is valid until the next track is created
    muse::mpe::TrackEvents &track(const InstrumentTrackId &id);
    const muse::mpe::TrackEvents *find(const InstrumentTrackId &id) const;
    // Drains nontrivial owners before freeing buffers. If a poll throws, finish cleanup and rethrow.
    void release(muse::FnRef<void()> checkpoint = {});
};

//! Builds an export snapshot on the score's owner thread.
struct PlaybackScoreBuilder {
    // Resolved profile data, owned by this builder.
    muse::mpe::ArticulationProfileBundle profiles;
    bool play_repeats = true;
    bool metronome_enabled = false;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    // Test only: installed as FreezeProgress::dump_note for the whole build (new-path dump A)
    muse::UniqueFn<std::string(const muse::mpe::TrackEvents &, const muse::mpe::NoteRecord &, const muse::mpe::ArticulationMap &) const>
        dump_note;
#endif

    // checkpoint runs between walk steps and inside the freeze loops and may throw to abort; the partial store
    // is then discarded. Throws muse::mpe::StoreLimit when a buffer would leave the uint32 format. An empty
    // score gives an empty store
    PlaybackScore build(Score *score, muse::FnRef<void()> checkpoint = {}) const;
};
} // namespace mu::engraving
