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

#include "automationpoint.h"
#include "global/types/number.h"
#include "mpetypes.h"
#include "playbacksetupdata.h"
#include "realfn.h"
#include "types/flags.h"
#include "types/number.h"
#include "types/sharedmap.h"

namespace muse::mpe {
struct ArrangementContext {
    timestamp_t nominal_timestamp = 0;
    timestamp_t actual_timestamp = 0;
    duration_t nominal_duration = 0;
    duration_t actual_duration = 0;
    voice_layer_idx_t voice_layer_index = 0;
    staff_layer_idx_t staff_layer_index = 0;
    double bps = 0.0;

    bool has_start() const { return actual_duration > 0; }

    bool has_end() const { return actual_duration != mpe::infinite_duration; }

    bool operator==(const ArrangementContext &other) const {
        return nominal_timestamp == other.nominal_timestamp && actual_timestamp == other.actual_timestamp &&
               nominal_duration == other.nominal_duration && actual_duration == other.actual_duration &&
               voice_layer_index == other.voice_layer_index && staff_layer_index == other.staff_layer_index &&
               muse::is_equal(bps, other.bps);
    }
};

struct PitchContext {
    pitch_level_t nominal_pitch_level = 0;
    PitchCurve pitch_curve;

    bool operator==(const PitchContext &other) const {
        return nominal_pitch_level == other.nominal_pitch_level && pitch_curve == other.pitch_curve;
    }
};

struct ExpressionContext {
    ArticulationMap articulations;
    dynamic_level_t nominal_dynamic_level = ::muse::mpe::min_dynamic_level;
    ExpressionCurve expression_curve;
    std::optional<float> velocity_override;

    bool operator==(const ExpressionContext &other) const {
        return articulations == other.articulations && nominal_dynamic_level == other.nominal_dynamic_level &&
               expression_curve == other.expression_curve && velocity_override == velocity_override;
    }
};

struct NoteEvent {
    NoteEvent() = default;

    explicit NoteEvent(ArrangementContext &&arrangement_ctx, PitchContext &&pitch_ctx, ExpressionContext &&expression_ctx)
        : arrangement_ctx_(arrangement_ctx), pitch_ctx_(pitch_ctx), expression_ctx_(expression_ctx) {}

    explicit NoteEvent(const timestamp_t nominal_timestamp, const duration_t nominal_duration, const voice_layer_idx_t voice_idx,
                       const staff_layer_idx_t staff_idx, const pitch_level_t nominal_pitch_level,
                       const dynamic_level_t nominal_dynamic_level, const ArticulationMap &articulations_applied, const double bps,
                       const float required_velocity_fraction = 0.f, const PitchCurve &required_pitch_curve = {}) {
        arrangement_ctx_.nominal_duration = nominal_duration;
        arrangement_ctx_.nominal_timestamp = nominal_timestamp;
        arrangement_ctx_.voice_layer_index = voice_idx;
        arrangement_ctx_.staff_layer_index = staff_idx;
        arrangement_ctx_.bps = bps;

        pitch_ctx_.nominal_pitch_level = nominal_pitch_level;

        expression_ctx_.articulations = articulations_applied;
        expression_ctx_.nominal_dynamic_level = nominal_dynamic_level;

        set_up(required_velocity_fraction, required_pitch_curve);
    }

    const ArrangementContext &arrangement_ctx() const { return arrangement_ctx_; }

    const PitchContext &pitch_ctx() const { return pitch_ctx_; }

    const ExpressionContext &expression_ctx() const { return expression_ctx_; }

    bool operator==(const NoteEvent &other) const {
        return arrangement_ctx_ == other.arrangement_ctx_ && pitch_ctx_ == other.pitch_ctx_ && expression_ctx_ == other.expression_ctx_;
    }

private:

    template <typename T>
    inline T mult(T v, float f) {
        return static_cast<T>(static_cast<float>(v) * f);
    }

    void set_up(const float required_velocity_fraction, const PitchCurve &required_pitch_curve) {
        calculate_actual_duration(expression_ctx_.articulations);
        calculate_actual_timestamp(expression_ctx_.articulations);

        if (required_pitch_curve.empty()) {
            calculate_pitch_curve(expression_ctx_.articulations);
        } else {
            pitch_ctx_.pitch_curve = required_pitch_curve;
        }

        calculate_expression_curve(expression_ctx_.articulations, required_velocity_fraction);
    }

    void calculate_actual_timestamp(const ArticulationMap &articulations_applied) {
        arrangement_ctx_.actual_timestamp = arrangement_ctx_.nominal_timestamp;

        if (articulations_applied.empty()) {
            return;
        }

        timestamp_t timestamp_offset_value =
            mult(arrangement_ctx_.nominal_duration, percentage_to_factor(articulations_applied.average_timestamp_offset()));

        arrangement_ctx_.actual_timestamp += timestamp_offset_value;
    }

    void calculate_actual_duration(const ArticulationMap &articulations_applied) {
        arrangement_ctx_.actual_duration = arrangement_ctx_.nominal_duration;

        if (articulations_applied.empty() || arrangement_ctx_.nominal_duration == ::muse::mpe::infinite_duration) {
            return;
        }

        arrangement_ctx_.actual_duration =
            mult(arrangement_ctx_.actual_duration, percentage_to_factor(articulations_applied.average_duration_factor()));
    }

    void calculate_pitch_curve(const ArticulationMap &articulations_applied) {
        pitch_ctx_.pitch_curve = articulations_applied.average_pitch_offset_map();

        pitch_level_t average_pitch_range = articulations_applied.average_pitch_range();
        if (average_pitch_range == 0 || average_pitch_range == ::muse::mpe::pitch_level_step) {
            return;
        }

        float ratio = static_cast<float>(average_pitch_range) / static_cast<float>(::muse::mpe::pitch_level_step);
        float pattern_unit_ratio = ::muse::mpe::pitch_level_step / static_cast<float>(::muse::mpe::one_percent);

        // Mutable begin detaches before iteration; only values change in this loop.
        for (auto &pair : pitch_ctx_.pitch_curve) {
            pair.second = static_cast<pitch_level_t>(real_round(static_cast<float>(pair.second) * ratio * pattern_unit_ratio, 0));
        }
    }

    void calculate_expression_curve(const ArticulationMap &articulations_applied, const float required_velocity_fraction) {
        expression_ctx_.expression_curve = articulations_applied.average_dynamic_offset_map();

        if (!real_is_null(required_velocity_fraction)) {
            expression_ctx_.velocity_override = required_velocity_fraction;
        }

        if (expression_ctx_.expression_curve.empty()) {
            return;
        }

        dynamic_level_t articulation_dynamic_level = articulations_applied.average_max_amplitude_level();
        dynamic_level_t nominal_dynamic_level = expression_ctx_.nominal_dynamic_level;

        constexpr dynamic_level_t natural_dynamic_level = dynamic_level_from_type(DynamicType::NATURAL);

        float dynamic_amplify_factor =
            static_cast<float>(articulation_dynamic_level - natural_dynamic_level) / ::muse::mpe::dynamic_level_step;

        dynamic_level_t amplification_diff =
            mult(std::max(articulations_applied.average_dynamic_range(), ::muse::mpe::dynamic_level_step), dynamic_amplify_factor);

        dynamic_level_t actual_dynamic_level = nominal_dynamic_level + amplification_diff;

        if (actual_dynamic_level == articulation_dynamic_level) {
            return;
        }

        float ratio = static_cast<float>(actual_dynamic_level) / static_cast<float>(articulation_dynamic_level);

        // Mutable begin detaches before iteration; only values change in this loop.
        for (auto &pair : expression_ctx_.expression_curve) {
            pair.second = static_cast<dynamic_level_t>(real_round(pair.second * ratio, 0));
        }
    }

    ArrangementContext arrangement_ctx_;
    PitchContext pitch_ctx_;
    ExpressionContext expression_ctx_;
};

struct ControllerChangeEvent {
    enum Type : signed char {
        UNDEFINED = -1,
        MODULATION,
        SUSTAIN_PEDAL_ON_OFF,
        PITCH_BEND,
    };

    using Value = muse::NumberT<float>;

    Type type = UNDEFINED;
    Value val; // [0;1]
    layer_idx_t layer_idx = 0;

    bool operator==(const ControllerChangeEvent &e) const { return type == e.type && val == e.val && layer_idx == e.layer_idx; }
};

using ControllerChangeEventList = std::vector<ControllerChangeEvent>;

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

using PlaybackEvent =
    std::variant<std::monostate, NoteEvent, TextArticulationEvent, SoundPresetChangeEvent, SyllableEvent, ControllerChangeEvent>;

using PlaybackEventList = std::vector<PlaybackEvent>;
using PlaybackEventsMap = SharedMap<timestamp_t, PlaybackEventList>;

using DynamicAutomationMap = AutomationCurve<timestamp_t>;
using DynamicAutomationLayers = SharedMap<layer_idx_t, DynamicAutomationMap>;

struct PlaybackData {
    PlaybackEventsMap origin_events;
    PlaybackSetupData setup_data;
    DynamicAutomationLayers dynamics;

    bool operator==(const PlaybackData &other) const {
        return origin_events == other.origin_events && setup_data == other.setup_data && dynamics == other.dynamics;
    }

    bool is_valid() const { return setup_data.is_valid(); }
};
} // namespace muse::mpe
