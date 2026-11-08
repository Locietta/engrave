// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <bit>
#include <iomanip>
#include <ostream>
#include <type_traits>

#include "mpe/events.h"

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

    void event(const muse::mpe::PlaybackEvent &value) {
        using namespace muse::mpe;
        out << "event " << value.index() << '\n';
        std::visit(
            [&](const auto &e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, NoteEvent>) {
                    const auto &a = e.arrangement_ctx();
                    const auto &p = e.pitch_ctx();
                    const auto &x = e.expression_ctx();
                    out << "arrangement " << a.nominal_timestamp << ' ' << a.actual_timestamp << ' ' << a.nominal_duration << ' '
                        << a.actual_duration << ' ' << +a.voice_layer_index << ' ' << +a.staff_layer_index;
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
                        out << "art " << +static_cast<int>(key) << ' ' << static_cast<int>(m.type) << ' ' << m.timestamp << ' '
                            << m.overall_duration << ' ' << m.overall_pitch_changes_range << ' ' << m.overall_dynamic_changes_range << '\n';
                        out << "pattern " << m.pattern.size() << '\n';
                        for (const auto &[at, part] : m.pattern) {
                            out << "at " << at << '\n';
                            segment(part);
                        }
                        segment(data.applied_pattern_segment);
                        out << "occupied " << data.occupied_from_value << ' ' << data.occupied_to_value << ' '
                            << data.occupied_pitch_changes_range << ' ' << data.occupied_dynamic_changes_range << '\n';
                    }
                    out << "averages " << arts.average_duration_factor() << ' ' << arts.average_timestamp_offset() << ' '
                        << arts.average_pitch_range() << ' ' << arts.average_max_amplitude_level() << ' ' << arts.average_dynamic_range()
                        << '\n';
                    curve(arts.average_pitch_offset_map());
                    curve(arts.average_dynamic_offset_map());
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

    void track(std::uint64_t id, const muse::String &instrument, const muse::mpe::PlaybackData &data) {
        out << "track " << id;
        text(instrument);
        out << '\n';
        const auto &s = data.setup_data;
        out << "setup " << static_cast<int>(s.category) << ' ' << s.supports_single_note_dynamics;
        text(s.id);
        out << ' ' << s.sub_categories.size();
        for (const auto &sub : s.sub_categories) text(sub);
        out << ' ' << s.score_id.has_value();
        if (s.score_id) out << ' ' << std::quoted(*s.score_id);
        out << ' ' << s.music_xml_sound_id.has_value();
        if (s.music_xml_sound_id) out << ' ' << std::quoted(*s.music_xml_sound_id);
        out << "\ndynamics " << data.dynamics.size() << '\n';
        for (const auto &[layer, points] : data.dynamics) {
            out << "layer " << +layer << ' ' << points.size() << '\n';
            for (const auto &[at, point] : points) {
                out << "point " << at << ' ' << point.in_value.index();
                bits(point.out_value.raw());
                if (const auto *v = std::get_if<muse::mpe::AutomationPoint::ExplicitArrival>(&point.in_value)) {
                    bits(v->value.raw());
                    bits(v->ease.t.raw());
                    bits(v->ease.value.raw());
                }
                out << '\n';
            }
        }
        out << "groups " << data.origin_events.size() << '\n';
        for (const auto &[at, events] : data.origin_events) {
            out << "group " << at << ' ' << events.size() << '\n';
            for (const auto &e : events) event(e);
        }
    }
};
} // namespace music::test
