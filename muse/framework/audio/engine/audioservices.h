// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "internal/audioengineconfiguration.h"
#include "internal/audiofactory.h"
#include "internal/fx/fxresolver.h"
#include "internal/synthesizers/soundfontrepository.h"
#include "internal/synthesizers/synthresolver.h"

namespace muse::midi {
struct IMidiOutPort;
} // namespace muse::midi

namespace muse::audio::engine {
struct AudioServices {
    explicit AudioServices(midi::IMidiOutPort *midi_out_port);
    ~AudioServices();
    AudioServices(const AudioServices &) = delete;
    AudioServices &operator=(const AudioServices &) = delete;

    AudioEngineConfiguration &configuration() { return configuration_; }

    synth::SoundFontRepository &sound_font_repository() { return sound_font_repository_; }

    synth::SynthResolver &synth_resolver() { return synth_resolver_; }

    AudioFactory &factory() { return factory_; }
private:
    AudioEngineConfiguration configuration_;
    synth::SoundFontRepository sound_font_repository_;
    synth::SynthResolver synth_resolver_;
    fx::FxResolver fx_resolver_;
    AudioFactory factory_;
};
} // namespace muse::audio::engine
