/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include <amc/vector.hpp>
#include <ankerl/unordered_dense.h>

#include "audio/common/audiotypes.h"
#include "midi/midievent.h"
#include "midi/miditypes.h"
#include "mpe/events.h"
#include "offlinecancellation.h"

namespace muse::audio::engine {
struct StoreLimit {
    const char *message = "playback buffer exceeds uint32 offset limit";
};

template <class T>
    requires std::is_trivially_copyable_v<T>
using Buf = amc::vector<T>;

template <class T = std::byte>
constexpr std::uint32_t checked_grow(std::uint64_t size, std::uint64_t count) {
    static_assert(std::is_trivially_copyable_v<T>);
    constexpr std::uint64_t limit = std::numeric_limits<std::uint32_t>::max();
    if (size > limit || count > limit - size) {
        throw StoreLimit{};
    }
    const std::uint64_t total = size + count;
    if (total > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
        throw StoreLimit{};
    }
    return static_cast<std::uint32_t>(total);
}

template <class T>
void reserve_checked(Buf<T> &buf, std::uint64_t count) {
    buf.reserve(checked_grow<T>(0, count));
}

template <class T, class... Args>
T &emplace_checked(Buf<T> &buf, Args &&...args) {
    std::ignore = checked_grow<T>(buf.size(), 1);
    return buf.emplace_back(std::forward<Args>(args)...);
}

struct TimedMidiEvent {
    mpe::timestamp_t timestamp = 0;
    midi::Event event;
};

static_assert(std::is_trivially_copyable_v<TimedMidiEvent>);
static_assert(sizeof(TimedMidiEvent) == 24);

struct CcKey {
    midi::Event::Opcode opcode = midi::Event::Opcode::CONTROL_CHANGE;
    midi::channel_t channel = 0;
    std::uint8_t controller = 0;
    std::uint32_t value = 0;

    bool operator==(const CcKey &other) const = default;
};

struct CcKeyHash {
    using is_avalanching = void;

    std::uint64_t operator()(const CcKey &key) const noexcept {
        const std::uint64_t packed = (static_cast<std::uint64_t>(key.opcode) << 48) | (static_cast<std::uint64_t>(key.channel) << 40) |
                                     (static_cast<std::uint64_t>(key.controller) << 32) | static_cast<std::uint64_t>(key.value);
        return ankerl::unordered_dense::hash<std::uint64_t>{}(packed);
    }
};

inline bool is_dedup_eligible_cc(const midi::Event &event) {
    return event.is_channel_voice() && event.opcode() == midi::Event::Opcode::CONTROL_CHANGE &&
           event.index() != static_cast<std::uint8_t>(midi::expression_controller);
}

enum class TimelineStream : std::uint8_t {
    MAIN = 0,
    OFF = 1,
};

struct ChunkSlice {
    mpe::timestamp_t at = 0;
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    TimelineStream stream = TimelineStream::MAIN;
};

static_assert(std::is_trivially_copyable_v<ChunkSlice>);

struct MidiTimeline {
    Buf<TimedMidiEvent> events;
    std::uint32_t cursor = 0;

    void clear() {
        events.clear();
        cursor = 0;
    }

    bool empty() const { return events.empty(); }

    void seek(const mpe::timestamp_t position) {
        const auto it =
            std::lower_bound(events.cbegin(), events.cend(), position,
                             [](const TimedMidiEvent &entry, const mpe::timestamp_t target) { return entry.timestamp < target; });
        cursor = static_cast<std::uint32_t>(it - events.cbegin());
    }
};

template <class... Types>
struct AbstractEventSequencer {
    AbstractEventSequencer() { reset_all_iterators(); }

    virtual ~AbstractEventSequencer() = default;

    void load(const mpe::PlaybackData &data) {

        dynamics_ = data.dynamics;
        invalidate_chunk_plan();

        update_main_stream_events(data.origin_events, data.dynamics);
    }

    void flush_offstream() { invalidate_chunk_plan(); }

    void set_active(const bool active) {
        if (is_active_ == active) {
            return;
        }

        is_active_ = active;
    }

    bool is_active() const { return is_active_; }

    void set_playback_position(const msecs_t new_playback_position) {
        invalidate_chunk_plan();
        playback_position_ = new_playback_position;
        reset_all_iterators();
    }

    msecs_t playback_position() const { return playback_position_; }

    using OnFlushedCallback = std::function<void()>;

    void set_on_off_stream_flushed(OnFlushedCallback flushed) { on_off_stream_flushed_ = flushed; }

    void set_on_main_stream_flushed(OnFlushedCallback flushed) { on_main_stream_flushed_ = flushed; }

    // Builds the slice plan for the next chunk from the stream active at the start of advance().
    // Each ChunkSlice records its originating stream. Any sequencer mutation (set_playback_position,
    // load, flush_offstream) invalidates chunk_plan_ so stale slices cannot index a mutated or cleared timeline.
    void advance(const msecs_t next_msecs, Buf<ChunkSlice> &plan) {
        plan.clear();

        if (!is_active_) {
            emplace_checked(plan, ChunkSlice{0, 0, 0, TimelineStream::OFF});
            return;
        }

        // Empty sequence means to continue the previous sequence
        emplace_checked(plan, ChunkSlice{playback_position_.raw(), main_stream_events_.cursor, 0, TimelineStream::MAIN});
        playback_position_ += next_msecs;

        if (main_stream_events_.cursor >= main_stream_events_.events.size()) {
            return;
        }

        collect_slices(main_stream_events_, TimelineStream::MAIN, playback_position_.raw(), plan);
    }

    const Buf<ChunkSlice> &move_playback_forward(const msecs_t next_msecs) {
        advance(next_msecs, chunk_plan_);
        chunk_plan_valid_ = true;
        return chunk_plan_;
    }

    std::span<const TimedMidiEvent> slice_events(const ChunkSlice &slice) const {
        if (!chunk_plan_valid_) {
            throw StoreLimit{"slice plan was invalidated by a sequencer state transition"};
        }
        if (slice.stream == TimelineStream::OFF) {
            if (slice.first != 0 || slice.count != 0) {
                throw StoreLimit{"slice range exceeds timeline size"};
            }
            return {};
        }
        const auto &events = main_stream_events_.events;
        const std::uint32_t end = checked_grow<TimedMidiEvent>(slice.first, slice.count);
        if (end > events.size()) {
            throw StoreLimit{"slice range exceeds timeline size"};
        }
        return {events.data() + slice.first, slice.count};
    }

protected:
    virtual void update_main_stream_events(const mpe::PlaybackEventsMap &events, const mpe::DynamicAutomationLayers &dynamics) = 0;

    static bool bounded_stable_sort(MidiTimeline &timeline) {
        OfflineCancellation::SequencerScope phase;
        constexpr std::uint32_t k_block_size = 8192;
        if (OfflineCancellation::requested()) {
            timeline.clear();
            return false;
        }
        auto &events = timeline.events;
        const auto total = static_cast<std::uint32_t>(events.size());
        if (total <= 1) {
            return true;
        }

        const auto by_timestamp = [](const TimedMidiEvent &lhs, const TimedMidiEvent &rhs) { return lhs.timestamp < rhs.timestamp; };

        for (std::uint32_t offset = 0; offset < total; offset += k_block_size) {
            if (offset > 0 && OfflineCancellation::requested()) {
                timeline.clear();
                return false;
            }
            const std::uint32_t block_end = std::min(total - offset, k_block_size) + offset;
            std::stable_sort(events.begin() + offset, events.begin() + block_end, by_timestamp);
        }

        if (OfflineCancellation::requested()) {
            timeline.clear();
            return false;
        }
        if (total <= k_block_size) {
            return true;
        }

        Buf<TimedMidiEvent> scratch;
        reserve_checked(scratch, total);
        if (OfflineCancellation::requested()) {
            timeline.clear();
            return false;
        }

        std::uint32_t merge_steps = 0;
        for (std::uint64_t width = k_block_size; width < total; width *= 2) {
            scratch.clear();
            for (std::uint64_t left = 0; left < total; left += 2 * width) {
                const auto mid = static_cast<std::uint32_t>(std::min<std::uint64_t>(left + width, total));
                const auto right = static_cast<std::uint32_t>(std::min<std::uint64_t>(left + 2 * width, total));
                auto i = static_cast<std::uint32_t>(left);
                std::uint32_t j = mid;
                while (i < mid && j < right) {
                    if (++merge_steps >= k_block_size) {
                        merge_steps = 0;
                        if (OfflineCancellation::requested()) {
                            timeline.clear();
                            return false;
                        }
                    }
                    if (events[j].timestamp < events[i].timestamp) {
                        emplace_checked(scratch, events[j++]);
                    } else {
                        emplace_checked(scratch, events[i++]);
                    }
                }
                while (i < mid) {
                    if (++merge_steps >= k_block_size) {
                        merge_steps = 0;
                        if (OfflineCancellation::requested()) {
                            timeline.clear();
                            return false;
                        }
                    }
                    emplace_checked(scratch, events[i++]);
                }
                while (j < right) {
                    if (++merge_steps >= k_block_size) {
                        merge_steps = 0;
                        if (OfflineCancellation::requested()) {
                            timeline.clear();
                            return false;
                        }
                    }
                    emplace_checked(scratch, events[j++]);
                }
            }
            std::swap(events, scratch);
        }
        return true;
    }

    static bool dedup_timeline(MidiTimeline &timeline) {
        OfflineCancellation::SequencerScope phase;
        constexpr std::uint32_t k_poll_stride = 4096;
        if (OfflineCancellation::requested()) {
            timeline.clear();
            return false;
        }
        auto &events = timeline.events;
        const auto total = static_cast<std::uint32_t>(events.size());
        if (total <= 1) {
            return true;
        }

        ankerl::unordered_dense::set<CcKey, CcKeyHash> seen;
        std::uint32_t write = 0;
        std::uint32_t work = 0;

        for (std::uint32_t bucket_start = 0; bucket_start < total;) {
            const mpe::timestamp_t bucket_time = events[bucket_start].timestamp;
            seen.clear();

            std::uint32_t i = bucket_start;
            for (; i < total && events[i].timestamp == bucket_time; ++i) {
                if (++work >= k_poll_stride) {
                    work = 0;
                    if (OfflineCancellation::requested()) {
                        timeline.clear();
                        return false;
                    }
                }
                const midi::Event &ev = events[i].event;
                if (is_dedup_eligible_cc(ev)) {
                    const CcKey key{ev.opcode(), ev.channel(), ev.index(), ev.data()};
                    if (!seen.insert(key).second) {
                        continue;
                    }
                }
                if (write != i) {
                    events[write] = events[i];
                }
                ++write;
            }
            bucket_start = i;
        }

        events.resize(write);
        return true;
    }

    void invalidate_chunk_plan() {
        chunk_plan_.clear();
        chunk_plan_valid_ = false;
    }

    void reset_all_iterators() { update_main_sequence_iterator(); }

    void update_main_sequence_iterator() { main_stream_events_.seek(playback_position_.raw()); }

    static void collect_slices(MidiTimeline &timeline, const TimelineStream stream, const mpe::timestamp_t limit, Buf<ChunkSlice> &plan) {
        const auto total = static_cast<std::uint32_t>(timeline.events.size());
        while (timeline.cursor < total && timeline.events[timeline.cursor].timestamp <= limit) {
            if (OfflineCancellation::requested()) return;
            const mpe::timestamp_t at = timeline.events[timeline.cursor].timestamp;
            const std::uint32_t first = timeline.cursor;
            while (timeline.cursor < total && timeline.events[timeline.cursor].timestamp == at) {
                if (OfflineCancellation::requested()) return;
                ++timeline.cursor;
            }
            const std::uint32_t count = timeline.cursor - first;
            if (plan.back().at == at) {
                if (plan.back().count == 0) {
                    plan.back().first = first;
                }
                plan.back().count = checked_grow<TimedMidiEvent>(plan.back().count, count);
                std::ignore = checked_grow<TimedMidiEvent>(plan.back().first, plan.back().count);
            } else {
                std::ignore = checked_grow<TimedMidiEvent>(first, count);
                emplace_checked(plan, ChunkSlice{at, first, count, stream});
            }
        }
    }

    mutable msecs_t playback_position_ = 0;

    MidiTimeline main_stream_events_;
    Buf<ChunkSlice> chunk_plan_;
    bool chunk_plan_valid_ = false;

    // Offline loading lowers MPE events once; only dynamics are needed by seeks.
    mpe::DynamicAutomationLayers dynamics_;

    bool is_active_ = false;

    OnFlushedCallback on_off_stream_flushed_;
    OnFlushedCallback on_main_stream_flushed_;
};
} // namespace muse::audio::engine
