// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// This header and every call site are absent from ordinary production builds.
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include <functional>
#include <map>
#include <ostream>

#include "mpe/events.h"

namespace muse::audio::test {
struct PlaybackCapture {
    inline static std::function<void(std::uint64_t, const muse::String &, const mpe::PlaybackData &)> track;
    inline static std::ostream *synth = nullptr;
    inline static std::map<const void *, unsigned> ids;

    static unsigned id(const void *source) {
        auto [it, inserted] = ids.emplace(source, static_cast<unsigned>(ids.size()));
        return it->second;
    }
};
} // namespace muse::audio::test
#endif
