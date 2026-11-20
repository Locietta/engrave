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

#include "articulationprofilesrepository.h"

#include <algorithm>
#include <string>
#include <utility>

#include "internal/articulationstringutils.h"
#include "log.h"
#include "serialization/json_fields.h"

using namespace muse;
using namespace muse::mpe;

static const FlatMap<ArticulationFamily, io::PathT> default_articulation_profiles = {
    {ArticulationFamily::KEYBOARDS, io::PathT(":/mpe/resources/general_keyboard_articulations_profile.json")},
    {ArticulationFamily::STRINGS, io::PathT(":/mpe/resources/general_strings_articulations_profile.json")},
    {ArticulationFamily::WINDS, io::PathT(":/mpe/resources/general_winds_articulations_profile.json")},
    {ArticulationFamily::PERCUSSIONS, io::PathT(":/mpe/resources/general_percussion_articulations_profile.json")},
    {ArticulationFamily::VOICES, io::PathT(":/mpe/resources/general_voice_articulations_profile.json")}};

namespace {

namespace json = muse::json;
using ProfileInput = glz::generic_sorted;

struct Offset {
    int offset_position;
    int offset_value;

    struct glaze {
        using T = Offset;
        static constexpr auto value = glz::object("offsetPosition", &T::offset_position, "offsetValue", &T::offset_value);
    };
};

struct Arrangement {
    int duration_factor;
    int timestamp_offset;

    struct glaze {
        using T = Arrangement;
        static constexpr auto value = glz::object("durationFactor", &T::duration_factor, "timestampOffset", &T::timestamp_offset);
    };
};

struct Expression {
    std::vector<Offset> dynamic_offsets;

    struct glaze {
        static constexpr auto value = glz::object("dynamicOffsets", &Expression::dynamic_offsets);
    };
};

struct Pitch {
    std::vector<Offset> pitch_offsets;

    struct glaze {
        static constexpr auto value = glz::object("pitchOffsets", &Pitch::pitch_offsets);
    };
};

struct Segment {
    Arrangement arrangement;
    Expression expression;
    int position;
    Pitch pitch;

    struct glaze {
        using T = Segment;
        static constexpr auto value = glz::object("arrangementPattern", &T::arrangement, "expressionPattern", &T::expression,
                                                  "patternPosition", &T::position, "pitchPattern", &T::pitch);
    };
};

struct Profile {
    glz::ordered_small_map<std::vector<Segment>> patterns;
    std::vector<std::string> supported_families;

    struct glaze {
        using T = Profile;
        static constexpr auto value = glz::object("patterns", &T::patterns, "supportedFamilies", &T::supported_families);
    };
};

int integer(const ProfileInput &object, std::string_view key) { return json::rounded_int(json::member(object, key)); }

template <class Map>
void read_offsets(Map &dest, const ProfileInput &source) {
    for (const auto &entry : json::array(source)) {
        dest.emplace(integer(entry, "offsetPosition"), integer(entry, "offsetValue"));
    }
}

ArticulationPattern read_pattern(const ProfileInput &source) {
    ArticulationPattern pattern;
    for (const auto &entry : json::array(source)) {
        const auto &arrangement = json::member(entry, "arrangementPattern");
        const auto &pitch = json::member(entry, "pitchPattern");
        const auto &expression = json::member(entry, "expressionPattern");
        ArticulationPatternSegment segment;
        segment.arrangement_pattern = {integer(arrangement, "durationFactor"), integer(arrangement, "timestampOffset")};
        read_offsets(segment.pitch_pattern.pitch_offset_map, json::member(pitch, "pitchOffsets"));
        read_offsets(segment.expression_pattern.dynamic_offset_map, json::member(expression, "dynamicOffsets"));
        pattern.emplace(integer(entry, "patternPosition"), std::move(segment));
    }
    return pattern;
}

std::vector<Offset> write_offsets(const auto &source) {
    std::vector<Offset> offsets;
    offsets.reserve(source.size());
    for (const auto &[position, value] : source) offsets.push_back({static_cast<int>(position), static_cast<int>(value)});
    return offsets;
}

std::vector<Segment> write_pattern(const ArticulationPattern &pattern) {
    std::vector<Segment> segments;
    segments.reserve(pattern.size());
    for (const auto &[position, segment] : pattern) {
        segments.push_back({
            .arrangement = {static_cast<int>(segment.arrangement_pattern.duration_factor_value),
                            static_cast<int>(segment.arrangement_pattern.timestamp_offset_value)},
            .expression = {write_offsets(segment.expression_pattern.dynamic_offset_map)},
            .position = static_cast<int>(position),
            .pitch = {write_offsets(segment.pitch_pattern.pitch_offset_map)},
        });
    }
    return segments;
}

} // namespace

ArticulationsProfilePtr ArticulationProfilesRepository::create_new() const { return std::make_shared<ArticulationsProfile>(); }

ArticulationsProfilePtr ArticulationProfilesRepository::default_profile(const ArticulationFamily family) const {
    if (default_profile_override) return *default_profile_override;
    const auto index = static_cast<size_t>(family);
    if (index >= default_profiles_.size()) {
        return nullptr;
    }
    auto &cached = default_profiles_[index];
    if (cached.has_value()) {
        return *cached;
    }

    auto path_search = ::default_articulation_profiles.find(family);

    if (path_search == ::default_articulation_profiles.cend()) {
        LOGE() << "Unable to find path for undefined articulations family";
        return nullptr;
    }

    ArticulationsProfilePtr result = load_profile(path_search->second);
    cached = result;

    return result;
}

ArticulationsProfilePtr ArticulationProfilesRepository::load_profile(const io::PathT &path) const {
    RetVal<ByteArray> file_reading = file_system()->read_file(path);

    if (!file_reading.ret) {
        LOGE() << "Unable to read profile, path: " << path;
        return nullptr;
    }

    const auto source = json::text(file_reading.val);
    const auto parsed = glz::read_json<ProfileInput>(source);
    if (!parsed) {
        LOGE() << glz::format_error(parsed.error(), source);
        return nullptr;
    }

    auto result = std::make_shared<ArticulationsProfile>();
    for (const auto &family : json::array(json::member(*parsed, "supportedFamilies"))) {
        result->supported_families.push_back(
            articulation_family_from_string(String::from_std_string(json::get_or_empty<std::string>(family))));
    }
    for (const auto &[name, pattern] : json::object(json::member(*parsed, "patterns"))) {
        result->set_pattern(articulation_type_from_string(name), read_pattern(pattern));
    }
    return result;
}

void ArticulationProfilesRepository::save_profile(const io::PathT &path, const ArticulationsProfilePtr profile_ptr) {
    IF_ASSERT_FAILED(profile_ptr) { return; }

    Profile profile;
    for (const auto family : profile_ptr->supported_families) {
        profile.supported_families.push_back(articulation_family_to_string(family).to_std_string());
    }

    std::vector<std::pair<std::string, std::vector<Segment>>> patterns;
    for (const auto &[type, pattern] : profile_ptr->data()) {
        patterns.emplace_back(articulation_type_to_string(type).to_std_string(), write_pattern(pattern));
    }
    std::stable_sort(patterns.begin(), patterns.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    for (auto &[name, pattern] : patterns) profile.patterns.insert_or_assign(std::move(name), std::move(pattern));

    auto text = glz::write<json::PrettyPrint{}>(profile);
    if (!text) {
        LOGE() << "Unable to encode MPE Articulation Profile: " << glz::format_error(text.error());
        return;
    }
    const Ret result = file_system()->write_file(path, json::serialized_bytes(std::move(*text)));
    if (!result) {
        LOGE() << "Unable to write MPE Articulation Profile, err: " << result.to_string();
    }
}
