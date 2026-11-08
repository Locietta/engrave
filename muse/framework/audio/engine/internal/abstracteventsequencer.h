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

#include <functional>
#include <map>
#include <set>

#include "audio/common/audiotypes.h"
#include "mpe/events.h"
#include "offlinecancellation.h"

namespace muse::audio::engine {
template <class... Types>
struct AbstractEventSequencer {
    using EventType = std::variant<Types...>;
    using EventSequence = std::vector<EventType>;
    using EventSequenceMap = std::map<msecs_t, EventSequence>;

    typedef typename EventSequenceMap::const_iterator SequenceIterator;
    typedef typename EventSequence::const_iterator EventIterator;

    AbstractEventSequencer() { reset_all_iterators(); }

    virtual ~AbstractEventSequencer() = default;

    void load(const mpe::PlaybackData &data) {

        dynamics_ = data.dynamics;

        update_main_stream_events(data.origin_events, data.dynamics);
    }

    void flush_offstream() {
        if (off_stream_events_.empty()) {
            return;
        }

        off_stream_events_.clear();
        update_off_sequence_iterator();

        if (on_off_stream_flushed_) {
            on_off_stream_flushed_();
        }
    }

    void set_active(const bool active) {
        if (is_active_ == active) {
            return;
        }

        is_active_ = active;
    }

    bool is_active() const { return is_active_; }

    void set_playback_position(const msecs_t new_playback_position) {

        playback_position_ = new_playback_position;
        reset_all_iterators();
    }

    msecs_t playback_position() const { return playback_position_; }

    using OnFlushedCallback = std::function<void()>;

    void set_on_off_stream_flushed(OnFlushedCallback flushed) { on_off_stream_flushed_ = flushed; }

    void set_on_main_stream_flushed(OnFlushedCallback flushed) { on_main_stream_flushed_ = flushed; }

    EventSequenceMap move_playback_forward(const msecs_t next_msecs) {

        EventSequenceMap result;

        if (!is_active_) {
            result.emplace(offstream_position_, EventSequence());

            if (current_off_sequence_it_ == off_stream_events_.cend()) {
                return result;
            }

            offstream_position_ += next_msecs;
            handle_off_stream(result);

            return result;
        }

        // Empty sequence means to continue the previous sequence
        result.emplace(playback_position_, EventSequence());
        playback_position_ += next_msecs;

        if (current_main_sequence_it_ == main_stream_events_.cend()) {
            return result;
        }

        handle_main_stream(result);

        return result;
    }

protected:
    virtual void update_main_stream_events(const mpe::PlaybackEventsMap &events, const mpe::DynamicAutomationLayers &dynamics) = 0;
    virtual void update_off_stream_events(const mpe::PlaybackEventsMap &events) = 0;

    void reset_all_iterators() {
        update_main_sequence_iterator();
        update_off_sequence_iterator();
    }

    void update_main_sequence_iterator() { current_main_sequence_it_ = main_stream_events_.lower_bound(playback_position_); }

    void update_off_sequence_iterator() {
        current_off_sequence_it_ = off_stream_events_.cbegin();
        offstream_position_ = 0;
    }

    void handle_main_stream(EventSequenceMap &result) {
        while (current_main_sequence_it_ != main_stream_events_.end() && current_main_sequence_it_->first <= playback_position_) {
            if (OfflineCancellation::requested()) return;
            EventSequence &sequence = result[current_main_sequence_it_->first];
            for (const auto &event : current_main_sequence_it_->second) {
                if (OfflineCancellation::requested()) return;
                sequence.push_back(event);
            }
            current_main_sequence_it_ = std::next(current_main_sequence_it_);
        }
    }

    void handle_off_stream(EventSequenceMap &result) {
        if (off_stream_events_.empty()) {
            return;
        }

        while (current_off_sequence_it_ != off_stream_events_.end() && current_off_sequence_it_->first <= offstream_position_) {
            if (OfflineCancellation::requested()) return;
            EventSequence &sequence = result[current_off_sequence_it_->first];
            for (auto &event : current_off_sequence_it_->second) {
                if (OfflineCancellation::requested()) return;
                sequence.push_back(std::move(event));
            }
            current_off_sequence_it_ = off_stream_events_.erase(current_off_sequence_it_);
        }

        if (current_off_sequence_it_ == off_stream_events_.end()) {
            offstream_position_ = 0;
        }
    }

    mutable msecs_t playback_position_ = 0;
    mutable msecs_t offstream_position_ = 0;

    SequenceIterator current_main_sequence_it_;
    SequenceIterator current_off_sequence_it_;
    EventSequenceMap main_stream_events_;
    EventSequenceMap off_stream_events_;

    // Offline loading lowers MPE events once; only dynamics are needed by seeks.
    mpe::DynamicAutomationLayers dynamics_;

    bool is_active_ = false;

    OnFlushedCallback on_off_stream_flushed_;
    OnFlushedCallback on_main_stream_flushed_;
};
} // namespace muse::audio::engine
