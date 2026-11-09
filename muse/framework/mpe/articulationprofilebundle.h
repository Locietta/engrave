// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>

#include "mpetypes.h"

namespace muse::mpe {
// Resolved on the owner thread before the score walk. Null entries preserve failed/unsupported profiles.
struct ArticulationProfileBundle {
    std::array<ArticulationsProfilePtr, static_cast<std::size_t>(SoundCategory::LAST)> profiles;

    ArticulationsProfilePtr default_profile(ArticulationFamily family) const {
        const auto index = static_cast<std::size_t>(family);
        return index < profiles.size() ? profiles[index] : nullptr;
    }
};
} // namespace muse::mpe
