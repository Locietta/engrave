// SPDX-License-Identifier: GPL-3.0-only
#include "audioservices.h"

#include "internal/fx/musefxresolver.h"

using namespace muse::audio;
using namespace muse::audio::engine;

AudioServices::AudioServices() : factory_(fx_resolver_) {
    fx_resolver_.register_resolver(AudioFxType::MUSE_FX, std::make_shared<fx::MuseFxResolver>());
}

AudioServices::~AudioServices() = default;
