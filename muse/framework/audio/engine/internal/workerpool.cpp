// SPDX-License-Identifier: GPL-3.0-only
#include "workerpool.h"

#include <algorithm>
#include <chrono>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

using namespace muse::audio::engine;

WorkerPool::WorkerPool() {
#ifdef _WIN32
    drained_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    poll_timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!drained_event_ || !poll_timer_) {
        if (drained_event_) CloseHandle(drained_event_);
        if (poll_timer_) CloseHandle(poll_timer_);
        drained_event_ = poll_timer_ = nullptr;
    }
#endif
}

WorkerPool::~WorkerPool() {
    {
        const std::scoped_lock lock(mutex_);
        shutdown_ = true;
    }
    work_cv_.notify_all();
    for (auto &thread : threads_) thread.join();
#ifdef _WIN32
    if (drained_event_) CloseHandle(drained_event_);
    if (poll_timer_) CloseHandle(poll_timer_);
#endif
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
                (*task_)(task);
            } catch (...) {
                error = std::current_exception();
            }
            lock.lock();
            if (error) {
                if (!error_) error_ = std::move(error);
                stop_.store(true, std::memory_order_relaxed);
            }
        }
        if (--running_ == 0) {
            done_cv_.notify_one();
#ifdef _WIN32
            if (drained_event_) SetEvent(drained_event_);
#endif
        }
    }
}

bool WorkerPool::wait_drained(std::unique_lock<std::mutex> &lock) {
#ifdef _WIN32
    if (poll_timer_) {
        if (running_ == 0) return true;
        LARGE_INTEGER due;
        // Relative, in 100 ns units. The timer fires up to ~0.5 ms late, so ask for half the interval.
        due.QuadPart = -5'000;
        lock.unlock();
        if (SetWaitableTimer(poll_timer_, &due, 0, nullptr, nullptr, FALSE)) {
            const HANDLE handles[] = {drained_event_, poll_timer_};
            WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        } else {
            WaitForSingleObject(drained_event_, 1);
        }
        lock.lock();
        return running_ == 0;
    }
#endif
    return done_cv_.wait_for(lock, std::chrono::milliseconds(1), [&] { return running_ == 0; });
}

bool WorkerPool::run(std::size_t workers, const std::size_t count, const muse::FnRef<void(std::size_t) const> task,
                     const muse::FnRef<bool() const> poll) {
    stop_.store(false, std::memory_order_relaxed);
    if (count == 0) return true;
    workers = std::clamp<std::size_t>(std::min(workers, count), 1, k_max_workers);
    std::unique_lock lock(mutex_);
    grow_locked(workers);
    if (threads_.empty()) {
        lock.unlock();
        for (std::size_t i = 0; i < count; ++i) {
            if (poll()) {
                stop_.store(true, std::memory_order_relaxed);
                return false;
            }
            task(i);
        }
        return true;
    }

    task_ = &task;
    participants_ = std::min(workers, threads_.size());
    count_ = count;
    next_ = 0;
    running_ = participants_;
    error_ = nullptr;
    ++generation_;
#ifdef _WIN32
    if (drained_event_) ResetEvent(drained_event_);
#endif
    lock.unlock();
    work_cv_.notify_all();

    bool stopped = false;
    std::exception_ptr poll_error;
    lock.lock();
    while (!wait_drained(lock)) {
        if (stopped) continue;
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
    task_ = nullptr;
    participants_ = 0;
    std::exception_ptr error = std::exchange(error_, nullptr);
    lock.unlock();
    if (poll_error) std::rethrow_exception(poll_error);
    if (error) std::rethrow_exception(error);
    return !stopped;
}
