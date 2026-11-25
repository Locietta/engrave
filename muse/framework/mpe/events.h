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

#include <memory>
#include <vector>

#include "automationpoint.h"
#include "global/flatmap.h"
#include "mpetypes.h"
#include "types/flags.h"

namespace muse::mpe {
struct TextArticulationEvent {
    enum FlagType : unsigned char {
        NO_FLAGS = 0,
        STARTS_AT_PLAYBACK_POSITION,
    };

    String text;
    layer_idx_t layer_idx = 0;
    Flags<FlagType> flags;

    bool operator==(const TextArticulationEvent &e) const { return text == e.text && layer_idx == e.layer_idx && flags == e.flags; }
};

using TextArticulationEventList = std::vector<TextArticulationEvent>;

struct SoundPresetChangeEvent {
    String code;
    layer_idx_t layer_idx = 0;

    bool operator==(const SoundPresetChangeEvent &e) const { return code == e.code && layer_idx == e.layer_idx; }
};

using SoundPresetChangeEventList = std::vector<SoundPresetChangeEvent>;

struct SyllableEvent {
    enum FlagType : unsigned char {
        NO_FLAGS = 0,
        STARTS_AT_PLAYBACK_POSITION,
        HYPHENED_TO_NEXT,
    };

    String text;
    layer_idx_t layer_idx = 0;
    Flags<FlagType> flags;

    bool operator==(const SyllableEvent &e) const { return text == e.text && layer_idx == e.layer_idx && flags == e.flags; }
};

using SyllableEventList = std::vector<SyllableEvent>;

using DynamicAutomationMap = AutomationCurve<timestamp_t>;

struct DynamicAutomationLayers : muse::FlatMap<layer_idx_t, std::shared_ptr<const DynamicAutomationMap>> {
    using Base = muse::FlatMap<layer_idx_t, std::shared_ptr<const DynamicAutomationMap>>;
    using Base::Base;

    friend bool operator==(const DynamicAutomationLayers &lhs, const DynamicAutomationLayers &rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](const auto &a, const auto &b) {
            return a.first == b.first && (a.second == b.second || (a.second && b.second && *a.second == *b.second));
        });
    }
};

} // namespace muse::mpe
