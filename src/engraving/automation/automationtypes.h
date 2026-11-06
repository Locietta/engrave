/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include "global/types/id.h"
#include "global/types/number.h"
#include "global/types/sharedmap.h"

#include "mpe/automationpoint.h"

#include "engraving/infrastructure/eid.h"
#include "engraving/types/types.h"

#include <algorithm>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace mu::engraving {
struct AutomationPoint {
    using Ease = muse::mpe::AutomationPoint::Ease;
    using ArrivalFromPrevious = muse::mpe::AutomationPoint::ArrivalFromPrevious;
    using ExplicitArrival = muse::mpe::AutomationPoint::ExplicitArrival;
    using InValue = muse::mpe::AutomationPoint::InValue;

    muse::mpe::AutomationPoint value;
    std::optional<EID> item_id; // valid if it was created from an engraving item (e.g., Dynamic)
    bool generated = false;     // true if the point was generated automatically and hasn't been edited by the user

    bool operator==(const AutomationPoint &p) const { return value == p.value && item_id == p.item_id && generated == p.generated; }
};

using utick_t = int;
using AutomationCurve = muse::SharedMap<utick_t, AutomationPoint>;

inline std::optional<AutomationPoint::Ease> ease(const AutomationPoint &point) noexcept { return muse::mpe::ease(point.value); }

inline muse::real_t resolve_in_value(const AutomationCurve &curve, AutomationCurve::const_iterator it) {
    const std::optional<muse::real_t> prev_out_value =
        it == curve.begin() ? std::nullopt : std::optional(std::prev(it)->second.value.out_value);
    return muse::mpe::resolve_in_value(it->second.value, prev_out_value);
}

enum class AutomationType : unsigned char {
    UNKNOWN = 0,
    DYNAMICS,
    TEMPO,
    VOLUME,
    PAN,
};

struct AutomationCurveKey {
    //! NOTE: applies to a whole instrument (e.g. Volume, Pan)
    struct Instrument {
        InstrumentTrackId track_id;

        bool is_valid() const { return track_id.is_valid(); }

        bool operator==(const Instrument &o) const { return track_id == o.track_id; }

        bool operator<(const Instrument &o) const { return track_id < o.track_id; }
    };

    //! NOTE: applies to a specific staff (and optionally voice) (e.g. Dynamics)
    struct StaffVoice {
        muse::ID staff_id;
        std::optional<size_t> voice_idx;

        bool is_valid() const { return staff_id.is_valid(); }

        bool operator==(const StaffVoice &o) const { return staff_id == o.staff_id && voice_idx == o.voice_idx; }

        bool operator<(const StaffVoice &o) const { return std::tie(staff_id, voice_idx) < std::tie(o.staff_id, o.voice_idx); }
    };

    //! NOTE: std::monostate scope means the key applies to the whole score (e.g. Tempo)
    using Scope = std::variant<std::monostate, Instrument, StaffVoice>;

    AutomationType type = AutomationType::UNKNOWN;
    Scope scope;

    static AutomationCurveKey global(AutomationType type) {
        AutomationCurveKey key;
        key.type = type;
        return key;
    }

    static AutomationCurveKey instrument(AutomationType type, const InstrumentTrackId &track_id) {
        AutomationCurveKey key;
        key.type = type;
        key.scope = Instrument{track_id};
        return key;
    }

    static AutomationCurveKey staff(AutomationType type, const muse::ID &staff_id, std::optional<size_t> voice_idx = std::nullopt) {
        AutomationCurveKey key;
        key.type = type;
        key.scope = StaffVoice{staff_id, voice_idx};
        return key;
    }

    bool is_valid() const {
        if (type == AutomationType::UNKNOWN) {
            return false;
        }

        return std::visit(
            [](const auto &s) {
                using T = std::decay_t<decltype(s)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return true;
                } else {
                    return s.is_valid();
                }
            },
            scope);
    }

    bool is_global() const { return std::holds_alternative<std::monostate>(scope); }

    std::optional<InstrumentTrackId> track_id() const {
        const Instrument *instrument = std::get_if<Instrument>(&scope);
        return instrument ? std::optional(instrument->track_id) : std::nullopt;
    }

    std::optional<muse::ID> staff_id() const {
        const StaffVoice *staff_voice = std::get_if<StaffVoice>(&scope);
        return staff_voice ? std::optional(staff_voice->staff_id) : std::nullopt;
    }

    std::optional<size_t> voice_idx() const {
        const StaffVoice *staff_voice = std::get_if<StaffVoice>(&scope);
        return staff_voice ? staff_voice->voice_idx : std::nullopt;
    }

    //! NOTE: same staff, but the shared (voice-independent) curve
    AutomationCurveKey without_voice() const {
        AutomationCurveKey copy = *this;
        if (StaffVoice *staff_voice = std::get_if<StaffVoice>(&copy.scope)) {
            staff_voice->voice_idx = std::nullopt;
        }
        return copy;
    }

    bool operator==(const AutomationCurveKey &k) const { return type == k.type && scope == k.scope; }

    bool operator<(const AutomationCurveKey &k) const { return std::tie(type, scope) < std::tie(k.type, k.scope); }
};

using AutomationCurveMap = muse::SharedMap<AutomationCurveKey, AutomationCurve>;

inline const AutomationCurveKey tempo_key = AutomationCurveKey::global(AutomationType::TEMPO);

struct AutomationPointEdit {
    //! NOTE: write point at tick
    struct SetPoint {
        AutomationPoint point;
    };

    //! NOTE: write point at tick, removing whatever point currently sits at from
    struct MovePoint {
        AutomationPoint point;
        utick_t from = 0;
    };

    //! NOTE: erase whatever point currently sits at tick
    struct ErasePoint {};

    utick_t tick = 0; // destination tick for SetPoint/MovePoint, or the tick to erase for ErasePoint
    std::variant<SetPoint, MovePoint, ErasePoint> change;
};

using AutomationPointEdits = std::vector<AutomationPointEdit>;

struct AutomationChanges {
    bool is_full_reset = false;
    std::set<AutomationCurveKey> affected_keys;
    utick_t tick_from = -1;
    utick_t tick_to = -1;

    bool is_empty() const { return !is_full_reset && affected_keys.empty(); }

    void extend(utick_t from, utick_t to) {
        tick_from = (tick_from < 0) ? from : std::min(tick_from, from);
        tick_to = std::max(tick_to, to);
    }

    void extend(const AutomationCurveKey &key, utick_t from, utick_t to) {
        affected_keys.insert(key);
        extend(from, to);
    }

    void clear() { *this = {}; }
};
} // namespace mu::engraving
