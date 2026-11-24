#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <thread>

namespace music {
inline constexpr std::size_t k_max_worker_threads = 64;

// Threads for one render's parallel work (audio synthesis and encoding, PNG
// deflate), from EngineOptions::audio_threads: 0 picks one thread per spare
// core, at most 8. ENGRAVE_AUDIO_THREADS, when a positive integer, overrides
// the configured value.
inline std::size_t render_worker_threads(int configured) {
    if (const char *text = std::getenv("ENGRAVE_AUDIO_THREADS")) {
        char *end = nullptr;
        const long value = std::strtol(text, &end, 10);
        if (end != text && *end == '\0' && value > 0) configured = static_cast<int>(std::min<long>(value, k_max_worker_threads));
    }
    if (configured > 0) return std::min<std::size_t>(static_cast<std::size_t>(configured), k_max_worker_threads);
    const unsigned cores = std::thread::hardware_concurrency();
    return std::clamp<std::size_t>(cores > 1 ? cores - 1 : 1, 1, 8);
}
} // namespace music
