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
#include <map>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <amc/vector.hpp>

#include "audio/common/audiotypes.h"
#include "midi/midievent.h"
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
    // Temporary staging container kept for step-3a build before flattening; step 3b replaces construction.
    using EventType = std::variant<Types...>;
    using EventSequence = std::vector<EventType>;
    using EventSequenceMap = std::map<msecs_t, EventSequence>;

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

    static void flatten_timeline(EventSequenceMap &source, MidiTimeline &timeline) {
        OfflineCancellation::SequencerScope phase;
        constexpr std::uint32_t k_poll_stride = 4096;
        timeline.clear();
        if (OfflineCancellation::requested()) {
            source.clear();
            return;
        }

        std::uint64_t total_events = 0;
        std::uint32_t scan_steps = 0;
        for (const auto &[_, sequence] : source) {
            if (++scan_steps == k_poll_stride) {
                scan_steps = 0;
                if (OfflineCancellation::requested()) {
                    source.clear();
                    return;
                }
            }
            total_events = checked_grow<TimedMidiEvent>(total_events, sequence.size());
        }
        if (OfflineCancellation::requested()) {
            source.clear();
            return;
        }
        reserve_checked(timeline.events, total_events);

        std::uint32_t work = 0;
        for (auto it = source.begin(); it != source.end();) {
            const mpe::timestamp_t timestamp = it->first.raw();
            for (const EventType &item : it->second) {
                if (++work >= k_poll_stride) {
                    work = 0;
                    if (OfflineCancellation::requested()) {
                        timeline.clear();
                        source.clear();
                        return;
                    }
                }
                emplace_checked(timeline.events, TimedMidiEvent{timestamp, std::get<midi::Event>(item)});
            }
            it = source.erase(it);
            if (++work >= k_poll_stride) {
                work = 0;
                if (OfflineCancellation::requested()) {
                    timeline.clear();
                    source.clear();
                    return;
                }
            }
        }
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
