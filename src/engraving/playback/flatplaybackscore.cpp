// SPDX-License-Identifier: GPL-3.0-only
#include "flatplaybackscore.h"

#include <algorithm>

#include "playbackeventsrenderer_impl.h"

namespace mu::engraving {
namespace {
bool track_less(const std::pair<InstrumentTrackId, muse::mpe::TrackEvents> &entry, const InstrumentTrackId &id) { return entry.first < id; }
} // namespace

muse::mpe::TrackEvents &PlaybackScore::track(const InstrumentTrackId &id) {
    auto it = std::lower_bound(tracks.begin(), tracks.end(), id, track_less);
    if (it == tracks.end() || it->first != id) {
        it = tracks.emplace(it, id, muse::mpe::TrackEvents{});
    }
    return it->second;
}

const muse::mpe::TrackEvents *PlaybackScore::find(const InstrumentTrackId &id) const {
    const auto it = std::lower_bound(tracks.begin(), tracks.end(), id, track_less);
    return it == tracks.end() || it->first != id ? nullptr : &it->second;
}
} // namespace mu::engraving

// The renderer entry points for the flat sinks live with the sinks, not in the renderer unit
ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER(mu::engraving::FlatEventsSink);
ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER(mu::engraving::DualEventsSink);
