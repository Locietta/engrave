// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <utility>
#include <vector>

#include "../types/types.h"
#include "mpe/events.h"
#include "mpe/flatevents.h"
#include "playbackeventsink.h"

namespace mu::engraving {
//! NOTE: the flat playback store of one export: one TrackEvents per legacy model track, sorted by id.
//! A track is created exactly when the legacy model creates its map entry during the shared walk
struct PlaybackScore {
    std::vector<std::pair<InstrumentTrackId, muse::mpe::TrackEvents>> tracks;

    //! Finds or creates the track; the reference is valid until the next track is created
    muse::mpe::TrackEvents &track(const InstrumentTrackId &id);
    const muse::mpe::TrackEvents *find(const InstrumentTrackId &id) const;
};

//! Freezes each completed list into the track's flat records; the list itself is left untouched
struct FlatEventsSink {
    muse::mpe::TrackEvents &track;

    void operator()(const muse::mpe::timestamp_t key, muse::mpe::PlaybackEventList &&list) const {
        muse::mpe::freeze_events(track, key, list);
    }
};

//! Dual execution: freezes the list, then hands it to the legacy map exactly as EventsMapSink alone
struct DualEventsSink {
    EventsMapSink legacy;
    FlatEventsSink flat;

    void operator()(const muse::mpe::timestamp_t key, muse::mpe::PlaybackEventList &&list) const {
        muse::mpe::freeze_events(flat.track, key, list);
        legacy(key, std::move(list));
    }
};

static_assert(PlaybackEventSink<FlatEventsSink>);
static_assert(PlaybackEventSink<DualEventsSink>);
} // namespace mu::engraving
