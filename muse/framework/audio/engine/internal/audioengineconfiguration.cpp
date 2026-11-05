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

static const AudioResourceId DEFAULT_SOUND_FONT_NAME = "MS Basic";
static const AudioResourceAttributes DEFAULT_AUDIO_RESOURCE_ATTRIBUTES = {
    {PLAYBACK_SETUP_DATA_ATTRIBUTE, muse::mpe::GENERIC_SETUP_DATA_STRING},
    {synth::SOUNDFONT_NAME_ATTRIBUTE, String::fromStdString(DEFAULT_SOUND_FONT_NAME)}};

static const AudioResourceMeta DEFAULT_AUDIO_RESOURCE_META = {DEFAULT_SOUND_FONT_NAME, "Fluid", DEFAULT_AUDIO_RESOURCE_ATTRIBUTES,
                                                              std::string(FLUID_SOUNDFONT_TYPE_NAME)};

void AudioEngineConfiguration::setConfig(const AudioEngineConfig &conf) {

    if (conf.autoProcessOnlineSoundsInBackground != conf_.autoProcessOnlineSoundsInBackground) {
        conf_.autoProcessOnlineSoundsInBackground = conf.autoProcessOnlineSoundsInBackground;
    }

    setIsLazyProcessingOfOnlineSoundsEnabled(conf.isLazyProcessingOfOnlineSoundsEnabled);

    if (conf.useSoundFontLowPassFilter != conf_.useSoundFontLowPassFilter) {
        conf_.useSoundFontLowPassFilter = conf.useSoundFontLowPassFilter;
    }
}

bool AudioEngineConfiguration::autoProcessOnlineSoundsInBackground() const { return conf_.autoProcessOnlineSoundsInBackground; }

void AudioEngineConfiguration::setIsLazyProcessingOfOnlineSoundsEnabled(bool enabled) {
    if (conf_.isLazyProcessingOfOnlineSoundsEnabled == enabled) {
        return;
    }

    conf_.isLazyProcessingOfOnlineSoundsEnabled = enabled;
}

bool AudioEngineConfiguration::isLazyProcessingOfOnlineSoundsEnabled() const { return conf_.isLazyProcessingOfOnlineSoundsEnabled; }

bool AudioEngineConfiguration::useSoundFontLowPassFilter() const { return conf_.useSoundFontLowPassFilter; }

AudioInputParams AudioEngineConfiguration::defaultAudioInputParams() const {
    AudioInputParams result;
    result.resourceMeta = DEFAULT_AUDIO_RESOURCE_META;

    return result;
}
