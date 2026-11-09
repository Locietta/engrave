// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>

#include "internal/articulationprofilesrepository.h"
#include "mpetypes.h"

namespace muse::mpe {
// Owner-thread lookup at first use; direct population needs no repository.
struct ArticulationProfileBundle {
    mutable std::array<ArticulationsProfilePtr, static_cast<std::size_t>(SoundCategory::LAST)> profiles;
    const ArticulationProfilesRepository *repository = nullptr;

    ArticulationsProfilePtr default_profile(ArticulationFamily family) const {
        const auto index = static_cast<std::size_t>(family);
        if (index >= profiles.size()) return nullptr;
        if (repository && !resolved_[index]) {
            profiles[index] = repository->ArticulationProfilesRepository::default_profile(family);
            resolved_[index] = true;
        }
        return profiles[index];
    }

private:
    mutable std::array<bool, static_cast<std::size_t>(SoundCategory::LAST)> resolved_{};
};
} // namespace muse::mpe
