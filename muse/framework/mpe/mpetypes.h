/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
#include <array>
#include <cstddef>
#include <map>
#include <math.h>
#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "soundid.h"
#include "types/number.h"
#include "types/sharedhashmap.h"
#include "types/sharedmap.h"

#ifdef C
#undef C
#endif

namespace muse::mpe {
// common
using usecs_t = int64_t; // microseconds
using percentage_t = int_fast16_t;
constexpr percentage_t one_percent = 100;
constexpr percentage_t fifty_percent = ::muse::mpe::one_percent * 50;
constexpr percentage_t hundred_percent = ::muse::mpe::one_percent * 100;
constexpr percentage_t ten_percent = ::muse::mpe::one_percent * 10;

constexpr inline float percentage_to_factor(const percentage_t percents) {
    return static_cast<float>(percents) / static_cast<float>(::muse::mpe::hundred_percent);
}

constexpr inline percentage_t percentage_from_factor(const float factor) {
    return static_cast<percentage_t>(factor * ::muse::mpe::hundred_percent);
}

// Arrangement
using timestamp_t = usecs_t;
using duration_t = usecs_t;
using duration_percentage_t = percentage_t;
using voice_layer_idx_t = uint_fast8_t;
using staff_layer_idx_t = uint_fast16_t;
using layer_idx_t = uint_fast32_t;

static constexpr duration_t infinite_duration = std::numeric_limits<duration_t>::max();

struct TimestampAndDuration {
    timestamp_t timestamp = 0;
    duration_t duration = 0;
};

static constexpr voice_layer_idx_t max_voices = 4;

constexpr inline layer_idx_t make_layer_idx(const staff_layer_idx_t staff_idx, const voice_layer_idx_t voice_idx) {
    return staff_idx * ::muse::mpe::max_voices + voice_idx;
}

constexpr inline duration_percentage_t occupied_percentage(const timestamp_t timestamp, const duration_t overall_duration) {
    return percentage_from_factor(static_cast<float>(timestamp) / static_cast<float>(overall_duration));
}

template <typename T>
struct ValuesCurve : public SharedMap<duration_percentage_t, T> {
    std::pair<duration_percentage_t, T> amplitude_value_point() const {
        auto max = std::max_element(this->cbegin(), this->cend(),
                                    [](const auto &f, const auto &s) { return std::abs(f.second) < std::abs(s.second); });

        if (max == this->cend()) {
            static const std::pair<duration_percentage_t, T> empty{0, {}};
            return empty;
        }

        return *max;
    }

    T max_amplitude_level() const { return amplitude_value_point().second; }

    duration_percentage_t attack_phase_duration() const { return amplitude_value_point().first; }

    duration_percentage_t release_phase_duration() const {
        return this->empty() ? 0 : this->rbegin()->first - amplitude_value_point().first;
    }

    float velocity_fraction() const {
        auto amplitude_point = amplitude_value_point();
        duration_percentage_t duration = amplitude_point.first;

        if (duration == 0) {
            return 1.f;
        }

        float factor = log10f(amplitude_point.second / static_cast<float>(duration));
        return (factor + 1.f) / 2.f;
    }
};

// Pitch
enum class PitchClass : signed char {
    UNDEFINED = -1,
    C = 0,
    C_SHARP = 1,
    D = 2,
    D_SHARP = 3,
    E = 4,
    F = 5,
    F_SHARP = 6,
    G = 7,
    G_SHARP = 8,
    A = 9,
    A_SHARP = 10,
    B = 11
};

constexpr int steps_per_octave = 12;

using octave_t = int_fast8_t;
using pitch_level_t = percentage_t;
using PitchCurve = ValuesCurve<pitch_level_t>;

constexpr pitch_level_t pitch_level_step = 50;
constexpr pitch_level_t max_pitch_level = ::muse::mpe::hundred_percent;

constexpr int zero_pitch_level_midi_equivalent = 12; // C0 in MIDI standard

constexpr inline pitch_level_t pitch_level(const PitchClass pitch_class, const octave_t octave) {
    return (::muse::mpe::pitch_level_step * ::muse::mpe::steps_per_octave * octave) +
           (::muse::mpe::pitch_level_step * static_cast<int>(pitch_class));
}

constexpr inline pitch_level_t pitch_level_diff(const PitchClass f_class, const octave_t f_octave, const PitchClass s_class,
                                                const octave_t s_octave) {
    return pitch_level(f_class, f_octave) - pitch_level(s_class, s_octave);
}

constexpr inline size_t pitch_steps_count(const pitch_level_t pitch_range) {
    size_t range = pitch_range > 0 ? pitch_range : -pitch_range;
    return range / ::muse::mpe::pitch_level_step;
}

// Expression
using ArticulationFamily = mpe::SoundCategory;

enum class ArticulationType : signed char {
    UNDEFINED = -1,

    // single note articulations
    STANDARD,
    STACCATO,
    STACCATISSIMO,
    TENUTO,
    MARCATO,
    ACCENT,
    SOFT_ACCENT,

    LAISSEZ_VIBRER,

    SUBITO,

    FADE_IN,
    FADE_OUT,

    HARMONIC,
    JAZZ_TONE,
    PALM_MUTE,
    MUTE,
    OPEN,
    PIZZICATO,
    SNAP_PIZZICATO,
    RANDOM_PIZZICATO,
    UP_BOW,
    DOWN_BOW,
    DETACHE,
    MARTELE,
    JETE,
    COL_LEGNO,
    SUL_PONT,
    SUL_TASTO,

    GHOST_NOTE,
    CROSS_NOTE,
    CROSS_LARGE_NOTE,
    CROSS_ORNATE_NOTE,
    CIRCLE_NOTE,
    CIRCLE_CROSS_NOTE,
    CIRCLE_DOT_NOTE,
    TRIANGLE_LEFT_NOTE,
    TRIANGLE_RIGHT_NOTE,
    TRIANGLE_UP_NOTE,
    TRIANGLE_DOWN_NOTE,
    TRIANGLE_ROUND_DOWN_NOTE,
    DIAMOND_NOTE,
    MOON_NOTE,
    PLUS_NOTE,
    SLASH_NOTE,
    SQUARE_NOTE,
    SLASHED_BACKWARDS_NOTE,
    SLASHED_FORWARDS_NOTE,

    FALL,
    QUICK_FALL,
    DOIT,
    PLOP,
    SCOOP,
    BRASS_BEND,
    SLIDE_OUT_DOWN,
    SLIDE_OUT_UP,
    SLIDE_IN_ABOVE,
    SLIDE_IN_BELOW,
    VOLUME_SWELL,

    VIBRATO,
    WIDE_VIBRATO,
    MOLTO_VIBRATO,
    SENZA_VIBRATO,

    TREMOLO8TH,
    TREMOLO16TH,
    TREMOLO32ND,
    TREMOLO64TH,
    TREMOLO_BUZZ,

    TRILL_BAROQUE,
    UPPER_MORDENT,
    LOWER_MORDENT,
    UPPER_MORDENT_BAROQUE,
    LOWER_MORDENT_BAROQUE,
    PRALL_MORDENT,
    MORDENT_WITH_UPPER_PREFIX,
    UP_MORDENT,
    DOWN_MORDENT,
    TREMBLEMENT,
    UP_PRALL,
    PRALL_UP,
    PRALL_DOWN,
    LINE_PRALL,
    SLIDE,
    TURN,
    INVERTED_TURN,

    PRE_APPOGGIATURA,
    POST_APPOGGIATURA,
    ACCIACCATURA,

    TREMOLO_BAR,
    DISTORTION,
    OVERDRIVE,
    SLAP,
    POP,

    LEFT_HAND_TAPPING,
    RIGHT_HAND_TAPPING,

    CONTINUOUS_GLISSANDO,

    BREATH,

    // Handbells
    MALLET_BELL_ON_TABLE,
    MALLET_BELL_SUSPENDED,
    MALLET_LIFT,
    PLUCK,
    PLUCK_LIFT,
    GYRO,
    MARTELLATO,
    MARTELLATO_LIFT,
    HAND_MARTELLATO,
    MUTED_MARTELLATO,
    THUMB_DAMP,
    BRUSH_DAMP,
    RING,
    RING_TOUCH,
    SINGING_BELL,
    SINGING_VIBRATE,
    SWING,
    ECHO,

    // multi-note articulations
    TRILL,
    CRESCENDO,
    DIMINUENDO,
    DISCRETE_GLISSANDO,
    LEGATO,
    PEDAL,
    MULTIBEND,
    ARPEGGIO,
    ARPEGGIO_UP,
    ARPEGGIO_DOWN,
    ARPEGGIO_STRAIGHT_UP,
    ARPEGGIO_STRAIGHT_DOWN,

    LAST
};

using ArticulationTypeSet = std::unordered_set<ArticulationType>;

inline bool is_multi_note_articulation(const ArticulationType type) {
    static const ArticulationTypeSet multi_types{
        ArticulationType::TRILL,
        ArticulationType::CRESCENDO,
        ArticulationType::DIMINUENDO,
        ArticulationType::DISCRETE_GLISSANDO,
        ArticulationType::LEGATO,
        ArticulationType::PEDAL,
        ArticulationType::MULTIBEND,
        ArticulationType::ARPEGGIO,
        ArticulationType::ARPEGGIO_UP,
        ArticulationType::ARPEGGIO_DOWN,
        ArticulationType::ARPEGGIO_STRAIGHT_UP,
        ArticulationType::ARPEGGIO_STRAIGHT_DOWN,
    };

    return muse::contains(multi_types, type);
}

inline bool is_single_note_articulation(const ArticulationType type) { return !is_multi_note_articulation(type); }

inline bool is_ranged_articulation(const ArticulationType type) {
    if (is_single_note_articulation(type)) {
        return false;
    }

    return type == ArticulationType::LEGATO || type == ArticulationType::PEDAL || type == ArticulationType::MULTIBEND;
}

static const String ordinary_playing_technique_code(u"ordinary_technique");

using dynamic_level_t = percentage_t;
constexpr dynamic_level_t max_dynamic_level = ::muse::mpe::hundred_percent;
constexpr dynamic_level_t min_dynamic_level = 0;
constexpr dynamic_level_t dynamic_level_step = 5 * ::muse::mpe::one_percent;

inline dynamic_level_t dynamic_level_from_normalized(const real_t normalized) {
    return static_cast<dynamic_level_t>(lround(normalized.raw() * ::muse::mpe::max_dynamic_level));
}

enum class DynamicType : percentage_t {
    UNDEFINED = -1,
    PPPPPPPPP = ::muse::mpe::min_dynamic_level,
    PPPPPPPP = static_cast<int>(7.5f * ::muse::mpe::one_percent),
    PPPPPPP = static_cast<int>(12.5f * ::muse::mpe::one_percent),
    PPPPPP = static_cast<int>(17.5f * ::muse::mpe::one_percent),
    PPPPP = static_cast<int>(22.5f * ::muse::mpe::one_percent),
    PPPP = static_cast<int>(27.5f * ::muse::mpe::one_percent),
    PPP = static_cast<int>(32.5f * ::muse::mpe::one_percent),
    PP = static_cast<int>(37.5f * ::muse::mpe::one_percent),
    P = static_cast<int>(42.5f * ::muse::mpe::one_percent),
    MP = static_cast<int>(47.5f * ::muse::mpe::one_percent),
    NATURAL = 50 * ::muse::mpe::one_percent,
    MF = static_cast<int>(52.5f * ::muse::mpe::one_percent),
    F = static_cast<int>(57.5f * ::muse::mpe::one_percent),
    FF = static_cast<int>(62.5f * ::muse::mpe::one_percent),
    FFF = static_cast<int>(67.5f * ::muse::mpe::one_percent),
    FFFF = static_cast<int>(72.5f * ::muse::mpe::one_percent),
    FFFFF = static_cast<int>(77.5f * ::muse::mpe::one_percent),
    FFFFFF = static_cast<int>(82.5f * ::muse::mpe::one_percent),
    FFFFFFF = static_cast<int>(87.5f * ::muse::mpe::one_percent),
    FFFFFFFF = static_cast<int>(92.5f * ::muse::mpe::one_percent),
    FFFFFFFFF = ::muse::mpe::max_dynamic_level,
    LAST
};

inline DynamicType approximate_dynamic_type(const dynamic_level_t dynamic_level) {
    if (dynamic_level < ::muse::mpe::min_dynamic_level) {
        return DynamicType::PPPPPPPPP;
    }

    if (dynamic_level > ::muse::mpe::max_dynamic_level) {
        return DynamicType::FFFFFFFFF;
    }

    return static_cast<DynamicType>(dynamic_level);
}

constexpr inline dynamic_level_t dynamic_level_from_type(const DynamicType type) {
    if (type == DynamicType::UNDEFINED) {
        return ::muse::mpe::min_dynamic_level;
    }

    return static_cast<dynamic_level_t>(type);
}

struct ArrangementPattern {
    duration_percentage_t duration_factor_value = 0;
    duration_percentage_t timestamp_offset_value = 0;

    ArrangementPattern() = default;

    ArrangementPattern(duration_percentage_t input_duration_factor, duration_percentage_t input_timestamp_offset)
        : duration_factor_value(input_duration_factor), timestamp_offset_value(input_timestamp_offset) {}

    bool operator==(const ArrangementPattern &other) const {
        return duration_factor_value == other.duration_factor_value && timestamp_offset_value == other.timestamp_offset_value;
    }
};

using ArrangementPatternList = std::vector<ArrangementPattern>;

struct PitchPattern {
    using PitchOffsetMap = PitchCurve;

    PitchOffsetMap pitch_offset_map;

    PitchPattern() = default;

    PitchPattern(size_t size, percentage_t step, pitch_level_t default_value) {
        for (size_t i = 0; i < size; ++i) {
            pitch_offset_map.emplace(step * static_cast<int>(i), default_value);
        }
    }

    pitch_level_t max_amplitude_level() const { return pitch_offset_map.max_amplitude_level(); }

    bool operator==(const PitchPattern &other) const { return pitch_offset_map == other.pitch_offset_map; }
};

using PitchPatternList = std::vector<PitchPattern>;

using ExpressionCurve = ValuesCurve<dynamic_level_t>;

struct ExpressionPattern {
    using DynamicOffsetMap = ExpressionCurve;

    ExpressionCurve dynamic_offset_map;

    ExpressionPattern() = default;

    ExpressionPattern(size_t size, percentage_t step, dynamic_level_t default_value) {
        for (size_t i = 0; i < size; ++i) {
            dynamic_offset_map.emplace(step * static_cast<int>(i), default_value);
        }
    }

    dynamic_level_t max_amplitude_level() const { return dynamic_offset_map.max_amplitude_level(); }

    bool operator==(const ExpressionPattern &other) const { return dynamic_offset_map == other.dynamic_offset_map; }
};

using ExpressionPatternList = std::vector<ExpressionPattern>;

struct ArticulationPatternSegment {
    ArrangementPattern arrangement_pattern;
    PitchPattern pitch_pattern;
    ExpressionPattern expression_pattern;

    ArticulationPatternSegment() = default;

    ArticulationPatternSegment(ArrangementPattern &&arrangement, PitchPattern &&pitch, ExpressionPattern &&expression)
        : arrangement_pattern(arrangement), pitch_pattern(pitch), expression_pattern(expression) {}

    bool operator==(const ArticulationPatternSegment &other) const {
        return arrangement_pattern == other.arrangement_pattern && pitch_pattern == other.pitch_pattern &&
               expression_pattern == other.expression_pattern;
    }
};

using ArticulationPattern = SharedMap<duration_percentage_t, ArticulationPatternSegment>;

struct ArticulationsProfile {
    std::vector<ArticulationFamily> supported_families;

    const ArticulationPattern &pattern(const ArticulationType type, const ArticulationType fallback = ArticulationType::UNDEFINED) const {
        auto search = patterns_.find(type);
        if (search == patterns_.cend() && fallback != ArticulationType::UNDEFINED) {
            search = patterns_.find(fallback);
        }

        if (search == patterns_.cend()) {
            static const ArticulationPattern empty_pattern;
            return empty_pattern;
        }

        return search->second;
    }

    void set_pattern(const ArticulationType type, const ArticulationPattern &scope) { patterns_.insert_or_assign(type, scope); }

    void remove_pattern(const ArticulationType type) { patterns_.erase(type); }

    const SharedHashMap<ArticulationType, ArticulationPattern> &data() const { return patterns_; }

    bool contains(const ArticulationType type) const { return patterns_.find(type) != patterns_.cend(); }

    bool is_valid() const { return !patterns_.empty(); }

    bool operator==(const ArticulationsProfile &other) const { return patterns_ == other.patterns_; }

private:
    SharedHashMap<ArticulationType, ArticulationPattern> patterns_;
};

using ArticulationsProfilePtr = std::shared_ptr<ArticulationsProfile>;

struct ArticulationMeta {
    ArticulationType type = ArticulationType::UNDEFINED;
    ArticulationPattern pattern;

    timestamp_t timestamp = 0;
    duration_t overall_duration = 0;
    pitch_level_t overall_pitch_changes_range = 0;
    dynamic_level_t overall_dynamic_changes_range = 0;

    ArticulationMeta() = default;

    ArticulationMeta(const ArticulationType input_type) : type(input_type) {}

    ArticulationMeta(const ArticulationType input_type, const ArticulationPattern &input_pattern, const timestamp_t input_timestamp,
                     const duration_t input_duration, const pitch_level_t overall_pitch_range = 0,
                     const dynamic_level_t overall_dynamic_range = 0)
        : type(input_type), pattern(input_pattern), timestamp(input_timestamp), overall_duration(input_duration),
          overall_pitch_changes_range(overall_pitch_range), overall_dynamic_changes_range(overall_dynamic_range) {}

    bool operator==(const ArticulationMeta &other) const {
        return type == other.type && pattern == other.pattern && timestamp == other.timestamp &&
               overall_duration == other.overall_duration && overall_pitch_changes_range == other.overall_pitch_changes_range &&
               overall_dynamic_changes_range == other.overall_dynamic_changes_range;
    }

    bool has_start() const { return overall_duration > 0; }

    bool has_end() const { return overall_duration != mpe::infinite_duration; }
};

using ArticulationMetaMap = SharedHashMap<ArticulationType, ArticulationMeta>;

struct ArticulationAppliedData {
    ArticulationMeta meta;

    ArticulationPatternSegment applied_pattern_segment;

    duration_percentage_t occupied_from_value = 0;
    duration_percentage_t occupied_to_value = ::muse::mpe::hundred_percent;

    pitch_level_t occupied_pitch_changes_range = 0;
    dynamic_level_t occupied_dynamic_changes_range = 0;

    ArticulationAppliedData() = default;

    explicit ArticulationAppliedData(ArticulationMeta &&input_meta, const duration_percentage_t input_occupied_from,
                                     const duration_percentage_t input_occupied_to)
        : meta(std::move(input_meta)) {
        update_occupied_range(input_occupied_from, input_occupied_to);
    }

    explicit ArticulationAppliedData(const ArticulationMeta &input_meta, const duration_percentage_t input_occupied_from,
                                     const duration_percentage_t input_occupied_to)
        : meta(input_meta) {
        update_occupied_range(input_occupied_from, input_occupied_to);
    }

    bool operator==(const ArticulationAppliedData &other) const {
        return meta == other.meta && applied_pattern_segment == other.applied_pattern_segment &&
               occupied_from_value == other.occupied_from_value && occupied_to_value == other.occupied_to_value &&
               occupied_pitch_changes_range == other.occupied_pitch_changes_range &&
               occupied_dynamic_changes_range == other.occupied_dynamic_changes_range;
    }

    void update_occupied_range(const duration_percentage_t from, const duration_percentage_t to) {
        occupied_from_value = from;
        occupied_to_value = to;

        if (meta.pattern.empty()) {
            return;
        }

        if (occupied_from_value == 0 && occupied_to_value == ::muse::mpe::hundred_percent) {
            applied_pattern_segment = meta.pattern.cbegin()->second;
            occupied_pitch_changes_range = meta.overall_pitch_changes_range;
            occupied_dynamic_changes_range = meta.overall_dynamic_changes_range;
            return;
        }

        float occupied_factor = percentage_to_factor(occupied_to_value);
        occupied_pitch_changes_range = static_cast<pitch_level_t>(static_cast<float>(meta.overall_pitch_changes_range) * occupied_factor);
        occupied_dynamic_changes_range =
            static_cast<pitch_level_t>(static_cast<float>(meta.overall_dynamic_changes_range) * occupied_factor);

        const auto &lower = meta.pattern.lower_bound(occupied_from_value);

        if (lower != meta.pattern.cend()) {
            applied_pattern_segment = lower->second;
        } else {
            applied_pattern_segment = meta.pattern.cbegin()->second;
        }
    }
};

//! Ordered by ascending ArticulationType, which is the canonical precedence order of every decision that
//! iterates the map (averaging fallback, per-note emission, ornament/tremolo/arpeggio choice).
//! SharedMap is temporary scaffolding until the playback container step.
struct ArticulationMap : public SharedMap<ArticulationType, ArticulationAppliedData> {
    static constexpr size_t expected_size = (::muse::mpe::hundred_percent / ::muse::mpe::ten_percent) + 1;

    void update_occupied_range(const ArticulationType type, const duration_percentage_t input_occupied_from,
                               const duration_percentage_t input_occupied_to) {
        if (!contains(type)) {
            return;
        }

        ArticulationAppliedData &data = at(type);
        data.update_occupied_range(input_occupied_from, input_occupied_to);

        if (input_occupied_from == 0 && input_occupied_to == ::muse::mpe::hundred_percent) {
            return;
        }

        pre_calculate_average_data();
    }

    duration_percentage_t average_duration_factor() const { return average_duration_factor_; }

    duration_percentage_t average_timestamp_offset() const { return average_timestamp_offset_; }

    const PitchPattern::PitchOffsetMap &average_pitch_offset_map() const { return average_pitch_offset_map_; }

    pitch_level_t average_pitch_range() const { return average_pitch_range_; }

    dynamic_level_t average_max_amplitude_level() const { return average_max_amplitude_level_; }

    dynamic_level_t average_dynamic_range() const { return average_dynamic_range_; }

    const ExpressionPattern::DynamicOffsetMap &average_dynamic_offset_map() const { return average_dynamic_offset_map_; }

    void pre_calculate_average_data() {
        if (empty()) {
            return;
        }

        if (size() == 1) {
            const ArticulationAppliedData &art_data = cbegin()->second;
            const ArticulationPatternSegment &segment = art_data.applied_pattern_segment;
            average_duration_factor_ = segment.arrangement_pattern.duration_factor_value;
            average_timestamp_offset_ = segment.arrangement_pattern.timestamp_offset_value;
            average_max_amplitude_level_ = segment.expression_pattern.max_amplitude_level();
            average_pitch_range_ = art_data.occupied_pitch_changes_range;
            average_dynamic_range_ = art_data.occupied_dynamic_changes_range;
            average_dynamic_offset_map_ = segment.expression_pattern.dynamic_offset_map;
            average_pitch_offset_map_ = segment.pitch_pattern.pitch_offset_map;
            return;
        }

        reset_data();

        ParamsSum params_sum;
        for (size_t i = 0; i < ::muse::mpe::ArticulationMap::expected_size; ++i) {
            params_sum.pitch_offset_map.insert_or_assign(static_cast<int>(i) * ::muse::mpe::ten_percent, 0);
            params_sum.dynamic_offset_map.insert_or_assign(static_cast<int>(i) * ::muse::mpe::ten_percent, 0);
        }

        for (auto it = cbegin(); it != cend(); ++it) {
            const ArticulationAppliedData &applied_articulation = it->second;
            sum_up_data(applied_articulation, params_sum);
        }

        calculate_average(params_sum);
    }

private:
    void reset_data() {
        average_duration_factor_ = 0;
        average_timestamp_offset_ = 0;
        average_max_amplitude_level_ = 0;
        average_dynamic_range_ = 0;
        average_pitch_range_ = 0;
        average_pitch_offset_map_.clear();
        average_dynamic_offset_map_.clear();
    }

    struct ParamsSum {
        int duration_factor_value = 0;
        int timestamp_offset_value = 0;
        int max_amplitude_level = 0;
        int pitch_range = 0;
        int dynamic_range = 0;
        ValuesCurve<int> pitch_offset_map;
        ValuesCurve<int> dynamic_offset_map;
    };

    void sum_up_data(const ArticulationAppliedData &applied_articulation, ParamsSum &out) {
        const ArticulationPatternSegment &segment = applied_articulation.applied_pattern_segment;

        out.duration_factor_value += segment.arrangement_pattern.duration_factor_value;
        out.timestamp_offset_value += segment.arrangement_pattern.timestamp_offset_value;
        out.max_amplitude_level += segment.expression_pattern.max_amplitude_level();
        out.pitch_range += applied_articulation.occupied_pitch_changes_range;
        out.dynamic_range += applied_articulation.occupied_dynamic_changes_range;

        sum_up_offsets(segment, out);
    }

    void sum_up_offsets(const ArticulationPatternSegment &segment, ParamsSum &out) {
        // Dynamic offsets
        const ExpressionPattern &exp_pattern = segment.expression_pattern;
        const bool has_meaningful_dynamic_offset =
            !exp_pattern.dynamic_offset_map.empty() && exp_pattern.max_amplitude_level() != dynamic_level_from_type(DynamicType::NATURAL);

        if (has_meaningful_dynamic_offset) {
            auto out_it = out.dynamic_offset_map.begin();
            auto seg_it = exp_pattern.dynamic_offset_map.cbegin();

            while (out_it != out.dynamic_offset_map.end() && seg_it != exp_pattern.dynamic_offset_map.cend()) {
                out_it->second += seg_it->second;
                ++out_it;
                ++seg_it;
            }
        }

        // Pitch offsets
        const PitchPattern &pitch_pattern = segment.pitch_pattern;
        const bool has_meaningful_pitch_offset = !pitch_pattern.pitch_offset_map.empty() && pitch_pattern.max_amplitude_level() != 0;

        if (has_meaningful_pitch_offset) {
            auto out_it = out.pitch_offset_map.begin();
            auto seg_it = pitch_pattern.pitch_offset_map.cbegin();

            while (out_it != out.pitch_offset_map.end() && seg_it != pitch_pattern.pitch_offset_map.cend()) {
                out_it->second += seg_it->second;
                ++out_it;
                ++seg_it;
            }
        }
    }

    void calculate_average(const ParamsSum &params_sum) {
        int count = static_cast<int>(size());

        if (count == 1) {
            return;
        }

        int dynamic_changes_count = 0;
        int pitch_changes_count = 0;
        int timestamp_changes_count = 0;

        for (auto it = cbegin(); it != cend(); ++it) {
            // Only articulations with a meaningful dynamic offset participate in the average: an empty
            // dynamicOffsetMap means "use the nominal dynamic" (see sumUpOffsets), so it must not count as a
            // dynamic change. Otherwise two articulations without an expression pattern (the default in the
            // 5.0 profiles) produce a non-empty all-zero average map, and noteVelocity() then reads a maximum
            // amplitude level of 0 instead of falling back to the nominal dynamic level.
            const ExpressionPattern &exp_pattern = it->second.applied_pattern_segment.expression_pattern;
            const bool has_meaningful_dynamic_offset = !exp_pattern.dynamic_offset_map.empty() &&
                                                       exp_pattern.max_amplitude_level() != dynamic_level_from_type(DynamicType::NATURAL);

            if (has_meaningful_dynamic_offset) {
                dynamic_changes_count++;
            }

            if (it->second.applied_pattern_segment.pitch_pattern.max_amplitude_level() != 0) {
                pitch_changes_count++;
            }

            if (it->second.applied_pattern_segment.arrangement_pattern.timestamp_offset_value != 0) {
                timestamp_changes_count++;
            }
        }

        average_duration_factor_ = params_sum.duration_factor_value / count;

        if (timestamp_changes_count > 0) {
            average_timestamp_offset_ = params_sum.timestamp_offset_value / timestamp_changes_count;
        }

        if (dynamic_changes_count > 0) {
            average_max_amplitude_level_ = params_sum.max_amplitude_level / dynamic_changes_count;
            average_dynamic_range_ = params_sum.dynamic_range / dynamic_changes_count;

            for (const auto &pair : params_sum.dynamic_offset_map) {
                average_dynamic_offset_map_.insert_or_assign(pair.first, pair.second / dynamic_changes_count);
            }
        } else if (dynamic_changes_count == 0) {
            average_max_amplitude_level_ = cbegin()->second.applied_pattern_segment.expression_pattern.max_amplitude_level();
            average_dynamic_range_ = cbegin()->second.meta.overall_dynamic_changes_range;
            average_dynamic_offset_map_ = cbegin()->second.applied_pattern_segment.expression_pattern.dynamic_offset_map;
        }

        if (pitch_changes_count > 0) {
            average_pitch_range_ = params_sum.pitch_range / pitch_changes_count;

            for (const auto &pair : params_sum.pitch_offset_map) {
                average_pitch_offset_map_.insert_or_assign(pair.first, pair.second / pitch_changes_count);
            }
        } else if (pitch_changes_count == 0) {
            average_pitch_range_ = cbegin()->second.meta.overall_pitch_changes_range;
            average_pitch_offset_map_ = cbegin()->second.applied_pattern_segment.pitch_pattern.pitch_offset_map;
        }
    }

    duration_percentage_t average_duration_factor_ = ::muse::mpe::hundred_percent;
    duration_percentage_t average_timestamp_offset_ = 0;
    pitch_level_t average_pitch_range_ = 0;
    dynamic_level_t average_max_amplitude_level_ = 0;
    dynamic_level_t average_dynamic_range_ = 0;
    PitchPattern::PitchOffsetMap average_pitch_offset_map_;
    ExpressionPattern::DynamicOffsetMap average_dynamic_offset_map_;
};

//! Picks the accepted articulation with the lowest (rank, ArticulationType). A type's rank is its index in
//! `precedence`; unlisted types rank after every listed one, so the order is total. Returns UNDEFINED when
//! no articulation is accepted.
template <std::size_t N, typename Accept>
ArticulationType select_articulation(const ArticulationMap &articulations, const std::array<ArticulationType, N> &precedence,
                                     Accept &&accept) {
    ArticulationType best = ArticulationType::UNDEFINED;
    std::size_t best_rank = N + 1;

    // ascending ArticulationType, so the first type of a rank wins
    for (const auto &pair : articulations) {
        if (!accept(pair.first)) {
            continue;
        }

        const std::size_t rank =
            static_cast<std::size_t>(std::find(precedence.cbegin(), precedence.cend(), pair.first) - precedence.cbegin());
        if (rank < best_rank) {
            best = pair.first;
            best_rank = rank;
        }
    }

    return best;
}
} // namespace muse::mpe
