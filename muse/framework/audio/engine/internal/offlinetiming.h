// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "playbackmetrics.h"

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

struct OfflineTimer {
private:
    using Clock = std::chrono::steady_clock;
    double &total_;
    Clock::time_point start_;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    unsigned previous_phase_ = 0;
#endif
public:
    explicit OfflineTimer(double &total) : total_(total) {
#ifdef ENGRAVE_PLAYBACK_HARNESS
        auto &t = OfflineTiming::current();
        const double *fields[] = {nullptr,   &t.synth, &t.render, &t.encode, &t.setup,  &t.teardown,
                                  &t.create, &t.fonts, &t.flush,  &t.model,  &t.tracks, &t.aux};
        unsigned phase = 0;
        for (unsigned i = 1; i < test::AllocationMetrics::phase_count; ++i)
            if (&total == fields[i]) phase = i;
        previous_phase_ = test::AllocationMetrics::phase.exchange(phase);
#endif
        if (OfflineTiming::enabled()) start_ = Clock::now();
    }

    ~OfflineTimer() {
#ifdef ENGRAVE_PLAYBACK_HARNESS
        test::AllocationMetrics::phase.store(previous_phase_);
#endif
        if (OfflineTiming::enabled()) total_ += std::chrono::duration<double>(Clock::now() - start_).count();
    }
};

struct OfflineTimingReport {
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
