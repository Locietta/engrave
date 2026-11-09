// SPDX-License-Identifier: GPL-3.0-only
#include "audioservices.h"

#include "internal/fx/musefxresolver.h"
#include "internal/synthesizers/fluidsynth/fluidresolver.h"
using namespace muse::audio;
using namespace muse::audio::engine;

AudioServices::AudioServices(AudioResources &resources, midi::IMidiOutPort *midi_out_port) : factory_(synth_resolver_, fx_resolver_) {
    fx_resolver_.register_resolver(AudioFxType::MUSE_FX, std::make_shared<fx::MuseFxResolver>());
    auto fluid = std::make_shared<synth::FluidResolver>(resources, configuration_, midi_out_port);
    synth_resolver_.register_resolver(AudioSourceType::FLUID, fluid);
}

AudioServices::~AudioServices() = default;
