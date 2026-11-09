// SPDX-License-Identifier: GPL-3.0-only
#pragma once

//! NOTE: the flat MPE store (playback DoD design 4.1). One TrackEvents holds a track's frozen
//! per-chord event lists as plain records in emission order: every record carries its legacy group
//! key (the PlaybackEventsMap timestamp) and a per-track emission ordinal shared by notes, cold
//! events and empty groups, so a stable order by (group, seq) is the legacy map order. Owner thread only

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <amc/vector.hpp>

#include "events.h"
#include "flatmap.h"

namespace muse::mpe {
struct Span {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

//! The records of buf that span names; a span outside the buffer throws StoreLimit
template <class T>
std::span<const T> checked_span(const Buf<T> &buf, const Span span) {
    if (checked_grow<T>(span.first, span.count) > buf.size()) {
        throw StoreLimit{"flat record span exceeds its buffer"};
    }
    return {buf.data() + span.first, span.count};
}

//! The ArticulationMeta fields the sequencer reads; type is also the legacy map key
struct ArticulationRecord {
    timestamp_t timestamp = 0;
    duration_t overall_duration = 0;
    ArticulationType type = ArticulationType::UNDEFINED;

    // As ArticulationMeta::has_start/has_end
    bool has_start() const { return overall_duration > 0; }

    bool has_end() const { return overall_duration != infinite_duration; }
};

//! Selection helpers (select_articulation) read the type of a flat record as of an ArticulationMap entry
inline ArticulationType articulation_type(const ArticulationRecord &record) { return record.type; }

inline bool contains_articulation(std::span<const ArticulationRecord> articulations, const ArticulationType type) {
    for (const ArticulationRecord &record : articulations) {
        if (record.type == type) {
            return true;
        }
    }
    return false;
}

//! A ValuesCurve entry in the curve's own key and value types (design B3: no narrowing). The sorted-range
//! helpers (amplitude_value_point, velocity_fraction, ...) therefore run on the legacy arithmetic types on
//! every platform; percentage_t is int_fast16_t, 32-bit on MSVC and 64-bit on glibc/libstdc++
struct CurvePoint {
    duration_percentage_t at = 0;
    percentage_t value = 0;
};

struct NoteRecord {
    timestamp_t group_timestamp = 0;
    std::uint32_t seq = 0;
    timestamp_t nominal_timestamp = 0;
    timestamp_t actual_timestamp = 0;
    duration_t nominal_duration = 0;
    duration_t actual_duration = 0;
    double bps = 0.0;
    Span articulations;    // canonical (ascending ArticulationType) order, as the legacy map iterates
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

// The flat curve entry restores exactly the legacy curve types, whatever their platform width
static_assert(std::is_same_v<pitch_level_t, percentage_t> && std::is_same_v<dynamic_level_t, percentage_t>);
static_assert(std::is_same_v<PitchCurve::key_type, decltype(CurvePoint::at)>);
static_assert(std::is_same_v<PitchCurve::mapped_type, decltype(CurvePoint::value)>);
static_assert(std::is_same_v<ExpressionCurve::key_type, decltype(CurvePoint::at)>);
static_assert(std::is_same_v<ExpressionCurve::mapped_type, decltype(CurvePoint::value)>);
static_assert(std::is_same_v<sorted_key_t<std::span<const CurvePoint>>, sorted_key_t<PitchCurve>>);
static_assert(std::is_same_v<sorted_value_t<std::span<const CurvePoint>>, sorted_value_t<PitchCurve>>);
static_assert(std::is_same_v<sorted_value_t<std::span<const CurvePoint>>, sorted_value_t<ExpressionCurve>>);
static_assert(sizeof(CurvePoint::value) == sizeof(percentage_t) && sizeof(CurvePoint::at) == sizeof(duration_percentage_t));

//! Bounded cancellation inside the freeze loops (design 4.4): checkpoint runs after every
//! k_stride frozen records (notes, articulations, curve points, cold events, empty groups,
//! dynamic points), counted across calls. It may throw to abort; the store is then discarded
struct FreezeProgress {
    static constexpr std::uint32_t k_stride = 4096;

    std::function<void()> checkpoint;
    std::uint32_t pending = 0;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    //! Test only: serializes each NoteEvent as it is frozen (new-path dump A); the text goes to TrackEvents::note_dumps
    std::function<std::string(const NoteEvent &)> dump_note;
#endif

    void advance() {
        if (++pending < k_stride) {
            return;
        }
        pending = 0;
        if (checkpoint) {
            checkpoint();
        }
    }
};

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
#ifdef ENGRAVE_PLAYBACK_HARNESS
    //! Test only: FreezeProgress::dump_note's text for notes[i], when set for the whole export
    std::vector<std::string> note_dumps;
#endif

    std::span<const ArticulationRecord> note_articulations(const NoteRecord &note) const { return view(articulations, note.articulations); }

    std::span<const CurvePoint> pitch_curve(const NoteRecord &note) const { return view(curve_points, note.pitch_curve); }

    std::span<const CurvePoint> expression_curve(const NoteRecord &note) const { return view(curve_points, note.expression_curve); }

    std::span<const DynamicPoint> layer_points(const DynamicLayer &layer) const { return view(dynamic_points, layer.points); }

private:
    template <class T>
    static std::span<const T> view(const Buf<T> &buf, const Span span) {
        return checked_span(buf, span);
    }
};

//! Appends one completed per-chord list under its group key. NoteEvents become NoteRecords, text,
//! preset and syllable events become cold events; an empty list records the empty group.
//! monostate and ControllerChangeEvent have no producer and throw std::logic_error
void freeze_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const PlaybackEvent> events, FreezeProgress &progress);

//! Context events that the model inserts straight into the legacy map (presets, text, syllables)
void freeze_cold_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const SoundPresetChangeEvent> events,
                        FreezeProgress &progress);
void freeze_cold_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const TextArticulationEvent> events,
                        FreezeProgress &progress);
void freeze_cold_events(TrackEvents &track, timestamp_t group_timestamp, std::span<const SyllableEvent> events, FreezeProgress &progress);

//! Replaces the track's dynamics with a copy of the layers (an assignment, as in the legacy model)
void freeze_dynamics(TrackEvents &track, const DynamicAutomationLayers &dynamics, FreezeProgress &progress);

//! One legacy track as a flat track: setup, dynamics, then every group in key order. For tests and
//! oracle comparisons that build legacy data directly
TrackEvents freeze_playback_data(const PlaybackData &data);
} // namespace muse::mpe
