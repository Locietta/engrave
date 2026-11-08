// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <latch>
#include <thread>
#include <vector>

#include "mpe/events.h"
#include "offlinetiming.h"

namespace muse::audio {
inline constexpr std::size_t parallel_event_cleanup_threshold = 16384;

struct EventCleanupProgress {
    enum class Branch { UNPREPARED, BELOW_THRESHOLD, PARALLEL, SERIAL_FALLBACK };
    inline static thread_local Branch branch = Branch::UNPREPARED;
    inline static thread_local std::size_t remaining_buckets = 0;
};

// Stage pointers and worker resources while setup may still throw. Destruction
// only wakes/joins those workers and clears unique maps; it never allocates.
struct OfflineEventCleanup {
    OfflineEventCleanup() {
        EventCleanupProgress::branch = EventCleanupProgress::Branch::UNPREPARED;
        EventCleanupProgress::remaining_buckets = 0;
    }

    OfflineEventCleanup(const OfflineEventCleanup &) = delete;
    OfflineEventCleanup &operator=(const OfflineEventCleanup &) = delete;

    template <typename Ids, typename Resolve>
    void prepare(const Ids &ids, Resolve &&resolve) {
        events_.reserve(ids.size());
        for (const auto &id : ids) events_.push_back(&resolve(id).origin_events);
        prepared_ = true;
        if (bucket_count() < parallel_event_cleanup_threshold || events_.size() < 2) return;
        try {
            for (std::size_t i = 0; i < workers_.size(); ++i) {
                workers_[i] = std::thread([this, i] {
                    start_.wait();
                    if (parallel_) clear(i, 4);
                });
            }
            workers_ready_ = true;
        } catch (...) {
            // Even a failed thread-state allocation must release/join every
            // successfully started worker before falling back to serial frees.
            wake_and_join();
        }
    }

    ~OfflineEventCleanup() noexcept {
        OfflineTimer timer(OfflineTiming::current().teardown);
        if (!prepared_) return; // Partial model construction tears down normally.
        const auto count = bucket_count();
        EventCleanupProgress::remaining_buckets = count;
        parallel_ = count >= parallel_event_cleanup_threshold && events_.size() >= 2 && workers_ready_;
        EventCleanupProgress::branch = parallel_                                ? EventCleanupProgress::Branch::PARALLEL :
                                       count < parallel_event_cleanup_threshold ? EventCleanupProgress::Branch::BELOW_THRESHOLD :
                                                                                  EventCleanupProgress::Branch::SERIAL_FALLBACK;
        if (!woken_) {
            start_.count_down();
            woken_ = true;
        }
        if (parallel_)
            clear(3, 4);
        else
            clear(0, 1);
        wake_and_join();
    }

    std::size_t bucket_count() const noexcept {
        std::size_t result = 0;
        for (const auto *events : events_) result += events->size();
        return result;
    }

private:
    void clear(std::size_t first, std::size_t stride) noexcept {
        for (std::size_t i = first; i < events_.size(); i += stride) {
            // Never copy an aliased map from an unwinding destructor.
            if (events_[i]->unique()) events_[i]->clear();
        }
    }

    void wake_and_join() noexcept {
        if (!woken_) {
            start_.count_down();
            woken_ = true;
        }
        for (auto &worker : workers_)
            if (worker.joinable()) worker.join();
    }

    std::vector<mpe::PlaybackEventsMap *> events_;
    std::array<std::thread, 3> workers_;
    std::latch start_{1};
    bool prepared_ = false;
    bool workers_ready_ = false;
    bool parallel_ = false;
    bool woken_ = false;
};
} // namespace muse::audio
