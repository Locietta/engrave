// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "mpe/events.h"
#include "mpe/flatevents.h"

namespace music::test {
// Deliberately walks the legacy const surface only. No freeze/projection code.
struct LegacyDump {
    std::ostream &out;

    void bits(float value) {
        out << " f32:" << std::hex << std::setw(8) << std::setfill('0') << std::bit_cast<std::uint32_t>(value) << std::dec;
    }

    void bits(double value) {
        out << " f64:" << std::hex << std::setw(16) << std::setfill('0') << std::bit_cast<std::uint64_t>(value) << std::dec;
    }

    void text(const muse::String &value) {
        // Dump UTF-16 code units, including embedded NULs, without locale conversion.
        out << ' ' << value.size() << ':';
        for (std::size_t i = 0; i < value.size(); ++i)
            out << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(value.at(i).unicode());
        out << std::dec;
    }

    template <typename Curve>
    void curve(const Curve &value) {
        out << " curve " << value.size();
        for (const auto &[key, item] : value) out << ' ' << +key << ':' << +item;
        out << '\n';
    }

    void segment(const muse::mpe::ArticulationPatternSegment &value) {
        out << "segment " << value.arrangement_pattern.duration_factor_value << ' ' << value.arrangement_pattern.timestamp_offset_value
            << '\n';
        curve(value.pitch_pattern.pitch_offset_map);
        curve(value.expression_pattern.dynamic_offset_map);
    }

    static constexpr std::size_t note_event_index = 1;
    static_assert(std::is_same_v<std::variant_alternative_t<note_event_index, muse::mpe::PlaybackEvent>, muse::mpe::NoteEvent>);

    // One NoteEvent exactly as event() writes it, for the new-path dump A (serialized when it is frozen)
    void note(const muse::mpe::NoteEvent &e) {
        out << "event " << note_event_index << '\n';
        note_body(e);
    }

    void note_body(const muse::mpe::NoteEvent &e) {
        const auto &a = e.arrangement_ctx();
        const auto &p = e.pitch_ctx();
        const auto &x = e.expression_ctx();
        out << "arrangement " << a.nominal_timestamp << ' ' << a.actual_timestamp << ' ' << a.nominal_duration << ' ' << a.actual_duration
            << ' ' << +a.voice_layer_index << ' ' << +a.staff_layer_index;
        bits(a.bps);
        out << '\n';
        out << "pitch " << p.nominal_pitch_level << '\n';
        curve(p.pitch_curve);
        out << "expression " << x.nominal_dynamic_level << ' ' << x.velocity_override.has_value();
        if (x.velocity_override) bits(*x.velocity_override);
        out << '\n';
        curve(x.expression_curve);
        const auto &arts = x.articulations;
        out << "articulations " << arts.size() << '\n';
        for (const auto &[key, data] : arts) {
            const auto &m = data.meta;
            out << "art " << +static_cast<int>(key) << ' ' << static_cast<int>(m.type) << ' ' << m.timestamp << ' ' << m.overall_duration
                << ' ' << m.overall_pitch_changes_range << ' ' << m.overall_dynamic_changes_range << '\n';
            out << "pattern " << m.pattern.size() << '\n';
            for (const auto &[at, part] : m.pattern) {
                out << "at " << at << '\n';
                segment(part);
            }
            segment(data.applied_pattern_segment);
            out << "occupied " << data.occupied_from_value << ' ' << data.occupied_to_value << ' ' << data.occupied_pitch_changes_range
                << ' ' << data.occupied_dynamic_changes_range << '\n';
        }
        out << "averages " << arts.average_duration_factor() << ' ' << arts.average_timestamp_offset() << ' ' << arts.average_pitch_range()
            << ' ' << arts.average_max_amplitude_level() << ' ' << arts.average_dynamic_range() << '\n';
        curve(arts.average_pitch_offset_map());
        curve(arts.average_dynamic_offset_map());
    }

    void event(const muse::mpe::PlaybackEvent &value) {
        using namespace muse::mpe;
        out << "event " << value.index() << '\n';
        std::visit(
            [&](const auto &e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, NoteEvent>) {
                    note_body(e);
                } else if constexpr (std::is_same_v<T, ControllerChangeEvent>) {
                    out << "controller " << static_cast<int>(e.type) << ' ' << +e.layer_idx;
                    bits(e.val.raw());
                    out << '\n';
                } else if constexpr (std::is_same_v<T, TextArticulationEvent> || std::is_same_v<T, SyllableEvent>) {
                    out << "text " << +e.layer_idx << ' ' << static_cast<unsigned>(e.flags);
                    text(e.text);
                    out << '\n';
                } else if constexpr (std::is_same_v<T, SoundPresetChangeEvent>) {
                    out << "preset " << +e.layer_idx;
                    text(e.code);
                    out << '\n';
                } else {
                    static_assert(std::is_same_v<T, std::monostate>, "serialize new variants explicitly");
                    out << "monostate\n";
                }
            },
            value);
    }

    void track_header(std::uint64_t id, const muse::String &instrument, const muse::mpe::PlaybackSetupData &s) {
        out << "track " << id;
        text(instrument);
        out << '\n';
        out << "setup " << static_cast<int>(s.category) << ' ' << s.supports_single_note_dynamics;
        text(s.id);
        out << ' ' << s.sub_categories.size();
        for (const auto &sub : s.sub_categories) text(sub);
        out << ' ' << s.score_id.has_value();
        if (s.score_id) out << ' ' << std::quoted(*s.score_id);
        out << ' ' << s.music_xml_sound_id.has_value();
        if (s.music_xml_sound_id) out << ' ' << std::quoted(*s.music_xml_sound_id);
        out << '\n';
    }

    void dynamics_point(muse::mpe::timestamp_t at, const muse::mpe::AutomationPoint &point) {
        out << "point " << at << ' ' << point.in_value.index();
        bits(point.out_value.raw());
        if (const auto *v = std::get_if<muse::mpe::AutomationPoint::ExplicitArrival>(&point.in_value)) {
            bits(v->value.raw());
            bits(v->ease.t.raw());
            bits(v->ease.value.raw());
        }
        out << '\n';
    }

    void track(std::uint64_t id, const muse::String &instrument, const muse::mpe::PlaybackData &data) {
        track_header(id, instrument, data.setup_data);
        out << "dynamics " << data.dynamics.size() << '\n';
        for (const auto &[layer, points] : data.dynamics) {
            out << "layer " << +layer << ' ' << points.size() << '\n';
            for (const auto &[at, point] : points) dynamics_point(at, point);
        }
        out << "groups " << data.origin_events.size() << '\n';
        for (const auto &[at, events] : data.origin_events) {
            out << "group " << at << ' ' << events.size() << '\n';
            for (const auto &e : events) event(e);
        }
    }
};

// New-path dump A, in the legacy format: one track of the flat store, whose NoteEvents were serialized by
// LegacyDump::note when they were frozen (TrackEvents::note_dumps). Groups are rebuilt from the records'
// (group key, emission ordinal) pairs, the cold events and the empty-group marks; setup and dynamics come
// from the store. It never reads a legacy PlaybackData
struct FlatDump {
    std::ostream &out;

    void track(std::uint64_t id, const muse::String &instrument, const muse::mpe::TrackEvents &track) {
        using namespace muse::mpe;
        if (track.note_dumps.size() != track.notes.size()) throw std::runtime_error("dump A: a frozen note has no serialization");
        LegacyDump dump{out};
        dump.track_header(id, instrument, track.setup);
        out << "dynamics " << track.dynamic_layers.size() << '\n';
        for (const DynamicLayer &layer : track.dynamic_layers) {
            const auto points = track.layer_points(layer);
            out << "layer " << +layer.layer << ' ' << points.size() << '\n';
            for (const DynamicPoint &point : points) dump.dynamics_point(point.at, point.point);
        }

        enum class Kind { NOTE, COLD, EMPTY };

        struct Entry {
            timestamp_t group;
            std::uint32_t seq;
            Kind kind;
            std::size_t index;
        };

        std::vector<Entry> entries;
        entries.reserve(track.notes.size() + track.cold.size() + track.empty_groups.size());
        for (std::size_t i = 0; i < track.notes.size(); ++i)
            entries.push_back({track.notes[i].group_timestamp, track.notes[i].seq, Kind::NOTE, i});
        for (std::size_t i = 0; i < track.cold.size(); ++i)
            entries.push_back({track.cold[i].group_timestamp, track.cold[i].seq, Kind::COLD, i});
        for (std::size_t i = 0; i < track.empty_groups.size(); ++i)
            entries.push_back({track.empty_groups[i].group_timestamp, track.empty_groups[i].seq, Kind::EMPTY, i});
        std::sort(entries.begin(), entries.end(),
                  [](const Entry &a, const Entry &b) { return std::tie(a.group, a.seq) < std::tie(b.group, b.seq); });

        std::size_t groups = 0;
        for (std::size_t i = 0; i < entries.size(); ++i)
            if (i == 0 || entries[i].group != entries[i - 1].group) ++groups;
        out << "groups " << groups << '\n';
        for (std::size_t begin = 0; begin < entries.size();) {
            std::size_t end = begin, events = 0;
            for (; end < entries.size() && entries[end].group == entries[begin].group; ++end)
                if (entries[end].kind != Kind::EMPTY) ++events;
            out << "group " << entries[begin].group << ' ' << events << '\n';
            for (std::size_t i = begin; i < end; ++i) {
                if (entries[i].kind == Kind::NOTE)
                    out << track.note_dumps[entries[i].index];
                else if (entries[i].kind == Kind::COLD)
                    std::visit([&](const auto &event) { dump.event(PlaybackEvent{event}); }, track.cold[entries[i].index].event);
            }
            begin = end;
        }
    }
};
} // namespace music::test
