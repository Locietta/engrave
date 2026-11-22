// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "../types/types.h"
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
    //! Shared by every freeze of this export, so the cancellation stride spans tracks and calls
    muse::mpe::FreezeProgress progress;

    //! Finds or creates the track; the reference is valid until the next track is created
    muse::mpe::TrackEvents &track(const InstrumentTrackId &id);
    const muse::mpe::TrackEvents *find(const InstrumentTrackId &id) const;
    // Drains nontrivial owners before freeing buffers. If a poll throws, finish cleanup and rethrow.
    void release(const std::function<void()> &checkpoint = {});
};

//! Freezes each completed list into the track's flat records; the list itself is left untouched
struct FlatEventsSink {
    muse::mpe::TrackEvents &track;
    muse::mpe::FreezeProgress &progress;

    void operator()(const muse::mpe::timestamp_t key, muse::mpe::PlaybackEventList &&list) const {
        muse::mpe::freeze_events(track, key, list, progress);
    }
};

static_assert(PlaybackEventSink<FlatEventsSink>);

//! Builds an export snapshot on the score's owner thread.
struct PlaybackScoreBuilder {
    // Resolved profile data, owned by this builder.
    muse::mpe::ArticulationProfileBundle profiles;
    bool play_repeats = true;
    bool metronome_enabled = false;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    // Test only: installed as FreezeProgress::dump_note for the whole build (new-path dump A)
    std::function<std::string(const muse::mpe::NoteEvent &)> dump_note;
#endif

    // checkpoint runs between walk steps and inside the freeze loops and may throw to abort; the partial store
    // is then discarded. Throws muse::mpe::StoreLimit when a buffer would leave the uint32 format. An empty
    // score gives an empty store
    PlaybackScore build(Score *score, const std::function<void()> &checkpoint = {}) const;
};
} // namespace mu::engraving
