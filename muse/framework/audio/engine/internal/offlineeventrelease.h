// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "mpe/events.h"

namespace muse::audio {
// Conservative destruction work, including the fixed backing/control blocks.
inline constexpr std::size_t event_release_batch_cost = 512;
inline constexpr std::size_t event_release_base_cost = 8;

struct EventReleaseProgress {
    inline static thread_local std::size_t remaining_buckets = 0;
    inline static thread_local bool before_batch = false;
};

// Saturate before traversing a large graph. Const access never detaches COW data.
inline std::size_t event_release_cost(const mpe::PlaybackEvent &event) {
    std::size_t cost = event_release_base_cost;
    const auto add = [&](std::size_t count) {
        if (count > event_release_batch_cost - cost) return false;
        cost += count;
        return true;
    };
    const auto *note = std::get_if<mpe::NoteEvent>(&event);
    if (!note) return cost;
    const auto &expression = note->expression_ctx();
    const auto &arts = expression.articulations;
    if (!add(note->pitch_ctx().pitch_curve.size()) || !add(expression.expression_curve.size()) ||
        !add(arts.average_pitch_offset_map().size()) || !add(arts.average_dynamic_offset_map().size()) || !add(arts.size()))
        return event_release_batch_cost + 1;
    const auto segment_cost = [&](const mpe::ArticulationPatternSegment &segment) {
        return add(4) && add(segment.pitch_pattern.pitch_offset_map.size()) && add(segment.expression_pattern.dynamic_offset_map.size());
    };
    for (const auto &[type, art] : arts) {
        if (!add(8) || !add(art.meta.pattern.size()) || !segment_cost(art.applied_pattern_segment)) return event_release_batch_cost + 1;
        for (const auto &[at, segment] : art.meta.pattern) {
            if (!segment_cost(segment)) return event_release_batch_cost + 1;
        }
    }
    return cost;
}

template <typename Poll>
bool release_event_batches(mpe::PlaybackEventsMap &events, Poll &&poll, std::size_t *remaining_buckets = nullptr) {
    // Mutable access would detach and copy an aliased map. Leave it for teardown.
    if (!events.unique()) return true;
    while (!events.empty()) {
        EventReleaseProgress::before_batch = true;
        if (poll()) return false;
        std::size_t remaining = event_release_batch_cost;
        while (remaining && !events.empty()) {
            auto bucket = std::prev(events.end());
            auto &values = bucket->second;
            if (!values.empty()) {
                const auto cost = event_release_cost(values.back());
                // A single oversized graph cannot be freed within this budget.
                // Leave this track's remainder to ordinary teardown, without detaching it.
                if (cost > event_release_batch_cost) {
                    EventReleaseProgress::before_batch = false;
                    return !poll();
                }
                if (cost > remaining) break;
                values.pop_back();
                remaining -= cost;
            }
            if (values.empty() && remaining) {
                --remaining;
                events.erase(bucket);
                if (remaining_buckets) --*remaining_buckets;
            }
        }
    }
    EventReleaseProgress::before_batch = false;
    return !poll();
}
} // namespace muse::audio
