// SPDX-License-Identifier: GPL-3.0-only
//
// Link and lifecycle probe for the offline audio services and render session.
// Musical output, encoder finalization and parity are checked by the public
// output tests and engine_audio_probe.

#include <iostream>
#include <memory>

#include "audio/engine/audioservices.h"
#include "audio/engine/internal/offlinerendersession.h"
#include "audio/engine/offlinerendersettings.h"
#include "engrave/src/audio/engine_audio.h"

int main() {
    muse::audio::engine::AudioResources resources;
    muse::audio::engine::AudioServices setup;

    muse::audio::OutputSpec spec;
    spec.sample_rate = 44100;
    spec.samples_per_channel = 4096;
    spec.audio_channel_count = 2;

    muse::audio::AudioEngineConfig config;
    config.use_sound_font_low_pass_filter = true;
    auto configuration = &setup.configuration();
    configuration->set_config(config);

    muse::audio::engine::OfflineRenderSession session(spec, resources, configuration->render_settings(), {});

    std::cout << "audio engine linked\n";
    return 0;
}
