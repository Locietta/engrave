// SPDX-License-Identifier: GPL-3.0-only
#pragma once

//! NOTE: the flat MPE store (playback DoD design 4.1). One TrackEvents holds a track's frozen
//! per-chord event lists as plain records in emission order: every record carries its legacy group
//! key (the PlaybackEventsMap timestamp) and a per-track emission ordinal shared by notes, cold
//! events and empty groups, so a stable order by (group, seq) is the legacy map order. Owner thread only

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <amc/vector.hpp>

#include "events.h"

namespace muse::mpe {
//! Thrown when a flat playback buffer or an offset in it would leave the uint32 format; the
//! export maps it to RESOURCE_LIMIT. Release builds keep the check
struct StoreLimit {
    const char *message = "playback buffer exceeds uint32 offset limit";
};

//! Trivially copyable payloads only: amc relocates by memcpy. Nothing on the playback path declares
//! trivially_relocatable
template <class T>
    requires std::is_trivially_copyable_v<T>
using Buf = amc::vector<T>;

template <class T = std::byte>
constexpr std::uint32_t checked_grow(std::uint64_t size, std::uint64_t count) {
    static_assert(std::is_trivially_copyable_v<T>);
    constexpr std::uint64_t limit = std::numeric_limits<std::uint32_t>::max();
    if (size > limit || count > limit - size) {
        throw StoreLimit{};
    }
    const std::uint64_t total = size + count;
    if (total > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
        throw StoreLimit{};
    }
    return static_cast<std::uint32_t>(total);
}

template <class T>
void reserve_checked(Buf<T> &buf, std::uint64_t count) {
    buf.reserve(checked_grow<T>(0, count));
}

template <class T, class... Args>
T &emplace_checked(Buf<T> &buf, Args &&...args) {
    std::ignore = checked_grow<T>(buf.size(), 1);
    return buf.emplace_back(std::forward<Args>(args)...);
}

struct Span {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

struct ArticulationRecord {
    timestamp_t timestamp = 0;
    duration_t overall_duration = 0;
    ArticulationType type = ArticulationType::UNDEFINED;
};

//! A ValuesCurve entry; value is range-checked into int32 when frozen, value() restores the curve type
struct CurvePoint {
    duration_percentage_t at = 0;
    std::int32_t value32 = 0;

    percentage_t value() const { return static_cast<percentage_t>(value32); }
};

struct NoteRecord {
    timestamp_t group_timestamp = 0;
    std::uint32_t seq = 0;
    timestamp_t nominal_timestamp = 0;
    timestamp_t actual_timestamp = 0;
    duration_t nominal_duration = 0;
    duration_t actual_duration = 0;
    double bps = 0.0;
    Span articulations;    // canonical (ascending ArticulationType) order
    Span pitch_curve;      // key order
    Span expression_curve; // key order
    pitch_level_t nominal_pitch_level = 0;
    dynamic_level_t nominal_dynamic_level = 0;
    std::optional<float> velocity_override;
    staff_layer_idx_t staff_layer = 0;
    voice_layer_idx_t voice_layer = 0;
};

struct DynamicPoint {
    timestamp_t at = 0;
    AutomationPoint point;
};

struct DynamicLayer {
    layer_idx_t layer = 0;
    Span points;
};

//! Text, preset and syllable events, kept whole for full identity. Not trivially copyable: std::vector only
struct ColdEvent {
    timestamp_t group_timestamp = 0;
    std::uint32_t seq = 0;
    std::variant<TextArticulationEvent, SoundPresetChangeEvent, SyllableEvent> event;
};

//! A group key emitted with no event; the legacy map keeps such keys
struct EmptyGroup {
    timestamp_t group_timestamp = 0;
    std::uint32_t seq = 0;
};

static_assert(std::is_trivially_copyable_v<Span>);
static_assert(std::is_trivially_copyable_v<ArticulationRecord>);
static_assert(std::is_trivially_copyable_v<CurvePoint>);
static_assert(std::is_trivially_copyable_v<NoteRecord>);
static_assert(std::is_trivially_copyable_v<AutomationPoint>, "DynamicPoint must fall back to std::vector");
static_assert(std::is_trivially_copyable_v<DynamicPoint>);
static_assert(std::is_trivially_copyable_v<DynamicLayer>);
static_assert(std::is_trivially_copyable_v<EmptyGroup>);

struct TrackEvents {
    Buf<NoteRecord> notes;
    Buf<ArticulationRecord> articulations;
    Buf<CurvePoint> curve_points;
    Buf<DynamicLayer> dynamic_layers; // ascending layer
    Buf<DynamicPoint> dynamic_points;
    Buf<EmptyGroup> empty_groups;
    std::vector<ColdEvent> cold;
    PlaybackSetupData setup; // deferred exception E3
    std::uint32_t next_seq = 0;

    std::span<const ArticulationRecord> note_articulations(const NoteRecord &note) const { return view(articulations, note.articulations); }

    std::span<const CurvePoint> pitch_curve(const NoteRecord &note) const { return view(curve_points, note.pitch_curve); }

    std::span<const CurvePoint> expression_curve(const NoteRecord &note) const { return view(curve_points, note.expression_curve); }

    std::span<const DynamicPoint> layer_points(const DynamicLayer &layer) const { return view(dynamic_points, layer.points); }

private:
    template <class T>
    static std::span<const T> view(const Buf<T> &buf, const Span span) {
        if (checked_grow<T>(span.first, span.count) > buf.size()) {
            throw StoreLimit{"flat record span exceeds its buffer"};
        }
        return {buf.data() + span.first, span.count};
    }
};

//! Appends one completed per-chord list under its group key. NoteEvents become NoteRecords, text,
//! preset and syllable events become cold events; an empty list records the empty group.
//! monostate and ControllerChangeEvent have no producer and throw std::logic_error
void freeze_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const PlaybackEvent> events);

//! Context events that the model inserts straight into the legacy map (presets, text, syllables)
void freeze_cold_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const SoundPresetChangeEvent> events);
void freeze_cold_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const TextArticulationEvent> events);
void freeze_cold_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const SyllableEvent> events);

//! Replaces the track's dynamics with a copy of the layers (an assignment, as in the legacy model)
void freeze_dynamics(TrackEvents &track, const DynamicAutomationLayers &dynamics);
} // namespace muse::mpe
