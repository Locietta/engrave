/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#ifndef MUSE_AUDIO_FLUIDSEQUENCER_H
#define MUSE_AUDIO_FLUIDSEQUENCER_H

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "../../miditimeline.h"
#include "midi/midievent.h"
#include "mpe/events.h"
#include "soundmapping.h"

namespace muse::audio::synth {
using engine::Buf;
using engine::CcKey;
using engine::CcKeyHash;
using engine::checked_grow;
using engine::ChunkSlice;
using engine::emplace_checked;
using engine::is_dedup_eligible_cc;
using engine::MidiTimeline;
using engine::reserve_checked;
using engine::StoreLimit;
using engine::TimedMidiEvent;
using engine::TimelineStream;

struct FluidSequencer {
    FluidSequencer() { seek_main_timeline(); }

    // Lowers one track of the flat MPE store; the sequencer keeps only a copy of its dynamics, and
    // nothing borrowed from the store outlives this call
    void load(const mpe::TrackEvents &events) {
        OfflineCancellation::SequencerScope phase;
        invalidate_chunk_plan();
        const auto copy = [](auto &destination, const auto &source) {
            destination.clear();
            mpe::reserve_checked(destination, source.size());
            for (std::size_t i = 0; i < source.size(); ++i) {
                if (i % 4096 == 4095 && OfflineCancellation::requested()) return false;
                mpe::emplace_checked(destination, source[i]);
            }
            return true;
        };
        if (!copy(dynamic_layers_, events.dynamic_layers) || !copy(dynamic_points_, events.dynamic_points)) {
            dynamic_layers_.clear();
            dynamic_points_.clear();
            main_stream_events_.clear();
            seek_main_timeline();
            return;
        }

        update_main_stream_events(events);
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
        seek_main_timeline();
    }

    msecs_t playback_position() const { return playback_position_; }

    using OnFlushedCallback = std::function<void()>;

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

    static constexpr std::uint32_t k_sort_block_size = 8192;

    static constexpr std::uint32_t next_sort_block_end(const std::uint32_t offset, const std::uint32_t total) noexcept {
        return offset + std::min(total - offset, k_sort_block_size);
    }

    static bool bounded_stable_sort(MidiTimeline &timeline) {
        const bool sorted = bounded_stable_sort_by(timeline.events, [](const TimedMidiEvent &entry) { return entry.timestamp; });
        if (!sorted) {
            timeline.clear();
        }
        return sorted;
    }

    //! The bounded stable sort of design 4.4 over any trivially copyable buffer, ordered by key(entry): each
    //! k_sort_block_size block is stably sorted, then blocks are merged with ties to the lower block, polling
    //! between blocks and every k_sort_block_size merge steps. Equals std::stable_sort. Returns false when
    //! cancelled, with the buffer in an unspecified order
    template <class T, class Key>
    static bool bounded_stable_sort_by(Buf<T> &events, Key &&key) {
        OfflineCancellation::SequencerScope phase;
        constexpr std::uint32_t k_block_size = k_sort_block_size;
        if (OfflineCancellation::requested()) {
            return false;
        }
        const auto total = static_cast<std::uint32_t>(events.size());
        if (total <= 1) {
            return true;
        }

        const auto by_key = [&key](const T &lhs, const T &rhs) { return key(lhs) < key(rhs); };

        for (std::uint32_t offset = 0; offset < total;) {
            if (offset > 0 && OfflineCancellation::requested()) {
                return false;
            }
            const std::uint32_t block_end = next_sort_block_end(offset, total);
            std::stable_sort(events.begin() + offset, events.begin() + block_end, by_key);
            offset = block_end;
        }

        if (OfflineCancellation::requested()) {
            return false;
        }
        if (total <= k_block_size) {
            return true;
        }

        Buf<T> scratch;
        reserve_checked(scratch, total);
        if (OfflineCancellation::requested()) {
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
                            return false;
                        }
                    }
                    if (key(events[j]) < key(events[i])) {
                        emplace_checked(scratch, events[j++]);
                    } else {
                        emplace_checked(scratch, events[i++]);
                    }
                }
                while (i < mid) {
                    if (++merge_steps >= k_block_size) {
                        merge_steps = 0;
                        if (OfflineCancellation::requested()) {
                            return false;
                        }
                    }
                    emplace_checked(scratch, events[i++]);
                }
                while (j < right) {
                    if (++merge_steps >= k_block_size) {
                        merge_steps = 0;
                        if (OfflineCancellation::requested()) {
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

    void seek_main_timeline() { main_stream_events_.seek(playback_position_.raw()); }

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

    // Offline loading lowers MPE events once; only dynamics are needed by seeks (copied out of the flat store).
    Buf<mpe::DynamicLayer> dynamic_layers_; // ascending layer
    Buf<mpe::DynamicPoint> dynamic_points_;

    std::span<const mpe::DynamicPoint> layer_points(const mpe::DynamicLayer &layer) const {
        return mpe::checked_span(dynamic_points_, layer.points);
    }

    bool is_active_ = false;

    OnFlushedCallback on_main_stream_flushed_;

public:
    void init(const mpe::PlaybackSetupData &setup_data, const std::optional<midi::Program> &program_override, bool use_dynamic_events,
              midi::channel_t max_channels);

    struct ChannelExpressionLevel {
        midi::channel_t channel = 0;
        int level = 0;
    };

    bool uses_dynamic_events() const;
    std::vector<ChannelExpressionLevel> current_expression_levels() const;
    int natural_expression_level() const;

    const ChannelMap &channels() const;
    void set_on_channel_added(std::function<void(midi::channel_t, const midi::Program &)> on_channel_added);
    int last_staff() const;

private:
    void update_main_stream_events(const mpe::TrackEvents &events);

    using SostenutoTimeAndDurations = std::array<engine::Buf<mpe::TimestampAndDuration>, k_fluid_channel_count>;

    //! A note's position in (group, seq) order: its legacy group key and its index in TrackEvents::notes
    struct NoteOrder {
        mpe::timestamp_t group_timestamp = 0;
        std::uint32_t index = 0;
    };

    bool add_playback_events(engine::MidiTimeline &destination, const mpe::TrackEvents &events, bool is_main_stream);
    bool add_dynamic_events(engine::MidiTimeline &destination);
    bool add_note_event(engine::MidiTimeline &destination, const mpe::TrackEvents &events, const mpe::NoteRecord &note,
                        SostenutoTimeAndDurations &sostenuto_time_and_durations, bool is_main_stream);
    void add_pedal_event(engine::MidiTimeline &destination, const mpe::ArticulationRecord &articulation, midi::channel_t channel_idx);
    void add_control_change(engine::MidiTimeline &destination, mpe::timestamp_t timestamp, int midi_control_idx,
                            midi::channel_t channel_idx, std::uint32_t value);
    bool add_pitch_curve(engine::MidiTimeline &destination, const mpe::NoteRecord &note, std::span<const mpe::CurvePoint> pitch_curve,
                         const mpe::ArticulationRecord &articulation, midi::channel_t channel_idx);
    void add_pitch_bend(engine::MidiTimeline &destination, mpe::timestamp_t timestamp, midi::channel_t channel_idx, std::uint32_t value);
    bool add_sostenuto_events(engine::MidiTimeline &destination, const SostenutoTimeAndDurations &sostenuto_time_and_durations);

    midi::note_idx_t note_index(mpe::pitch_level_t pitch_level) const;
    midi::tuning_t note_tuning(mpe::pitch_level_t nominal_pitch_level, int note_idx) const;
    midi::velocity_t note_velocity(const mpe::NoteRecord &note, std::span<const mpe::CurvePoint> expression_curve) const;
    int expression_level(mpe::dynamic_level_t dynamic_level) const;
    int pitch_bend_level(mpe::pitch_level_t pitch_level) const;

    mutable ChannelMap channels_;
    bool use_dynamic_events_ = false;
    int last_staff_ = -1;
};
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_FLUIDSEQUENCER_H
