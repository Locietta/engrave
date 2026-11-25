// SPDX-License-Identifier: GPL-3.0-only
#include "flatevents.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

#include "realfn.h"

namespace muse::mpe {
namespace {
std::uint32_t take_seq(TrackEvents &track) {
    const std::uint32_t seq = track.next_seq;
    track.next_seq = checked_grow(seq, 1);
    return seq;
}

template <class T>
std::uint32_t buffer_end(const Buf<T> &buf) {
    return checked_grow<T>(buf.size(), 0);
}

template <class Curve, class Scale = std::identity>
Span append_curve(TrackEvents &track, const Curve &curve, FreezeProgress &progress, Scale scale = {}) {
    Span span{buffer_end(track.curve_points), 0};
    for (const auto &[at, value] : curve) {
        emplace_checked(track.curve_points, CurvePoint{at, scale(value)});
        progress.advance();
    }
    span.count = buffer_end(track.curve_points) - span.first;
    return span;
}

Span append_articulations(TrackEvents &track, const ArticulationMap &articulations, FreezeProgress &progress) {
    Span span{buffer_end(track.articulations), 0};
    for (const auto &[type, data] : articulations) {
        if (data.meta.type != type) {
            throw std::logic_error("articulation key differs from its meta type");
        }
        emplace_checked(track.articulations, ArticulationRecord{data.meta.timestamp, data.meta.overall_duration, type});
        progress.advance();
    }
    span.count = buffer_end(track.articulations) - span.first;
    return span;
}

void append_note(TrackEvents &track, const NoteRecord &record, [[maybe_unused]] const ArticulationMap &articulations,
                 FreezeProgress &progress) {
    emplace_checked(track.notes, record);
#ifdef ENGRAVE_PLAYBACK_HARNESS
    if (progress.dump_note) {
        track.note_dumps.push_back(progress.dump_note(track, record, articulations));
    }
#endif
    progress.advance();
}

Span append_pitch_curve(TrackEvents &track, const ArticulationMap &articulations, std::span<const CurvePoint> custom_pitch_curve,
                        FreezeProgress &progress) {
    if (!custom_pitch_curve.empty()) {
        for (std::size_t i = 1; i < custom_pitch_curve.size(); ++i) {
            if (custom_pitch_curve[i].at <= custom_pitch_curve[i - 1].at) {
                throw std::logic_error("custom pitch curve must have strictly ascending keys");
            }
        }
        const auto *begin = track.curve_points.data();
        const auto *end = begin + track.curve_points.size();
        const bool aliases_pool = !track.curve_points.empty() && !std::less<const CurvePoint *>{}(custom_pitch_curve.data(), begin) &&
                                  std::less<const CurvePoint *>{}(custom_pitch_curve.data(), end);
        if (aliases_pool) {
            const std::size_t src_first = static_cast<std::size_t>(custom_pitch_curve.data() - begin);
            const std::size_t src_count = custom_pitch_curve.size();
            reserve_checked(track.curve_points, checked_grow<CurvePoint>(track.curve_points.size(), src_count));
            const Span span{buffer_end(track.curve_points), 0};
            for (std::size_t i = 0; i < src_count; ++i) {
                const CurvePoint point = track.curve_points[src_first + i];
                emplace_checked(track.curve_points, point);
                progress.advance();
            }
            return {span.first, buffer_end(track.curve_points) - span.first};
        }
        return append_curve(track, custom_pitch_curve, progress);
    }

    const auto &curve = articulations.average_pitch_offset_map();
    const pitch_level_t average_pitch_range = articulations.average_pitch_range();
    if (average_pitch_range == 0 || average_pitch_range == pitch_level_step) {
        return append_curve(track, curve, progress);
    }

    const float ratio = static_cast<float>(average_pitch_range) / static_cast<float>(pitch_level_step);
    const float pattern_unit_ratio = pitch_level_step / static_cast<float>(one_percent);
    constexpr double min_level = std::numeric_limits<pitch_level_t>::min();
    constexpr double max_level = std::numeric_limits<pitch_level_t>::max();
    return append_curve(track, curve, progress, [=](pitch_level_t value) {
        return static_cast<pitch_level_t>(
            std::clamp(real_round(static_cast<float>(value) * ratio * pattern_unit_ratio, 0), min_level, max_level));
    });
}

Span append_expression_curve(TrackEvents &track, const NoteEmitInput &input, const ArticulationMap &articulations,
                             FreezeProgress &progress) {
    const auto &curve = articulations.average_dynamic_offset_map();
    if (curve.empty()) {
        return {buffer_end(track.curve_points), 0};
    }

    const dynamic_level_t articulation_dynamic_level = articulations.average_max_amplitude_level();
    constexpr dynamic_level_t natural_dynamic_level = dynamic_level_from_type(DynamicType::NATURAL);
    const auto dynamic_range = std::max(articulations.average_dynamic_range(), dynamic_level_step);
    const double amplitude_diff = static_cast<double>(articulation_dynamic_level) - natural_dynamic_level;
    const double amplification_diff = std::trunc(amplitude_diff * dynamic_range / dynamic_level_step);
    const double actual_dynamic_level = input.nominal_dynamic_level + amplification_diff;

    if (articulation_dynamic_level == 0 || actual_dynamic_level == articulation_dynamic_level) {
        return append_curve(track, curve, progress);
    }

    const double ratio = actual_dynamic_level / articulation_dynamic_level;
    constexpr double min_level = std::numeric_limits<dynamic_level_t>::min();
    constexpr double max_level = std::numeric_limits<dynamic_level_t>::max();
    return append_curve(track, curve, progress, [=](dynamic_level_t value) {
        return static_cast<dynamic_level_t>(std::clamp(real_round(value * ratio, 0), min_level, max_level));
    });
}

template <typename T>
T mult(T value, float factor) {
    return static_cast<T>(static_cast<float>(value) * factor);
}

template <class Event>
void freeze_cold(TrackEvents &track, const timestamp_t group_timestamp, const Event &event, FreezeProgress &progress) {
    track.cold.push_back(ColdEvent{group_timestamp, take_seq(track), event});
    progress.advance();
}

template <class Event>
void freeze_cold_list(TrackEvents &track, const timestamp_t group_timestamp, std::span<const Event> events, FreezeProgress &progress) {
    if (events.empty()) {
        emit_empty_group(track, group_timestamp, progress);
        return;
    }
    for (const Event &event : events) {
        freeze_cold(track, group_timestamp, event, progress);
    }
}
} // namespace

NoteTiming compute_note_timing(const NoteEmitInput &input, const ArticulationMap &articulations) {
    NoteTiming timing{input.nominal_timestamp, input.nominal_duration};
    if (!articulations.empty() && input.nominal_duration != infinite_duration) {
        timing.actual_duration = mult(input.nominal_duration, percentage_to_factor(articulations.average_duration_factor()));
        timing.actual_timestamp += mult(input.nominal_duration, percentage_to_factor(articulations.average_timestamp_offset()));
    }
    if (input.clamp_negative_timestamp && timing.actual_timestamp < 0) {
        if (timing.actual_duration != infinite_duration) {
            timing.actual_duration += timing.actual_timestamp;
        }
        timing.actual_timestamp = 0;
    }
    return timing;
}

void emit_note(TrackEvents &track, const timestamp_t group_timestamp, const NoteEmitInput &input, const ArticulationMap &articulations,
               std::span<const CurvePoint> custom_pitch_curve, FreezeProgress &progress) {
    NoteRecord record;
    record.group_timestamp = group_timestamp;
    record.seq = take_seq(track);
    const NoteTiming timing = compute_note_timing(input, articulations);
    record.nominal_timestamp = input.nominal_timestamp;
    record.actual_timestamp = timing.actual_timestamp;
    record.nominal_duration = input.nominal_duration;
    record.actual_duration = timing.actual_duration;
    record.bps = input.bps;
    record.nominal_pitch_level = input.nominal_pitch_level;
    record.nominal_dynamic_level = input.nominal_dynamic_level;
    if (!real_is_null(input.required_velocity_fraction)) {
        record.velocity_override = input.required_velocity_fraction;
    }
    record.staff_layer = input.staff_layer;
    record.voice_layer = input.voice_layer;

    record.articulations = append_articulations(track, articulations, progress);
    record.pitch_curve = append_pitch_curve(track, articulations, custom_pitch_curve, progress);
    record.expression_curve = append_expression_curve(track, input, articulations, progress);
    append_note(track, record, articulations, progress);
}

void emit_empty_group(TrackEvents &track, const timestamp_t group_timestamp, FreezeProgress &progress) {
    emplace_checked(track.empty_groups, EmptyGroup{group_timestamp, take_seq(track)});
    progress.advance();
}

void freeze_cold_events(TrackEvents &track, const timestamp_t group_timestamp, std::span<const SoundPresetChangeEvent> events,
                        FreezeProgress &progress) {
    freeze_cold_list(track, group_timestamp, events, progress);
}

void freeze_cold_events(TrackEvents &track, const timestamp_t group_timestamp, std::span<const TextArticulationEvent> events,
                        FreezeProgress &progress) {
    freeze_cold_list(track, group_timestamp, events, progress);
}

void freeze_cold_events(TrackEvents &track, const timestamp_t group_timestamp, std::span<const SyllableEvent> events,
                        FreezeProgress &progress) {
    freeze_cold_list(track, group_timestamp, events, progress);
}

void freeze_dynamics(TrackEvents &track, const DynamicAutomationLayers &dynamics, FreezeProgress &progress) {
    track.dynamic_layers.clear();
    track.dynamic_points.clear();

    for (const auto &[layer, curve] : dynamics) {
        DynamicLayer record{layer, {buffer_end(track.dynamic_points), 0}};
        if (curve) {
            for (const auto &[at, point] : *curve) {
                emplace_checked(track.dynamic_points, DynamicPoint{at, point});
                progress.advance();
            }
        }
        record.points.count = buffer_end(track.dynamic_points) - record.points.first;
        emplace_checked(track.dynamic_layers, record);
    }
}

} // namespace muse::mpe
