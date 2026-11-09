// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <exception>
#include <functional>
#include <iterator>

namespace mu::engraving {
// Remove from the back: no vector shifts, and unordered_dense erases without a bulk bucket clear.
template <class Container, class Step>
void drain_playback_back(Container &items, const Step &step) {
    while (!items.empty()) {
        if constexpr (requires { typename Container::mapped_type; }) {
            auto &value = std::prev(items.end())->second;
            if constexpr (requires {
                              value.begin();
                              value.erase(value.begin());
                          }) {
                drain_playback_back(value, step);
            }
        }
        if constexpr (requires { items.pop_back(); })
            items.pop_back();
        else
            items.erase(std::prev(items.end()));
        step();
    }
}

// As with PlaybackScore release, finish cleanup before rethrowing a latched cancellation.
template <class Release>
void release_playback_polled(const std::function<void()> &checkpoint, Release release) {
    std::exception_ptr error;
    const auto poll = [&] {
        if (checkpoint && !error) {
            try {
                checkpoint();
            } catch (...) {
                error = std::current_exception();
            }
        }
    };
    std::size_t remaining = 4096;
    poll();
    release([&] {
        if (--remaining == 0) {
            poll();
            remaining = 4096;
        }
    });
    poll();
    if (error) std::rethrow_exception(error);
}
} // namespace mu::engraving
