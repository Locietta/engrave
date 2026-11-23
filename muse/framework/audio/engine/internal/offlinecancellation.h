// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <utility>

#include "global/fn.h"

namespace muse::audio {
// Owner-thread only. The driver catches callback exceptions before entering C
// codecs; inner polls return failure, never unwind through a C callback.
struct OfflineCancellation {
private:
    inline static thread_local muse::FnRef<bool() const> s_poll = nullptr;
    muse::FnRef<bool() const> previous_;
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

    explicit OfflineCancellation(muse::FnRef<bool() const> poll) : previous_(std::exchange(s_poll, poll)) {}

    template <class F>
        requires(!std::is_lvalue_reference_v<F> && !std::same_as<std::remove_cvref_t<F>, muse::FnRef<bool() const>> &&
                 !std::same_as<std::remove_cvref_t<F>, std::nullptr_t>)
    explicit OfflineCancellation(F &&) = delete;

    ~OfflineCancellation() { s_poll = previous_; }

    OfflineCancellation(const OfflineCancellation &) = delete;
    OfflineCancellation &operator=(const OfflineCancellation &) = delete;

    static bool requested() { return s_poll && s_poll(); }
};
} // namespace muse::audio
