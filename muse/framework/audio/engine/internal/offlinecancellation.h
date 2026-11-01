// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <functional>
#include <utility>
namespace muse::audio {
// Owner-thread only. The driver catches callback exceptions before entering C
// codecs; inner polls return failure, never unwind through a C callback.
class OfflineCancellation {
    inline static thread_local const std::function<bool()>* s_poll = nullptr;
    const std::function<bool()>* m_previous;
public:
    enum class Phase { Other, Synthesis };
    inline static thread_local Phase phase = Phase::Other;
    class SynthesisScope {
        Phase previous = std::exchange(phase, Phase::Synthesis);
    public:
        ~SynthesisScope() { phase = previous; }
    };
    explicit OfflineCancellation(const std::function<bool()>& poll)
        : m_previous(std::exchange(s_poll, &poll)) {}
    ~OfflineCancellation() { s_poll = m_previous; }
    OfflineCancellation(const OfflineCancellation&) = delete;
    OfflineCancellation& operator=(const OfflineCancellation&) = delete;
    static bool requested() { return s_poll && (*s_poll)(); }
};
}
