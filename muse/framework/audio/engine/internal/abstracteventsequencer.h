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
#include "offlinecancellation.h"
#include <map>
#include <set>

#include "mpe/events.h"

#include "audio/common/audiotypes.h"

namespace muse::audio::engine {
template <class... Types>
class AbstractEventSequencer {
public:
    using EventType = std::variant<Types...>;
    using EventSequence = std::vector<EventType>;
    using EventSequenceMap = std::map<msecs_t, EventSequence>;

    typedef typename EventSequenceMap::const_iterator SequenceIterator;
    typedef typename EventSequence::const_iterator EventIterator;

    AbstractEventSequencer() { resetAllIterators(); }

    virtual ~AbstractEventSequencer() = default;

    void load(const mpe::PlaybackData &data) {

        playbackData_ = data;

        updateMainStreamEvents(data.originEvents, data.dynamics);
    }

    const mpe::PlaybackData &playbackData() const { return playbackData_; }

    void updateMainStream() {
        if (shouldUpdateMainStreamEvents_) {
            updateMainStreamEvents(playbackData_.originEvents, playbackData_.dynamics);
            shouldUpdateMainStreamEvents_ = false;
        }
    }

    void setUpdateMainStreamWhenInactive(bool update) { updateMainStreamWhenInactive_ = update; }

    void flushOffstream() {
        if (offStreamEvents_.empty()) {
            return;
        }

        offStreamEvents_.clear();
        updateOffSequenceIterator();

        if (onOffStreamFlushed_) {
            onOffStreamFlushed_();
        }
    }

    void setActive(const bool active) {
        if (isActive_ == active) {
            return;
        }

        isActive_ = active;

        if (isActive_) {
            updateMainStream();
        }
    }

    bool isActive() const { return isActive_; }

    void setPlaybackPosition(const msecs_t newPlaybackPosition) {

        playbackPosition_ = newPlaybackPosition;
        resetAllIterators();
    }

    msecs_t playbackPosition() const { return playbackPosition_; }

    using OnFlushedCallback = std::function<void()>;

    void setOnOffStreamFlushed(OnFlushedCallback flushed) { onOffStreamFlushed_ = flushed; }

    void setOnMainStreamFlushed(OnFlushedCallback flushed) { onMainStreamFlushed_ = flushed; }

    EventSequenceMap movePlaybackForward(const msecs_t nextMsecs) {

        EventSequenceMap result;

        if (!isActive_) {
            result.emplace(offstreamPosition_, EventSequence());

            if (currentOffSequenceIt_ == offStreamEvents_.cend()) {
                return result;
            }

            offstreamPosition_ += nextMsecs;
            handleOffStream(result);

            return result;
        }

        // Empty sequence means to continue the previous sequence
        result.emplace(playbackPosition_, EventSequence());
        playbackPosition_ += nextMsecs;

        if (currentMainSequenceIt_ == mainStreamEvents_.cend()) {
            return result;
        }

        handleMainStream(result);

        return result;
    }

protected:
    virtual void updateMainStreamEvents(const mpe::PlaybackEventsMap &events, const mpe::DynamicAutomationLayers &dynamics) = 0;
    virtual void updateOffStreamEvents(const mpe::PlaybackEventsMap &events) = 0;

    void resetAllIterators() {
        updateMainSequenceIterator();
        updateOffSequenceIterator();
    }

    void updateMainSequenceIterator() { currentMainSequenceIt_ = mainStreamEvents_.lower_bound(playbackPosition_); }

    void updateOffSequenceIterator() {
        currentOffSequenceIt_ = offStreamEvents_.cbegin();
        offstreamPosition_ = 0;
    }

    void handleMainStream(EventSequenceMap &result) {
        while (currentMainSequenceIt_ != mainStreamEvents_.end() && currentMainSequenceIt_->first <= playbackPosition_) {
            if (OfflineCancellation::requested()) return;
            EventSequence &sequence = result[currentMainSequenceIt_->first];
            for (const auto &event : currentMainSequenceIt_->second) {
                if (OfflineCancellation::requested()) return;
                sequence.push_back(event);
            }
            currentMainSequenceIt_ = std::next(currentMainSequenceIt_);
        }
    }

    void handleOffStream(EventSequenceMap &result) {
        if (offStreamEvents_.empty()) {
            return;
        }

        while (currentOffSequenceIt_ != offStreamEvents_.end() && currentOffSequenceIt_->first <= offstreamPosition_) {
            if (OfflineCancellation::requested()) return;
            EventSequence &sequence = result[currentOffSequenceIt_->first];
            for (auto &event : currentOffSequenceIt_->second) {
                if (OfflineCancellation::requested()) return;
                sequence.push_back(std::move(event));
            }
            currentOffSequenceIt_ = offStreamEvents_.erase(currentOffSequenceIt_);
        }

        if (currentOffSequenceIt_ == offStreamEvents_.end()) {
            offstreamPosition_ = 0;
        }
    }

    mutable msecs_t playbackPosition_ = 0;
    mutable msecs_t offstreamPosition_ = 0;

    SequenceIterator currentMainSequenceIt_;
    SequenceIterator currentOffSequenceIt_;
    EventSequenceMap mainStreamEvents_;
    EventSequenceMap offStreamEvents_;

    mpe::PlaybackData playbackData_;

    bool isActive_ = false;

    OnFlushedCallback onOffStreamFlushed_;
    OnFlushedCallback onMainStreamFlushed_;

private:
    bool shouldUpdateMainStreamEvents_ = false;
    bool updateMainStreamWhenInactive_ = false;
};
} // namespace muse::audio::engine
