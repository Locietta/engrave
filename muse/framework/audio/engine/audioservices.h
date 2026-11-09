// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "audioresources.h"
#include "internal/audioengineconfiguration.h"
#include "internal/audiofactory.h"
#include "internal/fx/fxresolver.h"
#include "internal/synthesizers/soundfontrepository.h"

namespace muse::audio::engine {
struct AudioServices {
    AudioServices();
    ~AudioServices();
    AudioServices(const AudioServices &) = delete;
    AudioServices &operator=(const AudioServices &) = delete;

    AudioEngineConfiguration &configuration() { return configuration_; }

    AudioFactory &factory() { return factory_; }
private:
    AudioEngineConfiguration configuration_;
    fx::FxResolver fx_resolver_;
    AudioFactory factory_;
};
} // namespace muse::audio::engine
