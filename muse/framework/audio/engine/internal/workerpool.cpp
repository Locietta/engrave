// SPDX-License-Identifier: GPL-3.0-only
#include "workerpool.h"

#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <utility>

using namespace muse::audio::engine;

WorkerPool::WorkerPool() = default;

WorkerPool::~WorkerPool() {
    abandon();
    {
        const std::scoped_lock lock(mutex_);
        shutdown_ = true;
    }
    work_cv_.notify_all();
    for (auto &thread : threads_) thread.join();
}

std::size_t WorkerPool::started_threads() const {
    const std::scoped_lock lock(mutex_);
    return threads_.size();
}

void WorkerPool::grow_locked(std::size_t target) {
    target = std::min(target, k_max_workers);
    while (threads_.size() < target) {
        try {
            threads_.emplace_back([this, index = threads_.size()] { loop(index); });
        } catch (const std::system_error &) {
            break; // Run on the threads the system granted.
        }
    }
}

std::size_t WorkerPool::ensure_workers(const std::size_t requested) {
    const std::scoped_lock lock(mutex_);
    grow_locked(requested);
    return threads_.size();
}

void WorkerPool::loop(const std::size_t index) {
    std::uint64_t seen = 0;
    std::unique_lock lock(mutex_);
    for (;;) {
        work_cv_.wait(lock, [&] { return shutdown_ || (generation_ != seen && index < participants_); });
        if (shutdown_) return;
        seen = generation_;
        while (next_ < count_ && !stop_.load(std::memory_order_relaxed)) {
            const std::size_t task = next_++;
            lock.unlock();
            std::exception_ptr error;
            try {
                task_(task);
            } catch (...) {
                error = std::current_exception();
            }
            lock.lock();
            if (error) {
                if (!error_) error_ = std::move(error);
                stop_.store(true, std::memory_order_relaxed);
            }
        }
        if (--running_ == 0) drained_.signal();
    }
}

bool WorkerPool::start(std::size_t workers, const std::size_t count, const muse::FnRef<void(std::size_t) const> task) {
    std::unique_lock lock(mutex_);
    if (in_flight_) throw std::logic_error("WorkerPool::start called while a block is already in flight");
    stop_.store(false, std::memory_order_relaxed);
    if (count > 0) {
        workers = std::clamp<std::size_t>(std::min(workers, count), 1, k_max_workers);
        grow_locked(workers);
        if (threads_.empty()) return false;
        task_ = task;
        participants_ = std::min(workers, threads_.size());
    }
    count_ = count;
    next_ = 0;
    running_ = participants_;
    error_ = nullptr;
    ++generation_;
    in_flight_ = true;
    lock.unlock();
    if (count > 0) work_cv_.notify_all();
    return true;
}

std::exception_ptr WorkerPool::end_block_locked() {
    task_ = nullptr;
    participants_ = 0;
    in_flight_ = false;
    return std::exchange(error_, nullptr);
}

bool WorkerPool::finish(const muse::FnRef<bool() const> poll) {
    bool stopped = false;
    std::exception_ptr poll_error;
    std::unique_lock lock(mutex_);
    while (running_ != 0) {
        drained_.wait(lock);
        if (running_ == 0 || stopped) continue;
        lock.unlock();
        try {
            stopped = poll();
        } catch (...) {
            poll_error = std::current_exception();
            stopped = true;
        }
        if (stopped) stop_.store(true, std::memory_order_relaxed);
        lock.lock();
    }
    std::exception_ptr error = end_block_locked();
    lock.unlock();
    // Prioritize cancellation and poll exceptions over concurrent worker errors,
    // including when a worker failed before the first poll tick elapsed.
    if (!stopped && error) {
        try {
            stopped = poll();
        } catch (...) {
            poll_error = std::current_exception();
            stopped = true;
        }
    }
    if (poll_error) std::rethrow_exception(poll_error);
    if (stopped) return false;
    if (error) std::rethrow_exception(error);
    return true;
}

void WorkerPool::abandon() noexcept {
    if (!in_flight_) return;
    stop_.store(true, std::memory_order_relaxed);
    std::unique_lock lock(mutex_);
    while (running_ != 0) drained_.wait(lock);
    end_block_locked();
}

bool WorkerPool::run(std::size_t workers, const std::size_t count, const muse::FnRef<void(std::size_t) const> task,
                     const muse::FnRef<bool() const> poll) {
    if (start(workers, count, task)) return finish(poll);
    for (std::size_t i = 0; i < count; ++i) {
        if (poll()) {
            stop_.store(true, std::memory_order_relaxed);
            return false;
        }
        task(i);
    }
    return true;
}
