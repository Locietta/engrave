// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "mpe/events.h"

namespace muse::audio {
// Calibrated on Windows dense and huge-bucket inputs; see step-2 evidence.
inline constexpr std::size_t event_release_batch_size = 128;

template <typename Poll>
bool release_event_batches(mpe::PlaybackEventsMap &events, Poll &&poll) {
    // Mutable access would detach and copy an aliased map. Leave it for teardown.
    if (!events.unique()) return true;
    while (!events.empty()) {
        if (poll()) return false;
        std::size_t remaining = event_release_batch_size;
        while (remaining && !events.empty()) {
            auto bucket = std::prev(events.end());
            auto &values = bucket->second;
            const auto count = std::min(remaining, values.size());
            for (std::size_t i = 0; i < count; ++i) values.pop_back();
            // Empty buckets also consume a unit, bounding their erase work.
            remaining -= std::max(count, std::size_t{1});
            if (values.empty()) events.erase(bucket);
        }
    }
    return !poll();
}
} // namespace muse::audio
