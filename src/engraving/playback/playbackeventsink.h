// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <concepts>
#include <iterator>
#include <utility>

#include "mpe/events.h"

namespace mu::engraving {
//! NOTE: receives each completed event list with its group key (the legacy PlaybackEventsMap key),
//! as sink(key, std::move(list)). A list may be empty: the key is still emitted. The caller may
//! clear() and reuse the moved-from list
template <typename Sink>
concept PlaybackEventSink =
    requires(Sink &sink, muse::mpe::timestamp_t key, muse::mpe::PlaybackEventList list) { sink(key, std::move(list)); };

//! NOTE: legacy-compatible sink: appends to map[key], creating the key for an empty list too,
//! exactly as the direct insertions it replaces
struct EventsMapSink {
    muse::mpe::PlaybackEventsMap &events;

    void operator()(const muse::mpe::timestamp_t key, muse::mpe::PlaybackEventList &&list) const {
        muse::mpe::PlaybackEventList &destination = events[key];
        destination.insert(destination.end(), std::make_move_iterator(list.begin()), std::make_move_iterator(list.end()));
    }
};

static_assert(PlaybackEventSink<EventsMapSink>);
} // namespace mu::engraving
