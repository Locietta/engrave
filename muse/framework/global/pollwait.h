// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <condition_variable>
#include <mutex>

namespace muse {
// A coordinator's bounded wait for state guarded by an external mutex: it
// returns after at most ~1 ms, or earlier once another thread calls signal().
// The waiter re-checks its predicate under the lock, so a signal sent between
// that check and the wait is never lost.
struct PollWait {
    PollWait();
    ~PollWait();
    PollWait(const PollWait &) = delete;
    PollWait &operator=(const PollWait &) = delete;

    // Call with the guarding mutex held, or after releasing it.
    void signal();
    // `lock` must hold the guarding mutex; it is released during the wait.
    void wait(std::unique_lock<std::mutex> &lock);
private:
#ifdef _WIN32
    // Windows rounds condition-variable timeouts up to the scheduler tick (up to
    // ~15.6 ms); an auto-reset event plus a high-resolution waitable timer keep
    // the 1 ms interval. Null when unavailable, which falls back to the
    // condition variable.
    void *event_ = nullptr, *timer_ = nullptr;
#endif
    std::condition_variable cv_;
};
} // namespace muse
