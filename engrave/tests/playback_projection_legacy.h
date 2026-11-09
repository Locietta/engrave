// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Dump B, legacy side: projects the legacy model's PlaybackData onto the fields the flat store keeps.
// It walks the legacy const surface only and shares no code with the flat-record extractor
// (playback_projection_flat.h); the two must agree byte for byte.

#include <bit>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "mpe/events.h"

namespace music::test {
struct LegacyProjectionTrack {
    std::uint64_t part = 0;
    muse::String instrument;
    const muse::mpe::PlaybackData *data = nullptr;
};

struct LegacyProjection {
    std::string out;

    template <class... Args>
    void put(std::format_string<Args...> format, Args &&...args) {
        std::format_to(std::back_inserter(out), format, std::forward<Args>(args)...);
    }

    void units(const muse::String &value) {
        put(" {}:", value.size());
        for (std::size_t i = 0; i < value.size(); ++i) put("{:04x}", static_cast<unsigned>(value.at(i).unicode()));
    }

    void bytes(const std::optional<std::string> &value) {
        if (!value) {
            put(" -");
            return;
        }
        put(" {}:", value->size());
        for (const char c : *value) put("{:02x}", static_cast<unsigned>(static_cast<unsigned char>(c)));
    }

    template <class Curve>
    void curve(const char *label, const Curve &value) {
        put("{} {}", label, value.size());
        for (const auto &[at, level] : value) put(" {}:{}", static_cast<long long>(at), static_cast<long long>(level));
        put("\n");
    }

    void note(const muse::mpe::NoteEvent &e) {
        const auto &a = e.arrangement_ctx();
        const auto &p = e.pitch_ctx();
        const auto &x = e.expression_ctx();
        put("note {} {} {} {} {:016x} {} {}", a.nominal_timestamp, a.actual_timestamp, a.nominal_duration, a.actual_duration,
            std::bit_cast<std::uint64_t>(a.bps), static_cast<long long>(p.nominal_pitch_level),
            static_cast<long long>(x.nominal_dynamic_level));
        if (x.velocity_override)
            put(" {:08x}", std::bit_cast<std::uint32_t>(*x.velocity_override));
        else
            put(" -");
        put(" {} {}\n", static_cast<unsigned long long>(a.staff_layer_index), static_cast<unsigned long long>(a.voice_layer_index));
        put("arts {}", x.articulations.size());
        for (const auto &[type, data] : x.articulations)
            put(" {}:{}:{}", static_cast<int>(type), data.meta.timestamp, data.meta.overall_duration);
        put("\n");
        curve("pitch", p.pitch_curve);
        curve("expression", x.expression_curve);
    }

    void event(const muse::mpe::PlaybackEvent &value) {
        using namespace muse::mpe;
        if (const auto *e = std::get_if<NoteEvent>(&value)) {
            note(*e);
        } else if (const auto *t = std::get_if<TextArticulationEvent>(&value)) {
            put("text {} {}", static_cast<unsigned long long>(t->layer_idx), static_cast<unsigned>(t->flags));
            units(t->text);
            put("\n");
        } else if (const auto *s = std::get_if<SoundPresetChangeEvent>(&value)) {
            put("preset {}", static_cast<unsigned long long>(s->layer_idx));
            units(s->code);
            put("\n");
        } else if (const auto *y = std::get_if<SyllableEvent>(&value)) {
            put("syllable {} {}", static_cast<unsigned long long>(y->layer_idx), static_cast<unsigned>(y->flags));
            units(y->text);
            put("\n");
        } else {
            // monostate and controller events have no flat record: the projection marks them so B fails
            put("unprojectable {}\n", value.index());
        }
    }

    void track(const LegacyProjectionTrack &track) {
        const muse::mpe::PlaybackData &data = *track.data;
        put("track {}", track.part);
        units(track.instrument);
        put("\n");
        const auto &s = data.setup_data;
        put("setup {} {}", static_cast<int>(s.category), s.supports_single_note_dynamics ? 1 : 0);
        units(s.id);
        put(" {}", s.sub_categories.size());
        for (const auto &sub : s.sub_categories) units(sub);
        bytes(s.score_id);
        bytes(s.music_xml_sound_id);
        put("\n");
        put("dynamics {}\n", data.dynamics.size());
        for (const auto &[layer, points] : data.dynamics) {
            put("layer {} {}\n", static_cast<unsigned long long>(layer), points.size());
            for (const auto &[at, point] : points) {
                put("point {} {} {:016x}", at, point.in_value.index(), std::bit_cast<std::uint64_t>(point.out_value.raw()));
                if (const auto *v = std::get_if<muse::mpe::AutomationPoint::ExplicitArrival>(&point.in_value)) {
                    put(" {:016x} {:016x} {:016x}", std::bit_cast<std::uint64_t>(v->value.raw()),
                        std::bit_cast<std::uint64_t>(v->ease.t.raw()), std::bit_cast<std::uint64_t>(v->ease.value.raw()));
                }
                put("\n");
            }
        }
        put("groups {}\n", data.origin_events.size());
        for (const auto &[at, events] : data.origin_events) {
            put("group {} {}\n", at, events.size());
            for (const auto &e : events) event(e);
        }
    }

    void score(std::span<const LegacyProjectionTrack> tracks) {
        put("projection-v2\n");
        for (const auto &t : tracks) track(t);
        put("end-projection {}\n", tracks.size());
    }
};
} // namespace music::test
