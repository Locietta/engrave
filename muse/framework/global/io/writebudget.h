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
    enum class Failure { NONE, LIMIT, CANCELLED };
    std::optional<std::uint64_t> output;
    std::optional<std::uint64_t> working;
    const std::function<bool()> *cancelled = nullptr;
    std::uint64_t charged = 0;
    Failure failure = Failure::NONE;
    std::exception_ptr callback_error;

    bool poll() {
        if (failure != Failure::NONE) return false;
        if (cancelled && *cancelled) {
            try {
                if ((*cancelled)()) failure = Failure::CANCELLED;
            } catch (...) {
                callback_error = std::current_exception();
                failure = Failure::CANCELLED;
            }
        }
        return failure == Failure::NONE;
    }

    bool charge(std::uint64_t bytes) {
        if (!poll()) return false;
        if (working && (charged > *working || bytes > *working - charged)) {
            failure = Failure::LIMIT;
            return false;
        }
        if (working) charged += bytes;
        return true;
    }

    bool output_size(std::uint64_t bytes) {
        if (!poll()) return false;
        if (output && bytes > *output) {
            failure = Failure::LIMIT;
            return false;
        }
        return true;
    }
};

inline thread_local WriteBudget *active_write_budget = nullptr;

struct WriteBudgetScope {
    explicit WriteBudgetScope(WriteBudget &budget) : previous(active_write_budget) { active_write_budget = &budget; }

    ~WriteBudgetScope() { active_write_budget = previous; }

    WriteBudgetScope(const WriteBudgetScope &) = delete;
    WriteBudgetScope &operator=(const WriteBudgetScope &) = delete;
    WriteBudget *previous;
};

// Thrown only from the normal writer body, never from a cleanup flush.
struct WriteBudgetStop {};

} // namespace muse::io
