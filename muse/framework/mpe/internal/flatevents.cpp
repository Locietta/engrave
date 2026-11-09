// SPDX-License-Identifier: GPL-3.0-only
#include "flatevents.h"

#include <limits>
#include <stdexcept>
#include <type_traits>

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

template <class Curve>
Span freeze_curve(TrackEvents &track, const Curve &curve, FreezeProgress &progress) {
    static_assert(std::is_same_v<typename Curve::mapped_type, decltype(CurvePoint::value)>, "curve values are stored unnarrowed");
    Span span{buffer_end(track.curve_points), 0};
    for (const auto &[at, value] : curve) {
        emplace_checked(track.curve_points, CurvePoint{at, value});
        progress.advance();
    }
    span.count = buffer_end(track.curve_points) - span.first;
    return span;
}

void freeze_note(TrackEvents &track, const timestamp_t group_timestamp, const NoteEvent &event, FreezeProgress &progress) {
    const ArrangementContext &arrangement = event.arrangement_ctx();
    const PitchContext &pitch = event.pitch_ctx();
    const ExpressionContext &expression = event.expression_ctx();

    NoteRecord record;
    record.group_timestamp = group_timestamp;
    record.seq = take_seq(track);
    record.nominal_timestamp = arrangement.nominal_timestamp;
    record.actual_timestamp = arrangement.actual_timestamp;
    record.nominal_duration = arrangement.nominal_duration;
    record.actual_duration = arrangement.actual_duration;
    record.bps = arrangement.bps;
    record.nominal_pitch_level = pitch.nominal_pitch_level;
    record.nominal_dynamic_level = expression.nominal_dynamic_level;
    record.velocity_override = expression.velocity_override;
    record.staff_layer = arrangement.staff_layer_index;
    record.voice_layer = arrangement.voice_layer_index;

    // The map iterates in ascending ArticulationType, the canonical order
    record.articulations.first = buffer_end(track.articulations);
    for (const auto &[type, data] : expression.articulations) {
        if (data.meta.type != type) {
            throw std::logic_error("articulation key differs from its meta type");
        }
        emplace_checked(track.articulations, ArticulationRecord{data.meta.timestamp, data.meta.overall_duration, type});
        progress.advance();
    }
    record.articulations.count = buffer_end(track.articulations) - record.articulations.first;

    record.pitch_curve = freeze_curve(track, pitch.pitch_curve, progress);
    record.expression_curve = freeze_curve(track, expression.expression_curve, progress);

    emplace_checked(track.notes, record);
#ifdef ENGRAVE_PLAYBACK_HARNESS
    if (progress.dump_note) {
        track.note_dumps.push_back(progress.dump_note(event));
    }
#endif
    progress.advance();
}

void freeze_empty_group(TrackEvents &track, const timestamp_t group_timestamp, FreezeProgress &progress) {
    emplace_checked(track.empty_groups, EmptyGroup{group_timestamp, take_seq(track)});
    progress.advance();
}

template <class Event>
void freeze_cold(TrackEvents &track, const timestamp_t group_timestamp, const Event &event, FreezeProgress &progress) {
    track.cold.push_back(ColdEvent{group_timestamp, take_seq(track), event});
    progress.advance();
}

template <class Event>
void freeze_cold_list(TrackEvents &track, const timestamp_t group_timestamp, std::span<const Event> events, FreezeProgress &progress) {
    if (events.empty()) {
        freeze_empty_group(track, group_timestamp, progress);
        return;
    }
    for (const Event &event : events) {
        freeze_cold(track, group_timestamp, event, progress);
    }
}
} // namespace

void freeze_events(TrackEvents &track, const timestamp_t group_timestamp, std::span<const PlaybackEvent> events, FreezeProgress &progress) {
    if (events.empty()) {
        freeze_empty_group(track, group_timestamp, progress);
        return;
    }

    for (const PlaybackEvent &event : events) {
        std::visit(
            [&](const auto &value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, NoteEvent>) {
                    freeze_note(track, group_timestamp, value, progress);
                } else if constexpr (std::is_same_v<T, TextArticulationEvent> || std::is_same_v<T, SoundPresetChangeEvent> ||
                                     std::is_same_v<T, SyllableEvent>) {
                    freeze_cold(track, group_timestamp, value, progress);
                } else if constexpr (std::is_same_v<T, std::monostate>) {
                    throw std::logic_error("flat playback store: monostate event has no producer");
                } else {
                    static_assert(std::is_same_v<T, ControllerChangeEvent>, "freeze every PlaybackEvent variant explicitly");
                    throw std::logic_error("flat playback store: ControllerChangeEvent has no producer");
                }
            },
            event);
    }
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
        for (const auto &[at, point] : curve) {
            emplace_checked(track.dynamic_points, DynamicPoint{at, point});
            progress.advance();
        }
        record.points.count = buffer_end(track.dynamic_points) - record.points.first;
        emplace_checked(track.dynamic_layers, record);
    }
}

TrackEvents freeze_playback_data(const PlaybackData &data) {
    TrackEvents track;
    FreezeProgress progress;
    track.setup = data.setup_data;
    freeze_dynamics(track, data.dynamics, progress);
    for (const auto &[group, events] : data.origin_events) {
        freeze_events(track, group, std::span{events}, progress);
    }
    return track;
}
} // namespace muse::mpe
