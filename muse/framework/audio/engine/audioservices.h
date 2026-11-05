// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "internal/audioengineconfiguration.h"
#include "internal/synthesizers/soundfontrepository.h"
#include "internal/synthesizers/synthresolver.h"
#include "internal/fx/fxresolver.h"
#include "internal/audiofactory.h"

namespace muse::midi {
class IMidiOutPort;
} // namespace muse::midi

namespace muse::audio::engine {
class AudioServices {
public:
    explicit AudioServices(midi::IMidiOutPort *midiOutPort);
    ~AudioServices();
    AudioServices(const AudioServices &) = delete;
    AudioServices &operator=(const AudioServices &) = delete;

    AudioEngineConfiguration &configuration() { return configuration_; }

    synth::SoundFontRepository &soundFontRepository() { return soundFontRepository_; }

    synth::SynthResolver &synthResolver() { return synthResolver_; }

    AudioFactory &factory() { return factory_; }
private:
    AudioEngineConfiguration configuration_;
    synth::SoundFontRepository soundFontRepository_;
    synth::SynthResolver synthResolver_;
    fx::FxResolver fxResolver_;
    AudioFactory factory_;
};
} // namespace muse::audio::engine
