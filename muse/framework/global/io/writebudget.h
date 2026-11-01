// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <optional>

namespace muse::io {

// Scoped to one owner-thread capture. The working counter is cumulative and
// deliberately conservative; a stopped budget never invokes a callback again.
struct WriteBudget {
    enum class Failure { None, Limit, Cancelled };
    std::optional<std::uint64_t> output;
    std::optional<std::uint64_t> working;
    const std::function<bool()>* cancelled = nullptr;
    std::uint64_t charged = 0;
    Failure failure = Failure::None;
    std::exception_ptr callback_error;

    bool poll() {
        if (failure != Failure::None) return false;
        if (cancelled && *cancelled) {
            try {
                if ((*cancelled)()) failure = Failure::Cancelled;
            } catch (...) {
                callback_error = std::current_exception();
                failure = Failure::Cancelled;
            }
        }
        return failure == Failure::None;
    }
    bool charge(std::uint64_t bytes) {
        if (!poll()) return false;
        if (working && (charged > *working || bytes > *working - charged)) {
            failure = Failure::Limit;
            return false;
        }
        if (working) charged += bytes;
        return true;
    }
    bool outputSize(std::uint64_t bytes) {
        if (!poll()) return false;
        if (output && bytes > *output) {
            failure = Failure::Limit;
            return false;
        }
        return true;
    }
};

inline thread_local WriteBudget* activeWriteBudget = nullptr;

struct WriteBudgetScope {
    explicit WriteBudgetScope(WriteBudget& budget) : previous(activeWriteBudget) { activeWriteBudget = &budget; }
    ~WriteBudgetScope() { activeWriteBudget = previous; }
    WriteBudgetScope(const WriteBudgetScope&) = delete;
    WriteBudgetScope& operator=(const WriteBudgetScope&) = delete;
    WriteBudget* previous;
};

// Thrown only from the normal writer body, never from a cleanup flush.
struct WriteBudgetStop {};

} // namespace muse::io
