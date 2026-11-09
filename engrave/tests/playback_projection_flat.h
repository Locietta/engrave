// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Dump B, flat side: serializes the flat records of a PlaybackScore. Groups are rebuilt from the
// records' (group key, emission ordinal) pairs and the empty-group marks. It reads only the flat
// store and shares no code with the legacy extractor (playback_projection_legacy.h).

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "engraving/playback/flatplaybackscore.h"

namespace music::test {
struct FlatProjection {
    std::ostringstream out;

    template <class Unsigned>
    void hex(Unsigned value, int width) {
        out << std::hex << std::setw(width) << std::setfill('0') << value << std::dec << std::setfill(' ');
    }

    void units(const muse::String &text) {
        out << ' ' << text.size() << ':';
        for (std::size_t i = 0; i < text.size(); ++i) hex(static_cast<unsigned>(text.at(i).unicode()), 4);
    }

    void bytes(const std::optional<std::string> &text) {
        if (!text.has_value()) {
            out << " -";
            return;
        }
        out << ' ' << text->size() << ':';
        for (const unsigned char c : *text) hex(static_cast<unsigned>(c), 2);
    }

    void points(const char *label, std::span<const muse::mpe::CurvePoint> curve) {
        out << label << ' ' << curve.size();
        for (const muse::mpe::CurvePoint &point : curve)
            out << ' ' << static_cast<long long>(point.at) << ':' << static_cast<long long>(point.value());
        out << '\n';
    }

    void note(const muse::mpe::TrackEvents &track, const muse::mpe::NoteRecord &n) {
        out << "note " << n.nominal_timestamp << ' ' << n.actual_timestamp << ' ' << n.nominal_duration << ' ' << n.actual_duration << ' ';
        hex(std::bit_cast<std::uint64_t>(n.bps), 16);
        out << ' ' << static_cast<long long>(n.nominal_pitch_level) << ' ' << static_cast<long long>(n.nominal_dynamic_level) << ' ';
        if (n.velocity_override.has_value())
            hex(std::bit_cast<std::uint32_t>(n.velocity_override.value()), 8);
        else
            out << '-';
        out << ' ' << static_cast<unsigned long long>(n.staff_layer) << ' ' << static_cast<unsigned long long>(n.voice_layer) << '\n';
        const auto articulations = track.note_articulations(n);
        out << "arts " << articulations.size();
        for (const muse::mpe::ArticulationRecord &a : articulations)
            out << ' ' << static_cast<int>(a.type) << ':' << a.timestamp << ':' << a.overall_duration;
        out << '\n';
        points("pitch", track.pitch_curve(n));
        points("expression", track.expression_curve(n));
    }

    void cold(const muse::mpe::ColdEvent &c) {
        using namespace muse::mpe;
        switch (c.event.index()) {
            case 0: {
                const auto &t = std::get<TextArticulationEvent>(c.event);
                out << "text " << static_cast<unsigned long long>(t.layer_idx) << ' ' << static_cast<unsigned>(t.flags);
                units(t.text);
                break;
            }
            case 1: {
                const auto &p = std::get<SoundPresetChangeEvent>(c.event);
                out << "preset " << static_cast<unsigned long long>(p.layer_idx);
                units(p.code);
                break;
            }
            default: {
                const auto &s = std::get<SyllableEvent>(c.event);
                out << "syllable " << static_cast<unsigned long long>(s.layer_idx) << ' ' << static_cast<unsigned>(s.flags);
                units(s.text);
                break;
            }
        }
        out << '\n';
    }

    void track(const mu::engraving::InstrumentTrackId &id, const muse::mpe::TrackEvents &track) {
        out << "track " << id.part_id.to_uint64();
        units(id.instrument_id);
        out << '\n';
        const muse::mpe::PlaybackSetupData &setup = track.setup;
        out << "setup " << static_cast<int>(setup.category) << ' ' << (setup.supports_single_note_dynamics ? 1 : 0);
        units(setup.id);
        out << ' ' << setup.sub_categories.size();
        for (const muse::String &sub : setup.sub_categories) units(sub);
        bytes(setup.score_id);
        bytes(setup.music_xml_sound_id);
        out << '\n';

        out << "dynamics " << track.dynamic_layers.size() << '\n';
        for (const muse::mpe::DynamicLayer &layer : track.dynamic_layers) {
            const auto layer_points = track.layer_points(layer);
            out << "layer " << static_cast<unsigned long long>(layer.layer) << ' ' << layer_points.size() << '\n';
            for (const muse::mpe::DynamicPoint &p : layer_points) {
                out << "point " << p.at << ' ' << p.point.in_value.index() << ' ';
                hex(std::bit_cast<std::uint64_t>(p.point.out_value.raw()), 16);
                if (p.point.in_value.index() == 1) {
                    const auto &arrival = std::get<1>(p.point.in_value);
                    for (const double v : {arrival.value.raw(), arrival.ease.t.raw(), arrival.ease.value.raw()}) {
                        out << ' ';
                        hex(std::bit_cast<std::uint64_t>(v), 16);
                    }
                }
                out << '\n';
            }
        }

        // (group, seq, kind, index); kind 0 note, 1 cold, 2 empty mark
        std::vector<std::tuple<muse::mpe::timestamp_t, std::uint32_t, int, std::size_t>> order;
        for (std::size_t i = 0; i < track.notes.size(); ++i) order.emplace_back(track.notes[i].group_timestamp, track.notes[i].seq, 0, i);
        for (std::size_t i = 0; i < track.cold.size(); ++i) order.emplace_back(track.cold[i].group_timestamp, track.cold[i].seq, 1, i);
        for (std::size_t i = 0; i < track.empty_groups.size(); ++i)
            order.emplace_back(track.empty_groups[i].group_timestamp, track.empty_groups[i].seq, 2, i);
        std::sort(order.begin(), order.end());

        std::size_t groups = 0;
        for (std::size_t i = 0; i < order.size(); ++i)
            if (i == 0 || std::get<0>(order[i]) != std::get<0>(order[i - 1])) ++groups;
        out << "groups " << groups << '\n';
        for (std::size_t begin = 0; begin < order.size();) {
            std::size_t end = begin;
            std::size_t events = 0;
            while (end < order.size() && std::get<0>(order[end]) == std::get<0>(order[begin])) {
                if (std::get<2>(order[end]) != 2) ++events;
                ++end;
            }
            out << "group " << std::get<0>(order[begin]) << ' ' << events << '\n';
            for (std::size_t i = begin; i < end; ++i) {
                const std::size_t index = std::get<3>(order[i]);
                if (std::get<2>(order[i]) == 0)
                    note(track, track.notes[index]);
                else if (std::get<2>(order[i]) == 1)
                    cold(track.cold[index]);
            }
            begin = end;
        }
    }

    void score(const mu::engraving::PlaybackScore &score) {
        out << "projection-v1\n";
        for (const auto &[id, events] : score.tracks) track(id, events);
    }

    std::string str() const { return out.str(); }
};
} // namespace music::test
