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
    enum class Phase { Other, PlaybackModel, Sequencer, Synthesis, Encoding };
    inline static thread_local Phase phase = Phase::Other;
    class SequencerScope {
        Phase previous = std::exchange(phase, Phase::Sequencer);
    public:
        ~SequencerScope() { phase = previous; }
    };
    class EncodingScope {
        Phase previous = std::exchange(phase, Phase::Encoding);
    public:
        ~EncodingScope() { phase = previous; }
    };
    class ModelScope {
        Phase previous = std::exchange(phase, Phase::PlaybackModel);
    public:
        ~ModelScope() { phase = previous; }
    };
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
