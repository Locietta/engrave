/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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
#include "audioengineconfiguration.h"

#include "audio/common/soundfonttypes.h"

#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

static const AudioResourceId default_sound_font_name = "MS Basic";
static const AudioResourceAttributes default_audio_resource_attributes = {
    {::muse::audio::playback_setup_data_attribute, muse::mpe::generic_setup_data_string},
    {synth::soundfont_name_attribute, String::from_std_string(::default_sound_font_name)}};

static const AudioResourceMeta default_audio_resource_meta = {::default_sound_font_name, "Fluid", ::default_audio_resource_attributes,
                                                              std::string(::muse::audio::fluid_soundfont_type_name)};

void AudioEngineConfiguration::set_config(const AudioEngineConfig &conf) {

    if (conf.auto_process_online_sounds_in_background != conf_.auto_process_online_sounds_in_background) {
        conf_.auto_process_online_sounds_in_background = conf.auto_process_online_sounds_in_background;
    }

    set_is_lazy_processing_of_online_sounds_enabled(conf.is_lazy_processing_of_online_sounds_enabled);

    if (conf.use_sound_font_low_pass_filter != conf_.use_sound_font_low_pass_filter) {
        conf_.use_sound_font_low_pass_filter = conf.use_sound_font_low_pass_filter;
    }
}

bool AudioEngineConfiguration::auto_process_online_sounds_in_background() const { return conf_.auto_process_online_sounds_in_background; }

void AudioEngineConfiguration::set_is_lazy_processing_of_online_sounds_enabled(bool enabled) {
    if (conf_.is_lazy_processing_of_online_sounds_enabled == enabled) {
        return;
    }

    conf_.is_lazy_processing_of_online_sounds_enabled = enabled;
}

bool AudioEngineConfiguration::is_lazy_processing_of_online_sounds_enabled() const {
    return conf_.is_lazy_processing_of_online_sounds_enabled;
}

bool AudioEngineConfiguration::use_sound_font_low_pass_filter() const { return conf_.use_sound_font_low_pass_filter; }

AudioInputParams AudioEngineConfiguration::default_audio_input_params() const {
    AudioInputParams result;
    result.resource_meta = ::default_audio_resource_meta;

    return result;
}
