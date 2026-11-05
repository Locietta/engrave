// SPDX-License-Identifier: GPL-3.0-only
#include "audioservices.h"
#include "internal/synthesizers/fluidsynth/fluidresolver.h"
#include "internal/fx/musefxresolver.h"
using namespace muse::audio;
using namespace muse::audio::engine;

AudioServices::AudioServices(midi::IMidiOutPort *midiOutPort) : factory_(synthResolver_, fxResolver_) {
    fxResolver_.registerResolver(AudioFxType::MuseFx, std::make_shared<fx::MuseFxResolver>());
    auto fluid = std::make_shared<synth::FluidResolver>(soundFontRepository_, configuration_, midiOutPort);
    synthResolver_.registerResolver(AudioSourceType::Fluid, fluid);
    soundFontRepository_.setOnSoundFontsChanged([fluid]() { fluid->refresh(); });
}

AudioServices::~AudioServices() { soundFontRepository_.setOnSoundFontsChanged({}); }
