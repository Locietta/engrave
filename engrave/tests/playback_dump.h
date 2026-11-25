// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <tuple>
#include <variant>
#include <vector>

#include "mpe/flatevents.h"

namespace music::test {
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

    void note(const muse::mpe::TrackEvents &track, const muse::mpe::NoteRecord &n, const muse::mpe::ArticulationMap &articulations) {
        out << "event 1\n";
        out << "arrangement " << n.nominal_timestamp << ' ' << n.actual_timestamp << ' ' << n.nominal_duration << ' ' << n.actual_duration
            << ' ' << +n.voice_layer << ' ' << +n.staff_layer;
        bits(n.bps);
        out << '\n';
        out << "pitch " << n.nominal_pitch_level << '\n';
        curve(track.pitch_curve(n));
        out << "expression " << n.nominal_dynamic_level << ' ' << n.velocity_override.has_value();
        if (n.velocity_override) bits(*n.velocity_override);
        out << '\n';
        curve(track.expression_curve(n));
        articulation_data(articulations);
    }

    void articulation_data(const muse::mpe::ArticulationMap &arts) {
        out << "articulations " << arts.size() << '\n';
        for (const auto &[key, data] : arts) {
            const auto &m = data.meta;
            out << "art " << +static_cast<int>(key) << ' ' << static_cast<int>(m.type) << ' ' << m.timestamp << ' ' << m.overall_duration
                << ' ' << m.overall_pitch_changes_range << ' ' << m.overall_dynamic_changes_range << '\n';
            const auto &pattern = muse::mpe::pattern_or_empty(m.pattern);
            out << "pattern " << pattern.size() << '\n';
            for (const auto &[at, part] : pattern) {
                out << "at " << at << '\n';
                segment(part);
            }
            segment(data.applied_pattern_segment());
            out << "occupied " << data.occupied_from_value << ' ' << data.occupied_to_value << ' ' << data.occupied_pitch_changes_range
                << ' ' << data.occupied_dynamic_changes_range << '\n';
        }
        out << "averages " << arts.average_duration_factor() << ' ' << arts.average_timestamp_offset() << ' ' << arts.average_pitch_range()
            << ' ' << arts.average_max_amplitude_level() << ' ' << arts.average_dynamic_range() << '\n';
        curve(arts.average_pitch_offset_map());
        curve(arts.average_dynamic_offset_map());
    }

    // These IDs are part of Dump A's format.
    void cold_event(const muse::mpe::TextArticulationEvent &event) { text_event(2, event); }

    void cold_event(const muse::mpe::SoundPresetChangeEvent &event) {
        out << "event 3\npreset " << +event.layer_idx;
        text(event.code);
        out << '\n';
    }

    void cold_event(const muse::mpe::SyllableEvent &event) { text_event(4, event); }

    template <class Event>
    void text_event(const int id, const Event &event) {
        out << "event " << id << "\ntext " << +event.layer_idx << ' ' << static_cast<unsigned>(event.flags);
        text(event.text);
        out << '\n';
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
};

// Dump A rebuilds legacy map order from (group key, emission ordinal) pairs.
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
                    std::visit([&](const auto &event) { dump.cold_event(event); }, track.cold[entries[i].index].event);
            }
            begin = end;
        }
    }
};
} // namespace music::test
