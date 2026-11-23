// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "global/fn.h"
#include "midi/midievent.h"

namespace muse::audio::engine {
using MidiEventSink = muse::FnRef<void(const midi::Event &)>;
} // namespace muse::audio::engine
