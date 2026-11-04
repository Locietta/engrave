// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <chrono>
#include <cstdlib>
#include <cstdio>

namespace muse::audio {
struct OfflineTiming {
    double synth = 0, render = 0, encode = 0, setup = 0, teardown = 0, create = 0, fonts = 0, flush = 0, model = 0, tracks = 0, aux = 0;

    static bool enabled() {
        static const bool value = std::getenv("ENGRAVE_AUDIO_TIMING") != nullptr;
        return value;
    }

    static OfflineTiming &current() {
        static thread_local OfflineTiming value;
        return value;
    }
};

class OfflineTimer {
    using Clock = std::chrono::steady_clock;
    double &m_total;
    Clock::time_point m_start;
public:
    explicit OfflineTimer(double &total) : m_total(total) {
        if (OfflineTiming::enabled()) m_start = Clock::now();
    }

    ~OfflineTimer() {
        if (OfflineTiming::enabled()) m_total += std::chrono::duration<double>(Clock::now() - m_start).count();
    }
};

class OfflineTimingReport {
public:
    OfflineTimingReport() { OfflineTiming::current() = {}; }

    ~OfflineTimingReport() {
        if (!OfflineTiming::enabled()) return;
        const auto &t = OfflineTiming::current();
        std::fprintf(stderr, "offline_audio_seconds synth=%.9f mix=%.9f encode=%.9f\n", t.synth, t.render - t.synth, t.encode);
        std::fprintf(stderr, "offline_audio_seconds setup=%.9f teardown=%.9f create=%.9f fonts=%.9f flush=%.9f\n", t.setup, t.teardown,
                     t.create, t.fonts, t.flush);
        std::fprintf(stderr, "offline_audio_seconds model=%.9f tracks=%.9f aux=%.9f\n", t.model, t.tracks, t.aux);
    }
};
} // namespace muse::audio
