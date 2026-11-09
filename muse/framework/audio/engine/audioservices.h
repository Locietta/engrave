// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "audioresources.h"
#include "internal/audioengineconfiguration.h"
#include "internal/synthesizers/soundfontrepository.h"

namespace muse::audio::engine {
struct AudioServices {
    AudioServices();
    ~AudioServices();
    AudioServices(const AudioServices &) = delete;
    AudioServices &operator=(const AudioServices &) = delete;

    AudioEngineConfiguration &configuration() { return configuration_; }

private:
    AudioEngineConfiguration configuration_;
};
} // namespace muse::audio::engine
