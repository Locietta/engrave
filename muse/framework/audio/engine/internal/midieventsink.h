// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <functional>

#include "midi/midievent.h"

namespace muse::audio::engine {
using MidiEventSink = std::function<void(const midi::Event &)>;
} // namespace muse::audio::engine
