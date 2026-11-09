// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "audio/common/audiotypes.h"

namespace muse::audio::engine {
struct OfflineRenderSettings {
    AudioInputParams default_input;
    bool low_pass_filter = true;
};
} // namespace muse::audio::engine
