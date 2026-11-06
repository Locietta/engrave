/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#pragma once

#include "audiotypes.h"
#include "soundfonttypes.h"

namespace muse::audio {
inline AudioResourceMeta make_reverb_meta() {
    AudioResourceMeta meta;
    meta.id = ::muse::audio::muse_reverb_id;
    meta.type = ::muse::audio::native_effect_type_name;
    meta.vendor = "Muse";
    meta.attributes.emplace(::muse::audio::has_native_editor_support_attribute, u"true");

    return meta;
}

inline String audio_resource_type_to_string(const muse::audioplugins::PluginType &meta_type) {
    AudioResourceType type = resource_type_from_string(meta_type);
    auto search = ::muse::audio::resource_type_map.find(type);

    if (search != ::muse::audio::resource_type_map.end()) {
        return search->second;
    }

    return ::muse::audio::resource_type_map.at(AudioResourceType::UNDEFINED);
}

inline String audio_source_name(const AudioInputParams &params) {
    if (params.type() == AudioSourceType::MUSE_SAMPLER) {
        return params.resource_meta.attribute_val(u"museName");
    }

    if (is_resource_type(params.resource_meta, AudioResourceType::FLUID_SOUNDFONT)) {
        const String &preset_name = params.resource_meta.attribute_val(synth::preset_name_attribute);
        if (!preset_name.empty()) {
            return preset_name;
        }

        const String &sound_font_name = params.resource_meta.attribute_val(synth::soundfont_name_attribute);
        if (!sound_font_name.empty()) {
            return sound_font_name;
        }
    }

    return String::from_std_string(params.resource_meta.id);
}

inline String audio_source_pack_name(const AudioInputParams &params) {
    if (params.type() == AudioSourceType::MUSE_SAMPLER) {
        return params.resource_meta.attribute_val(u"musePack");
    }

    return String();
}

inline String audio_source_category_name(const AudioInputParams &params) {
    if (params.type() == AudioSourceType::MUSE_SAMPLER) {
        return params.resource_meta.attribute_val(u"museCategory");
    }

    if (is_resource_type(params.resource_meta, AudioResourceType::FLUID_SOUNDFONT)) {
        return params.resource_meta.attribute_val(synth::soundfont_name_attribute);
    }

    return String::from_std_string(params.resource_meta.id);
}

inline AudioFxCategories audio_fx_categories_from_string(const String &str) {
    if (str.empty()) {
        return {};
    }

    StringList list = str.split('|');

    AudioFxCategories result;
    for (const String &name : list) {
        result.insert(muse::key(::muse::audio::audio_fx_category_to_string_map, name, AudioFxCategory::FX_OTHER));
    }

    return result;
}

inline bool is_online_audio_resource(const AudioResourceMeta &meta) { return bool_attribute(meta, u"isOnline"); }
} // namespace muse::audio
