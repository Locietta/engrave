// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include "global/fn.h"
#include "global/pollwait.h"

namespace muse::audio::engine {
// Persistent threads for fork/join blocks issued by one coordinator thread at a
// time. Threads start on the first block that needs them and are joined by the
// destructor. A block is either run() synchronously, or start()ed and later
// finish()ed or abandon()ed by the same coordinator; at most one block is in
// flight.
struct WorkerPool {
    static constexpr std::size_t k_max_workers = 64;

    WorkerPool();
    ~WorkerPool();
    WorkerPool(const WorkerPool &) = delete;
    WorkerPool &operator=(const WorkerPool &) = delete;

    // Runs task(i) for every i in [0, count) on at most `workers` pool threads
    // and returns once every started task has returned. The coordinator runs no
    // task: it waits in waits of at most 1 ms and calls poll() after each one
    // that the block outlasts. Once poll() returns true or throws, or a task
    // throws, stopping() turns true and unstarted tasks are skipped. The first
    // exception from poll() or a task is rethrown after the block has drained.
    // Returns false when poll() stopped the block.
    bool run(std::size_t workers, std::size_t count, muse::FnRef<void(std::size_t) const> task, muse::FnRef<bool() const> poll);

    // Starts the same block without waiting and returns true, or returns false
    // with nothing started when no pool thread is available. Throws
    // std::logic_error if a block is already in flight. The target of `task`
    // must outlive the block.
    bool start(std::size_t workers, std::size_t count, muse::FnRef<void(std::size_t) const> task);
    // Completes a started block exactly as run() does.
    bool finish(muse::FnRef<bool() const> poll);
    // Stops a started block, waits for its running tasks and drops any error.
    void abandon() noexcept;

    bool in_flight() const noexcept { return in_flight_; }

    // Starts up to `requested` workers (clamped to k_max_workers) and returns
    // the total started count (0 if thread creation failed).
    std::size_t ensure_workers(std::size_t requested);

    // Readable from tasks: the only cross-thread cancellation signal.
    bool stopping() const noexcept { return stop_.load(std::memory_order_relaxed); }

    std::size_t started_threads() const;
private:
    void grow_locked(std::size_t target);
    void loop(std::size_t index);
    std::exception_ptr end_block_locked();

    mutable std::mutex mutex_;
    muse::PollWait drained_;
    std::condition_variable work_cv_;
    std::vector<std::thread> threads_;
    std::atomic<bool> stop_{false};
    // Coordinator-only.
    bool in_flight_ = false;
    // Block state, guarded by mutex_.
    muse::FnRef<void(std::size_t) const> task_;
    std::uint64_t generation_ = 0;
    std::size_t participants_ = 0, count_ = 0, next_ = 0, running_ = 0;
    std::exception_ptr error_;
    bool shutdown_ = false;
};
} // namespace muse::audio::engine
