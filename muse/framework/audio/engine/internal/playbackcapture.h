// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// This header and every call site are absent from ordinary production builds.
#ifdef ENGRAVE_PLAYBACK_HARNESS
#include <functional>
#include <iomanip>
#include <map>
#include <ostream>
#include <type_traits>

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

    // Record the arguments passed to the actual call, never a parallel description.
    template <typename Function, typename Synth, typename... Args>
    static auto setup_call(const void *source, const char *name, Function function, Synth *fluid, Args... args) {
        if (synth) {
            auto &out = *synth;
            out << "setup " << id(source) << ' ' << name;
            const auto argument = [&](auto value) {
                out << ' ';
                using T = decltype(value);
                if constexpr (std::is_same_v<T, std::nullptr_t>)
                    out << "null";
                else if constexpr (std::is_convertible_v<T, const char *>)
                    out << std::quoted(value);
                else
                    out << +value;
            };
            (argument(args), ...);
            out << '\n';
        }
        return function(fluid, args...);
    }
};
} // namespace muse::audio::test
#endif
