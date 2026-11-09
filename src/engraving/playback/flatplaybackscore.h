// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "../types/types.h"
#include "mpe/events.h"
#include "mpe/flatevents.h"
#include "mpe/iarticulationprofilesrepository.h"
#include "playbackeventsink.h"

namespace mu::engraving {
struct Score;

//! NOTE: the flat playback store of one export: one TrackEvents per legacy model track, sorted by id.
//! A track is created exactly when the legacy model creates its map entry during the shared walk
struct PlaybackScore {
    std::vector<std::pair<InstrumentTrackId, muse::mpe::TrackEvents>> tracks;
    //! Shared by every freeze of this export, so the cancellation stride spans tracks and calls
    muse::mpe::FreezeProgress progress;

    //! Finds or creates the track; the reference is valid until the next track is created
    muse::mpe::TrackEvents &track(const InstrumentTrackId &id);
    const muse::mpe::TrackEvents *find(const InstrumentTrackId &id) const;
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

#ifdef ENGRAVE_PLAYBACK_HARNESS
//! Dual execution (test only): freezes the list, then hands it to the legacy map exactly as EventsMapSink alone
struct DualEventsSink {
    EventsMapSink legacy;
    FlatEventsSink flat;

    void operator()(const muse::mpe::timestamp_t key, muse::mpe::PlaybackEventList &&list) const {
        muse::mpe::freeze_events(flat.track, key, list, flat.progress);
        legacy(key, std::move(list));
    }
};

static_assert(PlaybackEventSink<DualEventsSink>);
#endif

//! Builds the flat store of one export from the shared score walk (playbackwalk.h), without the legacy
//! model: no score listener, no incremental-update bookkeeping. Owner thread only
struct PlaybackScoreBuilder {
    // profiles_repository may be null (no default articulation profiles); it must outlive build()
    const muse::mpe::IArticulationProfilesRepository *profiles_repository = nullptr;
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
