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

#include <variant>
#include <vector>

#include "realfn.h"
#include "global/types/number.h"
#include "types/flags.h"
#include "types/number.h"
#include "types/sharedmap.h"

#include "automationpoint.h"
#include "mpetypes.h"
#include "playbacksetupdata.h"

namespace muse::mpe {
struct ArrangementContext {
    timestamp_t nominalTimestamp = 0;
    timestamp_t actualTimestamp = 0;
    duration_t nominalDuration = 0;
    duration_t actualDuration = 0;
    voice_layer_idx_t voiceLayerIndex = 0;
    staff_layer_idx_t staffLayerIndex = 0;
    double bps = 0.0;

    bool hasStart() const { return actualDuration > 0; }

    bool hasEnd() const { return actualDuration != mpe::INFINITE_DURATION; }

    bool operator==(const ArrangementContext &other) const {
        return nominalTimestamp == other.nominalTimestamp && actualTimestamp == other.actualTimestamp &&
               nominalDuration == other.nominalDuration && actualDuration == other.actualDuration &&
               voiceLayerIndex == other.voiceLayerIndex && staffLayerIndex == other.staffLayerIndex && muse::is_equal(bps, other.bps);
    }
};

struct PitchContext {
    pitch_level_t nominalPitchLevel = 0;
    PitchCurve pitchCurve;

    bool operator==(const PitchContext &other) const {
        return nominalPitchLevel == other.nominalPitchLevel && pitchCurve == other.pitchCurve;
    }
};

struct ExpressionContext {
    ArticulationMap articulations;
    dynamic_level_t nominalDynamicLevel = MIN_DYNAMIC_LEVEL;
    ExpressionCurve expressionCurve;
    std::optional<float> velocityOverride;

    bool operator==(const ExpressionContext &other) const {
        return articulations == other.articulations && nominalDynamicLevel == other.nominalDynamicLevel &&
               expressionCurve == other.expressionCurve && velocityOverride == velocityOverride;
    }
};

struct NoteEvent {
    NoteEvent() = default;

    explicit NoteEvent(ArrangementContext &&arrangementCtx, PitchContext &&pitchCtx, ExpressionContext &&expressionCtx)
        : arrangementCtx_(arrangementCtx), pitchCtx_(pitchCtx), expressionCtx_(expressionCtx) {}

    explicit NoteEvent(const timestamp_t nominalTimestamp, const duration_t nominalDuration, const voice_layer_idx_t voiceIdx,
                       const staff_layer_idx_t staffIdx, const pitch_level_t nominalPitchLevel, const dynamic_level_t nominalDynamicLevel,
                       const ArticulationMap &articulationsApplied, const double bps, const float requiredVelocityFraction = 0.f,
                       const PitchCurve &requiredPitchCurve = {}) {
        arrangementCtx_.nominalDuration = nominalDuration;
        arrangementCtx_.nominalTimestamp = nominalTimestamp;
        arrangementCtx_.voiceLayerIndex = voiceIdx;
        arrangementCtx_.staffLayerIndex = staffIdx;
        arrangementCtx_.bps = bps;

        pitchCtx_.nominalPitchLevel = nominalPitchLevel;

        expressionCtx_.articulations = articulationsApplied;
        expressionCtx_.nominalDynamicLevel = nominalDynamicLevel;

        setUp(requiredVelocityFraction, requiredPitchCurve);
    }

    const ArrangementContext &arrangementCtx() const { return arrangementCtx_; }

    const PitchContext &pitchCtx() const { return pitchCtx_; }

    const ExpressionContext &expressionCtx() const { return expressionCtx_; }

    bool operator==(const NoteEvent &other) const {
        return arrangementCtx_ == other.arrangementCtx_ && pitchCtx_ == other.pitchCtx_ && expressionCtx_ == other.expressionCtx_;
    }

private:

    template <typename T>
    inline T mult(T v, float f) {
        return static_cast<T>(static_cast<float>(v) * f);
    }

    void setUp(const float requiredVelocityFraction, const PitchCurve &requiredPitchCurve) {
        calculateActualDuration(expressionCtx_.articulations);
        calculateActualTimestamp(expressionCtx_.articulations);

        if (requiredPitchCurve.empty()) {
            calculatePitchCurve(expressionCtx_.articulations);
        } else {
            pitchCtx_.pitchCurve = requiredPitchCurve;
        }

        calculateExpressionCurve(expressionCtx_.articulations, requiredVelocityFraction);
    }

    void calculateActualTimestamp(const ArticulationMap &articulationsApplied) {
        arrangementCtx_.actualTimestamp = arrangementCtx_.nominalTimestamp;

        if (articulationsApplied.empty()) {
            return;
        }

        timestamp_t timestampOffsetValue =
            mult(arrangementCtx_.nominalDuration, percentageToFactor(articulationsApplied.averageTimestampOffset()));

        arrangementCtx_.actualTimestamp += timestampOffsetValue;
    }

    void calculateActualDuration(const ArticulationMap &articulationsApplied) {
        arrangementCtx_.actualDuration = arrangementCtx_.nominalDuration;

        if (articulationsApplied.empty() || arrangementCtx_.nominalDuration == INFINITE_DURATION) {
            return;
        }

        arrangementCtx_.actualDuration =
            mult(arrangementCtx_.actualDuration, percentageToFactor(articulationsApplied.averageDurationFactor()));
    }

    void calculatePitchCurve(const ArticulationMap &articulationsApplied) {
        pitchCtx_.pitchCurve = articulationsApplied.averagePitchOffsetMap();

        pitch_level_t averagePitchRange = articulationsApplied.averagePitchRange();
        if (averagePitchRange == 0 || averagePitchRange == PITCH_LEVEL_STEP) {
            return;
        }

        float ratio = static_cast<float>(averagePitchRange) / static_cast<float>(PITCH_LEVEL_STEP);
        float patternUnitRatio = PITCH_LEVEL_STEP / static_cast<float>(ONE_PERCENT);

        for (auto &pair : pitchCtx_.pitchCurve) {
            pair.second = static_cast<pitch_level_t>(RealRound(static_cast<float>(pair.second) * ratio * patternUnitRatio, 0));
        }
    }

    void calculateExpressionCurve(const ArticulationMap &articulationsApplied, const float requiredVelocityFraction) {
        expressionCtx_.expressionCurve = articulationsApplied.averageDynamicOffsetMap();

        if (!RealIsNull(requiredVelocityFraction)) {
            expressionCtx_.velocityOverride = requiredVelocityFraction;
        }

        if (expressionCtx_.expressionCurve.empty()) {
            return;
        }

        dynamic_level_t articulationDynamicLevel = articulationsApplied.averageMaxAmplitudeLevel();
        dynamic_level_t nominalDynamicLevel = expressionCtx_.nominalDynamicLevel;

        constexpr dynamic_level_t naturalDynamicLevel = dynamicLevelFromType(DynamicType::Natural);

        float dynamicAmplifyFactor = static_cast<float>(articulationDynamicLevel - naturalDynamicLevel) / DYNAMIC_LEVEL_STEP;

        dynamic_level_t amplificationDiff =
            mult(std::max(articulationsApplied.averageDynamicRange(), DYNAMIC_LEVEL_STEP), dynamicAmplifyFactor);

        dynamic_level_t actualDynamicLevel = nominalDynamicLevel + amplificationDiff;

        if (actualDynamicLevel == articulationDynamicLevel) {
            return;
        }

        float ratio = static_cast<float>(actualDynamicLevel) / static_cast<float>(articulationDynamicLevel);

        for (auto &pair : expressionCtx_.expressionCurve) {
            pair.second = static_cast<dynamic_level_t>(RealRound(pair.second * ratio, 0));
        }
    }

    ArrangementContext arrangementCtx_;
    PitchContext pitchCtx_;
    ExpressionContext expressionCtx_;
};

struct ControllerChangeEvent {
    enum Type : signed char {
        Undefined = -1,
        Modulation,
        SustainPedalOnOff,
        PitchBend,
    };

    using Value = muse::number_t<float>;

    Type type = Undefined;
    Value val; // [0;1]
    layer_idx_t layerIdx = 0;

    bool operator==(const ControllerChangeEvent &e) const { return type == e.type && val == e.val && layerIdx == e.layerIdx; }
};

using ControllerChangeEventList = std::vector<ControllerChangeEvent>;

struct TextArticulationEvent {
    enum FlagType : unsigned char {
        NoFlags = 0,
        StartsAtPlaybackPosition,
    };

    String text;
    layer_idx_t layerIdx = 0;
    Flags<FlagType> flags;

    bool operator==(const TextArticulationEvent &e) const { return text == e.text && layerIdx == e.layerIdx && flags == e.flags; }
};

using TextArticulationEventList = std::vector<TextArticulationEvent>;

struct SoundPresetChangeEvent {
    String code;
    layer_idx_t layerIdx = 0;

    bool operator==(const SoundPresetChangeEvent &e) const { return code == e.code && layerIdx == e.layerIdx; }
};

using SoundPresetChangeEventList = std::vector<SoundPresetChangeEvent>;

struct SyllableEvent {
    enum FlagType : unsigned char {
        NoFlags = 0,
        StartsAtPlaybackPosition,
        HyphenedToNext,
    };

    String text;
    layer_idx_t layerIdx = 0;
    Flags<FlagType> flags;

    bool operator==(const SyllableEvent &e) const { return text == e.text && layerIdx == e.layerIdx && flags == e.flags; }
};

using SyllableEventList = std::vector<SyllableEvent>;

using PlaybackEvent =
    std::variant<std::monostate, NoteEvent, TextArticulationEvent, SoundPresetChangeEvent, SyllableEvent, ControllerChangeEvent>;

using PlaybackEventList = std::vector<PlaybackEvent>;
using PlaybackEventsMap = SharedMap<timestamp_t, PlaybackEventList>;

using DynamicAutomationMap = AutomationCurve<timestamp_t>;
using DynamicAutomationLayers = SharedMap<layer_idx_t, DynamicAutomationMap>;

struct PlaybackData {
    PlaybackEventsMap originEvents;
    PlaybackSetupData setupData;
    DynamicAutomationLayers dynamics;

    bool operator==(const PlaybackData &other) const {
        return originEvents == other.originEvents && setupData == other.setupData && dynamics == other.dynamics;
    }

    bool isValid() const { return setupData.isValid(); }
};
} // namespace muse::mpe
