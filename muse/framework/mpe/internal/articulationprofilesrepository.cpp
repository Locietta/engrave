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

#include <string>
#include <utility>

#include "internal/articulationstringutils.h"
#include "log.h"
#include "serialization/json.h"

using namespace muse;
using namespace muse::mpe;

static const FlatMap<ArticulationFamily, io::PathT> default_articulation_profiles = {
    {ArticulationFamily::KEYBOARDS, io::PathT(":/mpe/resources/general_keyboard_articulations_profile.json")},
    {ArticulationFamily::STRINGS, io::PathT(":/mpe/resources/general_strings_articulations_profile.json")},
    {ArticulationFamily::WINDS, io::PathT(":/mpe/resources/general_winds_articulations_profile.json")},
    {ArticulationFamily::PERCUSSIONS, io::PathT(":/mpe/resources/general_percussion_articulations_profile.json")},
    {ArticulationFamily::VOICES, io::PathT(":/mpe/resources/general_voice_articulations_profile.json")}};

static const std::string supported_families = "supportedFamilies";
static const std::string patterns_key = "patterns";
static const std::string pattern_pos_key = "patternPosition";
static const std::string offset_pos_key = "offsetPosition";
static const std::string offset_val_key = "offsetValue";

// Arrangement
static const std::string arrangement_pattern_key = "arrangementPattern";
static const std::string duration_factor_key = "durationFactor";
static const std::string timestamp_offset_key = "timestampOffset";

// Pitch
static const std::string pitch_pattern_key = "pitchPattern";
static const std::string pitch_offsets_key = "pitchOffsets";

// Expression
static const std::string expression_pattern_constant = "expressionPattern";
static const std::string max_amplitude_level_key = "maxAmplitudeLevel";
static const std::string amplitude_time_shift = "amplitudeTimeShift";
static const std::string dynamic_offsets_key = "dynamicOffsets";

static JsonArray json_array(const JsonValue &value) { return value.is_array() ? value.to_array() : JsonArray(); }

static JsonObject json_object(const JsonValue &value) { return value.is_object() ? value.to_object() : JsonObject(); }

ArticulationsProfilePtr ArticulationProfilesRepository::create_new() const { return std::make_shared<ArticulationsProfile>(); }

ArticulationsProfilePtr ArticulationProfilesRepository::default_profile(const ArticulationFamily family) const {
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

    std::string err;

    JsonDocument file = JsonDocument::from_json(file_reading.val, &err);
    if (!err.empty()) {
        LOGE() << err;
        return nullptr;
    }

    ArticulationsProfilePtr result = std::make_shared<ArticulationsProfile>();

    JsonObject root_obj = file.root_object();

    result->supported_families = supported_families_from_json(json_array(root_obj.value(::supported_families)));

    JsonObject articulation_patterns = json_object(root_obj.value(::patterns_key));

    for (const std::string &key : articulation_patterns.keys()) {
        result->set_pattern(articulation_type_from_string(key), patterns_scope_from_json(json_array(articulation_patterns.value(key))));
    }

    return result;
}

void ArticulationProfilesRepository::save_profile(const io::PathT &path, const ArticulationsProfilePtr profile_ptr) {
    IF_ASSERT_FAILED(profile_ptr) { return; }

    JsonObject root_obj;

    root_obj.set(::supported_families, supported_families_to_json(profile_ptr->supported_families));

    JsonObject articulation_patterns;

    for (const auto &pair : profile_ptr->data()) {
        articulation_patterns.set(articulation_type_to_string(pair.first).to_std_string(), patterns_scope_to_json(pair.second));
    }

    root_obj.set(::patterns_key, articulation_patterns);

    ByteArray json = JsonDocument(root_obj).to_json();
    Ret file_writing = file_system()->write_file(path, json);

    if (!file_writing) {
        LOGE() << "Unable to write MPE Articulation Profile, err: " << file_writing.to_string();
        return;
    }
}

std::vector<ArticulationFamily> ArticulationProfilesRepository::supported_families_from_json(const JsonArray &array) const {
    std::vector<ArticulationFamily> result;
    result.reserve(array.size());

    for (size_t i = 0; i < array.size(); ++i) {
        result.push_back(articulation_family_from_string(array.at(i).to_string()));
    }

    return result;
}

JsonArray ArticulationProfilesRepository::supported_families_to_json(const std::vector<ArticulationFamily> &families) const {
    JsonArray result;

    for (const auto &family : families) {
        result.append(articulation_family_to_string(family));
    }

    return result;
}

ArticulationPattern ArticulationProfilesRepository::patterns_scope_from_json(const JsonArray &array) const {
    ArticulationPattern result;

    for (size_t i = 0; i < array.size(); ++i) {
        JsonObject pattern_obj = json_object(array.at(i));

        duration_percentage_t position = pattern_obj.value(::pattern_pos_key).to_int();

        ArrangementPattern arrangement_pattern = arrangement_pattern_from_json(json_object(pattern_obj.value(::arrangement_pattern_key)));
        PitchPattern pitch_pattern = pitch_pattern_from_json(json_object(pattern_obj.value(::pitch_pattern_key)));
        ExpressionPattern expression_pattern_value =
            expression_pattern_from_json(json_object(pattern_obj.value(::expression_pattern_constant)));

        ArticulationPatternSegment articulation;
        articulation.arrangement_pattern = std::move(arrangement_pattern);
        articulation.pitch_pattern = std::move(pitch_pattern);
        articulation.expression_pattern = std::move(expression_pattern_value);

        result.emplace(position, std::move(articulation));
    }

    return result;
}

JsonArray ArticulationProfilesRepository::patterns_scope_to_json(const ArticulationPattern &scope) const {
    JsonArray result;

    for (const auto &pair : scope) {
        JsonObject pattern;
        pattern.set(::pattern_pos_key, static_cast<int>(pair.first));
        pattern.set(::arrangement_pattern_key, arrangement_pattern_to_json(pair.second.arrangement_pattern));
        pattern.set(::pitch_pattern_key, pitch_pattern_to_json(pair.second.pitch_pattern));
        pattern.set(::expression_pattern_constant, expression_pattern_to_json(pair.second.expression_pattern));

        result.append(std::move(pattern));
    }

    return result;
}

ArrangementPattern ArticulationProfilesRepository::arrangement_pattern_from_json(const JsonObject &obj) const {
    ArrangementPattern result;

    result.duration_factor_value = obj.value(::duration_factor_key).to_int();
    result.timestamp_offset_value = obj.value(::timestamp_offset_key).to_int();

    return result;
}

JsonObject ArticulationProfilesRepository::arrangement_pattern_to_json(const ArrangementPattern &pattern) const {
    JsonObject result;

    result.set(::duration_factor_key, static_cast<int>(pattern.duration_factor_value));
    result.set(::timestamp_offset_key, static_cast<int>(pattern.timestamp_offset_value));

    return result;
}

PitchPattern ArticulationProfilesRepository::pitch_pattern_from_json(const JsonObject &obj) const {
    PitchPattern result;

    JsonArray offsets = json_array(obj.value(::pitch_offsets_key));

    for (size_t i = 0; i < offsets.size(); ++i) {
        JsonObject offset_obj = json_object(offsets.at(i));

        result.pitch_offset_map.emplace(offset_obj.value(::offset_pos_key).to_int(), offset_obj.value(::offset_val_key).to_int());
    }

    return result;
}

JsonObject ArticulationProfilesRepository::pitch_pattern_to_json(const PitchPattern &pattern) const {
    JsonObject result;

    JsonArray pitch_offsets;

    for (const auto &pair : pattern.pitch_offset_map) {
        JsonObject offset_obj;
        offset_obj.set(::offset_pos_key, static_cast<int>(pair.first));
        offset_obj.set(::offset_val_key, static_cast<int>(pair.second));

        pitch_offsets.append(std::move(offset_obj));
    }

    result.set(::pitch_offsets_key, pitch_offsets);

    return result;
}

ExpressionPattern ArticulationProfilesRepository::expression_pattern_from_json(const JsonObject &obj) const {
    ExpressionPattern result;

    JsonArray offsets = json_array(obj.value(::dynamic_offsets_key));

    for (size_t i = 0; i < offsets.size(); ++i) {
        JsonObject offset_obj = json_object(offsets.at(i));
        result.dynamic_offset_map.emplace(offset_obj.value(::offset_pos_key).to_int(), offset_obj.value(::offset_val_key).to_int());
    }

    return result;
}

JsonObject ArticulationProfilesRepository::expression_pattern_to_json(const ExpressionPattern &pattern) const {
    JsonObject result;

    JsonArray dynamic_offsets;

    for (const auto &pair : pattern.dynamic_offset_map) {
        JsonObject offset_obj;
        offset_obj.set(::offset_pos_key, static_cast<int>(pair.first));
        offset_obj.set(::offset_val_key, static_cast<int>(pair.second));

        dynamic_offsets.append(std::move(offset_obj));
    }

    result.set(::dynamic_offsets_key, dynamic_offsets);

    return result;
}
