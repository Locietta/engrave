// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include <array>
#include <atomic>
#include <cstdint>

namespace muse::audio::test {
struct AllocationMetrics {
    enum Operation { OP_NEW, OP_DELETE, OP_MALLOC, OP_REALLOC, OP_FREE, OP_CALLOC, OP_ALIGNED_NEW, OP_COUNT };

    static constexpr unsigned phase_count = 12;
    inline static constexpr const char *names[phase_count] = {"other",  "synth", "render", "encode", "setup",  "teardown",
                                                              "create", "fonts", "flush",  "model",  "tracks", "aux"};
    inline static std::atomic<unsigned> phase = 0;
    inline static std::atomic<bool> enabled = false;
    inline static std::atomic<std::uint64_t> counts[phase_count][OP_COUNT]{};

    static void count(Operation op) noexcept {
        if (enabled.load(std::memory_order_relaxed))
            counts[phase.load(std::memory_order_relaxed)][op].fetch_add(1, std::memory_order_relaxed);
    }
};
} // namespace muse::audio::test
#endif
