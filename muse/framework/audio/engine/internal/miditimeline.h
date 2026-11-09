/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include <amc/vector.hpp>
#include <ankerl/unordered_dense.h>

#include "audio/common/audiotypes.h"
#include "midi/midievent.h"
#include "midi/miditypes.h"
#include "mpe/events.h"
#include "mpe/flatevents.h"
#include "offlinecancellation.h"

namespace muse::audio::engine {
// The checked uint32 buffer helpers are shared with the flat MPE store
using mpe::Buf;
using mpe::checked_grow;
using mpe::emplace_checked;
using mpe::reserve_checked;
using mpe::StoreLimit;

struct TimedMidiEvent {
    mpe::timestamp_t timestamp = 0;
    midi::Event event;
};

static_assert(std::is_trivially_copyable_v<TimedMidiEvent>);
static_assert(sizeof(TimedMidiEvent) == 24);

struct CcKey {
    midi::Event::Opcode opcode = midi::Event::Opcode::CONTROL_CHANGE;
    midi::channel_t channel = 0;
    std::uint8_t controller = 0;
    std::uint32_t value = 0;

    bool operator==(const CcKey &other) const = default;
};

struct CcKeyHash {
    using is_avalanching = void;

    std::uint64_t operator()(const CcKey &key) const noexcept {
        const std::uint64_t packed = (static_cast<std::uint64_t>(key.opcode) << 48) | (static_cast<std::uint64_t>(key.channel) << 40) |
                                     (static_cast<std::uint64_t>(key.controller) << 32) | static_cast<std::uint64_t>(key.value);
        return ankerl::unordered_dense::hash<std::uint64_t>{}(packed);
    }
};

inline bool is_dedup_eligible_cc(const midi::Event &event) {
    return event.is_channel_voice() && event.opcode() == midi::Event::Opcode::CONTROL_CHANGE &&
           event.index() != static_cast<std::uint8_t>(midi::expression_controller);
}

enum class TimelineStream : std::uint8_t {
    MAIN = 0,
    OFF = 1,
};

struct ChunkSlice {
    mpe::timestamp_t at = 0;
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    TimelineStream stream = TimelineStream::MAIN;
};

static_assert(std::is_trivially_copyable_v<ChunkSlice>);

struct MidiTimeline {
    Buf<TimedMidiEvent> events;
    std::uint32_t cursor = 0;

    void clear() {
        events.clear();
        cursor = 0;
    }

    bool empty() const { return events.empty(); }

    void seek(const mpe::timestamp_t position) {
        const auto it =
            std::lower_bound(events.cbegin(), events.cend(), position,
                             [](const TimedMidiEvent &entry, const mpe::timestamp_t target) { return entry.timestamp < target; });
        cursor = static_cast<std::uint32_t>(it - events.cbegin());
    }
};

} // namespace muse::audio::engine
