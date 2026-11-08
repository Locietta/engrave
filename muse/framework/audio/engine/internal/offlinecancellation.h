// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <functional>
#include <utility>

namespace muse::audio {
// Owner-thread only. The driver catches callback exceptions before entering C
// codecs; inner polls return failure, never unwind through a C callback.
struct OfflineCancellation {
private:
    inline static thread_local const std::function<bool()> *s_poll = nullptr;
    const std::function<bool()> *previous_;
public:
    enum class Phase { OTHER, PLAYBACK_MODEL, SEQUENCER, SYNTHESIS, ENCODING, EVENT_RELEASE };
    inline static thread_local Phase phase = Phase::OTHER;

    struct EventReleaseScope {
    private:
        Phase previous = std::exchange(phase, Phase::EVENT_RELEASE);
    public:
        ~EventReleaseScope() { phase = previous; }
    };

    struct SequencerScope {
    private:
        Phase previous = std::exchange(phase, Phase::SEQUENCER);
    public:
        ~SequencerScope() { phase = previous; }
    };

    struct EncodingScope {
    private:
        Phase previous = std::exchange(phase, Phase::ENCODING);
    public:
        ~EncodingScope() { phase = previous; }
    };

    struct ModelScope {
    private:
        Phase previous = std::exchange(phase, Phase::PLAYBACK_MODEL);
    public:
        ~ModelScope() { phase = previous; }
    };

    struct SynthesisScope {
    private:
        Phase previous = std::exchange(phase, Phase::SYNTHESIS);
    public:
        ~SynthesisScope() { phase = previous; }
    };

    explicit OfflineCancellation(const std::function<bool()> &poll) : previous_(std::exchange(s_poll, &poll)) {}

    ~OfflineCancellation() { s_poll = previous_; }

    OfflineCancellation(const OfflineCancellation &) = delete;
    OfflineCancellation &operator=(const OfflineCancellation &) = delete;

    static bool requested() { return s_poll && (*s_poll)(); }
};
} // namespace muse::audio
